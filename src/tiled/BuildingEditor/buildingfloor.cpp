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

#include "filesystemwatcher.h"
#include "tiledeffile.h"
#include "tilemetainfomgr.h"

#include <QFileInfo>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the MapRands class as so:
// class TILEDSHARED_EXPORT MapRands : public QVector<QVector<int> >
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
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

static void SetDoor(Door *door, QVector<QVector<BuildingSquare> > &squares)
{
    int x = door->x(), y = door->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (!bounds.contains(x, y)) {
        return;
    }
    switch (door->dir()) {
    case BuildingObject::Direction::N:
        squares[x][y].mWallN.window = nullptr;
        squares[x][y].mWallN.door = door;
        break;
    case BuildingObject::Direction::W:
        squares[x][y].mWallW.window = nullptr;
        squares[x][y].mWallW.door = door;
        break;
    case BuildingObject::Direction::E:
        squares[x][y].mWallE.window = nullptr;
        squares[x][y].mWallE.door = door;
        break;
    case BuildingObject::Direction::S:
        squares[x][y].mWallS.window = nullptr;
        squares[x][y].mWallS.door = door;
        break;
    default:
        Q_ASSERT(false);
        break;
    }
#if 0
    squares[x][y].ReplaceDoor(door->tile(),
                              door->isW() ? BuildingSquare::SquareSection::SectionDoorW
                                          : BuildingSquare::SquareSection::SectionDoorN,
                              door->isW() ? BTC_Doors::West
                                          : BTC_Doors::North);
    squares[x][y].ReplaceFrame(door->frameTile(),
                               door->isW() ? BTC_DoorFrames::West
                                           : BTC_DoorFrames::North);
#endif
}

static void SetWindow(Window *window, QVector<QVector<BuildingSquare> > &squares)
{
    int x = window->x(), y = window->y();
    QRect bounds(0, 0, squares.size(), squares[0].size());
    if (!bounds.contains(x, y)) {
        return;
    }
    switch (window->dir()) {
    case BuildingObject::Direction::N:
        squares[x][y].mWallN.door = nullptr;
        squares[x][y].mWallN.window = window;
        break;
    case BuildingObject::Direction::W:
        squares[x][y].mWallW.door = nullptr;
        squares[x][y].mWallW.window = window;
        break;
    case BuildingObject::Direction::E:
        squares[x][y].mWallE.door = nullptr;
        squares[x][y].mWallE.window = window;
        break;
    case BuildingObject::Direction::S:
        squares[x][y].mWallS.door = nullptr;
        squares[x][y].mWallS.window = window;
        break;
    default:
        Q_ASSERT(false);
        break;
    }
#if 0
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
    }
#endif
}

static bool tileHasWallPropertyN(const QString &tileName)
{
    Tiled::Internal::TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    tileDefWatcher->check();
    QString tilesetName;
    int tileID;
    if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileID)) {
        return false;
    }
    if (Tiled::Internal::TileDefTileset *tdts = tileDefWatcher->mTileDefFile->tileset(tilesetName)) {
        if (Tiled::Internal::TileDefTile *tdt = tdts->tileAt(tileID)) {
            if (tdt->mProperties.contains(QString::fromLatin1("DoorWallN")) ||
                     tdt->mProperties.contains(QString::fromLatin1("WallN")) ||
                     tdt->mProperties.contains(QString::fromLatin1("WallNTrans")) ||
                     tdt->mProperties.contains(QString::fromLatin1("windowN")) ||
                     tdt->mProperties.contains(QString::fromLatin1("WallNW"))) {
                return true;
            }
        }
    }
    return false;
}

static bool tileHasWallPropertyW(const QString &tileName)
{
    Tiled::Internal::TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    tileDefWatcher->check();
    QString tilesetName;
    int tileID;
    if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileID)) {
        return false;
    }
    if (Tiled::Internal::TileDefTileset *tdts = tileDefWatcher->mTileDefFile->tileset(tilesetName)) {
        if (Tiled::Internal::TileDefTile *tdt = tdts->tileAt(tileID)) {
            if (tdt->mProperties.contains(QString::fromLatin1("DoorWallW")) ||
                    tdt->mProperties.contains(QString::fromLatin1("WallW")) ||
                    tdt->mProperties.contains(QString::fromLatin1("WallWTrans")) ||
                    tdt->mProperties.contains(QString::fromLatin1("windowW")) ||
                    tdt->mProperties.contains(QString::fromLatin1("WallNW"))) {
                return true;
            }
        }
    }
}

// Don't place east walls when there is a user-drawn west wall tile to the east
static bool canPlaceWallE(BuildingFloor *floor, int x, int y)
{
    if (x >= floor->width()) {
        return false;
    }
    QString userTile1 = floor->grimeAt(QLatin1Literal("Wall"), x + 1, y);
    if (!userTile1.isEmpty() && tileHasWallPropertyW(userTile1)) {
        return false;
    }
    QString userTile2 = floor->grimeAt(QLatin1Literal("Wall2"), x + 1, y);
    if (!userTile2.isEmpty() && tileHasWallPropertyW(userTile2)) {
        return false;
    }
    return true;
}

