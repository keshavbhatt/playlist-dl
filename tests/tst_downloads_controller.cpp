// The downloads controller offscreen, without an engine: the default options
// follow the settings, admission counts against the free tier's daily
// allowance and the gate sheet offers the plans, the active count reaches the
// rail, the list persists under XDG_DATA_HOME and comes back paused, and a
// request for a link that cannot be downloaded settles at once.

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/engine_manager.h"
#include "services/licensing/license_service.h"
#include "services/media_probe.h"
#include "ui/downloads_controller.h"
#include "ui/message_sheet.h"
#include "ui/thumbnail_cache.h"
#include "web/cookie_exporter.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWebEngineProfile>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;
using pldl::core::DownloadState;

namespace {
pldl::core::DownloadJob video(int n)
{
    pldl::core::DownloadJob j;
    j.url = u"https://www.youtube.com/watch?v=video%1"_s.arg(n);
    j.title = u"Video %1"_s.arg(n);
    return j;
}

pldl::ui::MessageSheet* visibleSheet()
{
    for (QWidget* top : QApplication::topLevelWidgets()) {
        if (auto* sheet = qobject_cast<pldl::ui::MessageSheet*>(top); sheet != nullptr && sheet->isVisible()) {
            return sheet;
        }
    }
    return nullptr;
}
} // namespace

