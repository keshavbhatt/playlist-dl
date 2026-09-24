// The search service client (FEATURES B1, ADR-003): the documented JSON shape
// parsed into playlist results, and every failure mapped to a typed outcome
// against local servers. Offline.

#include "services/ktechpit_search.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::services;

namespace {

// The shape 4.2 of the 2.x analysis documents, two playlists.
const QByteArray kServiceJson =
    "[\n"
    "  {\"title\": \"lofi hip hop radio\", \"playlistId\": \"PLOHoVaTp8R7dX\", \"author\": \"Lofi Girl\",\n"
    "   \"authorId\": \"UCSJ4gkVC6NrvII8umztf0Ow\", \"videoCount\": 120,\n"
    "   \"playlistThumbnail\": \"https://i.ytimg.com/vi/jfKfPfyJRdk/hqdefault.jpg\",\n"
    "   \"videos\": [{\"title\": \"one\", \"videoId\": \"jfKfPfyJRdk\", \"lengthSeconds\": 213}]},\n"
    "  {\"title\": \"no count\", \"playlistId\": \"PLsecond\", \"author\": \"\",\n"
    "   \"playlistThumbnail\": \"https://i.ytimg.com/vi/abc/mqdefault.jpg\"},\n"
    "  {\"title\": \"no id\", \"author\": \"nobody\"}\n"
    "]\n";

/// Answers every request with a fixed status and body, or never answers.
class CannedServer : public QObject
{
    Q_OBJECT

public:
    int status = 200;
    QByteArray body;
    bool silent = false;
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
                    if (silent) {
                        return; // the client's timeout has to fire
                    }
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
    void close() { m_server.close(); }

private:
    QTcpServer m_server;
};

} // namespace

