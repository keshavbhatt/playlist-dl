#include "ui/a11y.h"

#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QTimer>
#include <QRadioButton>
#include <QRegularExpression>
#include <QToolButton>
#include <QWidget>

using namespace Qt::StringLiterals;

namespace pldl::ui::a11y {

namespace {

QString plainText(const QLabel* label)
{
    QString text = label->text();
    if (label->textFormat() != Qt::PlainText && text.contains(u'<')) {
        static const QRegularExpression kTags(u"<[^>]*>"_s);
        text.remove(kTags);
    }
    text = text.trimmed();
    while (text.endsWith(u':') || text.endsWith(u'…')) {
        text.chop(1);
        text = text.trimmed();
    }
    return text;
}

QRect inRoot(const QWidget* widget, const QWidget* root)
{
    return QRect(widget->mapTo(root, QPoint(0, 0)), widget->size());
}

bool needsName(const QWidget* widget)
{
    if (!widget->accessibleName().isEmpty()) {
        return false;
    }
    if (widget->objectName().startsWith(u"qt_"_s)) {
        return false;
    }
    if (const auto* edit = qobject_cast<const QLineEdit*>(widget)) {
        return edit->placeholderText().trimmed().isEmpty();
    }
    if (qobject_cast<const QComboBox*>(widget) != nullptr || qobject_cast<const QAbstractSpinBox*>(widget) != nullptr) {
        return widget->toolTip().trimmed().isEmpty();
    }
    if (const auto* button = qobject_cast<const QAbstractButton*>(widget)) {
        return button->text().trimmed().isEmpty() && button->toolTip().trimmed().isEmpty();
    }
    return false;
}

} // namespace

void nameControlsFromLabels(QWidget* root)
{
    if (root == nullptr) {
        return;
    }
    // Geometry only exists once the layouts ran.
    for (QLayout* layout : root->findChildren<QLayout*>()) {
        layout->activate();
    }
    if (root->layout() != nullptr) {
        root->layout()->activate();
    }
    QList<QLabel*> labels;
    for (QLabel* label : root->findChildren<QLabel*>()) {
        if (!plainText(label).isEmpty() && label->pixmap().isNull()) {
            labels << label;
        }
    }
    const QList<QWidget*> controls = root->findChildren<QWidget*>();
    for (QWidget* control : controls) {
        if (!needsName(control)) {
            continue;
        }
        // A spin box names its own line edit.
        if (control->parentWidget() != nullptr
            && qobject_cast<QAbstractSpinBox*>(control->parentWidget()) != nullptr) {
            continue;
        }
        const QRect box = inRoot(control, root);
        QLabel* best = nullptr;
        int bestScore = 1 << 30;
        for (QLabel* label : labels) {
            const QRect lb = inRoot(label, root);
            if (lb.isEmpty() || box.isEmpty()) {
                continue;
            }
            const bool sameRow = lb.top() < box.bottom() && lb.bottom() > box.top();
            int score = 0;
            if (sameRow && lb.right() <= box.left() + 4) {
                score = box.left() - lb.right(); // to the left, closest first
            } else if (lb.bottom() <= box.top() + 4 && lb.left() >= box.left() - 240 && lb.left() <= box.right()) {
                score = 1000 + (box.top() - lb.bottom()); // above, closest first
            } else if (sameRow && lb.left() >= box.right() - 4) {
                score = 2000 + (lb.left() - box.right()); // a check box's own text to the right
            } else {
                continue;
            }
            if (score < bestScore) {
                bestScore = score;
                best = label;
            }
        }
        if (best != nullptr) {
            control->setAccessibleName(plainText(best));
        }
    }
}

void focusFirstField(QWidget* root)
{
    if (root == nullptr) {
        return;
    }
    QTimer::singleShot(0, root, [root] {
        QWidget* target = nullptr;
        for (QWidget* widget : root->findChildren<QWidget*>()) {
            if (!widget->isVisible() || !widget->isEnabled() || widget->focusPolicy() == Qt::NoFocus) {
                continue;
            }
            if (auto* edit = qobject_cast<QLineEdit*>(widget); edit != nullptr && !edit->isReadOnly()) {
                target = edit;
                break;
            }
            if (qobject_cast<QPlainTextEdit*>(widget) != nullptr || qobject_cast<QTextEdit*>(widget) != nullptr) {
                target = widget;
                break;
            }
        }
        if (target == nullptr) {
            for (auto* button : root->findChildren<QAbstractButton*>()) {
                if (button->isVisible() && button->isEnabled() && button->focusPolicy() != Qt::NoFocus
                    && button->objectName() != u"closeButton"_s && !button->accessibleName().contains(u"Close"_s)
                    && button->toolTip() != QAbstractButton::tr("Close")) {
                    target = button;
                    break;
                }
            }
        }
        if (target != nullptr) {
            target->setFocus(Qt::OtherFocusReason);
        }
    });
}

} // namespace pldl::ui::a11y
