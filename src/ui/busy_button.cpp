#include "ui/busy_button.h"

#include "ui/pldl_style.h"

#include <QAccessible>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVariant>

using namespace Qt::StringLiterals;

namespace pldl::ui::busy {

namespace {
// 30 frames a second, 12 degrees a frame: a smooth turn every second.
constexpr int kFrameMs = 33;
constexpr int kFrames = 30;
constexpr int kGlyph = 16;

class Spinner : public QTimer
{
public:
    explicit Spinner(QPushButton* button)
        : QTimer(button)
        , m_button(button)
    {
        setInterval(kFrameMs);
        connect(this, &QTimer::timeout, this, &Spinner::tick);
    }

private:
    void tick()
    {
        m_frame = (m_frame + 1) % kFrames;
        const bool dark = m_button->palette().window().color().lightness() < 128;
        const Tokens t = Tokens::forScheme(dark);
        const QColor colour = m_button->property("pldlPrimary").toBool() ? Tokens::textOn(t.accentStrong) : t.text;
        // Drawn fresh each frame (an arc with a faint full ring behind it):
        // rotating a rendered glyph in twelve steps looked jerky and blurred.
        const qreal dpr = m_button->devicePixelRatioF();
        QPixmap glyph(QSize(kGlyph, kGlyph) * dpr);
        glyph.setDevicePixelRatio(dpr);
        glyph.fill(Qt::transparent);
        QPainter p(&glyph);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF ring(2.0, 2.0, kGlyph - 4.0, kGlyph - 4.0);
        QColor track = colour;
        track.setAlphaF(0.25F);
        p.setPen(QPen(track, 2.0));
        p.drawEllipse(ring);
        p.setPen(QPen(colour, 2.0, Qt::SolidLine, Qt::RoundCap));
        const int start = 90 - (360 * m_frame / kFrames); // clockwise from the top
        p.drawArc(ring, start * 16, -100 * 16);
        p.end();
        m_button->setIcon(QIcon(glyph));
    }

    QPushButton* m_button;
    int m_frame = 0;
};

QTimer* spinnerOf(QPushButton* button)
{
    return button->findChild<QTimer*>(u"pldlBusySpinner"_s, Qt::FindDirectChildrenOnly);
}
} // namespace

bool isBusy(const QPushButton* button)
{
    return button != nullptr && button->property("pldlBusy").toBool();
}

void set(QPushButton* button, bool busy, const QString& verb, bool cancellable)
{
    if (button == nullptr || isBusy(button) == busy) {
        if (busy && button != nullptr && !verb.isEmpty()) {
            button->setText(verb);
        }
        return;
    }
    if (busy) {
        button->setProperty("pldlRestoreText", button->text());
        button->setProperty("pldlRestoreIcon", button->icon());
        button->setProperty("pldlRestoreEnabled", button->isEnabled());
        button->setProperty("pldlRestoreToolTip", button->toolTip());
        button->setProperty("pldlBusy", true);
        button->setProperty("pldlCancellable", cancellable);
        button->setText(verb.isEmpty() ? button->text() : verb);
        button->setEnabled(cancellable);
        if (cancellable) {
            button->setToolTip(QPushButton::tr("Click to cancel (Esc)"));
            button->setAccessibleDescription(QPushButton::tr("Working. Activate to cancel."));
        } else {
            button->setAccessibleDescription(QPushButton::tr("Working"));
        }
        auto* spinner = new Spinner(button);
        spinner->setObjectName(u"pldlBusySpinner"_s);
        spinner->start();
        QAccessibleEvent event(button, QAccessible::StateChanged);
        QAccessible::updateAccessibility(&event);
    } else {
        if (QTimer* spinner = spinnerOf(button)) {
            spinner->stop();
            spinner->deleteLater();
        }
        button->setProperty("pldlBusy", false);
        button->setProperty("pldlCancellable", false);
        button->setText(button->property("pldlRestoreText").toString());
        button->setIcon(button->property("pldlRestoreIcon").value<QIcon>());
        button->setEnabled(button->property("pldlRestoreEnabled").toBool());
        button->setToolTip(button->property("pldlRestoreToolTip").toString());
        button->setAccessibleDescription(QString());
    }
    button->style()->unpolish(button);
    button->style()->polish(button);
}

} // namespace pldl::ui::busy
