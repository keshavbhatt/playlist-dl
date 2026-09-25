#include "ui/playlist_entry_delegate.h"

#include "core/youtube_url.h"

#include "core/downloads/media_info.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"
#include "ui/thumbnail_cache.h"
#include "ui/thumbnail_painter.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTextLayout>
#include <QTimer>
#include <QToolTip>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kMargin = 12;
constexpr int kCheck = 18;
constexpr int kIndexWidth = 34;
constexpr int kGap = 12;
constexpr int kButtonSize = 28;
constexpr int kButtonGap = 4;
constexpr int kRadius = 8;
constexpr int kBadgePad = 7;

/// `text` laid out on up to two lines inside `width`, the second one elided.
QStringList twoLines(const QString& text, const QFont& font, int width)
{
    QStringList lines;
    QTextLayout layout(text, font);
    layout.beginLayout();
    while (lines.size() < 2) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        if (lines.size() == 1) {
            lines << QFontMetrics(font).elidedText(text.mid(line.textStart()).trimmed(), Qt::ElideRight, width);
        } else {
            lines << text.mid(line.textStart(), line.textLength()).trimmed();
        }
    }
    layout.endLayout();
    return lines;
}
} // namespace

PlaylistEntryDelegate::PlaylistEntryDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_thumbnails(thumbnails)
    , m_theme(theme)
{
}

QSize PlaylistEntryDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const
{
    return {option.rect.width(), kRowHeight};
}

bool PlaylistEntryDelegate::isUnavailableTitle(const QString& title)
{
    return !unavailableLabel(title).isEmpty();
}

bool PlaylistEntryDelegate::isUnavailableEntry(const core::MediaEntry& entry)
{
    // YouTube's flat shape for a removed video is a marker title, or no title
    // at all. Another site's flat playlist (a SoundCloud set) lists its
    // entries by link alone: no title there means nothing, the engine names
    // the item when it downloads.
    if (entry.title.trimmed().isEmpty()) {
        return entry.url.isEmpty() || core::isYouTubeHost(QUrl(entry.url).host());
    }
    return isUnavailableTitle(entry.title);
}

QString PlaylistEntryDelegate::unavailableLabel(const QString& title)
{
    const QString t = title.trimmed();
    if (t.isEmpty()) {
        return tr("Unavailable");
    }
    if (t.compare(u"[Private video]"_s, Qt::CaseInsensitive) == 0) {
        return tr("Private");
    }
    if (t.compare(u"[Deleted video]"_s, Qt::CaseInsensitive) == 0 ||
        t.compare(u"[Unavailable video]"_s, Qt::CaseInsensitive) == 0) {
        return tr("Removed");
    }
    return {};
}

QString PlaylistEntryDelegate::actionLabel(Action action)
{
    switch (action) {
    case Action::Play:
        return tr("Play");
    case Action::Download:
        return tr("Download this item");
    }
    return {};
}

QRect PlaylistEntryDelegate::checkRect(const QRect& rect)
{
    return {rect.left() + kMargin, rect.center().y() - kCheck / 2, kCheck, kCheck};
}

QList<PlaylistEntryDelegate::HitButton> PlaylistEntryDelegate::buttonsFor(const QRect& row)
{
    QList<HitButton> buttons{{QRect(), Action::Play, u"play"_s}, {QRect(), Action::Download, u"download"_s}};
    int x = row.right() - kMargin - kButtonSize;
    const int y = row.center().y() - kButtonSize / 2;
    for (int i = static_cast<int>(buttons.size()) - 1; i >= 0; --i) {
        buttons[i].rect = QRect(x, y, kButtonSize, kButtonSize);
        x -= kButtonSize + kButtonGap;
    }
    return buttons;
}

std::optional<PlaylistEntryDelegate::Action> PlaylistEntryDelegate::actionAt(const QPoint& pos, const QRect& rect,
                                                                             bool unavailable)
{
    if (unavailable) {
        return std::nullopt;
    }
    for (const HitButton& b : buttonsFor(rect)) {
        if (b.rect.contains(pos)) {
            return b.action;
        }
    }
    return std::nullopt;
}

