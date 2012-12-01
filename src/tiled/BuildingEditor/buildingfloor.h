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
            SectionWall,
            SectionWall2,
            SectionRoofCap,
            SectionRoofCap2,
            SectionWallOverlay,
            SectionWallOverlay2,
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

        bool IsWallOrient(WallOrientation orient);

        void ReplaceFloor(BuildingTileEntry *tile, int offset);
        void ReplaceWall(BuildingTileEntry *tile, WallOrientation orient, bool exterior = true);
        void ReplaceWall2(BuildingTileEntry *tile, WallOrientation orient, bool exterior = true);
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

    QVector<QVector<Room*> > resized(const QSize &newSize) const;

    void rotate(bool right);
    void flip(bool horizontal);

private:
    Building *mBuilding;
    QVector<QVector<Room*> > mRoomAtPos;
    QVector<QVector<int> > mIndexAtPos;
    int mLevel;
    QList<BuildingObject*> mObjects;
};

} // namespace BuildingEditor

#endif // BUILDINGFLOOR_H
