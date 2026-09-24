#include "ui/settings_dialog.h"

#include "core/downloads/engine_spec.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/a11y.h"
#include "ui/icons.h"
#include "ui/message_sheet.h"
#include "ui/pldl_style.h"
#include "ui/settings_form.h"
#include "web/request_interceptor.h"
#include "web/user_agent.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineProfile>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {

constexpr int kNavWidth = 200;
constexpr int kControlWidth = 220;
constexpr int kBlockedRefreshMs = 1000;

/// A bare check box for a row; `store` runs on a user change only.
template <typename Store>
QCheckBox* makeCheck(QDialog* owner, const bool& loading, Store store)
{
    auto* box = new QCheckBox(owner);
    QObject::connect(box, &QCheckBox::toggled, owner, [&loading, store](bool on) {
        if (!loading) {
            store(on);
        }
    });
    return box;
}

/// A combo of (label, value) pairs; `store` gets the chosen value.
template <typename Store>
QComboBox* makeCombo(QDialog* owner, const bool& loading, const QList<QPair<QString, int>>& items,
                     Store store)
{
    auto* combo = new QComboBox(owner);
    for (const auto& [label, value] : items) {
        combo->addItem(label, value);
    }
    combo->setMinimumWidth(kControlWidth);
    QObject::connect(combo, &QComboBox::currentIndexChanged, owner, [&loading, store, combo](int) {
        if (!loading) {
            store(combo->currentData().toInt());
        }
    });
    return combo;
}

template <typename Store>
QSpinBox* makeSpin(QDialog* owner, const bool& loading, int from, int to, int step, Store store)
{
    auto* spin = new QSpinBox(owner);
    spin->setRange(from, to);
    spin->setSingleStep(step);
    spin->setMinimumWidth(130);
    QObject::connect(spin, &QSpinBox::valueChanged, owner, [&loading, store](int value) {
        if (!loading) {
            store(value);
        }
    });
    return spin;
}

/// A text field that stores on Enter or focus loss.
template <typename Store>
QLineEdit* makeEdit(QDialog* owner, const bool& loading, const QString& placeholder, Store store)
{
    auto* edit = new QLineEdit(owner);
    edit->setPlaceholderText(placeholder);
    edit->setMinimumWidth(kControlWidth + 60);
    QObject::connect(edit, &QLineEdit::editingFinished, owner, [&loading, store, edit] {
        if (!loading) {
            store(edit->text());
        }
    });
    return edit;
}

void selectData(QComboBox* combo, int value)
{
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

/// Line edits keep what is being typed: only an idle field follows the setting.
void setIdleText(QLineEdit* edit, const QString& text)
{
    if (!edit->hasFocus() && edit->text() != text) {
        edit->setText(text);
        edit->setCursorPosition(0); // a long folder path shows its start, not its tail
    }
}

QString joinLanguages(const QStringList& languages)
{
    return languages.join(u", "_s);
}

QStringList splitLanguages(const QString& text)
{
    QStringList out;
    for (const QString& part : text.split(QRegularExpression(u"[,;\\s]+"_s), Qt::SkipEmptyParts)) {
        const QString code = part.trimmed().toLower();
        if (!code.isEmpty() && !out.contains(code)) {
            out << code;
        }
    }
    return out;
}

} // namespace

SettingsDialog::SettingsDialog(core::Settings& settings, core::ThemeService& theme,
                               services::EngineManager& engine, web::RequestInterceptor& interceptor,
                               bool trayAvailable, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_engine(engine)
    , m_interceptor(interceptor)
    , m_trayAvailable(trayAvailable)
{
    setWindowTitle(tr("Settings"));
    setModal(false);
    resize(860, 600);
    setMinimumSize(720, 480);
    setupUi();
    loadValues();
    connectSettings();
    installWheelGuards();
    refreshEngine(m_engine.status());
    connect(&m_engine, &services::EngineManager::statusChanged, this, &SettingsDialog::refreshEngine);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyNavIcons(); });
    m_blockedTimer = new QTimer(this);
    m_blockedTimer->setInterval(kBlockedRefreshMs);
    connect(m_blockedTimer, &QTimer::timeout, this, &SettingsDialog::refreshBlockedCount);
    const QByteArray geometry = m_settings.settingsDialogGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    a11y::nameControlsFromLabels(this);
}

// ---- shell -----------------------------------------------------------------

