/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "roomdefecator.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include "map.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;
using namespace Tiled::Internal;
using namespace BuildingEditor;

RoomDefecator::RoomDefecator(Map *map, int level, const QRect &bounds) :
    mMap(map),
    mBounds(bounds),
    mLayerFloor(0),
    mLayerWalls(0),
    mLayerWalls2(0)
{
    int i = map->indexOfLayer(QString::fromLatin1("%1_Floor").arg(level), Layer::TileLayerType);
    if (i < 0)
        return;
    mLayerFloor = map->layerAt(i)->asTileLayer();

    i = map->indexOfLayer(QString::fromLatin1("%1_Walls").arg(level), Layer::TileLayerType);
    if (i < 0)
        return;
    mLayerWalls = map->layerAt(i)->asTileLayer();

    i = map->indexOfLayer(QString::fromLatin1("%1_Walls2").arg(level), Layer::TileLayerType);
    if (i >= 0)
        mLayerWalls2 = map->layerAt(i)->asTileLayer();

    i = map->indexOfLayer(QString::fromLatin1("%1_Walls_2").arg(level), Layer::TileLayerType);
    if (i >= 0)
        mLayerWalls2 = map->layerAt(i)->asTileLayer();


    initTiles();
}

void RoomDefecator::defecate()
{
    if (!mLayerFloor || !mLayerWalls)
        return;

    for (int y = mBounds.top(); y <= mBounds.bottom(); y++) {
        for (int x = mBounds.left(); x <= mBounds.right(); x++) {
            if (!isValidPos(x, y))
                continue;
            if (!didTile(x, y) /*&& isInRoom(x, y)*/) {
                addTile(x, y);
            }
        }
    }
}

void RoomDefecator::initTiles()
{
    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
    QList<BuildingTileCategory*> categories;
    categories << btiles->catEWalls()
               << btiles->catIWalls();

    foreach (BuildingTileCategory *cat, categories) {
        foreach (BuildingTileEntry *entry, cat->entries()) {
            mWestWallTiles += btiles->tileFor(entry->tile(BTC_Walls::West));
            mWestWallTiles += btiles->tileFor(entry->tile(BTC_Walls::WestDoor));
            mWestWallTiles += btiles->tileFor(entry->tile(BTC_Walls::WestWindow));
            mWestWallTiles += btiles->tileFor(entry->tile(BTC_Walls::NorthWest));

            mNorthWallTiles += btiles->tileFor(entry->tile(BTC_Walls::North));
            mNorthWallTiles += btiles->tileFor(entry->tile(BTC_Walls::NorthDoor));
            mNorthWallTiles += btiles->tileFor(entry->tile(BTC_Walls::NorthWindow));
            mNorthWallTiles += btiles->tileFor(entry->tile(BTC_Walls::NorthWest));

            // These are for the benefit of MainWindow::RoomDefUnknownWalls
            mSouthEastWallTiles += btiles->tileFor(entry->tile(BTC_Walls::SouthEast));
        }

        foreach (FurnitureGroup *fg, FurnitureGroups::instance()->groups()) {
            foreach (FurnitureTiles *ftiles, fg->mTiles) {
                if (ftiles->layer() == FurnitureTiles::LayerWalls) {
                    foreach (FurnitureTile *ftile, ftiles->tiles()) {
                        if (!ftile || ftile->isCornerOrient(ftile->orient()))
                            continue;
                        foreach (BuildingTile *btile, ftile->tiles()) {
                            if (btile && !btile->isNone()) {
                                if (ftile->isW() || ftile->isE())
                                    mWestWallTiles += btiles->tileFor(btile);
                                else if (ftile->isN() || ftile->isS())
                                    mNorthWallTiles += btiles->tileFor(btile);
                            }
                        }
                    }
                }
            }
        }
    }

    QSet<Tileset*> mapTilesets = mMap->usedTilesets();
    foreach (Tileset *ts, mapTilesets) {
        mTilesets[ts->name()] = ts;
    }

    QSet<Tile*> westTiles, northTiles, southEastTiles;
    foreach (Tile *tile, mWestWallTiles) {
        if (mTilesets.contains(tile->tileset()->name())) {
            if (Tile *tile2 = mTilesets[tile->tileset()->name()]->tileAt(tile->id()))
                westTiles += tile2;
        }
    }
    foreach (Tile *tile, mNorthWallTiles) {
        if (mTilesets.contains(tile->tileset()->name())) {
            if (Tile *tile2 = mTilesets[tile->tileset()->name()]->tileAt(tile->id()))
                northTiles += tile2;
        }
    }
    foreach (Tile *tile, mSouthEastWallTiles) {
        if (mTilesets.contains(tile->tileset()->name())) {
            if (Tile *tile2 = mTilesets[tile->tileset()->name()]->tileAt(tile->id()))
                southEastTiles += tile2;
        }
    }
    mWestWallTiles = westTiles;
    mNorthWallTiles = northTiles;
    mSouthEastWallTiles = southEastTiles;
}