class TestKtechpitSearch : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesTheDocumentedShape()
    {
        const auto parsed = KtechpitSearch::parse(kServiceJson);
        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->size(), 2); // the object without an id is dropped
        const SearchResult& first = parsed->first();
        QCOMPARE(first.kind, SearchKind::Playlists);
        QCOMPARE(first.id, u"PLOHoVaTp8R7dX"_s);
        QCOMPARE(first.title, u"lofi hip hop radio"_s);
        QCOMPARE(first.channel, u"Lofi Girl"_s);
        QCOMPARE(first.itemCount, 120);
        QCOMPARE(first.url, u"https://www.youtube.com/playlist?list=PLOHoVaTp8R7dX"_s);
        // The large frame is rewritten to the medium one the cards show.
        QCOMPARE(first.thumbnailUrl, u"https://i.ytimg.com/vi/jfKfPfyJRdk/mqdefault.jpg"_s);
        QVERIFY(first.isValid());
        const SearchResult& second = parsed->at(1);
        QCOMPARE(second.itemCount, -1);
        QVERIFY(second.channel.isEmpty());
        QCOMPARE(second.thumbnailUrl, u"https://i.ytimg.com/vi/abc/mqdefault.jpg"_s);
    }

    void rejectsAnythingButAnArrayOfObjects()
    {
        QVERIFY(!KtechpitSearch::parse("{\"error\": \"down\"}").has_value());
        QVERIFY(!KtechpitSearch::parse("<html><body>503</body></html>").has_value());
        QVERIFY(!KtechpitSearch::parse("").has_value());
        QVERIFY(!KtechpitSearch::parse("[1, 2]").has_value());
        QVERIFY(!KtechpitSearch::parse("[{\"playlistId\": \"PL1\"}, \"x\"]").has_value());
    }

    void anEmptyArrayIsEmptyNotBad()
    {
        const auto parsed = KtechpitSearch::parse("[]");
        QVERIFY(parsed.has_value());
        QVERIFY(parsed->isEmpty());
        // Objects without ids count as nothing usable.
        const auto noIds = KtechpitSearch::parse("[{\"title\": \"x\"}]");
        QVERIFY(noIds.has_value());
        QVERIFY(noIds->isEmpty());
    }

    void requestUrlEncodesTheQuery()
    {
        const QUrl url = KtechpitSearch::requestUrl(QUrl(u"https://example.test/api.php"_s), u" lofi hip&hop+ "_s);
        QCOMPARE(QString::fromLatin1(url.toEncoded()), u"https://example.test/api.php?query=lofi%20hip%26hop%2B"_s);
        KtechpitSearch search;
        QCOMPARE(search.endpoint().host(), u"ktechpit.com"_s);
        QCOMPARE(search.endpoint().scheme(), u"https"_s);
        QCOMPARE(search.timeout(), KtechpitSearch::kDefaultTimeoutMs);
    }

    void aGoodAnswerFinishes()
    {
        CannedServer server;
        server.body = kServiceJson;
        QVERIFY(server.listen());
        KtechpitSearch search;
        search.setEndpoint(server.url());
        QSignalSpy finished(&search, &KtechpitSearch::finished);
        QSignalSpy failed(&search, &KtechpitSearch::failed);
        const quint64 id = search.search(u"lofi"_s);
        QVERIFY(search.isSearching());
        QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 5000);
        QVERIFY(failed.isEmpty());
        QCOMPARE(finished.first().at(0).toULongLong(), id);
        QCOMPARE(finished.first().at(1).value<QList<SearchResult>>().size(), 2);
        QVERIFY(!search.isSearching());
    }

    void emptyAndBadBodiesAreOutcomes()
    {
        CannedServer server;
        QVERIFY(server.listen());
        KtechpitSearch search;
        search.setEndpoint(server.url());
        QSignalSpy failed(&search, &KtechpitSearch::failed);

        server.body = "[]";
        search.search(u"nothing"_s);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 1, 5000);
        QCOMPARE(failed.last().at(1).value<KtechpitSearch::Outcome>(), KtechpitSearch::Outcome::Empty);

        server.body = "<html>maintenance</html>";
        search.search(u"nothing"_s);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 2, 5000);
        QCOMPARE(failed.last().at(1).value<KtechpitSearch::Outcome>(), KtechpitSearch::Outcome::BadPayload);

        server.status = 500;
        server.body = "[]";
        search.search(u"nothing"_s);
        QTRY_COMPARE_WITH_TIMEOUT(failed.size(), 3, 5000);
        QCOMPARE(failed.last().at(1).value<KtechpitSearch::Outcome>(), KtechpitSearch::Outcome::NetworkError);
    }

    void aSilentServerTimesOut()
    {
        CannedServer server;
        server.silent = true;
        QVERIFY(server.listen());
        KtechpitSearch search;
        search.setEndpoint(server.url());
        search.setTimeout(300);
        QCOMPARE(search.timeout(), 300);
        QSignalSpy failed(&search, &KtechpitSearch::failed);
        QSignalSpy finished(&search, &KtechpitSearch::finished);
        const quint64 id = search.search(u"slow"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 5000);
        QVERIFY(finished.isEmpty());
        QCOMPARE(failed.first().at(0).toULongLong(), id);
        QCOMPARE(failed.first().at(1).value<KtechpitSearch::Outcome>(), KtechpitSearch::Outcome::Timeout);
        QVERIFY(!search.isSearching());
    }

    void aRefusedConnectionIsANetworkError()
    {
        CannedServer server;
        QVERIFY(server.listen());
        const QUrl dead = server.url();
        server.close();
        KtechpitSearch search;
        search.setEndpoint(dead);
        QSignalSpy failed(&search, &KtechpitSearch::failed);
        search.search(u"nobody home"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 5000);
        QCOMPARE(failed.first().at(1).value<KtechpitSearch::Outcome>(), KtechpitSearch::Outcome::NetworkError);
    }

    void aNewSearchCancelsTheOldOne()
    {
        CannedServer server;
        server.silent = true;
        QVERIFY(server.listen());
        KtechpitSearch search;
        search.setEndpoint(server.url());
        search.setTimeout(400);
        QSignalSpy failed(&search, &KtechpitSearch::failed);
        const quint64 first = search.search(u"one"_s);
        const quint64 second = search.search(u"two"_s);
        QVERIFY(second > first);
        QTRY_VERIFY_WITH_TIMEOUT(!failed.isEmpty(), 5000);
        QTest::qWait(200);
        // Only the second search reports; the first was cancelled silently.
        QCOMPARE(failed.size(), 1);
        QCOMPARE(failed.first().at(0).toULongLong(), second);
        search.search(u"three"_s);
        search.cancel();
        QVERIFY(!search.isSearching());
        QTest::qWait(500);
        QCOMPARE(failed.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TestKtechpitSearch)
#include "tst_ktechpit_search.moc"
