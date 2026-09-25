#include "ui/pages/browser_page.h"

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "ui/browser_tab.h"
#include "ui/busy_button.h"
#include "ui/icons.h"
#include "ui/keyboard.h"
#include "ui/logging.h"
#include "ui/message_sheet.h"
#include "ui/pldl_style.h"
#include "web/cookie_exporter.h"
#include "web/full_screen_hint.h"
#include "web/permission_controller.h"
#include "web/request_interceptor.h"
#include "web/web_page.h"
#include "web/web_profile.h"
#include "web/web_view.h"

#include <QAbstractButton>
#include <QAccessible>
#include <QCheckBox>
#include <QWebEngineFindTextResult>
#include <QFrame>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineHistory>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kToolButton = 34;
constexpr int kDetectedInset = 36;
constexpr int kBadgeRefreshMs = 1000;

QString hostOf(const QUrl& url)
{
    QString host = url.host();
    if (host.startsWith(u"www."_s)) {
        host.remove(0, 4);
    }
    return host;
}
} // namespace

BrowserPage::BrowserPage(core::Settings& settings, core::ThemeService& theme, const QString& appVersion,
                         QWidget* parent)
    : Page(tr("Browser"), theme, parent)
    , m_settings(settings)
    , m_profile(new web::WebProfile(settings, appVersion, this))
{
    setTitleVisible(false);
    buildStrip();
    buildToolbar();
    buildFind();
    buildStage();

    m_badgeTimer = new QTimer(this);
    m_badgeTimer->setInterval(kBadgeRefreshMs);
    connect(m_badgeTimer, &QTimer::timeout, this, &BrowserPage::refreshBadges);
    connect(&m_settings, &core::Settings::blockAdsChanged, this, [this](bool) { refreshBadges(); });

    // Esc stops the load, or leaves full screen; the web view swallows plain
    // key events so this is a shortcut on the page (FEATURES M3 for pop-ups).
    // Armed only while there is something to stop, so a plain Esc still
    // reaches the window (it closes the details pane there).
    m_escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    m_escape->setContext(Qt::WidgetWithChildrenShortcut);
    m_escape->setEnabled(false);
    connect(m_escape, &QShortcut::activated, this, [this] {
        if (m_fullScreenView != nullptr) {
            exitFullScreen();
        } else if (isDownloadBusy()) {
            Q_EMIT cancelDownloadRequested();
        } else if (web::WebView* view = currentView(); view != nullptr && m_loading) {
            view->stop();
        }
    });
    // A file the page wants to download (a link click, "Save link") goes to
    // the app's own queue; the engine's transfer was cancelled.
    connect(m_profile, &web::WebProfile::fileDownloadRequested, this,
            [this](const QUrl& url, const QString&) { beginDownload(m_download, url); });
    connect(m_profile, &web::WebProfile::engineFileSaved, this, &BrowserPage::engineFileSaved);
    connect(m_profile, &web::WebProfile::engineFileFailed, this, &BrowserPage::engineFileFailed);

    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setSingleShot(true);
    m_sessionTimer->setInterval(500);
    connect(m_sessionTimer, &QTimer::timeout, this, &BrowserPage::saveSession);

    restoreSession();
    applyIcons();
    refreshBadges();
}

void BrowserPage::restoreSession()
{
    // The tabs of the last session come back, on the addresses they showed,
    // when the Browser setting asks for it (off by default); otherwise, or
    // without any, one tab on the start page. The session is saved either way
    // so switching the setting on later reopens the tabs of that run.
    const core::Settings::BrowserSession session =
        m_settings.restoreBrowserTabs() ? m_settings.browserSession() : core::Settings::BrowserSession{};
    m_restoring = true;
    newTab();
    for (int i = 0; i < session.urls.size(); ++i) {
        const QUrl url = QUrl::fromUserInput(session.urls.at(i));
        const bool web = url.isValid() && (url.scheme() == u"http"_s || url.scheme() == u"https"_s);
        if (i > 0) {
            newTab();
        }
        if (web) {
            m_tabs.last().untouched = false;
            if (core::Settings::isEmptyStartPage(url.toString())) {
                loadStart(m_tabs.last().view);
            } else {
                m_tabs.last().view->loadUrl(url);
            }
        } else {
            loadStart(m_tabs.last().view);
        }
    }
    if (session.urls.isEmpty()) {
        loadStart(m_tabs.first().view);
    }
    m_restoring = false;
    setCurrentIndex(session.urls.isEmpty() ? 0 : session.current);
    if (!session.urls.isEmpty()) {
        qCInfo(lcUi) << "browser: restored" << session.urls.size() << "tabs";
    }
}

void BrowserPage::scheduleSessionSave()
{
    if (!m_restoring && m_sessionTimer != nullptr) {
        m_sessionTimer->start();
    }
}

void BrowserPage::saveSession()
{
    core::Settings::BrowserSession session;
    for (const Tab& tab : std::as_const(m_tabs)) {
        const QUrl url = tab.view->url();
        const bool web = url.isValid() && (url.scheme() == u"http"_s || url.scheme() == u"https"_s);
        session.urls << (web && !tab.untouched ? url.toString() : QString(core::Settings::kEmptyStartPage));
    }
    session.current = std::max(0, currentIndex());
    m_settings.setBrowserSession(session);
}

