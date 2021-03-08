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
        Tile* noneTile = BuildingTilesMgr::instance()->noneTiledTile();
        int NORTH = TileRotateInfo::NORTH;
        int EAST = TileRotateInfo::EAST;
        int SOUTH = TileRotateInfo::SOUTH;
        int WEST = TileRotateInfo::WEST;
        int NORTHWEST = EAST; // northwest corner tile
        QList<TileRotateInfo*> newTiles;
        for (TileRotateInfo *entry : mTiles) {
            for (int i = 0; i < 4; i++) {
                BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(entry->mTileNames[i]);
                entry->mTiles[i] = BuildingTilesMgr::instance()->tileFor(buildingTile);
            }
            if (entry->mType == TileRotateType::Wall) {
                Tile* tileNW = entry->mTiles[NORTHWEST];
                entry->mTiles[NORTHWEST] = nullptr; // create a new TileRotateInfo for the north-west tile below.
                Tile* tileN = entry->mTiles[NORTH];
                Tile* tileW = entry->mTiles[WEST];

                if (tileN != noneTile) {
                    if (mRenderInfo.contains(tileN)) {
                        entry->mRenderInfo[NORTH] += mRenderInfo[tileN];
                    } else {
                        entry->mRenderInfo[NORTH] += ZTileRenderInfo(tileN);
                    }
                    if (mSouthWallTiles.contains(tileN)) {
                        entry->mRenderInfo[SOUTH] = mSouthWallTiles[tileN];
                    }
                    mTileLookup[tileN] = entry;
                }
                if (tileW != noneTile) {
                    if (mRenderInfo.contains(tileW)) {
                        entry->mRenderInfo[WEST] += mRenderInfo[tileW];
                    } else {
                        entry->mRenderInfo[WEST] += ZTileRenderInfo(tileW);
                    }
                    if (mEastWallTiles.contains(tileW)) {
                        entry->mRenderInfo[EAST] = mEastWallTiles[tileW];
                    }
                    mTileLookup[tileW] = entry;
                }

                if ((tileN != noneTile) && (tileW != noneTile) && (tileNW != noneTile)) {
                    TileRotateInfo *entryNW = new TileRotateInfo();
                    entryNW->mType = TileRotateType::Wall;
                    entryNW->mTiles[WEST] = tileNW;
                    if (mNorthEastWallTiles.contains(tileNW)) {
                        entryNW->mRenderInfo[NORTH] = mNorthEastWallTiles[tileNW];
                    }
                    if (mSouthEastWallTiles.contains(tileNW)) {
                        entryNW->mRenderInfo[EAST] = mSouthEastWallTiles[tileNW];
                    }
                    if (mSouthWestWallTiles.contains(tileNW)) {
                        entryNW->mRenderInfo[SOUTH] = mSouthWestWallTiles[tileNW];
                    }
                    mTileLookup[tileNW] = entryNW;
                    newTiles += entryNW;
                }

                continue;
            }

            for (int i = 0; i < 4; i++) {
                Tile* tile = entry->mTiles[i];
                if (tile == noneTile) {
#if 1
                    // Door frames, etc, but not walls handled above.
                    if (i == 1 && mEastWallTiles.contains(entry->mTiles[3])) {
                        entry->mRenderInfo[i] = mEastWallTiles[entry->mTiles[3]];
                    } else if (i == 0 && mNorthEastWallTiles.contains(entry->mTiles[3])) {
                        entry->mRenderInfo[i] = mNorthEastWallTiles[entry->mTiles[3]];
                    } else if (i == 1 && mSouthEastWallTiles.contains(entry->mTiles[3])) {
                        entry->mRenderInfo[i] = mSouthEastWallTiles[entry->mTiles[3]];
                    } else if (i == 2 && mSouthWestWallTiles.contains(entry->mTiles[3])) {
                        entry->mRenderInfo[i] = mSouthWestWallTiles[entry->mTiles[3]];
                    } else if (i == 2 && mSouthWallTiles.contains(entry->mTiles[0])) {
                        entry->mRenderInfo[i] = mSouthWallTiles[entry->mTiles[0]];
                    }
#endif
                } else {
                    if (mRenderInfo.contains(tile)) {
                        entry->mRenderInfo[i] += mRenderInfo[tile];
                    } else {
                        entry->mRenderInfo[i] += ZTileRenderInfo(tile);
                    }
                    mTileLookup[tile] = entry;
                }
            }
        }
        mTiles += newTiles;
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

    void initWalls(TileRotateInfo* tileInfo)
    {
        int floorWidth = 64 * 2;
        int floorHeight = 32 * 2;

        // Offset North walls onto the south edge
        BuildingTile *btileN = BuildingTilesMgr::instance()->get(tileInfo->mTileNames[0]);
        if (!btileN->isNone()) {
            Tile* tileN = BuildingTilesMgr::instance()->tileFor(btileN);
            ZTileRenderInfo tileInfo(tileN);
            tileInfo.mOffset = QPoint(-floorWidth / 2, floorHeight / 2);
            tileInfo.mOrder = ZTileRenderOrder::SouthWall;
            mSouthWallTiles[tileN] += tileInfo;
            // Set render order
            tileInfo.mOrder = ZTileRenderOrder::NorthWall;
            tileInfo.mOffset = QPoint();
            mRenderInfo[tileN] += tileInfo;
        }

        // Offset West walls onto the east edge
        BuildingTile *btileW = BuildingTilesMgr::instance()->get(tileInfo->mTileNames[3]);
        if (!btileW->isNone()) {
            Tile* tileW = BuildingTilesMgr::instance()->tileFor(btileW);
            ZTileRenderInfo tileInfo(tileW);
            tileInfo.mOffset = QPoint(floorWidth / 2, floorHeight / 2);
            tileInfo.mOrder = ZTileRenderOrder::EastWall;
            mEastWallTiles[tileW] += tileInfo;
            // Set render order
            tileInfo.mOrder = ZTileRenderOrder::WestWall;
            tileInfo.mOffset = QPoint();
            mRenderInfo[tileW] += tileInfo;
        }

        // NorthWest walls become 2 tiles
        if (tileInfo->mType == TileRotateType::Wall) {
            BuildingTile *btileNW = BuildingTilesMgr::instance()->get(tileInfo->mTileNames[1]);
            if (!btileNW->isNone() && !btileN->isNone() && !btileW->isNone()) {
                Tile* tileNW = BuildingTilesMgr::instance()->tileFor(btileNW);
                Tile* tileN = BuildingTilesMgr::instance()->tileFor(btileN);
                Tile* tileW = BuildingTilesMgr::instance()->tileFor(btileW);
                // NorthWest -> NorthEast
                {
                    // North wall is unchanged
                    ZTileRenderInfo tileInfoN(tileN);
                    tileInfoN.mOrder = ZTileRenderOrder::NorthWall;
                    mNorthEastWallTiles[tileNW] += tileInfoN;
                    // Offset West walls onto the east edge
                    ZTileRenderInfo tileInfoW(tileW);
                    tileInfoW.mOffset = QPoint(floorWidth / 2, floorHeight / 2);
                    tileInfoW.mOrder = ZTileRenderOrder::EastWall;
                    mNorthEastWallTiles[tileNW] += tileInfoW;
                }
                // NorthWest -> SouthEast
                {
                    // Offset North walls onto the south edge
                    ZTileRenderInfo tileInfoN(tileN);
                    tileInfoN.mOffset = QPoint(-floorWidth / 2, floorHeight / 2);
                    tileInfoN.mOrder = ZTileRenderOrder::SouthWall;
                    mSouthEastWallTiles[tileNW] += tileInfoN;
                    // Offset West walls onto the east edge
                    ZTileRenderInfo tileInfoW(tileW);
                    tileInfoW.mOffset = QPoint(floorWidth / 2, floorHeight / 2);
                    tileInfoW.mOrder = ZTileRenderOrder::EastWall;
                    mSouthEastWallTiles[tileNW] += tileInfoW;
                }
                // NorthWest -> SouthWest
                {
                    // Offset North walls onto the south edge
                    ZTileRenderInfo tileInfoN(tileN);
                    tileInfoN.mOffset = QPoint(-floorWidth / 2, floorHeight / 2);
                    tileInfoN.mOrder = ZTileRenderOrder::SouthWall;
                    mSouthWestWallTiles[tileNW] += tileInfoN;
                    // West wall is unchanged
                    ZTileRenderInfo tileInfoW(tileW);
                    tileInfoW.mOrder = ZTileRenderOrder::WestWall;
                    mSouthWestWallTiles[tileNW] += tileInfoW;
                }
            }
        }
    }

    void initWalls()
    {
        for (TileRotateInfo* tileInfo : mTiles) {
            switch (tileInfo->mType) {
            case TileRotateType::Door:
            case TileRotateType::DoorFrame:
            case TileRotateType::Wall:
            case TileRotateType::WallExtra:
            case TileRotateType::Window:
            case TileRotateType::WindowFrame:
                initWalls(tileInfo);
                break;
            default:
                break;
            }
        }
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
            mFurnitureTiles = file.takeTiles();
            qDeleteAll(mTiles);
            {
                for (TileRotateFileInfo* fileInfo : mFurnitureTiles) {
                    FurnitureTiles* furnitureTiles = fileInfo->mFurnitureTiles;
                    FurnitureTile* ft[4];
                    ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                    ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                    ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                    ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
#if 0
                    if (ft[1]->isEmpty())
                        ft[1] = ft[3]; // E = W
                    if (ft[2]->isEmpty())
                        ft[2] = ft[0]; // S = N
#endif
                    int width = ft[0]->width();
                    int height = ft[0]->height();
                    for (int dy = 0; dy < height; dy++) {
                        for (int dx = 0; dx < width; dx++) {
                            TileRotateInfo entry;
                            bool empty = true;
                            for (int i = 0; i < 4; i++) {
                                QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                                BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                                if (buildingTile != nullptr && !buildingTile->isNone()) {
                                    entry.mTileNames[i] = buildingTile->name();
                                    empty = false;
                                }
                            }
                            if (empty)
                                continue;
                            TileRotateInfo *tileInfo = new TileRotateInfo(entry);
                            tileInfo->mType = fileInfo->mType;
                            mTiles += tileInfo;
                        }
                    }
                }
            }
            initWalls();
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
        if (mTiles.isEmpty()) {
            tempLazyInit();
        }

        TileRotateInfo* info = mTileLookup.value(tile);
        if (info == nullptr) {
//            if (rotation == MapRotation::NotRotated)
            tileInfos += ZTileRenderInfo(tile);
            return;
//            return BuildingTilesMgr::instance()->noneTiledTile();
        }
        int index = 0;
        switch (rotation) {
        case MapRotation::NotRotated:
            index = 0;
            break;
        case MapRotation::Clockwise90:
            index = 1;
            break;
        case MapRotation::Clockwise180:
            index = 2;
            break;
        case MapRotation::Clockwise270:
            index = 3;
            break;
        }
        if (info->mTiles[0] == tile)
            index += 0;
        else if (info->mTiles[1] == tile)
            index += 1;
        else if (info->mTiles[2] == tile)
            index += 2;
        else if (info->mTiles[3] == tile)
            index += 3;
        tileInfos += info->mRenderInfo[index % 4];
    }

    void reload()
    {
        qDeleteAll(mTiles);
        mTiles.clear();
        mTileLookup.clear();
        qDeleteAll(mFurnitureTiles);
        mFurnitureTiles.clear();
        mRenderInfo.clear();
        mEastWallTiles.clear();
        mSouthWallTiles.clear();
        mNorthEastWallTiles.clear();
        mSouthEastWallTiles.clear();
        mSouthWestWallTiles.clear();
    }

    TileRotation& mOuter;
    QList<TileRotateInfo*> mTiles;
    QMap<Tile*, TileRotateInfo*> mTileLookup;
    QList<TileRotateFileInfo *> mFurnitureTiles;
    QMap<Tile*, QVector<ZTileRenderInfo>> mRenderInfo;
    QMap<Tile*, QVector<ZTileRenderInfo>> mEastWallTiles;
    QMap<Tile*, QVector<ZTileRenderInfo>> mSouthWallTiles;
    QMap<Tile*, QVector<ZTileRenderInfo>> mNorthEastWallTiles;
    QMap<Tile*, QVector<ZTileRenderInfo>> mSouthEastWallTiles;
    QMap<Tile*, QVector<ZTileRenderInfo>> mSouthWestWallTiles;
};

// // // // //

TileRotateFileInfo::TileRotateFileInfo()
    : mFurnitureTiles(new FurnitureTiles(false))
{

}

TileRotateFileInfo::~TileRotateFileInfo()
{
    if (mOwnsFurniture) {
        delete mFurnitureTiles;
    }
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
    mPrivate->mFurnitureTiles = file.takeTiles();
    mPrivate->fileLoaded();
}

const QList<TileRotateFileInfo *> TileRotation::furnitureTiles() const
{
    return mPrivate->mFurnitureTiles;
}

void TileRotation::rotateTile(Tile *tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
{
    mPrivate->rotateTile(tile, rotation, tileInfos);
}

void TileRotation::reload()
{
    mPrivate->reload();
}
