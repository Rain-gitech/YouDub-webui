#include "TaskDetailVm.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QUrl>

#include "AuthManager.h"

namespace youdub {

TaskDetailVm::TaskDetailVm(QObject *parent)
    : QObject(parent) {
    timer_.setInterval(2500);
    logTimer_.setInterval(3000);
    connect(&timer_, &QTimer::timeout, this, &TaskDetailVm::pollOnce);
    connect(&logTimer_, &QTimer::timeout, this, &TaskDetailVm::pollLog);
}

ApiClient *TaskDetailVm::api() { return auth_ ? auth_->apiClient() : nullptr; }

void TaskDetailVm::setAuthManager(AuthManager *a) {
    if (auth_ == a) return;
    auth_ = a;
    emit authManagerChanged();
}

void TaskDetailVm::setTaskId(const QString &id) {
    if (taskId_ == id) return;
    taskId_ = id;
    emit taskIdChanged();
    stages_.clear();
    log_.clear();
    emit logChanged();
    if (!taskId_.isEmpty()) {
        startPolling();
        pollOnce();
        pollLog();
    } else {
        stopPolling();
    }
}

void TaskDetailVm::startPolling() {
    if (polling_) return;
    polling_ = true;
    timer_.start();
    logTimer_.start();
}

void TaskDetailVm::stopPolling() {
    polling_ = false;
    timer_.stop();
    logTimer_.stop();
}

void TaskDetailVm::pollOnce() {
    auto *client = api();
    if (!client || taskId_.isEmpty()) return;
    client->get(QStringLiteral("/api/tasks/") + taskId_,
        [this](const QJsonObject &obj, const ApiError &err) {
            if (err.status == 404) { emit deleted(); return; }
            if (err.status != 0) return;
            applyTaskJson(obj);
        });
}

void TaskDetailVm::pollLog() {
    auto *client = api();
    if (!client || taskId_.isEmpty()) return;
    client->requestText(Method::Get, QStringLiteral("/api/tasks/") + taskId_ + "/log", {},
        [this](const QString &text, const ApiError &) {
            if (log_ != text) { log_ = text; emit logChanged(); }
        });
}

void TaskDetailVm::applyTaskJson(const QJsonObject &o) {
    title_ = o.value("title").toString();
    status_ = o.value("status").toString();
    url_ = o.value("url").toString();
    currentStage_ = o.value("current_stage").toString();
    errorMessage_ = o.value("error_message").toString();
    createdAt_ = o.value("created_at").toString();
    executionMode_ = o.value("execution_mode").toString();
    hasFinalVideo_ = !o.value("final_video_path").toString().isEmpty();

    QVector<TaskStage> stages;
    const auto arr = o.value("stages").toArray();
    stages.reserve(arr.size());
    for (const auto &item : arr) {
        if (item.isObject()) stages.append(parseTaskStage(item.toObject()));
    }
    stages_.replaceAll(stages);
    emit taskChanged();
}

void TaskDetailVm::setActionError(const QString &msg) {
    if (actionError_ == msg) return;
    actionError_ = msg;
    emit actionErrorChanged();
}

void TaskDetailVm::setBusy(bool v) {
    if (busy_ == v) return;
    busy_ = v;
    emit busyChanged();
}

void TaskDetailVm::rerun() {
    auto *client = api();
    if (!client) return;
    setBusy(true);
    setActionError(QString{});
    client->requestJson(Method::Post, QStringLiteral("/api/tasks/") + taskId_ + "/rerun", {},
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) { setActionError(err.detail); return; }
            applyTaskJson(obj);
        });
}

void TaskDetailVm::resume() {
    auto *client = api();
    if (!client) return;
    setBusy(true);
    setActionError(QString{});
    client->requestJson(Method::Post, QStringLiteral("/api/tasks/") + taskId_ + "/resume", {},
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) { setActionError(err.detail); return; }
            applyTaskJson(obj);
        });
}

void TaskDetailVm::continueTask(const QString &executionMode) {
    auto *client = api();
    if (!client) return;
    setBusy(true);
    setActionError(QString{});
    QJsonObject body;
    if (!executionMode.isEmpty()) body.insert("execution_mode", executionMode);
    client->requestJson(Method::Post, QStringLiteral("/api/tasks/") + taskId_ + "/continue", body,
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) { setActionError(err.detail); return; }
            applyTaskJson(obj);
        });
}

