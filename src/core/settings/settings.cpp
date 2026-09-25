#include "core/settings/settings.h"

#include "core/logging.h"
#include "core/settings/settings_keys.h"

#include <QDir>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::core {

namespace {

// ---- Defaults: the single source of truth ---------------------------------
constexpr CloseAction kDefaultCloseAction = CloseAction::Quit;
constexpr bool kDefaultNotifyOnDownloadFinish = true;
constexpr bool kDefaultTrayEnabled = true;
constexpr StartPage kDefaultStartPage = StartPage::Search;
constexpr bool kDefaultShowWhatsNew = true;
constexpr Theme kDefaultTheme = Theme::System;
constexpr double kDefaultInterfaceScale = 1.0;
constexpr bool kDefaultBlockAds = true;
constexpr bool kDefaultDoNotTrack = false;
// An empty tab (owner, 2026-09-24); YouTube and a custom address are the other choices.
constexpr QLatin1StringView kDefaultBrowserStartPage{"about:blank"};
constexpr bool kDefaultRestoreBrowserTabs = false;
constexpr FilenamePattern kDefaultFilenamePattern = FilenamePattern::Title;
constexpr bool kDefaultOrganiseDownloads = true;
constexpr bool kDefaultNumberPlaylistFiles = true;
constexpr int kDefaultAudioBitrate = 0;
constexpr bool kDefaultSkipExisting = true;
constexpr bool kDefaultWritePlaylistFile = true;
constexpr VideoQuality kDefaultQuality = VideoQuality::Best;
constexpr Container kDefaultContainer = Container::Mp4;
constexpr AudioFormat kDefaultAudioFormat = AudioFormat::Best;
constexpr DownloadKind kDefaultDownloadKind = DownloadKind::Video;
constexpr int kDefaultConcurrent = 2;
constexpr int kDefaultSpeedLimit = 0;
constexpr bool kDefaultUseSessionCookies = true;
constexpr bool kDefaultEmbedThumbnail = true;
constexpr bool kDefaultEmbedMetadata = true;
constexpr int kDefaultSearchResultsPerPage = 20;
constexpr bool kDefaultKeepSearchHistory = true;
constexpr bool kDefaultSearchGridView = false; // rows by default (owner, 2026-09-25)
constexpr bool kDefaultSearchSuggestions = true;
constexpr bool kDefaultEngineAutoUpdate = true;
constexpr bool kDefaultEngineUseSystem = false;
constexpr HardwareAcceleration kDefaultHardwareAcceleration = HardwareAcceleration::Auto;
constexpr bool kDefaultHardwareVideoDecode = false;

template <typename Enum>
Enum enumFromInt(int value, Enum def, int count)
{
    return (value >= 0 && value < count) ? static_cast<Enum>(value) : def;
}

} // namespace

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_store(std::make_unique<QSettings>())
{
    qCDebug(lcSettings) << "settings store:" << m_store->fileName();
}

Settings::Settings(const QString& iniFilePath, QObject* parent)
    : QObject(parent)
    , m_store(std::make_unique<QSettings>(iniFilePath, QSettings::IniFormat))
{
    qCDebug(lcSettings) << "settings store (ini):" << m_store->fileName();
}

Settings::~Settings()
{
    m_store->sync();
}

// ---- helpers ---------------------------------------------------------------

bool Settings::boolValue(QLatin1StringView key, bool def) const
{
    return m_store->value(key, def).toBool();
}

