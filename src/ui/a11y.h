#pragma once

class QWidget;

namespace pldl::ui::a11y {

/// Gives every text field, combo, spin box, check box and glyph-only button
/// under `root` an accessible name when it has none, taken from the label
/// that sits to its left or right above it in the laid-out geometry (the way a
/// sighted user reads the form). Explicit names set by the owner are kept.
/// Call once the widget tree is built; cheap enough to call again after
/// rebuilding a page.
void nameControlsFromLabels(QWidget* root);

/// Moves keyboard focus to where a sheet's work starts: the first editable
/// text field, else the first enabled check, chip or button that is not the
/// close button. Deferred to the next event loop turn so it wins over Qt's
/// default (the first widget in the tab chain, often the close button).
void focusFirstField(QWidget* root);

} // namespace pldl::ui::a11y
