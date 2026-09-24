#include "ui/range_slider.h"

#include "core/theme/theme_service.h"
#include "ui/pldl_style.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScopeGuard>

#include <algorithm>

namespace pldl::ui {

namespace {
constexpr int kKnob = 16;
constexpr int kTrack = 4;
/// The focus ring reaches 5 px past the knob (radius + 3, pen 2): the widget
/// leaves that room on every side so the ring is never clipped.
constexpr int kRing = 5;
constexpr int kInset = kKnob / 2 + kRing;
constexpr int kHeight = kKnob + 2 * kRing + 2;
} // namespace

RangeSlider::RangeSlider(core::ThemeService& theme, QWidget* parent)
    : QWidget(parent)
    , m_theme(theme)
{
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setMinimumWidth(120);
    setFixedHeight(kHeight);
}

QSize RangeSlider::sizeHint() const
{
    return {220, kHeight};
}

QSize RangeSlider::minimumSizeHint() const
{
    return {120, kHeight};
}

void RangeSlider::setRange(int minimum, int maximum)
{
    m_min = std::min(minimum, maximum);
    m_max = std::max(minimum, maximum);
    setValues(m_min, m_max);
}

void RangeSlider::setValues(int lower, int upper)
{
    const int lo = std::clamp(std::min(lower, upper), m_min, m_max);
    const int hi = std::clamp(std::max(lower, upper), m_min, m_max);
    if (lo == m_lower && hi == m_upper) {
        return;
    }
    m_lower = lo;
    m_upper = hi;
    update();
    Q_EMIT valuesChanged(m_lower, m_upper);
}

int RangeSlider::xFor(int value) const
{
    const int span = std::max(1, m_max - m_min);
    const int usable = width() - 2 * kInset;
    return kInset + (value - m_min) * usable / span;
}

int RangeSlider::valueFor(int x) const
{
    const int usable = std::max(1, width() - 2 * kInset);
    const int span = m_max - m_min;
    const double fraction = std::clamp(static_cast<double>(x - kInset) / usable, 0.0, 1.0);
    return m_min + static_cast<int>(std::lround(fraction * span));
}

void RangeSlider::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int y = height() / 2;
    p.setPen(Qt::NoPen);
    p.setBrush(t.border);
    p.drawRoundedRect(QRect(kInset, y - kTrack / 2, width() - 2 * kInset, kTrack), 2, 2);
    const int x0 = xFor(m_lower);
    const int x1 = xFor(m_upper);
    p.setBrush(t.accent);
    p.drawRoundedRect(QRect(x0, y - kTrack / 2, std::max(0, x1 - x0), kTrack), 2, 2);
    int knob = 0;
    for (const int x : {x0, x1}) {
        p.setBrush(t.accent);
        p.setPen(QPen(t.panel, 2));
        p.drawEllipse(QPoint(x, y), kKnob / 2 - 1, kKnob / 2 - 1);
        if (hasFocus() && knob == m_activeKnob) {
            // The focus ring on the knob the keyboard moves (DESIGN.md section 5).
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(t.accent, 2));
            p.drawEllipse(QPoint(x, y), kKnob / 2 + 3, kKnob / 2 + 3);
        }
        ++knob;
    }
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
    const int x = static_cast<int>(event->position().x());
    const int d0 = std::abs(x - xFor(m_lower));
    const int d1 = std::abs(x - xFor(m_upper));
    m_dragging = d0 <= d1 ? 0 : 1;
    m_activeKnob = m_dragging;
    setFocus(Qt::MouseFocusReason);
    mouseMoveEvent(event);
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
    const int lowerBefore = m_lower;
    const int upperBefore = m_upper;
    const auto announce = qScopeGuard([this, lowerBefore, upperBefore] {
        if (m_lower != lowerBefore || m_upper != upperBefore) {
            Q_EMIT userChanged(m_lower, m_upper);
        }
    });
    if (m_dragging < 0) {
        return;
    }
    const int value = valueFor(static_cast<int>(event->position().x()));
    if (m_dragging == 0) {
        setValues(std::min(value, m_upper), m_upper);
    } else {
        setValues(m_lower, std::max(value, m_lower));
    }
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    m_dragging = -1;
}

void RangeSlider::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
}

void RangeSlider::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

void RangeSlider::keyPressEvent(QKeyEvent* event)
{
    const int lowerBefore = m_lower;
    const int upperBefore = m_upper;
    const auto announce = qScopeGuard([this, lowerBefore, upperBefore] {
        if (m_lower != lowerBefore || m_upper != upperBefore) {
            Q_EMIT userChanged(m_lower, m_upper);
        }
    });
    // Left and Right move the active knob (so a range can be narrowed as well
    // as widened); Tab within the widget switches the knob; Home and End jump.
    const bool lower = m_activeKnob == 0;
    switch (event->key()) {
    case Qt::Key_Left:
        if (lower) {
            setValues(m_lower - 1, m_upper);
        } else {
            setValues(m_lower, std::max(m_lower, m_upper - 1));
        }
        break;
    case Qt::Key_Right:
        if (lower) {
            setValues(std::min(m_lower + 1, m_upper), m_upper);
        } else {
            setValues(m_lower, m_upper + 1);
        }
        break;
    case Qt::Key_Home:
        if (lower) {
            setValues(m_min, m_upper);
        } else {
            setValues(m_lower, m_lower);
        }
        break;
    case Qt::Key_End:
        if (lower) {
            setValues(m_upper, m_upper);
        } else {
            setValues(m_lower, m_max);
        }
        break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        if ((event->key() == Qt::Key_Tab && lower) || (event->key() == Qt::Key_Backtab && !lower)) {
            m_activeKnob = lower ? 1 : 0;
            update();
            break;
        }
        QWidget::keyPressEvent(event);
        break;
    default:
        QWidget::keyPressEvent(event);
    }
    update();
}

} // namespace pldl::ui
