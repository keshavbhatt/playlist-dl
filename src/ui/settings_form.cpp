#include "ui/settings_form.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui::settings_form {

QLabel* muted(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("pldlMuted", true);
    label->setWordWrap(true);
    return label;
}

QWidget* row(const QString& label, QWidget* control, const QString& note)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    auto* text = new QWidget(row);
    auto* v = new QVBoxLayout(text);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(2);
    auto* title = new QLabel(label, text);
    title->setWordWrap(true);
    v->addWidget(title);
    if (!note.isEmpty()) {
        v->addWidget(muted(note, text));
    }
    h->addWidget(text, 1, Qt::AlignVCenter);
    if (control != nullptr) {
        if (control->accessibleName().isEmpty()) {
            control->setAccessibleName(label);
        }
        control->setParent(row);
        h->addWidget(control, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
    return row;
}

QFrame* card(const QString& title, const QList<QWidget*>& rows, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setProperty("pldlCard", true);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(12);
    if (!title.isEmpty()) {
        auto* heading = new QLabel(title, card);
        QFont font = heading->font();
        font.setPixelSize(14);
        font.setWeight(QFont::Medium);
        heading->setFont(font);
        v->addWidget(heading);
    }
    for (QWidget* row : rows) {
        row->setParent(card);
        v->addWidget(row);
    }
    return card;
}

QWidget* page(const QString& title, const QString& subtitle, const QList<QWidget*>& cards, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* page = new QWidget(scroll);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);
    auto* head = new QWidget(page);
    auto* headLayout = new QVBoxLayout(head);
    headLayout->setContentsMargins(0, 0, 0, 4);
    headLayout->setSpacing(4);
    auto* heading = new QLabel(title, head);
    heading->setProperty("pldlHeading", true);
    headLayout->addWidget(heading);
    if (!subtitle.isEmpty()) {
        headLayout->addWidget(muted(subtitle, head));
    }
    layout->addWidget(head);
    for (QWidget* card : cards) {
        card->setParent(page);
        layout->addWidget(card);
    }
    layout->addStretch(1);
    page->setMaximumWidth(720);
    scroll->setWidget(page);
    return scroll;
}

} // namespace pldl::ui::settings_form