void SettingsDialog::setupUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setObjectName(u"settingsNav"_s);
    m_nav->setAccessibleName(tr("Settings pages"));
    m_nav->setProperty("pldlNav", true);
    m_nav->setFixedWidth(kNavWidth);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setSpacing(0);
    for (const QString& name :
         {tr("General"), tr("Appearance"), tr("Downloads"), tr("Browser"), tr("Search"), tr("Advanced")}) {
        m_nav->addItem(name);
    }
    root->addWidget(m_nav);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::NoFrame);
    line->setProperty("pldlVSeparator", true);
    root->addWidget(line);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildGeneral());
    m_pages->addWidget(buildAppearance());
    m_pages->addWidget(buildDownloads());
    m_pages->addWidget(buildBrowser());
    m_pages->addWidget(buildSearch());
    m_pages->addWidget(buildAdvanced());
    Q_ASSERT(m_pages->count() == kPageCount);
    root->addWidget(m_pages, 1);
    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        m_pages->setCurrentIndex(row);
        if (row == Browser) {
            refreshBlockedCount();
        }
    });
    m_nav->setCurrentRow(0);
    applyNavIcons();
}

void SettingsDialog::applyNavIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const QStringList names{u"general"_s, u"palette"_s, u"downloads"_s,
                            u"globe"_s,   u"search"_s,  u"advanced"_s};
    for (int i = 0; i < m_nav->count() && i < names.size(); ++i) {
        m_nav->item(i)->setIcon(icons::themed(names.at(i), t.text));
    }
}

int SettingsDialog::pageIndex(const QString& name)
{
    static const QHash<QString, int> kNames{
        {u"general"_s, General}, {u"appearance"_s, Appearance}, {u"downloads"_s, Downloads},
        {u"browser"_s, Browser}, {u"search"_s, Search},         {u"advanced"_s, Advanced},
    };
    return kNames.value(name.trimmed().toLower(), General);
}

void SettingsDialog::showPage(int index)
{
    m_nav->setCurrentRow(std::clamp(index, 0, m_nav->count() - 1));
}

void SettingsDialog::showPage(const QString& name)
{
    showPage(pageIndex(name));
}

int SettingsDialog::currentPage() const
{
    return m_pages->currentIndex();
}

void SettingsDialog::closeEvent(QCloseEvent* event)
{
    m_settings.setSettingsDialogGeometry(saveGeometry());
    QDialog::closeEvent(event);
}

void SettingsDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    refreshBlockedCount();
    m_blockedTimer->start();
}

void SettingsDialog::hideEvent(QHideEvent* event)
{
    m_blockedTimer->stop();
    QDialog::hideEvent(event);
}

// ---- pages -----------------------------------------------------------------

QWidget* SettingsDialog::buildGeneral()
{
    using namespace settings_form;
    m_startPage = makeCombo(this, m_loading,
                            {{tr("Search"), static_cast<int>(core::StartPage::Search)},
                             {tr("Last page"), static_cast<int>(core::StartPage::LastPage)}},
                            [this](int v) { m_settings.setStartPage(static_cast<core::StartPage>(v)); });
    m_closeAction =
        makeCombo(this, m_loading,
                  {{tr("Quit"), static_cast<int>(core::CloseAction::Quit)},
                   {tr("Keep in tray"), static_cast<int>(core::CloseAction::MinimizeToTray)}},
                  [this](int v) { m_settings.setCloseAction(static_cast<core::CloseAction>(v)); });
    m_closeAction->setEnabled(m_trayAvailable);
    m_keepHistory = makeCheck(this, m_loading, [this](bool on) { m_settings.setKeepSearchHistory(on); });
    m_showWhatsNew = makeCheck(this, m_loading, [this](bool on) { m_settings.setShowWhatsNew(on); });
    m_notifyFinish =
        makeCheck(this, m_loading, [this](bool on) { m_settings.setNotifyOnDownloadFinish(on); });

    QWidget* startup = card(tr("Startup and closing"),
                            {row(tr("Start page"), m_startPage),
                             row(tr("When closing"), m_closeAction,
                                 m_trayAvailable ? QString() : tr("This desktop has no system tray."))},
                            this);
    QWidget* behaviour = card(tr("Behaviour"),
                              {row(tr("Keep search history"), m_keepHistory,
                                   tr("Recent searches come back as chips on the Search page.")),
                               row(tr("Show What's new after updates"), m_showWhatsNew),
                               row(tr("Notifications on finish"), m_notifyFinish,
                                   tr("A desktop notification when a download completes."))},
                              this);
    return page(tr("General"), tr("How the app starts, closes and keeps you informed."), {startup, behaviour},
                this);
}

