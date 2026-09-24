#pragma once

class QWidget;

namespace pldl::ui::keyboard {

/// Left and Right (and Up and Down) move focus between the buttons inside
/// `container`, so a chip row or a rail behaves like one control for the
/// keyboard (DESIGN.md section 5). Home and End jump to the ends. Safe to
/// call again after buttons were added (the Browser page's tabs); buttons
/// with the `pldlArrowSkip` property (a tab's close button) are left out of
/// the ring but stay reachable with Tab.
void installArrowNavigation(QWidget* container);

} // namespace pldl::ui::keyboard
