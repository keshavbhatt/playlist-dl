// The supported sites list (FEATURES S13): the engine's extractor names parsed,
// grouped and cased, the host match for the browser pill, the cache file.

#include "services/supported_sites.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::services;

namespace {
const QByteArray kList = "youtube\nyoutube:tab\nyoutube:search_url\nvimeo\nvimeo:album\nVimeoPro\n"
                         "abc.net.au\nabc.net.au:iview\n20min (CURRENTLY BROKEN)\ngeneric\ngeneric:quoted-html\n"
                         "bbc.co.uk\nbbc\n9now.com.au\nAbemaTV\n\n";
}

class TestSupportedSites : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesGroupsAndCases()
    {
        const QList<SupportedSite> sites = SupportedSites::parse(kList);
        QStringList display;
        for (const SupportedSite& s : sites) {
            display << s.display();
        }
        // Broken entries are gone, the generic scraper stays; a site's variants follow it; brands keep their casing.
        QCOMPARE(display, (QStringList{u"9now.com.au"_s, u"abc.net.au"_s, u"abc.net.au: iview"_s, u"AbemaTV"_s,
                                       u"BBC"_s, u"BBC"_s, u"Generic"_s, u"Generic: quoted-html"_s, u"Vimeo"_s,
                                       u"Vimeo: album"_s, u"VimeoPro"_s, u"YouTube"_s, u"YouTube: search url"_s,
                                       u"YouTube: tab"_s}));
        SupportedSites service;
        service.setSites(sites, u"2026.08.19"_s);
        QCOMPARE(service.siteCount(), 8); // bbc and bbc.co.uk both read BBC
    }

    void matchesAHostToASite()
    {
        SupportedSites service;
        service.setSites(SupportedSites::parse(kList), u"v"_s);
        QCOMPARE(service.siteForHost(u"www.youtube.com"_s), u"YouTube"_s);
        QCOMPARE(service.siteForHost(u"player.vimeo.com"_s), u"Vimeo"_s);
        QCOMPARE(service.siteForHost(u"www.abc.net.au"_s), u"abc.net.au"_s);
        QCOMPARE(service.siteForHost(u"bbc.co.uk"_s), u"BBC"_s);
        QCOMPARE(service.siteForHost(u"9now.com.au"_s), u"9now.com.au"_s);
        QVERIFY(service.siteForHost(u"example.com"_s).isEmpty());
        QVERIFY(service.siteForHost(u"com"_s).isEmpty());
        QVERIFY(service.siteForHost(QString()).isEmpty());
        QVERIFY(service.siteForHost(u"tab.example.org"_s).isEmpty()); // a variant never matches
    }

    void guessesAHomePage()
    {
        QCOMPARE(SupportedSites::homePage(u"Vimeo"_s), QUrl(u"https://vimeo.com"_s));
        QCOMPARE(SupportedSites::homePage(u"abc.net.au"_s), QUrl(u"https://abc.net.au"_s));
        QVERIFY(SupportedSites::homePage(QString()).isEmpty());
    }

    void readsTheCacheBeforeAskingTheEngine()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        SupportedSites::setCacheDirectory(dir.path());
        QFile cache(SupportedSites::cachePath(u"2026.08.19"_s));
        QVERIFY(cache.open(QIODevice::WriteOnly));
        cache.write(kList);
        cache.close();

        SupportedSites service;
        QSignalSpy spy(&service, &SupportedSites::loaded);
        service.load(u"/nonexistent/engine"_s, u"2026.08.19"_s); // the cache answers, the engine is never run
        QCOMPARE(spy.count(), 1);
        QVERIFY(service.isLoaded());
        QCOMPARE(service.version(), u"2026.08.19"_s);
        QCOMPARE(service.siteCount(), 8);

        // The same version again is a no-op; a newer one (an engine update, the
        // manager's ready() again) loads afresh and the older cache goes.
        service.load(u"/nonexistent/engine"_s, u"2026.08.19"_s);
        QCOMPARE(spy.count(), 1);
        QFile newer(SupportedSites::cachePath(u"2026.09.20"_s));
        QVERIFY(newer.open(QIODevice::WriteOnly));
        newer.write("youtube\nnewsite\n");
        newer.close();
        service.load(u"/nonexistent/engine"_s, u"2026.09.20"_s);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(service.version(), u"2026.09.20"_s);
        QCOMPARE(service.siteCount(), 2);
        SupportedSites::pruneCaches(u"2026.09.20"_s); // what a fresh engine answer does after caching
        QVERIFY(!QFile::exists(SupportedSites::cachePath(u"2026.08.19"_s)));
        QVERIFY(QFile::exists(SupportedSites::cachePath(u"2026.09.20"_s)));

        // No cache and no engine: nothing loads, nothing crashes. A system
        // engine's list is never cached.
        SupportedSites empty;
        empty.load(QString(), u"other"_s);
        QVERIFY(!empty.isLoaded());
        QVERIFY(!SupportedSites::isCacheable(u"system"_s));
        QVERIFY(!SupportedSites::isCacheable(QString()));
        QVERIFY(SupportedSites::isCacheable(u"2026.09.20"_s));
        QCOMPARE(SupportedSites::cachePath(u"a/b c"_s), dir.path() + u"/supported-sites-a_b_c.txt"_s);
        SupportedSites::setCacheDirectory(QString());
    }
};

QTEST_GUILESS_MAIN(TestSupportedSites)
#include "tst_supported_sites.moc"