class TestDownloadsController : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<pldl::core::Settings>(m_dir->filePath(u"c.ini"_s));
        m_settings->setDownloadDirectory(m_dir->filePath(u"downloads"_s));
        m_theme = std::make_unique<pldl::core::ThemeService>(*m_settings);
        m_engine = std::make_unique<pldl::services::EngineManager>(*m_settings);
        m_probe = std::make_unique<pldl::services::MediaProbe>();
        m_license = std::make_unique<pldl::services::LicenseService>(*m_settings); // never started: free tier
        m_profile = std::make_unique<QWebEngineProfile>();
        m_cookies = std::make_unique<pldl::web::CookieExporter>(m_profile->cookieStore(),
                                                                m_dir->filePath(u"Cookies"_s));
        m_parent = std::make_unique<QWidget>();
        m_parent->resize(800, 600);
        m_parent->show();
    }

    void cleanup()
    {
        for (QWidget* top : QApplication::topLevelWidgets()) {
            if (top != m_parent.get() && top->isWindow()) {
                top->close();
            }
        }
        m_parent.reset();
        m_cookies.reset();
        m_profile.reset();
        m_license.reset();
        m_probe.reset();
        m_engine.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    std::unique_ptr<pldl::ui::DownloadsController> makeController()
    {
        auto controller = std::make_unique<pldl::ui::DownloadsController>(
            *m_settings, *m_theme, *m_engine, *m_probe, *m_license, *m_cookies, m_parent.get());
        return controller;
    }

    void writesThePlaylistFileForAFinishedPlaylist()
    {
        auto controller = makeController();
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        pldl::core::DownloadJob job;
        job.id = 42;
        job.title = u"Songs"_s;
        job.options.isPlaylist = true;
        job.options.outputDirectory = folder.path();
        const QString first = folder.filePath(u"001 - One.mp4"_s);
        QFile f(first);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        job.outputFiles << first;
        job.entries << pldl::core::PlaylistEntry{u"one"_s, u"One"_s, first};
        job.state = pldl::core::DownloadState::Completed;
        controller->queue().setJobs({job});
        m_settings->setWritePlaylistFile(false);
        QVERIFY(!controller->writePlaylistFileFor(42));
        QVERIFY(!QFile::exists(folder.filePath(u"Songs.m3u8"_s)));
        m_settings->setWritePlaylistFile(true);
        QVERIFY(controller->writePlaylistFileFor(42));
        QVERIFY(QFile::exists(folder.filePath(u"Songs.m3u8"_s)));
        QVERIFY(!controller->writePlaylistFileFor(43)); // no such job
    }

    void defaultOptionsFollowTheSettings()
    {
        m_settings->setLastDownloadKind(pldl::core::DownloadKind::Audio);
        m_settings->setDefaultAudioFormat(pldl::core::AudioFormat::Mp3);
        m_settings->setDefaultQuality(pldl::core::VideoQuality::Q720);
        m_settings->setDefaultContainer(pldl::core::Container::Mkv);
        m_settings->setSpeedLimitKbps(512);
        m_settings->setOrganiseDownloads(true);
        auto controller = makeController();

        pldl::core::DownloadOptions single = controller->defaultOptions(false);
        QCOMPARE(single.kind, pldl::core::DownloadOptions::Kind::Audio);
        QCOMPARE(single.audioFormat, pldl::core::AudioFormat::Mp3);
        QCOMPARE(single.quality, pldl::core::VideoQuality::Q720);
        QCOMPARE(single.container, pldl::core::Container::Mkv);
        QCOMPARE(single.outputDirectory, m_settings->downloadDirectory());
        QCOMPARE(single.folder, u"Music"_s);
        QCOMPARE(single.speedLimitKbps, 512);
        QVERIFY(!single.isPlaylist);

        pldl::core::DownloadOptions playlist = controller->defaultOptions(true);
        QVERIFY(playlist.isPlaylist);
        QCOMPARE(playlist.folder, u"Playlists"_s);

        m_settings->setLastDownloadKind(pldl::core::DownloadKind::Video);
        m_settings->setOrganiseDownloads(false);
        QCOMPARE(controller->defaultOptions(false).kind, pldl::core::DownloadOptions::Kind::Video);
        QVERIFY(controller->defaultOptions(false).folder.isEmpty());
    }

    void admissionCountsAgainstTheDailyAllowance()
    {
        auto controller = makeController();
        QSignalSpy plans(controller.get(), &pldl::ui::DownloadsController::plansRequested);
        QSignalSpy toasts(controller.get(), &pldl::ui::DownloadsController::toast);
        QCOMPARE(m_license->downloadsRemainingToday(), pldl::services::LicenseService::kFreeDownloadsPerDay);

        QList<quint64> ids;
        for (int n = 0; n < pldl::services::LicenseService::kFreeDownloadsPerDay; ++n) {
            const quint64 id = controller->enqueue(video(n));
            QVERIFY(id != 0);
            ids << id;
        }
        QCOMPARE(m_license->downloadsRemainingToday(), 0);
        QCOMPARE(toasts.count(), pldl::services::LicenseService::kFreeDownloadsPerDay);
        QCOMPARE(controller->queue().rowCount(), 5);
        // A link already in the list folds into its entry and costs nothing.
        QCOMPARE(controller->enqueue(video(0)), ids.first());
        QCOMPARE(controller->queue().rowCount(), 5);
        QVERIFY(visibleSheet() == nullptr);

        // The sixth is refused: the gate sheet appears, View plans asks for the plans.
        QCOMPARE(controller->enqueue(video(9)), 0ULL);
        QTRY_VERIFY(visibleSheet() != nullptr);
        pldl::ui::MessageSheet* sheet = visibleSheet();
        QPushButton* viewPlans = nullptr;
        for (QPushButton* button : sheet->findChildren<QPushButton*>()) {
            if (button->text() == u"View plans"_s) {
                viewPlans = button;
            }
        }
        QVERIFY(viewPlans != nullptr);
        viewPlans->click();
        QTRY_COMPARE(plans.count(), 1);
        QCOMPARE(controller->queue().rowCount(), 5);

        // A batch is admitted or refused as a whole.
        QVERIFY(controller->enqueueAll({video(20), video(21)}).isEmpty());
        QTRY_VERIFY(visibleSheet() != nullptr);
        visibleSheet()->close();
    }

    void playlistsCountTheirEntries()
    {
        auto controller = makeController();
        pldl::core::DownloadJob playlist;
        playlist.url = u"https://www.youtube.com/playlist?list=PL123"_s;
        playlist.options.isPlaylist = true;
        playlist.itemCount = 3;
        QVERIFY(controller->enqueue(playlist) != 0);
        QCOMPARE(m_license->downloadsRemainingToday(), 2);
        pldl::core::DownloadJob ranged;
        ranged.url = u"https://www.youtube.com/playlist?list=PL456"_s;
        ranged.options.isPlaylist = true;
        ranged.options.playlistItems = u"1,4-6"_s; // four entries: more than the two left
        QCOMPARE(controller->enqueue(ranged), 0ULL);
        QTRY_VERIFY(visibleSheet() != nullptr);
        visibleSheet()->close();
    }

    void activeCountReachesTheRailAndTheListPersists()
    {
        const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
        QVERIFY(!dataHome.isEmpty());
        QVERIFY(pldl::core::DownloadQueue::defaultFilePath().startsWith(dataHome));
        QVERIFY(pldl::ui::ThumbnailCache::keepsakeDirectory().startsWith(dataHome));
        QFile::remove(pldl::core::DownloadQueue::defaultFilePath());
        quint64 id = 0;
        {
            auto controller = makeController();
            QCOMPARE(controller->persistencePath(), pldl::core::DownloadQueue::defaultFilePath());
            controller->start(); // nothing on disk yet
            QCOMPARE(controller->queue().rowCount(), 0);
            QSignalSpy active(controller.get(), &pldl::ui::DownloadsController::activeCountChanged);
            id = controller->enqueue(video(1));
            QVERIFY(id != 0);
            QCOMPARE(controller->queue().job(id)->state, DownloadState::Queued); // no engine
            QCOMPARE(active.count(), 1);
            QCOMPARE(active.at(0).at(0).toInt(), 1);
            // Persisted 500 ms after the change.
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pldl::core::DownloadQueue::defaultFilePath()), 3000);
        }
        auto again = makeController();
        QSignalSpy active(again.get(), &pldl::ui::DownloadsController::activeCountChanged);
        again->start();
        QCOMPARE(again->queue().rowCount(), 1);
        QCOMPARE(again->queue().job(id)->title, u"Video 1"_s);
        QCOMPARE(again->queue().job(id)->state, DownloadState::Paused); // a stale queued entry
        QCOMPARE(active.count(), 0);                                    // paused is not active: still 0
        again->shutdown();
        QVERIFY(QFile::exists(pldl::core::DownloadQueue::defaultFilePath()));
    }

    void requestsSettleWhenNothingCanBeQueued()
    {
        auto controller = makeController();
        QSignalSpy settled(controller.get(), &pldl::ui::DownloadsController::requestSettled);
        QSignalSpy toasts(controller.get(), &pldl::ui::DownloadsController::toast);
        controller->requestDownload(QUrl(u"https://example.com/not-youtube"_s));
        QCOMPARE(settled.count(), 1);
        QCOMPARE(settled.at(0).at(1).toBool(), false);
        // A YouTube link before the engine exists: settled too, with a toast.
        controller->requestDownload(QUrl(u"https://www.youtube.com/watch?v=jNQXAC9IVRw"_s));
        QCOMPARE(settled.count(), 2);
        QCOMPARE(toasts.count(), 2);
        QCOMPARE(controller->queue().rowCount(), 0);
        controller->cancelRequests();
        QCOMPARE(settled.count(), 2);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
    std::unique_ptr<pldl::services::EngineManager> m_engine;
    std::unique_ptr<pldl::services::MediaProbe> m_probe;
    std::unique_ptr<pldl::services::LicenseService> m_license;
    std::unique_ptr<QWebEngineProfile> m_profile;
    std::unique_ptr<pldl::web::CookieExporter> m_cookies;
    std::unique_ptr<QWidget> m_parent;
};

int main(int argc, char* argv[])
{
    // Everything the controller writes (the list, the thumbnails) lands in
    // this run's own data home, never the developer's.
    QTemporaryDir dataHome;
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", dataHome.filePath(u"cache"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", dataHome.filePath(u"config"_s).toUtf8());
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-downloads-controller"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestDownloadsController test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_downloads_controller.moc"
