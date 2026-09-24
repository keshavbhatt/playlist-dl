// The Downloads page offscreen: the empty state, the filters over canned
// jobs with the count line, the Clear menu, the header buttons' enabled
// states and the engine chip's text per engine status.

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/engine_manager.h"
#include "services/licensing/license_service.h"
#include "services/media_probe.h"
#include "ui/badge_label.h"
#include "ui/downloads_controller.h"
#include "ui/pages/downloads_page.h"
#include "ui/message_sheet.h"
#include "web/cookie_exporter.h"

#include <QApplication>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QWebEngineProfile>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;
using pldl::core::DownloadState;
using Filter = pldl::ui::DownloadsPage::Filter;

namespace {
pldl::core::DownloadJob canned(quint64 id, DownloadState state)
{
    pldl::core::DownloadJob j;
    j.id = id;
    j.url = u"https://www.youtube.com/watch?v=v%1"_s.arg(id);
    j.title = u"Video %1"_s.arg(id);
    j.state = state;
    return j;
}
} // namespace

class TestDownloadsPage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<pldl::core::Settings>(m_dir->filePath(u"p.ini"_s));
        m_theme = std::make_unique<pldl::core::ThemeService>(*m_settings);
        m_engine = std::make_unique<pldl::services::EngineManager>(*m_settings);
        m_probe = std::make_unique<pldl::services::MediaProbe>();
        m_license = std::make_unique<pldl::services::LicenseService>(*m_settings);
        m_profile = std::make_unique<QWebEngineProfile>();
        m_cookies = std::make_unique<pldl::web::CookieExporter>(m_profile->cookieStore(),
                                                                m_dir->filePath(u"Cookies"_s));
        m_controller = std::make_unique<pldl::ui::DownloadsController>(*m_settings, *m_theme, *m_engine, *m_probe,
                                                                       *m_license, *m_cookies, nullptr);
        m_controller->setPersistencePath(m_dir->filePath(u"downloads.json"_s));
        m_page = std::make_unique<pldl::ui::DownloadsPage>(*m_controller, *m_settings, *m_theme);
        m_page->resize(1000, 640);
        m_page->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_page.get()));
    }

    void cleanup()
    {
        m_page.reset();
        m_controller.reset();
        m_cookies.reset();
        m_profile.reset();
        m_license.reset();
        m_probe.reset();
        m_engine.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void startsEmpty()
    {
        QVERIFY(m_page->isEmptyStateVisible());
        QCOMPARE(m_page->visibleCount(), 0);
        QVERIFY(m_page->countText().isEmpty());
        QCOMPARE(m_page->findChild<QLabel*>(u"emptyTitle"_s)->text(), u"Downloads you start will show up here"_s);
        QCOMPARE(m_page->findChild<QLabel*>(u"emptyBody"_s)->text(),
                 u"Pick items on a playlist page and press Download"_s);
        QVERIFY(!m_page->pauseAllButton()->isEnabled());
        QVERIFY(!m_page->resumeAllButton()->isEnabled());
        QVERIFY(!m_page->clearButton()->isEnabled());
        QVERIFY(m_page->minimumSizeHint().width() <= 1144);
    }

    void filtersTheCannedJobs()
    {
        fillQueue();
        QVERIFY(!m_page->isEmptyStateVisible());
        QCOMPARE(m_page->filter(), Filter::All);
        QCOMPARE(m_page->visibleCount(), 6);
        QCOMPARE(m_page->countText(), u"2 active, 1 paused, 1 finished, 2 failed"_s);
        m_page->setFilter(Filter::Active);
        QCOMPARE(m_page->visibleCount(), 3);
        m_page->setFilter(Filter::Finished);
        QCOMPARE(m_page->visibleCount(), 1);
        m_page->setFilter(Filter::Failed);
        QCOMPARE(m_page->visibleCount(), 2);
        QVERIFY(m_page->pauseAllButton()->isEnabled());
        QVERIFY(m_page->resumeAllButton()->isEnabled());
        QVERIFY(m_page->clearButton()->isEnabled());

        // The chips drive the filter, and a filter with nothing left shows an empty state of its own.
        const QList<QToolButton*> chips = m_page->findChild<QWidget*>(u"filterRow"_s)->findChildren<QToolButton*>();
        QCOMPARE(chips.size(), 4);
        chips.at(2)->click();
        QCOMPARE(m_page->filter(), Filter::Finished);
        m_page->clearFinished();
        QCOMPARE(m_page->visibleCount(), 0);
        QVERIFY(m_page->isEmptyStateVisible());
        QCOMPARE(m_page->findChild<QLabel*>(u"emptyTitle"_s)->text(), u"Nothing here"_s);
        chips.at(0)->click();
        QCOMPARE(m_page->visibleCount(), 5);
        QCOMPARE(m_page->countText(), u"2 active, 1 paused, 2 failed"_s);
    }

    void clearMenuRemovesWhatItSays()
    {
        fillQueue();
        QCOMPARE(m_page->clearMenu()->actions().size(), 3);
        QCOMPARE(m_page->clearMenu()->actions().at(0)->text(), u"Clear finished"_s);
        QCOMPARE(m_page->clearMenu()->actions().at(1)->text(), u"Clear failed"_s);
        QCOMPARE(m_page->clearMenu()->actions().at(2)->text(), u"Clear all finished and failed"_s);
        m_page->clearMenu()->actions().at(1)->trigger();
        QCOMPARE(m_controller->queue().rowCount(), 4); // failed and cancelled gone
        QVERIFY(!m_page->clearMenu()->actions().at(1)->isEnabled());
        m_page->clearMenu()->actions().at(0)->trigger();
        QCOMPARE(m_controller->queue().rowCount(), 3);
        QVERIFY(!m_page->clearButton()->isEnabled());
        fillQueue();
        m_page->clearMenu()->actions().at(2)->trigger();
        QCOMPARE(m_controller->queue().rowCount(), 3);
        QCOMPARE(m_page->countText(), u"2 active, 1 paused"_s);
    }

    void allowanceChipCountsTheDay()
    {
        pldl::ui::BadgeLabel* chip = m_page->allowanceChip();
        QVERIFY(chip != nullptr);
        m_page->setAllowance(3, 5);
        QVERIFY(chip->isVisibleTo(m_page.get()));
        QCOMPARE(chip->text(), u"3 of 5 downloads left today"_s);
        m_page->setAllowance(1, 5);
        QCOMPARE(chip->text(), u"1 of 5 downloads left today"_s);
        m_page->setAllowance(0, 5);
        QCOMPARE(chip->text(), u"No downloads left today"_s);
        QVERIFY(chip->toolTip().contains(u"tomorrow"_s));
        m_page->setAllowance(-1, 5); // Pro or the evaluation: no limit, no chip
        QVERIFY(!chip->isVisibleTo(m_page.get()));
    }

    void engineChipFollowsTheStatus()
    {
        using Status = pldl::services::EngineManager::Status;
        using State = pldl::services::EngineManager::State;
        pldl::ui::BadgeLabel* chip = m_page->engineChip();
        QVERIFY(chip != nullptr);
        Status status;
        status.state = State::NotInstalled;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Set up the download engine"_s); // day one: an invitation
        QCOMPARE(chip->tone(), pldl::ui::BadgeLabel::Tone::Accent);
        status.error = u"no network"_s;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Engine missing"_s); // after a failed setup: a warning
        QCOMPARE(chip->tone(), pldl::ui::BadgeLabel::Tone::Warning);
        status.error.clear();
        status.state = State::Installing;
        status.progress = 0.5;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Setting up 50%"_s);
        status.state = State::Ready;
        status.progress = -1;
        status.ytdlpVersion = u"2026.09.01"_s;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Download engine 2026.09.01"_s);
        QCOMPARE(chip->tone(), pldl::ui::BadgeLabel::Tone::Ok);
        status.updateAvailable = true;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Update available"_s);
        status.state = State::Error;
        status.error = u"no network"_s;
        m_page->setEngineStatus(status);
        QCOMPARE(chip->text(), u"Engine error"_s);
        QCOMPARE(chip->tone(), pldl::ui::BadgeLabel::Tone::Danger);
        QCOMPARE(chip->toolTip(), u"no network"_s);
        // No engine name reaches the user.
        for (const QString& word : {u"yt-dlp"_s, u"ffmpeg"_s, u"deno"_s}) {
            QVERIFY(!chip->text().contains(word, Qt::CaseInsensitive));
        }

        QSignalSpy setup(m_page.get(), &pldl::ui::DownloadsPage::engineSetupRequested);
        QTest::mouseClick(chip, Qt::LeftButton);
        QCOMPARE(setup.count(), 1);
    }

    void listOffersKeyboardAndDoubleClick()
    {
        fillQueue();
        QListView* list = m_page->list();
        QVERIFY(list->isVisible());
        // Delete on a finished item (the cancelled one, last row) removes it at once.
        const QModelIndex cancelled = list->model()->index(5, 0);
        QCOMPARE(cancelled.data(pldl::core::DownloadQueue::IdRole).toULongLong(), 6ULL);
        list->setCurrentIndex(cancelled);
        QTest::keyClick(list, Qt::Key_Delete);
        QCOMPARE(m_controller->queue().rowCount(), 5);
        QVERIFY(!m_controller->queue().job(6));
        // Delete on an active one asks first: the list is untouched until the sheet answers.
        list->setCurrentIndex(list->model()->index(0, 0));
        QTest::keyClick(list, Qt::Key_Delete);
        QCOMPARE(m_controller->queue().rowCount(), 5);
        QVERIFY(m_controller->removeSheet() != nullptr);
        for (QWidget* top : QApplication::topLevelWidgets()) {
            if (top != m_page.get() && top->isWindow() && top->isVisible()) {
                top->close();
            }
        }
    }

private:
    /// Six jobs, one per state that matters to the filters, ids 1 to 6 in
    /// row order (setJobs keeps the order it is given).
    void fillQueue()
    {
        m_controller->queue().setJobs({canned(1, DownloadState::Queued), canned(2, DownloadState::Downloading),
                                       canned(3, DownloadState::Paused), canned(4, DownloadState::Completed),
                                       canned(5, DownloadState::Failed), canned(6, DownloadState::Cancelled)});
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
    std::unique_ptr<pldl::services::EngineManager> m_engine;
    std::unique_ptr<pldl::services::MediaProbe> m_probe;
    std::unique_ptr<pldl::services::LicenseService> m_license;
    std::unique_ptr<QWebEngineProfile> m_profile;
    std::unique_ptr<pldl::web::CookieExporter> m_cookies;
    std::unique_ptr<pldl::ui::DownloadsController> m_controller;
    std::unique_ptr<pldl::ui::DownloadsPage> m_page;
};

int main(int argc, char* argv[])
{
    QTemporaryDir dataHome;
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", dataHome.filePath(u"cache"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", dataHome.filePath(u"config"_s).toUtf8());
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-downloads-page"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestDownloadsPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_downloads_page.moc"
