#include "ui/main_window.h"

#include "core/downloads/media_info.h"
#include "core/log_sink.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "platform/file_manager.h"
#include "services/engine_manager.h"
#include "services/licensing/license_service.h"
#include "services/media_probe.h"
#include "ui/about_dialog.h"
#include "ui/account_dialog.h"
#include "ui/actions.h"
#include "ui/bug_report_dialog.h"
#include "ui/download_options_sheet.h"
#include "ui/downloads_controller.h"
#include "ui/diagnostics.h"
#include "ui/engine_setup_dialog.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/logging.h"
#include "ui/message_sheet.h"
#include "ui/pages/browser_page.h"
#include "ui/pages/downloads_page.h"
#include "ui/pages/page.h"
#include "ui/pages/playlist_page.h"
#include "ui/pages/search_page.h"
#include "ui/permission_prompt.h"
#include "ui/plans_dialog.h"
#include "ui/playlist_items_sheet.h"
#include "ui/settings_dialog.h"
#include "ui/shortcuts_dialog.h"
#include "ui/side_rail.h"
#include "ui/theme_applier.h"
#include "ui/thumbnail_cache.h"
#include "ui/toast.h"
#include "ui/tray_controller.h"
#include "ui/whats_new_dialog.h"
#include "web/web_profile.h"

#include "core/downloads/download_queue.h"
#include "core/downloads/playlist_file.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSessionManager>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QPushButton>
#include <QMenu>
#include <QTimer>
#include <QToolButton>
#include <QtEnvironmentVariables>

#include <algorithm>
#include <memory>
#include <utility>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr QSize kDefaultSize(1280, 800);
constexpr QSize kMinimumSize(1024, 640);
} // namespace

MainWindow::MainWindow(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_appVersion(appVersion)
{
    setupUi();
    connectActions();
    restoreWindowState();
    connect(
        qApp, &QGuiApplication::commitDataRequest, this,
        [this](QSessionManager&) {
            m_quitting = true;
            saveWindowState();
        },
        Qt::DirectConnection);
}

MainWindow::~MainWindow()
{
    // The pages use the controller's thumbnail cache and the browser's
    // profile; take them down before the objects they lean on.
    delete m_pages;
    m_pages = nullptr;
    qCInfo(lcUi) << "main window gone";
}

void MainWindow::setupUi()
{
    setWindowTitle(u"Playlist Downloader"_s);
    setAcceptDrops(true); // a link dropped anywhere on the window opens as if pasted
    setWindowIcon(icons::brand());
    resize(kDefaultSize);
    setMinimumSize(kMinimumSize);

    m_themeApplier = new ThemeApplier(m_theme, this);
    m_actions = new Actions(this);
    m_rail = new SideRail(*m_actions, m_theme, this);
    m_license = new services::LicenseService(m_settings, this);
    connect(m_license, &services::LicenseService::upgradeRequested, this, &MainWindow::promptUpgrade);
    m_tray = new TrayController(m_settings, *m_actions, this);

    m_engine = new services::EngineManager(m_settings, this);
    m_probe = new services::MediaProbe(this);
    connect(m_engine, &services::EngineManager::ready, this, [this](const core::EnginePaths& paths) {
        m_probe->setEnginePaths(paths);
        // A setup sheet the app opened on its own closes once the engine is
        // there (after a moment, so the ready state is seen); one the user
        // opened stays.
        if (m_engineSetupAuto && m_engineSetup != nullptr) {
            QPointer<EngineSetupDialog> sheet = m_engineSetup;
            QTimer::singleShot(900, this, [sheet] {
                if (sheet != nullptr) {
                    sheet->close();
                }
            });
        }
        m_engineSetupAuto = false;
        m_engineQuiet = false;
        const QList<std::function<void()>> waiting = std::exchange(m_awaitingEngine, {});
        for (const auto& then : waiting) {
            then();
        }
    });
    connect(m_engine, &services::EngineManager::installFailed, this, [this](const QString& error) {
        qCWarning(lcUi) << "engine setup failed:" << error;
        m_awaitingEngine.clear();
        if (m_engineQuiet) {
            m_engineQuiet = false;
            showEngineSetup(); // the page waited in place; the reason and Retry are on the sheet
        }
        if (m_search != nullptr) {
            m_search->retryPending(); // a search waiting for the engine fails with a message
        }
        if (m_playlist != nullptr && m_playlist->state() == PlaylistPage::State::Loading) {
            m_playlist->showError(tr("The download engine could not be set up."));
        }
    });

    m_pages = new QStackedWidget(this);
    // The browser first (its session cookies feed the downloads), then the
    // controller, whose thumbnail cache every page shares.
    m_browser = new BrowserPage(m_settings, m_theme, m_appVersion, this);
    m_downloadsController = new DownloadsController(m_settings, m_theme, *m_engine, *m_probe, *m_license,
                                                    m_browser->profile().cookies(), this, this);
    m_search = new SearchPage(m_settings, m_theme, m_downloadsController->thumbnails(), this);
    connect(m_search, &SearchPage::playlistChosen, this,
            [this](const services::SearchResult& result) { m_knownPlaylist = result; });
    connect(m_search, &SearchPage::playlistRequested, this, &MainWindow::openPlaylist);
    // One gesture, one outcome (review 2026-09-24): a pasted single item opens
    // the options sheet, as Download this and a link on any other site do.
    connect(m_search, &SearchPage::videoRequested, this,
            [this](const QUrl& url) { ensureEngine([this, url] { openVideoOptions(url); }); });
    connect(m_search, &SearchPage::linkRequested, this,
            [this](const QUrl& url) { ensureEngine([this, url] { openAnyLink(url); }); });
    connect(m_search, &SearchPage::engineNeeded, this, [this] {
        ensureEngine(
            [this] {
                m_search->setEnginePaths(m_engine->paths());
                m_search->retryPending();
            },
            true);
    });
    connect(m_engine, &services::EngineManager::ready, m_search, &SearchPage::setEnginePaths);
    connect(m_engine, &services::EngineManager::statusChanged, m_search, &SearchPage::setEngineStatus);
    m_playlist = new PlaylistPage(m_settings, m_theme, *m_probe, m_downloadsController->thumbnails(), this);
    connect(m_playlist, &PlaylistPage::backRequested, this, [this] { showPage(PageId::Search); });
    connect(m_playlist, &PlaylistPage::titleChanged, this, &MainWindow::updateWindowTitle);
    connect(m_playlist, &PlaylistPage::playRequested, this, [this](const QUrl& url) { openUrl(url.toString()); });
    connect(m_playlist, &PlaylistPage::toast, this, &MainWindow::toast);
    connect(m_playlist, &PlaylistPage::hasPlaylistChanged, m_actions->playlist, &QAction::setEnabled);
    connect(m_playlist, &PlaylistPage::engineNeeded, this,
            [this] { ensureEngine([this] { m_playlist->reload(); }, true); });
    connect(m_engine, &services::EngineManager::statusChanged, m_playlist, &PlaylistPage::setEngineStatus);
    connect(m_playlist, &PlaylistPage::downloadRequested, this, &MainWindow::openPlaylistOptions);
    connect(m_playlist, &PlaylistPage::videoDownloadRequested, this,
            [this](const core::MediaEntry& entry) { openVideoOptions(QUrl(entry.url)); });
    m_actions->playlist->setEnabled(false);
    // The answers to openVideoOptions' probes: the sheet, or a toast.
    connect(m_probe, &services::MediaProbe::finished, this, [this](quint64 id, const core::MediaInfo& info) {
        if (m_linkProbes.contains(id)) {
            // Any site: the engine said what the link is.
            const QUrl probed = m_linkProbes.take(id);
            m_browser->setDownloadBusy(false);
            if (info.isPlaylist()) {
                qCInfo(lcUi) << "link is a playlist:" << probed << info.entries.size() << "entries";
                m_playlist->openInfo(probed, info);
                showPage(PageId::Playlist);
            } else {
                presentVideoOptions(probed, info);
            }
            return;
        }
        if (!m_videoProbes.contains(id)) {
            return;
        }
        presentVideoOptions(m_videoProbes.take(id), info);
    });
    connect(m_probe, &services::MediaProbe::failed, this, [this](quint64 id, const QString& error) {
        if (!m_videoProbes.contains(id) && !m_linkProbes.contains(id)) {
            return;
        }
        m_videoProbes.remove(id);
        m_linkProbes.remove(id);
        toast(tr("Could not read that link: %1").arg(error));
        m_browser->setDownloadBusy(false);
    });
    m_downloads = new DownloadsPage(*m_downloadsController, m_settings, m_theme, this);
    for (QWidget* page : {static_cast<QWidget*>(m_search), static_cast<QWidget*>(m_playlist),
                          static_cast<QWidget*>(m_browser), static_cast<QWidget*>(m_downloads)}) {
        m_pages->addWidget(page);
    }

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    layout->addWidget(m_rail);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);
    // Over the page area, never the rail: the bottom-left corner of the pages
    // is where toasts belong (DESIGN.md section 1) and the rail's bottom
    // buttons stay clickable.
    m_toasts = new ToastHost(m_pages);
    // Settings, General: Start page, Search or the page the app was closed on.
    const int last = std::min(m_settings.lastPage(), static_cast<int>(PageId::Downloads));
    showPage(m_settings.startPage() == core::StartPage::LastPage ? static_cast<PageId>(last)
                                                                 : PageId::Search);
}