// Don't place south walls when there is a user-drawn north wall tile to the south
static bool canPlaceWallS(BuildingFloor *floor, int x, int y)
{
    if (y >= floor->height()) {
        return false;
    }
    QString userTile1 = floor->grimeAt(QLatin1Literal("Wall"), x, y + 1);
    if (!userTile1.isEmpty() && tileHasWallPropertyN(userTile1)) {
        return false;
    }
    QString userTile2 = floor->grimeAt(QLatin1Literal("Wall2"), x, y + 1);
    if (!userTile2.isEmpty() && tileHasWallPropertyN(userTile2)) {
        return false;
    }
    return true;
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
            // Place N and S walls...
            if (x < width()) {
                if (y == height() && mIndexAtPos[x][y - 1] >= 0) {
                    squares[x][y].SetWallN(exteriorWall);
                    squares[x][y].SetWallTrimN(exteriorWallTrim);
                    if (canPlaceWallS(this, x, y - 1)) {
                        squares[x][y-1].SetWallS(interiorWalls[mIndexAtPos[x][y-1]]);
                        squares[x][y-1].SetWallTrimS(interiorWallTrim[mIndexAtPos[x][y-1]]);
                    }
                } else if (y < height() && mIndexAtPos[x][y] < 0 && y > 0 &&
                           mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallN(exteriorWall);
                    squares[x][y].SetWallTrimN(exteriorWallTrim);
                    if (canPlaceWallS(this, x, y - 1)) {
                        squares[x][y-1].SetWallS(interiorWalls[mIndexAtPos[x][y-1]]);
                        squares[x][y-1].SetWallTrimS(interiorWallTrim[mIndexAtPos[x][y-1]]);
                    }
                } else if (y < height() && (y == 0 || mIndexAtPos[x][y-1] != mIndexAtPos[x][y])
                           && mIndexAtPos[x][y] >= 0) {
                    squares[x][y].SetWallN(interiorWalls[mIndexAtPos[x][y]]);
                    squares[x][y].SetWallTrimN(interiorWallTrim[mIndexAtPos[x][y]]);
                    if (canPlaceWallS(this, x, y - 1)) {
                        if ((y > 0) && (mIndexAtPos[x][y-1] == -1)) {
                            squares[x][y-1].SetWallS(exteriorWall);
                            squares[x][y-1].SetWallTrimS(exteriorWallTrim);
                        } else if (y > 0) {
                            squares[x][y-1].SetWallS(interiorWalls[mIndexAtPos[x][y-1]]);
                            squares[x][y-1].SetWallTrimS(interiorWallTrim[mIndexAtPos[x][y-1]]);
                        }
                    }
                }
                // TODO: south walls at y=-1
            }
            // Place W and E walls...
            if (y < height()) {
                if (x == width() && mIndexAtPos[x - 1][y] >= 0) {
                    squares[x][y].SetWallW(exteriorWall);
                    squares[x][y].SetWallTrimW(exteriorWallTrim);
                    if (canPlaceWallE(this, x - 1, y)) {
                        squares[x-1][y].SetWallE(interiorWalls[mIndexAtPos[x-1][y]]);
                        squares[x-1][y].SetWallTrimE(interiorWallTrim[mIndexAtPos[x-1][y]]);
                    }
                } else if (x < width() && mIndexAtPos[x][y] < 0 && x > 0
                           && mIndexAtPos[x - 1][y] != mIndexAtPos[x][y]) {
                    squares[x][y].SetWallW(exteriorWall);
                    squares[x][y].SetWallTrimW(exteriorWallTrim);
                    if (canPlaceWallE(this, x - 1, y)) {
                        squares[x-1][y].SetWallE(interiorWalls[mIndexAtPos[x-1][y]]);
                        squares[x-1][y].SetWallTrimE(interiorWallTrim[mIndexAtPos[x-1][y]]);
                    }
                } else if (x < width() && mIndexAtPos[x][y] >= 0 &&
                           (x == 0 || mIndexAtPos[x - 1][y] != mIndexAtPos[x][y])) {
                    squares[x][y].SetWallW(interiorWalls[mIndexAtPos[x][y]]);
                    squares[x][y].SetWallTrimW(interiorWallTrim[mIndexAtPos[x][y]]);
                    if (canPlaceWallE(this, x - 1, y)) {
                        if ((x > 0) && (mIndexAtPos[x-1][y] == -1)) {
                            squares[x-1][y].SetWallE(exteriorWall);
                            squares[x-1][y].SetWallTrimE(exteriorWallTrim);
                        } else if (x > 0) {
                            squares[x-1][y].SetWallE(interiorWalls[mIndexAtPos[x-1][y]]);
                            squares[x-1][y].SetWallTrimE(interiorWallTrim[mIndexAtPos[x-1][y]]);
                        }
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
                    // TODO: exterior/interior on opposite squares
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
                    // TODO: exterior/interior on opposite squares
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
#if 0
    // Determine if wall sprites on each square should be door frames, window frames, 2-wall corners, or walls.
    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare &s = squares[x][y];
            BuildingTileEntry *wallN = s.mWallN.entryWall;
            BuildingTileEntry *wallW = s.mWallW.entryWall;
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
                }
            }
            BuildingTileEntry *wallS = s.mWallS.entryWall;
            BuildingTileEntry *wallE = s.mWallE.entryWall;
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
#endif
    // Place wall pillars at the ends of two perpendicular walls.
    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare &sq = squares[x][y];
            if (sq.HasWallN() || sq.HasWallW() ||
                    (sq.mWallW.furniture) || (sq.mWallN.furniture)) {
                continue;
            }
            // Put in the SE piece...
            if ((x > 0) && (y > 0) &&
                    squares[x][y - 1].HasWallW() &&
                    squares[x - 1][y].HasWallN()) {
                // With WallObjects, there could be 2 different tiles meeting
                // at this SE corner.
                wtype = squares[x][y - 1].mWallW.entryWall;
                sq.mEntries[BuildingSquare::SectionWall] = wtype;
                sq.mEntryEnum[BuildingSquare::SectionWall] = BTC_Walls::SouthEast;
                sq.mWallW.type = BuildingSquare::WallType::Pillar;
            }
            // South end of a north-south wall.
            else if ((y > 0) &&
                     squares[x][y - 1].HasWallW()) {
                wtype = squares[x][y - 1].mWallW.entryWall;
                sq.mWallW.type = BuildingSquare::WallType::Pillar;
                sq.mEntries[BuildingSquare::SectionWall] = wtype;
                sq.mEntryEnum[BuildingSquare::SectionWall] = BTC_Walls::SouthEast;
            }
            // East end of a west-east wall.
            else if ((x > 0) &&
                     squares[x - 1][y].HasWallN()) {
                wtype = squares[x - 1][y].mWallN.entryWall;
                sq.mWallN.type = BuildingSquare::WallType::Pillar;
                sq.mEntries[BuildingSquare::SectionWall] = wtype;
                sq.mEntryEnum[BuildingSquare::SectionWall] = BTC_Walls::SouthEast;
            }
        }
    }

    mFlatRoofsWithDepthThree.clear();
    mStairs.clear();

    for (BuildingObject *object : mObjects) {
        int x = object->x();
        int y = object->y();
        if (Door *door = object->asDoor()) {
            SetDoor(door, squares);
        }
        if (Window *window = object->asWindow()) {
            SetWindow(window, squares);
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
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlay;
                            sectionMax = BuildingSquare::SquareSection::SectionWallOverlay2;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureN:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlay;
                            sectionMax = BuildingSquare::SquareSection::SectionWallOverlay2;
                            sectionMax = sectionMin;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureE:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlay3;
                            sectionMax = BuildingSquare::SquareSection::SectionWallOverlay4;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureS:
                            sectionMin = BuildingSquare::SquareSection::SectionWallOverlay3;
                            sectionMax = BuildingSquare::SquareSection::SectionWallOverlay4;
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
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurniture;
                            sectionMax = BuildingSquare::SquareSection::SectionWallFurniture2;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureN:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurniture;
                            sectionMax = BuildingSquare::SquareSection::SectionWallFurniture2;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureE:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurniture3;
                            sectionMax = BuildingSquare::SquareSection::SectionWallFurniture4;
                            break;
                        case FurnitureTile::FurnitureOrientation::FurnitureS:
                            sectionMin = BuildingSquare::SquareSection::SectionWallFurniture3;
                            sectionMax = BuildingSquare::SquareSection::SectionWallFurniture4;
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
                                         BuildingSquare::SectionFrame1,
                                         BuildingSquare::SectionFrame2,
                                         dx, dy);
                        break;
                    }
                    case FurnitureTiles::LayerDoors: {
                        int dx = 0, dy = 0;
                        if (fo->furnitureTile()->isE()) ++dx;
                        if (fo->furnitureTile()->isS()) ++dy;
                        ReplaceFurniture(x + j + dx, y + i + dy,
                                         squares, ftile->tile(j, i),
                                         fo->furnitureTile()->isN() ? BuildingSquare::SectionDoor1 : BuildingSquare::SectionDoor2,
                                         fo->furnitureTile()->isN() ? BuildingSquare::SectionDoor1 : BuildingSquare::SectionDoor2,
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
            BuildingTileEntry *entryN = square.mWallN.entryWall;
            BuildingTileEntry *entryW = square.mWallW.entryWall;
            if (entryN != nullptr && entryN->isNone())
                entryN = nullptr;
            if (entryW != nullptr && entryW->isNone())
                entryW = nullptr;
            const int SectionWallN = BuildingSquare::SquareSection::SectionWall;
            const int SectionWallW = BuildingSquare::SquareSection::SectionWall2;
            if (entryN != nullptr) {
                square.mEntries[SectionWallN] = entryN;
                BuildingSquare *squareN = (y > 0) ? &squares[x][y - 1] : nullptr;
                if (square.mWallN.type == BuildingSquare::WallType::Pillar) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::SouthEast;
                } else if (square.HasDoorN() || square.HasDoorFrameN() || (squareN != nullptr && (squareN->HasDoorS() || squareN->HasDoorFrameS()))) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::NorthDoor;
                } else if (square.HasWindowN() || (squareN != nullptr && squareN->HasWindowS())) {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::NorthWindow;
                } else {
                    square.mEntryEnum[SectionWallN] = BTC_Walls::North;
                }
            }
            if (entryW != nullptr) {
                square.mEntries[SectionWallW] = entryW;
                BuildingSquare *squareW = (x > 0) ? &squares[x- 1][y ] : nullptr;
                if (square.mWallW.type == BuildingSquare::WallType::Pillar) {
                    square.mEntryEnum[SectionWallW] = BTC_Walls::SouthEast;
                } else if (square.HasDoorW() || square.HasDoorFrameW() || (squareW != nullptr && (squareW->HasDoorE() || squareW->HasDoorFrameE()))) {
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

            // Legacy issue: Before the tile-rotation stuff was added, north and west walls would be
            // placed in either SectionWall or SectionWall2.  When there was only a single wall, it would
            // be placed into SectionWall.  With both a north and west wall on a square, a door-cutout or window-cutout wall
            // would be placed into SectionWall2, above the "regular" wall in SectionWall.
            if ((square.mEntryEnum[SectionWallN] == BTC_Walls::NorthDoor || square.mEntryEnum[SectionWallN] == BTC_Walls::NorthWindow) &&
                    (square.mEntryEnum[SectionWallW] == BTC_Walls::West)) {
                qSwap(square.mEntries[SectionWallN], square.mEntries[SectionWallW]);
                qSwap(square.mEntryEnum[SectionWallN], square.mEntryEnum[SectionWallW]);
            }
            if (square.mEntries[SectionWallN] == nullptr && square.mEntries[SectionWallW] != nullptr) {
                qSwap(square.mEntries[SectionWallN], square.mEntries[SectionWallW]);
                qSwap(square.mEntryEnum[SectionWallN], square.mEntryEnum[SectionWallW]);
            }

            BuildingTileEntry *entryE = square.mWallE.entryWall;
            BuildingTileEntry *entryS = square.mWallS.entryWall;
            if (entryE != nullptr && entryE->isNone())
                entryE = nullptr;
            if (entryS != nullptr && entryS->isNone())
                entryS = nullptr;
            const int SectionWallE = BuildingSquare::SquareSection::SectionWall3;
            const int SectionWallS = BuildingSquare::SquareSection::SectionWall4;
            if (entryE != nullptr) {
                square.mEntries[SectionWallE] = entryE;
                BuildingSquare *squareE = (x < width() + 1) ? &squares[x + 1][y] : nullptr;
                if (square.mWallE.type == BuildingSquare::WallType::Pillar) {
                    //
                } else if (square.HasDoorE() || square.HasDoorFrameE() || (squareE != nullptr && (squareE->HasDoorW() || squareE->HasDoorFrameW()))) {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::WestDoor;
                } else if (square.HasWindowE() || (squareE != nullptr && squareE->HasWindowW())) {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::WestWindow;
                } else {
                    square.mEntryEnum[SectionWallE] = BTC_Walls::West;
                }
            }
            if (entryS != nullptr) {
                square.mEntries[SectionWallS] = entryS;
                BuildingSquare *squareS = (y < height() + 1) ? &squares[x][y + 1] : nullptr;
                if (square.mWallS.type == BuildingSquare::WallType::Pillar) {
                    //
                } else if (square.HasDoorS() || square.HasDoorFrameS() || (squareS != nullptr && (squareS->HasDoorN() || squareS->HasDoorFrameN()))) {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::NorthDoor;
                } else if (square.HasWindowN() || (squareS != nullptr && squareS->HasWindowN())) {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::NorthWindow;
                } else {
                    square.mEntryEnum[SectionWallS] = BTC_Walls::North;
                }
            }
        }
    }

    for (int x = 0; x < width() + 1; x++) {
        for (int y = 0; y < height() + 1; y++) {
            BuildingSquare& square = squares[x][y];

            if (square.HasDoorFrameN()) {
                square.ReplaceFrame(square.mWallN.door->frameTile(), BuildingSquare::SquareSection::SectionFrame1, BTC_DoorFrames::North);
            }
            if (square.HasDoorFrameW()) {
                square.ReplaceFrame(square.mWallW.door->frameTile(), BuildingSquare::SquareSection::SectionFrame2, BTC_DoorFrames::West);
            }
            if (square.HasDoorFrameE()) {
            }
            if (square.HasDoorFrameS()) {
            }

            if (square.HasDoorN()) {
                square.ReplaceDoor(square.mWallN.door->tile(), BuildingSquare::SquareSection::SectionDoor1, BTC_Doors::North);
            }
            if (square.HasDoorW()) {
                square.ReplaceDoor(square.mWallW.door->tile(), BuildingSquare::SquareSection::SectionDoor2, BTC_Doors::West);
            }
            if (square.HasDoorE()) {
            }
            if (square.HasDoorS()) {
            }

            if (square.HasWindowN()) {
                square.ReplaceWindow(square.mWallN.window->tile(), BuildingSquare::SquareSection::SectionWindow1, BTC_Windows::North);
            }
            if (square.HasWindowW()) {
                square.ReplaceWindow(square.mWallW.window->tile(), BuildingSquare::SquareSection::SectionWindow2, BTC_Windows::West);
            }
            if (square.HasWindowE()) {
            }
            if (square.HasWindowS()) {
            }

            // Window curtains on exterior walls must be *inside* the room.
            if (square.HasCurtainsN()) {
                if (square.mExterior) {
                    int x1 = x;
                    int y1 = y - 1;
                    if ((x1 >= 0) && (y1 >= 0)) {
                        squares[x1][y1].ReplaceCurtains(square.mWallN.window, BuildingSquare::SquareSection::SectionCurtains4, BTC_Curtains::South);
                    }
                } else {
                    square.ReplaceCurtains(square.mWallN.window, BuildingSquare::SquareSection::SectionCurtains1, BTC_Curtains::North);
                }
            }
            if (square.HasCurtainsW()) {
                if (square.mExterior) {
                    int x1 = x - 1;
                    int y1 = y;
                    if ((x1 >= 0) && (y1 >= 0)) {
                        squares[x1][y1].ReplaceCurtains(square.mWallW.window, BuildingSquare::SquareSection::SectionCurtains3, BTC_Curtains::East);
                    }
                } else {
                    square.ReplaceCurtains(square.mWallW.window, BuildingSquare::SquareSection::SectionCurtains2, BTC_Curtains::West);
                }
            }
            if (square.HasCurtainsE()) {
                if (square.mExterior) {
                    int x1 = x + 1;
                    int y1 = y;
                    if (true) {
                        squares[x1][y1].ReplaceCurtains(square.mWallE.window, BuildingSquare::SquareSection::SectionCurtains2, BTC_Curtains::West);
                    }
                } else {
                    square.ReplaceCurtains(square.mWallE.window, BuildingSquare::SquareSection::SectionCurtains3, BTC_Curtains::East);
                }
            }
            if (square.HasCurtainsS()) {
                if (square.mExterior) {
                    int x1 = x;
                    int y1 = y + 1;
                    if (true) {
                        squares[x1][y1].ReplaceCurtains(square.mWallS.window, BuildingSquare::SquareSection::SectionCurtains1, BTC_Curtains::North);
                    }
                } else {
                    square.ReplaceCurtains(square.mWallS.window, BuildingSquare::SquareSection::SectionCurtains4, BTC_Curtains::South);
                }
            }

            if (square.HasShuttersN()) {
                if (square.mExterior) {
                    if (x > 0)
                        squares[x-1][y].ReplaceShutters(square.mWallN.window, true);
                    squares[x][y].ReplaceShutters(square.mWallN.window, true);
                    squares[x][y].ReplaceShutters(square.mWallN.window, false);
                    if (x < width() + 1)
                        squares[x + 1][y].ReplaceShutters(square.mWallN.window, false);
                }
            }
            if (square.HasShuttersW()) {
                if (square.mExterior) {
                    if (y > 0)
                        squares[x][y - 1].ReplaceShutters(square.mWallW.window, true);
                    squares[x][y].ReplaceShutters(square.mWallW.window, true);
                    squares[x][y].ReplaceShutters(square.mWallW.window, false);
                    if (y < height() + 1)
                        squares[x][y + 1].ReplaceShutters(square.mWallW.window, false);
                }
            }
            if (square.HasShuttersE()) {

            }
            if (square.HasShuttersS()) {

            }
        }
    }

    for (FurnitureObject *fo : wallReplacement) {
        FurnitureTile *ftile = fo->furnitureTile()->resolved();
        int x = fo->x(), y = fo->y();
        int dx = 0, dy = 0;
        switch (fo->furnitureTile()->orient()) {
        case FurnitureTile::FurnitureW:
            break;
        case FurnitureTile::FurnitureE:
            dx = 1;
            break;
        case FurnitureTile::FurnitureN:
            break;
        case FurnitureTile::FurnitureS:
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
            break;
        }
        for (int i = 0; i < ftile->size().height(); i++) {
            for (int j = 0; j < ftile->size().width(); j++) {
                if (bounds(1, 1).contains(x + j + dx, y + i + dy)) {
                    BuildingSquare &s = squares[x + j + dx][y + i + dy];
                    BuildingSquare::SquareSection section = BuildingSquare::SectionWall;
                    if (s.mEntries[section] && !s.mEntries[section]->isNone()) {
                        // FIXME: if SectionWall2 is occupied, push it down to SectionWall
                        section = BuildingSquare::SectionWall2;
                    }
                    if (s.mTiles[section] && !s.mTiles[section]->isNone()) {
                        // FIXME: if SectionWall2 is occupied, push it down to SectionWall
                        section = BuildingSquare::SectionWall2;
                    }
                    s.mTiles[section] = ftile->tile(j, i);
                    s.mEntries[section] = nullptr;
                    s.mEntryEnum[section] = 0;
#if 0
                    switch (fo->furnitureTile()->orient()) {
                    case FurnitureTile::FurnitureW:
                    case FurnitureTile::FurnitureE:
                        if (s.mWallOrientation == BuildingSquare::WallOrientN)
                            s.mWallOrientation = BuildingSquare::WallOrientNW;
                        else
                            s.mWallOrientation = BuildingSquare::WallOrientW;
                        break;
                    case FurnitureTile::FurnitureN:
                    case FurnitureTile::FurnitureS:
                        if (s.mWallOrientation == BuildingSquare::WallOrientW)
                            s.mWallOrientation = BuildingSquare::WallOrientNW;
                        else
                            s.mWallOrientation = BuildingSquare::WallOrientN;
                        break;
                    default:
                        break;
                    }
#endif
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

    FloorTileGrid *userTilesWalls = mGrimeGrid.contains(QLatin1Literal("Wall")) ? mGrimeGrid[QLatin1Literal("Wall")] : nullptr;
    FloorTileGrid *userTilesWalls2 = mGrimeGrid.contains(QLatin1Literal("Wall2")) ? mGrimeGrid[QLatin1Literal("Wall2")] : nullptr;

    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            BuildingSquare &sq = squares[x][y];

            sq.ReplaceWallTrim();
            sq.ReplaceWallTrimES();

            if (sq.mEntryEnum[BuildingSquare::SectionWall] == BTC_Walls::SouthEast) {
                if (y > 0 && squares[x][y-1].mWallW.entryTrim) {
                    // SouthEast corner or south end of north-south wall
                    sq.mEntries[BuildingSquare::SectionWallTrim] = squares[x][y-1].mWallW.entryTrim;
                    sq.mEntryEnum[BuildingSquare::SectionWallTrim] = BTC_Walls::SouthEast;
                } else if (x > 0 && squares[x-1][y].mWallN.entryTrim) {
                    // East end of west-east wall
                    sq.mEntries[BuildingSquare::SectionWallTrim] = squares[x-1][y].mWallN.entryTrim;
                    sq.mEntryEnum[BuildingSquare::SectionWallTrim] = BTC_Walls::SouthEast;
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
    for (BuildingObject *o : mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = o->asDoor())
            return door;
    }
    return nullptr;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    for (BuildingObject *o : mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = o->asWindow())
            return window;
    }
    return nullptr;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    for (BuildingObject *o : mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = o->asStairs())
            return stairs;
    }
    return nullptr;
}

FurnitureObject *BuildingFloor::GetFurnitureAt(int x, int y)
{
    for (BuildingObject *o : mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (FurnitureObject *fo = o->asFurniture())
            return fo;
    }
    return nullptr;
}

void BuildingFloor::SetRoomAt(int x, int y, Room *room)
{
    mRoomAtPos[x][y] = room;
}

Room *BuildingFloor::GetRoomAt(const QPoint &pos)
{
    if (!contains(pos))
        return nullptr;
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
    mWallN.entryWall = tile;
    mWallN.furniture = nullptr;
    mWallN.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallW(BuildingTileEntry *tile)
{
    mWallW.entryWall = tile;
    mWallW.furniture = nullptr;
    mWallW.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallN(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallN.entryWall = nullptr;
    mWallN.furniture = ftile;
    mWallN.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallW(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallW.entryWall = nullptr;
    mWallW.furniture = ftile;
    mWallW.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallS(BuildingTileEntry *tile)
{
    mWallS.entryWall = tile;
    mWallS.furniture = nullptr;
    mWallS.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallE(BuildingTileEntry *tile)
{
    mWallE.entryWall = tile;
    mWallE.furniture = nullptr;
    mWallE.furnitureBldgTile = nullptr;
}

void BuildingSquare::SetWallS(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallS.entryWall = nullptr;
    mWallS.furniture = ftile;
    mWallS.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallE(FurnitureTile *ftile, BuildingTile *btile)
{
    mWallE.entryWall = nullptr;
    mWallE.furniture = ftile;
    mWallE.furnitureBldgTile = btile;
}

void BuildingSquare::SetWallTrimN(BuildingTileEntry *tile)
{
    mWallN.entryTrim = tile;
}

void BuildingSquare::SetWallTrimW(BuildingTileEntry *tile)
{
    mWallW.entryTrim = tile;
}

void BuildingSquare::SetWallTrimE(BuildingTileEntry *tile)
{
    mWallE.entryTrim = tile;
}

void BuildingSquare::SetWallTrimS(BuildingTileEntry *tile)
{
    mWallS.entryTrim = tile;
}

bool BuildingSquare::HasWallN() const
{
    return mWallN.entryWall != nullptr && !mWallN.entryWall->isNone();
//    return mEntries[SectionWall] != nullptr &&
//            !mEntries[SectionWall]->isNone() &&
//            mWallN.type != WallType::Pillar;
}

bool BuildingSquare::HasWallW() const
{
    return mWallW.entryWall != nullptr && !mWallW.entryWall->isNone();
//    return mEntries[SectionWallW] != nullptr &&
//            !mEntries[SectionWallW]->isNone() &&
//            mWallW.type != WallType::Pillar;
}

bool BuildingSquare::HasDoorN() const
{
    return mWallN.door != nullptr && mWallN.door->tile() != nullptr && !mWallN.door->tile()->isNone();
}

bool BuildingSquare::HasDoorS() const
{
    return mWallS.door != nullptr && mWallS.door->tile() != nullptr && !mWallS.door->tile()->isNone();
}

bool BuildingSquare::HasDoorW() const
{
    return mWallW.door != nullptr && mWallW.door->tile() != nullptr && !mWallW.door->tile()->isNone();
}

bool BuildingSquare::HasDoorE() const
{
    return mWallE.door != nullptr && mWallE.door->tile() != nullptr && !mWallE.door->tile()->isNone();
}

bool BuildingSquare::HasDoorFrameN() const
{
    return mWallN.door != nullptr && mWallN.door->frameTile() != nullptr && !mWallN.door->frameTile()->isNone();
}

bool BuildingSquare::HasDoorFrameS() const
{
    return mWallS.door != nullptr && mWallS.door->frameTile() != nullptr && !mWallS.door->frameTile()->isNone();
}

bool BuildingSquare::HasDoorFrameW() const
{
    return mWallW.door != nullptr && mWallW.door->frameTile() != nullptr && !mWallW.door->frameTile()->isNone();
}

bool BuildingSquare::HasDoorFrameE() const
{
    return mWallE.door != nullptr && mWallE.door->frameTile() != nullptr && !mWallE.door->frameTile()->isNone();
}

bool BuildingSquare::HasWindowN() const
{
    return mWallN.window != nullptr && mWallN.window->tile() != nullptr && !mWallN.window->tile()->isNone();
}

bool BuildingSquare::HasWindowS() const
{
    return mWallS.window != nullptr && mWallS.window->tile() != nullptr && !mWallS.window->tile()->isNone();
}

bool BuildingSquare::HasWindowW() const
{
    return mWallW.window != nullptr && mWallW.window->tile() != nullptr && !mWallW.window->tile()->isNone();
}

bool BuildingSquare::HasWindowE() const
{
    return mWallE.window != nullptr && mWallE.window->tile() != nullptr && !mWallE.window->tile()->isNone();
}

bool BuildingSquare::HasCurtainsN() const
{
    return mWallN.window != nullptr && mWallN.window->curtainsTile() != nullptr && !mWallN.window->curtainsTile()->isNone();
}

bool BuildingSquare::HasCurtainsS() const
{
    return mWallS.window != nullptr && mWallS.window->curtainsTile() != nullptr && !mWallS.window->curtainsTile()->isNone();
}

bool BuildingSquare::HasCurtainsW() const
{
    return mWallW.window != nullptr && mWallW.window->curtainsTile() != nullptr && !mWallW.window->curtainsTile()->isNone();
}

bool BuildingSquare::HasCurtainsE() const
{
    return mWallE.window != nullptr && mWallE.window->curtainsTile() != nullptr && !mWallE.window->curtainsTile()->isNone();
}

bool BuildingSquare::HasShuttersN() const
{
    return mWallN.window != nullptr && mWallN.window->shuttersTile() != nullptr && !mWallN.window->shuttersTile()->isNone();
}

bool BuildingSquare::HasShuttersS() const
{
    return mWallS.window != nullptr && mWallS.window->shuttersTile() != nullptr && !mWallS.window->shuttersTile()->isNone();
}

bool BuildingSquare::HasShuttersW() const
{
    return mWallW.window != nullptr && mWallW.window->shuttersTile() != nullptr && !mWallW.window->shuttersTile()->isNone();
}

bool BuildingSquare::HasShuttersE() const
{
    return mWallE.window != nullptr && mWallE.window->shuttersTile() != nullptr && !mWallE.window->shuttersTile()->isNone();
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
    // This is done after doors and windows are placed.
    mEntryEnum[section] = -1; // getWallOffset(wallInfo, section);
}

void BuildingSquare::ReplaceDoor(BuildingTileEntry *tile, SquareSection section, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[section] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[section] = offset;
}

void BuildingSquare::ReplaceFrame(BuildingTileEntry *tile, SquareSection section, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[section] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[section] = offset;
}

void BuildingSquare::ReplaceWindow(BuildingTileEntry *tile, SquareSection section, int offset)
{
    // Must put a non-zero tile here.  See getWallOffset().
    mEntries[section] = tile ? tile : BuildingTilesMgr::instance()->noneTileEntry();
    mEntryEnum[section] = offset;
}

void BuildingSquare::ReplaceCurtains(Window *window, SquareSection section, int offset)
{
    mEntries[section] = window->curtainsTile();
    mEntryEnum[section] = offset;
}

void BuildingSquare::ReplaceShutters(Window *window, bool first)
{
    if (!window->shuttersTile() || window->shuttersTile()->isNone())
        return;

    if (mExterior) {
        // TODO: East and West
        if (window->isN()) {
            ReplaceFurniture(window->shuttersTile()->tile(first ? BTC_Shutters::NorthLeft : BTC_Shutters::NorthRight),
                             SectionWallFurniture, SectionWallFurniture2);
        } else {
            ReplaceFurniture(window->shuttersTile()->tile(first ? BTC_Shutters::WestAbove : BTC_Shutters::WestBelow),
                             SectionWallFurniture, SectionWallFurniture2);
        }
    }
}

void BuildingSquare::ReplaceFurniture(BuildingTileEntry *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mEntries[SectionFurniture] = nullptr;
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

    BuildingTileEntry *wallTile1 = mEntries[SectionWall];
    if (wallTile1 && wallTile1->isNone()) wallTile1 = nullptr;

    BuildingTileEntry *wallTile2 = mEntries[SectionWall2];
    if (wallTile2 && wallTile2->isNone()) wallTile2 = nullptr;

    Q_ASSERT(!(wallTile1 == nullptr && wallTile2 != nullptr));

    int grimeEnumW = -1, grimeEnumN = -1;

    if (wallTile1) {
        switch (mEntryEnum[SectionWall]) {
        case BTC_Walls::West:
        case BTC_Walls::WestWindow:
            grimeEnumW = BTC_GrimeFloor::West;
            break;
        case BTC_Walls::WestDoor:
            break; // no grime by doors
        case BTC_Walls::North:
        case BTC_Walls::NorthWindow:
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::NorthDoor:
            break; // no grime by doors
        case BTC_Walls::NorthWest:
            grimeEnumW = BTC_GrimeFloor::West;
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::SouthEast:
#if 0 // FIXME: this was in the pre-rotation version
            // Hack - ignore "end cap" of a wall object.
            if (mWallOrientation != Square::WallOrientSE)
                break;
#endif
            grimeEnumW = BTC_GrimeFloor::NorthWest;
            break;
        }
    }
    if (wallTile2) {
        switch (mEntryEnum[SectionWall2]) {
        case BTC_Walls::West:
        case BTC_Walls::WestWindow:
            grimeEnumW = BTC_GrimeFloor::West;
            break;
        case BTC_Walls::WestDoor:
            break;
        case BTC_Walls::North:
        case BTC_Walls::NorthWindow:
            grimeEnumN = BTC_GrimeFloor::North;
            break;
        case BTC_Walls::NorthDoor:
            break;
        case BTC_Walls::NorthWest:
            Q_ASSERT(false);
            break;
        case BTC_Walls::SouthEast:
            Q_ASSERT(false);
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

    BuildingTileEntry *tileEntry1 = mEntries[SectionWall];
    if (tileEntry1 && tileEntry1->isNone())
        tileEntry1 = nullptr;

    BuildingTileEntry *tileEntry2 = mEntries[SectionWall2];
    if (tileEntry2 && tileEntry2->isNone())
        tileEntry2 = nullptr;

    Q_ASSERT(!(tileEntry1 == nullptr && tileEntry2 != nullptr));

    BuildingTile *tileW = nullptr, *tileN = nullptr, *tileNW = nullptr, *tileSE = nullptr;
    int grimeEnumW = -1, grimeEnumN = -1;

    if (tileEntry1) {
        BuildingTile *btile = tileEntry1->tile(mEntryEnum[SectionWall]);
        switch (mEntryEnum[SectionWall]) {
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
        BuildingTile *btile = tileEntry2->tile(mEntryEnum[SectionWall2]);
        switch (mEntryEnum[SectionWall2]) {
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
        mEntries[SectionWallGrime] = grimeTile;
        mEntryEnum[SectionWallGrime] = grimeEnumW;
    }
    if (grimeEnumN >= 0) {
        mEntries[SectionWallGrime2] = grimeTile;
        mEntryEnum[SectionWallGrime2] = grimeEnumN;
    }
}

void BuildingSquare::ReplaceWallTrim()
{
    BuildingTileEntry *wallTile1 = mEntries[SectionWall];
    if (wallTile1 && wallTile1->isNone())
        wallTile1 = nullptr;

    BuildingTileEntry *wallTile2 = mEntries[SectionWall2];
    if (wallTile2 && wallTile2->isNone())
        wallTile2 = nullptr;

    if (!wallTile1 && !wallTile2) return;

    BuildingTileEntry *trimW = mWallW.entryTrim;
    BuildingTileEntry *trimN = mWallN.entryTrim;

    // No trim over furniture in the Walls layer
    if (trimW && mWallW.furniture) trimW = nullptr;
    if (trimN && mWallN.furniture) trimN = nullptr;

    if (!trimW && !trimN) return;

    int enumW = -1;
    int enumN = -1;

    // SectionWall could be a west, north, or north-west wall
    if (wallTile1) {
        if (mEntryEnum[SectionWall] == BTC_Walls::West ||
                mEntryEnum[SectionWall] == BTC_Walls::WestDoor ||
                mEntryEnum[SectionWall] == BTC_Walls::WestWindow ||
                mEntryEnum[SectionWall] == BTC_Walls::NorthWest) {
            enumW = mEntryEnum[SectionWall];
            if (enumW == BTC_Walls::NorthWest) {
                enumW = BTC_Walls::West;
                enumN = BTC_Walls::North;
            }
        } else {
            enumN = mEntryEnum[SectionWall];
        }
    }

    // Use the same stacking order as the walls
    int sectionW = SectionWallTrim2;
    int sectionN = SectionWallTrim;

    // 2 different wall tiles forming north-west corner
    if (wallTile2) {
        if (mEntryEnum[SectionWall2] == BTC_Walls::West ||
                mEntryEnum[SectionWall2] == BTC_Walls::WestDoor ||
                mEntryEnum[SectionWall2] == BTC_Walls::WestWindow) {
            enumW = mEntryEnum[SectionWall2];
        } else {
            enumN = mEntryEnum[SectionWall2];
            qSwap(sectionW, sectionN);
        }
    }

    if ((enumW == BTC_Walls::West && enumN == BTC_Walls::North) && (trimW == trimN)) {
        if (sectionN == SectionWallTrim2) {
            enumN = BTC_Walls::NorthWest;
            enumW = -1;
        } else {
            enumW = BTC_Walls::NorthWest;
            enumN = -1;
        }
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

void BuildingSquare::ReplaceWallTrimES()
{
    BuildingTileEntry *wallTile1 = mEntries[SectionWall3];
    if (wallTile1 && wallTile1->isNone()) wallTile1 = nullptr;

    BuildingTileEntry *wallTile2 = mEntries[SectionWall4];
    if (wallTile2 && wallTile2->isNone()) wallTile2 = nullptr;

    if (!wallTile1 && !wallTile2) return;

    BuildingTileEntry *trimE = mWallE.entryTrim;
    BuildingTileEntry *trimS = mWallS.entryTrim;

    // No trim over furniture in the Walls layer
    if (trimE && mWallE.furniture) trimE = nullptr;
    if (trimS && mWallS.furniture) trimS = nullptr;

    if (!trimE && !trimS) return;

    int enumE = -1;
    int enumS = -1;

    if (wallTile1) {
        enumE = mEntryEnum[SectionWall3];
//        if (enumS == BTC_Walls::NorthWest) {
//            enumE = BTC_Walls::West;
//            enumS = BTC_Walls::North;
//        }
    }

    int sectionE = SectionWallTrim3;
    int sectionS = SectionWallTrim4;

    if (wallTile2) {
        enumS = mEntryEnum[SectionWall4];
//        if (enumE == BTC_Walls::NorthWest) {
//            enumE = BTC_Walls::West;
//            enumS = BTC_Walls::North;
//        }
    }

//    if ((enumE == BTC_Walls::West && enumS == BTC_Walls::North) && (trimE == trimS)) {
//        enumS = BTC_Walls::NorthWest;
//        enumE = -1;
//    }

    if (enumE >= 0) {
        mEntries[sectionE] = trimE;
        mEntryEnum[sectionE] = enumE;
    }
    if (enumS >= 0) {
        mEntries[sectionS] = trimS;
        mEntryEnum[sectionS] = enumS;
    }
}
