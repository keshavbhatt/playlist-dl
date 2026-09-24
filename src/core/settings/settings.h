#pragma once

#include "core/licensing/daily_allowance.h"

#include <QByteArray>
#include <QDateTime>
#include <QLatin1StringView>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace pldl::core {

enum class Theme
{
    System,
    Light,
    Dark,
};

enum class CloseAction
{
    Quit,
    MinimizeToTray,
};

/// The rail page the app opens on (Settings, General).
enum class StartPage
{
    Search,
    LastPage,
};

/// User-agent variants the browser layer can produce. The app has one mode;
/// the enum stays because `web::effectiveUserAgent` is keyed on it.
enum class AppMode
{
    Desktop,
    Tv,
    Music,
};

enum class HardwareAcceleration
{
    Auto, ///< Chromium decides (GPU on unless blocklisted)
    On,   ///< ignore the GPU blocklist
    Off,  ///< --disable-gpu
};

/// Which browser identity Google's sign-in pages see (ADR-008). Only the
/// User-Agent header on the sign-in hosts is affected. Not a persisted
/// setting: the browser layer reads the default.
enum class SignInUserAgent
{
    Firefox, ///< Google accepts Firefox where it rejects embedded Chrome
    Chrome,  ///< same as the rest of the app
};

/// Quality ceiling for the video preset; the selector is bv*[height<=N]+ba.
enum class VideoQuality
{
    Best,
    Q2160,
    Q1440,
    Q1080,
    Q720,
    Q480,
    Q360,
};

enum class Container
{
    Mp4,
    Mkv,
    Webm,
};

enum class AudioFormat
{
    Best, ///< keep the source codec (m4a/opus)
    Mp3,
    M4a,
    Opus,
    Flac,
    Wav,
};

/// What a download sheet offers first (its "Video / Audio only / Advanced"
/// choice); remembered from the last download so audio-only users stay there.
enum class DownloadKind
{
    Video,
    Audio,
    Custom,
};

enum class FilenamePattern
{
    Title,        ///< Title.ext
    TitleId,      ///< Title [id].ext
    ChannelTitle, ///< Channel - Title.ext
};

/// Where a playlist search goes (ADR-003): the search service first with
/// the engine as the fallback, or the engine alone.
enum class SearchMode
{
    Automatic,
    EngineOnly,
};

/// Typed facade over QSettings. The only place in the code base allowed to
/// construct a QSettings. Defaults live in one table (settings.cpp), every
/// setter emits a change signal so the rest of the app reacts instead of
/// polling. Every persisted enum is clamped on read (enumFromInt).
class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Settings)