void MainWindow::connectActions()
{
    Actions& a = *m_actions;
    connect(a.home, &QAction::triggered, this, [this] { showPage(PageId::Search); });
    connect(a.playlist, &QAction::triggered, this, [this] { showPage(PageId::Playlist); });
    connect(a.browser, &QAction::triggered, this, [this] { showPage(PageId::Browser); });
    connect(a.downloads, &QAction::triggered, this, [this] { showPage(PageId::Downloads); });
    connect(a.showHide, &QAction::triggered, this, &MainWindow::toggleVisibility);
    connect(a.settings, &QAction::triggered, this, &MainWindow::showSettings);
    connect(a.shortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    connect(a.onlineGuide, &QAction::triggered, this, [] { platform::openUrl(links::kGuide); });
    connect(a.openLogFolder, &QAction::triggered, this,
            [] { platform::openDirectory(QFileInfo(core::LogSink::logFilePath()).absolutePath()); });
    connect(a.about, &QAction::triggered, this, &MainWindow::showAbout);
    connect(a.reportBug, &QAction::triggered, this, &MainWindow::showBugReport);
    connect(a.account, &QAction::triggered, this, &MainWindow::showAccount);
    connect(a.quit, &QAction::triggered, this, &MainWindow::quit);

    connect(m_tray, &TrayController::toggleRequested, this, &MainWindow::toggleVisibility);
    connect(m_browser, &BrowserPage::renderProcessGaveUp, this, &MainWindow::handleRenderProcessGaveUp);
    connect(m_browser, &BrowserPage::fullScreenChanged, this, &MainWindow::setBrowserFullScreen);
    connect(m_browser, &BrowserPage::permissionPromptRequested, this, &MainWindow::handlePermissionPrompt);
    // Download this (FEATURES W3): a YouTube playlist page lands on the
    // Playlist page, a YouTube video goes through the options sheet, a page on
    // any other site is probed and the engine's answer decides (a playlist,
    // a single item); a YouTube channel is queued with the defaults. The
    // browser's busy button is released when the request settles.
    connect(m_browser, &BrowserPage::downloadRequested, this, [this](const QUrl& url) {
        const core::YouTubeUrlKind kind = core::classifyYouTubeUrl(url).kind;
        if (kind == core::YouTubeUrlKind::Playlist) {
            m_browser->setDownloadBusy(false);
            openPlaylist(url);
            return;
        }
        m_browser->setDownloadBusy(true);
        if (kind == core::YouTubeUrlKind::Video) {
            ensureEngine([this, url] { openVideoOptions(url); });
            return;
        }
        if (kind == core::YouTubeUrlKind::Channel) {
            ensureEngine([this, url] { m_downloadsController->requestDownload(url); });
            return;
        }
        ensureEngine([this, url] { openAnyLink(url); });
    });
    connect(m_browser, &BrowserPage::cancelDownloadRequested, this, [this] {
        m_downloadsController->cancelRequests();
        for (const quint64 id : m_linkProbes.keys()) {
            m_probe->cancel(id);
        }
        m_linkProbes.clear();
        const QList<quint64> ids = m_videoProbes.keys();
        for (const quint64 id : ids) {
            m_probe->cancel(id);
            m_videoProbes.remove(id);
        }
        m_browser->setDownloadBusy(false);
    });
    connect(m_downloadsController, &DownloadsController::requestSettled, this,
            [this](const QUrl&, bool) { m_browser->setDownloadBusy(false); });
    connect(m_engine, &services::EngineManager::installFailed, this, [this] { m_browser->setDownloadBusy(false); });
    connect(m_downloadsController, &DownloadsController::activeCountChanged, this, [this](int count) {
        m_rail->setActiveDownloads(count);
        m_tray->setActiveDownloads(count);
    });
    connect(m_downloadsController, &DownloadsController::toast, this, &MainWindow::toast);
    connect(m_downloadsController, &DownloadsController::plansRequested, this, &MainWindow::showPlans);
    connect(m_downloadsController, &DownloadsController::queued, this, [this](const QString& text) {
        if (m_toasts != nullptr && isVisible() && m_pages->currentWidget() != m_downloads) {
            m_toasts->show(text, ToastHost::Kind::Success, tr("View"), [this] { showPage(PageId::Downloads); });
        } else {
            toast(text);
        }
    });
    connect(m_downloadsController, &DownloadsController::playlistItemsRequested, this,
            &MainWindow::showPlaylistItems);
    connect(m_downloads, &DownloadsPage::engineSetupRequested, this, &MainWindow::showEngineSetup);
    // Browser shortcuts act while the Browser page is showing; New tab brings
    // it up. Ctrl+W closes a tab there and does nothing elsewhere.
    auto onBrowser = [this](auto member) {
        return [this, member] {
            if (m_pages->currentWidget() == m_browser) {
                (m_browser->*member)();
            }
        };
    };
    connect(a.browserNewTab, &QAction::triggered, this, [this] {
        showPage(PageId::Browser);
        m_browser->newTab();
    });
    connect(a.browserCloseTab, &QAction::triggered, this, onBrowser(&BrowserPage::closeCurrentTab));
    connect(a.browserAddress, &QAction::triggered, this, onBrowser(&BrowserPage::focusAddress));
    connect(a.browserReload, &QAction::triggered, this, onBrowser(&BrowserPage::reload));
    connect(a.browserBack, &QAction::triggered, this, onBrowser(&BrowserPage::back));
    connect(a.browserForward, &QAction::triggered, this, onBrowser(&BrowserPage::forward));
    connect(a.browserNextTab, &QAction::triggered, this, onBrowser(&BrowserPage::nextTab));
    connect(a.browserPreviousTab, &QAction::triggered, this, onBrowser(&BrowserPage::previousTab));
    connect(a.browserDownload, &QAction::triggered, this, onBrowser(&BrowserPage::downloadCurrent));
    connect(a.browserFind, &QAction::triggered, this, onBrowser(&BrowserPage::showFind));
}

void MainWindow::start()
{
    show();
    m_license->start();
    m_downloadsController->start();
    m_engine->initialize();
    QTimer::singleShot(600, this, [this] {
        maybeShowGpuFallbackNotice();
        QTimer::singleShot(3000, this, [this] { maybeShowWhatsNew(); });
    });
}

// ---- engine ----------------------------------------------------------------

void MainWindow::ensureEngine(std::function<void()> then, bool quiet)
{
    if (m_engine->isReady()) {
        then();
        return;
    }
    m_awaitingEngine.append(std::move(then));
    if (quiet && m_engineSetup == nullptr) {
        m_engineQuiet = true; // the page shows the progress; no sheet unless it fails
    } else {
        m_engineSetupAuto = true;
        showEngineSetup();
    }
    if (!m_engine->status().isBusy()) {
        m_engine->install();
    }
}

void MainWindow::showEngineSetup()
{
    // Called from the UI (Settings, the Downloads chip) as well: unless
    // ensureEngine just set the flag, this is the user's own sheet.
    if (m_engineSetup == nullptr) {
        m_engineSetup = new EngineSetupDialog(*m_engine, m_theme, this);
        m_engineSetup->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_engineSetup->show();
    m_engineSetup->raise();
    m_engineSetup->activateWindow();
}

// ---- pages -----------------------------------------------------------------

void MainWindow::showPage(PageId page)
{
    const int index = static_cast<int>(page);
    m_pages->setCurrentIndex(index);
    m_settings.setLastPage(index);
    QAction* actions[] = {m_actions->home, m_actions->playlist, m_actions->browser, m_actions->downloads};
    actions[index]->setChecked(true);
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString app = u"Playlist Downloader"_s;
    if (m_pages->currentWidget() == m_playlist && m_playlist->state() == PlaylistPage::State::Ready) {
        const QString title = m_playlist->titleLabel()->toolTip();
        if (!title.isEmpty()) {
            setWindowTitle(u"%1, %2"_s.arg(title, app));
            return;
        }
    }
    setWindowTitle(app);
}

void MainWindow::openUrl(const QString& url)
{
    showAndRaise();
    showPage(PageId::Browser);
    m_browser->open(QUrl::fromUserInput(url));
}

void MainWindow::openPlaylist(const QUrl& url)
{
    qCInfo(lcUi) << "playlist requested:" << url.toString();
    // The Search page says which card was chosen right before it asks; that
    // card's data fills the header while the playlist is read.
    std::optional<services::SearchResult> known;
    if (m_knownPlaylist && QUrl(m_knownPlaylist->url) == url) {
        known = m_knownPlaylist;
    }
    m_knownPlaylist.reset();
    m_playlist->open(url, known);
    showPage(PageId::Playlist);
}

void MainWindow::downloadUrl(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        return;
    }
    qCInfo(lcUi) << "download requested:" << url;
    showAndRaise();
    const QUrl link = QUrl::fromUserInput(url.trimmed());
    switch (core::classifyYouTubeUrl(link).kind) {
    case core::YouTubeUrlKind::Playlist:
        openPlaylist(link);
        return;
    case core::YouTubeUrlKind::Video:
        ensureEngine([this, link] { openVideoOptions(link); });
        return;
    case core::YouTubeUrlKind::NotYouTube:
        if (core::isDownloadable(link)) {
            ensureEngine([this, link] { openAnyLink(link); }); // any site: the engine decides
            return;
        }
        break;
    case core::YouTubeUrlKind::Channel:
    case core::YouTubeUrlKind::Other:
        break;
    }
    showPage(PageId::Downloads);
    ensureEngine([this, link] { m_downloadsController->requestDownload(link); });
}

// ---- download options ------------------------------------------------------

void MainWindow::debugWatchQueue()
{
    if (m_debugWatching) {
        return;
    }
    m_debugWatching = true;
    auto say = [](const QString& line) {
        QTextStream out(stdout);
        out << line << Qt::endl;
        qCInfo(lcUi).noquote() << "autodownload:" << line;
    };
    core::DownloadQueue* queue = &m_downloadsController->queue();
    connect(queue, &core::DownloadQueue::jobAdded, this, [say](quint64 id) { say(u"queued:%1"_s.arg(id)); });
    connect(queue, &core::DownloadQueue::jobFinished, this, [this, say, queue](quint64 id, core::DownloadState state) {
        const auto job = queue->job(id);
        say(u"finished:%1 state:%2"_s.arg(id).arg(core::stateLabel(state)));
        if (job) {
            for (const QString& file : job->outputFiles) {
                say(u"file:"_s + file);
            }
            say(u"entries:%1 downloaded:%2"_s.arg(job->entries.size()).arg(job->downloadedEntryCount()));
            const QString list = core::playlist_file::pathFor(*job);
            say(u"playlist:%1 exists:%2"_s.arg(list, QFileInfo::exists(list) ? u"yes"_s : u"no"_s));
        }
        const int code = state == core::DownloadState::Completed ? 0 : 1;
        QTimer::singleShot(500, this, [this, code] {
            m_downloadsController->shutdown(); // the list and the playlist file, as a real quit would
            qApp->exit(code);
        });
    });
}

void MainWindow::showOptionsSheet(DownloadOptionsSheet* sheet)
{
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    // Headless verification (ADR-000): PLDL_DEBUG_AUTODOWNLOAD=video|audio
    // picks the kind and accepts the sheet, then the queue is watched and
    // the app quits with the outcome (debugWatchQueue).
    if (const QString kind = qEnvironmentVariable("PLDL_DEBUG_AUTODOWNLOAD"); !kind.isEmpty()) {
        sheet->setKind(kind == u"audio"_s ? DownloadOptionsSheet::Kind::Audio : DownloadOptionsSheet::Kind::Video);
        debugWatchQueue();
        QTimer::singleShot(400, sheet, &QDialog::accept);
    }
    connect(sheet, &QDialog::accepted, this, [this, sheet] {
        if (m_downloadsController->enqueue(sheet->job()) != 0) {
            showPage(PageId::Downloads);
        }
    });
    sheet->open();
}

void MainWindow::openPlaylistOptions(const core::MediaInfo& info, const QList<int>& indexes)
{
    if (indexes.isEmpty()) {
        return;
    }
    ensureEngine([this, info, indexes] {
        auto* sheet = new DownloadOptionsSheet(m_settings, m_theme, this);
        sheet->setPlaylist(info, indexes);
        showOptionsSheet(sheet);
    });
}

void MainWindow::openVideoOptions(const QUrl& url)
{
    const std::optional<QUrl> canonical = core::canonicalVideoUrl(url);
    const QUrl link = canonical ? *canonical : url;
    if (!m_probe->hasEngine()) {
        toast(tr("The download engine is not ready yet."));
        m_browser->setDownloadBusy(false);
        return;
    }
    const quint64 id = m_probe->probe(link, false);
    m_videoProbes.insert(id, link);
    qCInfo(lcUi) << "video options: probing" << link << "probe" << id;
}

void MainWindow::openAnyLink(const QUrl& url)
{
    if (!m_probe->hasEngine()) {
        toast(tr("The download engine is not ready yet."));
        m_browser->setDownloadBusy(false);
        return;
    }
    // Flat: a playlist answers with its entries, a single item with its formats.
    const quint64 id = m_probe->probe(url, true);
    m_linkProbes.insert(id, url);
    qCInfo(lcUi) << "any link: probing" << url << "probe" << id;
}

void MainWindow::presentVideoOptions(const QUrl& probed, const core::MediaInfo& info)
{
    core::MediaEntry entry;
    entry.id = info.id;
    entry.url = probed.toString();
    entry.title = info.title;
    entry.uploader = info.uploader;
    entry.thumbnail = info.thumbnail;
    entry.duration = info.duration;
    auto* sheet = new DownloadOptionsSheet(m_settings, m_theme, this);
    sheet->setVideo(entry, probed);
    connect(sheet, &QDialog::finished, this, [this] { m_browser->setDownloadBusy(false); });
    showOptionsSheet(sheet);
}

void MainWindow::showPlaylistItems(quint64 jobId)
{
    const auto job = m_downloadsController->queue().job(jobId);
    if (!job) {
        return;
    }
    auto* sheet = new PlaylistItemsSheet(*job, m_theme, this);
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    connect(sheet, &PlaylistItemsSheet::toast, this, &MainWindow::toast);
    connect(sheet, &PlaylistItemsSheet::downloadMissingRequested, this,
            [this](const core::DownloadJob& source, const QList<int>& positions) {
                // A fresh job for the same playlist, limited to the missing positions.
                core::DownloadJob fresh;
                fresh.url = source.url;
                fresh.title = source.title;
                fresh.uploader = source.uploader;
                fresh.thumbnail = source.thumbnail;
                fresh.options = source.options;
                fresh.options.playlistItems = DownloadOptionsSheet::itemSpec(positions);
                fresh.itemCount = static_cast<int>(positions.size());
                for (const int position : positions) {
                    if (position >= 1 && position <= source.entries.size()) {
                        core::PlaylistEntry entry = source.entries.at(position - 1);
                        entry.file.clear();
                        fresh.entries << entry;
                    }
                }
                if (m_downloadsController->enqueue(fresh) != 0) {
                    showPage(PageId::Downloads);
                }
            });
    sheet->open();
}

void MainWindow::showPlans()
{
    if (m_plans == nullptr) {
        m_plans = new PlansDialog(*m_license, m_theme, this);
        m_plans->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_plans->show();
    m_plans->raise();
    m_plans->activateWindow();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_toasts != nullptr) {
        m_toasts->reposition();
    }
}

// ---- window state ----------------------------------------------------------

void MainWindow::restoreWindowState()
{
    if (qEnvironmentVariableIsSet("PLDL_DEBUG_WINDOW_SIZE")) {
        return; // a grab wants the size it asked for, not the remembered one
    }
    const QByteArray geometry = m_settings.windowGeometry();
    if (!geometry.isEmpty() && !restoreGeometry(geometry)) {
        qCWarning(lcUi) << "stored window geometry rejected, using defaults";
    }
    const QByteArray state = m_settings.windowState();
    if (!state.isEmpty()) {
        restoreState(state);
    }
}

void MainWindow::saveWindowState()
{
    m_settings.setWindowGeometry(saveGeometry());
    m_settings.setWindowState(saveState());
}

QUrl MainWindow::linkIn(const QMimeData* mime)
{
    if (mime == nullptr) {
        return {};
    }
    if (mime->hasUrls()) {
        for (const QUrl& url : mime->urls()) {
            if (core::isDownloadable(url)) {
                return url;
            }
        }
    }
    if (mime->hasText()) {
        const QString text = mime->text().trimmed();
        if (!text.contains(u'\n') && text.size() < 2048) {
            const QUrl url = QUrl::fromUserInput(text);
            if ((text.startsWith(u"http://"_s) || text.startsWith(u"https://"_s) || text.startsWith(u"www."_s)) &&
                core::isDownloadable(url)) {
                return url;
            }
        }
    }
    return {};
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (linkIn(event->mimeData()).isValid()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QUrl url = linkIn(event->mimeData());
    if (!url.isValid()) {
        return;
    }
    event->acceptProposedAction();
    qCInfo(lcUi) << "link dropped:" << url;
    downloadUrl(url.toString());
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ActivationChange && isActiveWindow()) {
        offerClipboardLink();
    }
}

void MainWindow::offerClipboardLink()
{
    if (m_toasts == nullptr || !isVisible() || QApplication::activeModalWidget() != nullptr) {
        return;
    }
    const QUrl url = linkIn(QApplication::clipboard()->mimeData());
    if (!url.isValid()) {
        return;
    }
    const QString text = url.toString();
    if (text == m_lastClipboardOffer || url == m_playlist->currentUrl() || url == m_browser->currentUrl()) {
        return; // offered already, or the link on show (Copy link puts it there)
    }
    m_lastClipboardOffer = text;
    qCInfo(lcUi) << "clipboard link offered:" << url;
    m_toasts->show(tr("Link in the clipboard: %1").arg(url.host()), ToastHost::Kind::Info, tr("Open"),
                   [this, text] { downloadUrl(text); });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_quitting && m_settings.closeAction() == core::CloseAction::MinimizeToTray &&
        m_tray->isAvailable()) {
        event->ignore();
        hide();
        return;
    }
    m_quitting = true;
    saveWindowState();
    m_downloadsController->shutdown();
    event->accept();
    QApplication::quit();
}

