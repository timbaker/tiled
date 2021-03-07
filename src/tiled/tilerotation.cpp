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
        for (TileRotateInfo *entry : mTiles) {
            for (int i = 0; i < 4; i++) {
                BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(entry->mTileNames[i]);
                Tile* tile = BuildingTilesMgr::instance()->tileFor(buildingTile);
                entry->mTiles[i] = tile;
            }
            for (int i = 0; i < 4; i++) {
                Tile* tile = entry->mTiles[i];
                if (tile == BuildingTilesMgr::instance()->noneTiledTile()) {
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
    }

    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated: // w,h=3,2 x,y=2,1 -> 2,1
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x()); // w,h=3,2 x,y=2,1 -> 0,2
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1); // w,h=3,2 x,y=2,1 -> 0,0
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1); // w,h=3,2 x,y=2,1 -> 1,0
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

#if 0

    void initFromBuildingTiles(const QList<BuildingTileEntry*> &entries, int n, int e, int s, int w)
    {
        for (BuildingTileEntry* bte : entries) {
            BuildingTile *tileN = bte->tile(n);
            BuildingTile *tileW = bte->tile(w);
            if (tileN->isNone() && tileW->isNone())
                continue;
            TileRotateInfo entry;
            entry.mTileNames[0] = tileN->name();
            entry.mTileNames[3] = tileW->name();
            mTiles += new TileRotateInfo(entry);
            addFurnitureTilesForRotateInfo(*mTiles.last());
        }
    }

    void addFurnitureTilesForRotateInfo(const TileRotateInfo& rotateInfo)
    {
        FurnitureTile::FurnitureOrientation orient[4] = {
            FurnitureTile::FurnitureN,
            FurnitureTile::FurnitureE,
            FurnitureTile::FurnitureS,
            FurnitureTile::FurnitureW
        };

        FurnitureTiles* furnitureTiles1 = new FurnitureTiles(false);
        for (int j = 0; j < 4; j++) {
            FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
            BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(rotateInfo.mTileNames[j]);
            if (!buildingTile->isNone()) {
                furnitureTile1->setTile(0, 0, buildingTile);
            }
            furnitureTiles1->setTile(furnitureTile1);
        }
        mFurnitureTiles += furnitureTiles1;
    }

    MapRotation rotation[4] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    void initFromBuildingTiles()
    {
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoors()->entries(), BTC_Doors::North, -1, -1, BTC_Doors::West);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoorFrames()->entries(), BTC_DoorFrames::North, -1, -1, BTC_DoorFrames::West);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::North, -1, -1, BTC_Walls::West);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::North, -1, -1, BTC_Walls::West);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWallTrim()->entries(), BTC_Walls::North, -1, -1, BTC_Walls::West);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWallTrim()->entries(), BTC_Walls::North, -1, -1, BTC_Walls::West);

        QList<FurnitureTile*> addThese;

        // size=1x1
        const QList<FurnitureGroup*> furnitureGroups = FurnitureGroups::instance()->groups();
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
                if (ft[0]->size() != QSize(1, 1))
                    continue;
                TileRotateInfo entry;
                bool empty = true;
                for (int i = 0; i < 4; i++) {
                    BuildingTile* buildingTile = ft[i]->tile(0, 0);
                    if (buildingTile != nullptr) {
                        entry.mTileNames[i] = buildingTile->name();
                        empty = false;
                    }
                }
                if (empty)
                    continue;
                mTiles += new TileRotateInfo(entry);
                addThese << ft[0] << ft[1] << ft[2] << ft[3];
            }
        }

        // size > 1x1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
                if (ft[0]->size() == QSize(1, 1))
                    continue;
                int width = ft[0]->width();
                int height = ft[0]->height();
                int add = false;
                for (int dy = 0; dy < height; dy++) {
                    for (int dx = 0; dx < width; dx++) {
                        TileRotateInfo entry;
                        bool empty = true;
                        for (int i = 0; i < 4; i++) {
                            QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                            BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                            if (buildingTile != nullptr) {
                                entry.mTileNames[i] = buildingTile->name();
                                empty = false;
                            }
                        }
                        if (empty)
                            continue;
                        mTiles += new TileRotateInfo(entry);
                        add = true;
                    }
                }
                if (add) {
                    addThese << ft[0] << ft[1] << ft[2] << ft[3];
                }
            }
        }

        FurnitureTile::FurnitureOrientation orient[4] = {
            FurnitureTile::FurnitureN,
            FurnitureTile::FurnitureE,
            FurnitureTile::FurnitureS,
            FurnitureTile::FurnitureW
        };

        for (int i = 0; i < addThese.size(); i += 4) {
            FurnitureTile* ft[4];
            ft[0] = addThese[i+0];
            ft[1] = addThese[i+1];
            ft[2] = addThese[i+2];
            ft[3] = addThese[i+3];
            FurnitureTiles* furnitureTiles1 = new FurnitureTiles(false);
            for (int j = 0; j < 4; j++) {
                FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
                for (int dy = 0; dy < ft[j]->height(); dy++) {
                    for (int dx = 0; dx < ft[j]->width(); dx++) {
                        BuildingTile* buildingTile = ft[j]->tile(dx, dy);
                        if (buildingTile != nullptr) {
                            furnitureTile1->setTile(dx, dy, buildingTile);
                        }
                    }
                }
                furnitureTiles1->setTile(furnitureTile1);
            }
            mFurnitureTiles += furnitureTiles1;
        }

        for (BuildingTileEntry* bte : BuildingTilesMgr::instance()->catFloors()->entries()) {
            BuildingTile *tile = bte->tile(BTC_Floors::Floor);
            if (tile->isNone())
                continue;
            mNoRotateTileNames += tile->name();
        }

        fileLoaded();
    }
