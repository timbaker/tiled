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
#include <QString>
#include <QVector>

namespace BuildingEditor {

class BaseMapObject;
class Building;
class Door;
class FloorType;
class Layout;
class Stairs;
class WallType;
class Window;

class BuildingFloor
{
public:

    class WallTile
    {
    public:
        enum WallSection
        {
            N,
            NDoor,
            W,
            WDoor,
            NW,
            SE,
            WWindow,
            NWindow
        };

        WallSection Section;
        WallType *Type;

        WallTile(WallSection section, WallType *type) :
            Section(section),
            Type(type)
        {
        }
    };

    class FloorTile
    {
    public:
        FloorType *Type;

        FloorTile(FloorType *type) :
            Type(type)
        {

        }
    };

    WallType *exteriorWall;
    QVector<WallType*> interiorWalls;
    QVector<FloorType*> floors;

    QList<BaseMapObject*> Objects;

    class Square
    {
    public:
        Square() :
            floorTile(0)
        {}
        ~Square()
        {
            delete floorTile;
            qDeleteAll(walls);
        }

        FloorTile *floorTile;
        QList<WallTile*> walls;
        QString stairsTexture;

        bool Contains(WallTile::WallSection sec);

        void Replace(WallTile *tile, WallTile::WallSection secToReplace);
        void ReplaceWithDoor(WallTile::WallSection direction);
        void ReplaceWithWindow(WallTile::WallSection direction);
    };

    QVector<QVector<Square> > squares;


    BuildingFloor(Building *building, int level);

    Building *building() const
    { return mBuilding; }

    Layout *layout() const
    { return mLayout; }

    Door *GetDoorAt(int x, int y);
    Window *GetWindowAt(int x, int y);
    Stairs *GetStairsAt(int x, int y);

    void LayoutToSquares();

    bool IsTopStairAt(int x, int y);

    int width() const;
    int height() const;

    int level() const
    { return mLevel; }

private:
    Building *mBuilding;
    Layout *mLayout;
    int mLevel;
};

} // namespace BuildingEditor

#endif // BUILDINGFLOOR_H
