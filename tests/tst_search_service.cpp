// In-app search (FEATURES B5): the engine's flat search output parsed into
// typed results and the argv for a page of each kind. One case goes to the network and only runs with PLDL_NETWORK_TESTS=1.

#include "core/downloads/download_options.h"
#include "core/downloads/engine_spec.h"
#include "services/search_service.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using namespace pldl::services;

namespace {

// One page of `--flat-playlist -J` over `ytsearch:`, trimmed to the fields
// the Search page reads.
const QByteArray kVideosJson =
    "{\n"
    "  \"id\": \"me at the zoo\", \"title\": \"me at the zoo\", \"_type\": \"playlist\",\n"
    "  \"entries\": [\n"
    "    {\"_type\": \"url\", \"ie_key\": \"Youtube\", \"id\": \"jNQXAC9IVRw\",\n"
    "     \"url\": \"https://www.youtube.com/watch?v=jNQXAC9IVRw\",\n"
    "     \"title\": \"Me at the zoo\", \"duration\": 19.0,\n"
    "     \"channel\": \"jawed\", \"uploader\": \"jawed\", \"view_count\": 433630395,\n"
    "     \"upload_date\": \"20050423\",\n"
    "     \"thumbnails\": [{\"url\": \"https://i.ytimg.com/vi/jNQXAC9IVRw/small.jpg\", \"height\": 90, "
    "\"width\": 120},\n"
    "                    {\"url\": \"https://i.ytimg.com/vi/jNQXAC9IVRw/hq.jpg\", \"height\": 270, "
    "\"width\": 480}]},\n"
    "    {\"_type\": \"url\", \"ie_key\": \"Youtube\", \"id\": \"aaaaaaaaaaa\",\n"
    "     \"url\": \"https://www.youtube.com/watch?v=aaaaaaaaaaa\",\n"
    "     \"title\": \"No channel, no views\", \"duration\": null,\n"
    "     \"view_count\": null, \"thumbnails\": []},\n"
    "    {\"_type\": \"url\", \"id\": \"\", \"title\": \"Nothing to open\"}\n"
    "  ]\n"
    "}\n";

const QByteArray kPlaylistsJson =
    "{\n"
    "  \"id\": \"lofi\", \"title\": \"lofi\", \"_type\": \"playlist\",\n"
    "  \"entries\": [\n"
    "    {\"title\": \"Sad Lofi songs\", \"duration\": null, \"timestamp\": null,\n"
    "     \"ie_key\": \"YoutubeTab\", \"id\": \"PL115iZFgSUHaEbv9Why0FV7jvAN4qREdJ\", \"_type\": \"url\",\n"
    "     \"url\": \"https://www.youtube.com/playlist?list=PL115iZFgSUHaEbv9Why0FV7jvAN4qREdJ\",\n"
    "     \"thumbnails\": [{\"url\": \"https://i.ytimg.com/vi/i61nN7hcbPA/hq.jpg\", \"height\": 270, "
    "\"width\": 480}]}\n"
    "  ]\n"
    "}\n";

const QByteArray kChannelsJson =
    "{\n"
    "  \"id\": \"lofi\", \"title\": \"lofi\", \"_type\": \"playlist\",\n"
    "  \"entries\": [\n"
    "    {\"_type\": \"url\", \"url\": \"https://www.youtube.com/channel/UCSJ4gkVC6NrvII8umztf0Ow\",\n"
    "     \"id\": \"UCSJ4gkVC6NrvII8umztf0Ow\", \"ie_key\": \"YoutubeTab\", \"channel\": \"Lofi Girl\",\n"
    "     \"uploader\": \"Lofi Girl\", \"title\": \"Lofi Girl\", \"channel_follower_count\": 15800000,\n"
    "     \"thumbnails\": [{\"url\": \"https://yt3.ggpht.com/abc=s176\", \"height\": 176, \"width\": 176}]}\n"
    "  ]\n"
    "}\n";

} // namespace

