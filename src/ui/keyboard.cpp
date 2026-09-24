#include "ui/keyboard.h"

#include <QAbstractButton>
#include <QEvent>
#include <QKeyEvent>
#include <QList>
#include <QVariant>
#include <QWidget>

namespace pldl::ui::keyboard {

namespace {

class ArrowFilter : public QObject
{
public:
    explicit ArrowFilter(QWidget* container)
        : QObject(container)
        , m_container(container)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::KeyPress) {
            return false;
        }
        auto* key = static_cast<QKeyEvent*>(event);
        int step = 0;
        bool toEnd = false;
        switch (key->key()) {
        case Qt::Key_Right:
        case Qt::Key_Down:
            step = 1;
            break;
        case Qt::Key_Left:
        case Qt::Key_Up:
            step = -1;
            break;
        case Qt::Key_Home:
            step = -1;
            toEnd = true;
            break;
        case Qt::Key_End:
            step = 1;
            toEnd = true;
            break;
        default:
            return false;
        }
        auto* current = qobject_cast<QAbstractButton*>(watched);
        if (current == nullptr) {
            return false;
        }
        QList<QAbstractButton*> buttons;
        for (QAbstractButton* button : m_container->findChildren<QAbstractButton*>()) {
            if (button->isVisible() && button->isEnabled() && button->focusPolicy() != Qt::NoFocus &&
                !button->property("pldlArrowSkip").toBool()) {
                buttons << button;
            }
        }
        const qsizetype index = buttons.indexOf(current);
        if (index < 0 || buttons.size() < 2) {
            return false;
        }
        qsizetype next = 0;
        if (toEnd) {
            next = step < 0 ? 0 : buttons.size() - 1;
        } else {
            next = (index + step + buttons.size()) % buttons.size();
        }
        buttons.at(next)->setFocus(Qt::TabFocusReason);
        return true;
    }

private:
    QWidget* m_container;
};

} // namespace

void installArrowNavigation(QWidget* container)
{
    if (container == nullptr) {
        return;
    }
    // One filter per container, remembered on it so a second call reuses it.
    auto* filter = static_cast<ArrowFilter*>(container->property("pldlArrowFilter").value<QObject*>());
    if (filter == nullptr) {
        filter = new ArrowFilter(container);
        container->setProperty("pldlArrowFilter", QVariant::fromValue<QObject*>(filter));
    }
    for (QAbstractButton* button : container->findChildren<QAbstractButton*>()) {
        button->installEventFilter(filter); // a second install of the same filter is a no-op
    }
}

} // namespace pldl::ui::keyboard
