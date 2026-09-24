#include "ui/download_card_delegate.h"

#include "core/downloads/download_queue.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"
#include "ui/thumbnail_cache.h"
#include "ui/thumbnail_painter.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QTimer>
#include <QToolTip>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kMargin = 12;
constexpr int kButtonSize = 28;
constexpr int kButtonGap = 4;
constexpr int kRadius = 12;
constexpr int kStateBar = 3;

core::DownloadState stateOf(const QModelIndex& index)
{
    return index.data(core::DownloadQueue::StateRole).value<core::DownloadState>();
}

bool showsBar(core::DownloadState state)
{
    switch (state) {
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
    case core::DownloadState::Paused:
        return true;
    case core::DownloadState::Queued:
    case core::DownloadState::Completed:
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        break;
    }
    return false;
}
} // namespace

DownloadCardDelegate::DownloadCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent)
    : QStyledItemDelegate(parent)
    , m_thumbnails(thumbnails)
    , m_theme(theme)
    , m_pulse(new QTimer(this))
{
    m_pulse->setInterval(40);
    connect(m_pulse, &QTimer::timeout, this, &DownloadCardDelegate::pulse);
}

QSize DownloadCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& /*index*/) const
{
    return {option.rect.width(), kCardHeight};
}

QRect DownloadCardDelegate::cardRect(const QRect& rect)
{
    return rect.adjusted(0, 3, 0, -3);
}

QColor DownloadCardDelegate::stateColor(core::DownloadState state, bool dark)
{
    const Tokens t = Tokens::forScheme(dark);
    switch (state) {
    case core::DownloadState::Queued:
        return t.muted;
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
        return t.accent;
    case core::DownloadState::Paused:
        return t.warning;
    case core::DownloadState::Completed:
        return t.success;
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        return t.danger;
    }
    return t.accent;
}

QString DownloadCardDelegate::actionLabel(Action action, core::DownloadState state)
{
    switch (action) {
    case Action::PauseResume:
        return state == core::DownloadState::Paused ? tr("Resume") : tr("Pause");
    case Action::Cancel:
        return tr("Cancel");
    case Action::Retry:
        return tr("Retry");
    case Action::Open:
        return tr("Open");
    case Action::ShowInFolder:
        return tr("Show in folder");
    case Action::Remove:
        return tr("Remove from list");
    }
    return {};
}

QList<DownloadCardDelegate::Action> DownloadCardDelegate::actionsFor(core::DownloadState state, bool hasFile)
{
    switch (state) {
    case core::DownloadState::Queued:
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
    case core::DownloadState::Paused:
        return {Action::PauseResume, Action::Cancel};
    case core::DownloadState::Completed:
        if (hasFile) {
            return {Action::Open, Action::ShowInFolder, Action::Remove};
        }
        return {Action::Remove};
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        return {Action::Retry, Action::Remove};
    }
    return {};
}

QList<DownloadCardDelegate::HitButton> DownloadCardDelegate::buttonsFor(const QRect& card,
                                                                        core::DownloadState state,
                                                                        bool hasFile) const
{
    QList<HitButton> buttons;
    for (const Action action : actionsFor(state, hasFile)) {
        QString icon;
        switch (action) {
        case Action::PauseResume:
            icon = state == core::DownloadState::Paused ? u"play"_s : u"pause"_s;
            break;
        case Action::Cancel:
            icon = u"cancel"_s;
            break;
        case Action::Retry:
            icon = u"retry"_s;
            break;
        case Action::Open:
            icon = u"open"_s;
            break;
        case Action::ShowInFolder:
            icon = u"folder"_s;
            break;
        case Action::Remove:
            icon = u"trash"_s;
            break;
        }
        buttons.append({QRect(), action, icon, actionLabel(action, state)});
    }
    int x = card.right() - kMargin - kButtonSize;
    const int y = card.center().y() - kButtonSize / 2;
    for (int i = static_cast<int>(buttons.size()) - 1; i >= 0; --i) {
        buttons[i].rect = QRect(x, y, kButtonSize, kButtonSize);
        x -= kButtonSize + kButtonGap;
    }
    return buttons;
}

std::optional<DownloadCardDelegate::Action> DownloadCardDelegate::actionAt(const QPoint& pos, const QRect& rect,
                                                                           const core::DownloadJob& job) const
{
    for (const HitButton& b : buttonsFor(cardRect(rect), job.state, !job.primaryFile().isEmpty())) {
        if (b.rect.contains(pos)) {
            return b.action;
        }
    }
    return std::nullopt;
}

void DownloadCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    const bool dark = m_theme.isDark();
    const Tokens t = Tokens::forScheme(dark);
    const core::DownloadState state = stateOf(index);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const QRect card = cardRect(option.rect);
    const qreal dpr = painter->device()->devicePixelRatio();
    const QColor bar = stateColor(state, dark);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Card surface.
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    painter->fillPath(path, hovered ? t.elevated : t.panel);
    painter->setPen(QPen(t.border, 1));
    painter->drawPath(path);

    // Thumbnail with the state bar on its left edge.
    const QRect thumbRect(card.left() + kMargin, card.top() + (card.height() - kThumbHeight) / 2, kThumbWidth,
                          kThumbHeight);
    const QString thumbUrl = index.data(core::DownloadQueue::ThumbnailRole).toString();
    const QPixmap thumb = thumbUrl.isEmpty() ? QPixmap() : m_thumbnails.get(thumbUrl);
    const bool playlist = index.data(core::DownloadQueue::IsPlaylistRole).toBool();
    thumbs::paintThumbnail(painter, thumbRect, thumb, {t.accentStrong, playlist ? u"playlist"_s : u"film"_s, 22},
                           dpr, 8);
    {
        QPainterPath clip;
        clip.addRoundedRect(thumbRect, 8, 8);
        painter->save();
        painter->setClipPath(clip);
        painter->fillRect(QRect(thumbRect.left(), thumbRect.top(), kStateBar, thumbRect.height()), bar);
        painter->restore();
    }

    // Text block: the buttons take the right end while hovered.
    const QString file = index.data(core::DownloadQueue::FilePathRole).toString();
    const QList<HitButton> buttons = buttonsFor(card, state, !file.isEmpty());
    const int buttonsWidth =
        hovered && !buttons.isEmpty() ? static_cast<int>(buttons.size()) * (kButtonSize + kButtonGap) + 8 : 0;
    const int textLeft = thumbRect.right() + 1 + kMargin;
    const int textRight = card.right() - kMargin - buttonsWidth;
    const int textWidth = std::max(40, textRight - textLeft);

    QFont small = option.font;
    small.setPointSizeF(option.font.pointSizeF() - 1);
    const QFontMetrics smallMetrics(small);

    // Title, with the quality and format at the row's right end.
    QFont titleFont = option.font;
    titleFont.setWeight(QFont::Medium);
    const QRect titleRow(textLeft, card.top() + 9, textWidth, 18);
    const QString format = index.data(core::DownloadQueue::FormatLineRole).toString();
    int titleWidth = titleRow.width();
    if (!format.isEmpty()) {
        const int formatWidth = smallMetrics.horizontalAdvance(format);
        if (formatWidth + 60 < titleRow.width()) {
            painter->setFont(small);
            painter->setPen(t.muted);
            painter->drawText(titleRow, Qt::AlignRight | Qt::AlignVCenter, format);
            titleWidth -= formatWidth + 12;
        }
    }
    painter->setFont(titleFont);
    painter->setPen(t.text);
    const QString title = index.data(core::DownloadQueue::TitleRole).toString();
    painter->drawText(QRect(titleRow.left(), titleRow.top(), titleWidth, titleRow.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(title, Qt::ElideRight, titleWidth));

    // The uploader, or the playlist's current entry and place.
    painter->setFont(small);
    painter->setPen(t.muted);
    const QRect detailRow(textLeft, card.top() + 27, textWidth, 16);
    painter->drawText(detailRow, Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(index.data(core::DownloadQueue::DetailLineRole).toString(),
                                              Qt::ElideRight, detailRow.width()));

    // Progress: an accent chunk on the border track while something is in flight.
    if (showsBar(state)) {
        const QRect track(textLeft, card.top() + 48, textWidth, 4);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, 2, 2);
        painter->fillPath(trackPath, t.border);
        const double progress = index.data(core::DownloadQueue::ProgressRole).toDouble();
        const bool indeterminate = state == core::DownloadState::Probing ||
                                   state == core::DownloadState::Processing ||
                                   (state == core::DownloadState::Downloading && progress < 0);
        if (!indeterminate) {
            if (progress > 0) {
                QRect fill = track;
                fill.setWidth(std::max(4, static_cast<int>(track.width() * std::min(1.0, progress))));
                QPainterPath fillPath;
                fillPath.addRoundedRect(fill, 2, 2);
                painter->fillPath(fillPath, bar);
            }
        } else {
            // A short segment travelling along the track; the pulse timer
            // keeps the view repainting while one is on screen.
            m_animationPainted = true;
            if (!m_pulse->isActive()) {
                m_pulse->start();
            }
            const int span = track.width() / 4;
            const int offset =
                static_cast<int>((QDateTime::currentMSecsSinceEpoch() / 10) % (track.width() + span)) - span;
            const QRect seg(track.left() + std::max(0, offset), track.top(),
                            std::min(span, track.width() - std::max(0, offset)), track.height());
            if (seg.width() > 0) {
                QPainterPath segPath;
                segPath.addRoundedRect(seg, 2, 2);
                painter->fillPath(segPath, bar);
            }
        }
    }

    // Status line.
    QColor statusColor = t.muted;
    if (state == core::DownloadState::Failed) {
        statusColor = t.danger;
    } else if (state == core::DownloadState::Completed) {
        statusColor = t.success;
    } else if (state == core::DownloadState::Paused) {
        statusColor = t.warning;
    }
    painter->setPen(statusColor);
    const QRect statusRow(textLeft, card.top() + 56, textWidth, 16);
    painter->drawText(statusRow, Qt::AlignLeft | Qt::AlignVCenter,
                      smallMetrics.elidedText(index.data(core::DownloadQueue::StatusLineRole).toString(),
                                              Qt::ElideRight, statusRow.width()));

    // Hover actions.
    if (hovered) {
        for (const HitButton& b : buttons) {
            const bool over = b.rect.contains(m_hoverPos);
            QPainterPath bp;
            bp.addRoundedRect(b.rect, 8, 8);
            painter->fillPath(bp, over ? t.hover : t.panel);
            painter->setPen(QPen(over ? t.accent : t.border, 1));
            painter->drawPath(bp);
            const QPixmap icon = icons::pixmap(b.icon, over ? t.text : t.muted, 16, dpr);
            painter->drawPixmap(b.rect.center().x() - 8, b.rect.center().y() - 8, icon);
        }
    }

    // Keyboard focus: a 2 px accent ring around the card.
    if (option.state.testFlag(QStyle::State_HasFocus)) {
        QPainterPath ring;
        ring.addRoundedRect(QRectF(card).adjusted(1, 1, -1, -1), kRadius, kRadius);
        painter->setPen(QPen(t.accent, 2));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(ring);
    }
    painter->restore();
}