QWidget* SettingsDialog::buildAppearance()
{
    using namespace settings_form;
    m_themeChoice = makeCombo(this, m_loading,
                              {{tr("System"), static_cast<int>(core::Theme::System)},
                               {tr("Light"), static_cast<int>(core::Theme::Light)},
                               {tr("Dark"), static_cast<int>(core::Theme::Dark)}},
                              [this](int v) { m_settings.setTheme(static_cast<core::Theme>(v)); });
    m_themeChoice->setObjectName(u"themeCombo"_s);
    m_scale = makeSpin(this, m_loading, qRound(core::Settings::kMinInterfaceScale * 100),
                       qRound(core::Settings::kMaxInterfaceScale * 100), 25,
                       [this](int v) { m_settings.setInterfaceScale(v / 100.0); });
    m_scale->setSuffix(u" %"_s);
    QWidget* look =
        card(tr("Look"),
             {row(tr("Theme"), m_themeChoice, tr("System follows the desktop's light or dark setting.")),
              row(tr("Interface scale"), m_scale, tr("Takes effect after a restart."))},
             this);
    return page(tr("Appearance"), tr("Theme and size."), {look}, this);
}

QWidget* SettingsDialog::buildDownloads()
{
    using namespace settings_form;
    auto* folderBox = new QWidget(this);
    auto* folderLayout = new QHBoxLayout(folderBox);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    folderLayout->setSpacing(8);
    m_folder = makeEdit(this, m_loading, QString(), [this](const QString& text) {
        const QString dir = text.trimmed();
        if (!dir.isEmpty() && QDir(dir).isAbsolute()) {
            m_settings.setDownloadDirectory(dir);
        }
        m_folder->setText(m_settings.downloadDirectory());
    });
    m_folder->setAccessibleName(tr("Download folder"));
    folderLayout->addWidget(m_folder, 1);
    auto* change = new QPushButton(tr("Change…"), folderBox);
    connect(change, &QPushButton::clicked, this, &SettingsDialog::chooseDownloadFolder);
    folderLayout->addWidget(change);
    m_playlistFolder = makeCheck(this, m_loading, [this](bool on) { m_settings.setOrganiseDownloads(on); });
    m_numberFiles = makeCheck(this, m_loading, [this](bool on) { m_settings.setNumberPlaylistFiles(on); });
    m_skipExisting = makeCheck(this, m_loading, [this](bool on) { m_settings.setSkipExisting(on); });
    m_playlistFile = makeCheck(this, m_loading, [this](bool on) { m_settings.setWritePlaylistFile(on); });
    m_playlistFile->setObjectName(u"playlistFileCheck"_s);
    QWidget* files =
        card(tr("Files"),
             {row(tr("Folder"), folderBox), row(tr("Put each playlist in its own folder"), m_playlistFolder),
              row(tr("Number files in playlist order"), m_numberFiles, tr("\"01 - Title\"")),
              row(tr("Skip already downloaded"), m_skipExisting,
                  tr("A file that is already in the folder is left alone.")),
              row(tr("Write a playlist file"), m_playlistFile,
                  tr("An .m3u8 next to the videos of a playlist, so a media player plays them in order."))},
             this);

    m_concurrent = makeSpin(this, m_loading, 1, core::Settings::kMaxConcurrentDownloads, 1,
                            [this](int v) { m_settings.setConcurrentDownloads(v); });
    m_concurrent->setObjectName(u"concurrentSpin"_s);
    m_speedLimit =
        makeSpin(this, m_loading, 0, 1000000, 256, [this](int v) { m_settings.setSpeedLimitKbps(v); });
    m_speedLimit->setSuffix(tr(" KB/s"));
    m_speedLimit->setSpecialValueText(tr("No limit"));
    m_sessionCookies = makeCheck(this, m_loading, [this](bool on) { m_settings.setUseSessionCookies(on); });
    QWidget* transfer = card(tr("Transfer"),
                             {row(tr("Concurrent downloads"), m_concurrent),
                              row(tr("Speed limit"), m_speedLimit, tr("0 = no limit.")),
                              row(tr("Use my YouTube sign-in"), m_sessionCookies,
                                  tr("Downloads see what you see in the built-in browser: members-only and "
                                     "age-restricted videos."))},
                             this);
    return page(tr("Downloads"), tr("Where files go and what a download picks by default."),
                {files, buildDownloadDefaults(), transfer, buildEngineCard()}, this);
}

