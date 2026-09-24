#include "web/web_profile.h"

#include "core/blocking/block_list.h"
#include "web/cookie_exporter.h"
#include "web/logging.h"
#include "web/request_interceptor.h"
#include "web/script_bundle.h"
#include "web/user_agent.h"

#include <QDir>
#include <QStandardPaths>
#include <QWebEngineClientHints>
#include <QWebEngineCookieStore>
#include <QWebEngineDownloadRequest>
#include <QWebEngineScript>
#include <QWebEngineSettings>

using namespace Qt::StringLiterals;

namespace pldl::web {

namespace {

constexpr auto kProfileName = "pldl";

QString themeName(core::Theme theme)
{
    switch (theme) {
    case core::Theme::Light:
        return u"light"_s;
    case core::Theme::Dark:
        return u"dark"_s;
    case core::Theme::System:
        break;
    }
    return u"system"_s;
}

} // namespace

WebProfile::WebProfile(core::Settings& settings, const QString& appVersion, QObject* parent)
    : QWebEngineProfile(QString::fromLatin1(kProfileName), parent)
    , m_settings(settings)
    , m_appVersion(appVersion)
    , m_scripts(std::make_unique<ScriptBundle>(*this))
    , m_interceptor(std::make_unique<RequestInterceptor>())
{
    configureStorage();
    // After the storage path: the exporter asks the store to load every
    // persisted cookie, and before configureStorage() that store is empty
    // (the sign-in would never reach yt-dlp, ADR-006).
    m_cookies = std::make_unique<CookieExporter>(cookieStore(), persistentStoragePath() + u"/Cookies"_s);
    configureUserAgent();
    configureAttributes();
    configureBlocking();
    installBootstrap();
    setUrlRequestInterceptor(m_interceptor.get());
    connect(this, &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest* request) {
        const QUrl url = request->url();
        const QString name = request->downloadFileName();
        const QString scheme = url.scheme().toLower();
        if (scheme == u"http"_s || scheme == u"https"_s || scheme == u"ftp"_s) {
            qCInfo(lcWeb) << "engine download handed to the queue:" << url;
            request->cancel();
            Q_EMIT fileDownloadRequested(url, name);
            return;
        }
        // A blob: or data: file exists only inside the page: the engine saves
        // it into the download folder and the app reports the result.
        qCInfo(lcWeb) << "engine saves a page-made file:" << name;
        request->setDownloadDirectory(m_settings.downloadDirectory());
        connect(request, &QWebEngineDownloadRequest::isFinishedChanged, this, [this, request] {
            if (request->state() == QWebEngineDownloadRequest::DownloadCompleted) {
                Q_EMIT engineFileSaved(request->downloadDirectory() + u'/' + request->downloadFileName());
            } else {
                Q_EMIT engineFileFailed(request->downloadFileName());
            }
        });
        request->accept();
    });

    connect(&m_settings, &core::Settings::blockAdsChanged, this, [this](bool) {
        configureBlocking();
        pushConfig();
    });
    m_interceptor->setDoNotTrack(m_settings.doNotTrack());
    connect(&m_settings, &core::Settings::browserChanged, this,
            [this] { m_interceptor->setDoNotTrack(m_settings.doNotTrack()); });
    connect(&m_settings, &core::Settings::themeChanged, this, [this](core::Theme) { pushConfig(); });
    connect(&m_settings, &core::Settings::browserUserAgentChanged, this, [this] { configureUserAgent(); });
    qCInfo(lcWeb) << "profile ready, storage at" << persistentStoragePath();
}

WebProfile::~WebProfile()
{
    setUrlRequestInterceptor(nullptr);
}

void WebProfile::configureStorage()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString storage = base + u"/profile"_s;
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/profile"_s;
    QDir().mkpath(storage);
    QDir().mkpath(cache);

    setPersistentStoragePath(storage);
    setCachePath(cache);
    setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    setHttpCacheMaximumSize(512 * 1024 * 1024); // video sites churn the cache; cap it
    setPersistentPermissionsPolicy(QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
}

QStringList WebProfile::storageRoots() const
{
    return {QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)};
}

void WebProfile::configureUserAgent()
{
    // The engine default is read once; effectiveUserAgent strips the QtWebEngine
    // token and applies the identity the user chose (ADR-000, FEATURES B3).
    static const QString engineDefault = QWebEngineProfile::defaultProfile()->httpUserAgent();
    const QString presetId = m_settings.browserUserAgentPreset();
    const QString override = chosenUserAgent(presetId, m_settings.browserUserAgent(), engineDefault);
    setHttpUserAgent(effectiveUserAgent(engineDefault, override, core::AppMode::Desktop, m_appVersion));
    m_interceptor->setSignInUserAgent(signInUserAgent(core::SignInUserAgent::Firefox, override));
    // Client hints follow the preset so a site does not see a Safari header
    // next to Chromium's own platform hint; Default and Custom keep the engine's.
    if (QWebEngineClientHints* hints = clientHints(); hints != nullptr) {
        for (const UserAgentPreset& preset : userAgentPresets(engineDefault)) {
            if (preset.id == presetId && !preset.platform.isEmpty()) {
                hints->setPlatform(preset.platform);
                hints->setIsMobile(preset.mobile);
            }
        }
    }
    qCInfo(lcWeb) << "browser identity:" << presetId << httpUserAgent();
}

void WebProfile::configureAttributes()
{
    QWebEngineSettings* s = settings();
    s->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
    s->setAttribute(QWebEngineSettings::JavascriptCanPaste, true);
    s->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    s->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, false);
    s->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    s->setAttribute(QWebEngineSettings::DnsPrefetchEnabled, true);
    s->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    s->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    s->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, true);
    s->setAttribute(QWebEngineSettings::ErrorPageEnabled, false); // we render our own (web/error_page)
    s->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    s->setAttribute(QWebEngineSettings::NavigateOnDropEnabled, false);
}

void WebProfile::configureBlocking()
{
    core::BlockList::Options options;
    options.ads = m_settings.blockAds();
    options.trackers = m_settings.blockAds();
    m_interceptor->setBlockList(std::make_shared<const core::BlockList>(core::BlockList::builtin(options)));
    qCInfo(lcWeb) << "blocking: ads" << options.ads << "trackers" << options.trackers;
}

QJsonObject WebProfile::scriptConfig() const
{
    return QJsonObject{
        {u"appVersion"_s, m_appVersion},
        {u"colorScheme"_s, themeName(m_settings.theme())},
        {u"adblock"_s, m_settings.blockAds()},
    };
}

void WebProfile::installBootstrap()
{
    m_scripts->installBootstrap(scriptConfig());
    // Media detection needs the DOM (FEATURES B4); on every site, not only
    // YouTube, so it is its own DocumentReady script rather than part of the
    // bundle.
    if (!ScriptBundle::isDisabled(u"page-media"_s)) {
        m_scripts->installResource(u"page-media"_s, u":/scripts/page-media.js"_s, QWebEngineScript::DocumentReady);
    }
}

void WebProfile::pushConfig()
{
    installBootstrap();
    Q_EMIT scriptConfigChanged(scriptConfig());
}

} // namespace pldl::web
