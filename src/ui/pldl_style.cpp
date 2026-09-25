#include "ui/pldl_style.h"

#include "ui/icons.h"

#include <QHash>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace pldl::ui {

Tokens Tokens::forScheme(bool dark)
{
    // DESIGN.md section 1: the icon's purple is the accent, its yellow the badge.
    Tokens t;
    if (dark) {
        t.accent = QColor(0xC0, 0x61, 0xCB);
        t.accentStrong = QColor(0x91, 0x41, 0xAC);
        t.accentHover = QColor(0x81, 0x3D, 0x9C);
        t.accentSoft = QColor(0x23, 0x18, 0x29);
        t.accentText = QColor(0xFF, 0xFF, 0xFF);
        t.bg = QColor(0x14, 0x11, 0x18);
        t.rail = QColor(0x14, 0x11, 0x18);
        t.panel = QColor(0x1C, 0x18, 0x22);
        t.elevated = QColor(0x26, 0x21, 0x2E);
        t.hover = QColor(0x30, 0x2A, 0x3A);
        t.input = QColor(0x18, 0x14, 0x20);
        t.border = QColor(0x2E, 0x28, 0x38);
        t.text = QColor(0xF2, 0xEE, 0xF6);
        t.muted = QColor(0xA7, 0x9F, 0xB3);
        t.link = QColor(0xDC, 0x8A, 0xDD);
        t.success = QColor(0x34, 0xA8, 0x53);
        t.warning = QColor(0xF9, 0xAB, 0x00);
        t.danger = QColor(0xFF, 0x5A, 0x52);
        t.badge = QColor(0xF6, 0xD3, 0x2D);
        t.badgeText = QColor(0x3D, 0x1D, 0x4B);
        return t;
    }
    t.accent = QColor(0x81, 0x3D, 0x9C);
    t.accentStrong = QColor(0x81, 0x3D, 0x9C);
    t.accentHover = QColor(0x61, 0x35, 0x83);
    t.accentSoft = QColor(0xF3, 0xE8, 0xF6);
    t.accentText = QColor(0xFF, 0xFF, 0xFF);
    t.bg = QColor(0xF8, 0xF6, 0xFA);
    t.rail = QColor(0xFF, 0xFF, 0xFF);
    t.panel = QColor(0xFF, 0xFF, 0xFF);
    t.elevated = QColor(0xFF, 0xFF, 0xFF);
    t.hover = QColor(0xEC, 0xE6, 0xF0);
    t.input = QColor(0xFF, 0xFF, 0xFF);
    t.border = QColor(0xE2, 0xDC, 0xE8);
    t.text = QColor(0x1A, 0x15, 0x23);
    t.muted = QColor(0x5E, 0x58, 0x70);
    t.link = QColor(0x6F, 0x3A, 0x8C);
    t.success = QColor(0x1B, 0x7F, 0x38);
    t.warning = QColor(0xB4, 0x53, 0x09);
    t.danger = QColor(0xC6, 0x28, 0x28);
    t.badge = QColor(0xE5, 0xA5, 0x0A);
    t.badgeText = QColor(0x3D, 0x1D, 0x4B);
    return t;
}

namespace {
double channel(int value)
{
    const double c = value / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}
double luminance(const QColor& c)
{
    return 0.2126 * channel(c.red()) + 0.7152 * channel(c.green()) + 0.0722 * channel(c.blue());
}
} // namespace

double Tokens::contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    const double hi = std::max(la, lb);
    const double lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

QColor Tokens::textOn(const QColor& background)
{
    const QColor white(0xFF, 0xFF, 0xFF);
    const QColor ground(0x14, 0x11, 0x18);
    return contrast(white, background) >= contrast(ground, background) ? white : ground;
}

