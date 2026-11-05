#include "MultipartUpload.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>

#include "ApiClient.h"

namespace youdub {

namespace {
QString localPathFromUrl(const QString &urlOrPath) {
    // QML FileDialog 给的是 file:/// 转成本地路径。
    QUrl u(urlOrPath);
    return u.isLocalFile() ? u.toLocalFile() : urlOrPath;
}
} // namespace

MultipartUpload::MultipartUpload(ApiClient *client, QObject *parent)
    : QObject(parent), client_(client) {}

void MultipartUpload::start(const QString &videoPath, const QString &subtitlePath,
                            const QString &direction, const QString &executionMode) {
    const QString vPath = localPathFromUrl(videoPath);
    const QString sPath = localPathFromUrl(subtitlePath);

    auto *videoFile = new QFile(vPath, this);
    if (!videoFile->open(QIODevice::ReadOnly)) {
        emit finished(QString{}, tr("无法打开视频文件：%1").arg(vPath));
        return;
    }

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType, this);

    // direction
    QHttpPart directionPart;
    directionPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            "form-data; name=\"direction\"");
    directionPart.setBody(direction.toUtf8());
    multiPart->append(directionPart);

    // execution_mode
    QHttpPart modePart;
    modePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       "form-data; name=\"execution_mode\"");
    modePart.setBody(executionMode.toUtf8());
    multiPart->append(modePart);

    // 视频文件（流式）。QFile 的 parent 设给 multiPart 以随其释放。
    videoFile->setParent(multiPart);
    QHttpPart filePart;
    const QString fileName = QFileInfo(vPath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName));
    filePart.setBodyDevice(videoFile);
    multiPart->append(filePart);

    // 可选字幕
    if (!sPath.isEmpty()) {
        auto *subFile = new QFile(sPath, multiPart);
        if (subFile->open(QIODevice::ReadOnly)) {
            QHttpPart subPart;
            subPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                              QStringLiteral("form-data; name=\"subtitle_file\"; filename=\"%1\"")
                                  .arg(QFileInfo(sPath).fileName()));
            subPart.setBodyDevice(subFile);
            multiPart->append(subPart);
        } else {
            delete subFile;
        }
    }

    QNetworkRequest req((QUrl(client_->baseUrl() + "/api/tasks/upload")));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setHeader(QNetworkRequest::UserAgentHeader, "YouDub-Qt/0.1");
    req.setRawHeader("Cache-Control", "no-store");
    // CSRF：上传是 POST，需要带 token。
    if (!client_->csrfToken().isEmpty()) {
        req.setRawHeader("X-CSRF-Token", client_->csrfToken().toUtf8());
    }

    QNetworkReply *reply = client_->nam()->post(req, multiPart);
    multiPart->setParent(reply);  // 随 reply 释放

    QObject::connect(reply, &QNetworkReply::uploadProgress, this, &MultipartUpload::progress);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const auto data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString detail = tr("上传失败: %1").arg(status);
            if (!data.isEmpty()) {
                const auto doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    const auto d = doc.object().value("detail").toString();
                    if (!d.isEmpty()) detail = d;
                }
            }
            emit finished(QString{}, detail);
            return;
        }
        QString id;
        if (!data.isEmpty()) {
            const auto doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) id = doc.object().value("id").toString();
        }
        emit finished(id, QString{});
    });
}

} // namespace youdub
