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
#include "roofhiding.h"

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QVector<QVector<int> >;
#endif

using namespace BuildingEditor;

/////

FloorTileGrid::FloorTileGrid(int width, int height) :
    mWidth(width),
    mHeight(height),
    mCount(0),
    mUseVector(false)
{
}

const QString &FloorTileGrid::at(int index) const
{
    if (mUseVector)
        return mCellsVector[index];
    QHash<int,QString>::const_iterator it = mCells.find(index);
    if (it != mCells.end())
        return *it;
    return mEmptyCell;
}

void FloorTileGrid::replace(int index, const QString &tile)
{
    if (mUseVector) {
        if (!mCellsVector[index].isEmpty() && tile.isEmpty()) mCount--;
        if (mCellsVector[index].isEmpty() && !tile.isEmpty()) mCount++;
        mCellsVector[index] = tile;
        return;
    }
    QHash<int,QString>::iterator it = mCells.find(index);
    if (it == mCells.end()) {
        if (tile.isEmpty())
            return;
        mCells.insert(index, tile);
        mCount++;
    } else if (!tile.isEmpty()) {
        (*it) = tile;
        mCount++;
    } else {
        mCells.erase(it);
        mCount--;
    }
    if (mCells.size() > 300 * 300 / 3)
        swapToVector();
}

void FloorTileGrid::replace(int x, int y, const QString &tile)
{
    Q_ASSERT(contains(x, y));
    replace(y * mWidth + x, tile);
}

bool FloorTileGrid::replace(const QString &tile)
{
    bool changed = false;
    for (int x = 0; x < mWidth; x++) {
        for (int y = 0; y < mHeight; y++) {
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRegion &rgn, const QString &tile)
{
    bool changed = false;
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                if (at(x, y) != tile) {
                    replace(x, y, tile);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRegion &rgn, const QPoint &p,
                            const FloorTileGrid *other)
{
    Q_ASSERT(other->bounds().translated(p).contains(rgn.boundingRect()));
    bool changed = false;
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds();
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                QString tile = other->at(x - p.x(), y - p.y());
                if (at(x, y) != tile) {
                    replace(x, y, tile);
                    changed = true;
                }
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QRect &r, const QString &tile)
{
    bool changed = false;
    for (int x = r.left(); x <= r.right(); x++) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

bool FloorTileGrid::replace(const QPoint &p, const FloorTileGrid *other)
{
    const QRect r = other->bounds().translated(p) & bounds();
    bool changed = false;
    for (int x = r.left(); x <= r.right(); x++) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            QString tile = other->at(x - p.x(), y - p.y());
            if (at(x, y) != tile) {
                replace(x, y, tile);
                changed = true;
            }
        }
    }
    return changed;
}

void FloorTileGrid::clear()
{
    if (mUseVector)
        mCellsVector.fill(mEmptyCell);
    else
        mCells.clear();
    mCount = 0;
}

FloorTileGrid *FloorTileGrid::clone()
{
    return new FloorTileGrid(*this);
}

FloorTileGrid *FloorTileGrid::clone(const QRect &r)
{
    FloorTileGrid *klone = new FloorTileGrid(r.width(), r.height());
    const QRect r2 = r & bounds();
    for (int x = r2.left(); x <= r2.right(); x++) {
        for (int y = r2.top(); y <= r2.bottom(); y++) {
            klone->replace(x - r.x(), y - r.y(), at(x, y));
        }
    }
    return klone;
}

FloorTileGrid *FloorTileGrid::clone(const QRect &r, const QRegion &rgn)
{
    FloorTileGrid *klone = new FloorTileGrid(r.width(), r.height());
    foreach (QRect r2, rgn.rects()) {
        r2 &= bounds() & r;
        for (int x = r2.left(); x <= r2.right(); x++) {
            for (int y = r2.top(); y <= r2.bottom(); y++) {
                klone->replace(x - r.x(), y - r.y(), at(x, y));
            }
        }
    }
    return klone;
}

void FloorTileGrid::swapToVector()
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

/////

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
            mRoomAtPos[x][y] = nullptr;
        }
    }
}

BuildingFloor::~BuildingFloor()
{
    qDeleteAll(mObjects);
}

BuildingFloor *BuildingFloor::floorAbove() const
{
    return isTopFloor() ? nullptr : mBuilding->floor(mLevel + 1);
}

BuildingFloor *BuildingFloor::floorBelow() const
{
    return isBottomFloor() ? nullptr : mBuilding->floor(mLevel - 1);
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
    BuildingObject *object = mObjects.takeAt(index);
    if (RoofObject *ro = object->asRoof())
        mFlatRoofsWithDepthThree.removeAll(ro);
    if (Stairs *stairs = object->asStairs())
        mStairs.removeAll(stairs);
    return object;
}

BuildingObject *BuildingFloor::objectAt(int x, int y)
{
    foreach (BuildingObject *object, mObjects)
        if (object->bounds().contains(x, y))
            return object;
    return nullptr;
}

void BuildingFloor::setGrid(const QVector<QVector<Room *> > &grid)
{
    mRoomAtPos = grid;

    mIndexAtPos.resize(mRoomAtPos.size());
    for (int x = 0; x < mIndexAtPos.size(); x++) {
        mIndexAtPos[x].resize(mRoomAtPos[x].size());
    }
}

static void ReplaceRoofSlope(RoofObject *ro, const QRect &r,
                             QVector<QVector<BuildingSquare> > &squares,
                             RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->slopeTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++) {
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++) {
            squares[x][y].ReplaceRoof(ro->slopeTiles(), offset);
        }
    }
}

static void ReplaceRoofSlope(RoofObject *ro, const QRect &r,
                           const QVector<RoofObject::RoofTile> &tiles,
                           QVector<QVector<BuildingSquare> > &squares)
{
    if (tiles.isEmpty()) return;
    for (int y = r.top(); y <= r.bottom(); y++) {
        for (int x = r.left(); x <= r.right(); x++) {
            ReplaceRoofSlope(ro, QRect(x, y, 1, 1), squares, tiles.at(x - r.left() + (y - r.top()) * r.width()));
        }
    }
}

static void ReplaceRoofGap(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingSquare> > &squares,
                           RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++) {
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++) {
            squares[x][y].ReplaceRoofCap(ro->capTiles(), offset);
        }
    }
}

static void ReplaceRoofCap(RoofObject *ro, int x, int y,
                           QVector<QVector<BuildingSquare> > &squares,
                           RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p)) {
        squares[p.x()][p.y()].ReplaceRoofCap(ro->capTiles(), offset);
    }
}

static void ReplaceRoofCap(RoofObject *ro, const QRect &r,
                           const QVector<RoofObject::RoofTile> &tiles,
                           QVector<QVector<BuildingSquare> > &squares)
{
    if (tiles.isEmpty()) return;
    for (int y = r.top(); y <= r.bottom(); y++) {
        for (int x = r.left(); x <= r.right(); x++) {
            ReplaceRoofCap(ro, x, y, squares, tiles.at(x - r.left() + (y - r.top()) * r.width()));
        }
    }
}

static void ReplaceRoofTop(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingSquare> > &squares)
{
    if (r.isEmpty()) return;
    int offset = 0;
    if (ro->depth() == RoofObject::Zero)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    else if (ro->depth() == RoofObject::One)
        offset = ro->isN() ? BTC_RoofTops::North1 : BTC_RoofTops::West1;
    else if (ro->depth() == RoofObject::Two)
        offset = ro->isN() ? BTC_RoofTops::North2 : BTC_RoofTops::West2;
    else if (ro->depth() == RoofObject::Three)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    QPoint tileOffset = ro->topTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++) {
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++) {
            (ro->depth() == RoofObject::Zero || ro->depth() == RoofObject::Three)
                ? squares[x][y].ReplaceFloor(ro->topTiles(), offset)
                : squares[x][y].ReplaceRoofTop(ro->topTiles(), offset);
        }
    }
}

static void ReplaceRoofCorner(RoofObject *ro, int x, int y,
                              QVector<QVector<BuildingSquare> > &squares,
                              RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->slopeTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p)) {
        squares[p.x()][p.y()].ReplaceRoof(ro->slopeTiles(), offset);
    }
}

static void ReplaceRoofCorner(RoofObject *ro, const QRect &r,
                              const QVector<RoofObject::RoofTile> &tiles,
                              QVector<QVector<BuildingSquare> > &squares)
{
    if (tiles.isEmpty()) return;
    for (int y = r.top(); y <= r.bottom(); y++) {
        for (int x = r.left(); x <= r.right(); x++) {
            RoofObject::RoofTile tile = tiles.at(x - r.left() + (y - r.top()) * r.width());
            if (tile != RoofObject::TileCount)
                ReplaceRoofCorner(ro, x, y, squares, tile);
        }
    }
}

static void ReplaceFurniture(int x, int y,
                             QVector<QVector<BuildingSquare> > &squares,
                             BuildingTile *btile,
                             BuildingSquare::SquareSection sectionMin,
                             BuildingSquare::SquareSection sectionMax,
                             int dw = 0, int dh = 0)
{
    if (!btile)
        return;
    Q_ASSERT(dw <= 1 && dh <= 1);
    QRect bounds(0, 0, squares.size() - 1 + dw, squares[0].size() - 1 + dh);
    if (bounds.contains(x, y)) {
        squares[x][y].ReplaceFurniture(btile, sectionMin, sectionMax);
    }
}