public:
    explicit Settings(QObject* parent = nullptr);
    Settings(const QString& iniFilePath, QObject* parent = nullptr);
    ~Settings() override;

    // window/
    [[nodiscard]] QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& geometry);
    [[nodiscard]] QByteArray windowState() const;
    void setWindowState(const QByteArray& state);
    [[nodiscard]] QByteArray settingsDialogGeometry() const;
    void setSettingsDialogGeometry(const QByteArray& geometry);
    [[nodiscard]] CloseAction closeAction() const;
    void setCloseAction(CloseAction action);
    /// The rail page shown when the app was last closed (the window's PageId).
    [[nodiscard]] int lastPage() const;
    void setLastPage(int page);
    /// The Browser page's open tabs and the current one, restored on the next
    /// start (web addresses only; a blank tab comes back as the start page).
    struct BrowserSession
    {
        QStringList urls;
        int current = 0;
    };
    [[nodiscard]] BrowserSession browserSession() const;
    void setBrowserSession(const BrowserSession& session);
    /// The app version whose release notes were shown ("What's new"); empty = never.
    [[nodiscard]] QString whatsNewSeenVersion() const;
    void setWhatsNewSeenVersion(const QString& version);

    // general/
    [[nodiscard]] bool notifyOnDownloadFinish() const;
    void setNotifyOnDownloadFinish(bool enabled);
    [[nodiscard]] bool trayEnabled() const;
    void setTrayEnabled(bool enabled);
    [[nodiscard]] StartPage startPage() const;
    void setStartPage(StartPage page);
    /// Whether the What's new sheet opens after an update.
    [[nodiscard]] bool showWhatsNew() const;
    void setShowWhatsNew(bool enabled);

    // appearance/
    [[nodiscard]] Theme theme() const;
    void setTheme(Theme theme);
    [[nodiscard]] double interfaceScale() const;
    void setInterfaceScale(double scale);
    static constexpr double kMinInterfaceScale = 0.5;
    static constexpr double kMaxInterfaceScale = 3.0;

    // browser/
    [[nodiscard]] bool blockAds() const;
    void setBlockAds(bool enabled);
    [[nodiscard]] bool doNotTrack() const;
    void setDoNotTrack(bool enabled);
    [[nodiscard]] QString browserStartPage() const;
    void setBrowserStartPage(const QString& url);
    /// The start page value for an empty tab.
    static constexpr QLatin1StringView kEmptyStartPage{"about:blank"};
    [[nodiscard]] static bool isEmptyStartPage(const QString& url);
    /// Whether the Browser page reopens last time's tabs (off: one tab on the start page).
    [[nodiscard]] bool restoreBrowserTabs() const;
    void setRestoreBrowserTabs(bool enabled);
    /// Browser identity: "default", a `web::userAgentPresets` id, or "custom".
    [[nodiscard]] QString browserUserAgentPreset() const;
    void setBrowserUserAgentPreset(const QString& id);
    /// The custom identity string, used when the preset is "custom".
    [[nodiscard]] QString browserUserAgent() const;
    void setBrowserUserAgent(const QString& userAgent);

    // downloads/
    [[nodiscard]] QString downloadDirectory() const;
    void setDownloadDirectory(const QString& directory);
    /// Whether the user (or the 2.x migration) chose a folder; false means the default applies.
    [[nodiscard]] bool hasDownloadDirectory() const;
    [[nodiscard]] FilenamePattern filenamePattern() const;
    void setFilenamePattern(FilenamePattern pattern);
    /// Downloads sorted into Videos / Music / Playlists / Channels (default on).
    [[nodiscard]] bool organiseDownloads() const;
    void setOrganiseDownloads(bool enabled);
    /// Playlist entries get their index in the file name ("01 - Title").
    [[nodiscard]] bool numberPlaylistFiles() const;
    void setNumberPlaylistFiles(bool enabled);
    [[nodiscard]] VideoQuality defaultQuality() const;
    void setDefaultQuality(VideoQuality quality);
    [[nodiscard]] Container defaultContainer() const;
    void setDefaultContainer(Container container);
    [[nodiscard]] AudioFormat defaultAudioFormat() const;
    void setDefaultAudioFormat(AudioFormat format);
    /// Audio-only bitrate in kbps for the download default: one of kAudioBitrates (0 = best).
    [[nodiscard]] int defaultAudioBitrate() const;
    void setDefaultAudioBitrate(int kbps);
    static constexpr int kAudioBitrates[] = {0, 192, 128};
    [[nodiscard]] DownloadKind lastDownloadKind() const;
    void setLastDownloadKind(DownloadKind kind);
    [[nodiscard]] int concurrentDownloads() const;
    void setConcurrentDownloads(int count);
    static constexpr int kMaxConcurrentDownloads = 5;
    /// 0 = unlimited.
    [[nodiscard]] int speedLimitKbps() const;
    void setSpeedLimitKbps(int kbps);
    /// Files already in the download folder are left alone.
    [[nodiscard]] bool skipExisting() const;
    void setSkipExisting(bool enabled);
    [[nodiscard]] bool useSessionCookies() const;
    void setUseSessionCookies(bool enabled);
    [[nodiscard]] bool embedThumbnail() const;
    void setEmbedThumbnail(bool enabled);
    [[nodiscard]] bool embedMetadata() const;
    void setEmbedMetadata(bool enabled);
    /// Preferred subtitle languages for the download default (e.g. {"en"}).
    [[nodiscard]] QStringList subtitleLanguages() const;
    void setSubtitleLanguages(const QStringList& languages);

    // search/
    [[nodiscard]] SearchMode searchMode() const;
    void setSearchMode(SearchMode mode);
    /// Results per page of the engine's search, kMinSearchResultsPerPage to kMaxSearchResultsPerPage.
    [[nodiscard]] int searchResultsPerPage() const;
    void setSearchResultsPerPage(int count);
    static constexpr int kMinSearchResultsPerPage = 10;
    static constexpr int kMaxSearchResultsPerPage = 50;
    /// Whether the Search page remembers the last queries as chips.
    [[nodiscard]] bool keepSearchHistory() const;
    void setKeepSearchHistory(bool enabled);
    /// The last queries, most recent first, at most kMaxRecentQueries.
    [[nodiscard]] QStringList recentQueries() const;
    /// Puts `query` first (dropping an older copy); a blank query is ignored.
    void addRecentQuery(const QString& query);
    void clearRecentQueries();
    static constexpr int kMaxRecentQueries = 8;

    // engine/
    [[nodiscard]] bool engineAutoUpdate() const;
    void setEngineAutoUpdate(bool enabled);
    [[nodiscard]] bool engineUseSystem() const;
    void setEngineUseSystem(bool enabled);
    [[nodiscard]] QString engineSystemPath() const;
    void setEngineSystemPath(const QString& path);
    [[nodiscard]] QDateTime engineLastCheck() const;
    void setEngineLastCheck(const QDateTime& when);

    // advanced/
    [[nodiscard]] HardwareAcceleration hardwareAcceleration() const;
    void setHardwareAcceleration(HardwareAcceleration mode);
    [[nodiscard]] bool hardwareVideoDecode() const;
    void setHardwareVideoDecode(bool enabled);
    [[nodiscard]] bool gpuAutoDisabled() const;
    void setGpuAutoDisabled(bool disabled);
    [[nodiscard]] int gpuProbeStrikes() const;
    void setGpuProbeStrikes(int strikes);
    [[nodiscard]] bool gpuFallbackNotice() const;
    void setGpuFallbackNotice(bool pending);
    /// Pending "hardware video decoding was turned off" notice (GPU storm
    /// with the experimental decoder on: that switch is backed out first).
    [[nodiscard]] bool videoDecodeFallbackNotice() const;
    void setVideoDecodeFallbackNotice(bool pending);
    /// The free tier's download counter (day + count); see services::LicenseService.
    [[nodiscard]] DailyAllowance freeDownloads() const;
    void setFreeDownloads(const DailyAllowance& allowance);

    void sync();
    /// Every preference back to its default. Window state, the What's new
    /// marker and the free tier's counter are kept.
    void resetToDefaults();
    [[nodiscard]] QString fileName() const;

