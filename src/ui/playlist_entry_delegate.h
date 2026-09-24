#pragma once

#include "core/downloads/media_info.h"

#include <QPersistentModelIndex>
#include <QPoint>
#include <QRect>
#include <QStyledItemDelegate>

#include <optional>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class ThumbnailCache;
struct Tokens;

/// Paints one playlist entry as a 72 px row (DESIGN.md section 3, Playlist):
/// the check box, the playlist index, the thumbnail, the title on two lines,
/// the channel and duration line, and on hover the play and download glyph
/// buttons. Unavailable entries (private, removed) are muted with a badge and
/// take no clicks; entries already in the download folder carry a
/// "Downloaded" badge. A click on the row toggles its check state through the
/// model; the buttons are reported through actionTriggered.
class PlaylistEntryDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlaylistEntryDelegate)

public:
    enum Role
    {
        IndexRole = Qt::UserRole + 1, ///< int, the 1-based playlist index
        IdRole,                       ///< the video id
        TitleRole,
        UploaderRole,
        ThumbnailRole,   ///< URL
        DurationRole,    ///< double seconds, 0 unknown
        UrlRole,         ///< the watch URL
        UnavailableRole, ///< bool: private or removed
        DownloadedRole,  ///< bool: a file for it sits in the download folder
        LiveRole,        ///< bool
    };

    enum class Action
    {
        Play,
        Download,
    };
    Q_ENUM(Action)

    static constexpr int kRowHeight = 72;
    static constexpr int kThumbWidth = 96;
    static constexpr int kThumbHeight = 54;

    PlaylistEntryDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~PlaylistEntryDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    /// The button under `pos` when the row fills `rect`; none for an unavailable entry.
    [[nodiscard]] static std::optional<Action> actionAt(const QPoint& pos, const QRect& rect, bool unavailable);
    /// Where the check box is painted inside a row `rect`.
    [[nodiscard]] static QRect checkRect(const QRect& rect);
    /// Whether an entry's title marks it unavailable (pure): the engine's
    /// flat read gives "[Private video]" / "[Deleted video]" for some, and no
    /// title at all (no duration, no channel either) for the rest.
    [[nodiscard]] static bool isUnavailableTitle(const QString& title);
    /// The whole verdict: a marker title, or no title on a YouTube link; an
    /// untitled entry from another site is a plain item (ADR-006).
    [[nodiscard]] static bool isUnavailableEntry(const core::MediaEntry& entry);
    /// The badge word for an unavailable title: "Private", "Removed",
    /// "Unavailable" for a missing title, else empty.
    [[nodiscard]] static QString unavailableLabel(const QString& title);
    /// The action's tooltip and accessible name.
    [[nodiscard]] static QString actionLabel(Action action);

Q_SIGNALS:
    void actionTriggered(const QModelIndex& index, pldl::ui::PlaylistEntryDelegate::Action action);

private:
    struct HitButton
    {
        QRect rect;
        Action action;
        QString icon;
    };
    [[nodiscard]] static QList<HitButton> buttonsFor(const QRect& row);
    static void paintCheck(QPainter* painter, const QRect& box, Qt::CheckState state, bool enabled, bool hovered,
                           const Tokens& t, qreal dpr);
    static void paintBadge(QPainter* painter, int x, int centreY, const QString& text, const QColor& fill,
                           const QColor& textColor, const QFont& font);
    void paintButtons(QPainter* painter, const QList<HitButton>& buttons, const Tokens& t, qreal dpr) const;

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    QPoint m_hoverPos{-1, -1};
};

} // namespace pldl::ui
