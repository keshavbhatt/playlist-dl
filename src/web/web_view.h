#pragma once

#include "core/render_crash_policy.h"
#include "web/error_page.h"

#include <QJsonObject>
#include <QUrl>
#include <QWebEnginePermission>
#include <QWebEngineView>

class QContextMenuEvent;
class QMenu;

namespace pldl::core {
class ThemeService;
} // namespace pldl::core

namespace pldl::web {

class Bridge;
class PermissionController;
class WebPage;
class WebProfile;

/// One tab of the built-in browser (FEATURES B1). Every view shares the one
/// WebProfile (cookies, storage, scripts) and owns its page, its bridge (so
/// a report from a script is known to come from this tab) and its permission
/// controller. Exposes what the UI layer needs: loading, crash recovery,
/// permission prompts, download requests and the media the page plays.
class WebView : public QWebEngineView
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebView)

public:
    /// `profile` must outlive the view: the owner deletes its views first.
    WebView(WebProfile& profile, core::ThemeService& theme, QWidget* parent = nullptr);
    ~WebView() override;

    void loadUrl(const QUrl& url);
    [[nodiscard]] QString userAgent() const;
    [[nodiscard]] WebProfile& profile() { return m_profile; }
    [[nodiscard]] WebPage& webPage() { return *m_page; }
    [[nodiscard]] Bridge& bridge() { return *m_bridge; }
    [[nodiscard]] PermissionController& permissions() { return *m_permissions; }
    /// The last media report from the page ("kind" empty when none).
    [[nodiscard]] const QJsonObject& pageMedia() const { return m_pageMedia; }
    /// Feeds a report as the page script would (tests, the debug hook).
    void setPageMedia(const QJsonObject& state);
    /// The UI's tokens for the load-failure page.
    void setErrorPageStyle(const ErrorPageStyle& style) { m_errorStyle = style; }

    /// Keyboard zoom (Ctrl+Plus, Ctrl+Minus, Ctrl+0), clamped by core::zoom.
    void zoomIn();
    void zoomOut();
    void resetZoom();

    /// What the context menu is built from; pure over the request so the
    /// menu can be tested without a page.
    struct ContextInfo
    {
        QUrl pageUrl;
        QUrl linkUrl;
        QUrl mediaUrl;
        bool isImage = false;
        bool isMedia = false; ///< a video or audio element
        bool editable = false;
        QString selectedText;
    };
    /// The curated menu (FEATURES B1): Back, Forward, Reload; Open link in
    /// new tab, Download link, Copy link; Download image or media, Copy
    /// address; Cut, Copy, Paste; Download this page. Caller owns the menu.
    [[nodiscard]] QMenu* buildContextMenu(const ContextInfo& info);

Q_SIGNALS:
    void renderProcessGaveUp();
    /// A link, image or media address should open in a new tab.
    void newTabRequested(const QUrl& url);
    /// Zoom changed through the keyboard (the UI shows the level).
    void zoomChanged(double factor);
    void permissionPromptRequested(QWebEnginePermission permission);
    /// A page script asked to download this URL.
    void downloadRequested(const QUrl& url);
    /// The media the page plays changed (FEATURES B4).
    void pageMediaChanged(const QJsonObject& state);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int exitCode);
    void showLoadError(const QString& title, const QString& detail);

    WebProfile& m_profile;
    core::ThemeService& m_theme;
    WebPage* m_page = nullptr;
    Bridge* m_bridge = nullptr;
    PermissionController* m_permissions = nullptr;
    core::RenderCrashPolicy m_crashPolicy;
    QUrl m_lastRequested;
    QJsonObject m_pageMedia;
    ErrorPageStyle m_errorStyle;
};

} // namespace pldl::web
