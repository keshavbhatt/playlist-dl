#pragma once

#include <QList>
#include <QObject>

class QAction;
class QWidget;

namespace pldl::ui {

/// All user-triggerable actions with their shortcuts, created once and shared
/// by the window, the rail, the tray menu and the shortcuts sheet. The owner
/// connects behaviour; this class only declares.
class Actions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Actions)

public:
    explicit Actions(QWidget* owner);
    ~Actions() override = default;

    QAction* home = nullptr; ///< the Search page
    QAction* playlist = nullptr;
    QAction* browser = nullptr;
    QAction* downloads = nullptr;
    QAction* showHide = nullptr;
    QAction* settings = nullptr;
    QAction* shortcuts = nullptr;
    QAction* onlineGuide = nullptr;
    QAction* openLogFolder = nullptr;
    QAction* about = nullptr;
    QAction* account = nullptr;
    QAction* quit = nullptr;
    // Browser page. Ctrl+W is shared with showHide: the window closes a tab
    // while the Browser page is showing, hides otherwise.
    QAction* browserNewTab = nullptr;
    QAction* browserCloseTab = nullptr;
    QAction* browserAddress = nullptr;
    QAction* browserReload = nullptr;
    QAction* browserBack = nullptr;
    QAction* browserForward = nullptr;
    QAction* browserNextTab = nullptr;
    QAction* browserPreviousTab = nullptr;
    QAction* browserDownload = nullptr;
    QAction* browserFind = nullptr;

    /// Every action with a shortcut, in sheet order.
    [[nodiscard]] QList<QAction*> all() const;
};

} // namespace pldl::ui
