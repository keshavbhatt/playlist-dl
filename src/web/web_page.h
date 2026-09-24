#pragma once

#include "web/script_dialogs.h"

#include <QWebEnginePage>
#include <QWidget>

#include <functional>

class QAuthenticator;

namespace pldl::web {
class WebProfile;
}

namespace pldl::web {

/// Answers an HTTP or proxy authentication request through the dialogs'
/// `authenticate`, or leaves it unanswered (shared by the main page and the
/// pop-up windows).
void answerAuthentication(const QUrl& url, QAuthenticator* authenticator, bool proxy, const ScriptDialogs& dialogs,
                          QWidget* window);

} // namespace pldl::web

namespace pldl::web {

/// The built-in browser page. Routes the page console to the `pldl.web.js`
/// logging category, hands non-web links to the desktop and hosts
/// window.open() targets (sign-in) in PopupWindows.
class WebPage : public QWebEnginePage
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebPage)

public:
    explicit WebPage(WebProfile& profile, QObject* parent = nullptr);
    ~WebPage() override = default;

    /// Parent for pop-up windows and dialogs (the main window's view).
    void setHostWidget(QWidget* host) { m_host = host; }

    /// Replacements for the stock script dialogs; pop-up windows inherit them.
    void setScriptDialogs(ScriptDialogs dialogs) { m_dialogs = std::move(dialogs); }

    /// Where target=_blank links go: the Browser page supplies a page in a new
    /// tab (`background` for InNewBackgroundTab). Without an opener, or for
    /// window.open() dialogs (sign-in), a PopupWindow is used.
    using TabOpener = std::function<QWebEnginePage*(bool background)>;
    void setTabOpener(TabOpener opener) { m_tabOpener = std::move(opener); }

Q_SIGNALS:
    void popupOpened(QWidget* window);

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber,
                                  const QString& sourceId) override;
    void javaScriptAlert(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptConfirm(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue,
                          QString* result) override;

private:
    [[nodiscard]] QWidget* hostWindow() const { return m_host != nullptr ? m_host->window() : nullptr; }

    WebProfile& m_profile;
    QWidget* m_host = nullptr;
    ScriptDialogs m_dialogs;
    TabOpener m_tabOpener;
};

} // namespace pldl::web