void TaskDetailVm::redoStage(const QString &stageName) {
    auto *client = api();
    if (!client) return;
    setBusy(true);
    setActionError(QString{});
    client->requestJson(Method::Post,
        QStringLiteral("/api/tasks/") + taskId_ + "/stages/" + stageName + "/redo", {},
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) { setActionError(err.detail); return; }
            applyTaskJson(obj);
        });
}

void TaskDetailVm::deleteTask() {
    auto *client = api();
    if (!client) return;
    setBusy(true);
    setActionError(QString{});
    client->del(QStringLiteral("/api/tasks/") + taskId_,
        [this](const QJsonObject &, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) { setActionError(err.detail); return; }
            stopPolling();
            emit deleted();
        });
}

void TaskDetailVm::setVideoLoading(bool v) {
    if (videoLoading_ == v) return;
    videoLoading_ = v;
    emit videoLoadingChanged();
}

void TaskDetailVm::setVideoProgress(double p) {
    if (qFuzzyCompare(videoProgress_, p)) return;
    videoProgress_ = p;
    emit videoProgressChanged();
}

// 鉴权 GET 最终视频 artifact，流式写入 file。
// 对齐后端 GET /api/tasks/{id}/artifact/final-video（cookie 鉴权，支持 Range）。
void TaskDetailVm::streamVideoArtifact(QFile *file,
        std::function<void(bool ok, const QString &err)> cb) {
    auto *client = api();
    if (!client) { cb(false, tr("未认证。")); return; }
    if (taskId_.isEmpty()) { cb(false, tr("无任务 ID。")); return; }
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cb(false, tr("无法创建本地文件。"));
        return;
    }
    const QString path = QStringLiteral("/api/tasks/") + taskId_ +
                         QStringLiteral("/artifact/final-video");
    // rawRequest 已注入 UserAgent/Cache-Control；GET 不需要 CSRF；cookie 由 nam 的 CookieJar 自动带。
    QNetworkReply *reply = client->rawRequest(Method::Get, path);
    videoReply_ = reply;

    QObject::connect(reply, &QIODevice::readyRead, this, [this, reply, file]() {
        if (file && file->isOpen()) file->write(reply->readAll());
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 sent, qint64 total) {
        if (total > 0) setVideoProgress(double(sent) / double(total));
    });
    QObject::connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, cb]() {
        reply->deleteLater();
        if (videoReply_ == reply) videoReply_ = nullptr;
        // 写完剩余缓冲。
        if (file && file->isOpen()) {
            file->write(reply->readAll());
            file->flush();
        }
        const bool ok = (reply->error() == QNetworkReply::NoError);
        if (!ok) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            file->close();
            if (status == 401) emit requestOpenList();
            cb(false, status ? tr("下载失败: %1").arg(status) : tr("网络错误。"));
        } else {
            file->close();
            setVideoProgress(1.0);
            cb(true, QString{});
        }
    });
}

void TaskDetailVm::prepareVideo() {
    if (videoLoading_) return;
    // 清理上一次临时文件。
    delete tempVideoFile_;
    tempVideoFile_ = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/youdub-XXXXXX.mp4"), this);
    setVideoProgress(0.0);
    setVideoLoading(true);
    streamVideoArtifact(tempVideoFile_, [this](bool ok, const QString &err) {
        setVideoLoading(false);
        if (!ok) {
            emit videoError(err);
            return;
        }
        // tempVideoFile_ 仍持有句柄；用其文件名构造 file:// URL 交给 MediaPlayer。
        emit videoReady(QUrl::fromLocalFile(tempVideoFile_->fileName()).toString());
    });
}

void TaskDetailVm::downloadVideo(const QString &savePathOrUrl) {
    if (videoLoading_) return;
    // 兼容 QML FileDialog 给的 file:/// URL 与本地路径。
    const QUrl u(savePathOrUrl);
    const QString localPath = u.isLocalFile() ? u.toLocalFile() : savePathOrUrl;
    auto *file = new QFile(localPath, this);
    setVideoProgress(0.0);
    setVideoLoading(true);
    streamVideoArtifact(file, [this, file, localPath](bool ok, const QString &err) {
        setVideoLoading(false);
        file->deleteLater();
        if (!ok) {
            emit videoError(err);
        } else {
            emit videoDownloaded(localPath);
        }
    });
}

void TaskDetailVm::cancelVideo() {
    if (videoReply_) videoReply_->abort();
}

} // namespace youdub
