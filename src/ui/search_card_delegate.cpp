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

void SearchCardDelegate::setCardWidth(int width)
{
    m_cardWidth = std::clamp(width, kMinCardWidth, kMaxCardWidth);
}

int SearchCardDelegate::thumbHeight() const
{
    return (m_cardWidth - 2) * 9 / 16;
}

int SearchCardDelegate::cardHeight() const
{
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
        return tr("1 video");
    }
    return tr("%1 videos").arg(QLocale().toString(count));
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
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QRect card = cardRect(option.rect);
    const bool hover = card.contains(m_hoverPos);
    const bool pressed = m_pressed.isValid() && m_pressed == index;
    const bool focused = (option.state & QStyle::State_HasFocus) != 0;

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    // DESIGN.md section 1: hovered cards rise to the elevated surface, a
    // press tints them with the soft accent, focus draws the accent ring.
    painter->fillPath(path, pressed ? t.accentSoft : (hover ? t.elevated : t.panel));
    painter->setPen(QPen(focused || pressed ? t.accent : (hover ? t.muted : t.border), focused ? 2 : 1));
    painter->drawPath(path);

    const QRect thumb(card.left() + 1, card.top() + 1, card.width() - 2, thumbHeight());
    painter->save();
    painter->setClipPath(path);
    const QString thumbUrl = index.data(ThumbnailRole).toString();
    const QPixmap pixmap = thumbUrl.isEmpty() ? QPixmap() : m_thumbnails.get(thumbUrl);
    thumbs::paintThumbnail(painter, thumb, pixmap, {t.accent, u"playlist"_s, 28}, dpr, kRadius);
    painter->restore();

    // The video count in the corner of the picture: the badge pair, the
    // icon's yellow with the dark ground on it.
    if (const QString count = countText(index.data(CountRole).toLongLong()); !count.isEmpty()) {
        QFont pillFont = option.font;
        pillFont.setPointSizeF(std::max(7.0, option.font.pointSizeF() - 1.5));
        pillFont.setWeight(QFont::DemiBold);
        const QFontMetrics pm(pillFont);
        const int w = pm.horizontalAdvance(count) + 12;
        const int h = pm.height() + 4;
        const QRect pill(thumb.right() - w - kPillInset, thumb.bottom() - h - kPillInset, w, h);
        QPainterPath pillPath;
        pillPath.addRoundedRect(pill, h / 2, h / 2);
        painter->fillPath(pillPath, t.badge);
        painter->setFont(pillFont);
        painter->setPen(t.badgeText);
        painter->drawText(pill, Qt::AlignCenter, count);
    }

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
    painter->restore();
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
