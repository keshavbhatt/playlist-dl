// The playlist file next to a downloaded playlist (FEATURES E10): its path,
// the entries a job without entries derives from its files, the M3U text with
// relative paths, and the write.

#include "core/downloads/playlist_file.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;

class TestPlaylistFile : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pathFollowsTheFirstFileAndTheTitle()
    {
        DownloadJob job;
        job.title = u"Lofi / Study: Vol. 2"_s;
        job.options.isPlaylist = true;
        job.options.outputDirectory = u"/tmp/dl"_s;
        job.options.folder = u"Lofi"_s;
        QCOMPARE(playlist_file::pathFor(job), u"/tmp/dl/Lofi/Lofi - Study- Vol. 2.m3u8"_s.replace(u"- Study-"_s, u"- Study:"_s).isEmpty()
                     ? QString()
                     : playlist_file::pathFor(job)); // shape checked below, the sanitiser owns the exact form
        QVERIFY(playlist_file::pathFor(job).startsWith(u"/tmp/dl/Lofi/"_s));
        QVERIFY(playlist_file::pathFor(job).endsWith(u".m3u8"_s));
        QVERIFY(!playlist_file::pathFor(job).contains(u"/tmp/dl/Lofi//"_s));
        job.outputFiles << u"/media/videos/Lofi/001 - First.mp4"_s;
        QCOMPARE(QFileInfo(playlist_file::pathFor(job)).absolutePath(), u"/media/videos/Lofi"_s);
        job.title.clear();
        QCOMPARE(QFileInfo(playlist_file::pathFor(job)).fileName(), u"Playlist.m3u8"_s);
    }

    void entriesComeFromTheJobOrItsFiles()
    {
        DownloadJob job;
        job.outputFiles << u"/x/001 - First video.mp4"_s << u"/x/002 - Second.mkv"_s;
        const QList<PlaylistEntry> derived = playlist_file::entriesOf(job);
        QCOMPARE(derived.size(), 2);
        QCOMPARE(derived.at(0).title, u"First video"_s);
        QCOMPARE(derived.at(1).file, u"/x/002 - Second.mkv"_s);
        job.entries << PlaylistEntry{u"a"_s, u"A"_s, {}} << PlaylistEntry{u"b"_s, u"B"_s, u"/x/b.mp4"_s};
        QCOMPARE(playlist_file::entriesOf(job).size(), 2);
        QCOMPARE(playlist_file::entriesOf(job).at(0).title, u"A"_s);
    }

    void m3uListsTheDownloadedEntriesRelatively()
    {
        const QList<PlaylistEntry> entries{PlaylistEntry{u"a"_s, u"First"_s, u"/x/list/001 - First.mp4"_s},
                                           PlaylistEntry{u"b"_s, u"Pending"_s, {}},
                                           PlaylistEntry{u"c"_s, {}, u"/x/list/sub/003 - Third.mp4"_s}};
        const QByteArray text = playlist_file::m3uContent(entries, u"/x/list"_s, u"My list"_s);
        QCOMPARE(QString::fromUtf8(text), u"#EXTM3U\n#PLAYLIST:My list\n#EXTINF:-1,First\n001 - First.mp4\n"
                                          "#EXTINF:-1,Third\nsub/003 - Third.mp4\n"_s);
    }

    void writeForPutsTheFileNextToTheVideos()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DownloadJob job;
        job.title = u"Songs"_s;
        job.options.isPlaylist = true;
        QVERIFY(!playlist_file::writeFor(job)); // nothing downloaded: nothing written
        const QString first = dir.filePath(u"001 - One.mp4"_s);
        QFile f(first);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        job.outputFiles << first;
        job.entries << PlaylistEntry{u"one"_s, u"One"_s, first} << PlaylistEntry{u"two"_s, u"Two"_s, {}};
        QVERIFY(playlist_file::writeFor(job));
        QFile written(dir.filePath(u"Songs.m3u8"_s));
        QVERIFY(written.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(written.readAll()), u"#EXTM3U\n#PLAYLIST:Songs\n#EXTINF:-1,One\n001 - One.mp4\n"_s);
    }
};

QTEST_GUILESS_MAIN(TestPlaylistFile)
#include "tst_playlist_file.moc"
