#include "ui/browser_tab.h"

#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kPad = 10;
constexpr int kGlyph = 16;
constexpr int kGap = 8;
constexpr int kClose = 28; ///< the largest target the 34 px tab can hold
constexpr int kRadius = 8;
} // namespace

BrowserTabButton::BrowserTabButton(core::ThemeService& theme, QWidget* parent)
    : QAbstractButton(parent)
    , m_theme(theme)
    , m_close(new QToolButton(this))
    , m_glyph(u"globe"_s)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setMaximumWidth(kMaxWidth);
    setAttribute(Qt::WA_Hover, true);
    // Not pldlFlat: its 8 px side padding would leave a 22 px button no room
    // for the glyph. Its own rule keeps the glyph at 14 px.
    m_close->setProperty("pldlTabClose", true);
    m_close->setProperty("pldlArrowSkip", true); // Left and Right hop tab to tab
    m_close->setAutoRaise(true);
    m_close->setFixedSize(kClose, kClose);
    m_close->setIconSize(QSize(14, 14));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setFocusPolicy(Qt::TabFocus);
    m_close->setToolTip(tr("Close tab"));
    m_close->setAccessibleName(tr("Close tab"));
    connect(m_close, &QToolButton::clicked, this, &BrowserTabButton::closeRequested);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) { applyTheme(); });
    applyTheme();
    setTitle(tr("New tab"));
}

BrowserTabButton::~BrowserTabButton() = default;

void BrowserTabButton::setTitle(const QString& title)
{
    m_title = title.trimmed().isEmpty() ? tr("New tab") : title.trimmed();
    setText(m_title); // the accessible name and the tooltip follow the title
    setToolTip(m_title);
    setAccessibleName(m_title);
    m_close->setAccessibleName(tr("Close tab: %1").arg(m_title));
    update();
}

void BrowserTabButton::setSiteIcon(const QIcon& icon)
{
    m_siteIcon = icon;
    update();
}

void BrowserTabButton::setGlyph(const QString& glyph)
{
    if (m_glyph != glyph) {
        m_glyph = glyph;
        update();
    }
}

void BrowserTabButton::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    if (m_spin == nullptr) {
        m_spin = new QTimer(this);
        m_spin->setInterval(33);
        connect(m_spin, &QTimer::timeout, this, &BrowserTabButton::tick);
    }
    if (loading) {
        m_spin->start();
    } else {
        m_spin->stop();
        m_spinAngle = 0;
    }
    update();
}

void BrowserTabButton::tick()
{
    m_spinAngle = (m_spinAngle + 12) % 360; // a turn a second, like the busy button
    update();
}

QSize BrowserTabButton::sizeHint() const
{
    const int text = fontMetrics().horizontalAdvance(m_title);
    const int width = kPad + kGlyph + kGap + text + kGap + kClose + (kPad / 2);
    return {std::clamp(width, kMinWidth, kMaxWidth), kHeight};
}

QSize BrowserTabButton::minimumSizeHint() const
{
    return {kCompactWidth, kHeight};
}

void BrowserTabButton::paintEvent(QPaintEvent* /*event*/)
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect();
    // The tab shape: rounded top corners, open at the bottom, drawn past the
    // bottom edge so the strip's border does not cut under the active tab.
    QPainterPath path;
    path.moveTo(r.left(), r.bottom() + 1);
    path.lineTo(r.left(), r.top() + kRadius);
    path.arcTo(r.left(), r.top(), kRadius * 2, kRadius * 2, 180, -90);
    path.lineTo(r.right() - kRadius, r.top());
    path.arcTo(r.right() - (kRadius * 2), r.top(), kRadius * 2, kRadius * 2, 90, -90);
    path.lineTo(r.right(), r.bottom() + 1);
    path.closeSubpath();
    if (isChecked()) {
        // The page background alone is too close to the strip in dark mode:
        // a hairline outline and an accent bar along the top make the active
        // tab unmistakable in both schemes (owner feedback, 2026-09-19).
        p.fillPath(path, t.bg);
        p.setPen(QPen(t.border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.save();
        p.setClipPath(path);
        p.fillRect(QRectF(r.left(), r.top(), r.width(), 3), t.accent);
        p.restore();
    } else if (isDown()) {
        p.fillPath(path, t.accentSoft);
    } else if (m_hover) {
        p.fillPath(path, t.hover);
    }
    if (hasFocus()) {
        QPen pen(t.accent, 2);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(1, 1, -1, 1), kRadius, kRadius);
    }

    // Glyph or site icon, "loader" while the page loads. A compact tab (many
    // tabs, no room for words) centres the glyph and shows nothing else.
    const QColor ink = isChecked() ? t.text : t.muted;
    const bool compact = width() < kMinWidth;
    const QRect glyphRect(compact ? (width() - kGlyph) / 2 : kPad, (height() - kGlyph) / 2, kGlyph, kGlyph);
    if (m_loading) {
        // The loader turns while the page loads; a still glyph read as a freeze.
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.translate(glyphRect.center());
        p.rotate(m_spinAngle);
        p.drawPixmap(QRect(-kGlyph / 2, -kGlyph / 2, kGlyph, kGlyph),
                     icons::pixmap(u"loader"_s, t.accent, kGlyph, devicePixelRatioF()));
        p.restore();
    } else if (!m_siteIcon.isNull()) {
        m_siteIcon.paint(&p, glyphRect);
    } else {
        p.drawPixmap(glyphRect, icons::pixmap(m_glyph, ink, kGlyph, devicePixelRatioF()));
    }

    if (compact) {
        return; // the tooltip carries the title
    }
    // Title, elided to the room left of the close button.
    const int textLeft = glyphRect.right() + 1 + kGap;
    const int textRight = m_close->isVisible() ? m_close->x() - kGap : width() - kPad;
    const QRect textRect(textLeft, 0, std::max(0, textRight - textLeft), height());
    p.setPen(ink);
    QFont f = font();
    f.setWeight(isChecked() ? QFont::DemiBold : QFont::Normal);
    p.setFont(f);
    p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
               QFontMetrics(f).elidedText(m_title, Qt::ElideRight, textRect.width()));
}

void BrowserTabButton::resizeEvent(QResizeEvent* event)
{
    QAbstractButton::resizeEvent(event);
    placeClose();
}

void BrowserTabButton::placeClose()
{
    // Below the minimum there is no room for both the glyph and the close
    // button: the close button gives way (the tab still closes with the
    // keyboard and the middle button).
    const bool fits = width() >= kMinWidth + 20;
    m_close->setVisible(fits);
    m_close->move(width() - kClose - (kPad / 2), (height() - kClose) / 2);
}

void BrowserTabButton::enterEvent(QEnterEvent* event)
{
    m_hover = true;
    update();
    QAbstractButton::enterEvent(event);
}

void BrowserTabButton::leaveEvent(QEvent* event)
{
    m_hover = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void BrowserTabButton::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        Q_EMIT closeRequested();
        event->accept();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void BrowserTabButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && rect().contains(event->pos())) {
        Q_EMIT closeRequested();
        event->accept();
        return;
    }
    QAbstractButton::mouseReleaseEvent(event);
}

void BrowserTabButton::changeEvent(QEvent* event)
{
    QAbstractButton::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
    }
}

void BrowserTabButton::applyTheme()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    m_close->setIcon(icons::themed(u"cancel"_s, t.muted));
    update();
}

} // namespace pldl::ui
