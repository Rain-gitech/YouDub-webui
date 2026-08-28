#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <functional>

namespace youdub {

class CookieJar;

// 统一错误：对应前端 ApiError（apps/web/src/lib/api.ts）。
struct ApiError {
    int status = 0;        // 0 = 网络层错误（无 HTTP 响应）
    QString detail;
};

// HTTP 方法封装。客户端只用到 GET/POST/DELETE。
enum class Method { Get, Post, Delete };

// 所有 HTTP 出口的统一底层。负责：
//  - 基地址前缀；
//  - Cookie jar（session 自动回带）；
//  - JSON / multipart body；
//  - CSRF 头注入（不安全方法）；
//  - 非 2xx 归一化成 ApiError；
//  - 401 透传给 AuthManager（由 AuthManager 订阅 unauthorized 信号）。
//
// 注意：原生请求不带 Origin 头，后端 auth.py 的 _origin_allowed 在无 Origin 时
// 直接放行，因此不存在 CORS 问题。
class ApiClient : public QObject {
    Q_OBJECT
public:
    using JsonCallback = std::function<void(const QJsonObject &, const ApiError &)>;
    using TextCallback = std::function<void(const QString &, const ApiError &)>;

    explicit ApiClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &base) { base_ = base; }
    QString baseUrl() const { return base_; }

    // CSRF token：登录或 session 检查成功后由 AuthManager 写入。
    void setCsrfToken(const QString &token) { csrf_ = token; }
    QString csrfToken() const { return csrf_; }

    // 通用请求。path 形如 "/api/auth/login"。body 留空表示无 body。
    // 成功时 callback 收到解析后的 JSON 与 status=0 的 ApiError；
    // 失败时 JSON 为空对象，ApiError 携带状态码与 detail。
    void requestJson(Method method, const QString &path,
                     const QJsonObject &body, JsonCallback cb);

    // 文本响应（日志、纯文本）。
    void requestText(Method method, const QString &path,
                     const QJsonObject &body, TextCallback cb);

    // 发出任意请求并返回原始 reply（视频产物下载、上传等场景自行处理）。
    QNetworkReply *rawRequest(Method method, const QString &path,
                              const QByteArray &body = QByteArray(),
                              const char *contentType = "application/json");

    QNetworkAccessManager *nam() { return &nam_; }

signals:
    // 任何请求收到 401 时发出，AuthManager 订阅以清理并弹回登录页。
    void unauthorized();
    void requestFailed(const ApiError &error); // 任意非 2xx（除 401 外可选订阅）

private:
    static bool isUnsafe(Method m) { return m != Method::Get; }
    static ApiError parseError(QNetworkReply *reply);
    void applyHeaders(QNetworkRequest &req, Method method) const;

    QNetworkAccessManager nam_;
    CookieJar *cookies_{nullptr};
    QString base_;
    QString csrf_;
};

} // namespace youdub