bool Settings::storeBool(QLatin1StringView key, bool def, bool value)
{
    if (boolValue(key, def) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

int Settings::intValue(QLatin1StringView key, int def) const
{
    return m_store->value(key, def).toInt();
}

bool Settings::storeInt(QLatin1StringView key, int def, int value)
{
    if (intValue(key, def) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

QString Settings::stringValue(QLatin1StringView key, const QString& def) const
{
    return m_store->value(key, def).toString();
}

bool Settings::storeString(QLatin1StringView key, const QString& value)
{
    if (stringValue(key) == value) {
        return false;
    }
    m_store->setValue(key, value);
    return true;
}

// ---- window/ ---------------------------------------------------------------

QByteArray Settings::windowGeometry() const
{
    return m_store->value(keys::kWindowGeometry).toByteArray();
}

void Settings::setWindowGeometry(const QByteArray& geometry)
{
    m_store->setValue(keys::kWindowGeometry, geometry);
}

QByteArray Settings::windowState() const
{
    return m_store->value(keys::kWindowState).toByteArray();
}

void Settings::setWindowState(const QByteArray& state)
{
    m_store->setValue(keys::kWindowState, state);
}

QByteArray Settings::settingsDialogGeometry() const
{
    return m_store->value(keys::kSettingsDialogGeometry).toByteArray();
}

void Settings::setSettingsDialogGeometry(const QByteArray& geometry)
{
    m_store->setValue(keys::kSettingsDialogGeometry, geometry);
}

CloseAction Settings::closeAction() const
{
    return enumFromInt(intValue(keys::kCloseAction, static_cast<int>(kDefaultCloseAction)),
                       kDefaultCloseAction, 2);
}

void Settings::setCloseAction(CloseAction action)
{
    if (storeInt(keys::kCloseAction, static_cast<int>(kDefaultCloseAction), static_cast<int>(action))) {
        Q_EMIT closeActionChanged(action);
    }
}

int Settings::lastPage() const
{
    return std::max(0, intValue(keys::kLastPage, 0));
}

void Settings::setLastPage(int page)
{
    storeInt(keys::kLastPage, 0, std::max(0, page));
}

Settings::BrowserSession Settings::browserSession() const
{
    BrowserSession session;
    for (const QString& url : m_store->value(keys::kBrowserTabs).toStringList()) {
        if (!url.trimmed().isEmpty()) {
            session.urls << url.trimmed();
        }
    }
    session.current = std::clamp(m_store->value(keys::kBrowserTab, 0).toInt(), 0,
                                 std::max(0, static_cast<int>(session.urls.size()) - 1));
    return session;
}

void Settings::setBrowserSession(const BrowserSession& session)
{
    m_store->setValue(keys::kBrowserTabs, session.urls);
    m_store->setValue(keys::kBrowserTab, session.current);
}

QString Settings::whatsNewSeenVersion() const
{
    return stringValue(keys::kWhatsNewSeenVersion).trimmed();
}

void Settings::setWhatsNewSeenVersion(const QString& version)
{
    storeString(keys::kWhatsNewSeenVersion, version.trimmed());
}

// ---- general/ --------------------------------------------------------------

bool Settings::notifyOnDownloadFinish() const
{
    return boolValue(keys::kNotifyOnDownloadFinish, kDefaultNotifyOnDownloadFinish);
}

void Settings::setNotifyOnDownloadFinish(bool enabled)
{
    if (storeBool(keys::kNotifyOnDownloadFinish, kDefaultNotifyOnDownloadFinish, enabled)) {
        Q_EMIT notifyOnDownloadFinishChanged(enabled);
    }
}

bool Settings::trayEnabled() const
{
    return boolValue(keys::kTrayEnabled, kDefaultTrayEnabled);
}

void Settings::setTrayEnabled(bool enabled)
{
    if (storeBool(keys::kTrayEnabled, kDefaultTrayEnabled, enabled)) {
        Q_EMIT trayEnabledChanged(enabled);
    }
}

StartPage Settings::startPage() const
{
    return enumFromInt(intValue(keys::kStartPage, static_cast<int>(kDefaultStartPage)), kDefaultStartPage, 2);
}

void Settings::setStartPage(StartPage page)
{
    if (storeInt(keys::kStartPage, static_cast<int>(kDefaultStartPage), static_cast<int>(page))) {
        Q_EMIT generalChanged();
    }
}

bool Settings::showWhatsNew() const
{
    return boolValue(keys::kShowWhatsNew, kDefaultShowWhatsNew);
}

void Settings::setShowWhatsNew(bool enabled)
{
    if (storeBool(keys::kShowWhatsNew, kDefaultShowWhatsNew, enabled)) {
        Q_EMIT generalChanged();
    }
}

// ---- appearance/ -----------------------------------------------------------

Theme Settings::theme() const
{
    return enumFromInt(intValue(keys::kTheme, static_cast<int>(kDefaultTheme)), kDefaultTheme, 3);
}

void Settings::setTheme(Theme theme)
{
    if (storeInt(keys::kTheme, static_cast<int>(kDefaultTheme), static_cast<int>(theme))) {
        Q_EMIT themeChanged(theme);
    }
}

double Settings::interfaceScale() const
{
    return std::clamp(m_store->value(keys::kInterfaceScale, kDefaultInterfaceScale).toDouble(),
                      kMinInterfaceScale, kMaxInterfaceScale);
}

void Settings::setInterfaceScale(double scale)
{
    const double clamped = std::clamp(scale, kMinInterfaceScale, kMaxInterfaceScale);
    if (qFuzzyCompare(interfaceScale(), clamped)) {
        return;
    }
    m_store->setValue(keys::kInterfaceScale, clamped);
    Q_EMIT interfaceScaleChanged(clamped);
}

// ---- browser/ --------------------------------------------------------------

bool Settings::blockAds() const
{
    return boolValue(keys::kBlockAds, kDefaultBlockAds);
}

void Settings::setBlockAds(bool enabled)
{
    if (storeBool(keys::kBlockAds, kDefaultBlockAds, enabled)) {
        Q_EMIT blockAdsChanged(enabled);
        Q_EMIT browserChanged();
    }
}

bool Settings::doNotTrack() const
{
    return boolValue(keys::kDoNotTrack, kDefaultDoNotTrack);
}

void Settings::setDoNotTrack(bool enabled)
{
    if (storeBool(keys::kDoNotTrack, kDefaultDoNotTrack, enabled)) {
        Q_EMIT browserChanged();
    }
}

bool Settings::isEmptyStartPage(const QString& url)
{
    const QString trimmed = url.trimmed().toLower();
    return trimmed == kEmptyStartPage || trimmed == u"about:newtab"_s;
}

QString Settings::browserStartPage() const
{
    const QString v = stringValue(keys::kBrowserStartPage, QString(kDefaultBrowserStartPage)).trimmed();
    return v.isEmpty() ? QString(kDefaultBrowserStartPage) : v;
}

void Settings::setBrowserStartPage(const QString& url)
{
    const QString v = url.trimmed().isEmpty() ? QString(kDefaultBrowserStartPage) : url.trimmed();
    if (storeString(keys::kBrowserStartPage, v)) {
        Q_EMIT browserChanged();
    }
}

bool Settings::railExpanded() const
{
    return boolValue(keys::kRailExpanded, false);
}

void Settings::setRailExpanded(bool expanded)
{
    if (storeBool(keys::kRailExpanded, false, expanded)) {
        Q_EMIT generalChanged();
    }
}

bool Settings::restoreBrowserTabs() const
{
    return boolValue(keys::kRestoreBrowserTabs, kDefaultRestoreBrowserTabs);
}

void Settings::setRestoreBrowserTabs(bool enabled)
{
    if (storeBool(keys::kRestoreBrowserTabs, kDefaultRestoreBrowserTabs, enabled)) {
        Q_EMIT browserChanged();
    }
}

QString Settings::browserUserAgentPreset() const
{
    const QString v = stringValue(keys::kBrowserUserAgentPreset, u"default"_s).trimmed();
    return v.isEmpty() ? u"default"_s : v;
}

void Settings::setBrowserUserAgentPreset(const QString& id)
{
    const QString v = id.trimmed().isEmpty() ? u"default"_s : id.trimmed();
    if (storeString(keys::kBrowserUserAgentPreset, v)) {
        Q_EMIT browserUserAgentChanged();
        Q_EMIT browserChanged();
    }
}

QString Settings::browserUserAgent() const
{
    return stringValue(keys::kBrowserUserAgent, QString()).trimmed();
}

void Settings::setBrowserUserAgent(const QString& userAgent)
{
    if (storeString(keys::kBrowserUserAgent, userAgent.trimmed())) {
        Q_EMIT browserUserAgentChanged();
        Q_EMIT browserChanged();
    }
}

// ---- downloads/ ------------------------------------------------------------

QString Settings::downloadDirectory() const
{
    const QString stored = stringValue(keys::kDownloadDirectory);
    if (!stored.isEmpty() && QDir(stored).isAbsolute()) {
        return stored;
    }
    // ~/Downloads/Playlist Downloader: the folder every desktop shows for downloads.
    QString base = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(u"Playlist Downloader"_s);
}

bool Settings::hasDownloadDirectory() const
{
    return !stringValue(keys::kDownloadDirectory).isEmpty();
}

void Settings::setDownloadDirectory(const QString& directory)
{
    if (storeString(keys::kDownloadDirectory, directory)) {
        Q_EMIT downloadDirectoryChanged(downloadDirectory());
    }
}

FilenamePattern Settings::filenamePattern() const
{
    return enumFromInt(intValue(keys::kFilenamePattern, static_cast<int>(kDefaultFilenamePattern)),
                       kDefaultFilenamePattern, 3);
}

void Settings::setFilenamePattern(FilenamePattern pattern)
{
    if (storeInt(keys::kFilenamePattern, static_cast<int>(kDefaultFilenamePattern), static_cast<int>(pattern))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::organiseDownloads() const
{
    return boolValue(keys::kOrganiseDownloads, kDefaultOrganiseDownloads);
}

void Settings::setOrganiseDownloads(bool enabled)
{
    if (storeBool(keys::kOrganiseDownloads, kDefaultOrganiseDownloads, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::numberPlaylistFiles() const
{
    return boolValue(keys::kNumberPlaylistFiles, kDefaultNumberPlaylistFiles);
}

void Settings::setNumberPlaylistFiles(bool enabled)
{
    if (storeBool(keys::kNumberPlaylistFiles, kDefaultNumberPlaylistFiles, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

VideoQuality Settings::defaultQuality() const
{
    return enumFromInt(intValue(keys::kDefaultQuality, static_cast<int>(kDefaultQuality)), kDefaultQuality, 7);
}

void Settings::setDefaultQuality(VideoQuality quality)
{
    if (storeInt(keys::kDefaultQuality, static_cast<int>(kDefaultQuality), static_cast<int>(quality))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

Container Settings::defaultContainer() const
{
    return enumFromInt(intValue(keys::kDefaultContainer, static_cast<int>(kDefaultContainer)),
                       kDefaultContainer, 3);
}

void Settings::setDefaultContainer(Container container)
{
    if (storeInt(keys::kDefaultContainer, static_cast<int>(kDefaultContainer), static_cast<int>(container))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

AudioFormat Settings::defaultAudioFormat() const
{
    return enumFromInt(intValue(keys::kDefaultAudioFormat, static_cast<int>(kDefaultAudioFormat)),
                       kDefaultAudioFormat, 6);
}

void Settings::setDefaultAudioFormat(AudioFormat format)
{
    if (storeInt(keys::kDefaultAudioFormat, static_cast<int>(kDefaultAudioFormat), static_cast<int>(format))) {
        Q_EMIT downloadDefaultsChanged();
    }
}

int Settings::defaultAudioBitrate() const
{
    const int stored = intValue(keys::kDefaultAudioBitrate, kDefaultAudioBitrate);
    return std::ranges::find(kAudioBitrates, stored) != std::end(kAudioBitrates) ? stored
                                                                                 : kDefaultAudioBitrate;
}

void Settings::setDefaultAudioBitrate(int kbps)
{
    const int valid =
        std::ranges::find(kAudioBitrates, kbps) != std::end(kAudioBitrates) ? kbps : kDefaultAudioBitrate;
    if (storeInt(keys::kDefaultAudioBitrate, kDefaultAudioBitrate, valid)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

DownloadKind Settings::lastDownloadKind() const
{
    return enumFromInt(intValue(keys::kLastDownloadKind, static_cast<int>(kDefaultDownloadKind)),
                       kDefaultDownloadKind, 3);
}

void Settings::setLastDownloadKind(DownloadKind kind)
{
    if (storeInt(keys::kLastDownloadKind, static_cast<int>(kDefaultDownloadKind), static_cast<int>(kind))) {
        Q_EMIT downloadDefaultsChanged(); // Settings, Downloads shows it as the default kind
    }
}

int Settings::concurrentDownloads() const
{
    return std::clamp(intValue(keys::kConcurrentDownloads, kDefaultConcurrent), 1, kMaxConcurrentDownloads);
}

void Settings::setConcurrentDownloads(int count)
{
    const int clamped = std::clamp(count, 1, kMaxConcurrentDownloads);
    if (storeInt(keys::kConcurrentDownloads, kDefaultConcurrent, clamped)) {
        Q_EMIT concurrentDownloadsChanged(clamped);
    }
}

int Settings::speedLimitKbps() const
{
    return std::max(0, intValue(keys::kSpeedLimitKbps, kDefaultSpeedLimit));
}

void Settings::setSpeedLimitKbps(int kbps)
{
    const int clamped = std::max(0, kbps);
    if (storeInt(keys::kSpeedLimitKbps, kDefaultSpeedLimit, clamped)) {
        Q_EMIT speedLimitChanged(clamped);
    }
}

bool Settings::skipExisting() const
{
    return boolValue(keys::kSkipExisting, kDefaultSkipExisting);
}

void Settings::setSkipExisting(bool enabled)
{
    if (storeBool(keys::kSkipExisting, kDefaultSkipExisting, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::writePlaylistFile() const
{
    return boolValue(keys::kWritePlaylistFile, kDefaultWritePlaylistFile);
}

void Settings::setWritePlaylistFile(bool enabled)
{
    if (storeBool(keys::kWritePlaylistFile, kDefaultWritePlaylistFile, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::useSessionCookies() const
{
    return boolValue(keys::kUseSessionCookies, kDefaultUseSessionCookies);
}

void Settings::setUseSessionCookies(bool enabled)
{
    if (storeBool(keys::kUseSessionCookies, kDefaultUseSessionCookies, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::embedThumbnail() const
{
    return boolValue(keys::kEmbedThumbnail, kDefaultEmbedThumbnail);
}

void Settings::setEmbedThumbnail(bool enabled)
{
    if (storeBool(keys::kEmbedThumbnail, kDefaultEmbedThumbnail, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

bool Settings::embedMetadata() const
{
    return boolValue(keys::kEmbedMetadata, kDefaultEmbedMetadata);
}

void Settings::setEmbedMetadata(bool enabled)
{
    if (storeBool(keys::kEmbedMetadata, kDefaultEmbedMetadata, enabled)) {
        Q_EMIT downloadDefaultsChanged();
    }
}

QStringList Settings::subtitleLanguages() const
{
    return m_store->value(keys::kSubtitleLanguages).toStringList();
}

void Settings::setSubtitleLanguages(const QStringList& languages)
{
    if (subtitleLanguages() == languages) {
        return;
    }
    m_store->setValue(keys::kSubtitleLanguages, languages);
    Q_EMIT downloadDefaultsChanged();
}

// ---- search/ ---------------------------------------------------------------

int Settings::searchResultsPerPage() const
{
    return std::clamp(intValue(keys::kSearchResultsPerPage, kDefaultSearchResultsPerPage), kMinSearchResultsPerPage,
                      kMaxSearchResultsPerPage);
}

void Settings::setSearchResultsPerPage(int count)
{
    const int clamped = std::clamp(count, kMinSearchResultsPerPage, kMaxSearchResultsPerPage);
    if (storeInt(keys::kSearchResultsPerPage, kDefaultSearchResultsPerPage, clamped)) {
        Q_EMIT searchChanged();
    }
}

bool Settings::searchSuggestions() const
{
    return boolValue(keys::kSearchSuggestions, kDefaultSearchSuggestions);
}

void Settings::setSearchSuggestions(bool enabled)
{
    if (storeBool(keys::kSearchSuggestions, kDefaultSearchSuggestions, enabled)) {
        Q_EMIT searchChanged();
    }
}

bool Settings::searchGridView() const
{
    return boolValue(keys::kSearchGridView, kDefaultSearchGridView);
}

void Settings::setSearchGridView(bool grid)
{
    if (storeBool(keys::kSearchGridView, kDefaultSearchGridView, grid)) {
        Q_EMIT searchChanged();
    }
}

bool Settings::keepSearchHistory() const
{
    return boolValue(keys::kSearchKeepHistory, kDefaultKeepSearchHistory);
}

void Settings::setKeepSearchHistory(bool enabled)
{
    if (storeBool(keys::kSearchKeepHistory, kDefaultKeepSearchHistory, enabled)) {
        Q_EMIT searchChanged();
    }
}

QStringList Settings::recentQueries() const
{
    QStringList out;
    for (const QString& query : m_store->value(keys::kSearchRecentQueries).toStringList()) {
        const QString trimmed = query.trimmed();
        if (!trimmed.isEmpty() && !out.contains(trimmed, Qt::CaseInsensitive)) {
            out << trimmed;
        }
        if (out.size() >= kMaxRecentQueries) {
            break;
        }
    }
    return out;
}

void Settings::addRecentQuery(const QString& query)
{
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QStringList list = recentQueries();
    list.removeIf([&trimmed](const QString& q) { return q.compare(trimmed, Qt::CaseInsensitive) == 0; });
    list.prepend(trimmed);
    while (list.size() > kMaxRecentQueries) {
        list.removeLast();
    }
    m_store->setValue(keys::kSearchRecentQueries, list);
    Q_EMIT searchChanged();
}

void Settings::clearRecentQueries()
{
    if (recentQueries().isEmpty()) {
        return;
    }
    m_store->remove(keys::kSearchRecentQueries);
    Q_EMIT searchChanged();
}

// ---- engine/ ---------------------------------------------------------------

bool Settings::engineAutoUpdate() const
{
    return boolValue(keys::kEngineAutoUpdate, kDefaultEngineAutoUpdate);
}

void Settings::setEngineAutoUpdate(bool enabled)
{
    if (storeBool(keys::kEngineAutoUpdate, kDefaultEngineAutoUpdate, enabled)) {
        Q_EMIT engineConfigChanged();
    }
}

bool Settings::engineUseSystem() const
{
    return boolValue(keys::kEngineUseSystem, kDefaultEngineUseSystem);
}

void Settings::setEngineUseSystem(bool enabled)
{
    if (storeBool(keys::kEngineUseSystem, kDefaultEngineUseSystem, enabled)) {
        Q_EMIT engineConfigChanged();
    }
}

QString Settings::engineSystemPath() const
{
    return stringValue(keys::kEngineSystemPath);
}

void Settings::setEngineSystemPath(const QString& path)
{
    if (storeString(keys::kEngineSystemPath, path)) {
        Q_EMIT engineConfigChanged();
    }
}

QDateTime Settings::engineLastCheck() const
{
    return m_store->value(keys::kEngineLastCheck).toDateTime();
}

void Settings::setEngineLastCheck(const QDateTime& when)
{
    m_store->setValue(keys::kEngineLastCheck, when);
}

// ---- advanced/ -------------------------------------------------------------

HardwareAcceleration Settings::hardwareAcceleration() const
{
    return enumFromInt(intValue(keys::kHardwareAcceleration, static_cast<int>(kDefaultHardwareAcceleration)),
                       kDefaultHardwareAcceleration, 3);
}

void Settings::setHardwareAcceleration(HardwareAcceleration mode)
{
    if (storeInt(keys::kHardwareAcceleration, static_cast<int>(kDefaultHardwareAcceleration),
                 static_cast<int>(mode))) {
        // An explicit choice restarts the GPU trial (ADR-032 lineage).
        setGpuAutoDisabled(false);
        setGpuProbeStrikes(0);
        Q_EMIT hardwareAccelerationChanged(mode);
    }
}

bool Settings::hardwareVideoDecode() const
{
    return boolValue(keys::kHardwareVideoDecode, kDefaultHardwareVideoDecode);
}

void Settings::setHardwareVideoDecode(bool enabled)
{
    if (storeBool(keys::kHardwareVideoDecode, kDefaultHardwareVideoDecode, enabled)) {
        Q_EMIT hardwareVideoDecodeChanged(enabled);
    }
}

bool Settings::gpuAutoDisabled() const
{
    return boolValue(keys::kGpuAutoDisabled, false);
}

void Settings::setGpuAutoDisabled(bool disabled)
{
    if (storeBool(keys::kGpuAutoDisabled, false, disabled)) {
        Q_EMIT gpuAutoDisabledChanged(disabled);
    }
}

int Settings::gpuProbeStrikes() const
{
    return std::max(0, intValue(keys::kGpuProbeStrikes, 0));
}

void Settings::setGpuProbeStrikes(int strikes)
{
    storeInt(keys::kGpuProbeStrikes, 0, std::max(0, strikes));
}

bool Settings::gpuFallbackNotice() const
{
    return boolValue(keys::kGpuFallbackNotice, false);
}

void Settings::setGpuFallbackNotice(bool pending)
{
    storeBool(keys::kGpuFallbackNotice, false, pending);
}

bool Settings::videoDecodeFallbackNotice() const
{
    return boolValue(keys::kVideoDecodeFallbackNotice, false);
}

void Settings::setVideoDecodeFallbackNotice(bool pending)
{
    storeBool(keys::kVideoDecodeFallbackNotice, false, pending);
}

DailyAllowance Settings::freeDownloads() const
{
    return {stringValue(keys::kFreeDownloadsDay), intValue(keys::kFreeDownloadsUsed, 0)};
}

void Settings::setFreeDownloads(const DailyAllowance& allowance)
{
    storeString(keys::kFreeDownloadsDay, allowance.day);
    storeInt(keys::kFreeDownloadsUsed, 0, allowance.used);
}

// ---- misc ------------------------------------------------------------------

void Settings::sync()
{
    m_store->sync();
}

void Settings::resetToDefaults()
{
    // Not preferences: they survive the reset.
    const DailyAllowance downloads = freeDownloads();
    const QByteArray geometry = windowGeometry();
    const QByteArray state = windowState();
    const QString seen = whatsNewSeenVersion();
    const QStringList allKeys = m_store->allKeys();
    for (const QString& key : allKeys) {
        m_store->remove(key);
    }
    setFreeDownloads(downloads);
    setWindowGeometry(geometry);
    setWindowState(state);
    setWhatsNewSeenVersion(seen);
    m_store->sync();
    Q_EMIT closeActionChanged(closeAction());
    Q_EMIT notifyOnDownloadFinishChanged(notifyOnDownloadFinish());
    Q_EMIT trayEnabledChanged(trayEnabled());
    Q_EMIT generalChanged();
    Q_EMIT themeChanged(theme());
    Q_EMIT interfaceScaleChanged(interfaceScale());
    Q_EMIT blockAdsChanged(blockAds());
    Q_EMIT browserUserAgentChanged();
    Q_EMIT browserChanged();
    Q_EMIT downloadDirectoryChanged(downloadDirectory());
    Q_EMIT downloadDefaultsChanged();
    Q_EMIT concurrentDownloadsChanged(concurrentDownloads());
    Q_EMIT speedLimitChanged(speedLimitKbps());
    Q_EMIT searchChanged();
    Q_EMIT engineConfigChanged();
    Q_EMIT hardwareAccelerationChanged(hardwareAcceleration());
    Q_EMIT hardwareVideoDecodeChanged(hardwareVideoDecode());
}

QString Settings::fileName() const
{
    return m_store->fileName();
}

} // namespace pldl::core
