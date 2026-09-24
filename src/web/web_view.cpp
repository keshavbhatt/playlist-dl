#include "web/web_view.h"

#include "web/popup_window.h"

#include "core/theme/theme_service.h"
#include "web/bridge.h"
#include "web/error_page.h"
#include "web/logging.h"
#include "web/permission_controller.h"
#include "web/web_page.h"
#include "web/web_profile.h"

#include "core/zoom_policy.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QMenu>
#include <QShortcut>
#include <QTimer>
#include <QWebEngineContextMenuRequest>
#include <QWebChannel>
#include <QWebEngineLoadingInfo>
#include <QWebEngineScript>

#include <chrono>

using namespace Qt::StringLiterals;

namespace pldl::web {

WebView::WebView(WebProfile& profile, core::ThemeService& theme, QWidget* parent)
    : QWebEngineView(parent)
    , m_profile(profile)
    , m_theme(theme)
    , m_page(new WebPage(profile, this))
    , m_bridge(new Bridge(this))
    , m_permissions(new PermissionController(this))
{
    m_page->setHostWidget(this);
    setPage(m_page);
    m_permissions->attach(*m_page);
    connect(m_permissions, &PermissionController::promptRequested, this, &WebView::permissionPromptRequested);

    // One channel per page with this tab's own bridge: a report from a script
    // is then known to be about this tab. The profile's config changes are
    // relayed so every tab's scripts see them.
    auto* channel = new QWebChannel(m_page);
    channel->registerObject(u"bridge"_s, m_bridge);
    m_page->setWebChannel(channel, QWebEngineScript::MainWorld);
    connect(&m_profile, &WebProfile::scriptConfigChanged, m_bridge, &Bridge::configChanged);
    connect(m_bridge, &Bridge::downloadUrlRequested, this,
            [this](const QString& url) { Q_EMIT downloadRequested(QUrl::fromUserInput(url)); });
    connect(m_bridge, &Bridge::pageMediaChanged, this, &WebView::setPageMedia);
    connect(m_bridge, &Bridge::retryRequested, this, [this] {
        if (m_lastRequested.isValid()) {
            load(m_lastRequested);
        }
    });

    connect(m_page, &QWebEnginePage::renderProcessTerminated, this, &WebView::handleRenderProcessTerminated);
    connect(m_page, &QWebEnginePage::loadingChanged, this, [this](const QWebEngineLoadingInfo& info) {
        if (info.status() == QWebEngineLoadingInfo::LoadStartedStatus) {
            setPageMedia({}); // a new document: what the old one played is gone
        } else if (info.status() == QWebEngineLoadingInfo::LoadSucceededStatus) {
            m_crashPolicy.onLoadSucceeded();
        } else if (info.status() == QWebEngineLoadingInfo::LoadFailedStatus && !info.isErrorPage() &&
                   info.errorDomain() != QWebEngineLoadingInfo::HttpStatusCodeDomain) {
            showLoadError(tr("This page could not be loaded"), info.errorString());
        }
    });
    m_page->setBackgroundColor(m_theme.isDark() ? QColor(0x12, 0x14, 0x17) : QColor(0xF6, 0xF7, 0xF9));
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) {
        m_page->setBackgroundColor(m_theme.isDark() ? QColor(0x12, 0x14, 0x17) : QColor(0xF6, 0xF7, 0xF9));
    });

    // Keyboard zoom while the page has focus (FEATURES A6): the render widget
    // swallows key events, a shortcut on the view still fires.
    const auto zoomShortcut = [this](const QList<QKeySequence>& keys, void (WebView::*slot)()) {
        for (const QKeySequence& key : keys) {
            auto* shortcut = new QShortcut(key, this);
            shortcut->setContext(Qt::WidgetWithChildrenShortcut);
            connect(shortcut, &QShortcut::activated, this, slot);
        }
    };
    zoomShortcut({QKeySequence(QKeySequence::ZoomIn), QKeySequence(Qt::CTRL | Qt::Key_Equal)}, &WebView::zoomIn);
    zoomShortcut({QKeySequence(QKeySequence::ZoomOut)}, &WebView::zoomOut);
    zoomShortcut({QKeySequence(Qt::CTRL | Qt::Key_0)}, &WebView::resetZoom);
}

void WebView::zoomIn()
{
    setZoomFactor(core::zoomIn(zoomFactor()));
    Q_EMIT zoomChanged(zoomFactor());
}

void WebView::zoomOut()
{
    setZoomFactor(core::zoomOut(zoomFactor()));
    Q_EMIT zoomChanged(zoomFactor());
}

void WebView::resetZoom()
{
    setZoomFactor(core::kDefaultZoom);
    Q_EMIT zoomChanged(zoomFactor());
}

