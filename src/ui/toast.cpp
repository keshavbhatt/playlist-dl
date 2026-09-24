#include "ui/toast.h"

#include "ui/icons.h"
#include "ui/pldl_style.h"

#include <QAccessible>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kMargin = 16;
constexpr int kGap = 8;
constexpr int kWidth = 320;
} // namespace

ToastHost::ToastHost(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setObjectName(u"toastHost"_s);
    setAccessibleName(tr("Notifications"));
    if (parent != nullptr) {
        parent->installEventFilter(this);
    }
    reposition();
}

ToastHost::~ToastHost() = default;

void ToastHost::show(const QString& text, Kind kind)
{
    while (m_toasts.size() >= kMaxVisible) {
        dismiss(m_toasts.first().frame);
    }
    const bool dark = palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    auto* frame = new QFrame(this);
    frame->setProperty("pldlCard", true);
    frame->setObjectName(u"toast"_s);
    frame->setFixedWidth(kWidth);
    frame->setAccessibleName(text);
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(12, 10, 14, 10);
    layout->setSpacing(10);
    auto* glyph = new QLabel(frame);
    QString name = u"info"_s;
    QColor colour = t.accent;
    if (kind == Kind::Success) {
        name = u"check"_s;
        colour = t.success;
    } else if (kind == Kind::Error) {
        name = u"warning"_s;
        colour = t.danger;
    }
    glyph->setPixmap(icons::pixmap(name, colour, 18, devicePixelRatioF()));
    glyph->setFixedSize(18, 18);
    layout->addWidget(glyph);
    auto* label = new QLabel(text, frame);
    label->setWordWrap(true);
    layout->addWidget(label, 1);
    frame->adjustSize();
    frame->show();

    auto* timer = new QTimer(frame);
    timer->setSingleShot(true);
    timer->setInterval(kDismissMs);
    connect(timer, &QTimer::timeout, this, [this, frame] { dismiss(frame); });
    timer->start();
    frame->installEventFilter(this);
    m_toasts.append({frame, timer});
    layoutToasts();

    // Assistive technology hears the toast even though focus never moves.
    QAccessibleEvent event(frame, QAccessible::Alert);
    QAccessible::updateAccessibility(&event);
    Q_EMIT shown(text);
}

void ToastHost::dismiss(QFrame* frame)
{
    for (int i = 0; i < m_toasts.size(); ++i) {
        if (m_toasts.at(i).frame == frame) {
            m_toasts.removeAt(i);
            frame->hide();
            frame->deleteLater();
            break;
        }
    }
    layoutToasts();
}

void ToastHost::layoutToasts()
{
    int height = 0;
    for (const Toast& toast : m_toasts) {
        toast.frame->adjustSize();
        height += toast.frame->height() + kGap;
    }
    resize(kWidth, std::max(1, height));
    int y = height;
    for (const Toast& toast : m_toasts) {
        y -= toast.frame->height() + kGap;
        toast.frame->move(0, y + kGap);
    }
    setVisible(!m_toasts.isEmpty());
    reposition();
}

void ToastHost::reposition()
{
    if (parentWidget() == nullptr) {
        return;
    }
    move(kMargin, parentWidget()->height() - height() - kMargin);
    raise();
}

bool ToastHost::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        reposition();
        return false;
    }
    for (const Toast& toast : m_toasts) {
        if (watched == toast.frame) {
            if (event->type() == QEvent::Enter) {
                toast.timer->stop();
            } else if (event->type() == QEvent::Leave) {
                toast.timer->start();
            } else if (event->type() == QEvent::MouseButtonPress) {
                dismiss(toast.frame);
                return true;
            }
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace pldl::ui