BrowserPage::~BrowserPage()
{
    // Every view (and the pop-ups it parents) goes before the profile they
    // share; QObject would otherwise delete the older child, the profile,
    // first.
    exitFullScreen();
    if (m_sessionTimer != nullptr) {
        m_sessionTimer->stop();
        saveSession(); // the final word, whatever the debounce had pending
    }
    for (Tab& tab : m_tabs) {
        delete tab.view;
        tab.view = nullptr;
    }
    m_tabs.clear();
    for (const QPointer<web::WebView>& view : std::as_const(m_closing)) {
        delete view.data();
    }
    m_closing.clear();
    delete m_profile;
    m_profile = nullptr;
}

// ---- construction --------------------------------------------------------------

void BrowserPage::buildStrip()
{
    m_strip = new QFrame(this);
    m_strip->setObjectName(u"tabStrip"_s);
    m_strip->setProperty("pldlTabStrip", true);
    m_stripLayout = new QHBoxLayout(m_strip);
    m_stripLayout->setContentsMargins(12, 8, 12, 0);
    m_stripLayout->setSpacing(6);
    m_newTab = new QToolButton(m_strip);
    m_newTab->setProperty("pldlFlat", true);
    m_newTab->setAutoRaise(true);
    m_newTab->setFixedSize(kToolButton, kToolButton); // a 34 px target like the toolbar's
    m_newTab->setIconSize(QSize(18, 18));
    m_newTab->setCursor(Qt::PointingHandCursor);
    m_newTab->setToolTip(tr("New tab (Ctrl+T)"));
    m_newTab->setAccessibleName(tr("New tab"));
    connect(m_newTab, &QToolButton::clicked, this, [this] { newTab(); });
    m_stripLayout->addWidget(m_newTab, 0, Qt::AlignVCenter);
    m_stripLayout->addStretch(1);
    keyboard::installArrowNavigation(m_strip);
    insertAboveHeader(m_strip);
}

void BrowserPage::buildToolbar()
{
    QHBoxLayout* header = headerLayout();
    header->setContentsMargins(16, 10, 16, 10);
    header->setSpacing(8);
    // The base header holds the (hidden) title and a stretch: the toolbar
    // replaces the stretch with the address field.
    header->setStretch(1, 0);

    auto makeNav = [this](const QString& tip, const QString& name) {
        auto* button = new QToolButton(this);
        button->setProperty("pldlFlat", true);
        button->setAutoRaise(true);
        button->setFixedSize(kToolButton, kToolButton);
        button->setIconSize(QSize(18, 18));
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tip);
        button->setAccessibleName(name);
        return button;
    };
    m_back = makeNav(tr("Back (Alt+Left)"), tr("Back"));
    m_forward = makeNav(tr("Forward (Alt+Right)"), tr("Forward"));
    m_reload = makeNav(tr("Reload (F5)"), tr("Reload"));
    connect(m_back, &QToolButton::clicked, this, &BrowserPage::back);
    connect(m_forward, &QToolButton::clicked, this, &BrowserPage::forward);
    connect(m_reload, &QToolButton::clicked, this, [this] {
        if (web::WebView* view = currentView(); view != nullptr) {
            if (m_loading) {
                view->stop();
            } else {
                view->reload();
            }
        }
    });

    m_address = new AddressField(theme(), this);
    m_address->setObjectName(u"addressField"_s);
    m_address->setPlaceholderText(tr("Search or enter a web address"));
    m_address->setAccessibleName(tr("Address"));
    m_address->setClearButtonEnabled(true);
    m_address->installEventFilter(this); // Esc restores the address and returns to the page
    connect(m_address, &QLineEdit::returnPressed, this, [this] { navigate(m_address->text()); });

    // The badges paint their own glyph and words (BadgeLabel): a rich-text
    // <img> in a QLabel never lines up with the text it sits beside.
    m_adsBadge = new BadgeLabel(theme(), this);
    m_adsBadge->setObjectName(u"adsBadge"_s);
    m_adsBadge->setGlyph(u"shield"_s);
    m_adsBadge->setOk(true);
    m_adsBadge->setToolTip(tr("Requests to advertising and tracking hosts blocked on this session."));
    // No Signed in badge: it spoke about the YouTube session on every site
    // (owner, on SoundCloud's sign-in page). The session still reaches the engine.

    m_download = new QPushButton(tr("Download this"), this);
    m_download->setObjectName(u"downloadThisButton"_s);
    m_download->setProperty("pldlPrimary", true);
    m_download->setCursor(Qt::PointingHandCursor);
    m_download->setToolTip(tr("Download the page you are looking at (Ctrl+D)"));
    connect(m_download, &QPushButton::clicked, this, [this] { beginDownload(m_download, currentUrl()); });

    int index = 1; // after the hidden title
    header->insertWidget(index++, m_back);
    header->insertWidget(index++, m_forward);
    header->insertWidget(index++, m_reload);
    header->insertWidget(index++, m_address, 1);
    header->insertWidget(index++, m_adsBadge);
    header->insertWidget(index++, m_download);
}

