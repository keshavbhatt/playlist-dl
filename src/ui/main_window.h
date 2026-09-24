#pragma once

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
class EngineSetupDialog;
class Page;
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

public Q_SLOTS:
    void showAndRaise();
    void toggleVisibility();
    void showPage(PageId page);
    /// A link from the CLI or another instance: opens on the Browser page.
    void openUrl(const QString& url);
    /// A playlist from the Search page (a card or a pasted link): the
    /// Playlist page shows it.
    void openPlaylist(const QUrl& url);
    /// A link to download right away (the Downloads page is not built yet:
    /// the link opens in the browser for now).
    void downloadUrl(const QString& url);
    void showSettings();
    void showShortcuts();
    void showAbout();
    void showAccount();
    /// Headless verification aid: opens a screen by name: "about",
    /// "shortcuts", "account", "plans", "bug", "whatsnew", "settings",
    /// "browser:<url>" (a tab on that page), "browser-fullscreen",
    /// "search:<query>" (types and searches), "search-engine:<query>" (the
    /// same with the engine forced), "search-typing:<text>" (typed, with
    /// the suggestions), "search-demo" (canned results), or a page name
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

private:
    void setupUi();
    void connectActions();
    void restoreWindowState();
    void saveWindowState();
    void maybeShowWhatsNew(bool force = false);
    void connectSettingsDialog(SettingsDialog* dialog);
    void confirmClearSession();
    void toast(const QString& text);
    void maybeShowGpuFallbackNotice();
    void promptUpgrade(const QString& feature);
    void handlePermissionPrompt(const QWebEnginePermission& permission);
    /// A browser video went full screen: the window follows, hiding its chrome.
    void setBrowserFullScreen(bool on);
    void handleRenderProcessGaveUp();

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    QString m_appVersion;
    Actions* m_actions = nullptr;
    SideRail* m_rail = nullptr;
    QStackedWidget* m_pages = nullptr;
    ThumbnailCache* m_thumbnails = nullptr;
    SearchPage* m_search = nullptr;
    Page* m_playlist = nullptr;
    BrowserPage* m_browser = nullptr;
    Page* m_downloads = nullptr;
    TrayController* m_tray = nullptr;
    ThemeApplier* m_themeApplier = nullptr;
    services::LicenseService* m_license = nullptr;
    services::EngineManager* m_engine = nullptr;
    services::MediaProbe* m_probe = nullptr;
    QPointer<EngineSetupDialog> m_engineSetup;
    QList<std::function<void()>> m_awaitingEngine;
    QPointer<AccountDialog> m_accountDialog;
    QPointer<SettingsDialog> m_settingsDialog;
    ToastHost* m_toasts = nullptr;
    bool m_quitting = false;
    Qt::WindowStates m_stateBeforeFullScreen = Qt::WindowNoState;
};

} // namespace pldl::ui
