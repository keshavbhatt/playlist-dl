#pragma once

#include "core/settings/settings.h"

#include <QList>
#include <QString>

namespace pldl::web {

/// Qt WebEngine's default user agent is a real Chrome UA with an extra
/// "QtWebEngine/x.y.z" token; removing that token is enough to be treated as
/// Chrome by YouTube and Google sign-in (ADR-008). Pure, unit-tested.
[[nodiscard]] QString sanitizeUserAgent(const QString& defaultUserAgent);

/// A current Firefox UA, sent on Google's sign-in hosts only: Google rejects
/// embedded Chrome ("This browser or app may not be secure") but accepts
/// Firefox (SignInUserAgent::Firefox, the default).
[[nodiscard]] QString firefoxUserAgent();

/// One "browser identity" the user can pick (FEATURES B3, Settings, Browser).
struct UserAgentPreset
{
    QString id;        ///< stored in settings ("default", "chrome-windows", ...)
    QString label;     ///< shown in the picker
    QString userAgent; ///< empty for "default" (the sanitized engine identity)
    QString platform;  ///< client hint Sec-CH-UA-Platform ("Windows", "macOS", ...), empty = engine's
    bool mobile = false;
};
/// The presets, "default" first. Chrome and Edge take the engine's own
/// Chrome version from `engineDefault` so they never go stale; the Firefox
/// and Safari strings are fixed and bumped with the app.
[[nodiscard]] QList<UserAgentPreset> userAgentPresets(const QString& engineDefault);
/// The override for `effectiveUserAgent`: empty for "default", the preset's
/// string for a known id, the trimmed custom text for "custom" (and for an
/// unknown id, so an old setting never silently falls back to the default).
[[nodiscard]] QString chosenUserAgent(const QString& presetId, const QString& custom, const QString& engineDefault);
inline constexpr QLatin1StringView kUserAgentPresetDefault{"default"};
inline constexpr QLatin1StringView kUserAgentPresetCustom{"custom"};

/// accounts.google.com, accounts.youtube.com and their subdomains.
[[nodiscard]] bool isGoogleSignInHost(const QString& host);
/// The User-Agent header for Google's sign-in hosts, empty for "no change".
/// With a user override that is still Chrome-like (contains "Chrome/") the
/// Firefox identity stays: Google turns embedded Chrome away whatever the
/// platform token says. A Firefox or Safari override is used as it is.
[[nodiscard]] QString signInUserAgent(core::SignInUserAgent choice, const QString& userOverride);

/// TV mode (ADR-001, VacuumTube V1): what `navigator.userAgent` reports inside
/// Leanback, an old Cobalt so YouTube does not assume Widevine.
[[nodiscard]] QString tvNavigatorUserAgent(const QString& appVersion);
/// TV mode: what www.youtube.com sees on the wire, a newer Cobalt for the
/// current UI. Set per request by the interceptor.
[[nodiscard]] QString tvWireUserAgent(const QString& appVersion);
/// TV mode: everything that is not www.youtube.com.
[[nodiscard]] QString tvGenericUserAgent(const QString& appVersion);

/// The profile UA for a mode: the override wins, then the TV navigator UA,
/// else the sanitized engine default.
[[nodiscard]] QString effectiveUserAgent(const QString& engineDefault, const QString& userOverride,
                                         core::AppMode mode, const QString& appVersion);

} // namespace pldl::web
