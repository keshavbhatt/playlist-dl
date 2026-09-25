// The playlist entry delegate offscreen: every state paints (checked,
// unchecked, hovered, unavailable, downloaded, focused), the title rules
// for unavailable entries, the hit tests for the two hover buttons, and a
// click on the row toggling its check through the model.

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/pages/playlist_page.h"
#include "ui/playlist_entry_delegate.h"
#include "ui/playlist_model.h"
#include "ui/thumbnail_cache.h"

#include <QApplication>
#include <QImage>
#include <QListView>
#include <QPainter>
#include <QSignalSpy>
#include <QStyleOptionViewItem>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using pldl::ui::PlaylistEntryDelegate;
using Action = pldl::ui::PlaylistEntryDelegate::Action;

namespace {
constexpr int kWidth = 700;

/// The centre of `action`'s button when the row fills `rect`, from the delegate's own hit test.
QPoint buttonPoint(const QRect& rect, Action action)
{
    for (int x = rect.right(); x > rect.left(); --x) {
        const QPoint p(x, rect.center().y());
        if (PlaylistEntryDelegate::actionAt(p, rect, false) == action) {
            return QPoint(x - 14, rect.center().y()); // 28 px buttons: the centre is 14 in from the right edge
        }
    }
    return {};
}

bool hasInk(const QImage& image, const QRect& area)
{
    const QRgb first = image.pixel(area.topLeft());
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (image.pixel(x, y) != first) {
                return true;
            }
        }
    }
    return false;
}
} // namespace

