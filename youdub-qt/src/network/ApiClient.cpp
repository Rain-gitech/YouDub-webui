#include "ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QUrl>

#include "CookieJar.h"

#ifndef YOUDUB_API_BASE_URL
#define YOUDUB_API_BASE_URL "http://127.0.0.1:8000"
#endif

namespace youdub {

namespace {
// 对齐 apps/web/src/lib/api.ts 的 errorMessage()：
// 优先取 {"detail": string}；其次 FastAPI 校验错误 {"detail": [{msg:...}]}。
QString extractDetail(const QJsonObject &body, int status) {
    const auto detailVal = body.value("detail");
    if (detailVal.isString()) {
        const auto s = detailVal.toString().trimmed();
        if (!s.isEmpty()) return s;
    }
    if (detailVal.isArray()) {
        const auto arr = detailVal.toArray();
        QStringList msgs;
        for (const auto &item : arr) {
            if (!item.isObject()) continue;
            const auto msg = item.toObject().value("msg").toString().trimmed();
            if (!msg.isEmpty() && !msgs.contains(msg)) msgs << msg;
            if (msgs.size() >= 3) break;
        }
        if (!msgs.isEmpty()) return msgs.join("; ");
    }
    return QStringLiteral("Request failed: %1").arg(status);
}
} // namespace

ApiClient::ApiClient(QObject *parent)
    : QObject(parent), base_(QString::fromUtf8(YOUDUB_API_BASE_URL)) {
    cookies_ = new CookieJar(this);
    nam_.setCookieJar(cookies_);
}

void ApiClient::applyHeaders(QNetworkRequest &req, Method method) const {
    req.setHeader(QNetworkRequest::UserAgentHeader, "YouDub-Qt/0.1");
    // 不缓存（对齐前端 cache: "no-store"）。
    req.setRawHeader("Cache-Control", "no-store");
    if (isUnsafe(method) && !csrf_.isEmpty()) {
        req.setRawHeader("X-CSRF-Token", csrf_.toUtf8());
    }
}

QNetworkReply *ApiClient::rawRequest(Method method, const QString &path,
                                     const QByteArray &body, const char *contentType) {
    QUrl url(base_ + path);
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    if (!body.isNull() && contentType) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
    }
    applyHeaders(req, method);

    switch (method) {
        case Method::Get:
            return nam_.get(req);
        case Method::Post:
            return nam_.post(req, body);
        case Method::Delete:
            return nam_.deleteResource(req);
    }
    return nullptr;
}

ApiError ApiClient::parseError(QNetworkReply *reply) {
    ApiError err;
    err.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto payload = reply->readAll();
    if (!payload.isEmpty()) {
        const auto doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            err.detail = extractDetail(doc.object(), err.status);
        }
    }
    if (err.detail.isEmpty()) {
        err.detail = QStringLiteral("Request failed: %1").arg(err.status);
    }
    return err;
}

void ApiClient::requestJson(Method method, const QString &path,
                            const QJsonObject &body, JsonCallback cb) {
    QByteArray payload;
    if (!body.isEmpty()) {
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    QNetworkReply *reply = rawRequest(method, path, payload,
                                      payload.isNull() ? nullptr : "application/json");
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cb]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const auto data = reply->readAll();
            QJsonObject obj;
            if (!data.isEmpty()) {
                const auto doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) obj = doc.object();
            }
            cb(obj, ApiError{});
        } else {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401) emit unauthorized();
            cb(QJsonObject{}, parseError(reply));
        }
    });
}

void ApiClient::requestText(Method method, const QString &path,
                            const QJsonObject &body, TextCallback cb) {
    QByteArray payload;
    if (!body.isEmpty()) {
        payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    }
    QNetworkReply *reply = rawRequest(method, path, payload,
                                      payload.isNull() ? nullptr : "application/json");
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cb]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            cb(QString::fromUtf8(reply->readAll()), ApiError{});
        } else {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == 401) emit unauthorized();
            cb(QString{}, parseError(reply));
        }
    });
}

} // namespace youdub
