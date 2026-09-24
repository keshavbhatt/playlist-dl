#include "web/user_agent.h"

#include <QObject>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace pldl::web {

QString sanitizeUserAgent(const QString& defaultUserAgent)
{
    static const QRegularExpression kQtToken(u"\\s*QtWebEngine/[\\d.]+"_s);
    QString ua = defaultUserAgent;
    ua.remove(kQtToken);
    return ua.simplified();
}

QString firefoxUserAgent()
{
    return u"Mozilla/5.0 (X11; Linux x86_64; rv:141.0) Gecko/20100101 Firefox/141.0"_s;
}

QString tvNavigatorUserAgent(const QString& appVersion)
{
    return u"Mozilla/5.0 (PS4; Leanback Shell) Cobalt/19.lts.0-qa; compatible; PlaylistDownloader/"_s + appVersion;
}

QString tvWireUserAgent(const QString& appVersion)
{
    return u"Mozilla/5.0 (PS4; Leanback Shell) Cobalt/25.lts.40.1035033; compatible; PlaylistDownloader/"_s + appVersion;
}

QString tvGenericUserAgent(const QString& appVersion)
{
    return u"PlaylistDownloader/"_s + appVersion;
}

bool isGoogleSignInHost(const QString& host)
{
    const QString h = host.toLower();
    for (const auto base :
         {QLatin1StringView("accounts.google.com"), QLatin1StringView("accounts.youtube.com")}) {
        if (h == base || h.endsWith(u'.' + base)) {
            return true;
        }
    }
    return false;
}

QString signInUserAgent(core::SignInUserAgent choice, const QString& userOverride)
{
    if (choice != core::SignInUserAgent::Firefox) {
        return {};
    }
    const QString override = userOverride.trimmed();
    if (!override.isEmpty() && !override.contains(u"Chrome/"_s)) {
        return {}; // a Firefox or Safari identity of the user's own: Google accepts it
    }
    return firefoxUserAgent();
}

namespace {

QString chromeVersionOf(const QString& engineDefault)
{
    static const QRegularExpression kChrome(u"Chrome/([\\d.]+)"_s);
    const QRegularExpressionMatch match = kChrome.match(engineDefault);
    return match.hasMatch() ? match.captured(1) : u"130.0.0.0"_s;
}

} // namespace

QList<UserAgentPreset> userAgentPresets(const QString& engineDefault)
{
    const QString chrome = chromeVersionOf(engineDefault);
    const QString webkit = u"AppleWebKit/537.36 (KHTML, like Gecko)"_s;
    // Safari and Firefox strings: bump with the app's releases (2026-09).
    return {
        {QString(kUserAgentPresetDefault), QObject::tr("Default (this app)"), QString(), QString(), false},
        {u"chrome-windows"_s, u"Chrome on Windows"_s,
         u"Mozilla/5.0 (Windows NT 10.0; Win64; x64) %1 Chrome/%2 Safari/537.36"_s.arg(webkit, chrome), u"Windows"_s,
         false},
        {u"chrome-macos"_s, u"Chrome on macOS"_s,
         u"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) %1 Chrome/%2 Safari/537.36"_s.arg(webkit, chrome),
         u"macOS"_s, false},
        {u"chrome-android"_s, u"Chrome on Android"_s,
         u"Mozilla/5.0 (Linux; Android 14; Pixel 8) %1 Chrome/%2 Mobile Safari/537.36"_s.arg(webkit, chrome),
         u"Android"_s, true},
        {u"edge-windows"_s, u"Edge on Windows"_s,
         u"Mozilla/5.0 (Windows NT 10.0; Win64; x64) %1 Chrome/%2 Safari/537.36 Edg/%2"_s.arg(webkit, chrome),
         u"Windows"_s, false},
        {u"firefox-linux"_s, u"Firefox on Linux"_s, firefoxUserAgent(), u"Linux"_s, false},
        {u"safari-macos"_s, u"Safari on macOS"_s,
         u"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) "
         u"Version/18.6 Safari/605.1.15"_s,
         u"macOS"_s, false},
        {u"safari-iphone"_s, u"Safari on iPhone"_s,
         u"Mozilla/5.0 (iPhone; CPU iPhone OS 18_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) "
         u"Version/18.6 Mobile/15E148 Safari/604.1"_s,
         u"iOS"_s, true},
    };
}

QString chosenUserAgent(const QString& presetId, const QString& custom, const QString& engineDefault)
{
    const QString id = presetId.trimmed();
    if (id.isEmpty() || id == kUserAgentPresetDefault) {
        return {};
    }
    if (id != kUserAgentPresetCustom) {
        for (const UserAgentPreset& preset : userAgentPresets(engineDefault)) {
            if (preset.id == id) {
                return preset.userAgent;
            }
        }
    }
    return custom.trimmed();
}

QString effectiveUserAgent(const QString& engineDefault, const QString& userOverride, core::AppMode mode,
                           const QString& appVersion)
{
    const QString trimmed = userOverride.trimmed();
    if (!trimmed.isEmpty()) {
        return trimmed;
    }
    if (mode == core::AppMode::Tv) {
        return tvNavigatorUserAgent(appVersion);
    }
    return sanitizeUserAgent(engineDefault);
}

} // namespace pldl::web
