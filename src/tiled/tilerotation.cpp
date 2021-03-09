/*
 * tilerotation.cpp
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tilerotation.h"

#include "tilerotationfile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QPainter>

using namespace BuildingEditor;
using namespace Tiled;

// // // // //

namespace Tiled
{

class TileRotationPrivate
{
public:
    TileRotationPrivate(TileRotation& outer)
        : mOuter(outer)
    {

    }

    void fileLoaded()
    {
        for (TilesetRotated *tileset : mTilesets) {
            for (TileRotated *tile : tileset->mTiles) {
                Tile *tile1 = BuildingTilesMgr::instance()->tileFor(tile->mTileset->mName, tile->mID);
                if (isNone(tile1) || (tile1 == Tiled::Internal::TilesetManager::instance()->missingTile()))
                    continue;
                for (const QString& tileName : tile->r90.mTileNames) {
                    ZTileRenderInfo renderInfo;
                    Tile* tile2 = mOuter.getRotatedTile(tileName, renderInfo.mOffset);
                    renderInfo.mTile = tile2;
//                    renderInfo.mOrder = ???;  from tile.mProperties
                    mRenderInfo90[tile1] += renderInfo;
                }
                for (const QString& tileName : tile->r180.mTileNames) {
                    ZTileRenderInfo renderInfo;
                    Tile* tile2 = mOuter.getRotatedTile(tileName, renderInfo.mOffset);
                    renderInfo.mTile = tile2;
//                    renderInfo.mOrder = ???;  from tile.mProperties
                    mRenderInfo180[tile1] += renderInfo;
                }
                for (const QString& tileName : tile->r270.mTileNames) {
                    ZTileRenderInfo renderInfo;
                    Tile* tile2 = mOuter.getRotatedTile(tileName, renderInfo.mOffset);
                    renderInfo.mTile = tile2;
//                    renderInfo.mOrder = ???;  from tile.mProperties
                    mRenderInfo270[tile1] += renderInfo;
                }
            }
        }
    }

    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x());
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1);
        }
    }

    QPoint unrotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(pos.y(), height - pos.x() - 1); // ok
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(width - pos.y() - 1, pos.x());
        }
    }

    bool isNone(Tile *tile)
    {
        return tile == nullptr || tile == BuildingTilesMgr::instance()->noneTiledTile();
    }

    MapRotation rotation[4] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    void tempLazyInit()
    {
//      initFromBuildingTiles();
        TileRotationFile file;
        if (file.read(QLatin1Literal("D:\\pz\\TileRotation.txt"))) {
            mTilesets = file.takeTilesets();
            mRenderInfo90.clear();
            mRenderInfo180.clear();
            mRenderInfo270.clear();
            fileLoaded();
        }
    }

    void rotateTile(Tile *tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
    {
        if (rotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
            return;
        }

        // FIXME: temporary lazy init
        if (mTilesets.isEmpty()) {
            tempLazyInit();
        }

        switch (rotation) {
        case MapRotation::NotRotated:
            break;
        case MapRotation::Clockwise90:
            if (mRenderInfo90.contains(tile))
                tileInfos += mRenderInfo90[tile];
            break;
        case MapRotation::Clockwise180:
            if (mRenderInfo180.contains(tile))
                tileInfos += mRenderInfo180[tile];
            break;
        case MapRotation::Clockwise270:
            if (mRenderInfo270.contains(tile))
                tileInfos += mRenderInfo270[tile];
            break;
        }
    }

    void reload()
    {
        qDeleteAll(mTilesets);
        mTilesets.clear();
        mRenderInfo90.clear();
        mRenderInfo180.clear();
        mRenderInfo270.clear();
    }

    QImage createRotatedTilesetImage(const QString &tilesetName, bool dx)
    {
        Tileset* original = Internal::TileMetaInfoMgr::instance()->tileset(tilesetName);
        if (original == nullptr)
            return QImage();
        QImage image(QSize(original->imageWidth(), original->imageHeight()), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        bool is2x = !original->imageSource2x().isEmpty();
        int tileWidth = original->tileWidth() * (is2x ? 2 : 1);
        int tileHeight = original->tileHeight() * (is2x ? 2 : 1);
        int floorHeight = 32 * (is2x ? 2 : 1);
        int dx1 = dx ? tileWidth / 2 : -tileWidth / 2;
        int dy1 = floorHeight / 2;
        dx1 = dy1 = 0;
        for (int i = 0; i < original->tileCount(); i++) {
            Tile *tile = original->tileAt(i);
            int column = i % original->columnCount();
            int row = i / original->columnCount();
            QRect r(column * tileWidth, row * tileHeight, tileWidth, tileHeight);
            // TODO: don't call finalImage()
            painter.setClipRect(r);
            painter.drawImage(r.translated(dx1, dy1), tile->finalImage(tileWidth, tileHeight));
        }
        painter.end();
        return image;
    }

    Tileset* getRotatedTileset(const QString tilesetName, bool dx)
    {
        Tileset *tileset = dx ? mTilesetRotatedDX[tilesetName] : mTilesetRotatedDY[tilesetName];
        if (tileset == nullptr) {
            tileset = new Tileset(tilesetName + QLatin1Literal(dx ? "_DX" : "_DY"), 64, 128);
#if 1
            Tileset* original = Internal::TileMetaInfoMgr::instance()->tileset(tilesetName);
            tileset->loadFromCache(original);
#else
            QImage image = createRotatedTilesetImage(tilesetName, dx);
            tileset->setImageSource2x(tilesetName + QLatin1Literal(".png"));
            tileset->loadFromImage(image, tilesetName + QLatin1Literal(".png"));
#endif
            if (dx) {
                mTilesetRotatedDX[tilesetName] = tileset;
            } else {
                mTilesetRotatedDY[tilesetName] = tileset;
            }
        }
        return tileset;
    }

    // Get a tile copied from an actual tileset that is offset in x.
    Tile *getRotatedTileDX(const QString &tilesetName, int index)
    {
        Tileset *tileset = getRotatedTileset(tilesetName, true);
        return tileset->tileAt(index);
    }

    // Get a tile copied from an actual tileset that is offset in y.
    Tile *getRotatedTileDY(const QString &tilesetName, int index)
    {
        Tileset *tileset = getRotatedTileset(tilesetName, false);
        return tileset->tileAt(index);
    }

    TileRotation& mOuter;
    QList<TilesetRotated*> mTilesets;
    QMap<Tile*, QVector<ZTileRenderInfo>> mRenderInfo90;
    QMap<Tile*, QVector<ZTileRenderInfo>> mRenderInfo180;
    QMap<Tile*, QVector<ZTileRenderInfo>> mRenderInfo270;
    QMap<QString, Tileset*> mTilesetRotatedDX;
    QMap<QString, Tileset*> mTilesetRotatedDY;
};

// // // // //

TilesetRotated::~TilesetRotated()
{
    qDeleteAll(mTiles);
    mTiles.clear();
}

// namespace Tiled
}

// // // // //

TileRotation *TileRotation::mInstance = nullptr;

TileRotation *TileRotation::instance()
{
    if (mInstance == nullptr) {
        mInstance = new TileRotation();
    }
    return mInstance;
}

void TileRotation::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

TileRotation::TileRotation()
    : mPrivate(new TileRotationPrivate(*this))
{

}

void TileRotation::readFile(const QString &filePath)
{
    TileRotationFile file;
    if (!file.read(filePath)) {
        return;
    }
    qDeleteAll(mPrivate->mTilesets);
    mPrivate->mTilesets = file.takeTilesets();
    mPrivate->fileLoaded();
}

void TileRotation::rotateTile(Tile *tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
{
    mPrivate->rotateTile(tile, rotation, tileInfos);
}

void TileRotation::reload()
{
    mPrivate->reload();
}

Tile *TileRotation::getRotatedTileDX(const QString &tilesetName, int index)
{
    return mPrivate->getRotatedTileDX(tilesetName, index);
}

Tile *TileRotation::getRotatedTileDY(const QString &tilesetName, int index)
{
    return mPrivate->getRotatedTileDY(tilesetName, index);
}

Tile *TileRotation::getRotatedTile(const QString &tileName, QPoint &offset)
{
    offset = QPoint();
    QString tilesetName;
    int index;
    if (BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, index) == false) {
        return nullptr;
    }
    int floorWidth = 64;
    int floorHeight = 32;
    if (tilesetName.endsWith(QLatin1Literal("_DX"))) {
        offset = QPoint(floorWidth / 2, floorHeight / 2);
        return mPrivate->getRotatedTileDX(tilesetName.left(tilesetName.length() - 3), index);
    }
    if (tilesetName.endsWith(QLatin1Literal("_DY"))) {
        offset = QPoint(-floorWidth / 2, floorHeight / 2);
        return mPrivate->getRotatedTileDY(tilesetName.left(tilesetName.length() - 3), index);
    }
    return BuildingTilesMgr::instance()->tileFor(tilesetName, index);
}
