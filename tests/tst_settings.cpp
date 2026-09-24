#include "core/settings/settings.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;

class TestSettings : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"pldl.ini"_s));
    }

    void defaults()
    {
        QCOMPARE(m_settings->theme(), Theme::System);
        QCOMPARE(m_settings->closeAction(), CloseAction::Quit);
        QCOMPARE(m_settings->lastPage(), 0);
        QVERIFY(m_settings->trayEnabled());
        QVERIFY(m_settings->notifyOnDownloadFinish());
        QVERIFY(m_settings->blockAds());
        QVERIFY(!m_settings->doNotTrack());
        QVERIFY(!m_settings->restoreBrowserTabs());
        QCOMPARE(m_settings->browserStartPage(), u"https://www.youtube.com/"_s);
        QCOMPARE(m_settings->browserUserAgentPreset(), u"default"_s);
        QVERIFY(m_settings->browserUserAgent().isEmpty());
        QVERIFY(m_settings->browserSession().urls.isEmpty());
        QVERIFY(!m_settings->hardwareVideoDecode());
        QCOMPARE(m_settings->hardwareAcceleration(), HardwareAcceleration::Auto);
        QCOMPARE(m_settings->interfaceScale(), 1.0);
        QCOMPARE(m_settings->concurrentDownloads(), 2);
        QCOMPARE(m_settings->speedLimitKbps(), 0);
        QVERIFY(m_settings->useSessionCookies());
        QVERIFY(m_settings->embedThumbnail());
        QVERIFY(m_settings->embedMetadata());
        QVERIFY(m_settings->downloadDirectory().endsWith(u"/Playlist Downloader"_s));
        QVERIFY(m_settings->whatsNewSeenVersion().isEmpty());
        m_settings->setWhatsNewSeenVersion(u" 3.0.0 "_s);
        QCOMPARE(m_settings->whatsNewSeenVersion(), u"3.0.0"_s);
        QVERIFY(m_settings->organiseDownloads());
        m_settings->setOrganiseDownloads(false);
        QVERIFY(!m_settings->organiseDownloads());
        QVERIFY(m_settings->freeDownloads().day.isEmpty());
        m_settings->setFreeDownloads({u"2026-09-13"_s, 3});
        QCOMPARE(m_settings->freeDownloads().used, 3);
        m_settings->resetToDefaults(); // the counter is not a preference
        QCOMPARE(m_settings->freeDownloads().day, u"2026-09-13"_s);
        QCOMPARE(m_settings->freeDownloads().used, 3);
        QCOMPARE(m_settings->defaultContainer(), Container::Mp4);
        QCOMPARE(m_settings->defaultQuality(), VideoQuality::Best);
        QCOMPARE(m_settings->defaultAudioFormat(), AudioFormat::Best);
        QCOMPARE(m_settings->lastDownloadKind(), DownloadKind::Video);
        QCOMPARE(m_settings->filenamePattern(), FilenamePattern::Title);
        QVERIFY(m_settings->engineAutoUpdate());
        QVERIFY(!m_settings->engineUseSystem());
    }

    void signalsAndClamping()
    {
        QSignalSpy theme(m_settings.get(), &Settings::themeChanged);
        m_settings->setTheme(Theme::Dark);
        m_settings->setTheme(Theme::Dark);
        QCOMPARE(theme.count(), 1);
        QCOMPARE(m_settings->theme(), Theme::Dark);

        m_settings->setInterfaceScale(99);
        QCOMPARE(m_settings->interfaceScale(), Settings::kMaxInterfaceScale);
        m_settings->setConcurrentDownloads(50);
        QCOMPARE(m_settings->concurrentDownloads(), Settings::kMaxConcurrentDownloads);
        m_settings->setSpeedLimitKbps(-5);
        QCOMPARE(m_settings->speedLimitKbps(), 0);
        m_settings->setLastPage(-3);
        QCOMPARE(m_settings->lastPage(), 0);

        QSignalSpy ads(m_settings.get(), &Settings::blockAdsChanged);
        QSignalSpy browser(m_settings.get(), &Settings::browserChanged);
        m_settings->setBlockAds(false);
        m_settings->setBlockAds(false);
        QCOMPARE(ads.count(), 1);
        QCOMPARE(ads.first().at(0).toBool(), false);
        m_settings->setDoNotTrack(true);
        m_settings->setRestoreBrowserTabs(true);
        QCOMPARE(browser.count(), 3);

        QSignalSpy identity(m_settings.get(), &Settings::browserUserAgentChanged);
        m_settings->setBrowserUserAgentPreset(u"  "_s); // blank means "default", written once
        m_settings->setBrowserUserAgentPreset(u"default"_s);
        QCOMPARE(identity.count(), 1);
        QCOMPARE(m_settings->browserUserAgentPreset(), u"default"_s);
        m_settings->setBrowserUserAgentPreset(u"firefox-linux"_s);
        m_settings->setBrowserUserAgent(u"  Mine/1.0 "_s);
        QCOMPARE(identity.count(), 3);
        QCOMPARE(m_settings->browserUserAgent(), u"Mine/1.0"_s);
    }

    void startPage()
    {
        QVERIFY(Settings::isEmptyStartPage(u"about:blank"_s));
        QVERIFY(Settings::isEmptyStartPage(u" ABOUT:NEWTAB "_s));
        QVERIFY(!Settings::isEmptyStartPage(u"https://www.youtube.com/"_s));
        m_settings->setBrowserStartPage(u"  "_s);
        QCOMPARE(m_settings->browserStartPage(), u"https://www.youtube.com/"_s);
        m_settings->setBrowserStartPage(QString(Settings::kEmptyStartPage));
        QCOMPARE(m_settings->browserStartPage(), u"about:blank"_s);
    }

    void browserSessionRoundTrip()
    {
        Settings::BrowserSession session;
        session.urls = {u"https://www.youtube.com/"_s, u"  "_s, u"https://example.com/"_s};
        session.current = 7; // out of range: clamped on read
        m_settings->setBrowserSession(session);
        m_settings->sync();
        Settings again(m_dir->filePath(u"pldl.ini"_s));
        const Settings::BrowserSession saved = again.browserSession();
        QCOMPARE(saved.urls, (QStringList{u"https://www.youtube.com/"_s, u"https://example.com/"_s}));
        QCOMPARE(saved.current, 1);
    }

    void gpuFallbackResetsOnExplicitChoice()
    {
        m_settings->setGpuAutoDisabled(true);
        m_settings->setGpuProbeStrikes(1);
        m_settings->setHardwareAcceleration(HardwareAcceleration::On);
        QVERIFY(!m_settings->gpuAutoDisabled());
        QCOMPARE(m_settings->gpuProbeStrikes(), 0);
    }

    void persistsAcrossInstances()
    {
        m_settings->setDownloadDirectory(u"/tmp/pldl-test"_s);
        m_settings->setSubtitleLanguages({u"en"_s, u"de"_s});
        m_settings->setLastPage(2);
        m_settings->sync();
        Settings again(m_dir->filePath(u"pldl.ini"_s));
        QCOMPARE(again.downloadDirectory(), u"/tmp/pldl-test"_s);
        QCOMPARE(again.subtitleLanguages(), (QStringList{u"en"_s, u"de"_s}));
        QCOMPARE(again.lastPage(), 2);
    }

    void resetToDefaults()
    {
        m_settings->setTheme(Theme::Light);
        m_settings->setBlockAds(false);
        m_settings->setWindowGeometry(QByteArray("geometry"));
        QSignalSpy theme(m_settings.get(), &Settings::themeChanged);
        m_settings->resetToDefaults();
        QCOMPARE(m_settings->theme(), Theme::System);
        QVERIFY(m_settings->blockAds());
        QCOMPARE(m_settings->windowGeometry(), QByteArray("geometry")); // window state is kept
        QVERIFY(theme.count() >= 1);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
};

QTEST_GUILESS_MAIN(TestSettings)
#include "tst_settings.moc"
