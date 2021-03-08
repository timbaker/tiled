/*
 * tilerotation.h
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

#ifndef TILEROTATION_H
#define TILEROTATION_H

#include "maprotation.h"
#include "tile.h"

#include "BuildingEditor/furnituregroups.h"

#include "ztilelayergroup.h"

namespace Tiled
{

class TileRotationPrivate;

// See TILE_ROTATE_NAMES in tilerotationfile.h
enum class TileRotateType
{
    None,
    Door,
    DoorFrame,
    Wall,
    WallExtra,
    Window,
    WindowFrame,
    MAX
};

class TileRotateInfo
{
public:
    static const int NORTH = 0;
    static const int EAST = 1;
    static const int SOUTH = 2;
    static const int WEST = 3;

    TileRotateInfo()
    {
        mTiles[0] = mTiles[1] = mTiles[2] = mTiles[3] = nullptr;
        mType = TileRotateType::None;
    }
    // NotRotated, Clockwise90, Clockwise180, Clockwise270
    QString mTileNames[MAP_ROTATION_COUNT];
    Tile* mTiles[MAP_ROTATION_COUNT];
    QVector<ZTileRenderInfo> mRenderInfo[MAP_ROTATION_COUNT];
    TileRotateType mType;
};

class TileRotateFileInfo
{
public:
    TileRotateFileInfo();
    ~TileRotateFileInfo();
    BuildingEditor::FurnitureTiles* mFurnitureTiles;
    TileRotateType mType = TileRotateType::None;
    bool mOwnsFurniture = true;
};

class TileRotation : public QObject
{
    Q_OBJECT

public:
    static TileRotation *instance();
    static void deleteInstance();

    TileRotation();

    void readFile(const QString& filePath);

    const QList<TileRotateFileInfo *> furnitureTiles() const;

    void rotateTile(Tile* tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos);

    void reload();

private:
    static TileRotation *mInstance;
    TileRotationPrivate *mPrivate;
};

// namespace Tiled
}

#endif // TILEROTATION_H
