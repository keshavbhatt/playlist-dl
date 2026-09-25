#pragma once

#include <QFrame>
#include <QList>

class QLabel;
class QPropertyAnimation;
class QToolButton;

namespace pldl::core {
class ThemeService;
}

namespace pldl::ui {

class Actions;
class RailButton;

/// The rail on the left of the window (DESIGN.md section 2): the brand mark,
/// the four pages (Search, Playlist, Browser, Downloads), then Settings,
/// Account and Help at the bottom. Pure presentation over Actions; the
/// checked page action is the active one. Collapsed it is 56 px of glyphs;
/// expanded (the toggle under the logo, Ctrl+B) it shows the labels beside
/// them, animated between the two widths (owner, 2026-09-25).
class SideRail : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SideRail)
    Q_PROPERTY(int railWidth READ railWidth WRITE setRailWidth)

public:
    static constexpr int kCollapsedWidth = 56;
    static constexpr int kExpandedWidth = 200;
    static constexpr int kAnimationMs = 200;

    SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent = nullptr);
    ~SideRail() override = default;

    /// Active-download count badge on the Downloads button (0 hides it).
    void setActiveDownloads(int count);

    /// Labels beside the glyphs, animated unless `animate` is false (start-up).
    void setExpanded(bool expanded, bool animate = true);
    [[nodiscard]] bool isExpanded() const { return m_expanded; }
    [[nodiscard]] int railWidth() const { return width(); }
    void setRailWidth(int width);
    [[nodiscard]] bool isAnimating() const;

Q_SIGNALS:
    void expandedChanged(bool expanded);

private:
    void setupUi();
    void applyIcons();
    void applyWidth(int width);
    RailButton* makeButton(QAction* action, const QString& icon);

    Actions& m_actions;
    core::ThemeService& m_theme;
    RailButton* m_logo = nullptr;
    RailButton* m_toggle = nullptr;
    QList<RailButton*> m_buttons;
    QWidget* m_downloadsHost = nullptr;
    QLabel* m_badge = nullptr;
    QPropertyAnimation* m_animation = nullptr;
    bool m_expanded = false;
};

} // namespace pldl::ui
