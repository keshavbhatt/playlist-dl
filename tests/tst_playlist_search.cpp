// The search orchestrator (ADR-003): the service first, the engine when the
// service fails, "Load more" with the source that answered, the engine asked
// for lazily. Offline: a local server stands in for the service and the
// engine search is left without an engine.

#include "core/settings/settings.h"
#include "services/playlist_search.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using namespace pldl::services;

namespace {

const QByteArray kOneResult =
    "[{\"title\": \"lofi\", \"playlistId\": \"PL1\", \"author\": \"a\", \"videoCount\": 3,"
    " \"playlistThumbnail\": \"https://i.ytimg.com/vi/x/hqdefault.jpg\"}]";

/// Answers every request with a fixed status and body.
class CannedServer : public QObject
{
    Q_OBJECT

public:
    int status = 200;
    QByteArray body;
    int requests = 0;

    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    if (!socket->readAll().contains("\r\n\r\n")) {
                        return;
                    }
                    ++requests;
                    socket->write("HTTP/1.1 " + QByteArray::number(status) + " Whatever\r\n"
                                  "Content-Type: application/json\r\nContent-Length: " +
                                  QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    [[nodiscard]] QUrl url() const
    {
        return QUrl(u"http://127.0.0.1:"_s + QString::number(m_server.serverPort()) + u"/api.php"_s);
    }

private:
    QTcpServer m_server;
};

} // namespace

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

    void everyServiceOutcomeFallsBack()
    {
        QVERIFY(PlaylistSearch::fallsBack(KtechpitSearch::Outcome::NetworkError));
        QVERIFY(PlaylistSearch::fallsBack(KtechpitSearch::Outcome::Timeout));
        QVERIFY(PlaylistSearch::fallsBack(KtechpitSearch::Outcome::BadPayload));
        QVERIFY(PlaylistSearch::fallsBack(KtechpitSearch::Outcome::Empty));
    }

    void describeNeverNamesATool()
    {
        QCOMPARE(PlaylistSearch::describe(PlaylistSearch::Source::Service), u"Search"_s);
        QCOMPARE(PlaylistSearch::describe(PlaylistSearch::Source::Engine), u"Search (engine)"_s);
        QCOMPARE(PlaylistSearch::tooltip(PlaylistSearch::Source::Engine),
                 u"The search service did not answer, so the download engine searched instead"_s);
        for (const QString& text : {PlaylistSearch::describe(PlaylistSearch::Source::Service),
                                    PlaylistSearch::describe(PlaylistSearch::Source::Engine),
                                    PlaylistSearch::tooltip(PlaylistSearch::Source::Service),
                                    PlaylistSearch::tooltip(PlaylistSearch::Source::Engine)}) {
            QVERIFY2(!text.contains(u"ktechpit"_s, Qt::CaseInsensitive), qPrintable(text));
            QVERIFY2(!text.contains(u"yt-dlp"_s, Qt::CaseInsensitive), qPrintable(text));
        }
    }

    void modeFollowsTheSettingUnlessOverridden()
    {
        PlaylistSearch search(*m_settings);
        QCOMPARE(search.mode(), PlaylistSearch::Mode::Automatic);
        m_settings->setSearchMode(SearchMode::EngineOnly);
        QCOMPARE(search.mode(), PlaylistSearch::Mode::EngineOnly);
        search.setModeOverride(PlaylistSearch::Mode::Automatic);
        QCOMPARE(search.mode(), PlaylistSearch::Mode::Automatic);
        search.setModeOverride(std::nullopt);
        QCOMPARE(search.mode(), PlaylistSearch::Mode::EngineOnly);
        QVERIFY(!search.hasEngine());
        m_settings->setSearchResultsPerPage(35);
        QCOMPARE(search.engine().pageSize(), 35);
    }

    void theServiceAnswersPageZeroAndHasNoMore()
    {
        CannedServer server;
        server.body = kOneResult;
        QVERIFY(server.listen());
        PlaylistSearch search(*m_settings);
        search.service().setEndpoint(server.url());
        QSignalSpy finished(&search, &PlaylistSearch::finished);
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        const quint64 id = search.search(u"lofi"_s);
        QVERIFY(search.isSearching());
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 5000);
        QCOMPARE(finished.first().at(0).toULongLong(), id);
        QCOMPARE(finished.first().at(1).value<QList<SearchResult>>().size(), 1);
        QCOMPARE(finished.first().at(2).toBool(), false);
        QCOMPARE(finished.first().at(3).value<PlaylistSearch::Source>(), PlaylistSearch::Source::Service);
        QCOMPARE(search.source(), PlaylistSearch::Source::Service);
        QVERIFY(!search.isSearching());
        QVERIFY(failed.isEmpty());
        QVERIFY(needed.isEmpty());

