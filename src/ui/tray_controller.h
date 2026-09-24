#pragma once

#include <QIcon>
#include <QObject>
#include <QTimer>

class QAction;
class QMenu;
class QSystemTrayIcon;

namespace pldl::core {
class Settings;
}

namespace pldl::ui {

class Actions;

/// System tray icon with the way back to the window, the downloads and quit.
/// Never the only way back to the window: callers must check isAvailable()
/// before hiding.
class TrayController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TrayController)

public:
    TrayController(core::Settings& settings, Actions& actions, QObject* parent = nullptr);
    ~TrayController() override;

    [[nodiscard]] bool isAvailable() const;
    void setWindowVisible(bool visible);
    /// Active downloads, shown in the tooltip.
    void setActiveDownloads(int count);
    [[nodiscard]] QMenu* menu() const { return m_menu; }
    /// Shows the icon if a panel appeared since the last check (called when the window is first shown).
    void recheckAvailability();

Q_SIGNALS:
    void toggleRequested();

private:
    void buildMenu();
    void applyTrayVisibility();
    void updateTooltip();

    core::Settings& m_settings;
    Actions& m_actions;
    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    QIcon m_icon;
    int m_active = 0;
    QTimer* m_recheck = nullptr;
    int m_rechecks = 0;
};

} // namespace pldl::ui