class TestPlaylistEntryDelegate : public QObject
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
        m_model = std::make_unique<pldl::ui::PlaylistModel>();
        m_model->setEntries(pldl::ui::PlaylistPage::demoInfo().entries, {u"003 - Layouts that survive a resize.mp4"_s},
                            true);
        m_view = std::make_unique<QListView>();
        m_view->setModel(m_model.get());
        m_delegate = std::make_unique<PlaylistEntryDelegate>(*m_thumbnails, *m_theme);
        m_view->setItemDelegate(m_delegate.get());
        m_view->setMouseTracking(true);
        m_view->setSelectionMode(QAbstractItemView::NoSelection);
        m_view->resize(kWidth, 500);
    }

    void cleanup()
    {
        m_delegate.reset();
        m_view.reset();
        m_model.reset();
        m_thumbnails.reset();
        m_theme.reset();
        m_settings.reset();
        m_dir.reset();
    }

    QImage paintRow(int row, QStyle::State extra)
    {
        QImage image(kWidth, PlaylistEntryDelegate::kRowHeight, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.initFrom(m_view.get());
        option.rect = QRect(0, 0, kWidth, PlaylistEntryDelegate::kRowHeight);
        option.state |= extra;
        m_delegate->paint(&painter, option, m_model->index(row, 0));
        painter.end();
        return image;
    }

    void titleRules()
    {
        QVERIFY(PlaylistEntryDelegate::isUnavailableTitle(u"[Private video]"_s));
        QVERIFY(PlaylistEntryDelegate::isUnavailableTitle(u"[Deleted video]"_s));
        QVERIFY(PlaylistEntryDelegate::isUnavailableTitle(u" [deleted video] "_s));
        QVERIFY(!PlaylistEntryDelegate::isUnavailableTitle(u"A video about [Private video] tags"_s));
        QCOMPARE(PlaylistEntryDelegate::unavailableLabel(u"[Private video]"_s), u"Private"_s);
        QCOMPARE(PlaylistEntryDelegate::unavailableLabel(u"[Deleted video]"_s), u"Removed"_s);
        QVERIFY(PlaylistEntryDelegate::isUnavailableTitle(QString()));
        QVERIFY(PlaylistEntryDelegate::isUnavailableTitle(u"   "_s));
        QCOMPARE(PlaylistEntryDelegate::unavailableLabel(QString()), u"Unavailable"_s);
        // The whole entry: no title on a YouTube link is a removed video; no
        // title on another site's link is a plain item (a SoundCloud set read flat).
        pldl::core::MediaEntry gone;
        gone.url = u"https://www.youtube.com/watch?v=abc"_s;
        QVERIFY(PlaylistEntryDelegate::isUnavailableEntry(gone));
        pldl::core::MediaEntry track;
        track.url = u"https://api-v2.soundcloud.com/tracks/166237090"_s;
        QVERIFY(!PlaylistEntryDelegate::isUnavailableEntry(track));
        track.title = u"[Private video]"_s;
        QVERIFY(PlaylistEntryDelegate::isUnavailableEntry(track));
        QVERIFY(PlaylistEntryDelegate::isUnavailableEntry(pldl::core::MediaEntry{}));
        QVERIFY(PlaylistEntryDelegate::unavailableLabel(u"Fine"_s).isEmpty());
    }

    void paintsEveryState()
    {
        // Rows 0 (checked), 2 (downloaded, unchecked), 3 (private), 7 (no title).
        for (const int row : {0, 2, 3, 7}) {
            for (const QStyle::State extra : {QStyle::State(QStyle::State_None), QStyle::State(QStyle::State_MouseOver),
                                              QStyle::State(QStyle::State_HasFocus)}) {
                const QImage image = paintRow(row, extra);
                QVERIFY2(hasInk(image, QRect(0, 0, kWidth, PlaylistEntryDelegate::kRowHeight)),
                         qPrintable(u"row %1 painted nothing"_s.arg(row)));
                // The check box area carries the accent fill only when checked.
                const QRect box = PlaylistEntryDelegate::checkRect(QRect(0, 0, kWidth, PlaylistEntryDelegate::kRowHeight));
                QCOMPARE(hasInk(image, box.adjusted(5, 5, -5, -5)), m_model->isChecked(row));
            }
        }
        // Hover paints the buttons at the right end for an available row, never for an unavailable one.
        const QRect buttons(kWidth - 12 - 2 * 28 - 4, 20, 2 * 28 + 4, 32);
        QVERIFY(hasInk(paintRow(0, QStyle::State_MouseOver), buttons));
        QVERIFY(!hasInk(paintRow(3, QStyle::State_MouseOver), buttons));
        QVERIFY(!hasInk(paintRow(0, QStyle::State_None), buttons));
    }

    void hitTestsTheButtons()
    {
        const QRect row(0, 100, kWidth, PlaylistEntryDelegate::kRowHeight);
        const QPoint download(kWidth - 12 - 14, 100 + 36);
        const QPoint play(kWidth - 12 - 28 - 4 - 14, 100 + 36);
        QCOMPARE(PlaylistEntryDelegate::actionAt(download, row, false), std::optional<Action>(Action::Download));
        QCOMPARE(PlaylistEntryDelegate::actionAt(play, row, false), std::optional<Action>(Action::Play));
        QVERIFY(!PlaylistEntryDelegate::actionAt(QPoint(200, 136), row, false));
        QVERIFY(!PlaylistEntryDelegate::actionAt(play, row, true));
        QCOMPARE(PlaylistEntryDelegate::actionLabel(Action::Play), u"Play"_s);
        QCOMPARE(PlaylistEntryDelegate::actionLabel(Action::Download), u"Download this item"_s);
    }

    void clicksToggleAndTrigger()
    {
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view.get()));
        QTest::qWait(50); // the scroll bar settles the viewport width
        const QRect first = m_view->visualRect(m_model->index(0, 0));
        QVERIFY(m_model->isChecked(0));
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, first.center());
        QVERIFY(!m_model->isChecked(0));
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, first.center());
        QVERIFY(m_model->isChecked(0));

        QSignalSpy actions(m_delegate.get(), &PlaylistEntryDelegate::actionTriggered);
        const QPoint play = buttonPoint(m_view->visualRect(m_model->index(0, 0)), Action::Play);
        QVERIFY(!play.isNull());
        QTest::mouseMove(m_view->viewport(), play);
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, play);
        QTRY_COMPARE(actions.count(), 1);
        QCOMPARE(actions.at(0).at(1).value<Action>(), Action::Play);
        QCOMPARE(actions.at(0).at(0).toModelIndex().row(), 0);
        QVERIFY(m_model->isChecked(0)); // the button click did not toggle the row

        // The private row takes no click at all.
        const QRect privateRow = m_view->visualRect(m_model->index(3, 0));
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, privateRow.center());
        QVERIFY(!m_model->isChecked(3));
        const QPoint privatePlay = buttonPoint(privateRow, Action::Play);
        QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::NoModifier, privatePlay);
        QTest::qWait(20);
        QCOMPARE(actions.count(), 1);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<pldl::core::Settings> m_settings;
    std::unique_ptr<pldl::core::ThemeService> m_theme;
    std::unique_ptr<pldl::ui::ThumbnailCache> m_thumbnails;
    std::unique_ptr<pldl::ui::PlaylistModel> m_model;
    std::unique_ptr<QListView> m_view;
    std::unique_ptr<PlaylistEntryDelegate> m_delegate;
};

int main(int argc, char* argv[])
{
    QTemporaryDir dataHome;
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", dataHome.filePath(u"cache"_s).toUtf8());
    qputenv("XDG_CONFIG_HOME", dataHome.filePath(u"config"_s).toUtf8());
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"pldl-playlist-entry-delegate"_s);
    QApplication::setOrganizationName(u"ktechpit"_s);
    TestPlaylistEntryDelegate test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_playlist_entry_delegate.moc"
