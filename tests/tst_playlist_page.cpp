// The Playlist page offscreen: a canned playlist through openInfo renders
// the header and the rows, private and removed entries stay unchecked and
// unselectable, Select all, the range, the filter and the sort drive the
// selection count and the Download button's text, the hover buttons emit
// play and download requests, the skip toggle reads the download folder,
// and the empty, loading and error states show.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "services/media_probe.h"
#include "ui/busy_button.h"
#include "ui/pages/playlist_page.h"
#include "ui/playlist_entry_delegate.h"
#include "ui/playlist_model.h"
#include "ui/range_slider.h"
#include "ui/thumbnail_cache.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

using namespace Qt::StringLiterals;
using namespace pldl::core;
using namespace pldl::ui;
using State = PlaylistPage::State;

namespace {
/// The centre of `action`'s button when the row fills `rect`, from the delegate's own hit test.
QPoint buttonPoint(const QRect& rect, PlaylistEntryDelegate::Action action)
{
    for (int x = rect.right(); x > rect.left(); --x) {
        const QPoint p(x, rect.center().y());
        if (PlaylistEntryDelegate::actionAt(p, rect, false) == action) {
            return QPoint(x - 14, rect.center().y());
        }
    }
    return {};
}
} // namespace

class TestPlaylistPage : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"p.ini"_s));
        m_settings->setDownloadDirectory(m_dir->filePath(u"downloads"_s));
        m_theme = std::make_unique<ThemeService>(*m_settings);
        m_probe = std::make_unique<pldl::services::MediaProbe>();
        m_thumbnails = std::make_unique<ThumbnailCache>();
        m_page = std::make_unique<PlaylistPage>(*m_settings, *m_theme, *m_probe, *m_thumbnails);
        m_page->resize(1000, 640);
        m_page->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_page.get()));
    }

    void cleanup()
    {
        m_page.reset();
        m_thumbnails.reset();
        m_probe.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    void startsIdle()
    {
        QCOMPARE(m_page->state(), State::Idle);
        QVERIFY(m_page->currentUrl().isEmpty());
        QVERIFY(!m_page->downloadButton()->isEnabled());
        QVERIFY(!m_page->copyLinkButton()->isEnabled());
        QVERIFY(m_page->minimumSizeHint().width() <= 968);
    }

    void rendersACannedPlaylist()
    {
        QSignalSpy open(m_page.get(), &PlaylistPage::hasPlaylistChanged);
        m_page->openInfo(PlaylistPage::demoInfo());
        QCOMPARE(m_page->state(), State::Ready);
        QCOMPARE(open.count(), 1);
        QVERIFY(open.at(0).at(0).toBool());
        QCOMPARE(m_page->currentUrl(), QUrl(u"https://www.youtube.com/playlist?list=PLdemo"_s));
        QCOMPARE(m_page->titleLabel()->toolTip(), u"Learn Qt in 12 videos"_s);
        QCOMPARE(m_page->channelLabel()->text(), u"Tutorials"_s);
        QVERIFY(m_page->countLabel()->text().startsWith(u"12 videos, "_s));
        QCOMPARE(m_page->proxy()->rowCount(), 12);
        QCOMPARE(m_page->list()->model()->rowCount(), 12);
        QCOMPARE(m_page->selectedCount(), 10); // two unavailable
        QCOMPARE(m_page->downloadButton()->text(), u"Download 10 videos"_s);
        QVERIFY(m_page->downloadButton()->isEnabled());
        QCOMPARE(m_page->footerLabel()->text(), u"10 of 12 selected"_s);
        QCOMPARE(m_page->selectAllBox()->checkState(), Qt::Checked);
        QCOMPARE(m_page->fromSpin()->value(), 1);
        QCOMPARE(m_page->toSpin()->value(), 12);
        QCOMPARE(m_page->rangeSlider()->maximum(), 12);
        QVERIFY(m_page->list()->isVisible());
    }

    void unavailableEntriesStayOut()
    {
        m_page->openInfo(PlaylistPage::demoInfo());
        PlaylistModel* model = m_page->model();
        QVERIFY(model->isUnavailable(3));
        QVERIFY(model->isUnavailable(7));
        QVERIFY(!model->isChecked(3));
        model->setChecked(3, true);
        QVERIFY(!model->isChecked(3));
        m_page->setAllChecked(true);
        QVERIFY(!model->isChecked(7));
        QCOMPARE(m_page->selectedCount(), 10);
        QVERIFY(!m_page->selectedIndexes().contains(4));
        QVERIFY(!m_page->selectedIndexes().contains(8));
        m_page->setRange(3, 5);
        QCOMPARE(m_page->selectedIndexes(), (QList<int>{3, 5}));
    }

    void selectAllRangeFilterAndSort()
    {
        m_page->openInfo(PlaylistPage::demoInfo());
        QSignalSpy downloads(m_page.get(), &PlaylistPage::downloadRequested);

        // Select all toggles between everything and nothing.
        m_page->selectAllBox()->click();
        QCOMPARE(m_page->selectedCount(), 0);
        QCOMPARE(m_page->downloadButton()->text(), u"Download"_s);
        QVERIFY(!m_page->downloadButton()->isEnabled());
        QCOMPARE(m_page->footerLabel()->text(), u"0 of 12 selected"_s);
        m_page->selectAllBox()->click();
        QCOMPARE(m_page->selectedCount(), 10);
        QCOMPARE(m_page->downloadButton()->text(), u"Download 10 videos"_s);

        // The range: the spins, the slider and the selection agree.
        m_page->fromSpin()->setValue(2);
        m_page->toSpin()->setValue(6);
        QCOMPARE(m_page->selectedIndexes(), (QList<int>{2, 3, 5, 6}));
        QCOMPARE(m_page->rangeSlider()->lower(), 2);
        QCOMPARE(m_page->rangeSlider()->upper(), 6);
        QCOMPARE(m_page->downloadButton()->text(), u"Download 4 videos"_s);
        QCOMPARE(m_page->selectAllBox()->checkState(), Qt::PartiallyChecked);
        // A row click widens it: the spins follow the selection's ends.
        m_page->model()->setChecked(9, true);
        QCOMPARE(m_page->toSpin()->value(), 10);
        QCOMPARE(m_page->rangeSlider()->upper(), 10);
        // A single video's button text is singular.
        m_page->setRange(1, 1);
        QCOMPARE(m_page->downloadButton()->text(), u"Download 1 video"_s);
        m_page->downloadButton()->click();
        QCOMPARE(downloads.count(), 1);
        QCOMPARE(downloads.at(0).at(1).value<QList<int>>(), (QList<int>{1}));
        QCOMPARE(downloads.at(0).at(0).value<MediaInfo>().title, u"Learn Qt in 12 videos"_s);

        // The filter hides rows; Select all acts on what is visible.
        m_page->filterField()->setText(u"QT"_s);
        QCOMPARE(m_page->proxy()->rowCount(), 1);
        m_page->setAllChecked(true);
        QCOMPARE(m_page->selectedCount(), 1);
        m_page->setFilter(u"the"_s);
        QCOMPARE(m_page->proxy()->rowCount(), 2); // "the whole story", "Shipping the app"
        m_page->selectAllBox()->click();
        QCOMPARE(m_page->selectedCount(), 3);
        m_page->setFilter(QString());
        QCOMPARE(m_page->proxy()->rowCount(), 12);

        // Sorting reorders the rows, not the indexes.
        m_page->sortCombo()->setCurrentIndex(1);
        QCOMPARE(m_page->sortOrder(), PlaylistPage::SortOrder::Title);
        QCOMPARE(m_page->proxy()->index(0, 0).data(PlaylistEntryDelegate::IndexRole).toInt(), 8); // no title
        QCOMPARE(m_page->proxy()->index(1, 0).data(PlaylistEntryDelegate::TitleRole).toString(),
                 u"[Private video]"_s);
        m_page->setSortOrder(PlaylistPage::SortOrder::Duration);
        QCOMPARE(m_page->proxy()->index(0, 0).data(PlaylistEntryDelegate::IndexRole).toInt(), 12);
        QCOMPARE(m_page->sortCombo()->currentIndex(), 2);
        m_page->setSortOrder(PlaylistPage::SortOrder::PlaylistOrder);
        QCOMPARE(m_page->proxy()->index(0, 0).data(PlaylistEntryDelegate::IndexRole).toInt(), 1);
        QCOMPARE(m_page->selectedCount(), 3);
    }

    void hoverButtonsPlayAndDownload()
    {
        m_page->openInfo(PlaylistPage::demoInfo());
        QSignalSpy plays(m_page.get(), &PlaylistPage::playRequested);
        QSignalSpy videos(m_page.get(), &PlaylistPage::videoDownloadRequested);
        QListView* list = m_page->list();
        QTest::qWait(50); // the scroll bar settles the viewport width
        const QRect first = list->visualRect(list->model()->index(0, 0));
        const QPoint play = buttonPoint(first, PlaylistEntryDelegate::Action::Play);
        const QPoint download = buttonPoint(first, PlaylistEntryDelegate::Action::Download);
        QVERIFY(!play.isNull());
        QVERIFY(!download.isNull());
        QTest::mouseMove(list->viewport(), play);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, play);
        QTRY_COMPARE(plays.count(), 1);
        QCOMPARE(plays.at(0).at(0).toUrl(), QUrl(u"https://www.youtube.com/watch?v=demo0000001"_s));
        QTest::mouseMove(list->viewport(), download);
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, download);
        QTRY_COMPARE(videos.count(), 1);
        QCOMPARE(videos.at(0).at(0).value<MediaEntry>().id, u"demo0000001"_s);

        // Play all is a watch link inside the list; Copy link says so.
        m_page->playAllButton()->click();
        QCOMPARE(plays.count(), 2);
        QCOMPARE(plays.at(1).at(0).toUrl(), QUrl(u"https://www.youtube.com/watch?v=demo0000001&list=PLdemo"_s));
        QSignalSpy toasts(m_page.get(), &PlaylistPage::toast);
        m_page->copyLinkButton()->click();
        QCOMPARE(toasts.count(), 1);
        QCOMPARE(toasts.at(0).at(0).toString(), u"Link copied"_s);
        QSignalSpy back(m_page.get(), &PlaylistPage::backRequested);
        m_page->backButton()->click();
        QCOMPARE(back.count(), 1);
    }

    void skipsWhatIsInTheFolder()
    {
        // With organising on the playlist lands in its own folder: a file
        // there (named the engine's way) marks the entry downloaded.
        QVERIFY(m_settings->organiseDownloads());
        const QString folder = QDir(m_settings->downloadDirectory()).filePath(u"Learn Qt in 12 videos"_s);
        QVERIFY(QDir().mkpath(folder));
        QFile file(QDir(folder).filePath(u"003 - Layouts that survive a resize.mp4"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QFile byId(QDir(folder).filePath(u"Something [demo0000005].mkv"_s));
        QVERIFY(byId.open(QIODevice::WriteOnly));
        byId.close();

        m_page->openInfo(PlaylistPage::demoInfo());
        QCOMPARE(m_page->expectedFolder(), folder);
        QVERIFY(m_page->model()->isDownloaded(2));
        QVERIFY(m_page->model()->isDownloaded(4));
        QCOMPARE(m_page->model()->downloadedCount(), 2);
        QVERIFY(m_page->skipDownloaded());
        QCOMPARE(m_page->selectedCount(), 8);
        QCOMPARE(m_page->selectAllBox()->checkState(), Qt::Checked);
        m_page->setAllChecked(true);
        QCOMPARE(m_page->selectedCount(), 8);
        m_page->skipBox()->setChecked(false);
        QVERIFY(!m_page->skipDownloaded());
        m_page->setAllChecked(true);
        QCOMPARE(m_page->selectedCount(), 10);
        m_page->setSkipDownloaded(true);
        QCOMPARE(m_page->selectedCount(), 8);
        QVERIFY(m_page->skipBox()->isChecked());
    }

    void emptyLoadingAndErrorStates()
    {
        MediaInfo empty;
        empty.type = MediaInfo::Type::Playlist;
        empty.id = u"PLempty"_s;
        empty.title = u"Nothing here"_s;
        m_page->openInfo(empty);
        QCOMPARE(m_page->state(), State::Empty);
        QCOMPARE(m_page->statusLabel()->text(), u"This playlist has no videos"_s);
        QVERIFY(!m_page->list()->isVisible());
        QVERIFY(!m_page->retryButton()->isVisible());
        QCOMPARE(m_page->countLabel()->text(), u"0 videos"_s);

        // No engine: the page waits for it, header from the search card.
        QSignalSpy engine(m_page.get(), &PlaylistPage::engineNeeded);
        pldl::services::SearchResult known;
        known.title = u"Known from the card"_s;
        known.channel = u"Card channel"_s;
        known.itemCount = 42;
        known.url = u"https://www.youtube.com/playlist?list=PLknown"_s;
        m_page->open(QUrl(known.url), known);
        QCOMPARE(m_page->state(), State::Loading);
        QCOMPARE(engine.count(), 1);
        QCOMPARE(m_page->titleLabel()->toolTip(), u"Known from the card"_s);
        QCOMPARE(m_page->channelLabel()->text(), u"Card channel"_s);
        QCOMPARE(m_page->countLabel()->text(), u"42 videos"_s);
        QCOMPARE(m_page->statusLabel()->text(), u"Reading the playlist"_s);
        QVERIFY(busy::isBusy(m_page->downloadButton()));
        QCOMPARE(m_page->currentUrl(), QUrl(known.url));
        QVERIFY(m_page->copyLinkButton()->isEnabled());

        m_page->showError(u"Something went wrong"_s);
        QCOMPARE(m_page->state(), State::Error);
        QCOMPARE(m_page->statusLabel()->text(), u"Something went wrong"_s);
        QVERIFY(m_page->retryButton()->isVisible());
        QVERIFY(!busy::isBusy(m_page->downloadButton()));
        QVERIFY(!m_page->downloadButton()->isEnabled());
        // Retry asks for the engine again (it is still missing).
        m_page->retryButton()->click();
        QCOMPARE(engine.count(), 2);
        QCOMPARE(m_page->state(), State::Loading);
        // A probe without the engine fails through the probe itself too.
        m_probe->setEnginePaths({});
        QVERIFY(!m_probe->hasEngine());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<ThemeService> m_theme;
    std::unique_ptr<pldl::services::MediaProbe> m_probe;
    std::unique_ptr<ThumbnailCache> m_thumbnails;
    std::unique_ptr<PlaylistPage> m_page;
};

int main(int argc, char* argv[])
{
    QTemporaryDir dataHome;
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", dataHome.filePath(u"cache"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", dataHome.filePath(u"config"_s).toUtf8());
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-playlist-page"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestPlaylistPage test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_playlist_page.moc"