void MainWindow::quit()
{
    m_quitting = true;
    close();
}

void MainWindow::showEvent(QShowEvent* event)
{
    m_tray->recheckAvailability();
    QMainWindow::showEvent(event);
    m_tray->setWindowVisible(true);
}

void MainWindow::hideEvent(QHideEvent* event)
{
    QMainWindow::hideEvent(event);
    m_tray->setWindowVisible(false);
}

void MainWindow::showAndRaise()
{
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::toggleVisibility()
{
    if (isVisible() && !isMinimized()) {
        if (!m_tray->isAvailable()) {
            showMinimized();
        } else {
            hide();
        }
    } else {
        showAndRaise();
    }
}

// ---- sheets ----------------------------------------------------------------

void MainWindow::showSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(
            m_settings, m_theme, *m_engine, m_browser->profile().interceptor(), m_tray->isAvailable(), this);
        m_settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
        connectSettingsDialog(m_settingsDialog);
    }
    m_settingsDialog->show();
    m_settingsDialog->raise();
    m_settingsDialog->activateWindow();
}

void MainWindow::connectSettingsDialog(SettingsDialog* dialog)
{
    connect(dialog, &SettingsDialog::clearCacheRequested, this, [this] {
        m_browser->profile().clearHttpCache();
        toast(tr("Cache cleared"));
    });
    connect(dialog, &SettingsDialog::clearSessionRequested, this, &MainWindow::confirmClearSession);
    connect(dialog, &SettingsDialog::resetPermissionsRequested, this, [this] {
        m_browser->resetPermissions();
        toast(tr("Site permissions reset"));
    });
    connect(dialog, &SettingsDialog::openLogFolderRequested, this,
            [] { platform::openDirectory(QFileInfo(core::LogSink::logFilePath()).absolutePath()); });
    connect(dialog, &SettingsDialog::copyDiagnosticsRequested, this, [this] {
        QApplication::clipboard()->setText(buildDiagnostics(
            m_settings, m_browser->userAgent(), SettingsDialog::engineStatusText(m_engine->status())));
        toast(tr("Diagnostics copied"));
    });
    connect(dialog, &SettingsDialog::engineSetupRequested, this, [this] { ensureEngine([] {}); });
}

