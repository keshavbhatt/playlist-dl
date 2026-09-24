#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "web/popup_window.h"
#include "web/web_profile.h"
#include "web/web_view.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

// A pop-up page (a sign-in window) is a child of the view and must be gone
// before the profile it was created from; destroying the view with a pop-up
// still open used to delete them in the wrong order.
class TestWebViewLifetime : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void popupsGoBeforeTheProfile()
    {
        QTemporaryDir dir;
        pldl::core::Settings settings(dir.filePath(u"lifetime.ini"_s));
        pldl::core::ThemeService theme(settings);
        pldl::web::WebProfile profile(settings, u"7.0.0"_s);
        auto* view = new pldl::web::WebView(profile, theme);
        auto* popup = new pldl::web::PopupWindow(view->profile(), view);
        popup->show();
        QVERIFY(view->findChildren<pldl::web::PopupWindow*>().contains(popup));
        delete view; // no use after the profile, no crash
        QVERIFY(true);
    }

    void identityFollowsTheSetting()
    {
        QTemporaryDir dir;
        pldl::core::Settings settings(dir.filePath(u"identity.ini"_s));
        pldl::web::WebProfile profile(settings, u"7.0.0"_s);
        const QString engineIdentity = profile.httpUserAgent();
        QVERIFY(!engineIdentity.contains(u"QtWebEngine"_s));
        settings.setBrowserUserAgentPreset(u"firefox-linux"_s);
        QVERIFY(profile.httpUserAgent().contains(u"Firefox/"_s)); // live, no restart
        settings.setBrowserUserAgentPreset(u"custom"_s);
        settings.setBrowserUserAgent(u"Mine/1.0"_s);
        QCOMPARE(profile.httpUserAgent(), u"Mine/1.0"_s);
        settings.setBrowserUserAgentPreset(u"default"_s);
        QCOMPARE(profile.httpUserAgent(), engineIdentity);
    }

    void twoViewsShareOneProfile()
    {
        QTemporaryDir dir;
        pldl::core::Settings settings(dir.filePath(u"shared.ini"_s));
        pldl::core::ThemeService theme(settings);
        pldl::web::WebProfile profile(settings, u"7.0.0"_s);
        auto* a = new pldl::web::WebView(profile, theme);
        auto* b = new pldl::web::WebView(profile, theme);
        QCOMPARE(a->page()->profile(), &profile);
        QCOMPARE(b->page()->profile(), &profile);
        QVERIFY(&a->bridge() != &b->bridge()); // reports are per tab
        delete a;
        delete b;
    }
};

QTEST_MAIN(TestWebViewLifetime)
#include "tst_web_view_lifetime.moc"