void BrowserPage::buildFind()
{
    // mocks/browser-find.html: a panel-coloured bar with a hairline under the
    // toolbar. Enter and Shift+Enter step through the matches, Esc closes it
    // and returns to the page.
    m_findBar = new QFrame(this);
    m_findBar->setObjectName(u"findBar"_s);
    m_findBar->setProperty("pldlSection", true);
    auto* row = new QHBoxLayout(m_findBar);
    row->setContentsMargins(16, 8, 16, 8);
    row->setSpacing(8);
    auto* glyph = new QLabel(m_findBar);
    glyph->setObjectName(u"findGlyph"_s);
    glyph->setFixedSize(16, 16);
    row->addWidget(glyph);
    m_findField = new QLineEdit(m_findBar);
    m_findField->setObjectName(u"findField"_s);
    m_findField->setPlaceholderText(tr("Find in page"));
    m_findField->setAccessibleName(tr("Find in page"));
    m_findField->setFixedWidth(320);
    m_findField->setClearButtonEnabled(true);
    m_findField->installEventFilter(this); // Esc closes, Shift+Enter goes back
    connect(m_findField, &QLineEdit::textChanged, this, [this](const QString&) { runFind(false); });
    connect(m_findField, &QLineEdit::returnPressed, this, [this] { findNext(); });
    row->addWidget(m_findField);
    m_findCount = new QLabel(m_findBar);
    m_findCount->setObjectName(u"findCount"_s);
    m_findCount->setProperty("pldlMuted", true);
    m_findCount->setMinimumWidth(56);
    row->addWidget(m_findCount);
    auto makeStep = [this](const QString& tip, const QString& name) {
        auto* button = new QToolButton(m_findBar);
        button->setProperty("pldlFlat", true);
        button->setAutoRaise(true);
        button->setFixedSize(kToolButton, kToolButton);
        button->setIconSize(QSize(16, 16));
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(tip);
        button->setAccessibleName(name);
        return button;
    };
    m_findPrevious = makeStep(tr("Previous match (Shift+Enter)"), tr("Previous match"));
    m_findNext = makeStep(tr("Next match (Enter)"), tr("Next match"));
    connect(m_findPrevious, &QToolButton::clicked, this, &BrowserPage::findPrevious);
    connect(m_findNext, &QToolButton::clicked, this, &BrowserPage::findNext);
    row->addWidget(m_findPrevious);
    row->addWidget(m_findNext);
    m_findCase = new QCheckBox(tr("Match case"), m_findBar);
    connect(m_findCase, &QCheckBox::toggled, this, [this](bool) { runFind(false); });
    row->addWidget(m_findCase);
    row->addStretch(1);
    m_findClose = makeStep(tr("Close (Esc)"), tr("Close find"));
    connect(m_findClose, &QToolButton::clicked, this, &BrowserPage::hideFind);
    row->addWidget(m_findClose);
    m_findBar->hide();
    content()->addWidget(m_findBar);
}

void BrowserPage::showFind()
{
    m_findBar->show();
    m_findField->setFocus(Qt::ShortcutFocusReason);
    m_findField->selectAll();
    if (!m_findField->text().isEmpty()) {
        runFind(false);
    }
}

void BrowserPage::hideFind()
{
    if (!m_findBar->isVisible()) {
        return;
    }
    m_findBar->hide();
    clearFind(currentView());
    if (web::WebView* view = currentView(); view != nullptr) {
        view->setFocus(Qt::OtherFocusReason);
    }
}

bool BrowserPage::isFindVisible() const
{
    return m_findBar->isVisible();
}

void BrowserPage::findNext()
{
    runFind(false);
}

void BrowserPage::findPrevious()
{
    runFind(true);
}

void BrowserPage::clearFind(web::WebView* view)
{
    if (view != nullptr) {
        view->findText(QString());
    }
    m_findCount->clear();
    m_findField->setProperty("pldlNoMatch", false);
    m_findField->style()->unpolish(m_findField);
    m_findField->style()->polish(m_findField);
}