QWidget* SettingsDialog::buildDownloadDefaults()
{
    using namespace settings_form;
    m_kind = makeCombo(this, m_loading,
                       {{tr("Video"), static_cast<int>(core::DownloadKind::Video)},
                        {tr("Audio only"), static_cast<int>(core::DownloadKind::Audio)}},
                       [this](int v) { m_settings.setLastDownloadKind(static_cast<core::DownloadKind>(v)); });
    m_quality =
        makeCombo(this, m_loading,
                  {{tr("Best available"), static_cast<int>(core::VideoQuality::Best)},
                   {tr("2160p (4K)"), static_cast<int>(core::VideoQuality::Q2160)},
                   {tr("1440p"), static_cast<int>(core::VideoQuality::Q1440)},
                   {tr("1080p"), static_cast<int>(core::VideoQuality::Q1080)},
                   {tr("720p"), static_cast<int>(core::VideoQuality::Q720)},
                   {tr("480p"), static_cast<int>(core::VideoQuality::Q480)},
                   {tr("360p"), static_cast<int>(core::VideoQuality::Q360)}},
                  [this](int v) { m_settings.setDefaultQuality(static_cast<core::VideoQuality>(v)); });
    m_container =
        makeCombo(this, m_loading,
                  {{u"MP4"_s, static_cast<int>(core::Container::Mp4)},
                   {u"MKV"_s, static_cast<int>(core::Container::Mkv)},
                   {u"WebM"_s, static_cast<int>(core::Container::Webm)}},
                  [this](int v) { m_settings.setDefaultContainer(static_cast<core::Container>(v)); });
    m_audioFormat =
        makeCombo(this, m_loading,
                  {{tr("Best (as published)"), static_cast<int>(core::AudioFormat::Best)},
                   {u"MP3"_s, static_cast<int>(core::AudioFormat::Mp3)},
                   {u"M4A"_s, static_cast<int>(core::AudioFormat::M4a)},
                   {u"Opus"_s, static_cast<int>(core::AudioFormat::Opus)},
                   {u"FLAC"_s, static_cast<int>(core::AudioFormat::Flac)},
                   {u"WAV"_s, static_cast<int>(core::AudioFormat::Wav)}},
                  [this](int v) { m_settings.setDefaultAudioFormat(static_cast<core::AudioFormat>(v)); });
    m_audioBitrate =
        makeCombo(this, m_loading, {{tr("Best"), 0}, {tr("192 kbps"), 192}, {tr("128 kbps"), 128}},
                  [this](int v) { m_settings.setDefaultAudioBitrate(v); });
    m_subtitles = makeEdit(this, m_loading, tr("en, de"), [this](const QString& text) {
        m_settings.setSubtitleLanguages(splitLanguages(text));
        m_subtitles->setText(joinLanguages(m_settings.subtitleLanguages()));
    });
    m_embedThumbnail = makeCheck(this, m_loading, [this](bool on) { m_settings.setEmbedThumbnail(on); });
    m_embedMetadata = makeCheck(this, m_loading, [this](bool on) { m_settings.setEmbedMetadata(on); });
    return card(tr("Defaults for new downloads"),
                {row(tr("Default kind"), m_kind), row(tr("Default video quality"), m_quality),
                 row(tr("Default container"), m_container), row(tr("Default audio format"), m_audioFormat),
                 row(tr("Audio bitrate"), m_audioBitrate),
                 row(tr("Subtitles"), m_subtitles, tr("Language codes, comma separated. Empty: none.")),
                 row(tr("Embed thumbnail"), m_embedThumbnail),
                 row(tr("Embed metadata"), m_embedMetadata, tr("Title, artist and chapters in the file."))},
                this);
}

QWidget* SettingsDialog::buildEngineCard()
{
    using namespace settings_form;
    auto* statusRow = new QWidget(this);
    auto* h = new QHBoxLayout(statusRow);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    auto* text = new QWidget(statusRow);
    auto* v = new QVBoxLayout(text);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    m_engineStatus = new QLabel(text);
    m_engineStatus->setObjectName(u"engineStatus"_s);
    QFont statusFont = m_engineStatus->font();
    statusFont.setWeight(QFont::Medium);
    m_engineStatus->setFont(statusFont);
    m_engineStatus->setWordWrap(true);
    v->addWidget(m_engineStatus);
    m_engineDetail = muted(QString(), text);
    m_engineDetail->setObjectName(u"engineDetail"_s);
    v->addWidget(m_engineDetail);
    h->addWidget(text, 1, Qt::AlignVCenter);
    m_engineCheck = new QPushButton(tr("Check for updates"), statusRow);
    connect(m_engineCheck, &QPushButton::clicked, this, [this] { m_engine.checkForUpdates(true); });
    h->addWidget(m_engineCheck, 0, Qt::AlignVCenter);
    m_engineUpdate = new QPushButton(tr("Update now"), statusRow);
    m_engineUpdate->setProperty("pldlPrimary", true);
    connect(m_engineUpdate, &QPushButton::clicked, this, [this] { m_engine.update(); });
    h->addWidget(m_engineUpdate, 0, Qt::AlignVCenter);
    m_engineSetup = new QPushButton(tr("Set up"), statusRow);
    m_engineSetup->setProperty("pldlPrimary", true);
    connect(m_engineSetup, &QPushButton::clicked, this, &SettingsDialog::engineSetupRequested);
    h->addWidget(m_engineSetup, 0, Qt::AlignVCenter);

    m_engineAutoUpdate = makeCheck(this, m_loading, [this](bool on) { m_settings.setEngineAutoUpdate(on); });
    m_engineUseSystem = makeCheck(this, m_loading, [this](bool on) { m_settings.setEngineUseSystem(on); });
    m_engineSystemPath =
        makeEdit(this, m_loading, tr("Empty: the yt-dlp found on your PATH"),
                 [this](const QString& path) { m_settings.setEngineSystemPath(path.trimmed()); });
    return card(tr("Engine"),
                {statusRow, row(tr("Auto-update daily"), m_engineAutoUpdate),
                 row(tr("Use an engine already on this system"), m_engineUseSystem,
                     tr("For experts: a yt-dlp you maintain yourself. Applied on the next start.")),
                 row(tr("Engine path"), m_engineSystemPath)},
                this);
}