#endif

#if 1
    void initDoors(BuildingTileEntry *entry)
    {
        int floorWidth = 64 * 2;
        int floorHeight = 32 * 2;

        // Offset North doors onto the south edge
        BuildingTile *btileN = entry->tile(BTC_Doors::North);
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

        // Offset West doors onto the east edge
        BuildingTile *btileW = entry->tile(BTC_Doors::West);
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
#if 0 // FIXME
        // NorthWest walls become 2 tiles
        if (tileInfo->mCorners) {
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
#endif
    }

    void initWalls()
    {
        for (TileRotateInfo* tileInfo : mTiles) {
            switch (tileInfo->mType) {
            case TileRotateType::Door:
            case TileRotateType::DoorFrame:
            case TileRotateType::Wall:
            case TileRotateType::Window:
            case TileRotateType::WindowFrame:
                initWalls(tileInfo);
                break;
            default:
                break;
            }
        }
    }
#else
    void initDoors(BuildingTileEntry *entry)
    {
        int floorWidth = 64 * 2;
        int floorHeight = 32 * 2;

        // Offset North doors onto the south edge
        BuildingTile *btileN = entry->tile(BTC_Doors::North);
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

        // Offset West doors onto the east edge
        BuildingTile *btileW = entry->tile(BTC_Doors::West);
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

    }

    void initWalls(BuildingTileEntry* entry)
    {
        int floorWidth = 64 * 2;
        int floorHeight = 32 * 2;

        // Offset North walls onto the south edge
        BuildingTile *btileN = entry->tile(BTC_Walls::North);
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
        // Offset North door frames onto the south edge
        BuildingTile *btileDoorFrameN = entry->tile(BTC_Walls::NorthDoor);
        if (!btileDoorFrameN->isNone()) {
            Tile* tileN = BuildingTilesMgr::instance()->tileFor(btileDoorFrameN);
            ZTileRenderInfo tileInfo(tileN);
            tileInfo.mOffset = QPoint(-floorWidth / 2, floorHeight / 2);
            tileInfo.mOrder = ZTileRenderOrder::SouthWall;
            mSouthWallTiles[tileN] += tileInfo;
            // Set render order
            tileInfo.mOrder = ZTileRenderOrder::NorthWall;
            tileInfo.mOffset = QPoint();
            mRenderInfo[tileN] += tileInfo;
        }
        // Offset North window frames onto the south edge
        BuildingTile *btileWindowFrameN = entry->tile(BTC_Walls::NorthWindow);
        if (!btileWindowFrameN->isNone()) {
            Tile* tileN = BuildingTilesMgr::instance()->tileFor(btileWindowFrameN);
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
        BuildingTile *btileW = entry->tile(BTC_Walls::West);
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
        // Offset West door frames onto the east edge
        BuildingTile *btileDoorFrameW = entry->tile(BTC_Walls::WestDoor);
        if (!btileDoorFrameW->isNone()) {
            Tile* tileW = BuildingTilesMgr::instance()->tileFor(btileDoorFrameW);
            ZTileRenderInfo tileInfo(tileW);
            tileInfo.mOffset = QPoint(floorWidth / 2, floorHeight / 2);
            tileInfo.mOrder = ZTileRenderOrder::EastWall;
            mEastWallTiles[tileW] += tileInfo;
            // Set render order
            tileInfo.mOrder = ZTileRenderOrder::WestWall;
            tileInfo.mOffset = QPoint();
            mRenderInfo[tileW] += tileInfo;
        }
        // Offset West window frames onto the east edge
        BuildingTile *btileWindowFrameW = entry->tile(BTC_Walls::WestWindow);
        if (!btileWindowFrameW->isNone()) {
            Tile* tileW = BuildingTilesMgr::instance()->tileFor(btileWindowFrameW);
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
        BuildingTile *btileNW = entry->tile(BTC_Walls::NorthWest);
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

    void initWalls()
    {
        auto* category = BuildingTilesMgr::instance()->catEWalls();
        for (auto* entry : category->entries()) {
            initWalls(entry);
        }

        category = BuildingTilesMgr::instance()->catIWalls();
        for (auto* entry : category->entries()) {
            initWalls(entry);
        }

        category = BuildingTilesMgr::instance()->catDoors();
        for (auto* entry : category->entries()) {
            initDoors(entry);
        }
    }
#endif

#if 0
    Tileset* initRuntimeTilesets(Tileset *newTileset, int &tileCount, BuildingTileEntry* entry, int north, int west)
    {
        int floorWidth = 64;
        int floorHeight = 32;

        int tileWidth = 64;
        int tileHeight = 128;
        int columns = 8;
        int rows = 16;

        BuildingTile *btileN = entry->tile(north);
        if (!btileN->isNone()) {
            Tile* tileN = BuildingTilesMgr::instance()->tileFor(btileN);
            Tile* newTile = newTileset->tileAt(tileCount);
            QImage image(tileWidth, tileHeight, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            float scale = 0.5f;
            QRect r(0, 0, tileN->image().width() * scale, tileN->image().height() * scale);
            r.translate(tileN->offset() * scale);
            r.translate(-floorWidth / 2, floorHeight / 2); // onto south edge
            painter.drawImage(r, tileN->image());
            painter.end();
            newTile->setImage(image);
            mSouthWallTiles[tileN] = newTile;
//                qDebug() << "initRuntimeTilesets " << tileN->tileset()->name() << "_" << tileN->id();
            ++tileCount;
            if (tileCount == newTileset->tileCount()) {
                newTileset = new Tileset(QString(QLatin1Literal("ROTATE_%1")).arg(mRuntimeTilesets.size() + 1), tileWidth, tileHeight);
                newTileset->loadFromNothing(QSize(columns * tileWidth, rows * tileHeight), QLatin1Literal("TileRotation.png"));
                mRuntimeTilesets += newTileset;
                tileCount = 0;
            }
        }

        BuildingTile *btileW = entry->tile(west);
        if (!btileW->isNone()) {
            Tile* tileW = BuildingTilesMgr::instance()->tileFor(btileW);
            Tile* newTile = newTileset->tileAt(tileCount);
            QImage image(tileWidth, tileHeight, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            float scale = 0.5f;
            QRect r(0, 0, tileW->image().width() * scale, tileW->image().height() * scale);
            r.translate(tileW->offset() * scale);
            r.translate(floorWidth / 2, floorHeight / 2); // onto east edge
            painter.drawImage(r, tileW->image());
            painter.end();
            newTile->setImage(image);
            mEastWallTiles[tileW] = newTile;
//                qDebug() << "initRuntimeTilesets " << tileN->tileset()->name() << "_" << tileN->id();
            ++tileCount;
            if (tileCount == newTileset->tileCount()) {
                newTileset = new Tileset(QString(QLatin1Literal("ROTATE_%1")).arg(mRuntimeTilesets.size() + 1), tileWidth, tileHeight);
                newTileset->loadFromNothing(QSize(columns * tileWidth, rows * tileHeight), QLatin1Literal("TileRotation.png"));
                mRuntimeTilesets += newTileset;
                tileCount = 0;
            }
        }

        BuildingTile *btileNW = entry->tile(BTC_Walls::NorthWest);
        if (!btileNW->isNone()) {
            Tile* tileNW = BuildingTilesMgr::instance()->tileFor(btileNW);
            Tile* newTile = newTileset->tileAt(tileCount);
            QImage image(tileWidth, tileHeight, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            QImage imageNW = tileNW->finalImage(tileWidth, tileHeight);
            QRectF r(0, 0, tileWidth / 2, tileHeight);
            r.translate(floorWidth / 2, floorHeight / 2);
            // west half onto east edge
            painter.drawImage(r, imageNW, QRectF(0, 0, tileWidth / 2, tileHeight));
            // north half onto south edge
            painter.drawImage(QRectF(0, 0, tileWidth / 2, tileHeight), imageNW, QRectF(tileWidth / 2, 0, tileWidth / 2, tileHeight));
            painter.end();
            newTile->setImage(image);
            mSouthEastWallTiles[tileNW] = newTile;
            ++tileCount;
            if (tileCount == newTileset->tileCount()) {
                newTileset = new Tileset(QString(QLatin1Literal("ROTATE_%1")).arg(mRuntimeTilesets.size() + 1), tileWidth, tileHeight);
                newTileset->loadFromNothing(QSize(columns * tileWidth, rows * tileHeight), QLatin1Literal("TileRotation.png"));
                mRuntimeTilesets += newTileset;
                tileCount = 0;
            }
        }

        return newTileset;
    }

    void initRuntimeTilesets()
    {
        qDeleteAll(mRuntimeTilesets);
        mRuntimeTilesets.clear();

        int tileWidth = 64;
        int tileHeight = 128;
        int columns = 8;
        int rows = 16;
        Tileset* newTileset = new Tileset(QLatin1Literal("ROTATED_01"), tileWidth, tileHeight);
        newTileset->loadFromNothing(QSize(columns * tileWidth, rows * tileHeight), QLatin1Literal("TileRotation.png"));
        mRuntimeTilesets += newTileset;
        int tileCount = 0;

        auto* category = BuildingTilesMgr::instance()->catEWalls();
        for (auto* entry : category->entries()) {
            newTileset = initRuntimeTilesets(newTileset, tileCount, entry, BTC_Walls::North, BTC_Walls::West);
        }

        category = BuildingTilesMgr::instance()->catIWalls();
        for (auto* entry : category->entries()) {
            newTileset = initRuntimeTilesets(newTileset, tileCount, entry, BTC_Walls::North, BTC_Walls::West);
        }
    }
#endif

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
#if 0
            initRuntimeTilesets();
#endif
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
#if 0
    MapRotation rotationOf(Tile* tile)
    {
        // FIXME: temporary lazy init
        if (mTiles.isEmpty()) {
            tempLazyInit();
        }

        TileRotateInfo* info = mTileLookup.value(tile);
        if (info == nullptr) {
            return MapRotation::NotRotated;
        }
        if (info->mTiles[0] == tile)
            return MapRotation::NotRotated;
        else if (info->mTiles[1] == tile)
            return MapRotation::Clockwise90;
        else if (info->mTiles[2] == tile)
            return MapRotation::Clockwise180;
        else if (info->mTiles[3] == tile)
            return MapRotation::Clockwise270;
        return MapRotation::NotRotated;
    }
#endif
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
#if 0
    QList<Tileset*> mRuntimeTilesets;
#endif
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

#if 0
MapRotation TileRotation::rotationOf(Tile *tile) const
{
    return mPrivate->rotationOf(tile);
}
#endif

void TileRotation::reload()
{
    mPrivate->reload();
}
