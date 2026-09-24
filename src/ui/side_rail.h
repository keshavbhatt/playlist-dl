#pragma once

#include <QFrame>

class QToolButton;
class QLabel;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class Actions;

/// The 56 px rail on the left of the window (DESIGN.md section 2): the brand
/// mark, the four pages (Search, Playlist, Browser, Downloads), then Settings
/// and Account at the bottom. Pure
/// presentation over Actions; the checked page action is the active one.
class SideRail : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SideRail)

public:
    SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent = nullptr);
    ~SideRail() override = default;

    /// Active-download count badge on the Downloads button (0 hides it).
    void setActiveDownloads(int count);

private:
    void setupUi();
    void applyIcons();
    QToolButton* makeButton(QAction* action, const QString& icon);

    Actions& m_actions;
    core::ThemeService& m_theme;
    QToolButton* m_logo = nullptr;
    QList<QToolButton*> m_buttons;
    QLabel* m_badge = nullptr;
};

} // namespace pldl::ui
