#pragma once

#include <QList>
#include <QString>

class QFrame;
class QLabel;
class QWidget;

/// The building blocks of a Settings page (DESIGN.md section 3): a scrolling
/// page with a title and a subtitle, cards with a 14/500 title, and rows
/// with the label on the left and the control on the right. Pure layout, no
/// settings access; the dialog wires the controls.
namespace pldl::ui::settings_form {

/// A muted, word-wrapped label.
[[nodiscard]] QLabel* muted(const QString& text, QWidget* parent);

/// A row: `label` on the left (with an optional muted `note` under it), the
/// control right-aligned, 12 px between them. The control takes the label as
/// its accessible name when it has none. A null control gives a text-only row.
[[nodiscard]] QWidget* row(const QString& label, QWidget* control, const QString& note = {});

/// A card (`pldlCard`) with a title and the rows 12 px apart.
[[nodiscard]] QFrame* card(const QString& title, const QList<QWidget*>& rows, QWidget* parent);

/// A page: the heading, the subtitle and the cards in a scroll area; wheel
/// events reach the scroll bar, not a control the pointer rests on (the
/// dialog's wheel guard does that part).
[[nodiscard]] QWidget* page(const QString& title, const QString& subtitle, const QList<QWidget*>& cards,
                            QWidget* parent);

} // namespace pldl::ui::settings_form
