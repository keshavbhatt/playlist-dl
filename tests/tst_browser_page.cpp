// The Browser page (mocks/browser.html, FEATURES B1, B2, B4) offscreen: tabs
// open, switch and close on one shared profile, the toolbar follows the
// current tab, Download this and the detected-media button hand the right
// link to the add flow, and the badges read the interceptor and the sign-in.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/browser_tab.h"
#include "ui/pages/browser_page.h"
#include "web/error_page.h"
#include "web/request_interceptor.h"
#include "web/script_dialogs.h"
#include "web/web_page.h"
#include "web/web_profile.h"
#include "web/web_view.h"

#include <QApplication>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QToolButton>
#include <QDir>
#include <QCheckBox>
#include <QFile>
#include <QFrame>

using namespace Qt::StringLiterals;

/// Answers every request with a small attachment, the way a "Download now"
/// link does; the engine turns that into a download request.
class AttachmentServer : public QObject
{
    Q_OBJECT

public:
    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [socket] {
                    if (!socket->readAll().contains("\r\n\r\n")) {
                        return;
                    }
                    const QByteArray body("0123456789");
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n"
                                  "Content-Disposition: attachment; filename=\"file.bin\"\r\n"
                                  "Content-Length: 10\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    [[nodiscard]] QUrl url() const
    {
        return QUrl(u"http://127.0.0.1:"_s + QString::number(m_server.serverPort()) + u"/file.bin"_s);
    }

private:
    QTcpServer m_server;
};

/// Answers 401 with a Basic challenge until an Authorization header arrives.
class AuthServer : public QObject
{
    Q_OBJECT

public:
    QString authorizationSeen;
    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    const QByteArray request = socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        return;
                    }
                    const qsizetype at = request.indexOf("Authorization: ");
                    if (at >= 0) {
                        authorizationSeen = QString::fromLatin1(request.mid(at + 15, request.indexOf("\r\n", at) - at - 15));
                        const QByteArray body("<html><body>welcome</body></html>");
                        socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
                                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    } else {
                        socket->write("HTTP/1.1 401 Unauthorized\r\nWWW-Authenticate: Basic realm=\"Private downloads\"\r\n"
                                      "Content-Length: 0\r\nConnection: close\r\n\r\n");
                    }
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    [[nodiscard]] QUrl url() const
    {
        return QUrl(u"http://127.0.0.1:"_s + QString::number(m_server.serverPort()) + u"/private/"_s);
    }

private:
    QTcpServer m_server;
};

