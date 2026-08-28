#pragma once

#include <QNetworkCookieJar>

namespace youdub {

// 持久化在内存中的 cookie jar：承载 FastAPI 的 youdub_session 会话 cookie。
// HttpOnly 对原生客户端无意义（HttpOnly 只是浏览器 JS 访问限制），QNAM 会自动
// 带上并存储。当前阶段内存即可；后续阶段如需跨进程持久化，可加保存/加载逻辑。
class CookieJar : public QNetworkCookieJar {
    Q_OBJECT
public:
    explicit CookieJar(QObject *parent = nullptr);
};

} // namespace youdub
