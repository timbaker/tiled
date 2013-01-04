/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGFLOOR_H
#define BUILDINGFLOOR_H

#include <QHash>
#include <QList>
#include <QMap>
#include <QRegion>
#include <QString>
#include <QStringList>
#include <QVector>

namespace BuildingEditor {

class BuildingObject;
class Building;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FloorType;
class FurnitureObject;
class Room;
class Stairs;
class Window;

class SparseTileGrid
{
public:
    SparseTileGrid(int width, int height)
        : mWidth(width)
        , mHeight(height)
        , mUseVector(false)
    {
    }

    int size() const
    { return mWidth * mHeight; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    const QString &at(int index) const
    {
        if (mUseVector)
            return mCellsVector[index];
        QHash<int,QString>::const_iterator it = mCells.find(index);
        if (it != mCells.end())
            return *it;
        return mEmptyCell;
    }

    const QString &at(int x, int y) const
    {
        Q_ASSERT(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
        return at(x + y * mWidth);
    }

    void replace(int index, const QString &tile)
    {
        if (mUseVector) {
            mCellsVector[index] = tile;
            return;
        }
        QHash<int,QString>::iterator it = mCells.find(index);
        if (it == mCells.end()) {
            if (tile.isEmpty())
                return;
            mCells.insert(index, tile);
        } else if (!tile.isEmpty())
            (*it) = tile;
        else
            mCells.erase(it);
        if (mCells.size() > 300 * 300 / 3)
            swapToVector();
    }

    void replace(int x, int y, const QString &tile)
    {
        Q_ASSERT(x >= 0 && x < mWidth && y >= 0 && y < mHeight);
        int index = y * mWidth + x;
        replace(index, tile);
    }

    void setTile(int index, const QString &tile)
    {
        replace(index, tile);
    }

    bool isEmpty() const
    { return !mUseVector && mCells.isEmpty(); }

    void clear()
    {
        if (mUseVector)
            mCellsVector.fill(mEmptyCell);
        else
            mCells.clear();
    }

private:
    void swapToVector()
    {
        Q_ASSERT(!mUseVector);
        mCellsVector.resize(size());
        QHash<int,QString>::const_iterator it = mCells.begin();
        while (it != mCells.end()) {
            mCellsVector[it.key()] = (*it);
            ++it;
        }
        mCells.clear();
        mUseVector = true;
    }

    int mWidth, mHeight;
    QHash<int,QString> mCells;
    QVector<QString> mCellsVector;
    bool mUseVector;
    QString mEmptyCell;
};

class BuildingFloor
{
public:

    class Square
    {
    public:
        enum SquareSection
        {
            SectionInvalid = -1,
            SectionFloor,
            SectionFloorGrime,
            SectionFloorGrime2,
            SectionWall,
            SectionWall2,
            SectionRoofCap,
            SectionRoofCap2,
            SectionWallOverlay,
            SectionWallOverlay2,
            SectionWallGrime,
            SectionWallFurniture,
            SectionFrame,
            SectionDoor,
            SectionCurtains, // West, North windows
            SectionFurniture,
            SectionFurniture2,
            SectionCurtains2, // East, South windows
            SectionRoof,
            SectionRoof2,
            SectionRoofTop,
            MaxSection
        };

        enum WallOrientation
        {
            WallOrientN,
            WallOrientW,
            WallOrientNW,
            WallOrientSE
        };

        Square();
        ~Square();

        QVector<BuildingTileEntry*> mEntries;
        QVector<int> mEntryEnum;
        WallOrientation mWallOrientation;
        bool mExterior;
        QVector<BuildingTile*> mTiles;

        struct {
            BuildingTileEntry *entry;
            bool exterior;
        } mWallN, mWallW;
        void SetWallN(BuildingTileEntry *tile, bool exterior = true);
        void SetWallW(BuildingTileEntry *tile, bool exterior = true);

        bool IsWallOrient(WallOrientation orient);

        void ReplaceFloor(BuildingTileEntry *tile, int offset);
        void ReplaceWall(BuildingTileEntry *tile, WallOrientation orient, bool exterior = true);
        void ReplaceDoor(BuildingTileEntry *tile, int offset);
        void ReplaceFrame(BuildingTileEntry *tile, int offset);
        void ReplaceCurtains(Window *window, bool exterior);
        void ReplaceFurniture(BuildingTileEntry *tile, int offset = 0);
        void ReplaceFurniture(BuildingTile *tile, SquareSection section,
                              SquareSection section2 = SectionInvalid);
        void ReplaceRoof(BuildingTileEntry *tile, int offset = 0);
        void ReplaceRoofCap(BuildingTileEntry *tile, int offset = 0);
        void ReplaceRoofTop(BuildingTileEntry *tile, int offset);

        int getWallOffset();
    };

    QVector<QVector<Square> > squares;

    BuildingFloor(Building *building, int level);
    ~BuildingFloor();

    void setLevel(int level)
    { mLevel = level; }

    Building *building() const
    { return mBuilding; }

    BuildingFloor *floorAbove() const;
    BuildingFloor *floorBelow() const;

    bool isTopFloor() const;
    bool isBottomFloor() const;

    void insertObject(int index, BuildingObject *object);
    BuildingObject *removeObject(int index);

    int indexOf(BuildingObject *object)
    { return mObjects.indexOf(object); }

    BuildingObject *object(int index) const
    { return mObjects.at(index); }

    const QList<BuildingObject*> &objects() const
    { return mObjects; }

    int objectCount() const
    { return mObjects.size(); }

    BuildingObject *objectAt(int x, int y);

    inline BuildingObject *objectAt(const QPoint &pos)
    { return objectAt(pos.x(), pos.y()); }

    Door *GetDoorAt(int x, int y);
    Window *GetWindowAt(int x, int y);
    Stairs *GetStairsAt(int x, int y);
    FurnitureObject *GetFurnitureAt(int x, int y);

    void SetRoomAt(int x, int y, Room *room);

    inline void SetRoomAt(const QPoint &pos, Room *room)
    { SetRoomAt(pos.x(), pos.y(), room); }

    Room *GetRoomAt(const QPoint &pos);
    Room *GetRoomAt(int x, int y)
    { return GetRoomAt(QPoint(x, y)); }

    void setGrid(const QVector<QVector<Room*> > &grid);

    const QVector<QVector<Room*> > &grid() const
    { return mRoomAtPos; }

    void LayoutToSquares();

    int width() const;
    int height() const;

    QRect bounds() const;

    int level() const
    { return mLevel; }

    QVector<QRect> roomRegion(Room *room);

    QVector<QVector<Room*> > resizeGrid(const QSize &newSize) const;
    QMap<QString,SparseTileGrid*> resizeGrime(const QSize &newSize) const;

    void rotate(bool right);
    void flip(bool horizontal);

    BuildingFloor *clone();

    QStringList grimeLayers() const
    { return mGrimeGrid.keys(); }

    QString grimeAt(const QString &layerName, int x, int y) const
    {
        if (mGrimeGrid.contains(layerName))
            return mGrimeGrid[layerName]->at(x, y);
        return QString();
    }

    QMap<QString,SparseTileGrid*> setGrime(const QMap<QString,SparseTileGrid*> &grime)
    {
        QMap<QString,SparseTileGrid*> old = mGrimeGrid;
        mGrimeGrid = grime;
        return old;
    }

    void setGrime(const QString &layerName, int x, int y, const QString &tileName)
    {
        if (!mGrimeGrid.contains(layerName))
            mGrimeGrid[layerName] = new SparseTileGrid(width() + 1, height() + 1);
        mGrimeGrid[layerName]->replace(x, y, tileName);
    }

    void setLayerOpacity(const QString &layerName, qreal opacity)
    { mLayerOpacity[layerName] = opacity; }

    qreal layerOpacity(const QString &layerName) const
    {
        if (mLayerOpacity.contains(layerName))
            return mLayerOpacity[layerName];
        return 1.0f;
    }

    void setLayerVisibility(const QString &layerName, bool visible)
    { mLayerVisibility[layerName] = visible; }

    bool layerVisibility(const QString &layerName) const
    {
        if (mLayerVisibility.contains(layerName))
            return mLayerVisibility[layerName];
        return 1.0f;
    }

private:
    Building *mBuilding;
    QVector<QVector<Room*> > mRoomAtPos;
    QVector<QVector<int> > mIndexAtPos;
    int mLevel;
    QList<BuildingObject*> mObjects;
    QMap<QString,SparseTileGrid*> mGrimeGrid;
    QMap<QString,qreal> mLayerOpacity;
    QMap<QString,bool> mLayerVisibility;
};

} // namespace BuildingEditor

#endif // BUILDINGFLOOR_H