class TestBrowserPage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<pldl::core::Settings>(m_dir->filePath(u"browser.ini"_s));
        // No network in the tests: the start page is a local document.
        m_settings->setBrowserStartPage(u"about:blank"_s);
        m_theme = std::make_unique<pldl::core::ThemeService>(*m_settings);
    }

    void cleanup()
    {
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void startsWithOneTabOnTheStartPage()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QCOMPARE(page.tabCount(), 1);
        QCOMPARE(page.currentIndex(), 0);
        QVERIFY(page.tabButton(0)->isChecked());
        QVERIFY(page.currentView() != nullptr);
        QCOMPARE(page.currentView()->page()->profile(), &page.profile());
        // Toolbar controls carry names for assistive technology.
        for (QToolButton* button : page.findChildren<QToolButton*>()) {
            if (QString::fromLatin1(button->metaObject()->className()) == u"QLineEditIconButton"_s) {
                continue; // Qt's own clear button, named by the field
            }
            QVERIFY2(!button->accessibleName().isEmpty() || !button->toolTip().isEmpty(),
                     qPrintable(button->objectName()));
        }
        QVERIFY(!page.addressField()->placeholderText().isEmpty());
    }

    void tabsOpenSwitchAndCloseOnOneProfile()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        const int second = page.newTab();
        QCOMPARE(second, 1);
        QCOMPARE(page.tabCount(), 2);
        QCOMPARE(page.currentIndex(), 1);
        QVERIFY(page.tabButton(1)->isChecked());
        QVERIFY(!page.tabButton(0)->isChecked());
        QCOMPARE(page.view(0)->page()->profile(), page.view(1)->page()->profile());

        page.tabButton(0)->click();
        QCOMPARE(page.currentIndex(), 0);
        page.nextTab();
        QCOMPARE(page.currentIndex(), 1);
        page.previousTab();
        QCOMPARE(page.currentIndex(), 0);

        page.tabButton(1)->closeButton()->click();
        QCOMPARE(page.tabCount(), 1);
        QCOMPARE(page.currentIndex(), 0);
        // Closing the last tab leaves a fresh one, never an empty page.
        page.closeCurrentTab();
        QCOMPARE(page.tabCount(), 1);
        QVERIFY(page.currentView() != nullptr);
        QTest::qWait(50); // the closed views' deleteLater runs
    }

    void openReusesAnUntouchedTabAndThenAddsOne()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        const QUrl first(u"data:text/html,<title>First</title>"_s);
        QCOMPARE(page.open(first), 0); // the blank start tab is reused
        QCOMPARE(page.tabCount(), 1);
        const QUrl second(u"data:text/html,<title>Second</title>"_s);
        QCOMPARE(page.open(second), 1); // a used tab is kept, a new one opens
        QCOMPARE(page.tabCount(), 2);
        QTRY_COMPARE_WITH_TIMEOUT(page.tabButton(1)->title(), u"Second"_s, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(page.tabButton(0)->title(), u"First"_s, 5000);
    }

    void downloadThisHandsOverTheCurrentPage()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QSignalSpy spy(&page, &pldl::ui::BrowserPage::downloadRequested);
        auto* button = page.findChild<QPushButton*>(u"downloadThisButton"_s);
        QVERIFY(button != nullptr);
        // about:blank is nothing to download: the button waits for a web page.
        page.downloadCurrent();
        QCOMPARE(spy.count(), 0);
        page.open(QUrl(u"https://example.com/watch?v=1"_s));
        // The URL is known to the view as soon as the load is requested.
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().host() == u"example.com"_s, 15000);
        page.downloadCurrent();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toUrl(), QUrl(u"https://example.com/watch?v=1"_s));
        page.setDownloadBusy(true);
        QVERIFY(page.isDownloadBusy());
        QVERIFY(button->isEnabled()); // a busy button is the cancel
        QCOMPARE(button->text(), u"Checking"_s);
        page.setDownloadBusy(false);
        QVERIFY(!page.isDownloadBusy());
    }

    void detectedMediaShowsTheFloatingButton()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QPushButton* detected = page.detectedButton();
        QVERIFY(!detected->isVisible());
        page.open(QUrl(u"https://example.com/clip"_s));
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().host() == u"example.com"_s, 15000);

        page.currentView()->setPageMedia(QJsonObject{{u"kind"_s, u"video"_s}, {u"height"_s, 1080}});
        QVERIFY(detected->isVisible());
        QCOMPARE(detected->text(), u"Download detected: 1080p video"_s);
        QCOMPARE(page.tabButton(page.currentIndex())->isChecked(), true);
        // Sits in the bottom right corner over the page.
        QWidget* stage = detected->parentWidget();
        QVERIFY(detected->geometry().right() < stage->width());
        QVERIFY(detected->geometry().bottom() < stage->height());
        QVERIFY(detected->geometry().right() > stage->width() / 2);

        // A direct file wins over the page link; the page link otherwise.
        QSignalSpy spy(&page, &pldl::ui::BrowserPage::downloadRequested);
        detected->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toUrl().host(), u"example.com"_s);
        page.setDownloadBusy(false);
        page.currentView()->setPageMedia(QJsonObject{
            {u"kind"_s, u"audio"_s}, {u"direct"_s, u"https://cdn.example.net/track.mp3"_s}});
        QCOMPARE(detected->text(), u"Download detected: audio"_s);
        detected->click();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toUrl(), QUrl(u"https://cdn.example.net/track.mp3"_s));
        page.setDownloadBusy(false);

        // Another tab in front hides it; back to this one shows it again.
        page.newTab();
        QVERIFY(!detected->isVisible());
        page.setCurrentIndex(0);
        QVERIFY(detected->isVisible());
        page.currentView()->setPageMedia({});
        QVERIFY(!detected->isVisible());
    }

    void arrowKeysHopBetweenTabsAndTabsAnnounceTheirPlace()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.newTab();
        page.newTab();
        QCOMPARE(page.tabButton(0)->accessibleDescription(), u"Tab 1 of 3"_s);
        QCOMPARE(page.tabButton(2)->accessibleDescription(), u"Tab 3 of 3"_s);
        page.tabButton(0)->setFocus(Qt::TabFocusReason);
        QTRY_VERIFY(page.tabButton(0)->hasFocus());
        QTest::keyClick(page.tabButton(0), Qt::Key_Right);
        QVERIFY2(page.tabButton(1)->hasFocus(), "Right moves to the next tab, not its close button");
        QTest::keyClick(page.tabButton(1), Qt::Key_End);
        QVERIFY(page.tabButton(2)->hasFocus());
        QTest::keyClick(page.tabButton(2), Qt::Key_Right);
        QVERIFY2(QApplication::focusWidget()->accessibleName() == u"New tab"_s, "the ring wraps through the + button");
        QTest::keyClick(QApplication::focusWidget(), Qt::Key_Right);
        QVERIFY(page.tabButton(0)->hasFocus());
        page.closeTab(2);
        QCOMPARE(page.tabButton(1)->accessibleDescription(), u"Tab 2 of 2"_s);
        // Hit targets (DESIGN.md section 5): the strip's controls are at least 28 px inside a 34 px row.
        QCOMPARE(page.tabButton(0)->closeButton()->size(), QSize(28, 28));
        QCOMPARE(page.findChild<QToolButton*>(u""_s, Qt::FindDirectChildrenOnly) != nullptr, false);
        for (QToolButton* button : page.findChildren<QToolButton*>()) {
            if (button->accessibleName() == u"New tab"_s) {
                QCOMPARE(button->size(), QSize(34, 34));
            }
        }
        QTest::qWait(50);
    }

    void thePressedButtonShowsChecking()
    {
        // Owner feedback: clicking the detected button left it as it was while
        // Download this in the toolbar spun. The pressed one is the busy one.
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.open(QUrl(u"https://example.com/clip"_s));
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().host() == u"example.com"_s, 15000);
        page.currentView()->setPageMedia(QJsonObject{{u"kind"_s, u"video"_s}, {u"height"_s, 720}});
        QSignalSpy spy(&page, &pldl::ui::BrowserPage::downloadRequested);
        page.detectedButton()->click();
        QCOMPARE(spy.count(), 1);
        QVERIFY(page.isDownloadBusy());
        QCOMPARE(page.detectedButton()->text(), u"Checking"_s);
        auto* downloadThis = page.findChild<QPushButton*>(u"downloadThisButton"_s);
        QVERIFY(!downloadThis->isEnabled());
        QCOMPARE(downloadThis->text(), u"Download this"_s);
        // While checking, the busy button is a cancel: it stays enabled, says
        // so, and a press asks the window to stop the check.
        QVERIFY(page.detectedButton()->isEnabled());
        QVERIFY(page.detectedButton()->toolTip().contains(u"cancel"_s));
        QSignalSpy cancels(&page, &pldl::ui::BrowserPage::cancelDownloadRequested);
        page.detectedButton()->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(cancels.count(), 1);
        page.setDownloadBusy(false);
        QVERIFY(!page.isDownloadBusy());
        QCOMPARE(page.detectedButton()->text(), u"Download detected: 720p video"_s);
        QVERIFY(page.detectedButton()->isEnabled());
        QVERIFY(downloadThis->isEnabled());
        // And the other way round.
        downloadThis->click();
        QCOMPARE(spy.count(), 2);
        QCOMPARE(downloadThis->text(), u"Checking"_s);
        QVERIFY(!page.detectedButton()->isEnabled());
        page.setDownloadBusy(false);
    }

    void escapeInTheAddressBarRestoresTheAddress()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.activateWindow();
        page.open(QUrl(u"https://example.com/watch"_s));
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().host() == u"example.com"_s, 15000);
        page.focusAddress();
        QTRY_VERIFY(page.addressField()->hasFocus());
        page.addressField()->setText(u"something else"_s);
        QTest::keyClick(page.addressField(), Qt::Key_Escape);
        QVERIFY(page.addressField()->text().contains(u"example.com/watch"_s));
        QVERIFY(!page.addressField()->hasFocus());
    }

    void contextMenuOffersTheDownloadRoutes()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        pldl::web::WebView* view = page.currentView();
        pldl::web::WebView::ContextInfo info;
        info.pageUrl = QUrl(u"https://example.com/page"_s);
        info.linkUrl = QUrl(u"https://example.com/file.zip"_s);
        info.mediaUrl = QUrl(u"https://cdn.example.net/poster.jpg"_s);
        info.isImage = true;
        info.selectedText = u"quote"_s;
        QMenu* menu = view->buildContextMenu(info);
        QStringList texts;
        for (QAction* action : menu->actions()) {
            if (!action->isSeparator()) {
                texts << action->text();
            }
        }
        QVERIFY(texts.contains(u"Open link in new tab"_s));
        QVERIFY(texts.contains(u"Download link"_s));
        QVERIFY(texts.contains(u"Copy link address"_s));
        QVERIFY(texts.contains(u"Download image"_s));
        QVERIFY(texts.contains(u"Download this page"_s));
        QVERIFY2(!texts.join(u' ').contains(u"Inspect"_s) && !texts.join(u' ').contains(u"source"_s),
                 "no developer items");
        QSignalSpy downloads(&page, &pldl::ui::BrowserPage::downloadRequested);
        for (QAction* action : menu->actions()) {
            if (action->text() == u"Download link"_s) {
                action->trigger();
            }
        }
        QCOMPARE(downloads.count(), 1);
        QCOMPARE(downloads.at(0).at(0).toUrl(), info.linkUrl);
        page.setDownloadBusy(false);
        // A plain page: only navigation and Download this page.
        pldl::web::WebView::ContextInfo plain;
        plain.pageUrl = info.pageUrl;
        QMenu* plainMenu = view->buildContextMenu(plain);
        int count = 0;
        for (QAction* action : plainMenu->actions()) {
            count += action->isSeparator() ? 0 : 1;
        }
        QCOMPARE(count, 4);
        delete menu;
        delete plainMenu;
    }

    void errorPageUsesTheAppColoursAndRetriesTheSameLink()
    {
        pldl::web::ErrorPageStyle style;
        style.accent = QColor(0x2F, 0x6F, 0xE4);
        const QString html = pldl::web::errorPageHtml(style, u"Cannot load"_s, u"<offline>"_s,
                                                     QUrl(u"https://vimeo.com/a?b=1&c=2"_s));
        QVERIFY(html.contains(u"#2f6fe4"_s));
        QVERIFY(!html.contains(u"#ff0000"_s));
        QVERIFY(html.contains(u"href=\"https://vimeo.com/a?b=1&amp;c=2\""_s));
        QVERIFY(html.contains(u"&lt;offline&gt;"_s));
        QVERIFY(!html.contains(u"youtube.com"_s));
    }

    void anEmptyStartPageIsANewTab()
    {
        m_settings->setBrowserStartPage(QString(pldl::core::Settings::kEmptyStartPage));
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QTest::qWait(300); // the engine reports the about:blank title
        QCOMPARE(page.tabButton(0)->title(), u"New tab"_s);
        QVERIFY(page.addressField()->text().isEmpty());
        QVERIFY(!page.findChild<QPushButton*>(u"downloadThisButton"_s)->isEnabled());
    }

    void aPageDownloadLinkReachesTheQueue()
    {
        // Owner report: "Download now" on a page did nothing. The engine's
        // own download is cancelled and the link goes to the add flow.
        AttachmentServer server;
        QVERIFY(server.listen());
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QSignalSpy downloads(&page, &pldl::ui::BrowserPage::downloadRequested);
        page.open(server.url()); // navigating to an attachment is what a link click does
        QTRY_COMPARE_WITH_TIMEOUT(downloads.count(), 1, 10000);
        QCOMPARE(downloads.at(0).at(0).toUrl(), server.url());
        QVERIFY(page.isDownloadBusy()); // Download this says Checking meanwhile
        page.setDownloadBusy(false);
    }

    void aPageMadeFileIsSavedByTheEngine()
    {
        QTemporaryDir folder;
        m_settings->setDownloadDirectory(folder.path());
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QSignalSpy saved(&page, &pldl::ui::BrowserPage::engineFileSaved);
        QSignalSpy queued(&page, &pldl::ui::BrowserPage::downloadRequested);
        page.currentView()->page()->download(QUrl(u"data:application/octet-stream;base64,SGVsbG8="_s),
                                             u"made-by-page.bin"_s);
        QTRY_COMPARE_WITH_TIMEOUT(saved.count(), 1, 10000);
        const QString path = saved.at(0).at(0).toString();
        QVERIFY2(path.startsWith(folder.path()), qPrintable(path));
        QVERIFY(QFile::exists(path));
        QCOMPARE(queued.count(), 0);
    }

    void theFloatingButtonCanBePutAway()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.open(QUrl(u"https://example.com/clip"_s));
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().host() == u"example.com"_s, 15000);
        page.currentView()->setPageMedia(QJsonObject{{u"kind"_s, u"video"_s}, {u"height"_s, 720}});
        QVERIFY(page.detectedButton()->isVisible());
        QVERIFY(page.detectedDismissButton()->isVisible());
        page.detectedDismissButton()->click();
        QVERIFY(!page.detectedButton()->isVisible());
        QVERIFY(!page.detectedDismissButton()->isVisible());
        // The same report stays hidden; a new one brings the button back.
        page.newTab();
        page.setCurrentIndex(0);
        QVERIFY(!page.detectedButton()->isVisible());
        page.currentView()->setPageMedia(QJsonObject{{u"kind"_s, u"video"_s}, {u"height"_s, 1080}});
        QVERIFY(page.detectedButton()->isVisible());
    }

    void openTabsComeBackOnTheNextStartWhenAskedFor()
    {
        m_settings->setRestoreBrowserTabs(true);
        {
            pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
            page.show();
            QVERIFY(QTest::qWaitForWindowExposed(&page));
            page.open(QUrl(u"https://example.com/one"_s));
            page.open(QUrl(u"https://example.org/two"_s));
            QTRY_VERIFY_WITH_TIMEOUT(page.view(1)->url().host() == u"example.org"_s, 15000);
            QTRY_VERIFY_WITH_TIMEOUT(page.view(0)->url().host() == u"example.com"_s, 15000);
            page.setCurrentIndex(0);
            QCOMPARE(page.tabCount(), 2);
        } // the session is written at teardown
        const pldl::core::Settings::BrowserSession saved = m_settings->browserSession();
        QCOMPARE(saved.urls.size(), 2);
        QCOMPARE(saved.urls.at(0), u"https://example.com/one"_s);
        QCOMPARE(saved.urls.at(1), u"https://example.org/two"_s);
        QCOMPARE(saved.current, 0);

        pldl::ui::BrowserPage again(*m_settings, *m_theme, u"7.0.0-test"_s);
        again.show();
        QVERIFY(QTest::qWaitForWindowExposed(&again));
        QCOMPARE(again.tabCount(), 2);
        QCOMPARE(again.currentIndex(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(again.view(1)->url().host() == u"example.org"_s, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(again.view(0)->url().host() == u"example.com"_s, 15000);
    }

    void aStartLeavesTheSavedTabsAloneByDefault()
    {
        m_settings->setBrowserStartPage(QString(pldl::core::Settings::kEmptyStartPage));
        m_settings->setBrowserSession({{u"https://example.com/one"_s, u"https://example.org/two"_s}, 1});
        QVERIFY(!m_settings->restoreBrowserTabs());
        pldl::ui::BrowserPage fresh(*m_settings, *m_theme, u"7.0.0-test"_s);
        fresh.show();
        QVERIFY(QTest::qWaitForWindowExposed(&fresh));
        QCOMPARE(fresh.tabCount(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(fresh.currentUrl().toString() == u"about:blank"_s, 5000);
    }

    void fullScreenHidesTheChromeAndKeepsTheView()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QSignalSpy spy(&page, &pldl::ui::BrowserPage::fullScreenChanged);
        pldl::web::WebView* view = page.currentView();
        auto* strip = page.findChild<QFrame*>(u"tabStrip"_s);
        page.debugEnterFullScreen();
        QVERIFY(page.isInFullScreen());
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());
        QVERIFY(!strip->isVisible());
        QVERIFY(!page.addressField()->isVisible());
        QVERIFY(view->isVisible());                 // the view never moved
        QCOMPARE(view->parentWidget()->window(), &page);
        QTest::keyClick(&page, Qt::Key_Escape);
        QVERIFY(!page.isInFullScreen());
        QCOMPARE(spy.count(), 2);
        QVERIFY(!spy.at(1).at(0).toBool());
        QVERIFY(strip->isVisible());
        QVERIFY(page.addressField()->isVisible());
    }

    void findInPageCountsAndStepsThroughMatches()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1000, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        page.open(QUrl(u"data:text/html,<p>public domain</p><p>Public Domain</p><p>public domain</p>"_s));
        QTRY_VERIFY_WITH_TIMEOUT(page.currentUrl().scheme() == u"data"_s, 5000);
        QTest::qWait(300); // the document renders
        QVERIFY(!page.isFindVisible());
        page.showFind();
        QVERIFY(page.isFindVisible());
        QVERIFY(page.findField()->hasFocus());
        page.findField()->setText(u"public domain"_s);
        QTRY_COMPARE_WITH_TIMEOUT(page.findCount()->text(), u"1 of 3"_s, 5000);
        page.findNext();
        QTRY_COMPARE_WITH_TIMEOUT(page.findCount()->text(), u"2 of 3"_s, 5000);
        page.findPrevious();
        QTRY_COMPARE_WITH_TIMEOUT(page.findCount()->text(), u"1 of 3"_s, 5000);
        auto* matchCase = page.findChild<QCheckBox*>();
        QVERIFY(matchCase != nullptr);
        matchCase->setChecked(true);
        QTRY_VERIFY_WITH_TIMEOUT(page.findCount()->text().endsWith(u"of 2"_s), 5000); // the engine keeps the nearest match active
        page.findField()->setText(u"nothing here"_s);
        QTRY_COMPARE_WITH_TIMEOUT(page.findCount()->text(), u"No matches"_s, 5000);
        QVERIFY(page.findField()->property("pldlNoMatch").toBool());
        QTest::keyClick(page.findField(), Qt::Key_Escape);
        QVERIFY(!page.isFindVisible());
        QVERIFY(page.findCount()->text().isEmpty());
    }

    void aSiteAskingForANameAndPasswordGetsThemFromTheSheet()
    {
        AuthServer server;
        QVERIFY(server.listen());
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        // The page's own sheet is modal: the test answers in its place.
        pldl::web::ScriptDialogs dialogs;
        QString realmSeen;
        QString hostSeen;
        bool proxySeen = true;
        dialogs.authenticate = [&](QWidget*, const QUrl& url, const QString& realm, bool proxy, QString* user,
                                   QString* password) -> bool {
            realmSeen = realm;
            hostSeen = url.host();
            proxySeen = proxy;
            *user = u"keshav"_s;
            *password = u"secret"_s;
            return true;
        };
        page.currentView()->webPage().setScriptDialogs(dialogs);
        page.open(server.url());
        QTRY_VERIFY_WITH_TIMEOUT(!server.authorizationSeen.isEmpty(), 10000);
        QCOMPARE(server.authorizationSeen, u"Basic "_s + QString::fromLatin1(QByteArray("keshav:secret").toBase64()));
        QCOMPARE(realmSeen, u"Private downloads"_s);
        QCOMPARE(hostSeen, u"127.0.0.1"_s);
        QVERIFY(!proxySeen);
    }

    void detectedLabels()
    {
        using pldl::ui::BrowserPage;
        QCOMPARE(BrowserPage::detectedLabel({}), QString());
        QCOMPARE(BrowserPage::detectedLabel(QJsonObject{{u"kind"_s, u"video"_s}}), u"Download detected: video"_s);
        QCOMPARE(BrowserPage::detectedLabel(QJsonObject{{u"kind"_s, u"video"_s}, {u"height"_s, 720}}),
                 u"Download detected: 720p video"_s);
        QCOMPARE(BrowserPage::detectedLabel(QJsonObject{{u"kind"_s, u"audio"_s}}), u"Download detected: audio"_s);
    }

    void badgesFollowTheBlockerAndTheSignIn()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QLabel* ads = page.adsBadge();
        QVERIFY(ads->isVisible());
        QVERIFY(ads->text().contains(u"Ads blocked: 0"_s));
        page.profile().interceptor().resetBlockedCount(12);
        page.refreshBadges();
        QVERIFY(ads->text().contains(u"Ads blocked: 12"_s));
        QCOMPARE(ads->accessibleName(), u"Ads blocked: 12"_s);
        m_settings->setBlockAds(false);
        QVERIFY(!ads->isVisible());
        m_settings->setBlockAds(true);
        QVERIFY(ads->isVisible());
        // No YouTube session cookie: no Signed in badge.
        QVERIFY(!page.signedInBadge()->isVisible());
    }

    void tabButtonElidesAndKeepsItsNames()
    {
        pldl::ui::BrowserTabButton button(*m_theme);
        button.setTitle(u"How the James Webb telescope sees the first galaxies, explained"_s);
        QVERIFY(button.sizeHint().width() <= pldl::ui::BrowserTabButton::kMaxWidth);
        QVERIFY(button.minimumSizeHint().width() >= pldl::ui::BrowserTabButton::kCompactWidth);
        QCOMPARE(button.accessibleName(), button.title());
        QVERIFY(button.closeButton()->accessibleName().contains(u"James Webb"_s));
        QSignalSpy closed(&button, &pldl::ui::BrowserTabButton::closeRequested);
        button.closeButton()->click();
        QCOMPARE(closed.count(), 1);
        button.setTitle(QString());
        QCOMPARE(button.title(), u"New tab"_s);
        // Many tabs: a tab can shrink to its glyph so the strip never overflows.
        QCOMPARE(button.minimumSizeHint().width(), pldl::ui::BrowserTabButton::kCompactWidth);
        QVERIFY(pldl::ui::BrowserTabButton::kCompactWidth < pldl::ui::BrowserTabButton::kMinWidth);
    }

    void manyTabsStayInsideTheStrip()
    {
        pldl::ui::BrowserPage page(*m_settings, *m_theme, u"7.0.0-test"_s);
        page.resize(1024, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        for (int i = 0; i < 24; ++i) {
            page.newTab();
        }
        QTest::qWait(50);
        QCOMPARE(page.tabCount(), 25);
        auto* strip = page.findChild<QFrame*>(u"tabStrip"_s);
        QVERIFY(strip != nullptr);
        QVERIFY2(strip->width() <= page.width(), qPrintable(QString::number(strip->width())));
        for (int i = 0; i < page.tabCount(); ++i) {
            const QRect r = page.tabButton(i)->geometry();
            QVERIFY2(r.right() <= strip->width(), qPrintable(u"tab %1 ends at %2"_s.arg(i).arg(r.right())));
            QVERIFY(r.width() >= pldl::ui::BrowserTabButton::kCompactWidth);
        }
        QTest::qWait(50);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
};

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-browser-page"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestBrowserPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_browser_page.moc"