Q_SIGNALS:
    void closeActionChanged(pldl::core::CloseAction action);
    void notifyOnDownloadFinishChanged(bool enabled);
    void trayEnabledChanged(bool enabled);
    void generalChanged(); ///< the start page or the What's new choice
    void themeChanged(pldl::core::Theme theme);
    void interfaceScaleChanged(double scale);
    void blockAdsChanged(bool enabled);
    void browserChanged();          ///< any browser/ key
    void browserUserAgentChanged(); ///< the identity preset or the custom text
    void downloadDirectoryChanged(const QString& directory);
    void downloadDefaultsChanged(); ///< any downloads/ default a download sheet reads
    void concurrentDownloadsChanged(int count);
    void speedLimitChanged(int kbps);
    void searchChanged();       ///< any search/ key
    void engineConfigChanged(); ///< auto-update / system path
    void hardwareAccelerationChanged(pldl::core::HardwareAcceleration mode);
    void hardwareVideoDecodeChanged(bool enabled);
    void gpuAutoDisabledChanged(bool disabled);

private:
    [[nodiscard]] bool boolValue(QLatin1StringView key, bool def) const;
    bool storeBool(QLatin1StringView key, bool def, bool value);
    [[nodiscard]] int intValue(QLatin1StringView key, int def) const;
    bool storeInt(QLatin1StringView key, int def, int value);
    [[nodiscard]] QString stringValue(QLatin1StringView key, const QString& def = {}) const;
    bool storeString(QLatin1StringView key, const QString& value);

    std::unique_ptr<QSettings> m_store;
};

} // namespace pldl::core

Q_DECLARE_METATYPE(pldl::core::Theme)
Q_DECLARE_METATYPE(pldl::core::CloseAction)
Q_DECLARE_METATYPE(pldl::core::StartPage)
Q_DECLARE_METATYPE(pldl::core::AppMode)
Q_DECLARE_METATYPE(pldl::core::HardwareAcceleration)
Q_DECLARE_METATYPE(pldl::core::SignInUserAgent)
Q_DECLARE_METATYPE(pldl::core::VideoQuality)
Q_DECLARE_METATYPE(pldl::core::Container)
Q_DECLARE_METATYPE(pldl::core::AudioFormat)
Q_DECLARE_METATYPE(pldl::core::DownloadKind)
Q_DECLARE_METATYPE(pldl::core::FilenamePattern)
Q_DECLARE_METATYPE(pldl::core::SearchMode)
