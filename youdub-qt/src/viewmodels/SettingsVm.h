#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <qqmlintegration.h>
#include <QSettings>

#include "ApiClient.h"

namespace youdub {

class AuthManager;

// 设置页 ViewModel。对齐 apps/web/src/components/settings-dialog.tsx：
// YouTube Cookie、OpenAI（含 get models）、yt-dlp 代理。
// API key 读取时为掩码，保存时仅在用户改动时回传（clear_api_key 处理清空）。
class SettingsVm : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(AuthManager *authManager READ authManager WRITE setAuthManager NOTIFY authManagerChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(bool modelsLoading READ modelsLoading NOTIFY modelsLoadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QString infoMessage READ infoMessage NOTIFY infoMessageChanged)

    // YouTube Cookie
    Q_PROPERTY(QString cookieContent READ cookieContent WRITE setCookieContent NOTIFY cookieChanged)
    Q_PROPERTY(QString cookieInfo READ cookieInfo NOTIFY cookieChanged)

    // OpenAI
    Q_PROPERTY(QString openaiBaseUrl READ openaiBaseUrl WRITE setOpenaiBaseUrl NOTIFY openaiChanged)
    Q_PROPERTY(QString openaiApiKey READ openaiApiKey WRITE setOpenaiApiKey NOTIFY openaiChanged)
    Q_PROPERTY(bool openaiHasApiKey READ openaiHasApiKey NOTIFY openaiChanged)
    Q_PROPERTY(QString openaiModel READ openaiModel WRITE setOpenaiModel NOTIFY openaiChanged)
    Q_PROPERTY(QString openaiTranslateConcurrency READ openaiTranslateConcurrency WRITE setOpenaiTranslateConcurrency NOTIFY openaiChanged)
    Q_PROPERTY(QVariantList openaiModels READ openaiModels NOTIFY openaiModelsChanged)

    // yt-dlp
    Q_PROPERTY(QString ytdlpProxyPort READ ytdlpProxyPort WRITE setYtdlpProxyPort NOTIFY ytdlpChanged)

    // UI 语言（"zh" | "en"），存 QSettings，重启生效。
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
public:
    explicit SettingsVm(QObject *parent = nullptr);

    AuthManager *authManager() const { return auth_; }
    void setAuthManager(AuthManager *a);

    bool loading() const { return loading_; }
    bool saving() const { return saving_; }
    bool modelsLoading() const { return modelsLoading_; }
    QString errorMessage() const { return errorMessage_; }
    QString infoMessage() const { return infoMessage_; }

    QString cookieContent() const { return cookieContent_; }
    void setCookieContent(const QString &v);
    QString cookieInfo() const;

    QString openaiBaseUrl() const { return openaiBaseUrl_; }
    void setOpenaiBaseUrl(const QString &v);
    QString openaiApiKey() const { return openaiApiKey_; }
    void setOpenaiApiKey(const QString &v);
    bool openaiHasApiKey() const { return openaiHasApiKey_; }
    QString openaiModel() const { return openaiModel_; }
    void setOpenaiModel(const QString &v);
    QString openaiTranslateConcurrency() const { return openaiTranslateConcurrency_; }
    void setOpenaiTranslateConcurrency(const QString &v);
    QVariantList openaiModels() const { return openaiModels_; }

    QString ytdlpProxyPort() const { return ytdlpProxyPort_; }
    void setYtdlpProxyPort(const QString &v);

    QString language() const;
    void setLanguage(const QString &v);

    Q_INVOKABLE void loadAll();
    Q_INVOKABLE void saveAll();
    Q_INVOKABLE void fetchModels();

signals:
    void authManagerChanged();
    void loadingChanged();
    void savingChanged();
    void modelsLoadingChanged();
    void errorMessageChanged();
    void infoMessageChanged();
    void cookieChanged();
    void openaiChanged();
    void openaiModelsChanged();
    void ytdlpChanged();
    void languageChanged();
    void saved();        // 全部保存完成（可能部分失败，看 errorMessage）

private:
    void setLoading(bool v);
    void setSaving(bool v);
    void setModelsLoading(bool v);
    void setError(const QString &msg);
    void setInfo(const QString &msg);
    void applyCookieJson(const QJsonObject &o);
    void applyOpenaiJson(const QJsonObject &o);
    void applyYtdlpJson(const QJsonObject &o);
    ApiClient *api();

    AuthManager *auth_{nullptr};
    bool loading_{false};
    bool saving_{false};
    bool modelsLoading_{false};
    bool loadingFromServer_{false};   // 加载时抑制 dirty 标记
    bool apiKeyDirty_{false};
    bool cookieDirty_{false};
    QString errorMessage_;
    QString infoMessage_;

    // Cookie
    QString cookieContent_;
    bool cookieExists_{false};
    qint64 cookieSize_{0};
    qint64 cookieUpdatedAt_{0};

    // OpenAI
    QString openaiBaseUrl_;
    QString openaiApiKey_;
    bool openaiHasApiKey_{false};
    QString openaiModel_;
    QString openaiTranslateConcurrency_;
    QVariantList openaiModels_;

    // yt-dlp
    QString ytdlpProxyPort_;
};

} // namespace youdub