void MainWindow::confirmClearSession()
{
    QWidget* parent = m_settingsDialog ? static_cast<QWidget*>(m_settingsDialog) : this;
    if (!MessageSheet::confirm(parent, MessageSheet::Tone::Danger, tr("Sign out and clear the session?"),
                               tr("Cookies, site data and your sign-ins are removed and the app "
                                  "restarts. Downloads and settings are kept."),
                               tr("Sign out and restart"), tr("Keep"))) {
        return;
    }
    m_quitting = true;
    saveWindowState();
    m_settings.sync();
    Q_EMIT clearSessionRequested();
}

void MainWindow::toast(const QString& text)
{
    if (m_toasts != nullptr && isVisible()) {
        m_toasts->show(text, ToastHost::Kind::Success);
        return;
    }
    MessageSheet::info(this, tr("Done"), text);
}

void MainWindow::showShortcuts()
{
    auto* dialog = new ShortcutsDialog(*m_actions, m_theme, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showBugReport()
{
    auto* dialog = new BugReportDialog(m_settings, m_theme, m_browser->userAgent(), QString(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showAbout()
{
    auto* dialog = new AboutDialog(m_settings, m_theme, m_browser->userAgent(), QString(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::showAccount()
{
    if (!m_accountDialog) {
        m_accountDialog = new AccountDialog(*m_license, m_theme, this);
        m_accountDialog->setAttribute(Qt::WA_DeleteOnClose);
    }
    m_accountDialog->show();
    m_accountDialog->raise();
    m_accountDialog->activateWindow();
}

void MainWindow::promptUpgrade(const QString& feature)
{
    qCInfo(lcUi) << "upgrade needed for" << feature;
    showAccount();
    m_accountDialog->showPlans();
}

void MainWindow::debugOpen(const QString& what)
{
    if (what == u"about"_s) {
        showAbout();
    } else if (what == u"shortcuts"_s) {
        showShortcuts();
    } else if (what == u"account"_s) {
        showAccount();
    } else if (what == u"plans"_s) {
        showAccount();
        m_accountDialog->showPlans();
    } else if (what == u"bug"_s) {
        showBugReport();
    } else if (what == u"help"_s) {
        // The rail's help menu, popped without blocking, for a grab.
        if (auto* button = m_rail->findChild<QToolButton*>(u"helpButton"_s); button != nullptr && button->menu()) {
            button->menu()->popup(button->mapToGlobal(QPoint(button->width(), 0)));
        }
    } else if (what == u"whatsnew"_s || what.startsWith(u"whatsnew:"_s)) {
        maybeShowWhatsNew(true);
        if (auto* dialog = findChild<WhatsNewDialog*>(); dialog != nullptr && what.contains(u':')) {
            dialog->showVersion(what.section(u':', 1)); // whatsnew:<version> opens on an older release
        }
    } else if (what == u"settings"_s) {
        showSettings();
    } else if (what.startsWith(u"settings:"_s)) {
        showSettings();
        m_settingsDialog->showPage(what.mid(9));
    } else if (what.startsWith(u"browser:"_s)) {
        openUrl(what.mid(8));
    } else if (what == u"browser-fullscreen"_s) {
        showPage(PageId::Browser);
        m_browser->debugEnterFullScreen();
    } else if (what == u"search"_s) {
        showPage(PageId::Search);
    } else if (what.startsWith(u"search:"_s)) {
        showPage(PageId::Search);
        m_search->search(what.mid(7));
    } else if (what.startsWith(u"search-typing:"_s)) {
        showPage(PageId::Search);
        m_search->typeQuery(what.mid(14));
    } else if (what == u"search-demo"_s) {
        showPage(PageId::Search);
        m_search->queryField()->setText(u"lofi"_s);
        m_search->showResults(SearchPage::demoResults(), true);
    } else if (what == u"playlist"_s) {
        showPage(PageId::Playlist);
    } else if (what.startsWith(u"playlist:"_s)) {
        openPlaylist(QUrl::fromUserInput(what.mid(9)));
        if (qEnvironmentVariableIsSet("PLDL_DEBUG_AUTODOWNLOAD")) {
            // Press Download as soon as the playlist is in (the button enables
            // with the first checked row); one press only.
            auto* poll = new QTimer(this);
            poll->setInterval(500);
            connect(poll, &QTimer::timeout, this, [this, poll] {
                if (m_playlist->downloadButton()->isEnabled()) {
                    poll->stop();
                    poll->deleteLater();
                    m_playlist->downloadButton()->click();
                }
            });
            poll->start();
        }
    } else if (what == u"playlist-demo"_s) {
        m_playlist->openInfo(PlaylistPage::demoInfo());
        showPage(PageId::Playlist);
    } else if (what == u"options-demo"_s) {
        m_playlist->openInfo(PlaylistPage::demoInfo());
        showPage(PageId::Playlist);
        auto* sheet = new DownloadOptionsSheet(m_settings, m_theme, this);
        sheet->setPlaylist(PlaylistPage::demoInfo(), {1, 2, 3, 5, 6});
        showOptionsSheet(sheet);
    } else if (what == u"options-video-demo"_s) {
        m_playlist->openInfo(PlaylistPage::demoInfo());
        showPage(PageId::Playlist);
        auto* sheet = new DownloadOptionsSheet(m_settings, m_theme, this);
        const core::MediaEntry entry = PlaylistPage::demoInfo().entries.first();
        sheet->setVideo(entry, QUrl(entry.url));
        showOptionsSheet(sheet);
    } else if (what == u"browser"_s) {
        showPage(PageId::Browser);
    } else if (what == u"downloads"_s) {
        showPage(PageId::Downloads);
    } else if (what.startsWith(u"remove:"_s)) {
        showPage(PageId::Downloads);
        m_downloadsController->handleCardAction(what.mid(7).toULongLong(), DownloadCardDelegate::Action::Remove);
    } else if (what.startsWith(u"playlist-items:"_s)) {
        showPage(PageId::Downloads);
        showPlaylistItems(what.mid(15).toULongLong());
    } else if (what == u"playlist-items-demo"_s || what == u"playlist-items-demo-file"_s) {
        // A playlist with three of five files on disk, for a grab of the items sheet;
        // the -file variant has a playlist file there already (the banner).
        core::DownloadJob job;
        job.id = 9001;
        job.title = u"Learn Qt in 12 videos"_s;
        job.options.isPlaylist = true;
        const QString folder = QDir::temp().filePath(u"playlist-dl-items-demo"_s);
        QDir().mkpath(folder);
        const QStringList titles{u"Getting started with Qt Widgets"_s, u"Signals and slots"_s,
                                 u"Layouts that survive a resize"_s, u"Model, view, delegate"_s,
                                 u"Style sheets without tears"_s};
        for (int i = 0; i < titles.size(); ++i) {
            core::PlaylistEntry entry;
            entry.id = u"demo%1"_s.arg(i + 1);
            entry.title = titles.at(i);
            if (i < 3) {
                entry.file = QDir(folder).filePath(u"%1 - %2.mp4"_s.arg(i + 1, 3, 10, u'0').arg(titles.at(i)));
                QFile file(entry.file);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write("demo");
                }
                job.outputFiles << entry.file;
            }
            job.entries << entry;
        }
        job.options.outputDirectory = folder;
        job.state = core::DownloadState::Completed;
        const QString playlistFile = core::playlist_file::pathFor(job);
        if (what.endsWith(u"-file"_s)) {
            core::playlist_file::writeFor(job);
        } else {
            QFile::remove(playlistFile);
        }
        m_downloadsController->queue().setJobs({job});
        showPage(PageId::Downloads);
        showPlaylistItems(job.id);
    } else if (what == u"downloads-demo"_s) {
        m_downloadsController->queue().setJobs(demoDownloads());
        showPage(PageId::Downloads);
        // After the page shows: showing it refreshes the chip from the licence.
        m_downloads->setAllowance(3, services::LicenseService::kFreeDownloadsPerDay); // the free tier's chip
    } else {
        qCWarning(lcUi) << "debugOpen: unknown target" << what;
    }
}

QList<core::DownloadJob> MainWindow::demoDownloads()
{
    // One card per state, newest first as the queue lists them; the ids are
    // the queue's own so every card action resolves.
    using core::DownloadState;
    QList<core::DownloadJob> jobs;
    quint64 id = 1;
    auto make = [&id](const QString& title, const QString& uploader, DownloadState state) {
        core::DownloadJob j;
        j.id = id++;
        j.url = u"https://www.youtube.com/watch?v=demo%1"_s.arg(j.id);
        j.videoId = u"demo%1"_s.arg(j.id);
        j.title = title;
        j.uploader = uploader;
        j.state = state;
        j.options.kind = core::DownloadOptions::Kind::Video;
        j.options.quality = core::VideoQuality::Q1080;
        j.options.container = core::Container::Mp4;
        j.createdAt = QDateTime::currentDateTime();
        return j;
    };
    core::DownloadJob failed = make(u"Removed video (private since last week)"_s, u"Some channel"_s,
                                    DownloadState::Failed);
    failed.error = u"This video is unavailable."_s;
    failed.finishedAt = QDateTime::currentDateTime();
    jobs << failed;
    core::DownloadJob cancelled = make(u"A talk I changed my mind about"_s, u"Conference"_s, DownloadState::Cancelled);
    cancelled.finishedAt = QDateTime::currentDateTime();
    jobs << cancelled;
    core::DownloadJob completed = make(u"Me at the zoo"_s, u"jawed"_s, DownloadState::Completed);
    completed.totalBytes = 118 * 1024 * 1024;
    completed.downloadedBytes = completed.totalBytes;
    completed.outputFiles << u"/tmp/Me at the zoo.mp4"_s;
    completed.finishedAt = QDateTime::currentDateTime();
    jobs << completed;
    core::DownloadJob paused = make(u"Long documentary, part 2"_s, u"Docs channel"_s, DownloadState::Paused);
    paused.totalBytes = 900 * 1024 * 1024;
    paused.downloadedBytes = 300 * 1024 * 1024;
    jobs << paused;
    core::DownloadJob processing = make(u"Live concert (audio only)"_s, u"Band"_s, DownloadState::Processing);
    processing.options.kind = core::DownloadOptions::Kind::Audio;
    processing.options.audioFormat = core::AudioFormat::Mp3;
    processing.stage = u"Converting"_s;
    jobs << processing;
    core::DownloadJob playlist = make(u"Learn Qt in 12 videos"_s, u"Tutorials"_s, DownloadState::Downloading);
    playlist.options.isPlaylist = true;
    playlist.itemIndex = 3;
    playlist.itemCount = 12;
    playlist.currentItemTitle = u"Signals and slots"_s;
    playlist.totalBytes = 64 * 1024 * 1024;
    playlist.downloadedBytes = 21 * 1024 * 1024;
    playlist.bytesPerSecond = 3.2 * 1024 * 1024;
    playlist.etaSeconds = 14;
    jobs << playlist;
    core::DownloadJob downloading = make(u"Keynote 2026"_s, u"Conference"_s, DownloadState::Downloading);
    downloading.totalBytes = 118 * 1024 * 1024;
    downloading.downloadedBytes = 12 * 1024 * 1024 + 400 * 1024;
    downloading.bytesPerSecond = 3.2 * 1024 * 1024;
    downloading.etaSeconds = 32;
    jobs << downloading;
    jobs << make(u"Waiting for a slot"_s, u"Some channel"_s, DownloadState::Probing);
    jobs << make(u"Queued behind the others"_s, u"Some channel"_s, DownloadState::Queued);
    std::reverse(jobs.begin(), jobs.end());
    return jobs;
}

void MainWindow::debugDownload(const QString& url)
{
    // The hook's protocol goes to stdout (the caller parses it); the log
    // keeps a copy.
    auto say = [](const QString& line) {
        QTextStream out(stdout);
        out << line << Qt::endl;
        qCInfo(lcUi) << "debug download:" << line;
    };
    auto* dir = new QTemporaryDir();
    if (!dir->isValid()) {
        say(u"error:no temporary directory"_s);
        QTimer::singleShot(0, qApp, [] { qApp->exit(1); });
        return;
    }
    dir->setAutoRemove(false); // the caller inspects the file
    say(u"folder:"_s + dir->path());
    showPage(PageId::Downloads);
    // Every job the request queues lands in the temporary folder.
    m_settings.setDownloadDirectory(dir->path());
    auto* poll = new QTimer(this);
    poll->setInterval(500);
    auto lastState = std::make_shared<QString>();
    auto watched = std::make_shared<quint64>(0);
    core::DownloadQueue* queue = &m_downloadsController->queue();
    connect(queue, &core::DownloadQueue::jobAdded, this, [this, say, poll, lastState, watched, queue](quint64 id) {
        if (*watched != 0) {
            return;
        }
        *watched = id;
        say(u"queued:%1"_s.arg(id));
        connect(poll, &QTimer::timeout, this, [say, lastState, watched, queue] {
            if (const std::optional<core::DownloadJob> job = queue->job(*watched); job) {
                const QString line = core::stateLabel(job->state) + u" "_s + job->statusLine();
                if (line != *lastState) {
                    *lastState = line;
                    say(u"state:"_s + line);
                }
            }
        });
        poll->start();
    });
    connect(queue, &core::DownloadQueue::jobFinished, this,
            [this, say, poll, watched, queue](quint64 id, core::DownloadState state) {
                if (id != *watched) {
                    return;
                }
                poll->stop();
                const std::optional<core::DownloadJob> job = queue->job(id);
                const bool ok = state == core::DownloadState::Completed && job && !job->primaryFile().isEmpty();
                say(u"state:"_s + core::stateLabel(state));
                say(u"file:"_s + (job ? job->primaryFile() : QString()));
                if (job && !job->error.isEmpty()) {
                    say(u"error:"_s + job->error);
                }
                m_downloadsController->shutdown();
                QTimer::singleShot(1500, qApp, [ok] { qApp->exit(ok ? 0 : 1); });
            });
    connect(m_downloadsController, &DownloadsController::requestSettled, this, [say](const QUrl&, bool queued) {
        if (!queued) {
            say(u"error:the link was not queued"_s);
            QTimer::singleShot(500, qApp, [] { qApp->exit(1); });
        }
    });
    connect(m_engine, &services::EngineManager::installFailed, this, [say](const QString& error) {
        say(u"error:engine setup failed: "_s + error);
        QTimer::singleShot(500, qApp, [] { qApp->exit(1); });
    });
    if (!m_engine->isReady()) {
        say(u"engine:setting up"_s);
    }
    ensureEngine([this, say, url] {
        say(u"engine:"_s + m_engine->status().ytdlpVersion);
        m_downloadsController->requestDownload(QUrl::fromUserInput(url));
    });
}

void MainWindow::maybeShowGpuFallbackNotice()
{
    if (m_settings.videoDecodeFallbackNotice()) {
        m_settings.setVideoDecodeFallbackNotice(false);
        MessageSheet::info(this, tr("Hardware video decoding turned off"),
                           tr("A graphics-driver problem was detected while the GPU video decoder was on, so "
                              "it was switched off. Everything still works; you can turn it back on in "
                              "Settings, Advanced."));
        return;
    }
    if (!m_settings.gpuFallbackNotice()) {
        return;
    }
    m_settings.setGpuFallbackNotice(false);
    MessageSheet::info(this, tr("Hardware acceleration turned off"),
                       tr("A graphics-driver problem was detected, so the app switched to software rendering "
                          "to stay stable. Everything still works; you can turn hardware acceleration back "
                          "on in Settings, Advanced."));
}

void MainWindow::maybeShowWhatsNew(bool force)
{
    if (!force && (!m_settings.showWhatsNew() || m_settings.whatsNewSeenVersion() == m_appVersion ||
                   QApplication::activeModalWidget() != nullptr)) {
        return;
    }
    const QString notes = WhatsNewDialog::bundledNotes(m_appVersion);
    if (notes.isEmpty()) {
        qCWarning(lcUi) << "no changelog section for" << m_appVersion << "- What's new not shown";
        return;
    }
    m_settings.setWhatsNewSeenVersion(m_appVersion); // marked when shown: a crash must not loop it
    auto* dialog = new WhatsNewDialog(m_appVersion, WhatsNewDialog::bundledChangelog(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::handleRenderProcessGaveUp()
{
    showAndRaise();
    MessageSheet::info(
        this, tr("The browser page keeps crashing"),
        tr("The page's render process crashed several times in a row. If this keeps happening, "
           "try Settings, Advanced, Hardware acceleration: Off."));
}

// ---- browser ---------------------------------------------------------------

void MainWindow::setBrowserFullScreen(bool on)
{
    if (on) {
        m_stateBeforeFullScreen = windowState();
        m_rail->hide();
        showFullScreen();
        return;
    }
    m_rail->show();
    // Explicit restore: clearing the flag through setWindowState is unreliable on Wayland.
    if (m_stateBeforeFullScreen & Qt::WindowMaximized) {
        showMaximized();
    } else {
        showNormal();
    }
}

void MainWindow::handlePermissionPrompt(const QWebEnginePermission& permission)
{
    askPermission(this, permission, [this, permission](bool allow, bool remember) {
        m_browser->answerPermission(permission, allow, remember);
    });
}

} // namespace pldl::ui
