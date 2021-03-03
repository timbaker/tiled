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
                if (tile != BuildingTilesMgr::instance()->noneTiledTile()) {
                    mTileLookup[tile] = entry;
                }
            }
        }
    }

    void initFromBuildingTiles()
    {
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
                TileRotateInfo entry;
                bool empty = true;
                for (int i = 0; i < 4; i++) {
                    if (ft[i]->size() != QSize(1, 1))
                        continue;
                    BuildingTile* buildingTile = ft[i]->tile(0, 0);
                    if (buildingTile != nullptr) {
                        entry.mTileNames[i] = buildingTile->name();
                        empty = false;
                    }
                }
                if (empty)
                    continue;
                mTiles += new TileRotateInfo(entry);
            }
        }

        fileLoaded();
    }

    Tile *rotateTile(Tile *tile, MapRotation rotation)
    {
        // FIXME: temporary lazy init
        if (mTiles.isEmpty()) {
            initFromBuildingTiles();
        }

        TileRotateInfo* info = mTileLookup.value(tile);
        if (info == nullptr) {
            return tile;
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
        return info->mTiles[index % 4];
    }

    TileRotation& mOuter;
    QList<TileRotateInfo*> mTiles;
    QMap<Tile*, TileRotateInfo*> mTileLookup;
};

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
    mPrivate->mTiles = file.takeTiles();
    mPrivate->fileLoaded();
}

Tile *TileRotation::rotateTile(Tile *tile, MapRotation rotation)
{
    return mPrivate->rotateTile(tile, rotation);
}
