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

#include "buildingfloor.h"

#include "building.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"

using namespace BuildingEditor;

BuildingFloor::BuildingFloor(Building *building, int level) :
    mBuilding(building),
    mLevel(level)
{
    int w = building->width();
    int h = building->height();

    mRoomAtPos.resize(w);
    mIndexAtPos.resize(w);
    for (int x = 0; x < w; x++) {
        mRoomAtPos[x].resize(h);
        mIndexAtPos[x].resize(h);
        for (int y = 0; y < h; y++) {
            mRoomAtPos[x][y] = 0;
        }
    }
}

BuildingFloor::~BuildingFloor()
{
    qDeleteAll(mObjects);
}

BuildingFloor *BuildingFloor::floorAbove() const
{
    return (mLevel < mBuilding->floorCount() - 1)
            ? mBuilding->floor(mLevel + 1)
            : 0;
}

BuildingFloor *BuildingFloor::floorBelow() const
{
    return (mLevel > 0) ? mBuilding->floor(mLevel - 1) : 0;
}

void BuildingFloor::insertObject(int index, BaseMapObject *object)
{
    mObjects.insert(index, object);
}

BaseMapObject *BuildingFloor::removeObject(int index)
{
    return mObjects.takeAt(index);
}

BaseMapObject *BuildingFloor::objectAt(int x, int y)
{
    foreach (BaseMapObject *object, mObjects)
        if (object->bounds().contains(x, y))
            return object;
    return 0;
}

void BuildingFloor::LayoutToSquares()
{
    int w = width() + 1;
    int h = height() + 1;
    // +1 for the outside walls;
    squares.clear();
    squares.resize(w);
    for (int x = 0; x < w; x++)
        squares[x].resize(h);

    BuildingTile *wtype = 0;

    BuildingTile *exteriorWall;
    QVector<BuildingTile*> interiorWalls;
    QVector<BuildingTile*> floors;

    exteriorWall = mBuilding->exteriorWall();

    foreach (Room *room, mBuilding->rooms()) {
        interiorWalls += room->Wall;
        floors += room->Floor;
    }

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            Room *room = mRoomAtPos[x][y];
            mIndexAtPos[x][y] = room ? mBuilding->indexOf(room) : -1;
        }
    }

    // first put back walls in...

    // N first...

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height() + 1; y++) {
            if (y == height() && mIndexAtPos[x][y - 1] >= 0) {
                // put N wall here...
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < height() && mIndexAtPos[x][y] < 0 && y > 0 && mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) {
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < height() && (y == 0 || mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) && mIndexAtPos[x][y] >= 0) {
                squares[x][y].ReplaceWall(interiorWalls[mIndexAtPos[x][y]], Square::WallOrientN);
            }
        }
    }

    for (int x = 0; x < width()+1; x++)
    {
        for (int y = 0; y < height(); y++)
        {
            if (x == width() && mIndexAtPos[x - 1][y] >= 0)
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < width() && mIndexAtPos[x][y] < 0 && x > 0 && mIndexAtPos[x - 1][y] != mIndexAtPos[x][y])
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < width() && mIndexAtPos[x][y] >= 0 && (x == 0 || mIndexAtPos[x - 1][y] != mIndexAtPos[x][y]))
            {
                wtype = interiorWalls[mIndexAtPos[x][y]];
                // If already contains a north, put in a west...
                if(squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, BuildingFloor::Square::WallOrientW);
            }

        }
    }

    for (int x = 0; x < width() + 1; x++)
    {
        for (int y = 0; y < height() + 1; y++)
        {
            if (x > 0 && y > 0)
            {
                if (squares[x][y].mTiles[Square::SectionWall]) // if (squares[x][y].walls.count() > 0)
                    continue;
                if (x < width() && mIndexAtPos[x][y - 1] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x][y - 1]];
                else if (y < height() &&  mIndexAtPos[x-1][y] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x - 1][y]];
                else
                    wtype = exteriorWall;
                // Put in the SE piece...
                if ((squares[x][y - 1].IsWallOrient(Square::WallOrientW) ||
                     squares[x][y - 1].IsWallOrient(Square::WallOrientNW)) &&
                        (squares[x - 1][y].IsWallOrient(Square::WallOrientN) ||
                         squares[x - 1][y].IsWallOrient(Square::WallOrientNW)))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientSE);
            }

        }
    }

    for (int x = 0; x < width(); x++)
    {
        for (int y = 0; y < height() + 1; y++)
        {
            if (Door *door = GetDoorAt(x, y)) {
                squares[x][y].ReplaceDoor(door->tile(),
                                          door->getOffset());
                squares[x][y].ReplaceFrame(door->frameTile(),
                                           door->getOffset());
            }

            if (Window *window = GetWindowAt(x, y))
                squares[x][y].ReplaceFrame(window->tile(),
                                           window->getOffset());

            if (Stairs *stairs = GetStairsAt(x, y)) {
                squares[x][y].ReplaceFurniture(stairs->tile(),
                                               stairs->getOffset(x, y));
            }
        }
    }

    // Place floors
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (mIndexAtPos[x][y] >= 0)
                squares[x][y].mTiles[Square::SectionFloor] = floors[mIndexAtPos[x][y]];
        }
    }

    // Nuke floors that have stairs on the floor below.
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BaseMapObject *object, floorBelow->objects()) {
            if (Stairs *stairs = dynamic_cast<Stairs*>(object)) {
                int x = stairs->x(), y = stairs->y();
                if (stairs->dir() == BaseMapObject::W) {
                    if (x + 3 >= width())
                        continue;
                    squares[x+1][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+2][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+3][y].mTiles[Square::SectionFloor] = 0;
                }
                if (stairs->dir() == BaseMapObject::N) {
                    if (y + 3 >= height())
                        continue;
                    squares[x][y+1].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+2].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+3].mTiles[Square::SectionFloor] = 0;
                }
            }
        }
    }
}

