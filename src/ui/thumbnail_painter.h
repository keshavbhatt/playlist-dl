#pragma once

#include <QColor>
#include <QPixmap>
#include <QRect>
#include <QString>

class QPainter;

// One thumbnail painter for the queue cards, the library cards and the search
// results: cover-crop (scale by expanding, centre crop) at the device pixel
// ratio, rounded corners, and a gradient tile with a kind glyph when there is
// no picture. The three delegates paint the same way, so a fix lands in all.
namespace pldl::ui::thumbs {

/// The gradient tile colours behind a thumbnail-less item, keyed by a base colour.
struct Tile
{
    QColor base;        ///< gradient start; the end is `base.lighter(135)`
    QString glyph;      ///< ui glyph name drawn in white in the middle
    int glyphSize = 22; ///< logical px
};

/// How a picture meets its slot.
enum class Fit
{
    Cover,   ///< scale by expanding and crop the centre: a uniform slot (search, queue)
    Contain, ///< scale to fit whole, letterboxed: the original ratio is kept (library)
};

/// `pixmap` cropped to `logical` at `dpr` device pixels, ratio set on the result.
[[nodiscard]] QPixmap coverCrop(const QPixmap& pixmap, const QSize& logical, qreal dpr);
/// `pixmap` scaled to fit inside `logical` at `dpr`, ratio kept, ratio set on the result.
[[nodiscard]] QPixmap containScale(const QPixmap& pixmap, const QSize& logical, qreal dpr);
/// Where a contained picture lands inside `rect` (centred), in logical px.
[[nodiscard]] QRect containedRect(const QSize& pixmapDeviceSize, const QRect& rect, qreal dpr);

/// Paints `pixmap` (or the tile when it is null) into `rect` with `radius`
/// rounded corners, intersecting the painter's current clip. With `Fit::Contain`
/// the bands around the picture are filled with `letterbox`. `circle` draws the
/// picture as a round avatar centred in `rect` instead (channel results).
void paintThumbnail(QPainter* painter, const QRect& rect, const QPixmap& pixmap, const Tile& tile, qreal dpr,
                    int radius = 8, bool circle = false, Fit fit = Fit::Cover, const QColor& letterbox = {});

/// The small translucent badge with a kind glyph in a corner of `thumbRect`.
void paintKindBadge(QPainter* painter, const QRect& thumbRect, const QString& glyph, qreal dpr, int size = 18);

/// A translucent pill with white text (a duration or a kind label) anchored in
/// the bottom-right (or bottom-left when `left`) corner of `thumbRect`.
void paintChip(QPainter* painter, const QRect& thumbRect, const QString& text, const QFont& font, qreal dpr,
               const QString& glyph = {}, bool left = false);

} // namespace pldl::ui::thumbs
