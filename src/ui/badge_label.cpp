#include "ui/badge_label.h"

#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QFontMetrics>
#include <QPainter>

#include <algorithm>

namespace pldl::ui {

namespace {
constexpr int kPadH = 8;       ///< the badge rule's 8 px side padding
constexpr int kPadV = 2;
constexpr int kGap = 5;        ///< glyph to words
constexpr int kRadius = 10;
constexpr int kFontPx = 11;
constexpr int kSoftAlpha = 46; ///< the soft tint of the style sheet: 18% of 255
} // namespace

BadgeLabel::BadgeLabel(core::ThemeService& theme, QWidget* parent)
    : QLabel(parent)
    , m_theme(theme)
{
    setTextFormat(Qt::PlainText); // the words, never markup: text() is what is read out
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    QFont badge = font();
    badge.setPixelSize(kFontPx);
    badge.setWeight(QFont::DemiBold);
    setFont(badge);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { update(); });
}

BadgeLabel::~BadgeLabel() = default;

void BadgeLabel::setGlyph(const QString& name)
{
    if (m_glyph != name) {
        m_glyph = name;
        updateGeometry();
        update();
    }
}

void BadgeLabel::setOk(bool ok)
{
    setTone(ok ? Tone::Ok : Tone::Accent);
}

void BadgeLabel::setTone(Tone tone)
{
    if (m_tone != tone) {
        m_tone = tone;
        update();
    }
}

QSize BadgeLabel::sizeHint() const
{
    const QFontMetrics metrics(font());
    int width = (kPadH * 2) + metrics.horizontalAdvance(text());
    if (!m_glyph.isEmpty()) {
        width += kGlyph + kGap;
    }
    const int height = std::max({kHeight, kGlyph + (kPadV * 2), metrics.height() + (kPadV * 2)});
    return {width, height};
}

QSize BadgeLabel::minimumSizeHint() const
{
    return sizeHint();
}

void BadgeLabel::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    QColor ink = t.accent;
    QColor fill = t.accentSoft;
    switch (m_tone) {
    case Tone::Accent:
        break;
    case Tone::Ok:
        ink = t.success;
        break;
    case Tone::Warning:
        ink = t.warning;
        break;
    case Tone::Danger:
        ink = t.danger;
        break;
    }
    if (m_tone != Tone::Accent) {
        fill = ink;
        fill.setAlpha(kSoftAlpha); // the tint the style sheet mixes for .badge.ok
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF pill = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = std::min<qreal>(kRadius, pill.height() / 2.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(pill, radius, radius);

    // The glyph sits on the text's line, not above it: its box is centred in
    // the pill, which is what rich text could not be made to do.
    int left = kPadH;
    if (!m_glyph.isEmpty()) {
        const QRect box(left, (height() - kGlyph) / 2, kGlyph, kGlyph);
        painter.drawPixmap(box, icons::pixmap(m_glyph, ink, kGlyph, devicePixelRatioF()));
        left = box.right() + 1 + kGap;
    }
    painter.setPen(ink);
    const QRect words(left, 0, std::max(0, width() - kPadH - left), height());
    painter.drawText(words, Qt::AlignVCenter | Qt::AlignLeft, text());
}

} // namespace pldl::ui
