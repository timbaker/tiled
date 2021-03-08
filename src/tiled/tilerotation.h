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

class TilesetRotated;

class TileRotatedDirection
{
public:
    QStringList mTileNames;
};

class PropertyRotateInfo
{
public:
    QString mName;
    QString mValue;
};

class TileRotated
{
public:
    TileRotated()
        : mTileset(nullptr)
    {
    }
    TilesetRotated *mTileset;
    int mID;
    QPoint mXY;
    TileRotatedDirection r90;
    TileRotatedDirection r180;
    TileRotatedDirection r270;
    QList<PropertyRotateInfo> mProperties;
};

class TilesetRotated
{
public:
    ~TilesetRotated();

    QString mName;
    int mColumnCount;
    QList<TileRotated*> mTiles;
    QVector<TileRotated*> mTileByID;
};

class TileRotation : public QObject
{
    Q_OBJECT

public:
    static TileRotation *instance();
    static void deleteInstance();

    TileRotation();

    void readFile(const QString& filePath);

    void rotateTile(Tile* tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos);

    void reload();

    Tile* getRotatedTileDX(const QString &tilesetName, int index);
    Tile* getRotatedTileDY(const QString &tilesetName, int index);
    Tile* getRotatedTile(const QString &tileName);

private:
    static TileRotation *mInstance;
    TileRotationPrivate *mPrivate;
};

// namespace Tiled
}

#endif // TILEROTATION_H