bool RoomDefecator::didTile(int x, int y)
{
    foreach (QRegion rgn, mRegions + mIgnoreRegions) {
        if (rgn.contains(QPoint(x, y)))
            return true;
    }
    return false;
}

bool RoomDefecator::isInRoom(int x, int y)
{
    Tile *tile = mLayerFloor->cellAt(x, y).tile;
    if (!tile) return false;

    // check for wall to north and south
    bool wallN = false, wallS = false;
    int yy = y;
    while (!wallN && isValidPos(x, yy)) {
        if (/*didTile(x, yy) ||*/ isNorthWall(x, yy) /*mLayerWalls->cellAt(x, yy).tile*/)
            wallN = true;
        --yy;
    }
    yy = y + 1;
    while (!wallS && isValidPos(x, yy)) {
        if (/*didTile(x, yy) || */isNorthWall(x, yy) /*mLayerWalls->cellAt(x, yy).tile*/)
            wallS = true;
        ++yy;
    }
    bool wallW = false, wallE = false;
    int xx = x;
    while (!wallW && isValidPos(xx, y)) {
        if (/*didTile(x, yy) ||*/ isWestWall(xx, y) /*mLayerWalls->cellAt(x, yy).tile*/)
            wallW = true;
        --xx;
    }
    xx = x + 1;
    while (!wallE && isValidPos(xx, y)) {
        if (/*didTile(x, yy) || */isWestWall(xx, y) /*mLayerWalls->cellAt(x, yy).tile*/)
            wallE = true;
        ++xx;
    }

    return wallW && wallE && wallN && wallS;
}

bool RoomDefecator::shouldVisit(int x1, int y1, int x2, int y2)
{
    if (isValidPos(x1, y1) && isValidPos(x2, y2) && !didTile(x2, y2)) {
/*        if (!mLayerFloor->cellAt(x2, y2).tile)
            return false;*/
        if (mLayerFloor->cellAt(x1, y1).tile != mLayerFloor->cellAt(x2, y2).tile)
            return false; // different floor tile
        if ((x2 < x1) && isWestWall(x1, y1))
            return false; // west wall between
        if ((x2 > x1) && isWestWall(x2, y2))
            return false; // west wall between
        if ((y2 < y1) && isNorthWall(x1, y1))
            return false; // north wall between
        if ((y2 > y1) && isNorthWall(x2, y2))
            return false; // north wall between
        return true;
    }
    return false;
}

void RoomDefecator::addTile(int x, int y)
{
    RoomDefecatorFill fill(this);
    fill.floodFillScanlineStack(x, y);
    // Ignore the region if cells touching an edge of the map/bounds have no walls.
    // This will strip away any "outer" region.
    foreach (QRect r, fill.mRegion.rects()) {
#if 1
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (!isInRoom(x, y)) {
                    mIgnoreRegions += fill.mRegion;
                    return;
                }
            }
        }
#else
        if (r.left() == 0 || r.left() == mBounds.left()) {
            for (int y = r.top(); y <= r.bottom(); y++) {
                if (!isWestWall(r.left(), y)) {
                    mIgnoreRegions += fill.mRegion;
                    return;
                }
            }
        }
        if (r.right() + 1 == mMap->width() || r.right() == mBounds.right()) {
            for (int y = r.top(); y <= r.bottom(); y++) {
                if (!isWestWall(r.right() + 1, y)) {
                    mIgnoreRegions += fill.mRegion;
                    return;
                }
            }
        }
        if (r.top() == 0 || r.top() == mBounds.top()) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (!isNorthWall(x, r.top())) {
                    mIgnoreRegions += fill.mRegion;
                    return;
                }
            }
        }
        if (r.bottom() + 1 == mMap->height() || r.bottom() == mBounds.bottom()) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (!isNorthWall(x, r.bottom() + 1)) {
                    mIgnoreRegions += fill.mRegion;
                    return;
                }
            }
        }