bool BuildingFloor::IsTopStairAt(int x, int y)
{
    return squares[x][y].mTiles[Square::SectionFurniture] != 0; // FIXME
}


Door *BuildingFloor::GetDoorAt(int x, int y)
{
    foreach (BaseMapObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = dynamic_cast<Door*>(o))
            return door;
    }
    return 0;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    foreach (BaseMapObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = dynamic_cast<Window*>(o))
            return window;
    }
    return 0;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    foreach (BaseMapObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = dynamic_cast<Stairs*>(o))
            return stairs;
    }
    return 0;
}

void BuildingFloor::SetRoomAt(int x, int y, Room *room)
{
    mRoomAtPos[x][y] = room;
}

Room *BuildingFloor::GetRoomAt(const QPoint &pos)
{
    return mRoomAtPos[pos.x()][pos.y()];
}

int BuildingFloor::width() const
{
    return mBuilding->width();
}

int BuildingFloor::height() const
{
    return mBuilding->height();
}

/////

BuildingFloor::Square::Square()
{
    for (int i = 0; i < MaxSection; i++) {
        mTiles[i] = 0;
        mTileOffset[i] = 0;
    }
}


BuildingFloor::Square::~Square()
{
    // mTiles are owned by BuildingTiles
//    for (int i = 0; i < MaxSection; i++)
//        delete mTiles[i];
}

void BuildingFloor::Square::ReplaceWall(BuildingTile *tile, Square::WallOrientation orient)
{
    mTiles[SectionWall] = tile;
    mWallOrientation = orient;
    mTileOffset[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceDoor(BuildingTile *tile, int offset)
{
    mTiles[SectionDoor] = tile;
    mTileOffset[SectionDoor] = offset;
    mTileOffset[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceFrame(BuildingTile *tile, int offset)
{
    mTiles[SectionFrame] = tile;
    mTileOffset[SectionFrame] = offset;
    mTileOffset[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTile *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mTiles[SectionFurniture] = 0;
        mTileOffset[SectionFurniture] = 0;
        return;
    }
    mTiles[SectionFurniture] = tile;
    mTileOffset[SectionFurniture] = offset;
}

int BuildingFloor::Square::getWallOffset()
{
    BuildingTile *tile = mTiles[SectionWall];
    if (!tile)
        return -1;

    int offset = 0;

    switch (mWallOrientation) {
    case WallOrientN:
        if (mTiles[SectionDoor] != 0)
            offset += 11;
        else if (mTiles[SectionFrame] != 0)
            offset += 9; // window
        else
            offset += 1;
        break;
    case  WallOrientNW:
        offset += 2;
        break;
    case  WallOrientW:
        if (mTiles[SectionDoor] != 0)
            offset += 10;
        else if (mTiles[SectionFrame] != 0)
            offset += 8; // window
        break;
    case  WallOrientSE:
        offset += 3;
        break;
    }

    return offset;
}