namespace {
bool isWebUrl(const QUrl& url)
{
    return url.isValid() && (url.scheme() == u"http"_s || url.scheme() == u"https"_s);
}
} // namespace

QMenu* WebView::buildContextMenu(const ContextInfo& info)
{
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    // Navigation from the page's own actions: their enabled state follows the history.
    menu->addAction(m_page->action(QWebEnginePage::Back));
    menu->addAction(m_page->action(QWebEnginePage::Forward));
    menu->addAction(m_page->action(QWebEnginePage::Reload));

    if (isWebUrl(info.linkUrl)) {
        menu->addSeparator();
        const QUrl link = info.linkUrl;
        menu->addAction(tr("Open link in new tab"), this, [this, link] { Q_EMIT newTabRequested(link); });
        menu->addAction(tr("Download link"), this, [this, link] { Q_EMIT downloadRequested(link); });
        menu->addAction(tr("Copy link address"), this, [link] { QApplication::clipboard()->setText(link.toString()); });
    }
    if (isWebUrl(info.mediaUrl) && (info.isImage || info.isMedia)) {
        menu->addSeparator();
        const QUrl media = info.mediaUrl;
        menu->addAction(info.isImage ? tr("Download image") : tr("Download media"), this,
                        [this, media] { Q_EMIT downloadRequested(media); });
        menu->addAction(info.isImage ? tr("Copy image address") : tr("Copy media address"), this,
                        [media] { QApplication::clipboard()->setText(media.toString()); });
    }
    if (info.editable || !info.selectedText.isEmpty()) {
        menu->addSeparator();
        if (info.editable) {
            menu->addAction(m_page->action(QWebEnginePage::Cut));
        }
        menu->addAction(m_page->action(QWebEnginePage::Copy));
        if (info.editable) {
            menu->addAction(m_page->action(QWebEnginePage::Paste));
        }
    }
    if (isWebUrl(info.pageUrl)) {
        menu->addSeparator();
        const QUrl page = info.pageUrl;
        menu->addAction(tr("Download this page"), this, [this, page] { Q_EMIT downloadRequested(page); });
    }
    return menu;
}

void WebView::contextMenuEvent(QContextMenuEvent* event)
{
    const QWebEngineContextMenuRequest* request = lastContextMenuRequest();
    ContextInfo info;
    info.pageUrl = url();
    if (request != nullptr) {
        info.linkUrl = request->linkUrl();
        info.mediaUrl = request->mediaUrl();
        info.isImage = request->mediaType() == QWebEngineContextMenuRequest::MediaTypeImage;
        info.isMedia = request->mediaType() == QWebEngineContextMenuRequest::MediaTypeVideo ||
                       request->mediaType() == QWebEngineContextMenuRequest::MediaTypeAudio;
        info.editable = request->isContentEditable();
        info.selectedText = request->selectedText();
    }
    buildContextMenu(info)->popup(event->globalPos());
}

WebView::~WebView()
{
    // Every page must go before the profile it belongs to: the pop-up
    // windows are children of this widget and the owner deletes the views
    // before the shared profile.
    const QList<PopupWindow*> popups = findChildren<PopupWindow*>(Qt::FindDirectChildrenOnly);
    for (PopupWindow* popup : popups) {
        delete popup;
    }
    setPage(nullptr);
    delete m_page;
    m_page = nullptr;
}

void WebView::loadUrl(const QUrl& url)
{
    m_lastRequested = url;
    load(url);
}

QString WebView::userAgent() const
{
    return m_profile.httpUserAgent();
}

void WebView::setPageMedia(const QJsonObject& state)
{
    // Normalise: a report without a kind means "nothing plays".
    QJsonObject normalised;
    const QString kind = state.value(u"kind"_s).toString();
    if (!kind.isEmpty()) {
        normalised = state;
    }
    if (normalised == m_pageMedia) {
        return;
    }
    m_pageMedia = normalised;
    Q_EMIT pageMediaChanged(m_pageMedia);
}

void WebView::handleRenderProcessTerminated(QWebEnginePage::RenderProcessTerminationStatus status, int exitCode)
{
    qCWarning(lcWeb) << "render process terminated:" << status << exitCode;
    const auto now = std::chrono::milliseconds(QDateTime::currentMSecsSinceEpoch());
    const core::RenderCrashPolicy::Decision decision = m_crashPolicy.onCrash(now);
    if (!decision.reload) {
        Q_EMIT renderProcessGaveUp();
        return;
    }
    QTimer::singleShot(decision.delay, this, [this] {
        if (m_lastRequested.isValid()) {
            load(m_lastRequested);
        }
    });
}

void WebView::showLoadError(const QString& title, const QString& detail)
{
    m_page->setHtml(errorPageHtml(m_errorStyle, title, detail, m_lastRequested.isValid() ? m_lastRequested : url()));
}

} // namespace pldl::web
