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
#include "furnituregroups.h"

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
    return isTopFloor() ? 0 : mBuilding->floor(mLevel + 1);
}

BuildingFloor *BuildingFloor::floorBelow() const
{
    return isBottomFloor() ? 0 : mBuilding->floor(mLevel - 1);
}

bool BuildingFloor::isTopFloor() const
{
    return mLevel == mBuilding->floorCount() - 1;
}

bool BuildingFloor::isBottomFloor() const
{
    return mLevel == 0;
}

void BuildingFloor::insertObject(int index, BuildingObject *object)
{
    mObjects.insert(index, object);
}

BuildingObject *BuildingFloor::removeObject(int index)
{
    return mObjects.takeAt(index);
}

BuildingObject *BuildingFloor::objectAt(int x, int y)
{
    foreach (BuildingObject *object, mObjects)
        if (object->bounds().contains(x, y))
            return object;
    return 0;
}

void BuildingFloor::setGrid(const QVector<QVector<Room *> > &grid)
{
    mRoomAtPos = grid;

    mIndexAtPos.resize(mRoomAtPos.size());
    for (int x = 0; x < mIndexAtPos.size(); x++)
        mIndexAtPos[x].resize(mRoomAtPos[x].size());
}

static void ReplaceRoof(RoofObject *ro, const QRect &r,
                        QVector<QVector<BuildingFloor::Square> > &squares,
                        RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    BuildingTile *btile = ro->roofTile(tile);
    for (int x = r.left(); x <= r.right(); x++)
        for (int y = r.top(); y <= r.bottom(); y++)
            squares[x][y].ReplaceRoof(btile);
}

static void ReplaceRoofGap(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile, int offset = 0)
{
    if (r.isEmpty()) return;
    BuildingTile *btile = ro->roofTile(tile);
    for (int x = r.left(); x <= r.right(); x++)
        for (int y = r.top(); y <= r.bottom(); y++)
            squares[x][y].ReplaceRoofCap(btile, offset);
}

static void ReplaceRoofCap(RoofObject *ro, int x, int y,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile, int offset = 0)
{
    BuildingTile *btile = ro->roofTile(tile);
    squares[x][y].ReplaceRoofCap(btile, offset);
}


