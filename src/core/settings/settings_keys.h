#pragma once

#include <QLatin1StringView>

// Every persisted key lives here exactly once. Format: "section/camelCase".
// Adding a key = adding a typed accessor pair on core::Settings + a test.
namespace pldl::core::keys {

// window/
inline constexpr QLatin1StringView kWindowGeometry{"window/geometry"};
inline constexpr QLatin1StringView kWindowState{"window/state"};
inline constexpr QLatin1StringView kSettingsDialogGeometry{"window/settingsDialogGeometry"};
inline constexpr QLatin1StringView kCloseAction{"window/closeAction"};
inline constexpr QLatin1StringView kLastPage{"window/lastPage"};
inline constexpr QLatin1StringView kBrowserTabs{"window/browserTabs"}; ///< the open tabs' addresses
inline constexpr QLatin1StringView kBrowserTab{"window/browserTab"};   ///< the current tab's index
inline constexpr QLatin1StringView kWhatsNewSeenVersion{"window/whatsNewSeenVersion"};

// licensing/
inline constexpr QLatin1StringView kFreeDownloadsDay{"licensing/freeDownloadsDay"};
inline constexpr QLatin1StringView kFreeDownloadsUsed{"licensing/freeDownloadsUsed"};

// general/
inline constexpr QLatin1StringView kNotifyOnDownloadFinish{"general/notifyOnDownloadFinish"};
inline constexpr QLatin1StringView kTrayEnabled{"general/trayEnabled"};

// appearance/
inline constexpr QLatin1StringView kTheme{"appearance/theme"};
inline constexpr QLatin1StringView kInterfaceScale{"appearance/interfaceScale"};

// browser/
inline constexpr QLatin1StringView kBlockAds{"browser/blockAds"};
inline constexpr QLatin1StringView kDoNotTrack{"browser/doNotTrack"};
inline constexpr QLatin1StringView kBrowserStartPage{"browser/startPage"};
inline constexpr QLatin1StringView kRestoreBrowserTabs{"browser/restoreTabs"}; ///< reopen last time's tabs
inline constexpr QLatin1StringView kBrowserUserAgentPreset{"browser/userAgentPreset"}; ///< "default", a preset id, "custom"
inline constexpr QLatin1StringView kBrowserUserAgent{"browser/userAgent"};             ///< the custom text

// downloads/
inline constexpr QLatin1StringView kDownloadDirectory{"downloads/directory"};
inline constexpr QLatin1StringView kFilenamePattern{"downloads/filenamePattern"};
inline constexpr QLatin1StringView kOrganiseDownloads{"downloads/organise"};
inline constexpr QLatin1StringView kDefaultQuality{"downloads/defaultQuality"};
inline constexpr QLatin1StringView kDefaultContainer{"downloads/defaultContainer"};
inline constexpr QLatin1StringView kDefaultAudioFormat{"downloads/defaultAudioFormat"};
inline constexpr QLatin1StringView kLastDownloadKind{"downloads/lastKind"};
inline constexpr QLatin1StringView kConcurrentDownloads{"downloads/concurrent"};
inline constexpr QLatin1StringView kSpeedLimitKbps{"downloads/speedLimitKbps"};
inline constexpr QLatin1StringView kUseSessionCookies{"downloads/useSessionCookies"};
inline constexpr QLatin1StringView kEmbedThumbnail{"downloads/embedThumbnail"};
inline constexpr QLatin1StringView kEmbedMetadata{"downloads/embedMetadata"};
inline constexpr QLatin1StringView kSubtitleLanguages{"downloads/subtitleLanguages"};

// engine/
inline constexpr QLatin1StringView kEngineAutoUpdate{"engine/autoUpdate"};
inline constexpr QLatin1StringView kEngineUseSystem{"engine/useSystem"};
inline constexpr QLatin1StringView kEngineSystemPath{"engine/systemPath"};
inline constexpr QLatin1StringView kEngineLastCheck{"engine/lastCheck"};

// advanced/
inline constexpr QLatin1StringView kHardwareAcceleration{"advanced/hardwareAcceleration"};
inline constexpr QLatin1StringView kHardwareVideoDecode{"advanced/hardwareVideoDecode"};
inline constexpr QLatin1StringView kGpuAutoDisabled{"advanced/gpuAutoDisabled"};
inline constexpr QLatin1StringView kGpuProbeStrikes{"advanced/gpuProbeStrikes"};
inline constexpr QLatin1StringView kGpuFallbackNotice{"advanced/gpuFallbackNotice"};
inline constexpr QLatin1StringView kVideoDecodeFallbackNotice{"advanced/videoDecodeFallbackNotice"};

} // namespace pldl::core::keys