namespace {

// The sheet, with {{token}} placeholders. Backgrounds are set only on the
// containers and controls that need the app's look; the palette handles the
// rest, so nothing paints an opaque block over the browser page.
const char* kSheet = R"qss(
QDialog, QMainWindow { background: {{bg}}; }
QWidget { color: {{text}}; }
QLabel, QCheckBox, QRadioButton, QGroupBox, QTabWidget, QTabBar { background: transparent; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QToolTip {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 6px; padding: 5px 8px;
}
QStatusBar { background: {{bg}}; border-top: 1px solid {{border}}; }
QLabel[pldlMuted="true"] { color: {{muted}}; }
QLabel[pldlTitle="true"] { font-size: 16px; font-weight: 600; }
QLabel[pldlHeading="true"] { font-size: 20px; font-weight: 600; }
/* Section headings carry the accent so they stand apart from the rows' own
   titles and descriptions (owner request). */
QLabel[pldlSection="true"] { font-size: 12px; font-weight: 600; color: {{accent}}; text-transform: uppercase; letter-spacing: 1px; }
QLabel[pldlLink="true"] { color: {{link}}; }
QPushButton[pldlLink="true"] { color: {{link}}; }

/* Cards */
QFrame[pldlCard="true"] {
    background: {{panel}}; border: 1px solid {{border}}; border-radius: 12px;
}
QFrame[pldlPanel="true"] { background: {{panel}}; border-left: 1px solid {{border}}; }
QFrame[pldlRail="true"] { background: {{rail}}; border-right: 1px solid {{border}}; }
QFrame[pldlSeparator="true"] { background: {{border}}; max-height: 1px; min-height: 1px; border: none; }
QFrame[pldlVSeparator="true"] { background: {{border}}; max-width: 1px; min-width: 1px; border: none; }

/* Tabs: the active section is underlined. */
QTabWidget::pane { border: none; background: transparent; }
QTabBar::tab {
    background: transparent; color: {{muted}};
    padding: 8px 16px 9px 16px; margin-right: 2px;
    border: none; border-bottom: 2px solid transparent; font-weight: 600;
}
QTabBar::tab:hover { color: {{text}}; }
QTabBar::tab:selected { color: {{text}}; border-bottom: 2px solid {{text}}; }
QTabBar::tab:focus { border-bottom: 2px solid {{accent}}; color: {{accent}}; }

/* Grouping panels. */
QGroupBox {
    background: {{panel}}; border: 1px solid {{border}}; border-radius: 12px;
    margin-top: 18px; padding: 18px 14px 14px 14px; font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    left: 12px; padding: 0 4px; color: {{muted}};
}

/* Text inputs, combos and spin boxes share the rounded shape. */
QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit, QTextEdit {
    background: {{input}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 8px;
    padding: 6px 10px; min-height: 22px;
    selection-background-color: {{accent}}; selection-color: #ffffff;
}
QLineEdit:hover, QComboBox:hover, QAbstractSpinBox:hover { border: 1px solid {{muted}}; }
/* Focus ring (DESIGN.md section 5): 2 px accent; padding shrinks by 1 px so nothing moves. */
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
    border: 2px solid {{accent}}; padding: 5px 9px;
}
QLineEdit:read-only { color: {{muted}}; }
/* Find in page with no match: the field says so with the warning token. */
QLineEdit[pldlNoMatch="true"], QLineEdit[pldlNoMatch="true"]:focus { border-color: {{warning}}; }
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { color: {{muted}}; border-color: {{border}}; }
QComboBox::drop-down, QComboBox::drop-down:editable {
    subcontrol-origin: padding; subcontrol-position: center right;
    width: 26px; border: none; background: transparent;
}
QComboBox::down-arrow { image: url({{chevronDown}}); width: 14px; height: 14px; }
/* List-style popup: the menu-style one shows every row and overflows the screen
   when the list is long (subtitle languages); this one scrolls past maxVisibleItems. */
QComboBox { combobox-popup: 0; }
QComboBox QAbstractItemView {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 8px; padding: 4px;
    selection-background-color: {{hover}}; selection-color: {{text}}; outline: none;
}
QComboBox QAbstractItemView::item { padding: 6px 8px; border-radius: 6px; min-height: 22px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 18px; border: none; background: transparent; }
QAbstractSpinBox::up-arrow { image: url({{chevronUp}}); width: 10px; height: 10px; }
QAbstractSpinBox::down-arrow { image: url({{chevronDown}}); width: 10px; height: 10px; }

/* Buttons: neutral pills; pldlPrimary turns one accent blue; pldlFlat is icon-only. */
QPushButton {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 18px;
    padding: 7px 18px; min-height: 22px; font-weight: 600;
}
QPushButton:hover { background: {{hover}}; }
QPushButton:pressed { background: {{accentSoft}}; border-color: {{accent}}; padding: 8px 18px 6px 18px; }
QPushButton:focus { border: 2px solid {{accent}}; padding: 6px 17px; }
QPushButton:disabled { color: {{muted}}; background: transparent; border-color: {{border}}; }
QPushButton[pldlPrimary="true"] {
    background: {{accentStrong}}; color: {{onAccentStrong}}; border: 1px solid {{accentStrong}};
}
QPushButton[pldlPrimary="true"]:hover { background: {{accentHover}}; border-color: {{accentHover}}; }
QPushButton[pldlPrimary="true"]:pressed { background: {{accentHover}}; border-color: {{accentHover}}; padding: 8px 18px 6px 18px; }
QPushButton[pldlPrimary="true"]:focus { border: 2px solid {{text}}; padding: 6px 17px; }
QPushButton[pldlPrimary="true"]:disabled { background: {{hover}}; color: {{muted}}; border-color: transparent; }
QPushButton[pldlBusy="true"] { color: {{onAccentStrong}}; }
QPushButton[pldlDanger="true"] { color: {{danger}}; }
QPushButton[pldlDanger="true"]:pressed { border-color: {{danger}}; }
QPushButton[pldlFlat="true"], QToolButton[pldlFlat="true"] {
    background: transparent; border: 2px solid transparent; border-radius: 8px; padding: 4px 8px; min-height: 22px;
}
QPushButton[pldlFlat="true"]:hover, QToolButton[pldlFlat="true"]:hover { background: {{hover}}; }
QPushButton[pldlFlat="true"]:pressed, QToolButton[pldlFlat="true"]:pressed { background: {{accentSoft}}; border-color: {{accent}}; }
QPushButton[pldlFlat="true"]:focus, QToolButton[pldlFlat="true"]:focus { border: 2px solid {{accent}}; }
QPushButton[pldlFlat="true"]:disabled, QToolButton[pldlFlat="true"]:disabled { color: {{muted}}; }
/* Release notes in the What's new sheet: plain text on the card, not an input box. */
QTextBrowser[pldlNotes="true"] { background: transparent; border: none; padding: 0; }
QToolButton { background: transparent; border: 2px solid transparent; border-radius: 8px; padding: 2px; }
QToolButton:hover { background: {{hover}}; }
QToolButton:pressed { background: {{accentSoft}}; border-color: {{accent}}; }
QToolButton:checked { background: {{accentSoft}}; }
QToolButton:focus { border: 2px solid {{accent}}; }
QToolButton:disabled { color: {{muted}}; }
QToolButton::menu-indicator { image: none; }

/* Chips: pill tool buttons with a label (the Home page pickers, queue filters). */
QToolButton[pldlChip="true"] {
    background: {{panel}}; color: {{text}}; border: 1px solid {{border}}; border-radius: 15px;
    padding: 4px 12px; min-height: 20px; font-weight: 500;
}
QToolButton[pldlChip="true"]:hover { background: {{hover}}; }
QToolButton[pldlChip="true"]:pressed { background: {{accentSoft}}; border-color: {{accent}}; }
QToolButton[pldlChip="true"]:focus { border: 2px solid {{accent}}; padding: 3px 11px; }
QToolButton[pldlChip="true"]:checked, QToolButton[pldlChip="true"][pldlOn="true"] {
    background: {{accentSoft}}; border-color: {{accent}}; color: {{accent}};
}
QToolButton[pldlChip="true"]:disabled { color: {{muted}}; background: transparent; }
QLabel[pldlBadge="true"][pldlPro="true"] { background: {{warning}}; color: {{onWarning}}; letter-spacing: 1px; }
QLabel[pldlBadge="true"] {
    background: {{accentSoft}}; color: {{accent}}; border-radius: 10px; padding: 2px 8px;
    font-size: 11px; font-weight: 600;
}
/* The "good news" badge of the mocks (.badge.ok): a size that is known, a
   server that allows resuming, metadata that arrived. */
QLabel[pldlBadge="true"][pldlOk="true"] { background: {{successSoft}}; color: {{success}}; }
QLabel[pldlBadge="true"][pldlNeutral="true"] { background: {{hover}}; color: {{muted}}; }
/* Informational banner (mocks .banner): accent tint, accent hairline. */
QFrame[pldlBanner="true"] {
    background: {{accentSoft}}; border: 1px solid {{accent}}; border-radius: 12px;
}
/* The warning tint of the mocks (.banner.warn): the evaluation banner. */
QFrame[pldlBanner="true"][pldlWarn="true"] { background: {{warningSoft}}; border-color: {{warning}}; }
QFrame[pldlDrop="true"] { border: 2px dashed {{border}}; border-radius: 14px; background: transparent; }
QFrame[pldlDrop="true"][pldlHot="true"] { border-color: {{accent}}; background: {{accentSoft}}; }
QFrame[pldlHeader="true"] { background: {{bg}}; border-bottom: 1px solid {{border}}; }
/* The Browser page's tab strip (mocks/browser.html): rail-coloured, the
   active tab is painted in the page background by BrowserTabButton. */
QFrame[pldlTabStrip="true"] { background: {{rail}}; border-bottom: 1px solid {{border}}; }
QToolButton[pldlTabClose="true"] { padding: 1px; border-radius: 6px; }
/* A hairline loading bar under a toolbar. */
QProgressBar[pldlThin="true"] { min-height: 3px; max-height: 3px; border-radius: 0; background: transparent; }
QProgressBar[pldlThin="true"]::chunk { border-radius: 0; }

/* Rail buttons: square, rounded, the active one carries the accent bar. */
QToolButton[pldlRailButton="true"] {
    background: transparent; border: none; border-radius: 10px; padding: 0; margin: 0;
    min-width: 40px; min-height: 40px; max-height: 40px;
}
QToolButton[pldlRailButton="true"]:hover { background: {{hover}}; }
QToolButton[pldlRailButton="true"]:pressed { background: {{accentSoft}}; }
QToolButton[pldlRailButton="true"]:checked { background: {{accentSoft}}; }
QToolButton[pldlRailButton="true"]:focus { border: 2px solid {{accent}}; }
QToolButton[pldlRailButton="true"][pldlAccent="true"] { background: {{accent}}; }
QToolButton[pldlRailButton="true"][pldlAccent="true"]:hover { background: {{accentHover}}; }
QToolButton[pldlRailButton="true"]:disabled { background: transparent; }

/* Check boxes and radios. */
QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 18px; height: 18px; }
QCheckBox::indicator {
    border: 2px solid {{muted}}; border-radius: 4px; background: transparent;
}
QCheckBox::indicator:hover { border-color: {{text}}; }
/* Keyboard focus on a check or radio: the label turns accent (the indicator cannot take a ring). */
QCheckBox:focus, QRadioButton:focus { color: {{accent}}; }
QCheckBox:focus::indicator, QRadioButton:focus::indicator { border-color: {{accent}}; }
QCheckBox::indicator:checked { background: {{accent}}; border-color: {{accent}}; image: url({{checkWhite}}); }
QCheckBox::indicator:disabled { border-color: {{border}}; }
QCheckBox::indicator:checked:disabled { background: {{hover}}; border-color: {{hover}}; }
/* Both radio states add up to 18px: QSS sizes the indicator per state and a
   toggle would otherwise shift the label. */
QRadioButton::indicator { width: 14px; height: 14px; border: 2px solid {{muted}}; border-radius: 9px; background: transparent; }
QListView::indicator, QTreeView::indicator, QTableView::indicator {
    width: 16px; height: 16px; border: 2px solid {{muted}}; border-radius: 4px; background: transparent;
}
QListView::indicator:hover, QTreeView::indicator:hover { border-color: {{text}}; }
QListView::indicator:checked, QTreeView::indicator:checked, QTableView::indicator:checked {
    background: {{accent}}; border-color: {{accent}}; image: url({{checkWhite}});
}
QRadioButton::indicator:hover { border-color: {{text}}; }
QRadioButton::indicator:checked { border: 6px solid {{accent}}; background: {{accentText}}; width: 6px; height: 6px; }

/* Menus. */
QMenu {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 10px; padding: 6px;
}
QMenu::item { padding: 7px 28px 7px 12px; border-radius: 6px; }
QMenu::item:selected { background: {{hover}}; }
QMenu::item:disabled { color: {{muted}}; }
QMenu::separator { height: 1px; background: {{border}}; margin: 6px 4px; }
QMenu::icon { padding-left: 6px; }
QMenu::indicator { width: 16px; height: 16px; margin-left: 6px; }
QMenu::indicator:checked { image: url({{checkText}}); }

/* Scrollbars: thin, rounded. */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: {{hover}}; border-radius: 3px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: {{muted}}; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: {{hover}}; border-radius: 3px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* Lists / tables. */
QListView, QTreeView, QTableView, QListWidget, QTreeWidget, QTableWidget {
    background: transparent; border: none; outline: none;
    selection-background-color: {{hover}}; selection-color: {{text}};
    alternate-background-color: transparent;
}
QListView::item, QTreeView::item { padding: 4px; border-radius: 6px; }
QListView::item:selected, QTreeView::item:selected, QTableView::item:selected { background: {{hover}}; color: {{text}}; }
QListView::item:hover, QTreeView::item:hover { background: {{hover}}; }
QHeaderView::section {
    background: transparent; color: {{muted}}; border: none; border-bottom: 1px solid {{border}};
    padding: 6px 8px; font-weight: 600;
}
QListWidget[pldlNav="true"] { background: {{bg}}; padding: 8px; }
QListWidget[pldlNav="true"]::item { padding: 9px 12px; border-radius: 8px; margin: 1px 0; }
QListWidget[pldlNav="true"]::item:selected { background: {{hover}}; font-weight: 600; }

/* Progress bars. */
QProgressBar {
    background: {{hover}}; border: none; border-radius: 3px; min-height: 6px; max-height: 6px; text-align: center;
}
QProgressBar::chunk { background: {{accent}}; border-radius: 3px; }

/* Sliders. */
QSlider::groove:horizontal { height: 4px; background: {{hover}}; border-radius: 2px; }
QSlider::sub-page:horizontal { background: {{accent}}; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: {{text}}; border-radius: 7px; }
QSlider::handle:horizontal:hover, QSlider::handle:horizontal:pressed { background: {{accent}}; }
/* The focus ring sits on the handle, not the widget: a widget-level :focus
   rule would box the whole slider (owner bug). */
QSlider::handle:horizontal:focus { background: {{accent}}; border: 2px solid {{text}}; }

/* Splitter handle between the page and the details pane. */
QSplitter::handle { background: {{border}}; }
QSplitter::handle:horizontal { width: 1px; }

/* Message boxes. */
QMessageBox { background: {{panel}}; }
QMessageBox QLabel { min-width: 320px; }
)qss";