void BrowserPage::runFind(bool backwards)
{
    web::WebView* view = currentView();
    const QString text = m_findField->text();
    if (view == nullptr || !m_findBar->isVisible()) {
        return;
    }
    if (text.isEmpty()) {
        clearFind(view);
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (backwards) {
        flags |= QWebEnginePage::FindBackward;
    }
    if (m_findCase->isChecked()) {
        flags |= QWebEnginePage::FindCaseSensitively;
    }
    view->findText(text, flags, [this, view](const QWebEngineFindTextResult& result) {
        if (view != currentView() || !m_findBar->isVisible()) {
            return;
        }
        const bool none = result.numberOfMatches() == 0;
        //: The find bar's count: the active match of how many
        m_findCount->setText(none ? tr("No matches") : tr("%1 of %2").arg(result.activeMatch()).arg(result.numberOfMatches()));
        m_findCount->setAccessibleName(m_findCount->text());
        m_findField->setProperty("pldlNoMatch", none);
        m_findField->style()->unpolish(m_findField);
        m_findField->style()->polish(m_findField);
    });
}

void BrowserPage::buildStage()
{
    // The web page fills the content area edge to edge; the detected-media
    // button floats over its bottom right corner (mocks/browser.html).
    content()->setContentsMargins(0, 0, 0, 0);
    content()->setSpacing(0);
    // The load progress lives inside the address field (AddressField): a bar
    // in this layout moved the page by its height at every load (UMD owner).

    auto* stage = new QWidget(this);
    stage->setObjectName(u"browserStage"_s);
    auto* stageLayout = new QVBoxLayout(stage);
    stageLayout->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(stage);
    stageLayout->addWidget(m_stack);
    content()->addWidget(stage, 1);

    m_detected = new QPushButton(stage);
    m_detected->setObjectName(u"detectedButton"_s);
    m_detected->setProperty("pldlPrimary", true);
    m_detected->setCursor(Qt::PointingHandCursor);
    m_detected->setText(tr("Download detected"));
    m_detected->setToolTip(tr("Download the media this page is playing"));
    m_detected->hide();
    connect(m_detected, &QPushButton::clicked, this, [this] {
        web::WebView* view = currentView();
        if (view == nullptr) {
            return;
        }
        const QString direct = view->pageMedia().value(u"direct"_s).toString();
        const QUrl target = direct.isEmpty() ? view->url() : QUrl(direct);
        qCInfo(lcUi) << "browser: detected media download" << target;
        beginDownload(m_detected, target);
    });
    // The floating button can sit over a page's own controls (a cookie bar):
    // a × next to it puts it away until the page reports new media.
    m_detectedDismiss = new QToolButton(stage);
    m_detectedDismiss->setObjectName(u"detectedDismissButton"_s);
    m_detectedDismiss->setProperty("pldlTabClose", true);
    m_detectedDismiss->setAutoRaise(true);
    m_detectedDismiss->setFixedSize(28, 28);
    m_detectedDismiss->setIconSize(QSize(14, 14));
    m_detectedDismiss->setCursor(Qt::PointingHandCursor);
    m_detectedDismiss->setToolTip(tr("Hide this button"));
    m_detectedDismiss->setAccessibleName(tr("Hide the download button"));
    m_detectedDismiss->hide();
    connect(m_detectedDismiss, &QToolButton::clicked, this, [this] {
        if (const int index = currentIndex(); index >= 0) {
            m_tabs[index].detectedDismissed = true;
        }
        syncDetected();
    });
    stage->installEventFilter(this);
}

void BrowserPage::applyIcons()
{
    const Tokens t = Tokens::forScheme(theme().isDark());
    m_newTab->setIcon(icons::themed(u"plus"_s, t.text, t.muted));
    m_findPrevious->setIcon(icons::themed(u"chevron-up"_s, t.text, t.muted));
    m_findNext->setIcon(icons::themed(u"chevron-down"_s, t.text, t.muted));
    m_findClose->setIcon(icons::themed(u"cancel"_s, t.text, t.muted));
    if (auto* glyph = m_findBar->findChild<QLabel*>(u"findGlyph"_s); glyph != nullptr) {
        glyph->setPixmap(icons::pixmap(u"search"_s, t.muted, 16, devicePixelRatioF()));
    }
    m_back->setIcon(icons::themed(u"back"_s, t.text, t.muted));
    m_forward->setIcon(icons::themed(u"forward"_s, t.text, t.muted));
    m_reload->setIcon(icons::themed(m_loading ? u"stop"_s : u"reload"_s, t.text, t.muted));
    m_download->setIcon(icons::themed(u"download"_s, t.accentText));
    m_detected->setIcon(icons::themed(u"download"_s, t.accentText));
    m_detectedDismiss->setIcon(icons::themed(u"cancel"_s, t.muted));
}

void BrowserPage::onThemeChanged()
{
    applyIcons();
    applyErrorStyle();
}

web::ErrorPageStyle BrowserPage::pageStyle() const
{
    const Tokens t = Tokens::forScheme(theme().isDark());
    web::ErrorPageStyle style;
    style.background = t.bg;
    style.text = t.text;
    style.muted = t.muted;
    style.accent = t.accentStrong;
    style.accentHover = t.accentHover;
    return style;
}

void BrowserPage::applyErrorStyle()
{
    const web::ErrorPageStyle style = pageStyle();
    for (const Tab& tab : std::as_const(m_tabs)) {
        tab.view->setErrorPageStyle(style);
        if (tab.untouched && core::Settings::isEmptyStartPage(tab.view->url().toString())) {
            loadStart(tab.view); // the invitation follows the theme
        }
    }
}

void BrowserPage::loadStart(web::WebView* view)
{
    const QUrl start = startPage();
    if (!core::Settings::isEmptyStartPage(start.toString())) {
        view->loadUrl(start);
        return;
    }
    // An empty tab says what to do instead of showing nothing (review 2026-09-25).
    view->setHtml(web::startPageHtml(pageStyle(), tr("Type an address or a search above"),
                                     tr("Any site works here. Download this saves what a page shows; a playlist page "
                                        "opens on the Playlist page. Sign in to a site once and downloads use it too.")),
                  QUrl(QString(core::Settings::kEmptyStartPage)));
}

void BrowserPage::updateTabDescriptions()
{
    // "Tab 2 of 3" for screen readers; the name stays the title.
    const int count = static_cast<int>(m_tabs.size());
    for (int i = 0; i < count; ++i) {
        m_tabs.at(i).button->setAccessibleDescription(tr("Tab %1 of %2").arg(i + 1).arg(count));
    }
}

// ---- tabs --------------------------------------------------------------------------

QUrl BrowserPage::startPage() const
{
    return QUrl::fromUserInput(m_settings.browserStartPage());
}

int BrowserPage::newTab()
{
    Tab tab;
    tab.view = new web::WebView(*m_profile, theme(), m_stack);
    tab.view->setObjectName(u"webView"_s);
    tab.button = new BrowserTabButton(theme(), m_strip);
    tab.button->setAutoExclusive(false);
    m_tabs.append(tab);
    // Before the plus button and the stretch.
    m_stripLayout->insertWidget(static_cast<int>(m_tabs.size()) - 1, tab.button, 1);
    tab.button->show(); // at once, not on the layout's next turn: the arrow ring counts visible tabs
    m_stack->addWidget(tab.view);
    connectTab(tab);
    keyboard::installArrowNavigation(m_strip); // picks up the new tab's button
    updateTabDescriptions();
    applyErrorStyle();
    if (!m_restoring) {
        loadStart(tab.view); // a restored tab loads its own address instead
    }
    tab.button->setTitle(hostOf(startPage()));
    const int index = static_cast<int>(m_tabs.size()) - 1;
    setCurrentIndex(index);
    if (isVisible()) {
        focusAddress();
    }
    scheduleSessionSave();
    qCInfo(lcUi) << "browser: new tab" << index;
    return index;
}

int BrowserPage::open(const QUrl& url)
{
    if (!url.isValid() || url.isEmpty()) {
        return currentIndex();
    }
    const int current = currentIndex();
    if (current >= 0 && m_tabs.at(current).untouched) {
        if (core::Settings::isEmptyStartPage(url.toString())) {
            loadStart(m_tabs.at(current).view); // an empty address is the invitation, not a blank page
            return current;
        }
        m_tabs[current].untouched = false;
        m_tabs.at(current).view->loadUrl(url);
        m_address->setText(url.toString());
        return current;
    }
    const int index = newTab();
    m_tabs[index].untouched = false;
    m_tabs.at(index).view->loadUrl(url);
    m_address->setText(url.toString());
    return index;
}

void BrowserPage::connectTab(const Tab& tab)
{
    web::WebView* view = tab.view;
    BrowserTabButton* button = tab.button;
    connect(button, &QAbstractButton::clicked, this, [this, button] { setCurrentIndex(indexOf(button)); });
    connect(button, &BrowserTabButton::closeRequested, this, [this, button] { closeTab(indexOf(button)); });

    connect(view, &QWebEngineView::titleChanged, this, [view, button](const QString& title) {
        // An empty tab is "New tab", not the engine's "about:blank".
        const bool blank = title.isEmpty() || core::Settings::isEmptyStartPage(title);
        button->setTitle(blank ? hostOf(view->url()) : title);
    });
    connect(view, &QWebEngineView::iconChanged, this, [button](const QIcon& icon) { button->setSiteIcon(icon); });
    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl& url) {
        const int index = indexOf(view);
        if (index >= 0 && !url.matches(startPage(), QUrl::StripTrailingSlash | QUrl::NormalizePathSegments)) {
            m_tabs[index].untouched = false;
        }
        if (view == currentView()) {
            syncToolbar();
        }
        scheduleSessionSave();
    });
    connect(view, &QWebEngineView::loadStarted, this, [this, view, button] {
        button->setLoading(true);
        if (view == currentView()) {
            m_loading = true;
            m_escape->setEnabled(true);
            m_address->setProgress(0);
            applyIcons();
        }
    });
    connect(view, &QWebEngineView::loadProgress, this, [this, view](int progress) {
        if (view == currentView()) {
            m_address->setProgress(progress);
        }
    });
    connect(view, &QWebEngineView::loadFinished, this, [this, view, button](bool) {
        button->setLoading(false);
        if (view == currentView()) {
            m_loading = false;
            m_escape->setEnabled(m_fullScreenView != nullptr || isDownloadBusy());
            m_address->setProgress(-1);
            applyIcons();
            syncToolbar();
        }
        refreshBadges();
    });
    connect(view, &web::WebView::pageMediaChanged, this, [this, view, button](const QJsonObject& media) {
        button->setGlyph(media.value(u"kind"_s).toString().isEmpty() ? u"globe"_s : u"film"_s);
        if (const int index = indexOf(view); index >= 0) {
            m_tabs[index].detectedDismissed = false; // a new report earns the button back
        }
        if (view == currentView()) {
            syncDetected();
        }
    });
    connect(view, &web::WebView::downloadRequested, this,
            [this](const QUrl& url) { beginDownload(m_download, url); });
    connect(view, &web::WebView::newTabRequested, this, [this](const QUrl& url) {
        // "Open link in new tab": behind the current one, as browsers do.
        const int current = currentIndex();
        const int index = newTab();
        m_tabs[index].untouched = false;
        m_tabs.at(index).view->loadUrl(url);
        setCurrentIndex(current);
    });
    connect(view, &web::WebView::zoomChanged, this, &BrowserPage::zoomChanged);
    // The page's alert, confirm and prompt boxes as the app's sheets, never
    // Qt's stock dialogs (DESIGN.md section 4); pop-ups inherit them.
    web::ScriptDialogs dialogs;
    dialogs.alert = [](QWidget* window, const QUrl& origin, const QString& message) {
        MessageSheet::info(window, origin.host().isEmpty() ? tr("This page says") : origin.host(), message);
    };
    dialogs.confirm = [](QWidget* window, const QUrl& origin, const QString& message) {
        return MessageSheet::confirm(window, MessageSheet::Tone::Question,
                                     origin.host().isEmpty() ? tr("This page asks") : origin.host(), message,
                                     tr("OK"), tr("Cancel"));
    };
    dialogs.prompt = [](QWidget* window, const QUrl& origin, const QString& message, const QString& defaultValue,
                        QString* result) {
        bool ok = false;
        const QString text = MessageSheet::askText(window, origin.host().isEmpty() ? tr("This page asks") : origin.host(),
                                                   message, QString(), defaultValue, tr("OK"), &ok);
        if (ok && result != nullptr) {
            *result = text;
        }
        return ok;
    };
    dialogs.authenticate = [](QWidget* window, const QUrl& url, const QString& realm, bool proxy, QString* user,
                              QString* password) {
        // mocks/browser-auth.html: the sheet for a site's name and password.
        const QString host = url.host().isEmpty() ? url.toString() : url.host();
        MessageSheet sheet(window, MessageSheet::Tone::Question,
                           proxy ? tr("The proxy %1 asks for a name and password").arg(host)
                                 : tr("%1 asks for a name and password").arg(host),
                           realm.isEmpty()
                               ? tr("Nothing is stored; the site keeps you signed in for this browsing session.")
                               : tr("The site says: \"%1\". Nothing is stored; the site keeps you signed in for "
                                    "this browsing session.")
                                     .arg(realm));
        QLineEdit* name = sheet.addInput(tr("Name"));
        QLineEdit* secret = sheet.addInput(tr("Password"), QString(), true);
        sheet.addButton(tr("Cancel"));
        sheet.addButton(tr("Sign in"), MessageSheet::Role::Primary);
        if (sheet.run() != 1) {
            return false;
        }
        if (user != nullptr) {
            *user = name->text();
        }
        if (password != nullptr) {
            *password = secret->text();
        }
        return true;
    };
    view->webPage().setScriptDialogs(dialogs);
    connect(view, &web::WebView::permissionPromptRequested, this, &BrowserPage::permissionPromptRequested);
    connect(view, &web::WebView::renderProcessGaveUp, this, &BrowserPage::renderProcessGaveUp);
    connect(view->page(), &QWebEnginePage::windowCloseRequested, this, [this, view] { closeTab(indexOf(view)); });
    connect(view->page(), &QWebEnginePage::fullScreenRequested, this,
            [this, view](QWebEngineFullScreenRequest request) { handleFullScreen(view, request); });
    view->webPage().setTabOpener([this](bool background) { return openTabForPage(background); });
}

