#include "ui/side_rail.h"

#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/keyboard.h"
#include "ui/pldl_style.h"

#include <QAction>
#include <QActionGroup>
#include <QEasingCurve>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace pldl::ui {

namespace {
constexpr int kIconSize = 22;
constexpr int kButtonHeight = 40;
constexpr int kSideMargin = 8;
constexpr int kIconLeft = 9;  ///< the glyph's left edge inside the button, in both states
constexpr int kLabelLeft = 40; ///< where the label starts once the button is wider than a square
} // namespace

/// A rail entry that paints its own glyph and label: the glyph stays put while
/// the rail animates and the label fades with the width, which a stock
/// QToolButton (centred contents) cannot do.
class RailButton : public QToolButton
{
public:
    explicit RailButton(QWidget* parent)
        : QToolButton(parent)
    {
        setProperty("pldlRailButton", true);
        setToolButtonStyle(Qt::ToolButtonIconOnly);
        setFixedHeight(kButtonHeight);
        setCursor(Qt::PointingHandCursor);
    }

    void setGlyphSize(int size) { m_glyph = size; }
    void setLabel(const QString& text) { m_label = text; update(); }
    void setLabelAlpha(qreal alpha) { m_alpha = std::clamp(alpha, 0.0, 1.0); update(); }
    void setBold(bool bold) { m_bold = bold; }
    void setColours(const QColor& text, const QColor& active) { m_text = text; m_active = active; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QStylePainter painter(this);
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.icon = QIcon();
        option.text.clear();
        option.features &= ~QStyleOptionToolButton::HasMenu; // the help menu's arrow stays hidden
        painter.drawComplexControl(QStyle::CC_ToolButton, option);

        const QIcon::Mode mode = !isEnabled() ? QIcon::Disabled : (underMouse() ? QIcon::Active : QIcon::Normal);
        const QIcon::State state = isChecked() ? QIcon::On : QIcon::Off;
        const QPixmap pixmap = icon().pixmap(QSize(m_glyph, m_glyph), devicePixelRatioF(), mode, state);
        const int top = (height() - m_glyph) / 2;
        // A square button centres its glyph; a wider one keeps it at the same spot.
        const int left = width() <= kButtonHeight ? (width() - m_glyph) / 2 : kIconLeft - (m_glyph - kIconSize) / 2;
        painter.drawPixmap(left, top, pixmap);

        if (m_alpha > 0.0 && !m_label.isEmpty() && width() > kLabelLeft + 8) {
            painter.setOpacity(m_alpha);
            QFont font = painter.font();
            font.setWeight(m_bold ? QFont::DemiBold : QFont::Medium);
            painter.setFont(font);
            painter.setPen(isChecked() ? m_active : m_text); // the brand accent, not the palette's highlight
            const QRect textRect(kLabelLeft, 0, width() - kLabelLeft - 8, height());
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                             painter.fontMetrics().elidedText(m_label, Qt::ElideRight, textRect.width()));
        }
    }

private:
    QString m_label;
    qreal m_alpha = 0.0;
    int m_glyph = kIconSize;
    bool m_bold = false;
    QColor m_text;
    QColor m_active;
};

SideRail::SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent)
    : QFrame(parent)
    , m_actions(actions)
    , m_theme(theme)
{
    setProperty("pldlRail", true);
    setFixedWidth(kCollapsedWidth);
    setupUi();
    applyIcons();
    applyWidth(kCollapsedWidth);
    keyboard::installArrowNavigation(this);
    setAccessibleName(tr("Navigation"));
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyIcons(); });
    m_animation = new QPropertyAnimation(this, "railWidth", this);
    m_animation->setDuration(kAnimationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
}

