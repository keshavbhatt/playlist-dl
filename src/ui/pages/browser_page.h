#pragma once

#include "ui/badge_label.h"
#include "ui/address_field.h"
#include "web/error_page.h"
#include "ui/pages/page.h"

#include <QJsonObject>
#include <QList>
#include <QPointer>
#include <QUrl>
#include <QWebEnginePermission>

class QCheckBox;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QShortcut;
class QStackedWidget;
class QTimer;
class QToolButton;
class QWebEngineFullScreenRequest;
class QWebEnginePage;

namespace pldl::core {
class Settings;
}
namespace pldl::web {
class FullScreenHint;
class WebProfile;
class WebView;
} // namespace pldl::web

namespace pldl::ui {

class BrowserTabButton;

/// The Browser page (mocks/browser.html, FEATURES B1, B2, B4): a tab strip
/// above a toolbar (back, forward, reload, the address, the ads-blocked
/// badge, Download this) over the web view, with the
/// page-detected media offered as a floating button. Owns the one web
/// profile every tab shares; the sign-in cookies it holds reach the download
/// engine through the downloads controller.
class BrowserPage : public Page
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BrowserPage)

public:
    BrowserPage(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
                QWidget* parent = nullptr);
    ~BrowserPage() override;

    /// Opens `url` in a new tab, or in the current one when it still shows the
    /// untouched start page. Returns the tab index.
    int open(const QUrl& url);
    /// A fresh tab on the start page; focus lands in the address bar.
    int newTab();
    void closeTab(int index);
    void closeCurrentTab() { closeTab(currentIndex()); }
    [[nodiscard]] int tabCount() const { return static_cast<int>(m_tabs.size()); }
    [[nodiscard]] int currentIndex() const;
    void setCurrentIndex(int index);
    void nextTab();
    void previousTab();
    [[nodiscard]] QUrl currentUrl() const;
    [[nodiscard]] web::WebView* currentView() const;
    [[nodiscard]] web::WebView* view(int index) const;

    // Toolbar actions, for the window's shortcuts.
    /// Find in page (mocks/browser-find.html): the bar under the toolbar
    /// opens or takes focus with the current tab's text selected.
    void showFind();
    void hideFind();
    [[nodiscard]] bool isFindVisible() const;
    [[nodiscard]] QLineEdit* findField() const { return m_findField; }
    [[nodiscard]] QLabel* findCount() const { return m_findCount; }
    void findNext();
    void findPrevious();
    void focusAddress();
    void reload();
    void back();
    void forward();
    /// Download this: the current page's link goes to the add flow.
    void downloadCurrent();

    [[nodiscard]] web::WebProfile& profile() { return *m_profile; }
    [[nodiscard]] QString userAgent() const;
    /// Every stored site permission is forgotten (Settings, Browser).
    void resetPermissions();
    /// The window's answer to permissionPromptRequested.
    void answerPermission(const QWebEnginePermission& permission, bool allow, bool remember);
    /// The busy state while the link is being checked: the button that was
    /// pressed (Download this, or the floating detected button) shows
    /// "Checking", the other waits disabled.
    void setDownloadBusy(bool busy);
    [[nodiscard]] bool isDownloadBusy() const;

    /// The floating button's text for a page-media report, empty for none
    /// ("Download detected: 1080p video"). Exposed for the tests.
    [[nodiscard]] static QString detectedLabel(const QJsonObject& media);
    /// Re-reads the blocked count and the sign-in state into the badges.
    void refreshBadges();
    [[nodiscard]] QLabel* adsBadge() const { return m_adsBadge; }
    [[nodiscard]] QPushButton* detectedButton() const { return m_detected; }
    /// Download this, reading "Open playlist" while the current tab shows a
    /// playlist page (the window lands it on the Playlist page).
    [[nodiscard]] QPushButton* downloadButton() const { return m_download; }
    /// The small × next to the floating button: hides it for this page's
    /// current media (it may cover page controls); a new report shows it again.
    [[nodiscard]] QToolButton* detectedDismissButton() const { return m_detectedDismiss; }
    [[nodiscard]] QLineEdit* addressField() const { return m_address; }
    [[nodiscard]] BrowserTabButton* tabButton(int index) const;

