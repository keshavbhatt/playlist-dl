#pragma once

#include <QColor>
#include <QString>

namespace pldl::ui {

/// UMD's brand tokens (DESIGN.md section 1), per scheme. The single source of truth
/// for every colour the native chrome paints, the style sheet, the rail, the
/// download cards and the dialogs all read from here.
struct Tokens
{
    QColor accent;
    /// Primary button fill: dark enough for white text at 4.5:1 (DESIGN.md section 5).
    QColor accentStrong;
    QColor accentHover;
    QColor accentSoft;
    QColor accentText;
    QColor bg;
    QColor rail;
    QColor panel;
    QColor elevated;
    QColor hover;
    QColor input;
    QColor border;
    QColor text;
    QColor muted;
    QColor link;
    QColor success;
    QColor warning;
    QColor danger;
    /// The icon's yellow: the download badge pill on the rail and "new" chips.
    QColor badge;
    QColor badgeText; ///< text on badge

    [[nodiscard]] static Tokens forScheme(bool dark);
    /// White or the dark ground, whichever reads better on `background`
    /// (badges on warning, buttons on accent).
    [[nodiscard]] static QColor textOn(const QColor& background);
    /// WCAG 2 contrast ratio between two colours (1 to 21).
    [[nodiscard]] static double contrast(const QColor& a, const QColor& b);
};

/// The application-wide Qt style sheet built from the tokens (applied by
/// ThemeApplier on every scheme change). Primary buttons opt in with the `pldlPrimary` dynamic property,
/// flat icon buttons with `pldlFlat`, and cards with `pldlCard`.
[[nodiscard]] QString pldlStyleSheet(bool dark);

} // namespace pldl::ui
