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

class TileRotatedVisual
{
public:
    enum class Edge
    {
        None,
        North,
        East,
        South,
        West,
        MAX
    };

    static const char *EDGE_NAMES[int(Edge::MAX) + 1];

    void addTile(const QString& tileName, const QPoint& offset, Edge edge)
    {
        mTileNames += tileName;
        mOffsets += offset;
        mEdges += edge;
    }

    void addTile(const QString& tileName)
    {
        addTile(tileName, QPoint(), Edge::None);
    }

    void addTileDX(const QString& tileName)
    {
        addTile(tileName, QPoint(1, 0), Edge::East);
    }

    void addTileDY(const QString& tileName)
    {
        addTile(tileName, QPoint(0, 1), Edge::South);
    }

    void clear()
    {
        mTileNames.clear();
        mOffsets.clear();
        mEdges.clear();
    }

    QPoint pixelOffset(int i)
    {
        if (i < 0 || i >= mOffsets.size()) {
            return QPoint();
        }
        int floorWidth = 64;
        int floorHeight = 32;
        QPoint offset = mOffsets[i];
        if (offset == QPoint(1, 0))
            return QPoint(floorWidth / 2, floorHeight / 2);
        else if (offset == QPoint(0, 1))
            return QPoint(-floorWidth / 2, floorHeight / 2);
        return QPoint();
    }

    QStringList mTileNames; // The real tiles to get images from.
    QVector<QPoint> mOffsets; // How to offset the tile images when rendering.
    QVector<Edge> mEdges;
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
//        , mRotationOf(nullptr)
    {
    }
    QString name() const;
    void clear()
    {
//        mRotationOf = nullptr;
//        r0.clear();
//        r90.clear();
//        r180.clear();
//        r270.clear();
        mVisual.clear();
    }
    TilesetRotated *mTileset;
    int mID;
    QPoint mXY; // column,row in the tileset
//    TileRotated *mRotationOf;
    TileRotatedVisual mVisual;
//    TileRotatedVisual r90;
//    TileRotatedVisual r180;
//    TileRotatedVisual r270;
    QList<PropertyRotateInfo> mProperties;
};

class TilesetRotated
{
public:
    ~TilesetRotated();

    QString nameUnrotated()
    {
        return mNameUnrotated;
    }

    QString nameRotated()
    {
        return mNameRotated;
    }

    QString mNameUnrotated;
    QString mNameRotated;
    int mColumnCount;
    QList<TileRotated*> mTiles;
    QVector<TileRotated*> mTileByID;
    MapRotation mRotation;
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
    Tile *tileFor(const QString& tilesetName, int tileID);

    void reload();

    Tile* getRotatedTileDX(const QString &tilesetName, int index);
    Tile* getRotatedTileDY(const QString &tilesetName, int index);
    Tile* getRotatedTile(const QString &tileName, QPoint& offset);

    TileRotated *rotatedTileFor(Tile *tileR);
    Tileset *rotatedTilesetFor(TilesetRotated* tilesetR);

private:
    static TileRotation *mInstance;
    TileRotationPrivate *mPrivate;
};

// namespace Tiled
}

#endif // TILEROTATION_H
