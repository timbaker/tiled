#ifndef CUSTOMTILESIZE_H
#define CUSTOMTILESIZE_H

#include "tiled_global.h"

#include <QSize>
#include <QString>

class TILEDSHARED_EXPORT CustomTileSize
{
public:
    static const CustomTileSize &get(const QString &needle);
    static const QSize forTileset(const QString &tilesetName);

    CustomTileSize(const QString &needle, int tileWidth, int tileHeight) :
        mNeedle(needle),
        mTileWidth(tileWidth),
        mTileHeight(tileHeight)
    {

    }

    int tileWidth() const
    { return mTileWidth; }

    int tileHeight() const
    { return mTileHeight; }

    QSize size() const
    { return QSize(tileWidth(), tileHeight()); }

    bool isValid() const
    { return tileWidth() > 0; }

private:
    const QString mNeedle;
    const int mTileWidth;
    const int mTileHeight;
};

#endif // CUSTOMTILESIZE_H