QHash<QString, QString> tokenMap(const Tokens& t)
{
    return {
        {u"accent"_s, t.accent.name()},
        {u"accentStrong"_s, t.accentStrong.name()},
        {u"onAccentStrong"_s, Tokens::textOn(t.accentStrong).name()},
        {u"onWarning"_s, Tokens::textOn(t.warning).name()},
        {u"accentHover"_s, t.accentHover.name()},
        {u"accentSoft"_s, t.accentSoft.name()},
        {u"accentText"_s, t.accentText.name()},
        {u"bg"_s, t.bg.name()},
        {u"rail"_s, t.rail.name()},
        {u"panel"_s, t.panel.name()},
        {u"elevated"_s, t.elevated.name()},
        {u"hover"_s, t.hover.name()},
        {u"input"_s, t.input.name()},
        {u"border"_s, t.border.name()},
        {u"text"_s, t.text.name()},
        {u"muted"_s, t.muted.name()},
        {u"link"_s, t.link.name()},
        {u"success"_s, t.success.name()},
        {u"warning"_s, t.warning.name()},
        {u"danger"_s, t.danger.name()},
        {u"badge"_s, t.badge.name()},
        {u"badgeText"_s, t.badgeText.name()},
        // Style sheets take rgba(); the soft tint is the state colour at 18%.
        {u"successSoft"_s, u"rgba(%1, %2, %3, 0.18)"_s.arg(t.success.red())
                               .arg(t.success.green())
                               .arg(t.success.blue())},
        {u"warningSoft"_s, u"rgba(%1, %2, %3, 0.14)"_s.arg(t.warning.red())
                               .arg(t.warning.green())
                               .arg(t.warning.blue())},
    };
}

} // namespace

QString pldlStyleSheet(bool dark)
{
    QString sheet = QString::fromLatin1(kSheet);
    const Tokens t = Tokens::forScheme(dark);
    QHash<QString, QString> map = tokenMap(t);
    // Style sheets can only reference files, so the tinted glyphs are written
    // to the cache directory once per colour.
    map.insert(u"chevronDown"_s, icons::tintedFile(u"chevron-down"_s, t.muted));
    map.insert(u"chevronUp"_s, icons::tintedFile(u"chevron-up"_s, t.muted));
    map.insert(u"checkWhite"_s, icons::tintedFile(u"check"_s, t.accentText));
    map.insert(u"checkText"_s, icons::tintedFile(u"check"_s, t.text));
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        sheet.replace(u"{{"_s + it.key() + u"}}"_s, it.value());
    }
    return sheet;
}

} // namespace pldl::ui
