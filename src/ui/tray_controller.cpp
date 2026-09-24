#include "ui/tray_controller.h"

#include "core/settings/settings.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/logging.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>

using namespace Qt::StringLiterals;

namespace pldl::ui {

TrayController::TrayController(core::Settings& settings, Actions& actions, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_actions(actions)
    , m_icon(icons::brand())
{
    buildMenu();
    applyTrayVisibility();
    connect(&m_settings, &core::Settings::trayEnabledChanged, this, [this](bool) { applyTrayVisibility(); });
    // A panel that starts after the app (a fresh session) gets the icon once it
    // is there: re-check every 5 s for the first minute, then stop.
    m_recheck = new QTimer(this);
    m_recheck->setInterval(5000);
    connect(m_recheck, &QTimer::timeout, this, [this] {
        applyTrayVisibility();
        if (m_tray != nullptr || ++m_rechecks >= 12) {
            m_recheck->stop();
        }
    });
    m_recheck->start();
}

TrayController::~TrayController()
{
    delete m_menu;
}

void TrayController::recheckAvailability()
{
    applyTrayVisibility();
}

bool TrayController::isAvailable() const
{
    return m_tray != nullptr && m_tray->isVisible() && QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayController::buildMenu()
{
    m_menu = new QMenu(); // top-level (no parent widget); owned and deleted here
    // The way back to the window and the downloads first, then quit.
    m_menu->addAction(m_actions.showHide);
    m_menu->addAction(m_actions.downloads);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.settings);
    m_menu->addSeparator();
    m_menu->addAction(m_actions.quit);
    for (QAction* action : m_menu->actions()) {
        action->setIconVisibleInMenu(false);
    }
}

void TrayController::applyTrayVisibility()
{
    const bool wanted = m_settings.trayEnabled() && QSystemTrayIcon::isSystemTrayAvailable();
    if (wanted && m_tray == nullptr) {
        m_tray = new QSystemTrayIcon(m_icon, this);
        m_tray->setContextMenu(m_menu);
        connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                Q_EMIT toggleRequested();
            }
        });
        updateTooltip();
        m_tray->show();
        qCInfo(lcUi) << "tray icon shown";
    } else if (!wanted && m_tray != nullptr) {
        m_tray->hide();
        m_tray->deleteLater();
        m_tray = nullptr;
        qCInfo(lcUi) << "tray icon hidden";
    }
}

void TrayController::setWindowVisible(bool visible)
{
    m_actions.showHide->setText(visible ? tr("Hide window") : tr("Show window"));
}

void TrayController::setActiveDownloads(int count)
{
    m_active = count;
    updateTooltip();
}

void TrayController::updateTooltip()
{
    if (m_tray == nullptr) {
        return;
    }
    QString tip = u"Playlist Downloader"_s;
    // Singular and plural written out: a "%n" form is shown literally without a translator.
    if (m_active == 1) {
        tip = tr("Playlist Downloader: 1 download running");
    } else if (m_active > 1) {
        tip = tr("Playlist Downloader: %1 downloads running").arg(m_active);
    }
    m_tray->setToolTip(tip);
}

} // namespace pldl::ui
