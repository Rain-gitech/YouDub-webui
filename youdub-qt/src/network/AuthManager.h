#pragma once

#include <QObject>
#include <qqmlintegration.h>
#include <QString>

#include "ApiClient.h"

namespace youdub {

// 鉴权状态机。QML 通过它驱动登录页 / 跳转。
// 状态转换：
//   启动 -> checkSession() -> GET /api/auth/session
//        200 -> authenticated=true (csrf 写回 ApiClient)
//        401 -> authenticated=false (显示 Login)
//   login(pw) -> POST /api/auth/login -> 200: cookie 自动存储 + csrf 写入
//   logout() -> POST /api/auth/logout
//   任意请求 401 -> 订阅 ApiClient::unauthorized -> authenticated=false + 清 csrf
class AuthManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
public:
    // parent 缺省为 nullptr，同时充当 QML 单例所需的无参构造。
    explicit AuthManager(QObject *parent = nullptr);

    bool authenticated() const { return authenticated_; }
    bool busy() const { return busy_; }
    QString errorMessage() const { return errorMessage_; }

    // 启动时调用一次，探测现有会话是否有效。
    Q_INVOKABLE void checkSession();
    // 登录。成功后 authenticated=true 并写回 csrf。
    Q_INVOKABLE void login(const QString &password);
    // 登出。
    Q_INVOKABLE void logout();

    // 给其它 ViewModel 复用的 API 客户端。
    ApiClient *apiClient() { return &api_; }

signals:
    void authenticatedChanged();
    void busyChanged();
    void errorMessageChanged();

private:
    void setAuthenticated(bool v);
    void setBusy(bool v);
    void setError(const QString &msg);

    ApiClient api_;
    bool authenticated_{false};
    bool busy_{false};
    QString errorMessage_;
};

} // namespace youdub
