#pragma once

#include <QPersistentModelIndex>
#include <QPoint>
#include <QStyledItemDelegate>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class ThumbnailCache;

/// One playlist result of the Search page (DESIGN.md section 3). In the grid
/// layout: a card with a 16:9 picture, the video count pill in its corner,
/// the title on two lines and the channel under it. In the list layout: a
/// full-width row with a small picture on the left, the title on one line
/// and the channel under it (FEATURES B10). Hover lifts the item, pressed
/// tints it, focus draws the accent ring. The width is set by the page from
/// the view's width; the height follows the layout.
class SearchCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SearchCardDelegate)

public:
    enum Role
    {
        TitleRole = Qt::UserRole + 1,
        ChannelRole,
        ThumbnailRole,
        UrlRole,
        CountRole, ///< qint64, -1 when unknown
    };

    enum class Layout
    {
        Grid,
        List,
    };

    static constexpr int kMinCardWidth = 220;
    static constexpr int kMaxCardWidth = 320;
    static constexpr int kGap = 14; ///< the gutter kept inside every item rect
    static constexpr int kPad = 12;
    static constexpr int kRowThumbWidth = 96; ///< the list row's picture
    static constexpr int kRowThumbHeight = 54;

    SearchCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~SearchCardDelegate() override;

    void setLayout(Layout layout);
    [[nodiscard]] Layout layout() const { return m_layout; }
    /// Grid: clamped to kMinCardWidth..kMaxCardWidth. List: the row's full width.
    void setCardWidth(int width);
    [[nodiscard]] int cardWidth() const { return m_cardWidth; }
    [[nodiscard]] int cardHeight() const;
    /// The item size: the card plus the gutter.
    [[nodiscard]] QSize itemSize() const;
    /// Where the pointer is over the viewport (-1,-1 when outside).
    void setHoverPos(const QPoint& pos);
    /// The card inside an item rect.
    [[nodiscard]] static QRect cardRect(const QRect& itemRect);
    /// "12 videos", "1 video", empty when unknown (pure).
    [[nodiscard]] static QString countText(qint64 count);

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

Q_SIGNALS:
    /// The pressed or hovered card changed: the view repaints.
    void stateChanged();

private:
    [[nodiscard]] int thumbHeight() const;
    void paintCard(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                   const QRect& card) const;
    void paintRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                  const QRect& row) const;
    void paintCountPill(QPainter* painter, const QStyleOptionViewItem& option, const QRect& thumb,
                        qint64 count) const;

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    Layout m_layout = Layout::Grid;
    int m_cardWidth = kMinCardWidth;
    QPoint m_hoverPos{-1, -1};
    QPersistentModelIndex m_pressed;
};

} // namespace pldl::ui