void PlaylistEntryDelegate::paintCheck(QPainter* painter, const QRect& box, Qt::CheckState state, bool enabled,
                                       bool hovered, const Tokens& t, qreal dpr)
{
    QPainterPath path;
    path.addRoundedRect(QRectF(box).adjusted(1, 1, -1, -1), 4, 4);
    if (!enabled) {
        painter->setPen(QPen(t.border, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        return;
    }
    if (state == Qt::Checked) {
        painter->fillPath(path, t.accent);
        painter->setPen(QPen(t.accent, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
        const QPixmap check = icons::pixmap(u"check"_s, Qt::white, 12, dpr);
        painter->drawPixmap(box.center().x() - 6, box.center().y() - 6, check);
        return;
    }
    painter->setPen(QPen(hovered ? t.text : t.muted, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

void PlaylistEntryDelegate::paintBadge(QPainter* painter, int x, int centreY, const QString& text,
                                       const QColor& fill, const QColor& textColor, const QFont& font)
{
    const QFontMetrics fm(font);
    const QRect pill(x, centreY - 9, fm.horizontalAdvance(text) + kBadgePad * 2, 18);
    QPainterPath path;
    path.addRoundedRect(pill, 9, 9);
    painter->fillPath(path, fill);
    painter->setFont(font);
    painter->setPen(textColor);
    painter->drawText(pill, Qt::AlignCenter, text);
}

void PlaylistEntryDelegate::paintButtons(QPainter* painter, const QList<HitButton>& buttons, const Tokens& t,
                                         qreal dpr) const
{
    for (const HitButton& b : buttons) {
        const bool over = b.rect.contains(m_hoverPos);
        QPainterPath bp;
        bp.addRoundedRect(b.rect, 8, 8);
        painter->fillPath(bp, over ? t.hover : t.panel);
        painter->setPen(QPen(over ? t.accent : t.border, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(bp);
        const QPixmap icon = icons::pixmap(b.icon, over ? t.text : t.muted, 16, dpr);
        painter->drawPixmap(b.rect.center().x() - 8, b.rect.center().y() - 8, icon);
    }
}

void PlaylistEntryDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    const bool dark = m_theme.isDark();
    const Tokens t = Tokens::forScheme(dark);
    const QRect row = option.rect;
    const qreal dpr = painter->device()->devicePixelRatio();
    const bool unavailable = index.data(UnavailableRole).toBool();
    const bool downloaded = index.data(DownloadedRole).toBool();
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver) && !unavailable;
    const auto checkState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Row surface: a hairline under every row, a tint while hovered.
    if (hovered) {
        QPainterPath path;
        path.addRoundedRect(row.adjusted(0, 1, 0, -1), kRadius, kRadius);
        painter->fillPath(path, t.hover);
    }
    painter->setPen(QPen(t.border, 1));
    painter->drawLine(row.left() + kMargin, row.bottom(), row.right() - kMargin, row.bottom());

    // Check box and index.
    paintCheck(painter, checkRect(row), checkState, !unavailable, hovered, t, dpr);
    QFont small = option.font;
    small.setPointSizeF(option.font.pointSizeF() - 1);
    const QFontMetrics smallMetrics(small);
    painter->setFont(small);
    painter->setPen(t.muted);
    const QRect indexRect(row.left() + kMargin + kCheck + kGap, row.top(), kIndexWidth, row.height());
    painter->drawText(indexRect, Qt::AlignLeft | Qt::AlignVCenter, QString::number(index.data(IndexRole).toInt()));

    // Thumbnail with the duration chip.
    const QRect thumbRect(indexRect.right() + 1, row.center().y() - kThumbHeight / 2, kThumbWidth, kThumbHeight);
    const QString thumbUrl = index.data(ThumbnailRole).toString();
    const QPixmap thumb = thumbUrl.isEmpty() || unavailable ? QPixmap() : m_thumbnails.get(thumbUrl);
    const QColor tile = dark ? t.muted.darker(175) : t.muted.lighter(130);
    if (unavailable) {
        painter->setOpacity(0.55);
    }
    thumbs::paintThumbnail(painter, thumbRect, thumb, {tile, u"film"_s, 20}, dpr, 6);
    const double duration = index.data(DurationRole).toDouble();
    if (duration > 0 && !unavailable) {
        thumbs::paintChip(painter, thumbRect, core::formatDuration(duration), small, dpr);
    } else if (index.data(LiveRole).toBool()) {
        thumbs::paintChip(painter, thumbRect, tr("Live"), small, dpr);
    }

    // Text block; the buttons take the right end while hovered.
    const QList<HitButton> buttons = buttonsFor(row);
    const int buttonsWidth = hovered ? static_cast<int>(buttons.size()) * (kButtonSize + kButtonGap) + 8 : 0;
    const int textLeft = thumbRect.right() + 1 + kGap;
    const int textRight = row.right() - kMargin - buttonsWidth;
    const int textWidth = std::max(40, textRight - textLeft);

    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Medium);
    const QFontMetrics titleMetrics(titleFont);
    QString title = index.data(TitleRole).toString();
    const QString badge = unavailable ? unavailableLabel(title) : (downloaded ? tr("Downloaded") : QString());
    if (title.trimmed().isEmpty()) {
        title = unavailable ? tr("Unavailable") : tr("Item %1").arg(index.row() + 1);
    }
    const int badgeWidth = badge.isEmpty() ? 0 : smallMetrics.horizontalAdvance(badge) + kBadgePad * 2 + 8;
    const QStringList lines = twoLines(title, titleFont, textWidth - badgeWidth);
    const int lineHeight = titleMetrics.height();
    const int detailHeight = smallMetrics.height();
    const int block = lineHeight * std::max(1, static_cast<int>(lines.size())) + 2 + detailHeight;
    int y = row.center().y() - block / 2;
    painter->setFont(titleFont);
    painter->setPen(unavailable ? t.muted : t.text);
    for (const QString& line : lines) {
        painter->drawText(QRect(textLeft, y, textWidth - badgeWidth, lineHeight), Qt::AlignLeft | Qt::AlignVCenter,
                          line);
        y += lineHeight;
    }
    if (!badge.isEmpty()) {
        const int firstLineCentre = row.center().y() - block / 2 + lineHeight / 2;
        const int badgeX = textLeft + (lines.isEmpty() ? 0 : titleMetrics.horizontalAdvance(lines.first())) + 8;
        if (unavailable) {
            paintBadge(painter, badgeX, firstLineCentre, badge, t.hover, t.muted, small);
        } else {
            QColor fill = t.success;
            fill.setAlphaF(dark ? 0.22F : 0.16F);
            paintBadge(painter, badgeX, firstLineCentre, badge, fill, t.success, small);
        }
    }

    // Channel and duration.
    QStringList details;
    if (const QString uploader = index.data(UploaderRole).toString(); !uploader.isEmpty()) {
        details << uploader;
    }
    if (duration > 0) {
        details << core::formatDuration(duration);
    }
    painter->setFont(small);
    painter->setPen(t.muted);
    y += 2;
    painter->drawText(QRect(textLeft, y, textWidth, detailHeight), Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(details.join(u", "_s), Qt::ElideRight, textWidth));
    painter->setOpacity(1.0);

    if (hovered) {
        paintButtons(painter, buttons, t, dpr);
    }

    if (option.state.testFlag(QStyle::State_HasFocus)) {
        QPainterPath ring;
        ring.addRoundedRect(QRectF(row).adjusted(1, 2, -1, -2), kRadius, kRadius);
        painter->setPen(QPen(t.accent, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(ring);
    }
    painter->restore();
}

bool PlaylistEntryDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                        const QModelIndex& index)
{
    const auto* mouse = dynamic_cast<QMouseEvent*>(event);
    if (mouse == nullptr) {
        return false;
    }
    m_hoverPos = mouse->pos();
    if (const auto* view = qobject_cast<const QAbstractItemView*>(option.widget); view != nullptr) {
        view->viewport()->update(option.rect);
    }
    if (event->type() != QEvent::MouseButtonRelease || mouse->button() != Qt::LeftButton) {
        return false;
    }
    if (index.data(UnavailableRole).toBool()) {
        return true; // inert: nothing to toggle, nothing to play
    }
    if (const std::optional<Action> action = actionAt(mouse->pos(), option.rect, false); action) {
        const QPersistentModelIndex persistent(index);
        const Action which = *action;
        // The handler may open a sheet: let the view finish its own mouse handling first.
        QTimer::singleShot(0, this, [this, persistent, which] {
            if (persistent.isValid()) {
                Q_EMIT actionTriggered(QModelIndex(persistent), which);
            }
        });
        return true;
    }
    const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
    model->setData(index, checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    return true;
}

bool PlaylistEntryDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                                      const QModelIndex& index)
{
    if (event->type() != QEvent::ToolTip || !index.isValid()) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    if (const std::optional<Action> action = actionAt(event->pos(), option.rect, index.data(UnavailableRole).toBool());
        action) {
        QToolTip::showText(event->globalPos(), actionLabel(*action), view);
        return true;
    }
    QString tip = index.data(TitleRole).toString();
    if (const QString uploader = index.data(UploaderRole).toString(); !uploader.isEmpty()) {
        tip += u"\n"_s + uploader;
    }
    QToolTip::showText(event->globalPos(), tip, view);
    return true;
}

} // namespace pldl::ui