QWebEnginePage* BrowserPage::openTabForPage(bool background)
{
    const int current = currentIndex();
    const int index = newTab();
    m_tabs[index].untouched = false;
    if (background && current >= 0) {
        setCurrentIndex(current);
    }
    return m_tabs.at(index).view->page();
}

void BrowserPage::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }
    const bool wasCurrent = index == currentIndex();
    if (m_fullScreenView == m_tabs.at(index).view) {
        exitFullScreen(); // while the tab is still listed, so the view lands back in the stack first
    }
    Tab tab = m_tabs.takeAt(index);
    m_stack->removeWidget(tab.view);
    m_stripLayout->removeWidget(tab.button);
    delete tab.button;
    updateTabDescriptions();
    // Never deleted inside one of its own signal handlers (LESSONS R2); its
    // signals stop reaching this page first (the lambdas hold the deleted
    // button), and the destructor still deletes it before the profile if the
    // page goes before the event loop runs.
    disconnect(tab.view, nullptr, this, nullptr);
    disconnect(tab.view->page(), nullptr, this, nullptr);
    tab.view->hide();
    tab.view->deleteLater();
    m_closing.append(tab.view);
    qCInfo(lcUi) << "browser: closed tab" << index;
    scheduleSessionSave();
    if (m_tabs.isEmpty()) {
        newTab();
        return;
    }
    if (wasCurrent) {
        setCurrentIndex(std::min(index, static_cast<int>(m_tabs.size()) - 1));
    } else {
        syncToolbar();
    }
}

