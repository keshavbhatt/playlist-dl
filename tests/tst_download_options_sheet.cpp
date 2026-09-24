// The Download options sheet offscreen: the pure item spec and folder name
// helpers, the job built for a playlist selection (options, folder,
// numbering, playlist items) and for a single video, the kind switch
// changing the button text and the option group, and accept writing every
// choice back to the settings as the next default (FEATURES O1 to O5).

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/download_options_sheet.h"
#include "ui/kind_card.h"
#include "ui/pages/playlist_page.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using pldl::ui::DownloadOptionsSheet;
using Kind = DownloadOptions::Kind;

class TestDownloadOptionsSheet : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"s.ini"_s));
        m_settings->setDownloadDirectory(u"/tmp/pldl-sheet"_s);
        m_theme = std::make_unique<ThemeService>(*m_settings);
        m_sheet = std::make_unique<DownloadOptionsSheet>(*m_settings, *m_theme);
    }

    void cleanup()
    {
        m_sheet.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void itemSpec()
    {
        QVERIFY(DownloadOptionsSheet::itemSpec({}).isEmpty());
        QCOMPARE(DownloadOptionsSheet::itemSpec({1, 2, 3, 7, 9, 10, 11, 12}), u"1-3,7,9-12"_s);
        QCOMPARE(DownloadOptionsSheet::itemSpec({5, 3, 3, 1}), u"1,3,5"_s);
        QCOMPARE(DownloadOptionsSheet::itemSpec({0, -1, 2}), u"2"_s);
        QCOMPARE(DownloadOptionsSheet::itemSpec({4, 5}), u"4-5"_s);
        QCOMPARE(DownloadOptionsSheet::sanitiseFolderName(u"A/B: C"_s), u"A_B_ C"_s);
        QCOMPARE(DownloadOptionsSheet::sanitiseFolderName(QString()), u"Playlist"_s);
    }

    void startsFromTheDefaults()
    {
        QCOMPARE(m_sheet->width() <= DownloadOptionsSheet::kWidth, true);
        QCOMPARE(m_sheet->maximumWidth(), DownloadOptionsSheet::kWidth);
        QCOMPARE(m_sheet->kind(), Kind::Video);
        QVERIFY(m_sheet->videoCard()->isChecked());
        QCOMPARE(m_sheet->groups()->currentIndex(), 0);
        QCOMPARE(m_sheet->qualityCombo()->currentText(), u"Best available"_s);
        QCOMPARE(m_sheet->qualityCombo()->count(), 7);
        QCOMPARE(m_sheet->containerCombo()->currentText(), u"MP4"_s);
        QCOMPARE(m_sheet->subtitlesCombo()->currentText(), u"None"_s);
        QVERIFY(!m_sheet->embedSubtitlesBox()->isEnabled());
        QVERIFY(m_sheet->embedThumbnailBox()->isChecked());
        QVERIFY(m_sheet->embedMetadataBox()->isChecked());
        QCOMPARE(m_sheet->audioFormatCombo()->count(), 6);
        QCOMPARE(m_sheet->audioQualityCombo()->currentData().toInt(), 0);
        QVERIFY(m_sheet->ownFolderBox()->isChecked());
        QVERIFY(m_sheet->numberBox()->isChecked());

        // The last kind used opens the sheet.
        m_settings->setLastDownloadKind(DownloadKind::Audio);
        DownloadOptionsSheet again(*m_settings, *m_theme);
        QCOMPARE(again.kind(), Kind::Audio);
        QCOMPARE(again.groups()->currentIndex(), 1);
    }

    void buildsThePlaylistJob()
    {
        const MediaInfo info = pldl::ui::PlaylistPage::demoInfo();
        m_sheet->setPlaylist(info, {6, 1, 2, 3, 5});
        QVERIFY(m_sheet->isPlaylist());
        QCOMPARE(m_sheet->downloadButton()->text(), u"Download 5 videos"_s);
        QVERIFY(m_sheet->ownFolderBox()->isVisibleTo(m_sheet.get()));
        QVERIFY(m_sheet->numberBox()->isVisibleTo(m_sheet.get()));
        QVERIFY(m_sheet->folderLabel()->toolTip().endsWith(u"/pldl-sheet/Learn Qt in 12 videos"_s));
        QVERIFY(m_sheet->previewLabel()->toolTip().contains(u"Learn Qt in 12 videos/001 - Getting started"_s));
        QVERIFY(m_sheet->previewLabel()->toolTip().endsWith(u".mp4"_s));

        m_sheet->qualityCombo()->setCurrentIndex(4); // 720p
        m_sheet->containerCombo()->setCurrentIndex(1); // MKV
        m_sheet->subtitlesCombo()->setCurrentIndex(1); // en
        QVERIFY(m_sheet->embedSubtitlesBox()->isEnabled());
        DownloadJob job = m_sheet->job();
        QCOMPARE(job.url, u"https://www.youtube.com/playlist?list=PLdemo"_s);
        QVERIFY(job.videoId.isEmpty());
        QCOMPARE(job.title, u"Learn Qt in 12 videos"_s);
        QCOMPARE(job.uploader, u"Tutorials"_s);
        QCOMPARE(job.itemCount, 5);
        QCOMPARE(job.thumbnail, u"https://i.ytimg.com/vi/demo0000001/mqdefault.jpg"_s);
        QCOMPARE(job.duration, 5 * 300.0 + (1 + 2 + 3 + 5 + 6) * 95.0);
        QVERIFY(job.isPlaylist());
        QCOMPARE(job.options.kind, Kind::Video);
        QCOMPARE(job.options.quality, VideoQuality::Q720);
        QCOMPARE(job.options.container, Container::Mkv);
        QCOMPARE(job.options.subtitleLanguages, QStringList{u"en"_s});
        QVERIFY(job.options.embedSubtitles);
        QCOMPARE(job.options.playlistItems, u"1-3,5-6"_s);
        QCOMPARE(job.options.outputDirectory, u"/tmp/pldl-sheet"_s);
        QCOMPARE(job.options.folder, u"Learn Qt in 12 videos"_s);
        QVERIFY(!job.options.playlistSubfolder);
        QVERIFY(job.options.numberPlaylistItems);
        QCOMPARE(outputTemplate(job.options), u"Learn Qt in 12 videos/%(playlist_index|0)03d - %(title)s.%(ext)s"_s);
        QVERIFY(m_sheet->previewLabel()->toolTip().endsWith(u".mkv"_s));

        // The playlist toggles.
        m_sheet->ownFolderBox()->setChecked(false);
        m_sheet->numberBox()->setChecked(false);
        job = m_sheet->job();
        QVERIFY(job.options.folder.isEmpty());
        QVERIFY(!job.options.numberPlaylistItems);
        QCOMPARE(outputTemplate(job.options), u"%(title)s.%(ext)s"_s);
        QVERIFY(m_sheet->folderLabel()->toolTip().endsWith(u"/pldl-sheet"_s));
        QVERIFY(!m_sheet->previewLabel()->toolTip().contains(u"001 - "_s));
        m_sheet->setFolder(u"/tmp/elsewhere"_s);
        QCOMPARE(m_sheet->job().options.outputDirectory, u"/tmp/elsewhere"_s);

        // The sheet is as tall as its contents: shorter for one video (no playlist toggles).
        const int playlistHeight = m_sheet->height();
        m_sheet->setVideo(info.entries.first(), QUrl(info.entries.first().url));
        QVERIFY(m_sheet->height() < playlistHeight);
        m_sheet->setPlaylist(info, {1});
        QCOMPARE(m_sheet->height(), playlistHeight);
    }

    void buildsTheVideoJob()
    {
        MediaEntry entry = pldl::ui::PlaylistPage::demoInfo().entries.at(1);
        entry.thumbnail.clear();
        m_sheet->setVideo(entry, QUrl(entry.url));
        QVERIFY(!m_sheet->isPlaylist());
        QCOMPARE(m_sheet->downloadButton()->text(), u"Download video"_s);
        QVERIFY(!m_sheet->ownFolderBox()->isVisibleTo(m_sheet.get()));
        QVERIFY(!m_sheet->numberBox()->isVisibleTo(m_sheet.get()));
        DownloadJob job = m_sheet->job();
        QCOMPARE(job.url, entry.url);
        QCOMPARE(job.videoId, u"demo0000002"_s);
        QCOMPARE(job.title, entry.title);
        QCOMPARE(job.thumbnail, u"https://i.ytimg.com/vi/demo0000002/mqdefault.jpg"_s);
        QCOMPARE(job.duration, entry.duration);
        QVERIFY(!job.isPlaylist());
        QVERIFY(job.options.playlistItems.isEmpty());
        QCOMPARE(job.options.folder, u"Videos"_s); // organising on: the kind's folder
        QVERIFY(m_sheet->folderLabel()->toolTip().endsWith(u"/pldl-sheet/Videos"_s));

        // The kind switch: the group, the button, the folder, the preview.
        m_sheet->audioCard()->click();
        QCOMPARE(m_sheet->kind(), Kind::Audio);
        QCOMPARE(m_sheet->groups()->currentIndex(), 1);
        QCOMPARE(m_sheet->downloadButton()->text(), u"Download audio"_s);
        m_sheet->audioFormatCombo()->setCurrentIndex(1); // MP3
        m_sheet->audioQualityCombo()->setCurrentIndex(1); // 192
        m_sheet->embedCoverBox()->setChecked(false);
        job = m_sheet->job();
        QCOMPARE(job.options.kind, Kind::Audio);
        QCOMPARE(job.options.audioFormat, AudioFormat::Mp3);
        QCOMPARE(job.options.audioBitrateKbps, 192);
        QVERIFY(!job.options.embedThumbnail);
        QVERIFY(job.options.subtitleLanguages.isEmpty());
        QCOMPARE(job.options.folder, u"Music"_s);
        QVERIFY(m_sheet->previewLabel()->toolTip().endsWith(u".mp3"_s));
        m_sheet->setKind(Kind::Video);
        QCOMPARE(m_sheet->groups()->currentIndex(), 0);
        QCOMPARE(m_sheet->downloadButton()->text(), u"Download video"_s);
        m_settings->setOrganiseDownloads(false);
        QVERIFY(m_sheet->job().options.folder.isEmpty());
    }

    void acceptWritesTheDefaults()
    {
        m_sheet->setPlaylist(pldl::ui::PlaylistPage::demoInfo(), {1, 2});
        QSignalSpy accepted(m_sheet.get(), &QDialog::accepted);
        m_sheet->qualityCombo()->setCurrentIndex(3); // 1080p
        m_sheet->containerCombo()->setCurrentIndex(2); // WebM
        m_sheet->subtitlesCombo()->setCurrentIndex(1); // en
        m_sheet->embedThumbnailBox()->setChecked(false);
        m_sheet->numberBox()->setChecked(false);
        m_sheet->ownFolderBox()->setChecked(false);
        m_sheet->setFolder(u"/tmp/pldl-new"_s);
        m_sheet->downloadButton()->click();
        QCOMPARE(accepted.count(), 1);
        QCOMPARE(m_settings->lastDownloadKind(), DownloadKind::Video);
        QCOMPARE(m_settings->defaultQuality(), VideoQuality::Q1080);
        QCOMPARE(m_settings->defaultContainer(), Container::Webm);
        QCOMPARE(m_settings->subtitleLanguages(), QStringList{u"en"_s});
        QVERIFY(!m_settings->embedThumbnail());
        QVERIFY(m_settings->embedMetadata());
        QVERIFY(!m_settings->numberPlaylistFiles());
        QVERIFY(!m_settings->organiseDownloads());
        QCOMPARE(m_settings->downloadDirectory(), u"/tmp/pldl-new"_s);

        // Audio choices become the audio defaults; the video ones are kept.
        DownloadOptionsSheet audio(*m_settings, *m_theme);
        audio.setVideo(pldl::ui::PlaylistPage::demoInfo().entries.first(), QUrl(u"https://youtu.be/demo0000001"_s));
        QVERIFY(!audio.numberBox()->isChecked());
        QCOMPARE(audio.qualityCombo()->currentData().toInt(), static_cast<int>(VideoQuality::Q1080));
        audio.setKind(Kind::Audio);
        audio.audioFormatCombo()->setCurrentIndex(4); // FLAC
        audio.audioQualityCombo()->setCurrentIndex(2); // 128
        audio.accept();
        QCOMPARE(m_settings->lastDownloadKind(), DownloadKind::Audio);
        QCOMPARE(m_settings->defaultAudioFormat(), AudioFormat::Flac);
        QCOMPARE(m_settings->defaultAudioBitrate(), 128);
        QCOMPARE(m_settings->defaultQuality(), VideoQuality::Q1080);
        QCOMPARE(m_settings->subtitleLanguages(), QStringList{u"en"_s});
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<ThemeService> m_theme;
    std::unique_ptr<DownloadOptionsSheet> m_sheet;
};

int main(int argc, char* argv[])
{
    QTemporaryDir dataHome;
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", dataHome.filePath(u"cache"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", dataHome.filePath(u"config"_s).toUtf8());
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-download-options-sheet"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestDownloadOptionsSheet test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_download_options_sheet.moc"
