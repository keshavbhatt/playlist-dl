#include "ui/side_rail.h"

#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/keyboard.h"
#include "ui/pldl_style.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kRailWidth = 56;
constexpr int kIconSize = 22;
} // namespace

SideRail::SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent)
    : QFrame(parent)
    , m_actions(actions)
    , m_theme(theme)
{
    setProperty("pldlRail", true);
    setFixedWidth(kRailWidth);
    setupUi();
    applyIcons();
    keyboard::installArrowNavigation(this);
    setAccessibleName(tr("Navigation"));
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyIcons(); });
}

QToolButton* SideRail::makeButton(QAction* action, const QString& icon)
{
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setProperty("pldlRailButton", true);
    button->setProperty("pldlIcon", icon);
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::TabFocus); // reachable with Tab, walked with the arrow keys
    const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
    if (action->toolTip().isEmpty() || action->toolTip() == action->text()) {
        action->setToolTip(shortcut.isEmpty() ? action->text() : u"%1  (%2)"_s.arg(action->text(), shortcut));
    }
    button->setToolTip(action->toolTip());
    m_buttons << button;
    return button;
}

void SideRail::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignHCenter);

    m_logo = new QToolButton(this);
    m_logo->setProperty("pldlRailButton", true);
    m_logo->setIconSize(QSize(30, 30));
    m_logo->setFocusPolicy(Qt::NoFocus);
    m_logo->setToolTip(u"Playlist Downloader"_s);
    connect(m_logo, &QToolButton::clicked, m_actions.home, &QAction::trigger);
    layout->addWidget(m_logo, 0, Qt::AlignHCenter);
    layout->addSpacing(8);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    for (QAction* page : {m_actions.home, m_actions.playlist, m_actions.browser, m_actions.downloads}) {
        group->addAction(page);
    }

    layout->addWidget(makeButton(m_actions.home, u"search"_s), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(m_actions.playlist, u"playlist"_s), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(m_actions.browser, u"globe"_s), 0, Qt::AlignHCenter);

    // Downloads button with a count badge overlaid on its corner.
    auto* downloadsHost = new QWidget(this);
    downloadsHost->setFixedSize(40, 40);
    QToolButton* downloads = makeButton(m_actions.downloads, u"downloads"_s);
    downloads->setParent(downloadsHost);
    downloads->move(0, 0);
    m_badge = new QLabel(downloadsHost);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFixedHeight(16);
    m_badge->setMinimumWidth(16);
    m_badge->hide();
    layout->addWidget(downloadsHost, 0, Qt::AlignHCenter);
    layout->addStretch(1);
    layout->addWidget(makeButton(m_actions.settings, u"settings"_s), 0, Qt::AlignHCenter);
    layout->addWidget(makeButton(m_actions.account, u"account"_s), 0, Qt::AlignHCenter);
    // Help is a menu, so the shortcuts sheet is one click away without knowing
    // F1 (owner, 2026-09-25): the menu lists the keys next to each entry.
    QToolButton* help = makeButton(m_actions.help, u"info"_s);
    auto* helpMenu = new QMenu(help);
    helpMenu->setObjectName(u"helpMenu"_s);
    helpMenu->addAction(m_actions.shortcuts);
    helpMenu->addAction(m_actions.onlineGuide);
    helpMenu->addAction(m_actions.supportedSites);
    helpMenu->addAction(m_actions.reportBug);
    helpMenu->addSeparator();
    helpMenu->addAction(m_actions.about);
    help->setMenu(helpMenu);
    help->setPopupMode(QToolButton::InstantPopup);
    help->setObjectName(u"helpButton"_s);
    layout->addWidget(help, 0, Qt::AlignHCenter);
}

void SideRail::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    for (QToolButton* b : m_buttons) {
        const QString name = b->property("pldlIcon").toString();
        QIcon icon = icons::themed(name, t.muted, t.border);
        // The checked page reads in the accent colour.
        icon.addPixmap(icons::pixmap(name, t.accent, kIconSize, devicePixelRatioF()), QIcon::Normal, QIcon::On);
        icon.addPixmap(icons::pixmap(name, t.text, kIconSize, devicePixelRatioF()), QIcon::Active, QIcon::Off);
        if (QAction* action = b->defaultAction(); action != nullptr) {
            action->setIcon(icon);
        }
        b->setIcon(icon);
    }
    m_logo->setIcon(icons::brand());
    m_badge->setStyleSheet(
        u"background:%1;color:%2;border-radius:8px;font-size:10px;font-weight:700;padding:0 4px;"_s.arg(
            t.accent.name(), t.accentText.name()));
}

void SideRail::setActiveDownloads(int count)
{
    if (count <= 0) {
        m_badge->hide();
        return;
    }
    m_badge->setText(count > 99 ? u"99+"_s : QString::number(count));
    m_badge->setAccessibleName(count == 1 ? tr("1 active download") : tr("%1 active downloads").arg(count));
    m_badge->adjustSize();
    m_badge->move(40 - m_badge->width() - 1, 1);
    m_badge->show();
    m_badge->raise();
}

} // namespace pldl::ui
