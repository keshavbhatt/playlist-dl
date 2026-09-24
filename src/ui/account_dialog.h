#pragma once

#include <QDialog>

class QFrame;
class QShowEvent;
class QLabel;
class QPushButton;

namespace pldl::core {
class ThemeService;
}
namespace pldl::services {
class LicenseService;
}

namespace pldl::ui {

/// Account & licence (mocks/account.html): "Your account" with the id (and
/// copy), the plan badge, the status (with the last verification), the expiry
/// or the evaluation's end, the next check, this device, then Restore purchase
/// and the self-service portal; "Plan" with what Free includes and what Pro
/// adds in one line and See plans; and, during the evaluation (or once it has ended), the
/// banner with the days left and Upgrade. Follows LicenseService live.
class AccountDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AccountDialog)

public:
    AccountDialog(services::LicenseService& license, const core::ThemeService& theme,
                  QWidget* parent = nullptr);
    ~AccountDialog() override = default;

    /// Draws the eye to the Upgrade button (after a gate prompt).
    void highlightPurchase();
    /// The Free and Pro cards (mocks/plans.html).
    void showPlans();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void refresh();
    void fitToContent();
    QWidget* buildAccountCard();
    QWidget* buildPlanCard();
    QWidget* buildBanner();

    services::LicenseService& m_license;
    const core::ThemeService& m_theme;
    QLabel* m_id = nullptr;
    QPushButton* m_copy = nullptr;
    QLabel* m_planBadge = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_expiryKey = nullptr;
    QLabel* m_expiry = nullptr;
    QLabel* m_nextKey = nullptr;
    QLabel* m_next = nullptr;
    QLabel* m_devices = nullptr;
    QLabel* m_note = nullptr;
    QLabel* m_planSummary = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_portal = nullptr;
    QFrame* m_banner = nullptr;
    QLabel* m_bannerIcon = nullptr;
    QLabel* m_bannerText = nullptr;
    QPushButton* m_upgrade = nullptr;
};

} // namespace pldl::ui
