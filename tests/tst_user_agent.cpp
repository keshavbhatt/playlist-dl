#include "web/user_agent.h"

#include <QTest>

using namespace Qt::StringLiterals;
using pldl::web::effectiveUserAgent;
using pldl::web::sanitizeUserAgent;

class TestUserAgent : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stripsQtWebEngineToken()
    {
        const QString in = u"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                           "QtWebEngine/6.11.1 Chrome/130.0.6723.192 Safari/537.36"_s;
        const QString out = sanitizeUserAgent(in);
        QVERIFY(!out.contains(u"QtWebEngine"_s));
        QVERIFY(out.contains(u"Chrome/130.0.6723.192"_s));
        QVERIFY(!out.contains(u"  "_s));
    }

    void leavesPlainChromeUntouched()
    {
        const QString in = u"Mozilla/5.0 (X11; Linux x86_64) Chrome/130.0.0.0 Safari/537.36"_s;
        QCOMPARE(sanitizeUserAgent(in), in);
    }

    void overrideWins()
    {
        using pldl::core::AppMode;
        using pldl::core::SignInUserAgent;
        QCOMPARE(
            effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, u"  custom  "_s, AppMode::Desktop, u"10"_s),
            u"custom"_s);
        QVERIFY(effectiveUserAgent(u"engine"_s, QString(), AppMode::Tv, u"10"_s).contains(u"Cobalt/19"_s));
        // Music mode is the desktop site's sibling: same identity, never Cobalt.
        QCOMPARE(effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, QString(), AppMode::Music, u"10"_s),
                 effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, QString(), AppMode::Desktop, u"10"_s));
        // The Firefox identity is a per-host header, never the profile UA.
        QVERIFY(pldl::web::signInUserAgent(SignInUserAgent::Firefox, QString()).contains(u"Firefox/"_s));
        QVERIFY(pldl::web::signInUserAgent(SignInUserAgent::Chrome, QString()).isEmpty());
        QVERIFY(pldl::web::signInUserAgent(SignInUserAgent::Firefox, u"custom"_s).isEmpty());
        // A Chrome-like identity of the user's own still gets Firefox on the sign-in hosts.
        QVERIFY(pldl::web::signInUserAgent(SignInUserAgent::Firefox, u"Mozilla/5.0 (Windows) Chrome/130.0.0.0"_s)
                    .contains(u"Firefox/"_s));
        QVERIFY(pldl::web::signInUserAgent(SignInUserAgent::Firefox, u"Mozilla/5.0 (Macintosh) Version/18.6 Safari/605.1.15"_s)
                    .isEmpty());
        QVERIFY(pldl::web::isGoogleSignInHost(u"accounts.google.com"_s));
        QVERIFY(pldl::web::isGoogleSignInHost(u"accounts.youtube.com"_s));
        QVERIFY(!pldl::web::isGoogleSignInHost(u"www.youtube.com"_s));
        QVERIFY(!pldl::web::isGoogleSignInHost(u"myaccounts.google.com"_s));
        QVERIFY(pldl::web::tvWireUserAgent(u"10"_s).contains(u"Cobalt/25"_s));
        QCOMPARE(pldl::web::tvGenericUserAgent(u"10.0.0"_s), u"PlaylistDownloader/10.0.0"_s);
    }

    void presetsFollowTheEngineVersionAndTheChoiceResolves()
    {
        const QString engine = u"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                               "QtWebEngine/6.11.1 Chrome/130.0.6723.192 Safari/537.36"_s;
        const auto presets = pldl::web::userAgentPresets(engine);
        QVERIFY(presets.size() >= 8);
        QCOMPARE(presets.first().id, u"default"_s);
        QVERIFY(presets.first().userAgent.isEmpty());
        int mobile = 0;
        for (const pldl::web::UserAgentPreset& p : presets) {
            QVERIFY(!p.id.isEmpty() && !p.label.isEmpty());
            if (p.id.startsWith(u"chrome"_s) || p.id.startsWith(u"edge"_s)) {
                QVERIFY2(p.userAgent.contains(u"Chrome/130.0.6723.192"_s), qPrintable(p.userAgent));
                QVERIFY(!p.userAgent.contains(u"QtWebEngine"_s));
            }
            mobile += p.mobile ? 1 : 0;
        }
        QCOMPARE(mobile, 2); // Android and iPhone
        QCOMPARE(pldl::web::chosenUserAgent(u"default"_s, u"ignored"_s, engine), QString());
        QCOMPARE(pldl::web::chosenUserAgent(QString(), u"ignored"_s, engine), QString());
        QVERIFY(pldl::web::chosenUserAgent(u"firefox-linux"_s, QString(), engine).contains(u"Firefox/"_s));
        QCOMPARE(pldl::web::chosenUserAgent(u"custom"_s, u"  Mine/1.0  "_s, engine), u"Mine/1.0"_s);
        QCOMPARE(pldl::web::chosenUserAgent(u"no-such-preset"_s, u"Mine/1.0"_s, engine), u"Mine/1.0"_s);
    }

    void emptyOverrideFallsBackToSanitizedDefault()
    {
        QCOMPARE(effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, u"   "_s, pldl::core::AppMode::Desktop,
                                    u"10"_s),
                 u"engine"_s);
    }
};

QTEST_GUILESS_MAIN(TestUserAgent)
#include "tst_user_agent.moc"