int BrowserPage::currentIndex() const
{
    return indexOf(qobject_cast<web::WebView*>(m_stack->currentWidget()));
}

void BrowserPage::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.size()) {
        return;
    }
    for (int i = 0; i < m_tabs.size(); ++i) {
        m_tabs.at(i).button->setChecked(i == index);
    }
    if (web::WebView* previous = currentView(); previous != nullptr && previous != m_tabs.at(index).view) {
        clearFind(previous);
    }
    m_stack->setCurrentWidget(m_tabs.at(index).view);
    if (m_findBar != nullptr && m_findBar->isVisible()) {
        runFind(false);
    }
    scheduleSessionSave();
    m_loading = m_tabs.at(index).button->isLoading();
    m_address->setProgress(m_loading ? 0 : -1);
    applyIcons();
    syncToolbar();
    syncDetected();
}

void BrowserPage::nextTab()
{
    if (!m_tabs.isEmpty()) {
        setCurrentIndex((currentIndex() + 1) % static_cast<int>(m_tabs.size()));
    }
}

void BrowserPage::previousTab()
{
    if (!m_tabs.isEmpty()) {
        const int count = static_cast<int>(m_tabs.size());
        setCurrentIndex((currentIndex() + count - 1) % count);
    }
}

int BrowserPage::indexOf(const web::WebView* view) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).view == view) {
            return i;
        }
    }
    return -1;
}

int BrowserPage::indexOf(const BrowserTabButton* button) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs.at(i).button == button) {
            return i;
        }
    }
    return -1;
}

web::WebView* BrowserPage::currentView() const
{
    return qobject_cast<web::WebView*>(m_stack->currentWidget());
}

web::WebView* BrowserPage::view(int index) const
{
    return index >= 0 && index < m_tabs.size() ? m_tabs.at(index).view : nullptr;
}

BrowserTabButton* BrowserPage::tabButton(int index) const
{
    return index >= 0 && index < m_tabs.size() ? m_tabs.at(index).button : nullptr;
}

QUrl BrowserPage::currentUrl() const
{
    web::WebView* view = currentView();
    return view != nullptr ? view->url() : QUrl();
}

// ---- toolbar -----------------------------------------------------------------------

void BrowserPage::navigate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    web::WebView* view = currentView();
    if (view == nullptr) {
        return;
    }
    // A word or a phrase is a web search; anything with a dot or a scheme is an address.
    QUrl url = QUrl::fromUserInput(trimmed);
    const bool looksLikeAddress = trimmed.contains(u"://"_s) ||
                                  (!trimmed.contains(u' ') && (trimmed.contains(u'.') || trimmed.contains(u':')));
    if (!looksLikeAddress || !url.isValid()) {
        QUrlQuery query;
        query.addQueryItem(u"q"_s, trimmed);
        url = QUrl(u"https://www.google.com/search"_s);
        url.setQuery(query);
    }
    const int index = indexOf(view);
    if (index >= 0) {
        m_tabs[index].untouched = false;
    }
    view->loadUrl(url);
    view->setFocus();
}