QWidget* SettingsDialog::buildBrowser()
{
    using namespace settings_form;
    // Start page: an empty tab (default), YouTube, or an address of the user's
    // own; the address field shows only for the custom choice.
    enum StartChoice
    {
        EmptyTab,
        YouTube,
        Custom,
    };
    m_browserStartChoice = makeCombo(this, m_loading,
                                     {{tr("Empty tab"), EmptyTab}, {tr("YouTube"), YouTube}, {tr("Custom address"), Custom}},
                                     [this](int choice) {
                                         if (choice == EmptyTab) {
                                             m_settings.setBrowserStartPage(QString(core::Settings::kEmptyStartPage));
                                         } else if (choice == YouTube) {
                                             m_settings.setBrowserStartPage(QString(core::Settings::kYouTubeStartPage));
                                         } else if (!m_browserStart->text().trimmed().isEmpty()) {
                                             m_settings.setBrowserStartPage(m_browserStart->text());
                                         }
                                         m_browserStartRow->setVisible(choice == Custom);
                                         if (choice == Custom) {
                                             m_browserStart->setFocus();
                                         }
                                     });
    m_browserStartChoice->setObjectName(u"startPageCombo"_s);
    m_browserStart = makeEdit(this, m_loading, u"https://example.com/"_s, [this](const QString& text) {
        if (!text.trimmed().isEmpty()) {
            m_settings.setBrowserStartPage(text);
        }
    });
    m_browserStart->setObjectName(u"startPageEdit"_s);
    m_restoreTabs = makeCheck(this, m_loading, [this](bool on) { m_settings.setRestoreBrowserTabs(on); });
    m_blockAds = makeCheck(this, m_loading, [this](bool on) { m_settings.setBlockAds(on); });
    m_doNotTrack = makeCheck(this, m_loading, [this](bool on) { m_settings.setDoNotTrack(on); });
    m_blockedCount = muted(QString(), this);
    m_blockedCount->setObjectName(u"blockedCount"_s);

    m_browserStartRow = row(tr("Address"), m_browserStart);
    QWidget* pages = card(tr("Pages"),
                          {row(tr("Start page"), m_browserStartChoice, tr("What a new browser tab opens with.")),
                           m_browserStartRow,
                           row(tr("Restore tabs"), m_restoreTabs, tr("Reopen last time's tabs on start."))},
                          this);
    // The blocked-count line sits under Block ads as a row of its own.
    QWidget* privacy = card(
        tr("Privacy"),
        {row(tr("Block ads"), m_blockAds), m_blockedCount,
         row(tr("Do Not Track"), m_doNotTrack, tr("Asks every site not to track you. Sites may ignore it."))},
        this);
    QWidget* identity =
        card(tr("Identity"),
             {row(tr("Browser identity"), buildIdentityPicker(),
                  tr("What sites are told about this browser. The default is fine for YouTube."))},
             this);
    return page(tr("Browser"), tr("The built-in browser and the player."), {pages, privacy, identity}, this);
}

