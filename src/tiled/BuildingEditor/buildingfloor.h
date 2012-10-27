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

#include <QList>
#include <QRegion>
#include <QString>
#include <QVector>

namespace BuildingEditor {

class BaseMapObject;
class Building;
class BuildingTile;
class Door;
class FloorType;
class Room;
class Stairs;
class Window;

class BuildingFloor
{
public:

    class Square
    {
    public:
        enum SquareSection
        {
            SectionFloor,
            SectionWall,
            SectionFrame,
            SectionDoor,
            SectionFurniture,
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

        BuildingTile *mTiles[MaxSection];
        int mTileOffset[MaxSection];
        WallOrientation mWallOrientation;
        QString stairsTexture;

        bool IsWallOrient(WallOrientation orient)
        { return mTiles[SectionWall] && (mWallOrientation == orient); }

        void ReplaceWall(BuildingTile *tile, WallOrientation orient);
        void ReplaceDoor(BuildingTile *tile, int offset);
        void ReplaceFrame(BuildingTile *tile, int offset);
        void ReplaceFurniture(BuildingTile *tile, int offset);

        int getWallOffset();
        int getTileIndexForDoor();
    };

    QVector<QVector<Square> > squares;

    BuildingFloor(Building *building, int level);
    ~BuildingFloor();

    Building *building() const
    { return mBuilding; }

    BuildingFloor *floorAbove() const;
    BuildingFloor *floorBelow() const;

    void insertObject(int index, BaseMapObject *object);
    BaseMapObject *removeObject(int index);

    int indexOf(BaseMapObject *object)
    { return mObjects.indexOf(object); }

    BaseMapObject *object(int index) const
    { return mObjects.at(index); }

    const QList<BaseMapObject*> &objects() const
    { return mObjects; }

    int objectCount() const
    { return mObjects.size(); }

    BaseMapObject *objectAt(int x, int y);

    inline BaseMapObject *objectAt(const QPoint &pos)
    { return objectAt(pos.x(), pos.y()); }

    Door *GetDoorAt(int x, int y);
    Window *GetWindowAt(int x, int y);
    Stairs *GetStairsAt(int x, int y);

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

    bool IsTopStairAt(int x, int y);

    int width() const;
    int height() const;

    int level() const
    { return mLevel; }

    QRegion roomRegion(Room *room);

    QVector<QVector<Room*> > resized(const QSize &newSize) const;

    void rotate(bool right);
    void flip(bool horizontal);

private:
    Building *mBuilding;
    QVector<QVector<Room*> > mRoomAtPos;
    QVector<QVector<int> > mIndexAtPos;
    int mLevel;
    QList<BaseMapObject*> mObjects;
};

} // namespace BuildingEditor

#endif // BUILDINGFLOOR_H
