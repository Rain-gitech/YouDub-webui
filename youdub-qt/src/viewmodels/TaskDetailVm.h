#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <qqmlintegration.h>

#include "StageModel.h"
#include "ApiClient.h"

namespace youdub {

class AuthManager;

// 任务详情 ViewModel。轮询详情与日志，暴露任务字段与动作。
// 动作（对齐 apps/web/src/lib/api.ts）：rerun/resume/continue/redoStage/deleteTask
class TaskDetailVm : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AuthManager *authManager READ authManager WRITE setAuthManager NOTIFY authManagerChanged)
    Q_PROPERTY(QString taskId READ taskId WRITE setTaskId NOTIFY taskIdChanged)
    Q_PROPERTY(StageModel *stages READ stages CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY taskChanged)
    Q_PROPERTY(QString status READ status NOTIFY taskChanged)
    Q_PROPERTY(QString url READ url NOTIFY taskChanged)
    Q_PROPERTY(QString currentStage READ currentStage NOTIFY taskChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY taskChanged)
    Q_PROPERTY(QString createdAt READ createdAt NOTIFY taskChanged)
    Q_PROPERTY(QString executionMode READ executionMode NOTIFY taskChanged)
    Q_PROPERTY(bool hasFinalVideo READ hasFinalVideo NOTIFY taskChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(QString actionError READ actionError NOTIFY actionErrorChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    explicit TaskDetailVm(QObject *parent = nullptr);

    AuthManager *authManager() const { return auth_; }
    void setAuthManager(AuthManager *a);

    QString taskId() const { return taskId_; }
    void setTaskId(const QString &id);

    StageModel *stages() { return &stages_; }
    QString title() const { return title_; }
    QString status() const { return status_; }
    QString url() const { return url_; }
    QString currentStage() const { return currentStage_; }
    QString errorMessage() const { return errorMessage_; }
    QString createdAt() const { return createdAt_; }
    QString executionMode() const { return executionMode_; }
    bool hasFinalVideo() const { return hasFinalVideo_; }
    QString log() const { return log_; }
    QString actionError() const { return actionError_; }
    bool busy() const { return busy_; }

    Q_INVOKABLE void rerun();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void continueTask(const QString &executionMode = QString());
    Q_INVOKABLE void redoStage(const QString &stageName);
    Q_INVOKABLE void deleteTask();

public slots:
    void startPolling();
    void stopPolling();
    void pollOnce();
    void pollLog();

signals:
    void authManagerChanged();
    void taskIdChanged();
    void taskChanged();
    void logChanged();
    void actionErrorChanged();
    void busyChanged();
    void deleted();        // 任务已删除，QML 应返回列表
    void requestOpenList();

private:
    void applyTaskJson(const QJsonObject &obj);
    void setActionError(const QString &msg);
    void setBusy(bool v);
    ApiClient *api();

    AuthManager *auth_{nullptr};
    QString taskId_;
    QTimer timer_;
    QTimer logTimer_;
    StageModel stages_;

    QString title_;
    QString status_;
    QString url_;
    QString currentStage_;
    QString errorMessage_;
    QString createdAt_;
    QString executionMode_;
    bool hasFinalVideo_{false};
    QString log_;
    QString actionError_;
    bool busy_{false};
    bool polling_{false};
};

} // namespace youdub
