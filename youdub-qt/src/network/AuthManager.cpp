#include "AuthManager.h"

namespace youdub {

AuthManager::AuthManager(QObject *parent)
    : QObject(parent) {
    // 任意请求 401 -> 视为会话失效，弹回登录页。
    connect(&api_, &ApiClient::unauthorized, this, [this]() {
        api_.setCsrfToken(QString{});
        setAuthenticated(false);
        setError(QString{});
    });
}

void AuthManager::setAuthenticated(bool v) {
    if (authenticated_ == v) return;
    authenticated_ = v;
    emit authenticatedChanged();
}

void AuthManager::setBusy(bool v) {
    if (busy_ == v) return;
    busy_ = v;
    emit busyChanged();
}

void AuthManager::setError(const QString &msg) {
    if (errorMessage_ == msg) return;
    errorMessage_ = msg;
    emit errorMessageChanged();
}

void AuthManager::checkSession() {
    setBusy(true);
    api_.requestJson(Method::Get, "/api/auth/session", {},
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status == 0 && obj.isEmpty()) {
                // 网络层失败：保持未登录，给个提示。
                setError(QObject::tr("无法连接到服务，请确认后端已启动。"));
                setAuthenticated(false);
                return;
            }
            if (err.status != 0) {
                setError(QString{});
                setAuthenticated(false);
                return;
            }
            // 成功：后端返回 {"authenticated": true, "csrf_token": ..., "expires_at": ...}
            const auto csrf = obj.value("csrf_token").toString();
            api_.setCsrfToken(csrf);
            setError(QString{});
            setAuthenticated(true);
        });
}

void AuthManager::login(const QString &password) {
    setBusy(true);
    setError(QString{});
    QJsonObject body;
    body.insert("password", password);
    api_.requestJson(Method::Post, "/api/auth/login", body,
        [this](const QJsonObject &obj, const ApiError &err) {
            setBusy(false);
            if (err.status != 0) {
                setError(err.detail.isEmpty() ? QObject::tr("登录失败。") : err.detail);
                return;
            }
            const auto csrf = obj.value("csrf_token").toString();
            api_.setCsrfToken(csrf);
            setError(QString{});
            setAuthenticated(true);
        });
}

void AuthManager::logout() {
    setBusy(true);
    api_.requestJson(Method::Post, "/api/auth/logout", {},
        [this](const QJsonObject &, const ApiError &) {
            setBusy(false);
            api_.setCsrfToken(QString{});
            setAuthenticated(false);
        });
}

} // namespace youdub
