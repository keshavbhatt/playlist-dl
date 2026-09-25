#include "ui/search_card_delegate.h"

#include "core/theme/theme_service.h"
#include "ui/pldl_style.h"
#include "ui/thumbnail_cache.h"
#include "ui/thumbnail_painter.h"

#include <QAbstractItemModel>
#include <QFontMetrics>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTextOption>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kRadius = 12;
constexpr int kTextGap = 8;   ///< picture to title
constexpr int kLineGap = 4;   ///< title to channel
constexpr int kPillInset = 6; ///< the count pill's distance from the picture's corner
} // namespace

SearchCardDelegate::SearchCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_thumbnails(thumbnails)
    , m_theme(theme)
{}

SearchCardDelegate::~SearchCardDelegate() = default;

void SearchCardDelegate::setLayout(Layout layout)
{
    if (m_layout == layout) {
        return;
    }
    m_layout = layout;
    Q_EMIT stateChanged();
}

void SearchCardDelegate::setCardWidth(int width)
{
    m_cardWidth = m_layout == Layout::Grid ? std::clamp(width, kMinCardWidth, kMaxCardWidth)
                                           : std::max(width, kMinCardWidth);
}

int SearchCardDelegate::thumbHeight() const
{
    return (m_cardWidth - 2) * 9 / 16;
}

int SearchCardDelegate::cardHeight() const
{
    if (m_layout == Layout::List) {
        return kRowThumbHeight + 2 * kPad;
    }
    QFont title;
    title.setWeight(QFont::Medium);
    const QFontMetrics tm(title);
    const QFontMetrics fm{QFont()};
    return 1 + thumbHeight() + kTextGap + tm.height() * 2 + kLineGap + fm.height() + kPad + 1;
}

QSize SearchCardDelegate::itemSize() const
{
    return {m_cardWidth + kGap, cardHeight() + kGap};
}

QRect SearchCardDelegate::cardRect(const QRect& itemRect)
{
    return itemRect.adjusted(0, 0, -kGap, -kGap);
}

QString SearchCardDelegate::countText(qint64 count)
{
    if (count < 0) {
        return {};
    }
    if (count == 1) {
        return tr("1 item");
    }
    return tr("%1 items").arg(QLocale().toString(count));
}

void SearchCardDelegate::setHoverPos(const QPoint& pos)
{
    if (m_hoverPos == pos) {
        return;
    }
    m_hoverPos = pos;
    Q_EMIT stateChanged();
}

QSize SearchCardDelegate::sizeHint(const QStyleOptionViewItem& /*option*/, const QModelIndex& /*index*/) const
{
    return itemSize();
}

void SearchCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const QRect card = cardRect(option.rect);
    const bool hover = card.contains(m_hoverPos);
    const bool pressed = m_pressed.isValid() && m_pressed == index;
    const bool focused = (option.state & QStyle::State_HasFocus) != 0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    // DESIGN.md section 1: hovered items rise to the elevated surface, a
    // press tints them with the soft accent, focus draws the accent ring.
    painter->fillPath(path, pressed ? t.accentSoft : (hover ? t.elevated : t.panel));
    painter->setPen(QPen(focused || pressed ? t.accent : (hover ? t.muted : t.border), focused ? 2 : 1));
    painter->drawPath(path);
    if (m_layout == Layout::List) {
        paintRow(painter, option, index, card);
    } else {
        painter->save();
        painter->setClipPath(path);
        paintCard(painter, option, index, card);
        painter->restore();
    }
    painter->restore();
}

void SearchCardDelegate::paintCountPill(QPainter* painter, const QStyleOptionViewItem& option,
                                        const QRect& thumb, qint64 count) const
{
    // The video count in the corner of the picture: the badge pair, the
    // icon's yellow with the dark ground on it.
    const QString text = countText(count);
    if (text.isEmpty()) {
        return;
    }
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    QFont pillFont = option.font;
    pillFont.setPointSizeF(std::max(7.0, option.font.pointSizeF() - 1.5));
    pillFont.setWeight(QFont::DemiBold);
    const QFontMetrics pm(pillFont);
    const int w = pm.horizontalAdvance(text) + 12;
    const int h = pm.height() + 4;
    const int inset = m_layout == Layout::List ? 4 : kPillInset;
    const QRect pill(thumb.right() - w - inset, thumb.bottom() - h - inset, w, h);
    QPainterPath pillPath;
    pillPath.addRoundedRect(pill, h / 2, h / 2);
    painter->fillPath(pillPath, t.badge);
    painter->setFont(pillFont);
    painter->setPen(t.badgeText);
    painter->drawText(pill, Qt::AlignCenter, text);
}

