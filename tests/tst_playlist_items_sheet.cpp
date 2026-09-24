// The playlist items sheet (FEATURES E10): every entry with its state,
// arranging, and the playlist file written in the order shown.

#include "core/downloads/download_job.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/playlist_items_sheet.h"

#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using pldl::ui::PlaylistItemsSheet;

class TestPlaylistItemsSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"sheet.ini"_s));
        m_theme = std::make_unique<ThemeService>(*m_settings);
        m_job = DownloadJob();
        m_job.id = 7;
        m_job.title = u"Mix"_s;
        m_job.options.isPlaylist = true;
        m_job.options.outputDirectory = m_dir->path();
        const QStringList titles{u"Beta"_s, u"Alpha"_s, u"Gamma"_s};
        for (int i = 0; i < titles.size(); ++i) {
            PlaylistEntry e;
            e.id = u"id%1"_s.arg(i);
            e.title = titles.at(i);
            if (i < 2) {
                e.file = m_dir->filePath(u"00%1 - %2.mp4"_s.arg(i + 1).arg(titles.at(i)));
                QFile f(e.file);
                QVERIFY(f.open(QIODevice::WriteOnly));
                m_job.outputFiles << e.file;
            }
            m_job.entries << e;
        }
    }

    void cleanup()
    {
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void listsEveryEntryWithItsState()
    {
        PlaylistItemsSheet sheet(m_job, *m_theme);
        QCOMPARE(sheet.list()->count(), 3);
        QCOMPARE(sheet.downloadedCount(), 2);
        QVERIFY(sheet.list()->item(2)->text().contains(u"not downloaded"_s));
        QVERIFY(!sheet.list()->item(0)->text().contains(u"not downloaded"_s));
        QVERIFY(sheet.playAllButton()->isEnabled());
        // A recorded file that is gone shows as missing.
        DownloadJob gone = m_job;
        QFile::remove(gone.entries.first().file);
        PlaylistItemsSheet sheet2(gone, *m_theme);
        QVERIFY(sheet2.list()->item(0)->text().contains(u"file missing"_s));
        QCOMPARE(sheet2.downloadedCount(), 1);
    }

    void arrangingChangesTheOrderOfTheFile()
    {
        PlaylistItemsSheet sheet(m_job, *m_theme);
        QCOMPARE(sheet.orderedFiles().size(), 2);
        QVERIFY(sheet.orderedFiles().first().endsWith(u"001 - Beta.mp4"_s));
        sheet.sortByName(); // Alpha, Beta, Gamma
        QCOMPARE(sheet.entries().at(0).title, u"Alpha"_s);
        QVERIFY(sheet.orderedFiles().first().endsWith(u"002 - Alpha.mp4"_s));
        sheet.list()->setCurrentRow(2);
        sheet.moveSelected(-1); // Gamma above Beta
        QCOMPARE(sheet.entries().at(1).title, u"Gamma"_s);
        sheet.sortByPlaylistOrder();
        QCOMPARE(sheet.entries().at(0).title, u"Beta"_s);
        sheet.sortByName();
        QVERIFY(sheet.writePlaylistFile());
        QFile written(sheet.playlistFilePath());
        QVERIFY(written.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(written.readAll());
        QVERIFY(text.startsWith(u"#EXTM3U\n#PLAYLIST:Mix\n#EXTINF:-1,Alpha\n002 - Alpha.mp4\n#EXTINF:-1,Beta\n001 - Beta.mp4\n"_s));
        QVERIFY(!text.contains(u"Gamma"_s)); // not downloaded: not in the file
    }

    void nothingDownloadedDisablesPlayAll()
    {
        DownloadJob empty = m_job;
        for (PlaylistEntry& e : empty.entries) {
            e.file.clear();
        }
        empty.outputFiles.clear();
        PlaylistItemsSheet sheet(empty, *m_theme);
        QVERIFY(!sheet.playAllButton()->isEnabled());
        QVERIFY(!sheet.writePlaylistFile());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<ThemeService> m_theme;
    DownloadJob m_job;
};

QTEST_MAIN(TestPlaylistItemsSheet)
#include "tst_playlist_items_sheet.moc"
