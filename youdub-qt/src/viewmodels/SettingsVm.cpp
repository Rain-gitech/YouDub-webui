#include "SettingsVm.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

#include "AuthManager.h"

namespace youdub {

SettingsVm::SettingsVm(QObject *parent)
    : QObject(parent) {}

ApiClient *SettingsVm::api() { return auth_ ? auth_->apiClient() : nullptr; }

void SettingsVm::setAuthManager(AuthManager *a) {
    if (auth_ == a) return;
    auth_ = a;
    emit authManagerChanged();
}

void SettingsVm::setLoading(bool v) { if (loading_ != v) { loading_ = v; emit loadingChanged(); } }
void SettingsVm::setSaving(bool v) { if (saving_ != v) { saving_ = v; emit savingChanged(); } }
void SettingsVm::setModelsLoading(bool v) { if (modelsLoading_ != v) { modelsLoading_ = v; emit modelsLoadingChanged(); } }

void SettingsVm::setError(const QString &msg) {
    if (errorMessage_ == msg) return;
    errorMessage_ = msg;
    emit errorMessageChanged();
}

void SettingsVm::setInfo(const QString &msg) {
    if (infoMessage_ == msg) return;
    infoMessage_ = msg;
    emit infoMessageChanged();
}

// --- setters（带 dirty 追踪） ---
void SettingsVm::setCookieContent(const QString &v) {
    if (cookieContent_ == v) return;
    cookieContent_ = v;
    if (!loadingFromServer_) cookieDirty_ = true;
    emit cookieChanged();
}

void SettingsVm::setOpenaiBaseUrl(const QString &v) {
    if (openaiBaseUrl_ == v) return;
    openaiBaseUrl_ = v;
    emit openaiChanged();
}

void SettingsVm::setOpenaiApiKey(const QString &v) {
    if (openaiApiKey_ == v) return;
    openaiApiKey_ = v;
    if (!loadingFromServer_) apiKeyDirty_ = true;
    emit openaiChanged();
}

void SettingsVm::setOpenaiModel(const QString &v) {
    if (openaiModel_ == v) return;
    openaiModel_ = v;
    emit openaiChanged();
}

void SettingsVm::setOpenaiTranslateConcurrency(const QString &v) {
    if (openaiTranslateConcurrency_ == v) return;
    openaiTranslateConcurrency_ = v;
    emit openaiChanged();
}

void SettingsVm::setYtdlpProxyPort(const QString &v) {
    if (ytdlpProxyPort_ == v) return;
    ytdlpProxyPort_ = v;
    emit ytdlpChanged();
}

QString SettingsVm::language() const {
    QSettings s;
    return s.value(QStringLiteral("ui/language"), QStringLiteral("zh")).toString();
}

void SettingsVm::setLanguage(const QString &v) {
    QSettings s;
    s.setValue(QStringLiteral("ui/language"), v);
    emit languageChanged();
}

QString SettingsVm::cookieInfo() const {
    if (!cookieExists_) return QStringLiteral("");
    QString sizeStr;
    if (cookieSize_ >= 1024) sizeStr = QStringLiteral("%1 KB").arg(cookieSize_ / 1024);
    else sizeStr = QStringLiteral("%1 B").arg(cookieSize_);
    if (cookieUpdatedAt_ > 0) {
        const auto t = QDateTime::fromSecsSinceEpoch(cookieUpdatedAt_).toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        return QStringLiteral("已存在 · %1 · %2").arg(sizeStr, t);
    }
    return QStringLiteral("已存在 · %1").arg(sizeStr);
}

// --- 应用后端响应 ---
void SettingsVm::applyCookieJson(const QJsonObject &o) {
    cookieExists_ = o.value("exists").toBool();
    cookieSize_ = static_cast<qint64>(o.value("size").toDouble(0));
    cookieUpdatedAt_ = static_cast<qint64>(o.value("updated_at").toDouble(0));
    // content 恒为空字符串（后端不回显）。
    loadingFromServer_ = true;
    cookieContent_.clear();
    cookieDirty_ = false;
    loadingFromServer_ = false;
    emit cookieChanged();
}

void SettingsVm::applyOpenaiJson(const QJsonObject &o) {
    loadingFromServer_ = true;
    openaiBaseUrl_ = o.value("base_url").toString();
    openaiApiKey_ = o.value("api_key").toString();   // 掩码
    openaiHasApiKey_ = o.value("has_api_key").toBool();
    openaiModel_ = o.value("model").toString();
    openaiTranslateConcurrency_ = o.value("translate_concurrency").toString();
    apiKeyDirty_ = false;
    loadingFromServer_ = false;
    emit openaiChanged();
}

void SettingsVm::applyYtdlpJson(const QJsonObject &o) {
    loadingFromServer_ = true;
    ytdlpProxyPort_ = o.value("proxy_port").toString();
    loadingFromServer_ = false;
    emit ytdlpChanged();
}

// --- 加载 ---
void SettingsVm::loadAll() {
    auto *client = api();
    if (!client) return;
    setLoading(true);
    setError(QString{});
    int *pending = new int(3);
    auto done = [this, pending]() {
        if (--(*pending) == 0) {
            delete pending;
            setLoading(false);
        }
    };
    client->get(QStringLiteral("/api/cookies/youtube"),
        [this, done](const QJsonObject &o, const ApiError &err) {
            if (err.status == 0) applyCookieJson(o);
            done();
        });
    client->get(QStringLiteral("/api/settings/openai"),
        [this, done](const QJsonObject &o, const ApiError &err) {
            if (err.status == 0) applyOpenaiJson(o);
            done();
        });
    client->get(QStringLiteral("/api/settings/ytdlp"),
        [this, done](const QJsonObject &o, const ApiError &err) {
            if (err.status == 0) applyYtdlpJson(o);
            done();
        });
}

// --- 保存 ---
void SettingsVm::saveAll() {
    auto *client = api();
    if (!client) return;
    setSaving(true);
    setError(QString{});
    setInfo(QString{});
    int *pending = new int(cookieDirty_ ? 3 : 2);
    QString *firstErr = new QString();
    auto done = [this, pending, firstErr]() {
        if (--(*pending) == 0) {
            if (!firstErr->isEmpty()) setError(*firstErr);
            else setInfo(tr("设置已保存。"));
            delete pending;
            delete firstErr;
            setSaving(false);
            emit saved();
        }
    };
    auto handle = [this, firstErr, done](const QJsonObject &o, const ApiError &err) {
        if (err.status != 0 && firstErr->isEmpty()) *firstErr = err.detail;
        done();
    };

    // Cookie：仅在改动时保存（对齐 web cookieDirty）。
    if (cookieDirty_) {
        QJsonObject cookieBody;
        cookieBody.insert("content", cookieContent_);
        client->requestJson(Method::Post, QStringLiteral("/api/cookies/youtube"), cookieBody,
            [this, handle](const QJsonObject &o, const ApiError &err) {
                if (err.status == 0) applyCookieJson(o);
                handle(o, err);
            });
    }

    // OpenAI：api_key 仅在改动时回传；清空时 clear_api_key=true。
    QJsonObject openaiBody;
    openaiBody.insert("base_url", openaiBaseUrl_);
    if (apiKeyDirty_) {
        openaiBody.insert("api_key", openaiApiKey_);
        openaiBody.insert("clear_api_key", openaiApiKey_.trimmed().isEmpty());
    } else {
        openaiBody.insert("api_key", QStringLiteral(""));
        openaiBody.insert("clear_api_key", false);
    }
    openaiBody.insert("model", openaiModel_);
    openaiBody.insert("translate_concurrency", openaiTranslateConcurrency_);
    client->requestJson(Method::Post, QStringLiteral("/api/settings/openai"), openaiBody,
        [this, handle](const QJsonObject &o, const ApiError &err) {
            if (err.status == 0) applyOpenaiJson(o);
            handle(o, err);
        });

    // yt-dlp
    QJsonObject ytdlpBody;
    ytdlpBody.insert("proxy_port", ytdlpProxyPort_);
    client->requestJson(Method::Post, QStringLiteral("/api/settings/ytdlp"), ytdlpBody,
        [this, handle](const QJsonObject &o, const ApiError &err) {
            if (err.status == 0) applyYtdlpJson(o);
            handle(o, err);
        });
}

// --- 获取 OpenAI 模型列表 ---
void SettingsVm::fetchModels() {
    auto *client = api();
    if (!client) return;
    setModelsLoading(true);
    setError(QString{});
    QJsonObject body;
    body.insert("base_url", openaiBaseUrl_);
    body.insert("api_key", apiKeyDirty_ ? openaiApiKey_ : QStringLiteral(""));
    client->requestJson(Method::Post, QStringLiteral("/api/settings/openai/models"), body,
        [this](const QJsonObject &o, const ApiError &err) {
            setModelsLoading(false);
            if (err.status != 0) {
                setError(err.detail.isEmpty() ? tr("获取模型失败。") : err.detail);
                return;
            }
            QVariantList models;
            const auto arr = o.value("models").toArray();
            models.reserve(arr.size());
            for (const auto &m : arr) {
                const auto s = m.toString();
                if (!s.isEmpty()) models.append(s);
            }
            openaiModels_ = models;
            emit openaiModelsChanged();
            // 若当前 model 为空，默认选第一个。
            if (openaiModel_.isEmpty() && !models.isEmpty()) {
                setOpenaiModel(models.first().toString());
            }
            setInfo(models.isEmpty() ? tr("未找到模型。")
                                     : tr("已加载 %1 个模型。").arg(models.size()));
        });
}

} // namespace youdub