static void ReplaceRoofTop(RoofObject *ro, const QRect &r,
                        QVector<QVector<BuildingFloor::Square> > &squares)
{
    if (r.isEmpty()) return;
    BuildingTile *btile = ro->roofTile(ro->isN() ? RoofObject::FlatTopN : RoofObject::FlatTopW);
    for (int x = r.left(); x <= r.right(); x++)
        for (int y = r.top(); y <= r.bottom(); y++)
#ifdef ROOF_TOPS
            (ro->depth() == 3)
                ? squares[x][y].ReplaceFloor(btile)
                : squares[x][y].ReplaceRoofTop(btile);
#else
            squares[x][y].ReplaceFloor(btile);
#endif
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
                squares[x][y].ReplaceWall(interiorWalls[mIndexAtPos[x][y]], Square::WallOrientN, false);
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
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW, false);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, BuildingFloor::Square::WallOrientW, false);
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

    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            if (Door *door = GetDoorAt(x, y)) {
                squares[x][y].ReplaceDoor(door->tile(),
                                          door->getOffset());
                squares[x][y].ReplaceFrame(door->frameTile(),
                                           door->getOffset());
            }

            if (Window *window = GetWindowAt(x, y)) {
                squares[x][y].ReplaceFrame(window->tile(),
                                           window->getOffset());

                // Window curtains on exterior walls must be *inside* the
                // room.
                if (squares[x][y].mExterior)
                    squares[x - (window->isW() ? 1 : 0)][y - (window->isN() ? 1 : 0)].ReplaceCurtains(window, true);
                else
                    squares[x][y].ReplaceCurtains(window, false);
            }

            if (Stairs *stairs = GetStairsAt(x, y)) {
                squares[x][y].ReplaceFurniture(stairs->tile(),
                                               stairs->getOffset(x, y));
            }
        }
    }

    foreach (BuildingObject *object, mObjects) {
        if (FurnitureObject *fo = object->asFurniture()) {
            int x = fo->x();
            int y = fo->y();
            FurnitureTile *ftile = fo->furnitureTile();
            if (ftile->mTiles[0])
                squares[x][y].ReplaceFurniture(ftile->mTiles[0]);
            if (ftile->mTiles[1])
                squares[x+1][y].ReplaceFurniture(ftile->mTiles[1]);
            if (ftile->mTiles[2])
                squares[x][y+1].ReplaceFurniture(ftile->mTiles[2]);
            if (ftile->mTiles[3])
                squares[x+1][y+1].ReplaceFurniture(ftile->mTiles[3]);
        }
        if (RoofObject *ro = object->asRoof()) {
            QRect r = ro->bounds();

            QRect se = ro->southEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                // This is a CHEAT! The 1/2 height roof tile is not 'touching
                // the ground' so to speak.
                ReplaceRoof(ro, se.adjusted(1,1,1,1), squares, RoofObject::HalfFlatS);
                break;
            case RoofObject::One:
                ReplaceRoof(ro, se, squares, RoofObject::FlatS1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoof(ro, se.adjusted(0,1,0,0), squares, RoofObject::FlatS1);
                ReplaceRoof(ro, se.adjusted(0,0,0,-1), squares, RoofObject::HalfFlatS);
                break;
            case RoofObject::Two:
                ReplaceRoof(ro, se.adjusted(0,1,0,0), squares, RoofObject::FlatS1);
                ReplaceRoof(ro, se.adjusted(0,0,0,-1), squares, RoofObject::FlatS2);
                break;
            case RoofObject::Three:
                ReplaceRoof(ro, se.adjusted(0,2,0,0), squares, RoofObject::FlatS1);
                ReplaceRoof(ro, se.adjusted(0,1,0,-1), squares, RoofObject::FlatS2);
                ReplaceRoof(ro, se.adjusted(0,0,0,-2), squares, RoofObject::FlatS3);
                break;
            }

            QRect ee = ro->eastEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                // This is a CHEAT! The 1/2 height roof tile is not 'touching
                // the ground' so to speak.
                ReplaceRoof(ro, ee.adjusted(1,1,1,1), squares, RoofObject::HalfFlatE);
                break;
            case RoofObject::One:
                ReplaceRoof(ro, ee, squares, RoofObject::FlatE1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoof(ro, ee.adjusted(1,0,0,0), squares, RoofObject::FlatE1);
                ReplaceRoof(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::HalfFlatE);
                break;
            case RoofObject::Two:
                ReplaceRoof(ro, ee.adjusted(1,0,0,0), squares, RoofObject::FlatE1);
                ReplaceRoof(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::FlatE2);
                break;
            case RoofObject::Three:
                ReplaceRoof(ro, ee.adjusted(2,0,0,0), squares, RoofObject::FlatE1);
                ReplaceRoof(ro, ee.adjusted(1,0,-1,0), squares, RoofObject::FlatE2);
                ReplaceRoof(ro, ee.adjusted(0,0,-2,0), squares, RoofObject::FlatE3);
                break;
            }

            // Corners
            switch (ro->roofType()) {
            case RoofObject::CornerInnerNW:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Inner1));
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Inner2));
                    squares[r.left()+1][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::Inner1));

                    squares[r.left()][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatE2));
                    squares[r.left()+1][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatS2));
                    break;
                case RoofObject::Three:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Inner3));
                    squares[r.left()+1][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::Inner2));
                    squares[r.left()+2][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::Inner1));

                    squares[r.left()][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatE3));
                    squares[r.left()][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::FlatE3));
                    squares[r.left()+1][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::FlatE2));

                    squares[r.left()+1][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatS3));
                    squares[r.left()+2][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatS3));
                    squares[r.left()+2][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatS2));
                    break;
                }
                break;
            case RoofObject::CornerOuterSE:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Outer1));
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Outer2));
                    squares[r.left()+1][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::Outer1));

                    squares[r.left()][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatS1));
                    squares[r.left()+1][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatE1));
                    break;
                case RoofObject::Three:
                    squares[r.left()][r.top()].ReplaceRoof(ro->roofTile(RoofObject::Outer3));
                    squares[r.left()+1][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::Outer2));
                    squares[r.left()+2][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::Outer1));

                    squares[r.left()+2][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatE1));
                    squares[r.left()+2][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatE1));
                    squares[r.left()+1][r.top()].ReplaceRoof(ro->roofTile(RoofObject::FlatE2));

                    squares[r.left()][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::FlatS1));
                    squares[r.left()+1][r.top()+2].ReplaceRoof(ro->roofTile(RoofObject::FlatS1));
                    squares[r.left()][r.top()+1].ReplaceRoof(ro->roofTile(RoofObject::FlatS2));
                    break;
                }
                break;
            }

            QRect eg = ro->eastGap(RoofObject::Three);
            ReplaceRoofGap(ro, eg, squares, RoofObject::CapGapE3);

            QRect sg = ro->southGap(RoofObject::Three);
            ReplaceRoofGap(ro, sg, squares, RoofObject::CapGapS3);

            // SE corner 'pole'
            if (ro->depth() == RoofObject::Three && eg.isValid() && sg.isValid() &&
                    (eg.adjusted(0,0,0,1).bottomLeft() == sg.adjusted(0,0,1,0).topRight()))
                squares[r.right()+1][r.bottom()+1].ReplaceRoofCap(ro->roofTile(RoofObject::CapGapE3), 3);

