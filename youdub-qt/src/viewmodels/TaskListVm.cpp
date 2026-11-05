#include "TaskListVm.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>

#include "AuthManager.h"
#include "MultipartUpload.h"

namespace youdub {

TaskListVm::TaskListVm(QObject *parent)
    : QObject(parent) {
    timer_.setInterval(2500);
    connect(&timer_, &QTimer::timeout, this, &TaskListVm::pollOnce);
}

ApiClient *TaskListVm::api() {
    return auth_ ? auth_->apiClient() : nullptr;
}

void TaskListVm::setAuthManager(AuthManager *a) {
    if (auth_ == a) return;
    auth_ = a;
    emit authManagerChanged();
    if (auth_) {
        startPolling();
        pollOnce();
    } else {
        stopPolling();
    }
}

void TaskListVm::startPolling() {
    if (polling_) return;
    polling_ = true;
    timer_.start();
}

void TaskListVm::stopPolling() {
    polling_ = false;
    timer_.stop();
}

void TaskListVm::setPage(int p) {
    p = qMax(1, p);
    if (page_ == p) return;
    page_ = p;
    emit pageChanged();
    pollOnce();
}

void TaskListVm::setPageSize(int s) {
    if (pageSize_ == s) return;
    pageSize_ = s;
    emit pageSizeChanged();
    resetPage();
}

void TaskListVm::setQuery(const QString &q) {
    if (query_ == q) return;
    query_ = q;
    emit queryChanged();
    resetPage();
}

void TaskListVm::setStatusFilter(const QString &s) {
    if (statusFilter_ == s) return;
    statusFilter_ = s;
    emit statusFilterChanged();
    resetPage();
}

void TaskListVm::setModeFilter(const QString &m) {
    if (modeFilter_ == m) return;
    modeFilter_ = m;
    emit modeFilterChanged();
    resetPage();
}

void TaskListVm::setSort(const QString &s) {
    if (sort_ == s) return;
    sort_ = s;
    emit sortChanged();
    resetPage();
}

void TaskListVm::resetPage() {
    if (page_ == 1) {
        pollOnce();
        return;
    }
    page_ = 1;
    emit pageChanged();
    pollOnce();
}

void TaskListVm::refreshNow() { pollOnce(); }

void TaskListVm::pollOnce() {
    auto *client = api();
    if (!client) return;
    // 组装 query string（对齐 listTasks）。
    QString path = QStringLiteral("/api/tasks?page=%1&page_size=%2")
                       .arg(page_).arg(pageSize_);
    if (!query_.isEmpty()) path += "&q=" + QUrl::toPercentEncoding(query_);
    if (statusFilter_ != "all") path += "&status=" + statusFilter_;
    if (modeFilter_ != "all") path += "&execution_mode=" + modeFilter_;
    path += "&sort=" + sort_;

    client->get(path, [this](const QJsonObject &obj, const ApiError &err) {
        if (err.status != 0) {
            setError(err.detail);
            return;
        }
        setError(QString{});
        QVector<TaskSummary> rows;
        const auto arr = obj.value("tasks").toArray();
        rows.reserve(arr.size());
        for (const auto &item : arr) {
            if (item.isObject()) rows.append(parseTaskSummary(item.toObject()));
        }
        model_.replaceAll(rows);
        int newTotal = obj.value("total").toInt();
        int newActive = obj.value("active_count").toInt(-1);
        if (total_ != newTotal) { total_ = newTotal; emit totalChanged(); }
        if (active_ != newActive) { active_ = newActive; emit activeCountChanged(); }
        // 越界自动回退（对齐前端 lastPage 处理）。
        const int lastPage = qMax(1, (total_ + pageSize_ - 1) / pageSize_);
        if (total_ > 0 && rows.isEmpty() && page_ > lastPage) {
            setPage(lastPage);
        }
    });
}

void TaskListVm::setCreating(bool v) {
    if (creating_ == v) return;
    creating_ = v;
    emit creatingChanged();
}

void TaskListVm::setUploading(bool v) {
    if (uploading_ == v) return;
    uploading_ = v;
    emit uploadingChanged();
}

void TaskListVm::setUploadProgress(double p) {
    if (qFuzzyCompare(uploadProgress_, p)) return;
    uploadProgress_ = p;
    emit uploadProgressChanged();
}

void TaskListVm::setError(const QString &msg) {
    if (errorMessage_ == msg) return;
    errorMessage_ = msg;
    emit errorMessageChanged();
}

void TaskListVm::createTask(const QString &url, const QString &executionMode) {
    auto *client = api();
    if (!client) return;
    setCreating(true);
    setError(QString{});
    QJsonObject body;
    body.insert("url", url);
    body.insert("execution_mode", executionMode);
    client->requestJson(Method::Post, "/api/tasks", body,
        [this](const QJsonObject &obj, const ApiError &err) {
            setCreating(false);
            if (err.status != 0) {
                setError(err.detail.isEmpty() ? tr("创建任务失败。") : err.detail);
                return;
            }
            const auto id = obj.value("id").toString();
            pollOnce();
            if (!id.isEmpty()) emit taskCreated(id);
        });
}

void TaskListVm::uploadLocal(const QString &videoPath, const QString &subtitlePath,
                              const QString &direction, const QString &executionMode) {
    auto *client = api();
    if (!client) return;
    if (uploading_) {
        setError(tr("已有上传任务进行中。"));
        return;
    }
    setError(QString{});
    setUploading(true);
    setUploadProgress(0.0);

    // 上传器作为本 VM 的子对象，完成后自删。
    auto *uploader = new MultipartUpload(client, this);
    connect(uploader, &MultipartUpload::progress, this, [this](qint64 sent, qint64 total) {
        if (total > 0) setUploadProgress(double(sent) / double(total));
    });
    connect(uploader, &MultipartUpload::finished, this,
            [this, uploader](const QString &taskId, const QString &error) {
                setUploading(false);
                setUploadProgress(0.0);
                if (!error.isEmpty()) {
                    setError(error);
                } else {
                    pollOnce();
                    if (!taskId.isEmpty()) emit taskCreated(taskId);
                }
                uploader->deleteLater();
            });
    uploader->start(videoPath, subtitlePath, direction, executionMode);
}

} // namespace youdub
