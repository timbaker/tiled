#include "customtilesize.h"

#include <QVector>

static const QVector<CustomTileSize> CUSTOM_TILE_SIZE = {
    { QStringLiteral("JUMBO_"), 64 * 3, 128 * 2 },
    { QStringLiteral("JUMBOXL_"), 640 / 2, 768 / 2 },
    { QStringLiteral("JUMBOXXL_"), 896 / 2, 1024 / 2 }
};

static const CustomTileSize INVALID(QString(), 0, 0);

const CustomTileSize &CustomTileSize::get(const QString &needle)
{
    for (const CustomTileSize& cts : CUSTOM_TILE_SIZE) {
        if (cts.mNeedle == needle) {
            return cts;
        }
    }
    return INVALID;
}

const QSize CustomTileSize::forTileset(const QString &tilesetName)
{
    for (const CustomTileSize& cts : CUSTOM_TILE_SIZE) {
        if (tilesetName.contains(cts.mNeedle)) {
            return cts.size();
        }
    }
    return QSize();
}
