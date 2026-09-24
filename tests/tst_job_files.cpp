// What a download left on disk and its removal (FEATURES E3).

#include "core/downloads/job_files.h"
#include "core/downloads/playlist_file.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;

class TestJobFiles : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void listsEachExistingFileOnce()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DownloadJob job;
        job.title = u"Mix"_s;
        job.options.isPlaylist = true;
        job.options.outputDirectory = dir.path();
        job.options.folder = u"Mix"_s;
        QDir().mkpath(dir.filePath(u"Mix"_s));
        const QString a = dir.filePath(u"Mix/001 - A.opus"_s);
        const QString b = dir.filePath(u"Mix/002 - B.opus"_s);
        for (const QString& f : {a, b}) {
            QFile file(f);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        job.outputFiles << a << b << dir.filePath(u"Mix/gone.opus"_s);
        job.entries << PlaylistEntry{u"a"_s, u"A"_s, a} << PlaylistEntry{u"c"_s, u"C"_s, {}};
        QVERIFY(playlist_file::writeFor(job));
        const QStringList files = job_files::existingFiles(job);
        QCOMPARE(files.size(), 3); // a, b and the playlist file; the missing one and the duplicate not
        QVERIFY(files.contains(a));
        QVERIFY(files.contains(playlist_file::pathFor(job)));
    }

    void removesTheFilesAndAnEmptiedOwnFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DownloadJob job;
        job.title = u"Mix"_s;
        job.options.isPlaylist = true;
        job.options.outputDirectory = dir.path();
        job.options.folder = u"Mix"_s;
        QDir().mkpath(dir.filePath(u"Mix"_s));
        const QString a = dir.filePath(u"Mix/001 - A.opus"_s);
        QFile file(a);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        job.outputFiles << a;
        QVERIFY(playlist_file::writeFor(job));
        const job_files::Removal removal = job_files::removeJobFiles(job);
        QCOMPARE(removal.filesRemoved, 2);
        QCOMPARE(removal.filesFailed, 0);
        QVERIFY(removal.folderRemoved);
        QVERIFY(!QDir(dir.filePath(u"Mix"_s)).exists());
        QVERIFY(QDir(dir.path()).exists()); // the download folder itself stays

        // A folder with something else in it stays.
        QDir().mkpath(dir.filePath(u"Mix"_s));
        QFile other(dir.filePath(u"Mix/notes.txt"_s));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.close();
        QFile again(a);
        QVERIFY(again.open(QIODevice::WriteOnly));
        again.close();
        const job_files::Removal second = job_files::removeJobFiles(job);
        QCOMPARE(second.filesRemoved, 1);
        QVERIFY(!second.folderRemoved);
        QVERIFY(QFile::exists(dir.filePath(u"Mix/notes.txt"_s)));
    }

    void aSingleVideoNeverRemovesTheFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        DownloadJob job;
        job.options.outputDirectory = dir.path();
        const QString a = dir.filePath(u"video.mp4"_s);
        QFile file(a);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        job.outputFiles << a;
        const job_files::Removal removal = job_files::removeJobFiles(job);
        QCOMPARE(removal.filesRemoved, 1);
        QVERIFY(!removal.folderRemoved);
        QVERIFY(QDir(dir.path()).exists());
    }
};

QTEST_GUILESS_MAIN(TestJobFiles)
#include "tst_job_files.moc"