void SearchCardDelegate::paintCard(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index, const QRect& card) const
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QRect thumb(card.left() + 1, card.top() + 1, card.width() - 2, thumbHeight());
    const QString thumbUrl = index.data(ThumbnailRole).toString();
    const QPixmap pixmap = thumbUrl.isEmpty() ? QPixmap() : m_thumbnails.get(thumbUrl);
    thumbs::paintThumbnail(painter, thumb, pixmap, {t.accent, u"playlist"_s, 28}, dpr, kRadius);
    paintCountPill(painter, option, thumb, index.data(CountRole).toLongLong());

    // Title on up to two lines, the channel under it in the muted colour.
    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Medium);
    const QFontMetrics tm(titleFont);
    const QRect text(card.left() + kPad, thumb.bottom() + 1 + kTextGap, card.width() - 2 * kPad,
                     card.bottom() - thumb.bottom() - kTextGap - kPad);
    const QString title = index.data(TitleRole).toString();
    QString shown = title;
    if (tm.horizontalAdvance(title) > text.width() * 2 - tm.averageCharWidth() * 4) {
        shown = tm.elidedText(title, Qt::ElideRight, text.width() * 2 - tm.averageCharWidth() * 6);
    }
    QTextOption wrap(Qt::AlignLeft | Qt::AlignTop);
    wrap.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    painter->setFont(titleFont);
    painter->setPen(t.text);
    const QRect titleSlot(text.left(), text.top(), text.width(), tm.height() * 2);
    painter->drawText(titleSlot, shown, wrap);

    const QFontMetrics fm(option.font);
    painter->setFont(option.font);
    painter->setPen(t.muted);
    const QRect channelSlot(text.left(), titleSlot.bottom() + 1 + kLineGap, text.width(), fm.height());
    painter->drawText(channelSlot, Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(index.data(ChannelRole).toString(), Qt::ElideRight, text.width()));
}

void SearchCardDelegate::paintRow(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index, const QRect& row) const
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QRect thumb(row.left() + kPad, row.top() + kPad, kRowThumbWidth, kRowThumbHeight);
    const QString thumbUrl = index.data(ThumbnailRole).toString();
    const QPixmap pixmap = thumbUrl.isEmpty() ? QPixmap() : m_thumbnails.get(thumbUrl);
    thumbs::paintThumbnail(painter, thumb, pixmap, {t.accent, u"playlist"_s, 20}, dpr, 8);

    // The count pill at the row's right end (the small picture would hide
    // under it); the text takes the room between.
    int pillWidth = 0;
    if (const QString count = countText(index.data(CountRole).toLongLong()); !count.isEmpty()) {
        QFont pillFont = option.font;
        pillFont.setPointSizeF(std::max(7.0, option.font.pointSizeF() - 1.5));
        pillFont.setWeight(QFont::DemiBold);
        const QFontMetrics pm(pillFont);
        pillWidth = pm.horizontalAdvance(count) + 12;
        const int h = pm.height() + 4;
        const QRect pill(row.right() - kPad - pillWidth, row.center().y() - h / 2, pillWidth, h);
        QPainterPath pillPath;
        pillPath.addRoundedRect(pill, h / 2, h / 2);
        painter->fillPath(pillPath, t.badge);
        painter->setFont(pillFont);
        painter->setPen(t.badgeText);
        painter->drawText(pill, Qt::AlignCenter, count);
        pillWidth += kPad;
    }

    // Title on one line, the channel under it; both centred on the picture.
    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Medium);
    const QFontMetrics tm(titleFont);
    const QFontMetrics fm(option.font);
    const int textLeft = thumb.right() + 1 + kPad;
    const int textWidth = row.right() - kPad - pillWidth - textLeft;
    const int block = tm.height() + kLineGap + fm.height();
    const int top = thumb.top() + (thumb.height() - block) / 2;
    painter->setFont(titleFont);
    painter->setPen(t.text);
    painter->drawText(QRect(textLeft, top, textWidth, tm.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      tm.elidedText(index.data(TitleRole).toString(), Qt::ElideRight, textWidth));
    painter->setFont(option.font);
    painter->setPen(t.muted);
    painter->drawText(QRect(textLeft, top + tm.height() + kLineGap, textWidth, fm.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(index.data(ChannelRole).toString(), Qt::ElideRight, textWidth));
}

bool SearchCardDelegate::editorEvent(QEvent* event, QAbstractItemModel* /*model*/,
                                     const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonPress) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && cardRect(option.rect).contains(mouse->pos())) {
            m_pressed = index;
            Q_EMIT stateChanged();
        }
        return false;
    }
    if (event->type() == QEvent::MouseButtonRelease && m_pressed.isValid()) {
        m_pressed = QPersistentModelIndex();
        Q_EMIT stateChanged();
    }
    return false;
}

} // namespace pldl::ui
