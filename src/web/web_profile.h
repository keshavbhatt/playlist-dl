#pragma once

#include "core/settings/settings.h"

#include <QJsonObject>
#include <QString>
#include <QWebEngineProfile>

#include <memory>

namespace pldl::web {

class CookieExporter;
class RequestInterceptor;
class ScriptBundle;

/// The one persistent profile of the built-in browser. Storage paths, cookie
/// policy, user agent, engine attributes, the request interceptor and the
/// injected script bundle are configured here and nowhere else (ADR-000).
class WebProfile : public QWebEngineProfile
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebProfile)

public:
    WebProfile(core::Settings& settings, const QString& appVersion, QObject* parent = nullptr);
    ~WebProfile() override;

    [[nodiscard]] core::Settings& appSettings() { return m_settings; }
    [[nodiscard]] const QString& appVersion() const { return m_appVersion; }
    [[nodiscard]] ScriptBundle& scripts() { return *m_scripts; }
    [[nodiscard]] RequestInterceptor& interceptor() { return *m_interceptor; }
    [[nodiscard]] CookieExporter& cookies() { return *m_cookies; }

    /// The config object injected as window.__red.config (pure over settings).
    [[nodiscard]] QJsonObject scriptConfig() const;
    /// Directory roots that storage clean-up is allowed to touch.
    [[nodiscard]] QStringList storageRoots() const;

Q_SIGNALS:
    /// The config changed (theme, ad blocking): every view's bridge relays it
    /// to its page as `configChanged`.
    void scriptConfigChanged(const QJsonObject& config);
    /// The engine wanted to download a web file (a link click, "Save link"):
    /// the engine's own transfer is cancelled and the app's queue takes the link.
    void fileDownloadRequested(const QUrl& url, const QString& fileName);
    /// A file only the engine could fetch (a blob: or data: made by the page)
    /// was saved into the download folder by the engine itself.
    void engineFileSaved(const QString& path);
    void engineFileFailed(const QString& fileName);

private:
    void configureStorage();
    void configureUserAgent();
    void configureAttributes();
    void configureBlocking();
    void installBootstrap();
    void pushConfig();

    core::Settings& m_settings;
    QString m_appVersion;
    std::unique_ptr<ScriptBundle> m_scripts;
    std::unique_ptr<RequestInterceptor> m_interceptor;
    std::unique_ptr<CookieExporter> m_cookies;
};

} // namespace pldl::web