static void ReplaceDoor(Door *door, QVector<QVector<BuildingSquare> > &squares)
{
    int x = door->x(), y = door->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (bounds.contains(x, y)) {
        squares[x][y].ReplaceDoor(door->tile(),
                                  door->isW() ? BuildingSquare::SquareSection::SectionDoorW
                                              : BuildingSquare::SquareSection::SectionDoorN,
                                  door->isW() ? BTC_Doors::West
                                              : BTC_Doors::North);
        squares[x][y].ReplaceFrame(door->frameTile(),
                                   door->isW() ? BTC_DoorFrames::West
                                               : BTC_DoorFrames::North);
        if (door->isN() && (y > 0)) {
            BuildingSquare& squareN = squares[x][y - 1];
            if (squareN.mWallS.type != BuildingSquare::WallType::Invalid) {
                squareN.mEntryEnum[BuildingSquare::SquareSection::SectionWallS] = squareN.getWallOffset(squareN.mWallS, BuildingSquare::SquareSection::SectionWallS);
            }
        }
        if (door->isW() && (x > 0)) {
            BuildingSquare& squareW = squares[x - 1][y];
            if (squareW.mWallE.type != BuildingSquare::WallType::Invalid) {
                squareW.mEntryEnum[BuildingSquare::SquareSection::SectionWallE] = squareW.getWallOffset(squareW.mWallE, BuildingSquare::SquareSection::SectionWallE);
            }
        }
    }
}

static void ReplaceWindow(Window *window, QVector<QVector<BuildingSquare> > &squares)
{
    int x = window->x(), y = window->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (bounds.contains(x, y)) {
        squares[x][y].ReplaceWindow(window->tile(),
                                    window->isW() ? BuildingSquare::SquareSection::SectionWindowW
                                                : BuildingSquare::SquareSection::SectionWindowN,
                                    window->isW() ? BTC_Windows::West
                                                  : BTC_Windows::North);

        // Window curtains on exterior walls must be *inside* the
        // room.
        if (squares[x][y].mExterior) {
            int dx = window->isW() ? 1 : 0;
            int dy = window->isN() ? 1 : 0;
            if ((x - dx >= 0) && (y - dy >= 0)) {
                squares[x - dx][y - dy].ReplaceCurtains(window,
                                                        dx ? BuildingSquare::SquareSection::SectionCurtainsE
                                                           : BuildingSquare::SquareSection::SectionCurtainsS);
            }
        } else {
            squares[x][y].ReplaceCurtains(window, window->isW()
                                          ? BuildingSquare::SquareSection::SectionCurtainsW
                                          : BuildingSquare::SquareSection::SectionCurtainsN);
        }

        if (squares[x][y].mExterior) {
            if (window->isN()) {
                if (x > 0)
                    squares[x-1][y].ReplaceShutters(window, true);
                squares[x][y].ReplaceShutters(window, true);
                squares[x][y].ReplaceShutters(window, false);
                if (x < bounds.right())
                    squares[x + 1][y].ReplaceShutters(window, false);
            } else {
                if (y > 0)
                    squares[x][y - 1].ReplaceShutters(window, true);
                squares[x][y].ReplaceShutters(window, true);
                squares[x][y].ReplaceShutters(window, false);
                if (y < bounds.bottom())
                    squares[x][y + 1].ReplaceShutters(window, false);
            }
        } else {

        }

        if (window->isN() && (y > 0)) {
            BuildingSquare& squareN = squares[x][y - 1];
            if (squareN.mWallS.type != BuildingSquare::WallType::Invalid) {
                squareN.mEntryEnum[BuildingSquare::SquareSection::SectionWallS] = squareN.getWallOffset(squareN.mWallS, BuildingSquare::SquareSection::SectionWallS);
            }
        }
        if (window->isW() && (x > 0)) {
            BuildingSquare& squareW = squares[x - 1][y];
            if (squareW.mWallE.type != BuildingSquare::WallType::Invalid) {
                squareW.mEntryEnum[BuildingSquare::SquareSection::SectionWallE] = squareW.getWallOffset(squareW.mWallE, BuildingSquare::SquareSection::SectionWallE);
            }
        }
    }
}

