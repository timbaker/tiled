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

    WallType *wtype = 0;

    exteriorWall = l->exteriorWall;
    interiorWalls = l->interiorWalls;
    floors = l->floors;

    // first put back walls in...

    // N first...

    for (int x = 0; x < l->w; x++) {
        for (int y = 0; y < l->h + 1; y++) {
            if (y == l->h && l->grid[x][y - 1] > 0)
                // put N wall here...
                squares[x][y].walls += new WallTile(WallTile::N, exteriorWall);
            else if (y < l->h && l->grid[x][y] < 0 && y > 0 && l->grid[x][y-1] != l->grid[x][y])
                squares[x][y].walls += new WallTile(WallTile::N, exteriorWall);
            else if (y < l->h && (y == 0 || l->grid[x][y-1] != l->grid[x][y]) && l->grid[x][y] >= 0)
                squares[x][y].walls += new WallTile(WallTile::N, interiorWalls[l->grid[x][y]]);
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
                if (squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                    // put W wall here...
                    squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }
            else if (x < l->w && l->grid[x][y] < 0 && x > 0 && l->grid[x - 1][y] != l->grid[x][y])
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                    // put W wall here...
                    squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }
            else if (x < l->w && l->grid[x][y] >= 0 && (x == 0 || l->grid[x - 1][y] != l->grid[x][y]))
            {
                wtype = interiorWalls[l->grid[x][y]];
                // If already contains a north, put in a west...
                if(squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                // put W wall here...
                squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }

        }
    }

    for (int x = 0; x < l->w + 1; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            if (x > 0 && y > 0)
            {
                if (squares[x][y].walls.count() > 0)
                    continue;
                if (x < l->w && l->grid[x][y - 1] >= 0)
                    wtype = interiorWalls[l->grid[x][y - 1]];
                else if (y < l->h &&  l->grid[x-1][y] >= 0)
                    wtype = interiorWalls[l->grid[x - 1][y]];
                else
                    wtype = exteriorWall;
                // Put in the SE piece...
                if ((squares[x][y - 1].Contains(WallTile::W) || squares[x][y - 1].Contains(WallTile::NW)) && (squares[x - 1][y].Contains(WallTile::N) || squares[x - 1][y].Contains(WallTile::NW)))
                    squares[x][y].walls += new WallTile(WallTile::SE, wtype);
            }

        }
    }

    for (int x = 0; x < l->w; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            Door *door = GetDoorAt(x, y);
            if (door != 0)
            {
                if (door->dir == BaseMapObject::N)
                    squares[x][y].ReplaceWithDoor(WallTile::N);
                if (door->dir == BaseMapObject::W)
                    squares[x][y].ReplaceWithDoor(WallTile::W);
            }

            Window *window = GetWindowAt(x, y);
            if (window != 0)
            {
                if (window->dir == BaseMapObject::N)
                    squares[x][y].ReplaceWithWindow(WallTile::N);
                if (window->dir == BaseMapObject::W)
                    squares[x][y].ReplaceWithWindow(WallTile::W);
            }

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
                squares[x][y].floorTile = new FloorTile(floors[l->grid[x][y]]);
        }
    }
}


Door *BuildingFloor::GetDoorAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = dynamic_cast<Door*>(o))
            return door;
    }
    return 0;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = dynamic_cast<Window*>(o))
            return window;
    }
    return 0;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = dynamic_cast<Stairs*>(o))
            return stairs;
    }
    return 0;
}

/////

bool BuildingFloor::Square::Contains(BuildingFloor::WallTile::WallSection sec)
{
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == sec)
            return true;
    }
    return false;
}

void BuildingFloor::Square::Replace(BuildingFloor::WallTile *tile,
                                    BuildingFloor::WallTile::WallSection secToReplace)
{
    int i = 0;
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == secToReplace) {
            walls.replace(i, tile);
            return;
        }
        i++;
    }
}

void BuildingFloor::Square::ReplaceWithDoor(BuildingFloor::WallTile::WallSection direction)
{
    Q_UNUSED(direction)
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == WallTile::N) {
            wallTile->Section = WallTile::NDoor;
            return;
        }
        if (wallTile->Section == WallTile::W) {
            wallTile->Section = WallTile::WDoor;
            return;
        }
    }
}

void BuildingFloor::Square::ReplaceWithWindow(BuildingFloor::WallTile::WallSection direction)
{
    Q_UNUSED(direction)
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == WallTile::N) {
            wallTile->Section = WallTile::NWindow;
            return;
        }
        if (wallTile->Section == WallTile::W) {
            wallTile->Section = WallTile::WWindow;
            return;
        }
    }
}

int BuildingFloor::width() const
{
    return mBuilding->width();
}

int BuildingFloor::height() const
{
    return mBuilding->height();
}
