#include "ui/icons.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace pldl::ui::icons {

namespace {

QByteArray tintedSvg(const QString& name, const QColor& color)
{
    QFile file(u":/icons/ui/"_s + name + u".svg"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = file.readAll();
    svg.replace("#000000", color.name(QColor::HexRgb).toLatin1());
    return svg;
}

QPixmap render(const QByteArray& svg, int size, qreal dpr)
{
    QSvgRenderer renderer(svg);
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(0, 0, size, size));
    }
    return pm;
}

QString cacheKey(const QString& name, const QColor& color, int size, qreal dpr)
{
    return name + u'|' + color.name(QColor::HexArgb) + u'|' + QString::number(size) + u'|' +
           QString::number(dpr);
}

} // namespace

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio)
{
    static QHash<QString, QPixmap> cache;
    const QString key = cacheKey(name, color, size, devicePixelRatio);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const QPixmap pm = render(tintedSvg(name, color), size, devicePixelRatio);
    cache.insert(key, pm);
    return pm;
}

QIcon themed(const QString& name, const QColor& color, const QColor& disabledColor)
{
    QIcon icon;
    const QColor disabled =
        disabledColor.isValid() ? disabledColor : QColor(color.red(), color.green(), color.blue(), 90);
    for (const int size : {16, 20, 24, 32, 48}) {
        for (const qreal dpr : {1.0, 2.0}) {
            // Selected and Active carry the same glyph: without them the style
            // tints the selected item's icon with the palette highlight (owner
            // bug: a pink Engines icon in the Settings navigation).
            const QPixmap normal = pixmap(name, color, size, dpr);
            icon.addPixmap(normal, QIcon::Normal);
            icon.addPixmap(normal, QIcon::Active);
            icon.addPixmap(normal, QIcon::Selected);
            icon.addPixmap(pixmap(name, disabled, size, dpr), QIcon::Disabled);
        }
    }
    return icon;
}

QIcon appIcon()
{
    static const QIcon icon = [] {
        QIcon i;
        for (const int size : {16, 32, 48, 64, 128, 256, 512}) {
            i.addFile(u":/icons/hicolor/%1x%1/apps/com.ktechpit.playlist-dl.png"_s.arg(size),
                      QSize(size, size));
        }
        return i;
    }();
    return icon;
}

QIcon brand()
{
    return appIcon();
}

QString tintedFile(const QString& name, const QColor& color)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/icons"_s;
    QDir().mkpath(dir);
    const QString path = dir + u'/' + name + u'-' + color.name(QColor::HexRgb).mid(1) + u".svg"_s;
    if (!QFile::exists(path)) {
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(tintedSvg(name, color));
            file.commit();
        }
    }
    return path;
}

} // namespace pldl::ui::icons