#ifdef ROOF_TOPS
            if (ro->depth() < 3)
                ReplaceRoofTop(ro, ro->flatTop(), squares);
#endif

            // East cap
            if (ro->isCappedE()) {
                switch (ro->roofType()) {
                case RoofObject::PeakWE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.right()+1, ro->y(), squares, RoofObject::CapMidPt5E);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapMidE);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-2, squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                case RoofObject::SlopeN:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        break;
                    }
                    break;
                case RoofObject::SlopeS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                }
            }

            // South cap
            if (ro->isCappedS()) {
                switch (ro->roofType()) {
                case RoofObject::PeakNS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapMidPt5S);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapMidS);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+3, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        ReplaceRoofCap(ro, r.left()+3, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+4, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+5, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                case RoofObject::SlopeW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        break;
                    }
                    break;
                case RoofObject::SlopeE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                }
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

    // Place flat roof tops above roofs on the floor below
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (RoofObject *ro = object->asRoof()) {
                if (ro->depth() == RoofObject::Three)
                    ReplaceRoofTop(ro, ro->flatTop(), squares);
            }
        }
    }

    // Nuke floors that have stairs on the floor below.
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (Stairs *stairs = object->asStairs()) {
                int x = stairs->x(), y = stairs->y();
                if (stairs->isW()) {
                    if (x + 1 < 0 || x + 3 >= width() || y < 0 || y >= height())
                        continue;
                    squares[x+1][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+2][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+3][y].mTiles[Square::SectionFloor] = 0;
                }
                if (stairs->isN()) {
                    if (x < 0 || x >= width() || y + 1 < 0 || y + 3 >= height())
                        continue;
                    squares[x][y+1].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+2].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+3].mTiles[Square::SectionFloor] = 0;
                }
            }
        }
    }
}

Door *BuildingFloor::GetDoorAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = o->asDoor())
            return door;
    }
    return 0;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = o->asWindow())
            return window;
    }
    return 0;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = o->asStairs())
            return stairs;
    }
    return 0;
}

FurnitureObject *BuildingFloor::GetFurnitureAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (FurnitureObject *fo = o->asFurniture())
            return fo;
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

QRegion BuildingFloor::roomRegion(Room *room)
{
    QRegion region;
    for (int y = 0; y < height(); y++) {
    for (int x = 0; x < width(); x++) {
            if (mRoomAtPos[x][y] == room)
                region |= QRegion(x, y, 1, 1);
        }
    }
    return region;
}

