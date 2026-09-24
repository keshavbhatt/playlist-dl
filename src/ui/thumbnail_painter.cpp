#include "ui/thumbnail_painter.h"

#include "ui/icons.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

using namespace Qt::StringLiterals;

namespace pldl::ui::thumbs {

QPixmap coverCrop(const QPixmap& pixmap, const QSize& logical, qreal dpr)
{
    if (pixmap.isNull() || logical.isEmpty()) {
        return {};
    }
    // Device pixels throughout: a fractional ratio (1.25, 1.5) must not be
    // truncated, or the item shows a zoomed centre crop.
    const int w = qRound(logical.width() * dpr);
    const int h = qRound(logical.height() * dpr);
    QPixmap scaled = pixmap.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    QPixmap out = scaled.copy((scaled.width() - w) / 2, (scaled.height() - h) / 2, w, h);
    out.setDevicePixelRatio(dpr);
    return out;
}

QPixmap containScale(const QPixmap& pixmap, const QSize& logical, qreal dpr)
{
    if (pixmap.isNull() || logical.isEmpty()) {
        return {};
    }
    QPixmap out = pixmap.scaled(qRound(logical.width() * dpr), qRound(logical.height() * dpr), Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
    out.setDevicePixelRatio(dpr);
    return out;
}

QRect containedRect(const QSize& pixmapDeviceSize, const QRect& rect, qreal dpr)
{
    const int w = qRound(pixmapDeviceSize.width() / dpr);
    const int h = qRound(pixmapDeviceSize.height() / dpr);
    return {rect.center().x() - w / 2, rect.center().y() - h / 2, w, h};
}

void paintThumbnail(QPainter* painter, const QRect& rect, const QPixmap& pixmap, const Tile& tile, qreal dpr,
                    int radius, bool circle, Fit fit, const QColor& letterbox)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath clip;
    QRect target = rect;
    if (circle) {
        const int d = std::min(rect.width(), rect.height()) - 16;
        target = QRect(rect.center().x() - d / 2, rect.center().y() - d / 2, d, d);
        clip.addEllipse(target);
    } else {
        clip.addRoundedRect(rect, radius, radius);
    }
    if (!pixmap.isNull()) {
        painter->setClipPath(clip, Qt::IntersectClip);
        if (fit == Fit::Contain && !circle) {
            if (letterbox.isValid()) {
                painter->fillPath(clip, letterbox);
            }
            const QPixmap scaled = containScale(pixmap, target.size(), dpr);
            painter->drawPixmap(containedRect(scaled.size(), target, dpr).topLeft(), scaled);
        } else {
            painter->drawPixmap(target, coverCrop(pixmap, target.size(), dpr));
        }
    } else {
        QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
        gradient.setColorAt(0, tile.base);
        gradient.setColorAt(1, tile.base.lighter(135));
        painter->fillPath(clip, gradient);
        if (!tile.glyph.isEmpty()) {
            const QPixmap icon = icons::pixmap(tile.glyph, Qt::white, tile.glyphSize, dpr);
            painter->drawPixmap(target.center().x() - tile.glyphSize / 2, target.center().y() - tile.glyphSize / 2,
                                icon);
        }
    }
    painter->restore();
}

void paintKindBadge(QPainter* painter, const QRect& thumbRect, const QString& glyph, qreal dpr, int size)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRect badge(thumbRect.right() - size - 3, thumbRect.bottom() - size - 3, size, size);
    QPainterPath path;
    path.addRoundedRect(badge, 5, 5);
    painter->fillPath(path, QColor(0, 0, 0, 140));
    const int glyphSize = size * 2 / 3;
    painter->drawPixmap(badge.center().x() - glyphSize / 2, badge.center().y() - glyphSize / 2,
                        icons::pixmap(glyph, Qt::white, glyphSize, dpr));
    painter->restore();
}

void paintChip(QPainter* painter, const QRect& thumbRect, const QString& text, const QFont& font, qreal dpr,
               const QString& glyph, bool left)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QFont f = font;
    f.setPointSizeF(std::max(7.0, font.pointSizeF() - 1.5));
    f.setWeight(QFont::DemiBold);
    const QFontMetrics fm(f);
    const int glyphSize = glyph.isEmpty() ? 0 : 12;
    const int w = fm.horizontalAdvance(text) + 12 + (glyphSize > 0 ? glyphSize + 4 : 0);
    const int h = fm.height() + 4;
    const int x = left ? thumbRect.left() + 5 : thumbRect.right() - w - 5;
    const QRect chip(x, thumbRect.bottom() - h - 5, w, h);
    QPainterPath path;
    path.addRoundedRect(chip, 5, 5);
    painter->fillPath(path, QColor(0, 0, 0, 165));
    int tx = chip.left() + 6;
    if (glyphSize > 0) {
        painter->drawPixmap(tx, chip.center().y() - glyphSize / 2 + 1, icons::pixmap(glyph, Qt::white, glyphSize, dpr));
        tx += glyphSize + 4;
    }
    painter->setFont(f);
    painter->setPen(Qt::white);
    painter->drawText(QRect(tx, chip.top(), chip.right() - tx, chip.height()), Qt::AlignLeft | Qt::AlignVCenter, text);
    painter->restore();
}

} // namespace pldl::ui::thumbs
