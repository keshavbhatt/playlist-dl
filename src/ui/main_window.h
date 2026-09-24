#pragma once

#include "core/downloads/download_job.h"
#include "core/settings/settings.h"

#include <QMainWindow>
#include <QPointer>
#include <QUrl>

#include <functional>

class QStackedWidget;
class QWebEnginePermission;

namespace pldl::core {
class ThemeService;
}
namespace pldl::services {
class EngineManager;
class LicenseService;
class MediaProbe;
} // namespace pldl::services

namespace pldl::ui {

class AccountDialog;
class Actions;
class BrowserPage;
class DownloadsController;
class DownloadsPage;
class EngineSetupDialog;
class Page;
class PlansDialog;
class SearchPage;
class SettingsDialog;
class SideRail;
class ThemeApplier;
class ThumbnailCache;
class ToastHost;
class TrayController;

/// Top-level window (DESIGN.md section 2): rail | page stack. Owns the pages
/// and the window-level behaviours (page switching, close-to-tray, browser
/// full screen, permission prompts). The Search, Playlist and Downloads pages
/// are placeholders until their own classes land; the Browser page is real.
class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MainWindow)

public:
    enum class PageId
    {
        Search,
        Playlist,
        Browser,
        Downloads,
    };

    MainWindow(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
               QWidget* parent = nullptr);
    ~MainWindow() override;

    void start();

    /// The download engine and the probe, shared by the Search (engine
    /// fallback), Playlist and Downloads pages. Created with the window,
    /// initialised in start().
    [[nodiscard]] services::EngineManager& engine() { return *m_engine; }
    [[nodiscard]] services::MediaProbe& probe() { return *m_probe; }
    /// Runs `then` once the engine is ready: at once when it is, otherwise
    /// after the setup sheet has provisioned it (nothing runs when the user
    /// closes the sheet or the install fails; the sheet shows the error).
    void ensureEngine(std::function<void()> then);
    void showEngineSetup();
    /// The download queue's owner (created after the Browser page: it needs
    /// the profile's cookies), for the Playlist and Search pages.
    [[nodiscard]] DownloadsController& downloads() { return *m_downloadsController; }
    /// A short confirmation in the window's corner ("Added to queue").
    void toast(const QString& text);
    void showPlans();

public Q_SLOTS:
    void showAndRaise();
    void toggleVisibility();
    void showPage(PageId page);
    /// A link from the CLI or another instance: opens on the Browser page.
    void openUrl(const QString& url);
    /// A playlist from the Search page (a card or a pasted link): the
    /// Playlist page shows it.
    void openPlaylist(const QUrl& url);
    /// A link to download right away (the CLI's --download, another
    /// instance): the engine is set up if needed, the link probed and queued
    /// with the default options, the Downloads page shown.
    void downloadUrl(const QString& url);
    /// Headless verification aid (PLDL_DEBUG_DOWNLOAD=<url>): queues the link
    /// into a temporary folder, prints "state:<name>" lines and the file path
    /// to stdout, then quits with 0 on Completed and 1 otherwise.
    void debugDownload(const QString& url);
    void showSettings();
    void showShortcuts();
    void showAbout();
    void showAccount();
    /// Headless verification aid: opens a screen by name: "about",
    /// "shortcuts", "account", "plans", "bug", "whatsnew", "settings",
    /// "browser:<url>" (a tab on that page), "browser-fullscreen",
    /// "search:<query>" (types and searches), "search-engine:<query>" (the
    /// same with the engine forced), "search-typing:<text>" (typed, with
    /// the suggestions), "search-demo" (canned results), "downloads-demo"
    /// (the Downloads page with one canned job per state), or a page name
    /// ("search", "playlist", "browser", "downloads").
    void debugOpen(const QString& what);
    void quit();

Q_SIGNALS:
    /// Settings, "Sign out and clear session", confirmed: the application
    /// writes the clear-session marker and relaunches.
    void clearSessionRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUi();
    void connectActions();
    void restoreWindowState();
    void saveWindowState();
    void maybeShowWhatsNew(bool force = false);
    void connectSettingsDialog(SettingsDialog* dialog);
    void confirmClearSession();
    void maybeShowGpuFallbackNotice();
    void promptUpgrade(const QString& feature);
    void handlePermissionPrompt(const QWebEnginePermission& permission);
    /// A browser video went full screen: the window follows, hiding its chrome.
    void setBrowserFullScreen(bool on);
    void handleRenderProcessGaveUp();
    /// One canned job per state for the "downloads-demo" grab.
    [[nodiscard]] static QList<core::DownloadJob> demoDownloads();

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_appVersion;
    Actions* m_actions = nullptr;
    SideRail* m_rail = nullptr;
    QStackedWidget* m_pages = nullptr;
    SearchPage* m_search = nullptr;
    Page* m_playlist = nullptr;
    BrowserPage* m_browser = nullptr;
    DownloadsPage* m_downloads = nullptr;
    DownloadsController* m_downloadsController = nullptr;
    ToastHost* m_toasts = nullptr;
    QPointer<PlansDialog> m_plans;
    TrayController* m_tray = nullptr;
    ThemeApplier* m_themeApplier = nullptr;
    services::LicenseService* m_license = nullptr;
    services::EngineManager* m_engine = nullptr;
    services::MediaProbe* m_probe = nullptr;
    QPointer<EngineSetupDialog> m_engineSetup;
    QList<std::function<void()>> m_awaitingEngine;
    QPointer<AccountDialog> m_accountDialog;
    QPointer<SettingsDialog> m_settingsDialog;
    bool m_quitting = false;
    Qt::WindowStates m_stateBeforeFullScreen = Qt::WindowNoState;
};

} // namespace pldl::ui