bool DownloadCardDelegate::editorEvent(QEvent* event, QAbstractItemModel* /*model*/,
                                       const QStyleOptionViewItem& option, const QModelIndex& index)
{
    const auto* mouse = dynamic_cast<QMouseEvent*>(event);
    if (mouse == nullptr) {
        return false;
    }
    m_hoverPos = mouse->pos();
    m_hoverIndex = index;
    // The view repaints a row on enter and leave only; the button under the
    // pointer changes within the row, so every move repaints it.
    if (const auto* view = qobject_cast<const QAbstractItemView*>(option.widget); view != nullptr) {
        view->viewport()->update(option.rect);
    }
    if (event->type() != QEvent::MouseButtonRelease || mouse->button() != Qt::LeftButton) {
        return false;
    }
    const core::DownloadState state = stateOf(index);
    const bool hasFile = !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty();
    for (const HitButton& b : buttonsFor(cardRect(option.rect), state, hasFile)) {
        if (b.rect.contains(mouse->pos())) {
            const quint64 id = index.data(core::DownloadQueue::IdRole).toULongLong();
            const Action action = b.action;
            // The handler may remove or move the row: let the view finish its
            // own mouse handling first.
            QTimer::singleShot(0, this, [this, id, action] { Q_EMIT actionTriggered(id, action); });
            return true;
        }
    }
    return false;
}

bool DownloadCardDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                                     const QModelIndex& index)
{
    if (event->type() != QEvent::ToolTip || !index.isValid()) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    const core::DownloadState state = stateOf(index);
    const bool hasFile = !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty();
    for (const HitButton& b : buttonsFor(cardRect(option.rect), state, hasFile)) {
        if (b.rect.contains(event->pos())) {
            QToolTip::showText(event->globalPos(), b.tooltip, view);
            return true;
        }
    }
    QString tip = index.data(core::DownloadQueue::TitleRole).toString();
    const QString file = index.data(core::DownloadQueue::FilePathRole).toString();
    if (!file.isEmpty()) {
        tip += u"\n"_s + file;
    }
    const QString error = index.data(core::DownloadQueue::ErrorRole).toString();
    if (!error.isEmpty()) {
        tip += u"\n"_s + error;
    }
    QToolTip::showText(event->globalPos(), tip, view);
    return true;
}

void DownloadCardDelegate::pulse()
{
    if (!m_animationPainted) {
        m_pulse->stop(); // nothing animated was painted since the last tick
        return;
    }
    m_animationPainted = false;
    Q_EMIT repaintNeeded();
}

} // namespace pldl::ui
