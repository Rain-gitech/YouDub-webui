#include "TaskDetailVm.h"

#include <QJsonArray>
#include <QJsonObject>

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

} // namespace youdub
