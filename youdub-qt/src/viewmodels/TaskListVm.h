#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <qqmlintegration.h>

#include "TaskListModel.h"
#include "ApiClient.h"

namespace youdub {

class AuthManager;

// 首页任务列表 ViewModel。持有分页/过滤/排序状态、轮询定时器、创建任务逻辑。
// 对齐 apps/web/src/app/page.tsx 的 pollTasks + 表单提交。
class TaskListVm : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TaskListModel *model READ model CONSTANT)
    Q_PROPERTY(int total READ total NOTIFY totalChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(int page READ page WRITE setPage NOTIFY pageChanged)
    Q_PROPERTY(int pageSize READ pageSize WRITE setPageSize NOTIFY pageSizeChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString statusFilter READ statusFilter WRITE setStatusFilter NOTIFY statusFilterChanged)
    Q_PROPERTY(QString modeFilter READ modeFilter WRITE setModeFilter NOTIFY modeFilterChanged)
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool creating READ creating NOTIFY creatingChanged)
public:
    explicit TaskListVm(QObject *parent = nullptr);

    // 由 QML 注入：AuthManager 提供 ApiClient。QML 中设置 authManager 属性。
    Q_PROPERTY(AuthManager *authManager READ authManager WRITE setAuthManager NOTIFY authManagerChanged)
    AuthManager *authManager() const { return auth_; }
    void setAuthManager(AuthManager *a);

    TaskListModel *model() { return &model_; }
    int total() const { return total_; }
    int activeCount() const { return active_; }
    int page() const { return page_; }
    void setPage(int p);
    int pageSize() const { return pageSize_; }
    void setPageSize(int s);
    const QString &query() const { return query_; }
    void setQuery(const QString &q);
    const QString &statusFilter() const { return statusFilter_; }
    void setStatusFilter(const QString &s);
    const QString &modeFilter() const { return modeFilter_; }
    void setModeFilter(const QString &m);
    const QString &sort() const { return sort_; }
    void setSort(const QString &s);
    QString errorMessage() const { return errorMessage_; }
    bool creating() const { return creating_; }

    // 创建 YouTube/Bilibili URL 任务。成功发 taskCreated(id)。
    Q_INVOKABLE void createTask(const QString &url, const QString &executionMode = "auto");

    // 立即刷新一次（过滤变化时调用）。
    Q_INVOKABLE void refreshNow();

public slots:
    void startPolling();
    void stopPolling();

signals:
    void totalChanged();
    void activeCountChanged();
    void pageChanged();
    void pageSizeChanged();
    void queryChanged();
    void statusFilterChanged();
    void modeFilterChanged();
    void sortChanged();
    void errorMessageChanged();
    void creatingChanged();
    void authManagerChanged();
    void taskCreated(const QString &taskId);

private:
    void pollOnce();
    void resetPage();
    void setError(const QString &msg);
    void setCreating(bool v);
    ApiClient *api();

    AuthManager *auth_{nullptr};
    TaskListModel model_;
    QTimer timer_;
    int page_{1};
    int pageSize_{20};
    int total_{0};
    int active_{-1};
    QString query_;
    QString statusFilter_{QStringLiteral("all")};
    QString modeFilter_{QStringLiteral("all")};
    QString sort_{QStringLiteral("created_desc")};
    QString errorMessage_;
    bool creating_{false};
    bool polling_{false};
};

} // namespace youdub