void BuildingFloor::LayoutToSquares()
{
    // +1 for the outside walls;
    int w = width() + 1;
    int h = height() + 1;

    static const BuildingSquare EMPTY_SQUARE;

    squares.resize(w);
    for (int x = 0; x < w; x++) {
        squares[x].fill(EMPTY_SQUARE, h);
    }

    BuildingTileEntry *wtype = nullptr;

    BuildingTileEntry *exteriorWall = mBuilding->exteriorWall();
    BuildingTileEntry *exteriorWallTrim = level() ? nullptr : mBuilding->exteriorWallTrim();
    QVector<BuildingTileEntry*> interiorWalls;
    QVector<BuildingTileEntry*> interiorWallTrim;
    QVector<BuildingTileEntry*> floors;

    for (Room *room : mBuilding->rooms()) {
        interiorWalls += room->tile(Room::InteriorWall);
        interiorWallTrim += room->tile(Room::InteriorWallTrim);
        floors += room->tile(Room::Floor);
    }

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            Room *room = mRoomAtPos[x][y];
            if (room != nullptr && RoofHiding::isEmptyOutside(room->Name))
                room = nullptr;
            mIndexAtPos[x][y] = room ? mBuilding->indexOf(room) : -1;
            squares[x][y].mExterior = room == nullptr;
        }
    }

    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            // Place N walls...
            if (x < width()) {
                if (y == height() && mIndexAtPos[x][y - 1] >= 0) {
                    squares[x][y].SetWallN(exteriorWall);
                    squares[x][y].SetWallTrimN(exteriorWallTrim);
                } else if (y < height() && mIndexAtPos[x][y] < 0 && y > 0 &&
                           mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallN(exteriorWall);
                    squares[x][y].SetWallTrimN(exteriorWallTrim);
                    squares[x][y-1].SetWallS(interiorWalls[mIndexAtPos[x][y-1]]);
                } else if (y < height() && (y == 0 || mIndexAtPos[x][y-1] != mIndexAtPos[x][y])
                           && mIndexAtPos[x][y] >= 0) {
                    squares[x][y].SetWallN(interiorWalls[mIndexAtPos[x][y]]);
                    squares[x][y].SetWallTrimN(interiorWallTrim[mIndexAtPos[x][y]]);
                    if ((y > 0) && (mIndexAtPos[x][y-1] == -1)) {
                        squares[x][y-1].SetWallS(exteriorWall);
                    } else if (y > 0) {
                        squares[x][y-1].SetWallS(interiorWalls[mIndexAtPos[x][y-1]]);
                    }
                }
                // TODO: south walls at y=-1
            }
            // Place W walls...
            if (y < height()) {
                if (x == width() && mIndexAtPos[x - 1][y] >= 0) {
                    squares[x][y].SetWallW(exteriorWall);
                    squares[x][y].SetWallTrimW(exteriorWallTrim);
                } else if (x < width() && mIndexAtPos[x][y] < 0 && x > 0
                           && mIndexAtPos[x - 1][y] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallW(exteriorWall);
                    squares[x][y].SetWallTrimW(exteriorWallTrim);
                    squares[x-1][y].SetWallE(interiorWalls[mIndexAtPos[x-1][y]]);
                } else if (x < width() && mIndexAtPos[x][y] >= 0 &&
                           (x == 0 || mIndexAtPos[x - 1][y] != mIndexAtPos[x][y])) {
                    squares[x][y].SetWallW(interiorWalls[mIndexAtPos[x][y]]);
                    squares[x][y].SetWallTrimW(interiorWallTrim[mIndexAtPos[x][y]]);
                    if ((x > 0) && (mIndexAtPos[x-1][y] == -1)) {
                        squares[x-1][y].SetWallE(exteriorWall);
                    } else if (x > 0) {
                        squares[x-1][y].SetWallE(interiorWalls[mIndexAtPos[x-1][y]]);
                    }
                }
                // TODO: east walls at x=-1
            }
        }
    }

    // Handle WallObjects.
    for (BuildingObject *object : mObjects) {
        if (WallObject *wall = object->asWall()) {
            int x = wall->x(), y = wall->y();
            if (wall->isN()) {
                QRect r = wall->bounds() & bounds(1, 0);
                for (y = r.top(); y <= r.bottom(); y++) {
                    squares[x][y].SetWallW(wall->tile(squares[x][y].mExterior
                                                      ? WallObject::TileExterior
                                                      : WallObject::TileInterior));
                    squares[x][y].SetWallTrimW(wall->tile(squares[x][y].mExterior
                                                          ? WallObject::TileExteriorTrim
                                                          : WallObject::TileInteriorTrim));
                }
            } else {
                QRect r = wall->bounds() & bounds(0, 1);
                for (x = r.left(); x <= r.right(); x++) {
                    squares[x][y].SetWallN(wall->tile(squares[x][y].mExterior
                                                      ? WallObject::TileExterior
                                                      : WallObject::TileInterior));
                    squares[x][y].SetWallTrimN(wall->tile(squares[x][y].mExterior
                                                          ? WallObject::TileExteriorTrim
                                                          : WallObject::TileInteriorTrim));
                }
            }
        }
    }

    // Furniture in the Walls layer replaces wall entries with tiles.
    QList<FurnitureObject*> wallReplacement;
    for (BuildingObject *object : mObjects) {
        if (FurnitureObject *fo = object->asFurniture()) {
            FurnitureTile *ftile = fo->furnitureTile()->resolved();
            if (ftile->owner()->layer() == FurnitureTiles::LayerWalls) {
                wallReplacement += fo;

                int x = fo->x(), y = fo->y();
                int dx = 0, dy = 0;
                bool killW = false, killN = false;
                switch (fo->furnitureTile()->orient()) {
                case FurnitureTile::FurnitureW:
                    killW = true;
                    break;
                case FurnitureTile::FurnitureE:
                    killW = true;
                    dx = 1;
                    break;
                case FurnitureTile::FurnitureN:
                    killN = true;
                    break;
                case FurnitureTile::FurnitureS:
                    killN = true;
                    dy = 1;
                    break;
                default:
                    break;
                }
                // TODO: setWallE() and setWallS() on opposite squares
                for (int i = 0; i < ftile->size().height(); i++) {
                    for (int j = 0; j < ftile->size().width(); j++) {
                        int sx = x + j + dx, sy = y + i + dy;
                        if (bounds(1, 1).contains(sx, sy)) {
                            BuildingSquare &sq = squares[sx][sy];
                            if (killW)
                                sq.SetWallW(fo->furnitureTile(), ftile->tile(j, i));
                            if (killN)
                                sq.SetWallN(fo->furnitureTile(), ftile->tile(j, i));
                        }
                    }
                }
            }
        }
    }

    // Determine if wall sprites on each square should be door frames, window frames, 2-wall corners, or walls.
    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare &s = squares[x][y];
            BuildingTileEntry *wallN = s.mWallN.entry;
            BuildingTileEntry *wallW = s.mWallW.entry;
            if (wallN || wallW) {
                if (!wallN) wallN = BuildingTilesMgr::instance()->noneTileEntry();
                if (!wallW) wallW = BuildingTilesMgr::instance()->noneTileEntry();
                if (wallN == wallW) { // may be "none"
                    s.mWallN.type = BuildingSquare::WallType::TwoWallCorner;
                    s.ReplaceWall(wallN, BuildingSquare::SectionWallN, s.mWallN);
                } else if (wallW->isNone()) {
                    s.mWallN.type = BuildingSquare::WallType::Simple;
                    s.ReplaceWall(wallN, BuildingSquare::SectionWallN, s.mWallN);
                } else if (wallN->isNone()) {
                    s.mWallW.type = BuildingSquare::WallType::Simple;
                    s.ReplaceWall(wallW, BuildingSquare::SectionWallW, s.mWallW);
                } else {
                    // Different non-none tiles.
                    s.mWallN.type = BuildingSquare::WallType::Simple;
                    s.ReplaceWall(wallN, BuildingSquare::SectionWallN, s.mWallN);
                    s.mWallW.type = BuildingSquare::WallType::Simple;
                    s.ReplaceWall(wallW, BuildingSquare::SectionWallW, s.mWallW);
//                    s.mWallOrientation = Square::WallOrientNW;
                }
            }
            BuildingTileEntry *wallS = s.mWallS.entry;
            BuildingTileEntry *wallE = s.mWallE.entry;
            if (wallS) {
                s.mWallS.type = BuildingSquare::WallType::Simple;
                s.ReplaceWall(wallS, BuildingSquare::SectionWallS, s.mWallS);
            }
            if (wallE) {
                s.mWallE.type = BuildingSquare::WallType::Simple;
                s.ReplaceWall(wallE, BuildingSquare::SectionWallE, s.mWallE);
            }
        }
    }

    // Place wall pillars at the corners of two perpendicular walls.
    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare &sq = squares[x][y];
            if ((sq.mEntries[BuildingSquare::SectionWallN] && !sq.mEntries[BuildingSquare::SectionWallN]->isNone()) ||
                    (sq.mEntries[BuildingSquare::SectionWallW] && !sq.mEntries[BuildingSquare::SectionWallW]->isNone()) ||
                    (sq.mWallW.furniture) || (sq.mWallN.furniture)) {
                continue;
            }
            // Put in the SE piece...
            if ((x > 0) && (y > 0) &&
                    squares[x][y - 1].HasWallW() &&
                    squares[x - 1][y].HasWallN()) {
                // With WallObjects, there could be 2 different tiles meeting
                // at this SE corner.
                wtype = squares[x][y - 1].mWallW.entry;
                sq.mWallW.type = BuildingSquare::WallType::Pillar;
                sq.ReplaceWall(wtype, BuildingSquare::SectionWallW, sq.mWallW);
            }
            // South end of a north-south wall.
            else if ((y > 0) &&
                     squares[x][y - 1].HasWallW()) {
                wtype = squares[x][y - 1].mWallW.entry;
                sq.mWallW.type = BuildingSquare::WallType::Pillar;
                sq.mEntries[BuildingSquare::SectionWallW] = wtype;
                sq.mEntryEnum[BuildingSquare::SectionWallW] = BTC_Walls::SouthEast;
//                sq.mWallOrientation = Square::WallOrientInvalid;
            }
            // East end of a west-east wall.
            else if ((x > 0) &&
                     squares[x - 1][y].HasWallN()) {
                wtype = squares[x - 1][y].mWallN.entry;
                sq.mWallN.type = BuildingSquare::WallType::Pillar;
                sq.mEntries[BuildingSquare::SectionWallN] = wtype;
                sq.mEntryEnum[BuildingSquare::SectionWallN] = BTC_Walls::SouthEast;
//                sq.mWallOrientation = Square::WallOrientInvalid;
            }
        }
    }

    mFlatRoofsWithDepthThree.clear();
    mStairs.clear();

    for (BuildingObject *object : mObjects) {
        int x = object->x();
        int y = object->y();
        if (Door *door = object->asDoor()) {
            ReplaceDoor(door, squares);
        }
        if (Window *window = object->asWindow()) {
            ReplaceWindow(window, squares);
        }
        if (Stairs *stairs = object->asStairs()) {
            // Stair objects are 5 tiles long but only have 3 tiles.
            if (stairs->isN()) {
                for (int i = 1; i <= 3; i++)
                    ReplaceFurniture(x, y + i, squares,
                                     stairs->tile()->tile(stairs->getOffset(x, y + i)),
                                     BuildingSquare::SectionFurniture,
                                     BuildingSquare::SectionFurniture4);
            } else {
                for (int i = 1; i <= 3; i++)
                    ReplaceFurniture(x + i, y, squares,
                                     stairs->tile()->tile(stairs->getOffset(x + i, y)),
                                     BuildingSquare::SectionFurniture,
                                     BuildingSquare::SectionFurniture4);
            }
            mStairs += stairs;
        }
        if (FurnitureObject *fo = object->asFurniture()) {
            FurnitureTile *ftile = fo->furnitureTile()->resolved();
            if (ftile->owner()->layer() == FurnitureTiles::LayerWalls) {
                continue;
            }
            for (int i = 0; i < ftile->size().height(); i++) {
                for (int j = 0; j < ftile->size().width(); j++) {
                    switch (ftile->owner()->layer()) {
                    case FurnitureTiles::LayerWalls:
                        // Handled after all the door/window objects
                        break;
                    case FurnitureTiles::LayerRoofCap: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         BuildingSquare::SectionRoofCap,
                                         BuildingSquare::SectionRoofCap2,
                                         dx, dy);
                        break;
                    }
                    case FurnitureTiles::LayerWallOverlay: {
                        BuildingSquare::SquareSection sectionMin;
                        BuildingSquare::SquareSection sectionMax;
                        switch (ftile->orient()) {
                        case FurnitureTile::FurnitureOrientation::FurnitureW:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlayW;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureN:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlayN;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureE:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlayE;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureS:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlayS;
                            sectionMax = sectionMin;
                            break;
                        default:
                            sectionMin = BuildingSquare::SquareSection::SectionInvalid;
                            sectionMax = BuildingSquare::SquareSection::SectionInvalid;
                            break;
                        }
                        // FIXME: possibly NW/NE/SE/SW ???
                        if (sectionMin == BuildingSquare::SquareSection::SectionInvalid) {
                            break;
                        }
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         sectionMin,
                                         sectionMax);
                        break;
                    }
                    case FurnitureTiles::LayerWallFurniture: {
                        BuildingSquare::SquareSection sectionMin;
                        BuildingSquare::SquareSection sectionMax;
                        switch (ftile->orient()) {
                        case FurnitureTile::FurnitureOrientation::FurnitureW:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurnitureW;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureN:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurnitureN;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureE:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurnitureE;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureS:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurnitureS;
                            sectionMax = sectionMin;
                            break;
                        default:
                            sectionMin = BuildingSquare::SquareSection::SectionInvalid;
                            sectionMax = BuildingSquare::SquareSection::SectionInvalid;
                            break;
                        }
                        // FIXME: possibly NW/NE/SE/SW ???
                        if (sectionMin == BuildingSquare::SquareSection::SectionInvalid) {
                            break;
                        }
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         sectionMin,
                                         sectionMax);
                        break;
                    }
                    case FurnitureTiles::LayerFrames: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         BuildingSquare::SectionFrame,
                                         BuildingSquare::SectionFrame,
                                         dx, dy);
                        break;
                    }
                    case FurnitureTiles::LayerDoors: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         fo->furnitureTile()->isN() ? BuildingSquare::SectionDoorN : BuildingSquare::SectionDoorW,
                                         fo->furnitureTile()->isN() ? BuildingSquare::SectionDoorN : BuildingSquare::SectionDoorW,
                                         dx, dy);
                        break;
                    }
                    case FurnitureTiles::LayerFurniture:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         BuildingSquare::SectionFurniture,
                                         BuildingSquare::SectionFurniture4);
                        break;
                    case FurnitureTiles::LayerRoof:
                        ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i),
                                         BuildingSquare::SectionRoof,
                                         BuildingSquare::SectionRoof2);
                        break;
                    default:
                        Q_ASSERT(false);
                        break;
                    }
                }
            }
        }
        if (RoofObject *ro = object->asRoof()) {
            QRect tileRect;
            QVector<RoofObject::RoofTile> tiles;

            tiles = ro->slopeTiles(tileRect);
            ReplaceRoofSlope(ro, tileRect, tiles, squares);

            tiles = ro->westCapTiles(tileRect);
            ReplaceRoofCap(ro, tileRect, tiles, squares);

            tiles = ro->eastCapTiles(tileRect);
            ReplaceRoofCap(ro, tileRect, tiles, squares);

            tiles = ro->northCapTiles(tileRect);
            ReplaceRoofCap(ro, tileRect, tiles, squares);

            tiles = ro->southCapTiles(tileRect);
            ReplaceRoofCap(ro, tileRect, tiles, squares);

            tiles = ro->cornerTiles(tileRect);
            ReplaceRoofCorner(ro, tileRect, tiles, squares);

            // Roof tops with depth of 3 are placed in the floor layer of the
            // floor above.
            if (ro->depth() != RoofObject::Three)
                ReplaceRoofTop(ro, ro->flatTop(), squares);
            else if (!ro->flatTop().isEmpty())
                mFlatRoofsWithDepthThree += ro;
        }
    }

    // Pick a wall variant based on the presence of doors and windows,
    // and merge identical north and west walls into north-west corners.
    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare& square = squares[x][y];
            BuildingTileEntry *entryN = square.mWallN.entry;
            BuildingTileEntry *entryW = square.mWallW.entry;
            if (entryN != nullptr && entryN->isNone())
                entryN = nullptr;
            if (entryW != nullptr && entryW->isNone())
                entryW = nullptr;
            const int SectionWallN = BuildingSquare::SquareSection::SectionWallN;
            const int SectionWallW = BuildingSquare::SquareSection::SectionWallW;
            if (entryN != nullptr) {
                BuildingSquare *squareN = (y > 0) ? &squares[x][y - 1] : nullptr;
                if (square.mWallN.type == BuildingSquare::WallType::Pillar) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::SouthEast;
                } else if (square.HasDoorN() || (squareN != nullptr && squareN->HasDoorS())) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::NorthDoor;
                } else if (square.HasWindowN() || (squareN != nullptr && squareN->HasWindowS())) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::NorthWindow;
                } else {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::North;
                }
            }
            if (entryW != nullptr) {
                BuildingSquare *squareW = (x > 0) ? &squares[x- 1][y ] : nullptr;
                if (square.mWallW.type == BuildingSquare::WallType::Pillar) {
                    square.mEntryEnum[SectionWallW] = BTC_Walls::SouthEast;
                } else if (square.HasDoorW() || (squareW != nullptr && squareW->HasDoorE())) {
                    square.mEntryEnum[SectionWallW] = BTC_Walls::WestDoor;
                } else if (square.HasWindowW() || (squareW != nullptr && squareW->HasWindowE())) {
                    square.mEntryEnum[SectionWallW] = BTC_Walls::WestWindow;
                } else {
                    square.mEntryEnum[SectionWallW] = BTC_Walls::West;
                }
            }
            if (entryN != nullptr && entryW != nullptr && entryN == entryW &&
                    square.mEntryEnum[SectionWallN] == BTC_Walls::North &&
                    square.mEntryEnum[SectionWallW] == BTC_Walls::West) {
                square.mEntryEnum[SectionWallN] = BTC_Walls::NorthWest;
                square.mEntries[SectionWallW] = nullptr;
                square.mEntryEnum[SectionWallW] = 0;
            }

            BuildingTileEntry *entryS = square.mWallS.entry;
            BuildingTileEntry *entryE = square.mWallE.entry;
            if (entryS != nullptr && entryS->isNone())
                entryS = nullptr;
            if (entryE != nullptr && entryE->isNone())
                entryE = nullptr;
            const int SectionWallS = BuildingSquare::SquareSection::SectionWallS;
            const int SectionWallE = BuildingSquare::SquareSection::SectionWallE;
            if (entryS != nullptr) {
                BuildingSquare *squareS = (y < height() + 1) ? &squares[x][y + 1] : nullptr;
                if (square.mWallS.type == BuildingSquare::WallType::Pillar) {
                    //
                } else if (square.HasDoorS() || (squareS != nullptr && squareS->HasDoorN())) {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::NorthDoor;
                } else if (square.HasWindowN() || (squareS != nullptr && squareS->HasWindowN())) {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::NorthWindow;
                } else {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::North;
                }
            }
            if (entryE != nullptr) {
                BuildingSquare *squareE = (x < width() + 1) ? &squares[x + 1][y] : nullptr;
                if (square.mWallE.type == BuildingSquare::WallType::Pillar) {
                    //
                } else if (square.HasDoorE() || (squareE != nullptr && squareE->HasDoorW())) {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::WestDoor;
                } else if (square.HasWindowE() || (squareE != nullptr && squareE->HasWindowW())) {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::WestWindow;
                } else {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::West;
                }
            }
        }
    }

    for (FurnitureObject *fo : wallReplacement) {
        FurnitureTile *ftile = fo->furnitureTile()->resolved();
        int x = fo->x(), y = fo->y();
        int dx = 0, dy = 0;
        BuildingSquare::SquareSection section;
        switch (fo->furnitureTile()->orient()) {
        case FurnitureTile::FurnitureW:
            section = BuildingSquare::SectionWallW;
            break;
        case FurnitureTile::FurnitureE:
            // FIXME: we didn't have E wall sprites before; this is going on the square to the east.
            section = BuildingSquare::SectionWallW;
            dx = 1;
            break;
        case FurnitureTile::FurnitureN:
            section = BuildingSquare::SectionWallN;
            break;
        case FurnitureTile::FurnitureS:
            // FIXME: we didn't have S wall sprites before; this is going on the square to the south.
            section = BuildingSquare::SectionWallN;
            dy = 1;
            break;
#if 0
        case FurnitureTile::FurnitureNW:
            s.mWallOrientation = Square::WallOrientNW;
            break;
        case FurnitureTile::FurnitureSE:
            s.mWallOrientation = Square::WallOrientSE;
            break;
#endif
        default:
            section = BuildingSquare::SectionInvalid;
            break;
        }
        for (int i = 0; i < ftile->size().height(); i++) {
            if (section == BuildingSquare::SectionInvalid) {
                continue;
            }
            for (int j = 0; j < ftile->size().width(); j++) {
                if (bounds(1, 1).contains(x + j + dx, y + i + dy)) {
                    BuildingSquare &s = squares[x + j + dx][y + i + dy];
#if 0
                    if (s.mEntries[section] && !s.mEntries[section]->isNone()) {
                        // FIXME: if SectionWall2 is occupied, push it down to SectionWall
                        section = Square::SectionWall2;
                    }
                    if (s.mTiles[section] && !s.mTiles[section]->isNone()) {
                        // FIXME: if SectionWall2 is occupied, push it down to SectionWall
                        section = Square::SectionWall2;
                    }
#endif
                    s.mTiles[section] = ftile->tile(j, i);
                    s.mEntries[section] = nullptr;
                    s.mEntryEnum[section] = 0;

                    switch (fo->furnitureTile()->orient()) {
                    case FurnitureTile::FurnitureW:
                    case FurnitureTile::FurnitureE:
                        if (s.HasWallN())
                            s.mWallN.type = BuildingSquare::WallType::TwoWallCorner;
                        else
                            s.mWallW.type = BuildingSquare::WallType::Simple;
                        break;
                    case FurnitureTile::FurnitureN:
                    case FurnitureTile::FurnitureS:
                        if (s.HasWallW())
                            s.mWallN.type = BuildingSquare::WallType::TwoWallCorner;
                        else
                            s.mWallN.type = BuildingSquare::WallType::Simple;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
    }


    // Place floors
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (mIndexAtPos[x][y] >= 0)
                squares[x][y].ReplaceFloor(floors[mIndexAtPos[x][y]], 0);
        }
    }

    if (BuildingFloor *floorBelow = this->floorBelow()) {
        // Place flat roof tops above roofs on the floor below
        for (RoofObject *ro : floorBelow->mFlatRoofsWithDepthThree) {
            ReplaceRoofTop(ro, ro->flatTop(), squares);
        }

        // Nuke floors that have stairs on the floor below.
        for (Stairs *stairs : floorBelow->mStairs) {
            int x = stairs->x(), y = stairs->y();
            if (stairs->isW()) {
                if (x + 1 < 0 || x + 3 >= width() || y < 0 || y >= height())
                    continue;
                squares[x+1][y].ReplaceFloor(nullptr, 0);
                squares[x+2][y].ReplaceFloor(nullptr, 0);
                squares[x+3][y].ReplaceFloor(nullptr, 0);
            }
            if (stairs->isN()) {
                if (x < 0 || x >= width() || y + 1 < 0 || y + 3 >= height())
                    continue;
                squares[x][y+1].ReplaceFloor(nullptr, 0);
                squares[x][y+2].ReplaceFloor(nullptr, 0);
                squares[x][y+3].ReplaceFloor(nullptr, 0);
            }
        }
    }

    // FIXME: I replaced Walls and Walls2 with WallN/WallW but they don't have the same meaning.
    // Existing TBX files with tiles in these layers won't work, unless you add Walls/Walls2 to TMXConfig.txt
    FloorTileGrid *userTilesWalls = mGrimeGrid.contains(QLatin1Literal("Walls")) ? mGrimeGrid[QLatin1Literal("Walls")] : nullptr;
    FloorTileGrid *userTilesWalls2 = mGrimeGrid.contains(QLatin1Literal("Walls2")) ? mGrimeGrid[QLatin1Literal("Walls2")] : nullptr;

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            BuildingSquare &sq = squares[x][y];

            sq.ReplaceWallTrim();
            if (sq.mEntryEnum[BuildingSquare::SectionWallW] == BTC_Walls::SouthEast) {
                if (y > 0 && squares[x][y-1].mWallW.trim) {
                    // SouthEast corner or south end of north-south wall
                    sq.mEntries[BuildingSquare::SectionWallTrimW] = squares[x][y-1].mWallW.trim;
                    sq.mEntryEnum[BuildingSquare::SectionWallTrimW] = BTC_Walls::SouthEast;
                } else if (x > 0 && squares[x-1][y].mWallN.trim) {
                    // East end of west-east wall
                    sq.mEntries[BuildingSquare::SectionWallTrimN] = squares[x-1][y].mWallN.trim;
                    sq.mEntryEnum[BuildingSquare::SectionWallTrimN] = BTC_Walls::SouthEast;
                }
            }

            if (sq.mExterior) {
                // Place exterior wall grime on level 0 only.
                if (level() > 0)
                    continue;
                QString userTileWalls = userTilesWalls ? userTilesWalls->at(x, y) : QString();
                QString userTileWalls2 = userTilesWalls2 ? userTilesWalls2->at(x, y) : QString();
                BuildingTileEntry *grimeTile = building()->tile(Building::GrimeWall);
                sq.ReplaceWallGrime(grimeTile, userTileWalls, userTileWalls2);
            } else {
                Room *room = GetRoomAt(x, y);
                BuildingTileEntry *grimeTile = room ? room->tile(Room::GrimeFloor) : nullptr;
                sq.ReplaceFloorGrime(grimeTile);

                QString userTileWalls = userTilesWalls ? userTilesWalls->at(x, y) : QString();
                QString userTileWalls2 = userTilesWalls2 ? userTilesWalls2->at(x, y) : QString();
                grimeTile = room ? room->tile(Room::GrimeWall) : nullptr;
                sq.ReplaceWallGrime(grimeTile, userTileWalls, userTileWalls2);
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
    if (!contains(pos))
        return 0;
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

QRect BuildingFloor::bounds(int dw, int dh) const
{
    return mBuilding->bounds().adjusted(0, 0, dw, dh);
}

bool BuildingFloor::contains(int x, int y, int dw, int dh)
{
    return bounds(dw, dh).contains(x, y);
}

bool BuildingFloor::contains(const QPoint &p, int dw, int dh)
{
    return contains(p.x(), p.y(), dw, dh);
}

QVector<QRect> BuildingFloor::roomRegion(Room *room)
{
    QRegion region;
    for (int y = 0; y < height(); y++) {
        for (int x = 0; x < width(); x++) {
            if (mRoomAtPos[x][y] == room)
                region += QRect(x, y, 1, 1);
        }
    }

    // Clean up the region by merging vertically-adjacent rectangles of the
    // same width.
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++) {
        QRect r = rects[i];
        if (!r.isValid()) continue;
        for (int j = 0; j < rects.size(); j++) {
            if (i == j) continue;
            QRect r2 = rects.at(j);
            if (!r2.isValid()) continue;
            if (r2.left() == r.left() && r2.right() == r.right()) {
                if (r.bottom() + 1 == r2.top()) {
                    r.setBottom(r2.bottom());
                    rects[j] = QRect();
                } else if (r.top() == r2.bottom() + 1) {
                    r.setTop(r2.top());
                    rects[j] = QRect();
                }
            }
        }
        rects[i] = r;
    }

    QVector<QRect> result;
    foreach (QRect r, rects) {
        if (r.isValid())
            result += r;
    }
    return result;
}

QVector<QVector<Room *> > BuildingFloor::resizeGrid(const QSize &newSize) const
{
    QVector<QVector<Room *> > grid = mRoomAtPos;
    grid.resize(newSize.width());
    for (int x = 0; x < newSize.width(); x++)
        grid[x].resize(newSize.height());
    return grid;
}

QMap<QString,FloorTileGrid*> BuildingFloor::resizeGrime(const QSize &newSize) const
{
    QMap<QString,FloorTileGrid*> grid;
    foreach (QString key, mGrimeGrid.keys()) {
        grid[key] = new FloorTileGrid(newSize.width(), newSize.height());
        for (int x = 0; x < qMin(mGrimeGrid[key]->width(), newSize.width()); x++)
            for (int y = 0; y < qMin(mGrimeGrid[key]->height(), newSize.height()); y++)
                grid[key]->replace(x, y, mGrimeGrid[key]->at(x, y));

    }

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
            roomAtPos[x][y] = nullptr;
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

BuildingFloor *BuildingFloor::clone()
{
    BuildingFloor *klone = new BuildingFloor(mBuilding, mLevel);
    klone->mIndexAtPos = mIndexAtPos;
    klone->mRoomAtPos = mRoomAtPos;
    foreach (BuildingObject *object, mObjects) {
        BuildingObject *kloneObject = object->clone();
        kloneObject->setFloor(klone);
        klone->mObjects += kloneObject;
    }
    klone->mGrimeGrid = mGrimeGrid;
    foreach (QString key, klone->mGrimeGrid.keys())
        klone->mGrimeGrid[key] = new FloorTileGrid(*klone->mGrimeGrid[key]);
    return klone;
}

QString BuildingFloor::grimeAt(const QString &layerName, int x, int y) const
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->at(x, y);
    return QString();
}

FloorTileGrid *BuildingFloor::grimeAt(const QString &layerName, const QRect &r)
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->clone(r);
    return new FloorTileGrid(r.width(), r.height());
}

FloorTileGrid *BuildingFloor::grimeAt(const QString &layerName, const QRect &r,
                                      const QRegion &rgn)
{
    if (mGrimeGrid.contains(layerName))
        return mGrimeGrid[layerName]->clone(r, rgn);
    return new FloorTileGrid(r.width(), r.height());
}

QMap<QString,FloorTileGrid*> BuildingFloor::grimeClone() const
{
    QMap<QString,FloorTileGrid*> clone;
    foreach (QString key, mGrimeGrid.keys()) {
        clone[key] = new FloorTileGrid(*mGrimeGrid[key]);
    }
    return clone;
}

QMap<QString, FloorTileGrid *> BuildingFloor::setGrime(const QMap<QString,
                                                       FloorTileGrid *> &grime)
{
    QMap<QString,FloorTileGrid*> old = mGrimeGrid;
    mGrimeGrid = grime;
    return old;
}

void BuildingFloor::setGrime(const QString &layerName, int x, int y,
                             const QString &tileName)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(x, y, tileName);
}


