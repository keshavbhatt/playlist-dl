#include "ui/main_window.h"

#include "core/log_sink.h"
#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "services/licensing/license_service.h"
#include "ui/about_dialog.h"
#include "ui/account_dialog.h"
#include "ui/actions.h"
#include "ui/bug_report_dialog.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/logging.h"
#include "ui/message_sheet.h"
#include "ui/pages/browser_page.h"
#include "ui/pages/page.h"
#include "ui/permission_prompt.h"
#include "ui/shortcuts_dialog.h"
#include "ui/side_rail.h"
#include "ui/theme_applier.h"
#include "ui/tray_controller.h"
#include "ui/whats_new_dialog.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QSessionManager>
#include <QStackedWidget>
#include <QTimer>
#include <QtEnvironmentVariables>

#include <algorithm>

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
    qCInfo(lcUi) << "main window gone";
}

void MainWindow::setupUi()
{
    setWindowTitle(u"Playlist Downloader"_s);
    setWindowIcon(icons::brand());
    resize(kDefaultSize);
    setMinimumSize(kMinimumSize);

    m_themeApplier = new ThemeApplier(m_theme, this);
    m_actions = new Actions(this);
    m_rail = new SideRail(*m_actions, m_theme, this);
    m_license = new services::LicenseService(m_settings, this);
    connect(m_license, &services::LicenseService::upgradeRequested, this, &MainWindow::promptUpgrade);
    m_tray = new TrayController(m_settings, *m_actions, this);

    m_pages = new QStackedWidget(this);
    m_search = new Page(tr("Search"), m_theme, this);
    m_search->setPlaceholder(tr("Search for playlists, videos and channels. Coming soon."));
    m_playlist = new Page(tr("Playlist"), m_theme, this);
    m_playlist->setPlaceholder(tr("A playlist's entries, ready to play or download. Coming soon."));
    m_browser = new BrowserPage(m_settings, m_theme, m_appVersion, this);
    m_downloads = new Page(tr("Downloads"), m_theme, this);
    m_downloads->setPlaceholder(tr("The download queue. Coming soon."));
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
    showPage(static_cast<PageId>(std::min(m_settings.lastPage(), static_cast<int>(PageId::Downloads))));
}

void MainWindow::connectActions()
{
    Actions& a = *m_actions;
    connect(a.home, &QAction::triggered, this, [this] { showPage(PageId::Search); });
    connect(a.playlist, &QAction::triggered, this, [this] { showPage(PageId::Playlist); });
    connect(a.browser, &QAction::triggered, this, [this] { showPage(PageId::Browser); });
    connect(a.downloads, &QAction::triggered, this, [this] { showPage(PageId::Downloads); });
    connect(a.showHide, &QAction::triggered, this, [this] {
        // Ctrl+W on the Browser page closes the tab, as in every browser.
        if (isVisible() && m_pages->currentWidget() == m_browser) {
            m_browser->closeCurrentTab();
            return;
        }
        toggleVisibility();
    });
    connect(a.settings, &QAction::triggered, this, &MainWindow::showSettings);
    connect(a.shortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);
    connect(a.onlineGuide, &QAction::triggered, this, [] { platform::openUrl(links::kGuide); });
    connect(a.openLogFolder, &QAction::triggered, this,
            [] { platform::openDirectory(QFileInfo(core::LogSink::logFilePath()).absolutePath()); });
    connect(a.about, &QAction::triggered, this, &MainWindow::showAbout);
    connect(a.account, &QAction::triggered, this, &MainWindow::showAccount);
    connect(a.quit, &QAction::triggered, this, &MainWindow::quit);

    connect(m_tray, &TrayController::toggleRequested, this, &MainWindow::toggleVisibility);
    connect(m_browser, &BrowserPage::renderProcessGaveUp, this, &MainWindow::handleRenderProcessGaveUp);
    connect(m_browser, &BrowserPage::fullScreenChanged, this, &MainWindow::setBrowserFullScreen);
    connect(m_browser, &BrowserPage::permissionPromptRequested, this, &MainWindow::handlePermissionPrompt);
    connect(m_browser, &BrowserPage::downloadRequested, this,
            [this](const QUrl& url) { downloadUrl(url.toString()); });
    // Browser shortcuts act while the Browser page is showing; New tab brings
    // it up. Ctrl+W is showHide's: it closes a tab there.
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
    QTimer::singleShot(600, this, [this] {
        maybeShowGpuFallbackNotice();
        QTimer::singleShot(3000, this, [this] { maybeShowWhatsNew(); });
    });
}

// ---- pages -----------------------------------------------------------------

void MainWindow::showPage(PageId page)
{
    const int index = static_cast<int>(page);
    m_pages->setCurrentIndex(index);
    m_settings.setLastPage(index);
    QAction* actions[] = {m_actions->home, m_actions->playlist, m_actions->browser, m_actions->downloads};
    actions[index]->setChecked(true);
}

void MainWindow::openUrl(const QString& url)
{
    showAndRaise();
    showPage(PageId::Browser);
    m_browser->open(QUrl::fromUserInput(url));
}

void MainWindow::downloadUrl(const QString& url)
{
    if (url.trimmed().isEmpty()) {
        return;
    }
    qCInfo(lcUi) << "download requested (queue not built yet), opening in the browser:" << url;
    openUrl(url);
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

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_quitting && m_settings.closeAction() == core::CloseAction::MinimizeToTray && m_tray->isAvailable()) {
        event->ignore();
        hide();
        return;
    }
    m_quitting = true;
    saveWindowState();
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
    MessageSheet::info(this, tr("Settings"), tr("The settings sheet is not built yet. It comes with the next step."));
}

void MainWindow::showShortcuts()
{
    auto* dialog = new ShortcutsDialog(*m_actions, m_theme, this);
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
        auto* dialog = new BugReportDialog(m_settings, m_theme, m_browser->userAgent(), QString(), this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    } else if (what == u"whatsnew"_s) {
        maybeShowWhatsNew(true);
    } else if (what == u"settings"_s) {
        showSettings();
    } else if (what.startsWith(u"browser:"_s)) {
        openUrl(what.mid(8));
    } else if (what == u"browser-fullscreen"_s) {
        showPage(PageId::Browser);
        m_browser->debugEnterFullScreen();
    } else if (what == u"search"_s) {
        showPage(PageId::Search);
    } else if (what == u"playlist"_s) {
        showPage(PageId::Playlist);
    } else if (what == u"browser"_s) {
        showPage(PageId::Browser);
    } else if (what == u"downloads"_s) {
        showPage(PageId::Downloads);
    } else {
        qCWarning(lcUi) << "debugOpen: unknown target" << what;
    }
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
    if (!force &&
        (m_settings.whatsNewSeenVersion() == m_appVersion || QApplication::activeModalWidget() != nullptr)) {
        return;
    }
    const QString notes = WhatsNewDialog::bundledNotes(m_appVersion);
    if (notes.isEmpty()) {
        qCWarning(lcUi) << "no changelog section for" << m_appVersion << "- What's new not shown";
        return;
    }
    m_settings.setWhatsNewSeenVersion(m_appVersion); // marked when shown: a crash must not loop it
    auto* dialog = new WhatsNewDialog(m_appVersion, notes, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void MainWindow::handleRenderProcessGaveUp()
{
    showAndRaise();
    MessageSheet::info(this, tr("The browser page keeps crashing"),
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
