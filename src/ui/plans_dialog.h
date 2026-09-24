#pragma once

#include <QDialog>

namespace pldl::core {
class ThemeService;
}
namespace pldl::services {
class LicenseService;
}

namespace pldl::ui {

/// The two plan cards of mocks/plans.html: Free (what it includes, "Continue
/// with Free") and Pro (accent border, PRO badge, "One-time purchase", "Buy
/// Pro"), the footnote about the account id, then Restore purchase and Close.
/// Opened from the account sheet and from the Pro gate.
class PlansDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlansDialog)

public:
    PlansDialog(services::LicenseService& license, const core::ThemeService& theme, QWidget* parent = nullptr);
    ~PlansDialog() override = default;
};

/// The feature lines the Account and Plans sheets share (mocks/account.html).
[[nodiscard]] QStringList accountFreeItems();
[[nodiscard]] QStringList accountProItems();

} // namespace pldl::ui
