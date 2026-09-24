// Search suggestions (FEATURES B5): the endpoint's `[query, [...]]` shape
// parsed, the request debounced and the newest text the only one asked.
// Offline: a local server stands in for the endpoint.

#include "services/search_suggestions.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::services;

namespace {

/// Answers every request with the query it saw echoed into two suggestions.
class EchoServer : public QObject
{
    Q_OBJECT

public:
    QStringList queries;

    bool listen()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                    const QByteArray request = socket->readAll();
                    if (!request.contains("\r\n\r\n")) {
                        return;
                    }
                    const QByteArray line = request.left(request.indexOf("\r\n"));
                    const QByteArray query = QUrl::fromPercentEncoding(line.mid(line.indexOf("q=") + 2)
                                                                           .split(' ')
                                                                           .first())
                                                 .toUtf8();
                    queries << QString::fromUtf8(query);
                    const QByteArray body = "[\"" + query + "\",[\"" + query + " radio\",\"" + query + " mix\"]]";
                    socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
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
        return QUrl(u"http://127.0.0.1:"_s + QString::number(m_server.serverPort()) + u"/complete/search"_s);
    }

private:
    QTcpServer m_server;
};

} // namespace

class TestSearchSuggestions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesTheShape()
    {
        const QStringList list =
            SearchSuggestions::parse("[\"lofi\",[\"lofi hip hop\",\"lofi girl\",\"  \",\"lofi jazz\"]]");
        QCOMPARE(list, (QStringList{u"lofi hip hop"_s, u"lofi girl"_s, u"lofi jazz"_s}));
        // The firefox client sometimes appends more arrays; only the second counts.
        QCOMPARE(SearchSuggestions::parse("[\"a\",[\"b\"],[],{\"x\":1}]"), QStringList{u"b"_s});
        QCOMPARE(SearchSuggestions::parse("[\"a\",[1, \"two\", null]]"), QStringList{u"two"_s});
    }

    void rejectsOtherShapes()
    {
        QVERIFY(SearchSuggestions::parse("").isEmpty());
        QVERIFY(SearchSuggestions::parse("{\"a\": [\"b\"]}").isEmpty());
        QVERIFY(SearchSuggestions::parse("[\"only the query\"]").isEmpty());
        QVERIFY(SearchSuggestions::parse("[\"a\", \"not an array\"]").isEmpty());
        QVERIFY(SearchSuggestions::parse("[\"a\", []]").isEmpty());
        QVERIFY(SearchSuggestions::parse("<html>nope</html>").isEmpty());
    }

    void requestUrlUsesTheJsonClient()
    {
        const QUrl url = SearchSuggestions::requestUrl(QUrl(u"https://example.test/complete/search"_s), u"lo fi&"_s);
        QCOMPARE(QString::fromLatin1(url.toEncoded()),
                 u"https://example.test/complete/search?client=firefox&ds=yt&q=lo%20fi%26"_s);
    }

    void debouncesAndAsksForTheNewestText()
    {
        EchoServer server;
        QVERIFY(server.listen());
        SearchSuggestions suggestions;
        suggestions.setEndpoint(server.url());
        QSignalSpy spy(&suggestions, &SearchSuggestions::suggestions);
        suggestions.request(u"l"_s);
        suggestions.request(u"lo"_s);
        suggestions.request(u"lof"_s);
        QVERIFY(suggestions.isPending());
        QTRY_VERIFY_WITH_TIMEOUT(!spy.isEmpty(), 5000);
        QCOMPARE(server.queries, QStringList{u"lof"_s});
        QCOMPARE(spy.first().at(0).toStringList(), (QStringList{u"lof radio"_s, u"lof mix"_s}));
        QVERIFY(!suggestions.isPending());

        // An empty text cancels: nothing is asked, nothing comes.
        suggestions.request(u"x"_s);
        suggestions.request(u"  "_s);
        QVERIFY(!suggestions.isPending());
        QTest::qWait(SearchSuggestions::kDebounceMs * 2);
        QCOMPARE(spy.size(), 1);
        QCOMPARE(server.queries.size(), 1);
    }

    void aDeadEndpointIsSilent()
    {
        SearchSuggestions suggestions;
        suggestions.setEndpoint(QUrl(u"http://127.0.0.1:1/complete/search"_s));
        QSignalSpy spy(&suggestions, &SearchSuggestions::suggestions);
        suggestions.request(u"lofi"_s);
        QTRY_VERIFY_WITH_TIMEOUT(!suggestions.isPending(), 5000);
        QVERIFY(spy.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSearchSuggestions)
#include "tst_search_suggestions.moc"
