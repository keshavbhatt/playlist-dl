#include "ui/actions.h"

#include <QAction>
#include <QKeySequence>
#include <QWidget>

namespace pldl::ui {

namespace {

QAction* make(QWidget* owner, const QString& text, const QList<QKeySequence>& shortcuts = {})
{
    auto* action = new QAction(text, owner);
    if (!shortcuts.isEmpty()) {
        QList<QKeySequence> unique;
        for (const QKeySequence& sequence : shortcuts) {
            if (!sequence.isEmpty() && !unique.contains(sequence)) {
                unique << sequence;
            }
        }
        action->setShortcuts(unique);
        action->setShortcutContext(Qt::WindowShortcut);
    }
    // Registered on the window so shortcuts work while the browser page has focus.
    owner->addAction(action);
    return action;
}

} // namespace

Actions::Actions(QWidget* owner)
    : QObject(owner)
{
    home = make(owner, tr("Search"), {QKeySequence(Qt::CTRL | Qt::Key_1)});
    playlist = make(owner, tr("Playlist"), {QKeySequence(Qt::CTRL | Qt::Key_2)});
    browser = make(owner, tr("Browser"), {QKeySequence(Qt::CTRL | Qt::Key_3)});
    downloads = make(owner, tr("Downloads"), {QKeySequence(Qt::CTRL | Qt::Key_4)});
    showHide = make(owner, tr("Hide window")); // the tray and the menu; no shortcut (Ctrl+W closes a tab)
    settings = make(owner, tr("Settings…"), {QKeySequence(Qt::CTRL | Qt::Key_Comma)});
    shortcuts = make(owner, tr("Keyboard shortcuts"), {QKeySequence(Qt::Key_F1), QKeySequence(Qt::CTRL | Qt::Key_Slash)});
    onlineGuide = make(owner, tr("Online guide"));
    openLogFolder = make(owner, tr("Open log folder"));
    about = make(owner, tr("About Playlist Downloader"));
    railLabels = make(owner, tr("Show labels"), {QKeySequence(Qt::CTRL | Qt::Key_B)});
    railLabels->setCheckable(true);
    railLabels->setToolTip(tr("Show or hide the labels on the rail"));
    help = make(owner, tr("Help"));
    help->setToolTip(tr("Help: keyboard shortcuts, the guide, report a bug, about"));
    reportBug = make(owner, tr("Report a bug…"));
    supportedSites = make(owner, tr("Supported sites…"));
    account = make(owner, tr("Account…"), {QKeySequence(Qt::CTRL | Qt::Key_5)});
    quit = make(owner, tr("Quit"), {QKeySequence::Quit});
    browserNewTab = make(owner, tr("New tab"), {QKeySequence(Qt::CTRL | Qt::Key_T)});
    browserCloseTab = make(owner, tr("Close tab"), {QKeySequence(Qt::CTRL | Qt::Key_W)});
    browserAddress = make(owner, tr("Address bar"), {QKeySequence(Qt::CTRL | Qt::Key_L)});
    browserReload = make(owner, tr("Reload page"), {QKeySequence(Qt::Key_F5), QKeySequence(Qt::CTRL | Qt::Key_R)});
    browserBack = make(owner, tr("Back"), {QKeySequence(Qt::ALT | Qt::Key_Left)});
    browserForward = make(owner, tr("Forward"), {QKeySequence(Qt::ALT | Qt::Key_Right)});
    browserNextTab = make(owner, tr("Next tab"), {QKeySequence(Qt::CTRL | Qt::Key_Tab)});
    browserPreviousTab = make(owner, tr("Previous tab"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab)});
    browserDownload = make(owner, tr("Download this page"), {QKeySequence(Qt::CTRL | Qt::Key_D)});
    browserFind = make(owner, tr("Find in page"), {QKeySequence(Qt::CTRL | Qt::Key_F)});

    for (QAction* page : {home, playlist, browser, downloads}) {
        page->setCheckable(true);
    }
    quit->setMenuRole(QAction::QuitRole);
    about->setMenuRole(QAction::AboutRole);
    settings->setMenuRole(QAction::PreferencesRole);
}

QList<QAction*> Actions::all() const
{
    return {home,           playlist,       browser,        downloads,     showHide,
            settings,       shortcuts,      onlineGuide,    openLogFolder, account,
            about,          help,           reportBug,      supportedSites,  railLabels,      quit,
            browserNewTab,
            browserCloseTab, browserAddress,
            browserReload,  browserBack,    browserForward, browserNextTab, browserPreviousTab,
            browserDownload, browserFind};
}

} // namespace pldl::ui
