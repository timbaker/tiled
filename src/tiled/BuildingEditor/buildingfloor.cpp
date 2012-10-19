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
#include "buildingeditorwindow.h"

using namespace BuildingEditor;

BuildingFloor::BuildingFloor(Building *building, int level) :
    mBuilding(building),
    mLayout(new Layout(mBuilding->width(), mBuilding->height())),
    mLevel(level)
{
}

void BuildingFloor::insertObject(int index, BaseMapObject *object)
{
    mObjects.insert(index, object);
}

BaseMapObject *BuildingFloor::removeObject(int index)
{
    return mObjects.takeAt(index);
}

void BuildingFloor::LayoutToSquares()
{
    Layout *l = mLayout;

    int w = l->w + 1;
    int h = l->h + 1;
    // +1 for the outside walls;
    squares.clear();
    squares.resize(w);
    for (int x = 0; x < w; x++)
        squares[x].resize(h);

    BuildingTile *wtype = 0;

#if 1
    exteriorWall = BuildingTiles::instance()->get(QLatin1String("exterior_walls"),
                                                  RoomDefinitionManager::instance->ExteriorWall);
    interiorWalls.clear();
    floors.clear();
    int nRooms = RoomDefinitionManager::instance->getRoomCount();
    for (int i = 0; i < nRooms; i++) {
        interiorWalls += RoomDefinitionManager::instance->getWallForRoom(i);
        floors += RoomDefinitionManager::instance->getFloorForRoom(i);
    }
#else
    exteriorWall = l->exteriorWall;
    interiorWalls = l->interiorWalls;
    floors = l->floors;
#endif

    // first put back walls in...

    // N first...

    for (int x = 0; x < l->w; x++) {
        for (int y = 0; y < l->h + 1; y++) {
            if (y == l->h && l->grid[x][y - 1] > 0) {
                // put N wall here...
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < l->h && l->grid[x][y] < 0 && y > 0 && l->grid[x][y-1] != l->grid[x][y]) {
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < l->h && (y == 0 || l->grid[x][y-1] != l->grid[x][y]) && l->grid[x][y] >= 0) {
                squares[x][y].ReplaceWall(interiorWalls[l->grid[x][y]], Square::WallOrientN);
            }
        }
    }

    for (int x = 0; x < l->w+1; x++)
    {
        for (int y = 0; y < l->h; y++)
        {
            if (x == l->w && l->grid[x - 1][y] >= 0)
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < l->w && l->grid[x][y] < 0 && x > 0 && l->grid[x - 1][y] != l->grid[x][y])
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < l->w && l->grid[x][y] >= 0 && (x == 0 || l->grid[x - 1][y] != l->grid[x][y]))
            {
                wtype = interiorWalls[l->grid[x][y]];
                // If already contains a north, put in a west...
                if(squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, BuildingFloor::Square::WallOrientW);
            }

        }
    }

    for (int x = 0; x < l->w + 1; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            if (x > 0 && y > 0)
            {
//                if (squares[x][y].walls.count() > 0)
//                    continue;
                if (x < l->w && l->grid[x][y - 1] >= 0)
                    wtype = interiorWalls[l->grid[x][y - 1]];
                else if (y < l->h &&  l->grid[x-1][y] >= 0)
                    wtype = interiorWalls[l->grid[x - 1][y]];
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

    for (int x = 0; x < l->w; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            if (Door *door = GetDoorAt(x, y)) {
                squares[x][y].ReplaceDoor(door->mDoorTile);
                squares[x][y].ReplaceFrame(door->mFrameTile);
            }

            if (Window *window = GetWindowAt(x, y))
                squares[x][h].ReplaceFrame(window->mTile);

            Stairs *stairs = GetStairsAt(x, y);
            if (stairs != 0)
            {
                squares[x][y].stairsTexture = stairs->getStairsTexture(x, y);
            }
        }
    }

    for (int x = 0; x < l->w; x++) {
        for (int y = 0; y < l->h; y++) {
            if (l->grid[x][y] >= 0)
                squares[x][y].mTiles[Square::SectionFloor] = floors[l->grid[x][y]];
        }
    }
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
    for (int i = 0; i < MaxSection; i++)
        mTiles[i] = 0;
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
}

void BuildingFloor::Square::ReplaceDoor(BuildingTile *tile)
{
    mTiles[SectionDoor] = tile;
}

void BuildingFloor::Square::ReplaceFrame(BuildingTile *tile)
{
    mTiles[SectionFrame] = tile;
}

int BuildingFloor::Square::getTileIndexForWall()
{
    BuildingTile *tile = mTiles[SectionWall];
    if (!tile)
        return -1;

    int index = tile->mIndex;

    switch (mWallOrientation) {
    case WallOrientN:
        if (mTiles[SectionDoor] != 0)
            index += 11;
        else if (mTiles[SectionFrame] != 0)
            index += 9; // window
        else
            index += 1;
        break;
    case  WallOrientNW:
        index += 2;
        break;
    case  WallOrientW:
        if (mTiles[SectionDoor] != 0)
            index += 10;
        else if (mTiles[SectionFrame] != 0)
            index += 8; // window
        break;
    case  WallOrientSE:
        index += 3;
        break;
    }

    return index;
}
