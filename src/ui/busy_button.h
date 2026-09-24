#pragma once

#include <QString>

class QPushButton;

namespace pldl::ui::busy {

/// Puts a button into its busy state (DESIGN.md section 5): a spinning arc,
/// the progress verb as label ("Probing"), disabled until `set(button, false)`
/// restores the original text, icon and enabled state. Idempotent. With
/// `cancellable` the button stays enabled and says so in its tooltip: a
/// click while busy is the owner's cue to cancel (busy::isBusy tells).
void set(QPushButton* button, bool busy, const QString& verb = {}, bool cancellable = false);
[[nodiscard]] bool isBusy(const QPushButton* button);

} // namespace pldl::ui::busy