QVector<QVector<Room *> > BuildingFloor::resized(const QSize &newSize) const
{
    QVector<QVector<Room *> > grid = mRoomAtPos;
    grid.resize(newSize.width());
    for (int x = 0; x < newSize.width(); x++)
        grid[x].resize(newSize.height());
    return grid;
}

void BuildingFloor::rotate(bool right)
{
    int oldWidth = mRoomAtPos.size();
    int oldHeight = mRoomAtPos[0].size();

    int newWidth = oldWidth, newHeight = oldHeight;
    qSwap(newWidth, newHeight);

    QVector<QVector<Room*> > roomAtPos;
    roomAtPos.resize(newWidth);
    for (int x = 0; x < newWidth; x++) {
        roomAtPos[x].resize(newHeight);
        for (int y = 0; y < newHeight; y++) {
            roomAtPos[x][y] = 0;
        }
    }

    for (int x = 0; x < oldWidth; x++) {
        for (int y = 0; y < oldHeight; y++) {
            Room *room = mRoomAtPos[x][y];
            if (right)
                roomAtPos[oldHeight - y - 1][x] = room;
            else
                roomAtPos[y][oldWidth - x - 1] = room;
        }
    }

    setGrid(roomAtPos);

    foreach (BuildingObject *object, mObjects)
        object->rotate(right);
}

void BuildingFloor::flip(bool horizontal)
{
    if (horizontal) {
        for (int x = 0; x < width() / 2; x++)
            mRoomAtPos[x].swap(mRoomAtPos[width() - x - 1]);
    } else {
        for (int x = 0; x < width(); x++)
            for (int y = 0; y < height() / 2; y++)
                qSwap(mRoomAtPos[x][y], mRoomAtPos[x][height() - y - 1]);
    }

    foreach (BuildingObject *object, mObjects)
        object->flip(horizontal);
}

/////

BuildingFloor::Square::Square() :
    mExterior(false)
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

void BuildingFloor::Square::ReplaceFloor(BuildingTile *tile)
{
    mTiles[SectionFloor] = tile;
    mTileOffset[SectionFloor] = 0;
}

void BuildingFloor::Square::ReplaceWall(BuildingTile *tile,
                                        Square::WallOrientation orient,
                                        bool exterior)
{
    mTiles[SectionWall] = tile;
    mWallOrientation = orient;
    mTileOffset[SectionWall] = getWallOffset();
    mExterior = exterior;
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

void BuildingFloor::Square::ReplaceCurtains(Window *window, bool exterior)
{
    mTiles[SectionCurtains] = window->curtainsTile();
    int offset = window->isW() ? 0 : 2; // W E N S
    if (exterior)
        ++offset;
    mTileOffset[SectionCurtains] = offset;
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTile *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mTiles[SectionFurniture] = 0;
        mTileOffset[SectionFurniture] = 0;
        return;
    }
    if (mTiles[SectionFurniture]) {
        mTiles[SectionFurniture2] = tile;
        mTileOffset[SectionFurniture2] = offset;
        return;
    }
    mTiles[SectionFurniture] = tile;
    mTileOffset[SectionFurniture] = offset;
}

void BuildingFloor::Square::ReplaceRoof(BuildingTile *tile, int offset)
{
    mTiles[SectionRoof] = tile;
    mTileOffset[SectionRoof] = offset;
}

void BuildingFloor::Square::ReplaceRoofCap(BuildingTile *tile, int offset)
{
    if (mTiles[SectionRoofCap]) {
        mTiles[SectionRoofCap2] = tile;
        mTileOffset[SectionRoofCap2] = offset;
        return;
    }
    mTiles[SectionRoofCap] = tile;
    mTileOffset[SectionRoofCap] = offset;
}

#ifdef ROOF_TOPS
void BuildingFloor::Square::ReplaceRoofTop(BuildingTile *tile)
{
    mTiles[SectionRoofTop] = tile;
    mTileOffset[SectionRoofTop] = 0;
}
#endif

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