void BrowserPage::syncToolbar()
{
    web::WebView* view = currentView();
    if (view == nullptr) {
        return;
    }
    const QUrl url = view->url();
    if (!m_address->hasFocus() || m_address->text().isEmpty()) {
        const bool blank = url.scheme() == u"data"_s || url.scheme() == u"about"_s || url.isEmpty();
        m_address->setText(blank ? QString() : url.toDisplayString());
        m_address->setCursorPosition(0);
    }
    m_back->setEnabled(view->history()->canGoBack());
    m_forward->setEnabled(view->history()->canGoForward());
    m_reload->setToolTip(m_loading ? tr("Stop (Esc)") : tr("Reload (F5)"));
    m_reload->setAccessibleName(m_loading ? tr("Stop") : tr("Reload"));
    const bool downloadable = url.isValid() && (url.scheme() == u"http"_s || url.scheme() == u"https"_s);
    m_download->setEnabled(downloadable && !busy::isBusy(m_download));
    // A playlist page lands on the Playlist page instead of the queue (FEATURES W3).
    if (!busy::isBusy(m_download)) {
        const bool playlist = core::classifyYouTubeUrl(url).kind == core::YouTubeUrlKind::Playlist;
        m_download->setText(playlist ? tr("Open playlist") : tr("Download this"));
        m_download->setToolTip(playlist ? tr("Open this playlist on the Playlist page (Ctrl+D)")
                                        : tr("Download the page you are looking at (Ctrl+D)"));
    }
}

void BrowserPage::focusAddress()
{
    m_address->setFocus(Qt::ShortcutFocusReason);
    m_address->selectAll();
}

void BrowserPage::reload()
{
    if (web::WebView* view = currentView(); view != nullptr) {
        view->reload();
    }
}

void BrowserPage::back()
{
    if (web::WebView* view = currentView(); view != nullptr) {
        view->back();
    }
}

void BrowserPage::forward()
{
    if (web::WebView* view = currentView(); view != nullptr) {
        view->forward();
    }
}

void BrowserPage::downloadCurrent()
{
    beginDownload(m_download, currentUrl());
}

void BrowserPage::beginDownload(QPushButton* source, const QUrl& url)
{
    if (!url.isValid() || url.isEmpty() || (url.scheme() != u"http"_s && url.scheme() != u"https"_s)) {
        return;
    }
    if (isDownloadBusy()) {
        Q_EMIT cancelDownloadRequested(); // the busy button reads "Click to cancel"
        return;
    }
    qCInfo(lcUi) << "browser: download" << url;
    // The pressed button reacts at once (DESIGN.md section 5); the window
    // calls setDownloadBusy(true) again and later releases it.
    m_busySource = source;
    setDownloadBusy(true);
    Q_EMIT downloadRequested(url);
}

void BrowserPage::setDownloadBusy(bool busy)
{
    QPushButton* source = m_busySource != nullptr ? m_busySource : m_download;
    QPushButton* other = source == m_download ? m_detected : m_download;
    if (busy) {
        busy::set(source, true, tr("Checking"), true);
        other->setEnabled(false);
        m_escape->setEnabled(true);
        return;
    }
    busy::set(m_download, false);
    busy::set(m_detected, false);
    m_detected->setEnabled(true);
    m_busySource = nullptr;
    m_escape->setEnabled(m_loading || m_fullScreenView != nullptr);
    syncToolbar(); // re-enables Download this for a web page
    placeDetected();
}

bool BrowserPage::isDownloadBusy() const
{
    return busy::isBusy(m_download) || busy::isBusy(m_detected);
}

QString BrowserPage::userAgent() const
{
    return m_profile->httpUserAgent();
}

void BrowserPage::resetPermissions()
{
    if (web::WebView* view = currentView(); view != nullptr) {
        view->permissions().resetAll(); // the store is the profile's, one call covers every tab
    }
}

void BrowserPage::answerPermission(const QWebEnginePermission& permission, bool allow, bool remember)
{
    if (web::WebView* view = currentView(); view != nullptr) {
        view->permissions().answer(permission, allow, remember);
    }
}

// ---- badges and the detected button ----------------------------------------------

void BrowserPage::refreshBadges()
{
    const quint64 blocked = m_profile->interceptor().blockedCount();
    const bool showAds = m_settings.blockAds();
    if (showAds) {
        // The badges draw their own glyph; the label text is the words alone,
        // which is also what the accessibility layer reads.
        const QString words = tr("Ads blocked: %L1").arg(static_cast<qulonglong>(blocked));
        m_adsBadge->setText(words);
        m_adsBadge->setAccessibleName(words);
    }
    m_adsBadge->setVisible(showAds);
}

QString BrowserPage::detectedLabel(const QJsonObject& media)
{
    const QString kind = media.value(u"kind"_s).toString();
    if (kind.isEmpty()) {
        return {};
    }
    if (kind == u"audio"_s) {
        return tr("Download detected: audio");
    }
    const int height = media.value(u"height"_s).toInt();
    return height > 0 ? tr("Download detected: %1p video").arg(height) : tr("Download detected: video");
}