RailButton* SideRail::makeButton(QAction* action, const QString& icon)
{
    auto* button = new RailButton(this);
    button->setDefaultAction(action);
    button->setProperty("pldlIcon", icon);
    button->setIconSize(QSize(kIconSize, kIconSize));
    QString label = action->text();
    label.remove(u'…'); // "Settings…" is the menu's spelling; the rail says Settings
    button->setLabel(label);
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
    layout->setContentsMargins(kSideMargin, 10, kSideMargin, 10);
    layout->setSpacing(6);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_logo = new RailButton(this);
    m_logo->setObjectName(u"railLogo"_s);
    m_logo->setGlyphSize(30);
    m_logo->setBold(true);
    m_logo->setLabel(u"Playlist Downloader"_s);
    m_logo->setFocusPolicy(Qt::NoFocus);
    m_logo->setToolTip(u"Playlist Downloader"_s);
    connect(m_logo, &QToolButton::clicked, m_actions.home, &QAction::trigger);
    layout->addWidget(m_logo, 0, Qt::AlignLeft);

    // The toggle: labels beside the glyphs, or glyphs alone.
    // Not the action's own button: a checkable action would paint the row like
    // the active page. The click triggers the action, the action drives the rail.
    m_toggle = new RailButton(this);
    m_toggle->setObjectName(u"railToggle"_s);
    m_toggle->setProperty("pldlIcon", u"rail-expand"_s);
    m_toggle->setIconSize(QSize(kIconSize, kIconSize));
    m_toggle->setFocusPolicy(Qt::TabFocus);
    connect(m_toggle, &QToolButton::clicked, m_actions.railLabels, &QAction::trigger);
    m_buttons << m_toggle;
    layout->addWidget(m_toggle, 0, Qt::AlignLeft);
    layout->addSpacing(8);

    auto* group = new QActionGroup(this);
    group->setExclusive(true);
    for (QAction* page : {m_actions.home, m_actions.playlist, m_actions.browser, m_actions.downloads}) {
        group->addAction(page);
    }

    const auto named = [this](QAction* action, const QString& icon, const QString& name) {
        RailButton* button = makeButton(action, icon);
        button->setObjectName(name);
        return button;
    };
    layout->addWidget(named(m_actions.home, u"search"_s, u"railSearch"_s), 0, Qt::AlignLeft);
    layout->addWidget(named(m_actions.playlist, u"playlist"_s, u"railPlaylist"_s), 0, Qt::AlignLeft);
    layout->addWidget(named(m_actions.browser, u"globe"_s, u"railBrowser"_s), 0, Qt::AlignLeft);

    // Downloads button with a count badge overlaid on its corner.
    m_downloadsHost = new QWidget(this);
    m_downloadsHost->setFixedSize(kButtonHeight, kButtonHeight);
    RailButton* downloads = named(m_actions.downloads, u"downloads"_s, u"railDownloads"_s);
    downloads->setParent(m_downloadsHost);
    downloads->move(0, 0);
    m_badge = new QLabel(m_downloadsHost);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFixedHeight(16);
    m_badge->setMinimumWidth(16);
    m_badge->hide();
    layout->addWidget(m_downloadsHost, 0, Qt::AlignLeft);
    layout->addStretch(1);
    layout->addWidget(named(m_actions.settings, u"settings"_s, u"railSettings"_s), 0, Qt::AlignLeft);
    layout->addWidget(named(m_actions.account, u"account"_s, u"railAccount"_s), 0, Qt::AlignLeft);
    // Help is a menu, so the shortcuts sheet is one click away without knowing
    // F1 (owner, 2026-09-25): the menu lists the keys next to each entry.
    RailButton* help = makeButton(m_actions.help, u"info"_s);
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
    layout->addWidget(help, 0, Qt::AlignLeft);
}

void SideRail::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    for (RailButton* b : m_buttons) {
        QString name = b->property("pldlIcon").toString();
        if (b == m_toggle) {
            name = m_expanded ? u"rail-collapse"_s : u"rail-expand"_s;
        }
        QIcon icon = icons::themed(name, t.muted, t.border);
        // The checked page reads in the accent colour.
        icon.addPixmap(icons::pixmap(name, t.accent, kIconSize, devicePixelRatioF()), QIcon::Normal, QIcon::On);
        icon.addPixmap(icons::pixmap(name, t.text, kIconSize, devicePixelRatioF()), QIcon::Active, QIcon::Off);
        if (QAction* action = b->defaultAction(); action != nullptr && b != m_toggle) {
            action->setIcon(icon);
        }
        b->setIcon(icon);
        b->setColours(t.text, t.accent);
    }
    m_logo->setIcon(icons::brand());
    m_logo->setColours(t.text, t.accent);
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
    // At the glyph's top-right corner in both states, not the wide button's.
    m_badge->move(kButtonHeight - m_badge->width() - 1, 1);
    m_badge->show();
    m_badge->raise();
}

void SideRail::setRailWidth(int width)
{
    setFixedWidth(width);
    applyWidth(width);
}

void SideRail::applyWidth(int width)
{
    const int inner = std::max(kButtonHeight, width - 2 * kSideMargin);
    const qreal progress =
        static_cast<qreal>(width - kCollapsedWidth) / static_cast<qreal>(kExpandedWidth - kCollapsedWidth);
    // The labels fade in over the last two thirds of the way, out over the first.
    const qreal alpha = std::clamp((progress - 0.35) / 0.65, 0.0, 1.0);
    for (RailButton* b : m_buttons) {
        b->setFixedWidth(inner);
        b->setLabelAlpha(alpha);
    }
    m_logo->setFixedWidth(inner);
    m_logo->setLabelAlpha(alpha);
    m_downloadsHost->setFixedSize(inner, kButtonHeight);
}

bool SideRail::isAnimating() const
{
    return m_animation != nullptr && m_animation->state() == QAbstractAnimation::Running;
}

void SideRail::setExpanded(bool expanded, bool animate)
{
    if (expanded == m_expanded && !isAnimating()) {
        return;
    }
    m_expanded = expanded;
    m_actions.railLabels->setChecked(expanded);
    m_actions.railLabels->setText(expanded ? tr("Hide labels") : tr("Show labels"));
    m_toggle->setLabel(m_actions.railLabels->text());
    m_toggle->setToolTip(u"%1  (%2)"_s.arg(m_actions.railLabels->text(),
                                            m_actions.railLabels->shortcut().toString(QKeySequence::NativeText)));
    m_toggle->setAccessibleName(m_actions.railLabels->text());
    applyIcons();
    const int target = expanded ? kExpandedWidth : kCollapsedWidth;
    m_animation->stop();
    if (!animate) {
        setRailWidth(target);
    } else {
        m_animation->setStartValue(width());
        m_animation->setEndValue(target);
        m_animation->start();
    }
    Q_EMIT expandedChanged(expanded);
}

} // namespace pldl::ui