#endif
    }

    if (!fill.mRegion.isEmpty())
        mRegions += fill.mRegion;
}

bool RoomDefecator::isValidPos(int x, int y)
{
    return mBounds.contains(x, y) &&
            QRect(0, 0, mMap->width(), mMap->height()).contains(x, y);
}

bool RoomDefecator::isWestWall(int x, int y)
{
    if (!isValidPos(x, y)) return false;
    Tile *tile = mLayerWalls->cellAt(x, y).tile;
    if (tile && mWestWallTiles.contains(tile)) return true;
    if (mLayerWalls2) {
        Tile *tile = mLayerWalls2->cellAt(x, y).tile;
        if (tile && mWestWallTiles.contains(tile)) return true;
    }
    return false;
}

bool RoomDefecator::isNorthWall(int x, int y)
{
    if (!isValidPos(x, y)) return false;
    Tile *tile = mLayerWalls->cellAt(x, y).tile;
    if (tile && mNorthWallTiles.contains(tile)) return true;
    if (mLayerWalls2) {
        Tile *tile = mLayerWalls2->cellAt(x, y).tile;
        if (tile && mNorthWallTiles.contains(tile)) return true;
    }
    return false;
}

/////

RoomDefecatorFill::RoomDefecatorFill(RoomDefecator *rd) :
    mRoomDefecator(rd)
{

}

// Taken from http://lodev.org/cgtutor/floodfill.html#Recursive_Scanline_Floodfill_Algorithm
// Copyright (c) 2004-2007 by Lode Vandevenne. All rights reserved.
void RoomDefecatorFill::floodFillScanlineStack(int x, int y)
{
    emptyStack();

    int y1;
    bool spanLeft, spanRight;

    if (!push(x, y)) return;

    while (pop(x, y)) {
        y1 = y;
        while (shouldVisit(x, y1, x, y1 - 1))
            y1--;
        spanLeft = spanRight = false;
        QRect r;
        int py = y;
        while (shouldVisit(x, py, x, y1)) {
            mRegion += QRect(x, y1, 1, 1);
            if (!spanLeft && x > 0 && shouldVisit(x, y1, x - 1, y1)) {
                if (!push(x - 1, y1)) return;
                spanLeft = true;
            }
            else if (spanLeft && x > 0 && !shouldVisit(x, y1, x - 1, y1)) {
                spanLeft = false;
            } else if (spanLeft && x > 0 && !shouldVisit(x - 1, y1 - 1, x - 1, y1)) {
                // North wall splits the span.
                if (!push(x - 1, y1)) return;
            }
            if (!spanRight && (x < mRoomDefecator->mMap->width() - 1) && shouldVisit(x, y1, x + 1, y1)) {
                if (!push(x + 1, y1)) return;
                spanRight = true;
            }
            else if (spanRight && (x < mRoomDefecator->mMap->width() - 1) && !shouldVisit(x, y1, x + 1, y1)) {
                spanRight = false;
            } else if (spanRight && (x < mRoomDefecator->mMap->width() - 1) && !shouldVisit(x + 1, y1 - 1, x + 1, y1)) {
                // North wall splits the span.
                if (!push(x + 1, y1)) return;
            }
            py = y1;
            y1++;
        }
        if (!r.isEmpty())
            mRegion += r;
    }
}

bool RoomDefecatorFill::shouldVisit(int x1, int y1, int x2, int y2)
{
    return mRoomDefecator->shouldVisit(x1, y1, x2, y2)
            && !mRegion.contains(QPoint(x2, y2));
}

bool RoomDefecatorFill::push(int x, int y)
{
    if (stackPointer < STACK_SIZE - 1) {
        stackPointer++;
        stack[stackPointer] = mRoomDefecator->mMap->height() * x + y;
        return true;
    } else {
        return false;
    }
}

bool RoomDefecatorFill::pop(int &x, int &y)
{
    if (stackPointer > 0) {
        int p = stack[stackPointer];
        x = p / mRoomDefecator->mMap->height();
        y = p % mRoomDefecator->mMap->height();
        stackPointer--;
        return true;
    } else {
        return false;
    }
}

void RoomDefecatorFill::emptyStack()
{
    stackPointer = 0;
}