Q_SIGNALS:
    /// Download this, the detected media, or a page script: goes to the add flow.
    void downloadRequested(const QUrl& url);
    /// Keyboard zoom on the current tab, for a toast ("Zoom 110%").
    void zoomChanged(double factor);
    /// The user cancelled the check of the link sent to the add flow.
    void cancelDownloadRequested();
    /// The engine saved a page-made file (blob: or data:) into the download folder.
    void engineFileSaved(const QString& path);
    void engineFileFailed(const QString& fileName);
    void permissionPromptRequested(QWebEnginePermission permission);
    void renderProcessGaveUp();
    /// A page's video went full screen (true) or came back (false): the
    /// window hides its rail and pane and goes full screen itself, since the
    /// web view cannot move to another window without going blank.
    void fullScreenChanged(bool on);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Tab
    {
        web::WebView* view = nullptr;
        BrowserTabButton* button = nullptr;
        bool untouched = true; ///< still the start page, nothing typed
        bool detectedDismissed = false; ///< the floating button was hidden for the current report
    };

    void buildStrip();
    void buildToolbar();
    void buildFind();
    void buildStage();
    void runFind(bool backwards);
    void clearFind(web::WebView* view);
    void applyIcons();
    void onThemeChanged() override;
    int indexOf(const web::WebView* view) const;
    int indexOf(const BrowserTabButton* button) const;
    void connectTab(const Tab& tab);
    void beginDownload(QPushButton* source, const QUrl& url);
    void updateTabDescriptions();
    void applyErrorStyle();
    [[nodiscard]] web::ErrorPageStyle pageStyle() const;
    /// Loads the start page into `view`: the setting's address, or the themed
    /// invitation when the setting is an empty tab (review 2026-09-25).
    void loadStart(web::WebView* view);
    /// The open tabs are remembered (debounced) and come back on the next start.
    void scheduleSessionSave();
    void saveSession();
    void restoreSession();
    void navigate(const QString& text);
    void syncToolbar();
    void syncDetected();
    void placeDetected();
    void handleFullScreen(web::WebView* view, QWebEngineFullScreenRequest request);
    void enterFullScreen(web::WebView* view);
    void exitFullScreen();

public:
    [[nodiscard]] bool isInFullScreen() const { return m_fullScreenView != nullptr; }
    /// Developer aid (PLDL_DEBUG_OPEN=browser-fullscreen): the full screen
    /// layout without a request from the page.
    void debugEnterFullScreen() { enterFullScreen(currentView()); }

private:
    QWebEnginePage* openTabForPage(bool background);
    [[nodiscard]] QUrl startPage() const;

    core::Settings& m_settings;
    web::WebProfile* m_profile = nullptr;
    QList<Tab> m_tabs;
    QList<QPointer<web::WebView>> m_closing; ///< closed tabs awaiting deleteLater
    QFrame* m_strip = nullptr;
    QHBoxLayout* m_stripLayout = nullptr;
    QToolButton* m_newTab = nullptr;
    QToolButton* m_back = nullptr;
    QToolButton* m_forward = nullptr;
    QToolButton* m_reload = nullptr;
    AddressField* m_address = nullptr;
    BadgeLabel* m_adsBadge = nullptr;
    QPushButton* m_download = nullptr;
    QFrame* m_findBar = nullptr;
    QLineEdit* m_findField = nullptr;
    QLabel* m_findCount = nullptr;
    QToolButton* m_findPrevious = nullptr;
    QToolButton* m_findNext = nullptr;
    QCheckBox* m_findCase = nullptr;
    QToolButton* m_findClose = nullptr;
    QStackedWidget* m_stack = nullptr;
    QPushButton* m_detected = nullptr;
    QToolButton* m_detectedDismiss = nullptr;
    QTimer* m_badgeTimer = nullptr;
    QShortcut* m_escape = nullptr;
    QTimer* m_sessionTimer = nullptr;
    bool m_restoring = false;
    QPushButton* m_busySource = nullptr;
    web::FullScreenHint* m_fullScreenHint = nullptr;
    web::WebView* m_fullScreenView = nullptr; ///< the tab in full screen, null otherwise
    bool m_loading = false;
};

} // namespace pldl::ui
