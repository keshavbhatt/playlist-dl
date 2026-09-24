// The download card delegate offscreen: one card per state paints with the
// state's bar colour, the hover buttons match the state, and actionAt finds
// them where they are painted.

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/download_card_delegate.h"
#include "ui/thumbnail_cache.h"

#include <QApplication>
#include <QImage>
#include <QListView>
#include <QPainter>
#include <QStandardPaths>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pldl::core::DownloadState;
using pldl::ui::DownloadCardDelegate;
using Action = pldl::ui::DownloadCardDelegate::Action;

namespace {
constexpr int kWidth = 600;

pldl::core::DownloadJob canned(quint64 id, DownloadState state, bool file = false)
{
    pldl::core::DownloadJob j;
    j.id = id;
    j.url = u"https://www.youtube.com/watch?v=v%1"_s.arg(id);
    j.title = u"Video %1"_s.arg(id);
    j.uploader = u"Channel"_s;
    j.state = state;
    j.totalBytes = 1000;
    j.downloadedBytes = 500;
    if (file) {
        j.outputFiles << u"/tmp/video.mp4"_s;
    }
    return j;
}
} // namespace

class TestDownloadCardDelegate : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_settings = std::make_unique<pldl::core::Settings>(m_dir->filePath(u"d.ini"_s));
        m_settings->setTheme(pldl::core::Theme::Dark);
        m_theme = std::make_unique<pldl::core::ThemeService>(*m_settings);
        m_thumbnails = std::make_unique<pldl::ui::ThumbnailCache>();
        m_queue = std::make_unique<pldl::core::DownloadQueue>();
        m_view = std::make_unique<QListView>();
        m_view->setModel(m_queue.get());
        m_delegate = std::make_unique<DownloadCardDelegate>(*m_thumbnails, *m_theme);
        m_view->setItemDelegate(m_delegate.get());
    }

    void cleanup()
    {
        m_delegate.reset();
        m_view.reset();
        m_queue.reset();
        m_thumbnails.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    QImage paintRow(int row, bool hovered)
    {
        QImage image(kWidth, DownloadCardDelegate::kCardHeight, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, kWidth, DownloadCardDelegate::kCardHeight);
        option.font = QApplication::font();
        option.widget = m_view.get();
        if (hovered) {
            option.state |= QStyle::State_MouseOver;
        }
        m_delegate->paint(&painter, option, m_queue->index(row));
        return image;
    }

    void paintsEveryStateWithItsBar()
    {
        const QList<DownloadState> states{DownloadState::Queued,    DownloadState::Probing,
                                          DownloadState::Downloading, DownloadState::Processing,
                                          DownloadState::Paused,    DownloadState::Completed,
                                          DownloadState::Failed,    DownloadState::Cancelled};
        QList<pldl::core::DownloadJob> jobs;
        quint64 id = 1;
        for (const DownloadState state : states) {
            jobs << canned(id++, state, state == DownloadState::Completed);
        }
        m_queue->setJobs(jobs);
        QCOMPARE(m_queue->rowCount(), states.size());
        const QSize hint = m_delegate->sizeHint(QStyleOptionViewItem(), m_queue->index(0));
        QCOMPARE(hint.height(), 88);
        for (int row = 0; row < states.size(); ++row) {
            const QImage plain = paintRow(row, false);
            QVERIFY(!plain.isNull());
            // The state bar: 3 px at the thumbnail's left edge, solid in the middle.
            const QColor expected = DownloadCardDelegate::stateColor(states.at(row), true);
            const QColor painted = plain.pixelColor(13, DownloadCardDelegate::kCardHeight / 2);
            QVERIFY2(painted == expected,
                     qPrintable(u"row %1: %2 != %3"_s.arg(row).arg(painted.name(), expected.name())));
            // Hovering paints the buttons: the picture changes at the right end.
            const QImage hovered = paintRow(row, true);
            QVERIFY(hovered != plain);
        }
    }

    void offersTheActionsOfTheState()
    {
        QCOMPARE(DownloadCardDelegate::actionsFor(DownloadState::Downloading, false),
                 (QList<Action>{Action::PauseResume, Action::Cancel}));
        QCOMPARE(DownloadCardDelegate::actionsFor(DownloadState::Paused, false),
                 (QList<Action>{Action::PauseResume, Action::Cancel}));
        QCOMPARE(DownloadCardDelegate::actionsFor(DownloadState::Completed, true),
                 (QList<Action>{Action::Open, Action::ShowInFolder, Action::Remove}));
        QCOMPARE(DownloadCardDelegate::actionsFor(DownloadState::Completed, false), (QList<Action>{Action::Remove}));
        QCOMPARE(DownloadCardDelegate::actionsFor(DownloadState::Failed, false),
                 (QList<Action>{Action::Retry, Action::Remove}));
        QCOMPARE(DownloadCardDelegate::actionLabel(Action::PauseResume, DownloadState::Paused), u"Resume"_s);
        QCOMPARE(DownloadCardDelegate::actionLabel(Action::PauseResume, DownloadState::Downloading), u"Pause"_s);
    }

    void hitTestsTheButtons()
    {
        const QRect rect(0, 0, kWidth, DownloadCardDelegate::kCardHeight);
        const int middle = DownloadCardDelegate::kCardHeight / 2;
        // Buttons are 28 px wide with a 4 px gap, 12 px from the card's right edge.
        const int last = kWidth - 12 - 14;
        const int previous = last - 32;
        const int first = previous - 32;

        const pldl::core::DownloadJob downloading = canned(1, DownloadState::Downloading);
        QCOMPARE(m_delegate->actionAt(QPoint(last, middle), rect, downloading), std::optional(Action::Cancel));
        QCOMPARE(m_delegate->actionAt(QPoint(previous, middle), rect, downloading),
                 std::optional(Action::PauseResume));
        QVERIFY(!m_delegate->actionAt(QPoint(first, middle), rect, downloading));
        QVERIFY(!m_delegate->actionAt(QPoint(200, middle), rect, downloading));
        QVERIFY(!m_delegate->actionAt(QPoint(last, 2), rect, downloading)); // above the buttons

        const pldl::core::DownloadJob completed = canned(2, DownloadState::Completed, true);
        QCOMPARE(m_delegate->actionAt(QPoint(last, middle), rect, completed), std::optional(Action::Remove));
        QCOMPARE(m_delegate->actionAt(QPoint(previous, middle), rect, completed),
                 std::optional(Action::ShowInFolder));
        QCOMPARE(m_delegate->actionAt(QPoint(first, middle), rect, completed), std::optional(Action::Open));

        const pldl::core::DownloadJob failed = canned(3, DownloadState::Failed);
        QCOMPARE(m_delegate->actionAt(QPoint(last, middle), rect, failed), std::optional(Action::Remove));
        QCOMPARE(m_delegate->actionAt(QPoint(previous, middle), rect, failed), std::optional(Action::Retry));

        // The same rects when the row sits lower in the view.
        const QRect lower(0, 300, kWidth, DownloadCardDelegate::kCardHeight);
        QCOMPARE(m_delegate->actionAt(QPoint(last, 300 + middle), lower, failed), std::optional(Action::Remove));
    }

    void describesPlaylistsAndFormats()
    {
        pldl::core::DownloadJob playlist = canned(1, DownloadState::Downloading);
        playlist.options.isPlaylist = true;
        playlist.itemIndex = 3;
        playlist.itemCount = 12;
        playlist.currentItemTitle = u"Signals and slots"_s;
        playlist.options.quality = pldl::core::VideoQuality::Q1080;
        pldl::core::DownloadJob audio = canned(2, DownloadState::Queued);
        audio.options.kind = pldl::core::DownloadOptions::Kind::Audio;
        audio.options.audioFormat = pldl::core::AudioFormat::Mp3;
        m_queue->setJobs({audio, playlist});
        QCOMPARE(m_queue->index(1).data(pldl::core::DownloadQueue::DetailLineRole).toString(),
                 u"Signals and slots, 3 of 12"_s);
        QCOMPARE(m_queue->index(1).data(pldl::core::DownloadQueue::FormatLineRole).toString(), u"1080p, MP4"_s);
        QCOMPARE(m_queue->index(0).data(pldl::core::DownloadQueue::DetailLineRole).toString(), u"Channel"_s);
        QCOMPARE(m_queue->index(0).data(pldl::core::DownloadQueue::FormatLineRole).toString(), u"Audio, MP3"_s);
        QVERIFY(!paintRow(1, false).isNull());
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
    std::unique_ptr<pldl::ui::ThumbnailCache> m_thumbnails;
    std::unique_ptr<pldl::core::DownloadQueue> m_queue;
    std::unique_ptr<QListView> m_view;
    std::unique_ptr<DownloadCardDelegate> m_delegate;
};

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-card-delegate"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestDownloadCardDelegate test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_download_card_delegate.moc"