QWidget* SettingsDialog::buildIdentityPicker()
{
    auto* identityBox = new QWidget(this);
    auto* identityLayout = new QVBoxLayout(identityBox);
    identityLayout->setContentsMargins(0, 0, 0, 0);
    identityLayout->setSpacing(6);
    m_identity = new QComboBox(identityBox);
    m_identity->setObjectName(u"identityCombo"_s);
    m_identity->setMinimumWidth(kControlWidth + 60);
    const QString engineDefault = QWebEngineProfile::defaultProfile()->httpUserAgent();
    for (const web::UserAgentPreset& preset : web::userAgentPresets(engineDefault)) {
        m_identity->addItem(preset.label, preset.id);
    }
    m_identity->addItem(tr("Custom"), QString(web::kUserAgentPresetCustom));
    m_identityText = makeEdit(this, m_loading, web::sanitizeUserAgent(engineDefault),
                              [this](const QString& text) { m_settings.setBrowserUserAgent(text); });
    m_identityText->setObjectName(u"identityField"_s);
    m_identityText->setAccessibleName(tr("Custom browser identity"));
    m_identityText->hide();
    identityLayout->addWidget(m_identity);
    identityLayout->addWidget(m_identityText);
    connect(m_identity, &QComboBox::currentIndexChanged, this, [this](int index) {
        const bool custom = m_identity->itemData(index).toString() == web::kUserAgentPresetCustom;
        m_identityText->setVisible(custom);
        if (m_loading) {
            return;
        }
        m_settings.setBrowserUserAgentPreset(m_identity->itemData(index).toString());
        if (custom) {
            m_identityText->setFocus();
            m_identityText->selectAll();
        }
    });
    return identityBox;
}

QWidget* SettingsDialog::buildSearch()
{
    using namespace settings_form;
    m_resultsPerPage = makeSpin(this, m_loading, core::Settings::kMinSearchResultsPerPage,
                                core::Settings::kMaxSearchResultsPerPage, 10,
                                [this](int v) { m_settings.setSearchResultsPerPage(v); });
    m_resultsPerPage->setObjectName(u"resultsPerPageSpin"_s);
    QWidget* search = card(tr("Search"), {row(tr("Results per page"), m_resultsPerPage)}, this);
    return page(tr("Search"), tr("How many results a search brings back at a time."), {search}, this);
}

QWidget* SettingsDialog::buildAdvanced()
{
    using namespace settings_form;
    m_hardware = makeCombo(
        this, m_loading,
        {{tr("Auto"), static_cast<int>(core::HardwareAcceleration::Auto)},
         {tr("On"), static_cast<int>(core::HardwareAcceleration::On)},
         {tr("Off"), static_cast<int>(core::HardwareAcceleration::Off)}},
        [this](int v) { m_settings.setHardwareAcceleration(static_cast<core::HardwareAcceleration>(v)); });
    QWidget* graphics = card(tr("Graphics"),
                             {row(tr("Hardware acceleration"), m_hardware,
                                  tr("Off helps with driver glitches. Takes effect after a restart."))},
                             this);

    auto button = [this](const QString& caption, auto signal, bool danger = false) {
        auto* b = new QPushButton(caption, this);
        if (danger) {
            b->setProperty("pldlDanger", true);
        }
        connect(b, &QPushButton::clicked, this, signal);
        return b;
    };
    QWidget* storage =
        card(tr("Storage and session"),
             {row(tr("Clear cache"), button(tr("Clear cache"), &SettingsDialog::clearCacheRequested),
                  tr("Pages and images the browser keeps for speed. Your sign-in stays.")),
              row(tr("Sign out and clear session"),
                  button(tr("Sign out…"), &SettingsDialog::clearSessionRequested, true),
                  tr("Removes cookies, site data and the sign-in, then restarts the app.")),
              row(tr("Reset permissions"), button(tr("Reset"), &SettingsDialog::resetPermissionsRequested),
                  tr("Forgets every site permission you allowed or blocked."))},
             this);
    auto* reset = new QPushButton(tr("Reset…"), this);
    reset->setObjectName(u"resetSettings"_s);
    reset->setProperty("pldlDanger", true);
    connect(reset, &QPushButton::clicked, this, &SettingsDialog::resetSettings);
    QWidget* support =
        card(tr("Support"),
             {row(tr("Open log folder"), button(tr("Open folder"), &SettingsDialog::openLogFolderRequested)),
              row(tr("Copy diagnostics"), button(tr("Copy"), &SettingsDialog::copyDiagnosticsRequested),
                  tr("Versions, paths and recent log lines for a bug report. Never your session.")),
              row(tr("Reset settings"), reset, tr("Every option back to its default."))},
             this);
    return page(tr("Advanced"), tr("Graphics, storage and diagnostics."), {graphics, storage, support}, this);
}

// ---- values ----------------------------------------------------------------

void SettingsDialog::connectSettings()
{
    const auto reload = [this] { loadValues(); };
    core::Settings& s = m_settings;
    connect(&s, &core::Settings::closeActionChanged, this, reload);
    connect(&s, &core::Settings::notifyOnDownloadFinishChanged, this, reload);
    connect(&s, &core::Settings::generalChanged, this, reload);
    connect(&s, &core::Settings::themeChanged, this, reload);
    connect(&s, &core::Settings::interfaceScaleChanged, this, reload);
    connect(&s, &core::Settings::blockAdsChanged, this, reload);
    connect(&s, &core::Settings::browserChanged, this, reload);
    connect(&s, &core::Settings::downloadDirectoryChanged, this, reload);
    connect(&s, &core::Settings::downloadDefaultsChanged, this, reload);
    connect(&s, &core::Settings::concurrentDownloadsChanged, this, reload);
    connect(&s, &core::Settings::speedLimitChanged, this, reload);
    connect(&s, &core::Settings::searchChanged, this, reload);
    connect(&s, &core::Settings::engineConfigChanged, this, reload);
    connect(&s, &core::Settings::hardwareAccelerationChanged, this, reload);
}

