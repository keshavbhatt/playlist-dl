#include "ui/address_field.h"

#include "core/theme/theme_service.h"
#include "ui/pldl_style.h"

#include <QPainter>
#include <QPainterPath>

using namespace Qt::StringLiterals;

namespace pldl::ui {

AddressField::AddressField(const core::ThemeService& theme, QWidget* parent)
    : QLineEdit(parent)
    , m_theme(theme)
{
}

void AddressField::setProgress(int percent)
{
    const int clamped = percent < 0 ? -1 : std::min(100, percent);
    if (clamped == m_progress) {
        return;
    }
    m_progress = clamped;
    setAccessibleDescription(clamped < 0 ? QString() : tr("Loading, %1%").arg(clamped));
    update();
}

void AddressField::paintEvent(QPaintEvent* event)
{
    QLineEdit::paintEvent(event);
    if (m_progress < 0) {
        return;
    }
    // A 2 px accent line inside the bottom edge, inset from the rounded
    // corners; the field's own frame stays untouched, so nothing else moves.
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    constexpr int kInset = 9;
    constexpr int kHeight = 2;
    const int available = width() - 2 * kInset;
    if (available <= 0) {
        return;
    }
    const int span = std::max(kHeight * 2, available * m_progress / 100);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(kInset, height() - kInset / 2 - kHeight, span, kHeight), 1, 1);
    p.fillPath(path, t.accent);
}

} // namespace pldl::ui