void BuildingFloor::setGrime(const QString &layerName, const QPoint &p,
                             const FloorTileGrid *other)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(p, other);
}

void BuildingFloor::setGrime(const QString &layerName, const QRegion &rgn,
                             const QString &tileName)
{
    if (!mGrimeGrid.contains(layerName)) {
        if (tileName.isEmpty())
            return;
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    }
    mGrimeGrid[layerName]->replace(rgn, tileName);
}

void BuildingFloor::setGrime(const QString &layerName, const QRegion &rgn,
                             const QPoint &pos, const FloorTileGrid *other)
{
    if (!mGrimeGrid.contains(layerName))
        mGrimeGrid[layerName] = new FloorTileGrid(width() + 1, height() + 1);
    mGrimeGrid[layerName]->replace(rgn, pos, other);
}

bool BuildingFloor::hasUserTiles() const
{
    foreach (FloorTileGrid *g, mGrimeGrid)
        if (!g->isEmpty()) return true;
    return false;
}

bool BuildingFloor::hasUserTiles(const QString &layerName)
{
    if (mGrimeGrid.contains(layerName))
        return !mGrimeGrid[layerName]->isEmpty();
    return false;
}

/////

BuildingSquare::BuildingSquare() :
    mEntries(MaxSection, nullptr),
    mEntryEnum(MaxSection, 0),