void SettingsDialog::loadValues()
{
    m_loading = true;
    selectData(m_startPage, static_cast<int>(m_settings.startPage()));
    selectData(m_closeAction, static_cast<int>(m_settings.closeAction()));
    m_keepHistory->setChecked(m_settings.keepSearchHistory());
    m_showWhatsNew->setChecked(m_settings.showWhatsNew());
    m_notifyFinish->setChecked(m_settings.notifyOnDownloadFinish());
    selectData(m_themeChoice, static_cast<int>(m_settings.theme()));
    m_scale->setValue(qRound(m_settings.interfaceScale() * 100));
    loadDownloadValues();
    loadBrowserValues();
    m_resultsPerPage->setValue(m_settings.searchResultsPerPage());
    selectData(m_hardware, static_cast<int>(m_settings.hardwareAcceleration()));
    m_loading = false;
}

void SettingsDialog::loadDownloadValues()
{
    setIdleText(m_folder, m_settings.downloadDirectory());
    m_playlistFolder->setChecked(m_settings.organiseDownloads());
    m_numberFiles->setChecked(m_settings.numberPlaylistFiles());
    m_skipExisting->setChecked(m_settings.skipExisting());
    m_playlistFile->setChecked(m_settings.writePlaylistFile());
    selectData(m_kind, static_cast<int>(m_settings.lastDownloadKind()));
    selectData(m_quality, static_cast<int>(m_settings.defaultQuality()));
    selectData(m_container, static_cast<int>(m_settings.defaultContainer()));
    selectData(m_audioFormat, static_cast<int>(m_settings.defaultAudioFormat()));
    selectData(m_audioBitrate, m_settings.defaultAudioBitrate());
    setIdleText(m_subtitles, joinLanguages(m_settings.subtitleLanguages()));
    m_embedThumbnail->setChecked(m_settings.embedThumbnail());
    m_embedMetadata->setChecked(m_settings.embedMetadata());
    m_concurrent->setValue(m_settings.concurrentDownloads());
    m_speedLimit->setValue(m_settings.speedLimitKbps());
    m_sessionCookies->setChecked(m_settings.useSessionCookies());
    m_engineAutoUpdate->setChecked(m_settings.engineAutoUpdate());
    m_engineUseSystem->setChecked(m_settings.engineUseSystem());
    m_engineSystemPath->setEnabled(m_settings.engineUseSystem());
    setIdleText(m_engineSystemPath, m_settings.engineSystemPath());
}

void SettingsDialog::loadStartPageChoice()
{
    const QString start = m_settings.browserStartPage();
    const bool empty = core::Settings::isEmptyStartPage(start);
    const bool youtube = start == core::Settings::kYouTubeStartPage;
    const bool custom = !empty && !youtube;
    // "Custom address" just chosen stores nothing until an address is typed:
    // a reload in between must not snap the choice back.
    if (custom || m_browserStartChoice->currentData().toInt() != 2) {
        selectData(m_browserStartChoice, empty ? 0 : (youtube ? 1 : 2));
        m_browserStartRow->setVisible(custom);
    }
    if (custom) {
        setIdleText(m_browserStart, start);
    }
}

void SettingsDialog::loadBrowserValues()
{
    loadStartPageChoice();
    m_restoreTabs->setChecked(m_settings.restoreBrowserTabs());
    m_blockAds->setChecked(m_settings.blockAds());
    m_doNotTrack->setChecked(m_settings.doNotTrack());
    int index = m_identity->findData(m_settings.browserUserAgentPreset());
    if (index < 0) {
        index = m_identity->count() - 1; // an unknown id behaves as custom
    }
    m_identity->setCurrentIndex(index);
    m_identityText->setVisible(index == m_identity->count() - 1);
    setIdleText(m_identityText, m_settings.browserUserAgent());
    refreshBlockedCount();
}

void SettingsDialog::refreshBlockedCount()
{
    if (!m_settings.blockAds()) {
        m_blockedCount->setText(tr("Ads and trackers load like any other request."));
        return;
    }
    const quint64 count = m_interceptor.blockedCount();
    m_blockedCount->setText(count == 1 ? tr("1 request blocked this session.")
                                       : tr("%1 requests blocked this session.").arg(QString::number(count)));
}