class TestSearchService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesVideoResults()
    {
        QString error;
        const QList<SearchResult> results =
            SearchService::parseResults(kVideosJson, SearchKind::Videos, &error);
        QVERIFY(error.isEmpty());
        // The third entry has neither id nor url: it is dropped.
        QCOMPARE(results.size(), 2);

        const SearchResult& first = results.first();
        QCOMPARE(first.id, u"jNQXAC9IVRw"_s);
        QCOMPARE(first.title, u"Me at the zoo"_s);
        QCOMPARE(first.channel, u"jawed"_s);
        QCOMPARE(first.durationSeconds, 19.0);
        QCOMPARE(first.viewCount, 433630395LL);
        QCOMPARE(first.uploadDate, u"20050423"_s);
        QCOMPARE(first.url, u"https://www.youtube.com/watch?v=jNQXAC9IVRw"_s);
        QCOMPARE(first.kind, SearchKind::Videos);
        // The widest thumbnail wins.
        QCOMPARE(first.thumbnailUrl, u"https://i.ytimg.com/vi/jNQXAC9IVRw/hq.jpg"_s);

        // Missing fields stay at their defaults, and a video with no
        // thumbnail still gets one from its id.
        const SearchResult& second = results.at(1);
        QCOMPARE(second.durationSeconds, 0.0);
        QCOMPARE(second.viewCount, -1LL);
        QVERIFY(second.channel.isEmpty());
        QCOMPARE(second.thumbnailUrl, u"https://i.ytimg.com/vi/aaaaaaaaaaa/hqdefault.jpg"_s);
    }

    void parsesPlaylistAndChannelResults()
    {
        const QList<SearchResult> playlists =
            SearchService::parseResults(kPlaylistsJson, SearchKind::Playlists);
        QCOMPARE(playlists.size(), 1);
        QCOMPARE(playlists.first().id, u"PL115iZFgSUHaEbv9Why0FV7jvAN4qREdJ"_s);
        QCOMPARE(playlists.first().title, u"Sad Lofi songs"_s);
        QCOMPARE(playlists.first().url,
                 u"https://www.youtube.com/playlist?list=PL115iZFgSUHaEbv9Why0FV7jvAN4qREdJ"_s);
        QCOMPARE(playlists.first().kind, SearchKind::Playlists);
        // The documented limitation: no duration and no view count on this path.
        QCOMPARE(playlists.first().durationSeconds, 0.0);
        QCOMPARE(playlists.first().viewCount, -1LL);

        const QList<SearchResult> channels = SearchService::parseResults(kChannelsJson, SearchKind::Channels);
        QCOMPARE(channels.size(), 1);
        QCOMPARE(channels.first().id, u"UCSJ4gkVC6NrvII8umztf0Ow"_s);
        QCOMPARE(channels.first().channel, u"Lofi Girl"_s);
        QCOMPARE(channels.first().url, u"https://www.youtube.com/channel/UCSJ4gkVC6NrvII8umztf0Ow"_s);
        QCOMPARE(channels.first().kind, SearchKind::Channels);
    }

    void parsesAPlaylistCountAndBuildsItsArgv()
    {
        QCOMPARE(SearchService::parsePlaylistCount(R"({"playlist_count": 77, "entries": [{}]})"), 77);
        QCOMPARE(SearchService::parsePlaylistCount(R"({"n_entries": 3})"), 3);
        QCOMPARE(SearchService::parsePlaylistCount(R"({"title": "no size"})"), -1);
        QCOMPARE(SearchService::parsePlaylistCount("not json"), -1);
        pldl::core::EnginePaths paths;
        paths.ytdlp = u"/opt/engine"_s;
        const QStringList args = SearchService::countArguments(paths, u"https://www.youtube.com/playlist?list=PLx"_s);
        QVERIFY(args.contains(u"--flat-playlist"_s));
        QVERIFY(args.contains(u"--playlist-items"_s));
        QCOMPARE(args.at(args.indexOf(u"--playlist-items"_s) + 1), u"1"_s);
        QCOMPARE(args.last(), u"https://www.youtube.com/playlist?list=PLx"_s);
    }

    void badJsonIsReported()
    {
        QString error;
        QVERIFY(SearchService::parseResults("not json at all", SearchKind::Videos, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        // A valid document with no entries is simply empty, not an error.
        error.clear();
        QVERIFY(SearchService::parseResults("{\"id\":\"x\",\"entries\":[]}", SearchKind::Videos, &error)
                    .isEmpty());
        QVERIFY(error.isEmpty());
    }

    void buildsTheArgvForEachKindAndPage()
    {
        EnginePaths paths;
        paths.ytdlp = u"/opt/pldl/yt-dlp"_s;

        QStringList args =
            SearchService::searchArguments(paths, u"me at the zoo"_s, SearchKind::Videos, 0, 20);
        QVERIFY(args.contains(u"--flat-playlist"_s));
        QVERIFY(args.contains(u"--dump-single-json"_s));
        QCOMPARE(args.last(), u"ytsearch20:me at the zoo"_s);
        QCOMPARE(args.at(args.size() - 2), u"--"_s);
        QCOMPARE(valueAfter(args, u"--playlist-items"_s), u"1:20"_s);

        // Load more: the next twenty of the same query.
        args = SearchService::searchArguments(paths, u"me at the zoo"_s, SearchKind::Videos, 1, 20);
        QCOMPARE(valueAfter(args, u"--playlist-items"_s), u"21:40"_s);
        QCOMPARE(args.last(), u"ytsearch40:me at the zoo"_s);

        // Playlists and channels go through the site's filtered search page.
        args = SearchService::searchArguments(paths, u"lo fi"_s, SearchKind::Playlists, 0, 5);
        QCOMPARE(args.last(), u"https://www.youtube.com/results?search_query=lo%20fi&sp=EgIQAw%3D%3D"_s);
        QCOMPARE(valueAfter(args, u"--playlist-items"_s), u"1:5"_s);
        args = SearchService::searchArguments(paths, u"lo fi"_s, SearchKind::Channels, 2, 5);
        QCOMPARE(args.last(), u"https://www.youtube.com/results?search_query=lo%20fi&sp=EgIQAg%3D%3D"_s);
        QCOMPARE(valueAfter(args, u"--playlist-items"_s), u"11:15"_s);

        QCOMPARE(searchKindName(SearchKind::Videos), u"videos"_s);
        QCOMPARE(searchKindName(SearchKind::Playlists), u"playlists"_s);
        QCOMPARE(searchKindName(SearchKind::Channels), u"channels"_s);
    }

    void withoutAnEngineItFailsAtOnce()
    {
        SearchService service;
        QVERIFY(!service.hasEngine());
        QSignalSpy failedSpy(&service, &SearchService::failed);
        const quint64 id = service.search(u"anything"_s, SearchKind::Videos);
        QTRY_VERIFY_WITH_TIMEOUT(!failedSpy.isEmpty(), 5000);
        QCOMPARE(failedSpy.first().at(0).toULongLong(), id);
        QVERIFY(!failedSpy.first().at(1).toString().contains(u"yt-dlp"_s, Qt::CaseInsensitive));

        // An empty query answers with nothing rather than starting anything.
        QSignalSpy finishedSpy(&service, &SearchService::finished);
        service.search(u"   "_s, SearchKind::Videos);
        QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty(), 5000);
        QVERIFY(!service.isSearching());
    }

    void networkSearch()
    {
        if (qEnvironmentVariable("PLDL_NETWORK_TESTS") != u"1"_s) {
            QSKIP("set PLDL_NETWORK_TESTS=1 to run the live search");
        }
        const QString engine = QStandardPaths::findExecutable(u"yt-dlp"_s);
        if (engine.isEmpty()) {
            QSKIP("no download engine on this machine");
        }
        EnginePaths paths;
        paths.ytdlp = engine;
        SearchService service;
        service.setEnginePaths(paths);
        service.setPageSize(5);

        QSignalSpy finishedSpy(&service, &SearchService::finished);
        QSignalSpy failedSpy(&service, &SearchService::failed);
        service.search(u"me at the zoo"_s, SearchKind::Videos);
        QTRY_VERIFY_WITH_TIMEOUT(!finishedSpy.isEmpty() || !failedSpy.isEmpty(), 90000);
        if (!failedSpy.isEmpty()) {
            QSKIP(qPrintable(u"the search did not answer: "_s + failedSpy.first().at(1).toString()));
        }
        const auto results = finishedSpy.first().at(1).value<QList<SearchResult>>();
        QVERIFY(!results.isEmpty());
        bool found = false;
        for (const SearchResult& result : results) {
            qInfo().noquote() << result.id << result.title << result.channel << result.durationSeconds;
            if (result.id == u"jNQXAC9IVRw"_s) {
                found = true;
                QCOMPARE(result.title, u"Me at the zoo"_s);
                QVERIFY(result.durationSeconds > 0);
                QVERIFY(!result.thumbnailUrl.isEmpty());
                QVERIFY(result.url.contains(u"jNQXAC9IVRw"_s));
            }
        }
        QVERIFY(found);
    }

private:
    static QString valueAfter(const QStringList& args, const QString& flag)
    {
        const qsizetype index = args.indexOf(flag);
        if (index < 0 || index + 1 >= args.size()) {
            return {};
        }
        return args.at(index + 1);
    }
};

QTEST_MAIN(TestSearchService)
#include "tst_search_service.moc"