        // The service has no paging: the next page is empty and done.
        const quint64 more = search.search(u"lofi"_s, 1);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 5000);
        QCOMPARE(finished.last().at(0).toULongLong(), more);
        QVERIFY(finished.last().at(1).value<QList<SearchResult>>().isEmpty());
        QCOMPARE(finished.last().at(2).toBool(), false);
        QCOMPARE(server.requests, 1);
    }

    void aFailingServiceAsksForTheEngine()
    {
        CannedServer server;
        server.status = 500;
        server.body = "down";
        QVERIFY(server.listen());
        PlaylistSearch search(*m_settings);
        search.service().setEndpoint(server.url());
        QSignalSpy finished(&search, &PlaylistSearch::finished);
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        QSignalSpy sourceChanged(&search, &PlaylistSearch::sourceChanged);
        const quint64 id = search.search(u"lofi"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!needed.isEmpty(), 5000);
        QCOMPARE(server.requests, 1);
        QVERIFY(search.hasPending());
        QVERIFY(search.isSearching());
        QCOMPARE(search.source(), PlaylistSearch::Source::Engine);
        QCOMPARE(sourceChanged.size(), 1);
        QVERIFY(finished.isEmpty());
        QVERIFY(failed.isEmpty());

        // Still no engine after the setup: the search fails with a message
        // instead of waiting forever.
        search.retryPending();
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toULongLong(), id);
        QVERIFY(!failed.first().at(1).toString().isEmpty());
        QVERIFY(!search.hasPending());
        QVERIFY(!search.isSearching());
    }

    void emptyAndBadAnswersFallBackToo()
    {
        CannedServer server;
        QVERIFY(server.listen());
        PlaylistSearch search(*m_settings);
        search.service().setEndpoint(server.url());
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        server.body = "[]";
        search.search(u"nothing"_s);
        QTRY_COMPARE_WITH_TIMEOUT(needed.size(), 1, 5000);
        server.body = "{\"oops\": true}";
        search.search(u"nothing"_s);
        QTRY_COMPARE_WITH_TIMEOUT(needed.size(), 2, 5000);
        QCOMPARE(server.requests, 2);
        // cancel() drops the pending search; retryPending then does nothing.
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        search.cancel();
        QVERIFY(!search.hasPending());
        search.retryPending();
        QVERIFY(failed.isEmpty());
    }

    void engineOnlySkipsTheService()
    {
        CannedServer server;
        server.body = kOneResult;
        QVERIFY(server.listen());
        m_settings->setSearchMode(SearchMode::EngineOnly);
        PlaylistSearch search(*m_settings);
        search.service().setEndpoint(server.url());
        QSignalSpy needed(&search, &PlaylistSearch::engineNeeded);
        search.search(u"lofi"_s);
        QCOMPARE(needed.size(), 1); // synchronous: no engine, nothing to wait for
        QCOMPARE(search.source(), PlaylistSearch::Source::Engine);
        QTest::qWait(200);
        QCOMPARE(server.requests, 0);

        // With an engine path set the search goes to the engine; a path that
        // cannot start reports a failure rather than the service.
        search.cancel();
        EnginePaths paths;
        paths.ytdlp = m_dir->filePath(u"no-such-engine"_s);
        search.setEnginePaths(paths);
        QVERIFY(search.hasEngine());
        QSignalSpy failed(&search, &PlaylistSearch::failed);
        search.search(u"lofi"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 10000);
        QCOMPARE(server.requests, 0);
    }

    void anEmptyQueryFinishesAtOnce()
    {
        PlaylistSearch search(*m_settings);
        QSignalSpy finished(&search, &PlaylistSearch::finished);
        search.search(u"   "_s);
        QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 1, 2000);
        QVERIFY(finished.first().at(1).value<QList<SearchResult>>().isEmpty());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
};

QTEST_GUILESS_MAIN(TestPlaylistSearch)
#include "tst_playlist_search.moc"