void SettingsDialog::chooseDownloadFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose the download folder"),
                                                          m_settings.downloadDirectory());
    if (!dir.isEmpty()) {
        m_settings.setDownloadDirectory(dir);
        m_folder->setText(m_settings.downloadDirectory());
    }
}

void SettingsDialog::resetSettings()
{
    if (!MessageSheet::confirm(this, MessageSheet::Tone::Danger, tr("Reset all settings?"),
                               tr("Every option on every page goes back to its default. Your downloads, "
                                  "sign-in and window layout are kept."),
                               tr("Reset"), tr("Keep"))) {
        return;
    }
    m_settings.resetToDefaults();
    loadValues();
}

// ---- engine card -----------------------------------------------------------

QString SettingsDialog::engineStatusText(const services::EngineManager::Status& s)
{
    if (s.isBusy()) {
        return tr("Setting up");
    }
    if (s.ytdlpPath.isEmpty() || s.jsRuntime.isEmpty()) {
        return tr("Not installed");
    }
    if (s.ffmpegMissing) {
        return tr("Media converter missing");
    }
    if (s.isReady() && s.updateAvailable) {
        return tr("Update available (%1)").arg(s.latestVersion);
    }
    if (s.isReady()) {
        return s.systemYtdlp ? tr("Download engine from this system")
                             : tr("Download engine %1").arg(s.ytdlpVersion);
    }
    return tr("Not installed");
}

QString SettingsDialog::engineStatusDetail(const services::EngineManager::Status& s)
{
    if (s.isBusy()) {
        return s.stepLabel;
    }
    if (s.state == services::EngineManager::State::Error && !s.error.isEmpty()) {
        return s.error;
    }
    if (s.ytdlpPath.isEmpty() || s.jsRuntime.isEmpty()) {
        return tr("The first download sets it up.");
    }
    if (s.ffmpegMissing) {
        return s.error.isEmpty() ? tr("Install the ffmpeg package (%1).").arg(core::ffmpegInstallHint())
                                 : s.error;
    }
    if (s.checkingForUpdates) {
        return tr("Checking for updates…");
    }
    if (!s.checkError.isEmpty()) {
        return tr("Could not check for updates: %1").arg(s.checkError);
    }
    if (s.updateAvailable) {
        return tr("Version %1 is installed.").arg(s.ytdlpVersion);
    }
    if (s.lastCheck.isValid() && !s.systemYtdlp) {
        const QDateTime local = s.lastCheck.toLocalTime();
        const QString when =
            local.date() == QDate::currentDate()
                ? tr("today at %1").arg(QLocale().toString(local.time(), QLocale::ShortFormat))
                : QLocale().toString(local.date(), QLocale::LongFormat);
        return tr("Up to date, checked %1.").arg(when);
    }
    return s.systemYtdlp ? s.ytdlpPath : tr("Kept up to date automatically.");
}

void SettingsDialog::refreshEngine(const services::EngineManager::Status& s)
{
    m_engineStatus->setText(engineStatusText(s));
    m_engineDetail->setText(engineStatusDetail(s));
    m_engineDetail->setVisible(!m_engineDetail->text().isEmpty());
    m_engineDetail->setToolTip(s.ytdlpPath);
    const bool ready = s.isReady();
    m_engineCheck->setVisible(ready && !s.updateAvailable);
    m_engineCheck->setEnabled(!s.isBusy() && !s.checkingForUpdates && !s.systemYtdlp);
    m_engineCheck->setText(s.checkingForUpdates ? tr("Checking…") : tr("Check for updates"));
    m_engineUpdate->setVisible(ready && s.updateAvailable);
    m_engineUpdate->setEnabled(!s.isBusy());
    m_engineSetup->setVisible(!ready);
    m_engineSetup->setEnabled(!s.isBusy());
    m_engineSetup->setText(s.state == services::EngineManager::State::Error ? tr("Retry") : tr("Set up"));
}

// ---- wheel guard -----------------------------------------------------------

void SettingsDialog::installWheelGuards()
{
    for (QComboBox* c : findChildren<QComboBox*>()) {
        c->setFocusPolicy(Qt::StrongFocus);
        c->installEventFilter(this);
    }
    for (QAbstractSpinBox* s : findChildren<QAbstractSpinBox*>()) {
        s->setFocusPolicy(Qt::StrongFocus);
        s->installEventFilter(this);
    }
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget != nullptr && !widget->hasFocus()) {
            event->ignore();
            return true; // the page scrolls, never a control the pointer happens to rest on
        }
    }
    return QDialog::eventFilter(watched, event);
}

} // namespace pldl::ui