void BrowserPage::syncDetected()
{
    web::WebView* view = currentView();
    const QString label = view != nullptr ? detectedLabel(view->pageMedia()) : QString();
    const int index = currentIndex();
    if (label.isEmpty() || m_fullScreenView != nullptr || (index >= 0 && m_tabs.at(index).detectedDismissed)) {
        m_detected->hide();
        m_detectedDismiss->hide();
        return;
    }
    if (busy::isBusy(m_detected)) {
        return; // keeps saying Checking until the link has been looked at
    }
    m_detected->setText(label);
    m_detected->setToolTip(tr("Download the media this page is playing"));
    m_detected->adjustSize();
    placeDetected();
    const bool wasHidden = !m_detected->isVisible();
    m_detected->show();
    m_detected->raise();
    m_detectedDismiss->show();
    m_detectedDismiss->raise();
    if (wasHidden) {
        // Announce the new button to assistive technology (as the toasts do).
        QAccessibleEvent event(m_detected, QAccessible::Alert);
        QAccessible::updateAccessibility(&event);
    }
}

void BrowserPage::placeDetected()
{
    QWidget* stage = m_detected->parentWidget();
    if (stage == nullptr) {
        return;
    }
    const QSize size = m_detected->sizeHint();
    m_detected->resize(size);
    m_detected->move(stage->width() - size.width() - kDetectedInset, stage->height() - size.height() - kDetectedInset);
    // The × sits over the pill's top right corner.
    m_detectedDismiss->move(m_detected->geometry().right() - 14, m_detected->geometry().top() - 14);
}

// ---- full screen -------------------------------------------------------------------

void BrowserPage::handleFullScreen(web::WebView* view, QWebEngineFullScreenRequest request)
{
    request.accept();
    if (request.toggleOn()) {
        enterFullScreen(view);
    } else {
        exitFullScreen();
    }
}

void BrowserPage::enterFullScreen(web::WebView* view)
{
    if (view == nullptr || m_fullScreenView != nullptr) {
        return;
    }
    // The view stays where it is (moving it to another window leaves it
    // blank, owner bug); everything around it goes instead: the strip, the
    // toolbar, the load bar here, the rail and the pane in the window.
    m_fullScreenView = view;
    setCurrentIndex(indexOf(view));
    m_strip->hide();
    headerLayout()->parentWidget()->hide();
    hideFind();
    m_address->setProgress(-1);
    m_detected->hide();
    m_detectedDismiss->hide();
    m_escape->setEnabled(true);
    Q_EMIT fullScreenChanged(true);
    if (m_fullScreenHint == nullptr) {
        m_fullScreenHint = new web::FullScreenHint(this);
    }
    view->setFocus();
    m_fullScreenHint->showHint(tr("Press Esc to exit full screen"));
}

void BrowserPage::exitFullScreen()
{
    if (m_fullScreenView == nullptr) {
        return;
    }
    web::WebView* view = m_fullScreenView;
    m_fullScreenView = nullptr;
    if (m_fullScreenHint != nullptr) {
        m_fullScreenHint->hideHint();
    }
    m_strip->show();
    headerLayout()->parentWidget()->show();
    m_address->setProgress(m_loading ? 0 : -1);
    view->page()->triggerAction(QWebEnginePage::ExitFullScreen);
    Q_EMIT fullScreenChanged(false);
    syncDetected();
    m_escape->setEnabled(m_loading || isDownloadBusy());
}

// ---- events ------------------------------------------------------------------------

void BrowserPage::showEvent(QShowEvent* event)
{
    Page::showEvent(event);
    m_badgeTimer->start();
    refreshBadges();
    placeDetected();
    // Where the keyboard lands (DESIGN.md section 5): the address bar on a
    // fresh tab, the page otherwise. Deferred a turn: the page stack moves
    // focus to the first control of the new page (the + button) right after
    // showing it, and this must win over that.
    QTimer::singleShot(0, this, [this] {
        const int index = currentIndex();
        if (!isVisible() || index < 0) {
            return;
        }
        QWidget* focus = QApplication::focusWidget();
        if (focus == m_address || (focus != nullptr && focus != m_newTab && isAncestorOf(focus) &&
                                   qobject_cast<QAbstractButton*>(focus) == nullptr)) {
            return; // the user already put it somewhere meaningful
        }
        if (m_tabs.at(index).untouched) {
            focusAddress();
        } else if (web::WebView* view = currentView(); view != nullptr) {
            view->setFocus(Qt::OtherFocusReason);
        }
    });
}

void BrowserPage::hideEvent(QHideEvent* event)
{
    Page::hideEvent(event);
    m_badgeTimer->stop();
}

void BrowserPage::resizeEvent(QResizeEvent* event)
{
    Page::resizeEvent(event);
    placeDetected();
}

bool BrowserPage::eventFilter(QObject* watched, QEvent* event)
{
    if (m_detected != nullptr && watched == m_detected->parentWidget() && event->type() == QEvent::Resize) {
        placeDetected();
    } else if (watched == m_findField && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Escape) {
            hideFind();
            return true;
        }
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers().testFlag(Qt::ShiftModifier)) {
            findPrevious();
            return true;
        }
    } else if (watched == m_address && event->type() == QEvent::KeyPress &&
               static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        // Esc in the address bar: what was typed goes, the page's address
        // comes back and the page has the keyboard again.
        m_address->clearFocus();
        syncToolbar();
        if (web::WebView* view = currentView(); view != nullptr) {
            view->setFocus(Qt::OtherFocusReason);
        }
        return true;
    }
    return Page::eventFilter(watched, event);
}

} // namespace pldl::ui
