// The Search page's one search (ADR-003 as revised): the engine's playlist
// search, asked for lazily. Offline: no engine, or a path that cannot start.

#include "core/settings/settings.h"
#include "services/playlist_search.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using namespace pldl::services;

class TestPlaylistSearch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"search.ini"_s));
    }

    void cleanup()
    {
        m_settings.reset();
        m_dir.reset();
    }

    void pageSizeFollowsTheSetting()
    {
        PlaylistSearch search(*m_settings);
        QVERIFY(!search.hasEngine());
        QCOMPARE(search.engine().pageSize(), m_settings->searchResultsPerPage());
        m_settings->setSearchResultsPerPage(30);
        QCOMPARE(search.engine().pageSize(), 30);
    }

    void withoutAnEngineTheSearchWaitsForIt()
    {
        PlaylistSearch search(*m_settings);
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        const quint64 id = search.search(u"lofi"_s);
        QCOMPARE(needed.size(), 1); // synchronous: no engine, nothing to wait for
        QVERIFY(search.isSearching());
        QVERIFY(search.hasPending());
        QCOMPARE(search.query(), u"lofi"_s);
        // Still no engine on retry: the search fails with a message instead of hanging.
        search.retryPending();
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toULongLong(), id);
        QVERIFY(!failed.first().at(1).toString().contains(u"yt-dlp"_s, Qt::CaseInsensitive));
        QVERIFY(!search.isSearching());
        QVERIFY(!search.hasPending());
    }

    void cancelDropsThePendingSearch()
    {
        PlaylistSearch search(*m_settings);
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        search.search(u"lofi"_s);
        QVERIFY(search.hasPending());
        search.cancel();
        QVERIFY(!search.hasPending());
        search.retryPending();
        QVERIFY(failed.isEmpty());
    }

    void anEngineThatCannotStartReportsAFailure()
    {
        PlaylistSearch search(*m_settings);
        EnginePaths paths;
        paths.ytdlp = m_dir->filePath(u"no-such-engine"_s);
        search.setEnginePaths(paths);
        QVERIFY(search.hasEngine());
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        const quint64 id = search.search(u"lofi"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 10000);
        QCOMPARE(failed.first().at(0).toULongLong(), id);
        QVERIFY(needed.isEmpty());
        QVERIFY(!search.isSearching());
    }

    void anEmptyQueryFinishesAtOnce()
    {
        PlaylistSearch search(*m_settings);
        QSignalSpy finished(&search, &PlaylistSearch::finished);
        search.search(u"   "_s);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2000);
        QVERIFY(finished.first().at(1).value<QList<SearchResult>>().isEmpty());
        QCOMPARE(finished.first().at(2).toBool(), false);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
};

QTEST_GUILESS_MAIN(TestPlaylistSearch)
#include "tst_playlist_search.moc"
