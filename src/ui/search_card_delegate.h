#pragma once

#include <QPersistentModelIndex>
#include <QPoint>
#include <QStyledItemDelegate>

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class ThumbnailCache;

/// One playlist card of the Search page's grid (DESIGN.md section 3): a 16:9
/// picture with the video count pill in its corner, the title on two lines,
/// the channel under it. Hover lifts the card, pressed tints it, focus draws
/// the accent ring. The card width is set by the page from the grid's width;
/// the height follows.
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

    static constexpr int kMinCardWidth = 220;
    static constexpr int kMaxCardWidth = 320;
    static constexpr int kGap = 14; ///< the gutter kept inside every item rect
    static constexpr int kPad = 12;

    SearchCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~SearchCardDelegate() override;

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

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    int m_cardWidth = kMinCardWidth;
    QPoint m_hoverPos{-1, -1};
    QPersistentModelIndex m_pressed;
};

} // namespace pldl::ui