//    mWallOrientation(WallOrientInvalid),
    mExterior(true),
    mTiles(MaxSection, nullptr)
{
}


BuildingSquare::~BuildingSquare()
{
    // mTiles are owned by BuildingTiles
//    for (int i = 0; i < MaxSection; i++)
    //        delete mTiles[i];
}

void BuildingSquare::SetWallN(BuildingTileEntry *tile)
{
    mWallN.entry = tile;
    mWallN.furniture = nullptr;
    mWallN.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallW(BuildingTileEntry *tile)
{
    mWallW.entry = tile;
    mWallW.furniture = nullptr;
    mWallW.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallN(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallN.entry = nullptr;
    mWallN.furniture = ftile;
    mWallN.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallW(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallW.entry = nullptr;
    mWallW.furniture = ftile;
    mWallW.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallS(BuildingTileEntry *tile)
{
    mWallS.entry = tile;
    mWallS.furniture = nullptr;
    mWallS.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallE(BuildingTileEntry *tile)
{
    mWallE.entry = tile;
    mWallE.furniture = nullptr;
    mWallE.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallS(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallS.entry = nullptr;
    mWallS.furniture = ftile;
    mWallS.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallE(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallE.entry = nullptr;
    mWallE.furniture = ftile;
    mWallE.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallTrimN(BuildingTileEntry *tile)
{
    mWallN.trim = tile;
}

void BuildingSquare::SetWallTrimW(BuildingTileEntry *tile)
{
    mWallW.trim = tile;
}

bool BuildingSquare::HasWallN() const
{
    return mEntries[SectionWallN] != nullptr &&
            !mEntries[SectionWallN]->isNone() &&
            mWallN.type != WallType::Pillar;
}

bool BuildingSquare::HasWallW() const
{
    return mEntries[SectionWallW] != nullptr &&
            !mEntries[SectionWallW]->isNone() &&
            mWallW.type != WallType::Pillar;
}

bool BuildingSquare::HasDoorN() const
{
    return mEntries[SectionDoorN] != nullptr &&
            !mEntries[SectionDoorN]->isNone();
}

bool BuildingSquare::HasDoorS() const
{
    return mEntries[SectionDoorS] != nullptr &&
            !mEntries[SectionDoorS]->isNone();
}

bool BuildingSquare::HasDoorW() const
{
    return mEntries[SectionDoorW] != nullptr &&
            !mEntries[SectionDoorW]->isNone();
}

bool BuildingSquare::HasDoorE() const
{
    return mEntries[SectionDoorE] != nullptr &&
            !mEntries[SectionDoorE]->isNone();
}

bool BuildingSquare::HasWindowN() const
{
    return mEntries[SectionWindowN] != nullptr &&
            !mEntries[SectionWindowN]->isNone();
}

bool BuildingSquare::HasWindowS() const
{
    return mEntries[SectionWindowS] != nullptr &&
            !mEntries[SectionWindowS]->isNone();
}

bool BuildingSquare::HasWindowW() const
{
    return mEntries[SectionWindowW] != nullptr &&
            !mEntries[SectionWindowW]->isNone();
}

bool BuildingSquare::HasWindowE() const
{
    return mEntries[SectionWindowE] != nullptr &&
            !mEntries[SectionWindowE]->isNone();
}

void BuildingSquare::ReplaceFloor(BuildingTileEntry *tile, int offset)
{
    mEntries[SectionFloor] = tile;
    mEntryEnum[SectionFloor] = offset;
}

void BuildingSquare::ReplaceWall(BuildingTileEntry *tile,
                                 BuildingSquare::SquareSection section,
                                 const WallInfo& wallInfo)
{
    mEntries[section] = tile;
//    mWallOrientation = orient; // Must set this before getWallOffset() is called
    // This is done after doors and windows are placed.
    mEntryEnum[section] = -1; // getWallOffset(wallInfo, section);
}

void BuildingSquare::ReplaceDoor(BuildingTileEntry *tile, SquareSection section, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[section] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[section] = offset;

    WallInfo *wallInfo = nullptr;

    SquareSection sectionWall;
    SquareSection sectionWindow;
    switch (section) {
    case SquareSection::SectionDoorN:
        wallInfo = &mWallN;
        sectionWall = SquareSection::SectionWallN;
        sectionWindow = SquareSection::SectionWindowN;
        break;
    case SquareSection::SectionDoorW:
        wallInfo = &mWallW;
        sectionWall = SquareSection::SectionWallW;
        sectionWindow = SquareSection::SectionWindowW;
        break;
    case SquareSection::SectionDoorE:
        wallInfo = &mWallE;
        sectionWall = SquareSection::SectionWallE;
        sectionWindow = SquareSection::SectionWindowE;
        break;
    case SquareSection::SectionDoorS:
        wallInfo = &mWallS;
        sectionWall = SquareSection::SectionWallS;
        sectionWindow = SquareSection::SectionWindowS;
        break;
    default:
        Q_ASSERT(false);
        return;
    }

    // Nuke any existing window
    if (mEntries[sectionWindow]) {
        mEntries[sectionWindow] = nullptr;
        mEntryEnum[sectionWindow] = 0;
    }
#if 0
    if (wallInfo->type == WallType::TwoWallCorner) {
        BuildingTileEntry *entry1 = mEntries[SectionWallN];
        BuildingTileEntry *entry2 = mEntries[SectionWallW];
        if (!entry1) entry1 = BuildingTilesMgr::instance()->noneTileEntry();
        if (!entry2) entry2 = BuildingTilesMgr::instance()->noneTileEntry();
        if (!entry1->isNone() && !entry2->isNone()) {
            // 2 different walls
            if (offset == BTC_Doors::West) {
                mEntryEnum[SectionWallW] = BTC_Walls::WestDoor;
            } else if (offset == BTC_Doors::North) {
                mEntryEnum[SectionWallN] = BTC_Walls::NorthDoor;
            }
        } else {
            // Single NW tile -> split into 2.
            // Stack the wall containing the door on top of the other wall.
            mEntries[SectionWallN] = entry1;
            if (offset == BTC_Doors::West) {
                mEntryEnum[SectionWallN] = BTC_Walls::North;
                mEntryEnum[SectionWallW] = BTC_Walls::WestDoor;
            } else {
                mEntryEnum[SectionWallN] = BTC_Walls::NorthDoor;
                mEntryEnum[SectionWallW] = BTC_Walls::West;
            }
        }
        return;
    }

    if (wallInfo->type != WallType::Invalid) {
        mEntryEnum[sectionWall] = getWallOffset(*wallInfo, sectionWall);
    }
#endif
}

void BuildingSquare::ReplaceFrame(BuildingTileEntry *tile, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[SectionFrame] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[SectionFrame] = offset;
}

void BuildingSquare::ReplaceWindow(BuildingTileEntry *tile, SquareSection section, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[section] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[section] = offset;

    WallInfo *wallInfo = nullptr;

    SquareSection sectionWall;
    SquareSection sectionDoor;
    switch (section) {
    case SquareSection::SectionWindowN:
        wallInfo = &mWallN;
        sectionWall = SquareSection::SectionWallN;
        sectionDoor = SquareSection::SectionDoorN;
        break;
    case SquareSection::SectionWindowW:
        wallInfo = &mWallW;
        sectionWall = SquareSection::SectionWallW;
        sectionDoor = SquareSection::SectionDoorW;
        break;
    case SquareSection::SectionWindowE:
        wallInfo = &mWallE;
        sectionWall = SquareSection::SectionWallE;
        sectionDoor = SquareSection::SectionDoorE;
        break;
    case SquareSection::SectionWindowS:
        wallInfo = &mWallS;
        sectionWall = SquareSection::SectionWallS;
        sectionDoor = SquareSection::SectionDoorS;
        break;
    default:
        Q_ASSERT(false);
        return;
    }

    // Nuke any existing door
    if (mEntries[sectionDoor]) {
        mEntries[sectionDoor] = nullptr;
        mEntryEnum[sectionDoor] = 0;
    }
#if 0
    if (wallInfo->type == WallType::TwoWallCorner) {
        BuildingTileEntry *entry1 = mEntries[SectionWallN];
        BuildingTileEntry *entry2 = mEntries[SectionWallW];
        if (!entry1) entry1 = BuildingTilesMgr::instance()->noneTileEntry();
        if (!entry2) entry2 = BuildingTilesMgr::instance()->noneTileEntry();
        if (!entry1->isNone() && !entry2->isNone()) {
            // 2 different walls
            if (offset == BTC_Windows::West) {
                mEntryEnum[SectionWallW] = BTC_Walls::WestWindow;
            } else if (offset == BTC_Windows::North) {
                mEntryEnum[SectionWallN] = BTC_Walls::NorthWindow;
            }
        } else {
            // Single NW tile -> split into 2.
            // Stack the wall containing the window on top of the other wall.
            mEntries[SectionWallN] = entry1;
            if (offset == BTC_Windows::West) {
                mEntryEnum[SectionWallN] = BTC_Walls::North;
                mEntryEnum[SectionWallW] = BTC_Walls::WestWindow;
            } else {
                mEntryEnum[SectionWallN] = BTC_Walls::NorthWindow;
                mEntryEnum[SectionWallW] = BTC_Walls::West;
            }
        }
        return;
    }

    if (wallInfo->type != WallType::Invalid) {
        mEntryEnum[sectionWall] = getWallOffset(*wallInfo, sectionWall);
    }
#endif
}

void BuildingSquare::ReplaceCurtains(Window *window, SquareSection section)
{
    mEntries[section] = window->curtainsTile();
    switch (section) {
    case SquareSection::SectionCurtainsN:
        mEntryEnum[section] = BTC_Curtains::North;
        break;
    case SquareSection::SectionCurtainsW:
        mEntryEnum[section] = BTC_Curtains::West;
        break;
    case SquareSection::SectionCurtainsE:
        mEntryEnum[section] = BTC_Curtains::East;
        break;
    case SquareSection::SectionCurtainsS:
        mEntryEnum[section] = BTC_Curtains::South;
        break;
    default:
        Q_ASSERT(false);
        break;
    }
}

void BuildingSquare::ReplaceShutters(Window *window, bool first)
{
    if (!window->shuttersTile() || window->shuttersTile()->isNone())
        return;

    if (mExterior) {
        // TODO: East and West
        if (window->isN()) {
            ReplaceFurniture(window->shuttersTile()->tile(first ? BTC_Shutters::NorthLeft : BTC_Shutters::NorthRight),
                             SectionWallFurnitureN, SectionWallFurnitureN);
        } else {
            ReplaceFurniture(window->shuttersTile()->tile(first ? BTC_Shutters::WestAbove : BTC_Shutters::WestBelow),
                             SectionWallFurnitureW, SectionWallFurnitureW);
        }
    } else {

    }
}

void BuildingSquare::ReplaceFurniture(BuildingTileEntry *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mEntries[SectionFurniture] = 0;
        mEntryEnum[SectionFurniture] = 0;
        return;
    }
    if (mEntries[SectionFurniture] && !mEntries[SectionFurniture]->isNone()) {
        mEntries[SectionFurniture2] = tile;
        mEntryEnum[SectionFurniture2] = offset;
        return;
    }
    mEntries[SectionFurniture] = tile;
    mEntryEnum[SectionFurniture] = offset;
}

void BuildingSquare::ReplaceFurniture(BuildingTile *btile,
                                             SquareSection sectionMin,
                                             SquareSection sectionMax)
{
    for (int s = sectionMin; s <= sectionMax; s++) {
        if (!mTiles[s] || mTiles[s]->isNone()) {
            mTiles[s] = btile;
            mEntryEnum[s] = 0;
            return;
        }
    }
    for (int s = sectionMin + 1; s <= sectionMax; s++) {
        mTiles[s-1] = mTiles[s];
        mEntryEnum[s-1] = mEntryEnum[s];
    }
    mTiles[sectionMax] = btile;
    mEntryEnum[sectionMax] = 0;
}

void BuildingSquare::ReplaceRoof(BuildingTileEntry *tile, int offset)
{
    if (mEntries[SectionRoof] && !mEntries[SectionRoof]->isNone()) {
        mEntries[SectionRoof2] = tile;
        mEntryEnum[SectionRoof2] = offset;
        return;
    }
    mEntries[SectionRoof] = tile;
    mEntryEnum[SectionRoof] = offset;
}

void BuildingSquare::ReplaceRoofCap(BuildingTileEntry *tile, int offset)
{
    if (mEntries[SectionRoofCap] && !mEntries[SectionRoofCap]->isNone()) {
        mEntries[SectionRoofCap2] = tile;
        mEntryEnum[SectionRoofCap2] = offset;
        return;
    }
    mEntries[SectionRoofCap] = tile;
    mEntryEnum[SectionRoofCap] = offset;
}

void BuildingSquare::ReplaceRoofTop(BuildingTileEntry *tile, int offset)
{
    mEntries[SectionRoofTop] = tile;
    mEntryEnum[SectionRoofTop] = offset;
}

void BuildingSquare::ReplaceFloorGrime(BuildingTileEntry *grimeTile)
{
    if (!grimeTile || grimeTile->isNone())
        return;

    BuildingTileEntry *wallTileN = mEntries[SectionWallN];
    if (wallTileN && wallTileN->isNone()) wallTileN = nullptr;

    BuildingTileEntry *wallTileW = mEntries[SectionWallW];
    if (wallTileW && wallTileW->isNone()) wallTileW = nullptr;

//    Q_ASSERT(!(wallTileN == nullptr && wallTileW != nullptr));

    int grimeEnumW = -1, grimeEnumN = -1;

    if (wallTileN) {
        switch (mEntryEnum[SectionWallN]) {
        case BTC_Walls::North:
        case BTC_Walls::NorthWindow:
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::NorthDoor:
             // no grime by doors
            break;
        case BTC_Walls::NorthWest:
            grimeEnumW = BTC_GrimeFloor::West;
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::SouthEast:
            // Hack - ignore "end cap" of a wall object.
            if (mWallN.type != WallType::Pillar)
                break;
            grimeEnumW = BTC_GrimeFloor::NorthWest;
            break;
        }
    }

    if (wallTileW) {
        switch (mEntryEnum[SectionWallW]) {
        case BTC_Walls::West:
        case BTC_Walls::WestWindow:
            grimeEnumW = BTC_GrimeFloor::West;
            break;
        case BTC_Walls::WestDoor:
             // no grime by doors
            break;
        case BTC_Walls::NorthWest:
            Q_ASSERT(false);
            grimeEnumW = BTC_GrimeFloor::West;
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::SouthEast:
            // Hack - ignore "end cap" of a wall object.
            if (mWallW.type != WallType::Pillar)
                break;
            grimeEnumW = BTC_GrimeFloor::NorthWest;
            break;
        }
    }

    // Handle furniture in Walls layer
    if (mWallW.furniture && mWallW.furniture->resolved()->allowGrime())
        grimeEnumW = BTC_GrimeFloor::West;
    if (mWallN.furniture && mWallN.furniture->resolved()->allowGrime())
        grimeEnumN = BTC_GrimeFloor::North;

    if (grimeEnumW >= 0) {
        mEntries[SectionFloorGrime] = grimeTile;
        mEntryEnum[SectionFloorGrime] = grimeEnumW;
    }
    if (grimeEnumN >= 0) {
        mEntries[SectionFloorGrime2] = grimeTile;
        mEntryEnum[SectionFloorGrime2] = grimeEnumN;
    }
}

#include "filesystemwatcher.h"
#include "tiledeffile.h"
#include "tilemetainfomgr.h"
#include <QFileInfo>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

namespace Tiled {
namespace Internal {

TileDefWatcher::TileDefWatcher() :
    mWatcher(new FileSystemWatcher(this)),
    mTileDefFile(new TileDefFile()),
    tileDefFileChecked(false),
    watching(false)
{
    connect(mWatcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));
}


void TileDefWatcher::check()
{
    if (!tileDefFileChecked) {
        QFileInfo fileInfo(TileMetaInfoMgr::instance()->tilesDirectory() + QString::fromLatin1("/newtiledefinitions.tiles"));
#if 1
        QFileInfo info2(QLatin1Literal("D:/zomboid-svn/Anims2/workdir/media/newtiledefinitions.tiles"));
        if (info2.exists())
            fileInfo = info2;
#endif
        if (fileInfo.exists()) {
            qDebug() << "TileDefWatcher read " << fileInfo.absoluteFilePath();
            mTileDefFile->read(fileInfo.absoluteFilePath());
            if (!watching) {
                mWatcher->addPath(fileInfo.canonicalFilePath());
                watching = true;
            }
        }
        tileDefFileChecked = true;
    }
}

void TileDefWatcher::fileChanged(const QString &path)
{
    qDebug() << "TileDefWatcher.fileChanged() " << path;
    tileDefFileChecked = false;
    //        removePath(path);
    //        addPath(path);
}

} // namespace Internal
} // namespace Tiled

static Tiled::Internal::TileDefWatcher *tileDefWatcher = nullptr;

namespace BuildingEditor
{

Tiled::Internal::TileDefWatcher *getTileDefWatcher()
{
    if (tileDefWatcher == nullptr) {
        tileDefWatcher = new Tiled::Internal::TileDefWatcher();
    }
    return tileDefWatcher;
}

} // namespace BuildingEditor

struct GrimeProperties
{
    bool West;
    bool North;
    bool SouthEast;
    bool FullWindow;
    bool Trim;
    bool DoubleLeft;
    bool DoubleRight;
};

static bool tileHasGrimeProperties(BuildingTile *btile, GrimeProperties *props)
{
    if (btile == nullptr)
        return false;

    Tiled::Internal::TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    tileDefWatcher->check();

    if (props) {
        props->West = props->North = props->SouthEast = false;
        props->FullWindow = props->Trim = false;
        props->DoubleLeft = props->DoubleRight = false;
    }

    if (Tiled::Internal::TileDefTileset *tdts = tileDefWatcher->mTileDefFile->tileset(btile->mTilesetName)) {
        if (Tiled::Internal::TileDefTile *tdt = tdts->tileAt(btile->mIndex)) {
            if (tdt->mProperties.contains(QString::fromLatin1("GrimeType"))) {
                if (props) {
                    if (tdt->mProperties.contains(QString::fromLatin1("DoorWallW")) ||
                            tdt->mProperties.contains(QString::fromLatin1("WallW")) ||
                            tdt->mProperties.contains(QString::fromLatin1("WallWTrans")) ||
                            tdt->mProperties.contains(QString::fromLatin1("windowW")))
                        props->West = true;
                    else if (tdt->mProperties.contains(QString::fromLatin1("DoorWallN")) ||
                             tdt->mProperties.contains(QString::fromLatin1("WallN")) ||
                             tdt->mProperties.contains(QString::fromLatin1("WallNTrans")) ||
                             tdt->mProperties.contains(QString::fromLatin1("windowN")))
                        props->North = true;
                    else if (tdt->mProperties.contains(QString::fromLatin1("WallNW")))
                        props->West = props->North = true;
                    else if (tdt->mProperties.contains(QString::fromLatin1("WallSE")))
                        props->SouthEast = true;
                    if (tdt->mProperties.contains(QLatin1Literal("GrimeType"))) {
                        props->FullWindow = tdt->mProperties[QLatin1Literal("GrimeType")] == QLatin1Literal("FullWindow");
                        props->Trim = tdt->mProperties[QLatin1Literal("GrimeType")] == QLatin1Literal("Trim");
                        props->DoubleLeft = tdt->mProperties[QLatin1Literal("GrimeType")] == QLatin1Literal("DoubleLeft");
                        props->DoubleRight = tdt->mProperties[QLatin1Literal("GrimeType")] == QLatin1Literal("DoubleRight");
                    }
                }
                return true;
            }
            if (tdt->mProperties.contains(QString::fromLatin1("WallW"))) {
                if (props) {
                    props->West = true; // regular west wall
                }
                return true;
            }
            if (tdt->mProperties.contains(QString::fromLatin1("WallN"))) {
                if (props) {
                    props->North = true; // regular north wall
                }
                return true;
            }
        }
    }

    return false;
}

void BuildingSquare::ReplaceWallGrime(BuildingTileEntry *grimeTile, const QString &userTileWalls, const QString &userTileWalls2)
{
    if (!grimeTile || grimeTile->isNone())
        return;

    BuildingTileEntry *tileEntry1 = mEntries[SectionWallN];
    if (tileEntry1 && tileEntry1->isNone())
        tileEntry1 = nullptr;

    BuildingTileEntry *tileEntry2 = mEntries[SectionWallW];
    if (tileEntry2 && tileEntry2->isNone())
        tileEntry2 = nullptr;

//    Q_ASSERT(!(tileEntry1 == nullptr && tileEntry2 != nullptr));

    BuildingTile *tileW = nullptr, *tileN = nullptr, *tileNW = nullptr, *tileSE = nullptr;
    int grimeEnumW = -1, grimeEnumN = -1;

    if (tileEntry1) {
        BuildingTile *btile = tileEntry1->tile(mEntryEnum[SectionWallN]);
        switch (mEntryEnum[SectionWallN]) {
        case BTC_Walls::North:
            grimeEnumN = BTC_GrimeWall::North;
            tileN = btile;
            break;
        case BTC_Walls::NorthDoor:
            tileN = btile;
            grimeEnumN = BTC_GrimeWall::NorthDoor;
            break;
        case BTC_Walls::NorthWindow:
            tileN = btile;
            grimeEnumN = BTC_GrimeWall::NorthWindow;
            break;
        case BTC_Walls::NorthWest:
            tileNW = btile;
            grimeEnumW = BTC_GrimeWall::NorthWest;
            grimeEnumN = -1;
            break;
        case BTC_Walls::SouthEast:
            tileSE = btile;
            grimeEnumW = BTC_GrimeWall::SouthEast;
            break;
        }
    }
    if (tileEntry2) {
        BuildingTile *btile = tileEntry2->tile(mEntryEnum[SectionWallW]);
        switch (mEntryEnum[SectionWallW]) {
        case BTC_Walls::West:
            tileW = btile;
            grimeEnumW = BTC_GrimeWall::West;
            break;
        case BTC_Walls::WestDoor:
            tileW = btile;
            grimeEnumW = BTC_GrimeWall::WestDoor;
            break;
        case BTC_Walls::WestWindow:
            tileW = btile;
            grimeEnumW = BTC_GrimeWall::WestWindow;
            break;
        case BTC_Walls::NorthWest:
            tileNW = btile;
            grimeEnumW = BTC_GrimeWall::NorthWest;
            grimeEnumN = -1;
            break;
        case BTC_Walls::SouthEast:
            tileSE = btile;
            grimeEnumW = BTC_GrimeWall::SouthEast;
            break;
        }
    }

    if (mWallW.furnitureBldgTile && !mWallW.furnitureBldgTile->isNone() && mWallW.furniture->resolved()->allowGrime()) {
        tileW = mWallW.furnitureBldgTile;
        grimeEnumW = BTC_GrimeWall::West;
    }
    if (mWallN.furnitureBldgTile && !mWallN.furnitureBldgTile->isNone() && mWallN.furniture->resolved()->allowGrime()) {
        tileN = mWallN.furnitureBldgTile;
        grimeEnumN = BTC_GrimeWall::North;
    }

    GrimeProperties props;

    if (tileNW && tileHasGrimeProperties(tileNW, &props)) {
        if (props.FullWindow)
            grimeEnumW = grimeEnumN = -1;
        else if (props.Trim) {
            grimeEnumW = BTC_GrimeWall::NorthWestTrim;
            grimeEnumN = -1;
        }
    }
    if (tileW && tileHasGrimeProperties(tileW, &props)) {
        if (props.FullWindow)
            grimeEnumW = -1;
        else if (props.Trim)
            grimeEnumW = BTC_GrimeWall::WestTrim;
        else if (props.DoubleLeft)
            grimeEnumW = BTC_GrimeWall::WestDoubleLeft;
        else if (props.DoubleRight)
            grimeEnumW = BTC_GrimeWall::WestDoubleRight;
    }
    if (tileN && tileHasGrimeProperties(tileN, &props)) {
        if (props.FullWindow)
            grimeEnumN = -1;
        else if (props.Trim)
            grimeEnumN = BTC_GrimeWall::NorthTrim;
        else if (props.DoubleLeft)
            grimeEnumN = BTC_GrimeWall::NorthDoubleLeft;
        else if (props.DoubleRight)
            grimeEnumN = BTC_GrimeWall::NorthDoubleRight;
    }

    if (!userTileWalls.isEmpty()) {
        if (BuildingTile *btile = BuildingTilesMgr::instance()->get(userTileWalls)) {
            if (tileHasGrimeProperties(btile, &props)) {
                if (props.FullWindow) {
                    if (props.West) grimeEnumW = -1;
                    if (props.North) grimeEnumN = -1;
                } else if (props.Trim) {
                    if (props.West && props.North) {
                        grimeEnumW = BTC_GrimeWall::NorthWestTrim;
                        grimeEnumN = -1;
                    } else if (props.West) {
                        grimeEnumW = BTC_GrimeWall::WestTrim;
                    } else if (props.North) {
                        grimeEnumN = BTC_GrimeWall::NorthTrim;
                    } else if (props.SouthEast) {
                        grimeEnumW = BTC_GrimeWall::SouthEastTrim;
                    }
                } else if (props.DoubleLeft) {
                    if (props.West) grimeEnumW = BTC_GrimeWall::WestDoubleLeft;
                    else if (props.North) grimeEnumN = BTC_GrimeWall::NorthDoubleLeft;
                } else if (props.DoubleRight) {
                    if (props.West) grimeEnumW = BTC_GrimeWall::WestDoubleRight;
                    else if (props.North) grimeEnumN = BTC_GrimeWall::NorthDoubleRight;
                } else if (props.West) {
                    grimeEnumW = BTC_GrimeWall::West;
                } else if (props.North) {
                    grimeEnumN = BTC_GrimeWall::North;
                }
            }
        }
    }
    if (!userTileWalls2.isEmpty()) {
        if (BuildingTile *btile = BuildingTilesMgr::instance()->get(userTileWalls2)) {
            if (tileHasGrimeProperties(btile, &props)) {
                if (props.FullWindow) {
                    if (props.West) grimeEnumW = -1;
                    if (props.North) grimeEnumN = -1;
                } else if (props.Trim) {
                    if (props.West && props.North) {
                        grimeEnumW = BTC_GrimeWall::NorthWestTrim;
                        grimeEnumN = -1;
                    } else if (props.West) {
                        grimeEnumW = BTC_GrimeWall::WestTrim;
                    } else if (props.North) {
                        grimeEnumN = BTC_GrimeWall::NorthTrim;
                    } else if (props.SouthEast) {
                        grimeEnumW = BTC_GrimeWall::SouthEastTrim;
                    }
                } else if (props.DoubleLeft) {
                    if (props.West) grimeEnumW = BTC_GrimeWall::WestDoubleLeft;
                    else if (props.North) grimeEnumN = BTC_GrimeWall::NorthDoubleLeft;
                } else if (props.DoubleRight) {
                    if (props.West) grimeEnumW = BTC_GrimeWall::WestDoubleRight;
                    else if (props.North) grimeEnumN = BTC_GrimeWall::NorthDoubleRight;
                } else if (props.West) {
                    grimeEnumW = BTC_GrimeWall::West;
                } else if (props.North) {
                    grimeEnumN = BTC_GrimeWall::North;
                }
            }
        }
    }

    // 2 different wall tiles in a NW corner can't use a single NW grime tile.
    if (grimeEnumW == BTC_GrimeWall::NorthWest && grimeEnumN >= 0) {
        grimeEnumW = BTC_GrimeWall::West;
    }
    if (grimeEnumW == BTC_GrimeWall::NorthWestTrim && grimeEnumN >= 0) {
        grimeEnumW = BTC_GrimeWall::WestTrim;
    }

    if (grimeEnumW >= 0) {
        mEntries[SectionWallGrimeW] = grimeTile;
        mEntryEnum[SectionWallGrimeW] = grimeEnumW;
    }
    if (grimeEnumN >= 0) {
        mEntries[SectionWallGrimeN] = grimeTile;
        mEntryEnum[SectionWallGrimeN] = grimeEnumN;
    }
}

void BuildingSquare::ReplaceWallTrim()
{
    BuildingTileEntry *wallTile1 = mEntries[SectionWallN];
    if (wallTile1 && wallTile1->isNone()) wallTile1 = nullptr;

    BuildingTileEntry *wallTile2 = mEntries[SectionWallW];
    if (wallTile2 && wallTile2->isNone()) wallTile2 = nullptr;

    if (!wallTile1 && !wallTile2) return;

    BuildingTileEntry *trimW = mWallW.trim;
    BuildingTileEntry *trimN = mWallN.trim;

    // No trim over furniture in the Walls layer
    if (trimW && mWallW.furniture) trimW = nullptr;
    if (trimN && mWallN.furniture) trimN = nullptr;

    if (!trimW && !trimN) return;

    int enumW = -1;
    int enumN = -1;

    // SectionWallN could be a north, or north-west wall
    if (wallTile1) {
        enumN = mEntryEnum[SectionWallN];
        if (enumN == BTC_Walls::NorthWest) {
            enumW = BTC_Walls::West;
            enumN = BTC_Walls::North;
        }
    }

    // Use the same stacking order as the walls
    int sectionW = SectionWallTrimW;
    int sectionN = SectionWallTrimN;

    // 2 different wall tiles forming north-west corner
    if (wallTile2) {
        enumW = mEntryEnum[SectionWallW];
        if (enumW == BTC_Walls::NorthWest) {
            enumW = BTC_Walls::West;
            enumN = BTC_Walls::North;
        }
    }

    if ((enumW == BTC_Walls::West && enumN == BTC_Walls::North) && (trimW == trimN)) {
        enumN = BTC_Walls::NorthWest;
        enumW = -1;
    }

    if (enumW >= 0) {
        mEntries[sectionW] = trimW;
        mEntryEnum[sectionW] = enumW;
    }
    if (enumN >= 0) {
        mEntries[sectionN] = trimN;
        mEntryEnum[sectionN] = enumN;
    }
}

int BuildingSquare::getWallOffset(const WallInfo &wallInfo, SquareSection section)
{
    BuildingTileEntry *tile = mEntries[section];
    if (!tile)
        return -1;

    int offset = BTC_Walls::West;

    // TODO: need to check for door/window on opposite square.  i.e. If door on south edge of north square, put door frame on north edge of this square.

    switch (wallInfo.edge) {
    case WallEdge::N:
        if (wallInfo.type == WallType::TwoWallCorner)
            offset = BTC_Walls::NorthWest;
        else if (wallInfo.type == WallType::Pillar)
            offset = BTC_Walls::SouthEast;
        else if (mEntries[SquareSection::SectionDoorN] != nullptr && mEntryEnum[SquareSection::SectionDoorN] == BTC_Doors::North)
            offset = BTC_Walls::NorthDoor;
        else if (mEntries[SquareSection::SectionWindowN] != nullptr && mEntryEnum[SectionWindowN] == BTC_Doors::North)
            offset = BTC_Walls::NorthWindow;
        else
            offset = BTC_Walls::North;
        break;
    case WallEdge::W:
        if (wallInfo.type == WallType::TwoWallCorner)
            offset = BTC_Walls::NorthWest;
        else if (wallInfo.type == WallType::Pillar)
            offset = BTC_Walls::SouthEast;
        else if (mEntries[SquareSection::SectionDoorW] != nullptr && mEntryEnum[SquareSection::SectionDoorW] == BTC_Doors::West)
            offset = BTC_Walls::WestDoor;
        else if (mEntries[SquareSection::SectionWindowW] != nullptr && mEntryEnum[SectionWindowN] == BTC_Doors::West)
            offset = BTC_Walls::WestWindow;
        else
            offset = BTC_Walls::West;
        break;
    case WallEdge::E:
        offset = BTC_Walls::West;
        break;
    case WallEdge::S:
        // TODO
        offset = BTC_Walls::North;
        break;
    default:
        Q_ASSERT(false);
        return -1;
    }

    return offset;
}
