#include "core/navigation_policy.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;

class TestNavigationPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyWebPageStaysInTheApp()
    {
        // The built-in browser is general purpose: a SoundCloud sign-in page's
        // links (terms, help, the site itself) open in it, not on the desktop.
        QVERIFY(!shouldOpenExternally(QUrl(u"https://www.youtube.com/watch?v=x"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://accounts.google.com/signin"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://soundcloud.com/signin"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://secure.soundcloud.com/connect"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://example.com/"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"http://example.com/"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"about:blank"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"data:text/html,<p>x</p>"_s)));
        QVERIFY(!shouldOpenExternally(QUrl()));
    }

    void onlyDesktopSchemesLeave()
    {
        QVERIFY(shouldOpenExternally(QUrl(u"mailto:someone@example.com"_s)));
        QVERIFY(shouldOpenExternally(QUrl(u"tel:+1555"_s)));
        QVERIFY(shouldOpenExternally(QUrl(u"magnet:?xt=urn:btih:abc"_s)));
        QVERIFY(shouldOpenExternally(QUrl(u"MAILTO:x@example.com"_s)));
        // Never handed to the desktop: a page must not open files or arbitrary handlers.
        QVERIFY(!shouldOpenExternally(QUrl(u"file:///etc/passwd"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"ftp://example.com/x"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"steam://run/1"_s)));
    }

    void popups()
    {
        QVERIFY(isInAppPopupUrl(QUrl()));
        QVERIFY(isInAppPopupUrl(QUrl(u"https://accounts.google.com/o/oauth2"_s)));
        QVERIFY(isInAppPopupUrl(QUrl(u"https://soundcloud.com/connect"_s)));
        QVERIFY(isInAppPopupUrl(QUrl(u"https://example.com"_s)));
        QVERIFY(!isInAppPopupUrl(QUrl(u"mailto:x@example.com"_s)));
    }

    void redirectUnwrap()
    {
        const QUrl wrapped(
            u"https://www.youtube.com/redirect?event=video_description&q=https%3A%2F%2Fexample.com%2Fpage"_s);
        QCOMPARE(unwrapRedirect(wrapped), QUrl(u"https://example.com/page"_s));
        QCOMPARE(unwrapRedirect(QUrl(u"https://example.com/"_s)), QUrl(u"https://example.com/"_s));
    }
};

QTEST_GUILESS_MAIN(TestNavigationPolicy)
#include "tst_navigation_policy.moc"
