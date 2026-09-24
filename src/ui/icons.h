#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

// Themed monochrome icons (DESIGN.md): the SVGs under :/icons/ui are drawn in
// #000000 and tinted at runtime, so one asset serves both schemes and the
// accent/muted variants. Rendered pixmaps are cached per (name, colour, size, dpr).
namespace pldl::ui::icons {

/// A QIcon whose normal/disabled modes are tinted `color` / `disabledColor`.
[[nodiscard]] QIcon themed(const QString& name, const QColor& color, const QColor& disabledColor = {});
/// A single pixmap at `size` logical px for the given device pixel ratio.
[[nodiscard]] QPixmap pixmap(const QString& name, const QColor& color, int size,
                             qreal devicePixelRatio = 1.0);
/// The application icon: the hicolor PNG set (16 to 512 px) in one QIcon so
/// every size Qt asks for, at any device pixel ratio, comes from the nearest
/// larger raster scaled smoothly. The single place the brand mark comes from:
/// the rail, the window, the tray, About, first run and the notifier.
[[nodiscard]] QIcon appIcon();
/// Alias of appIcon(), kept for the older call sites.
[[nodiscard]] QIcon brand();
/// Path of a tinted copy of the SVG on disk, for style-sheet url() references.
[[nodiscard]] QString tintedFile(const QString& name, const QColor& color);

} // namespace pldl::ui::icons
