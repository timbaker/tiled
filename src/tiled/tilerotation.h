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

#include <QUuid>
#include <QSharedPointer>

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

enum class TileRotatedVisualEdge
{
    None,
    North,
    East,
    South,
    West,
    MAX
};

class TileRotatedVisualData
{
public:
    QStringList mTileNames; // The real tiles to get images from.
    QVector<QPoint> mOffsets; // How to offset the tile images when rendering.
    QVector<TileRotatedVisualEdge> mEdges;

    QVector<ZTileRenderInfo> mRenderInfo; // Doesn't really belong here.

    void addTile(const QString& tileName, const QPoint& offset, TileRotatedVisualEdge edge)
    {
        mTileNames += tileName;
        mOffsets += offset;
        mEdges += edge;
    }

    void addTile(const QString& tileName)
    {
        addTile(tileName, QPoint(), TileRotatedVisualEdge::None);
    }

    void addTileDX(const QString& tileName)
    {
        addTile(tileName, QPoint(1, 0), TileRotatedVisualEdge::East);
    }

    void addTileDY(const QString& tileName)
    {
        addTile(tileName, QPoint(0, 1), TileRotatedVisualEdge::South);
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
};

class TileRotatedVisual
{
public:
    static const char *EDGE_NAMES[int(TileRotatedVisualEdge::MAX) + 1];

    QUuid mUuid;
    TileRotatedVisualData mData[MAP_ROTATION_COUNT];
};

class TileRotatedProperty
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
        , mRotation(MapRotation::NotRotated)
        , mVisual(nullptr)
    {
    }

    QString name() const;

    void clear()
    {
        mVisual = nullptr;
    }

    TilesetRotated *mTileset;
    int mID;
    QPoint mXY; // column,row in the tileset
    MapRotation mRotation; // The rotation of the original real tile.
    QSharedPointer<TileRotatedVisual> mVisual;
    QList<TileRotatedProperty> mProperties;
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

    QSharedPointer<TileRotatedVisual> allocVisual();

    void rotateTile(Tile* tile, MapRotation viewRotation, QVector<Tiled::ZTileRenderInfo>& tileInfos);
    Tile *rotateTile(Tile* tile, MapRotation rotation);
    Tile *tileFor(const QString& tilesetName, int tileID);

    void reload();

    Tile* getRotatedTileDX(const QString &tilesetName, int index);
    Tile* getRotatedTileDY(const QString &tilesetName, int index);
    Tile* getRotatedTile(const QString &tileName, QPoint& offset);

    TileRotated *rotatedTileFor(Tile *tileR);
    Tileset *rotatedTilesetFor(TilesetRotated* tilesetR);

    QString unrotateTile(const QString &tileName, MapRotation viewRotation);

private:
    static TileRotation *mInstance;
    TileRotationPrivate *mPrivate;
};

// namespace Tiled
}

#endif // TILEROTATION_H
