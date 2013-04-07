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

#ifndef ROOMDEFECATOR_H
#define ROOMDEFECATOR_H

#include <QMap>
#include <QRegion>
#include <QSet>
#include <QVector>

namespace Tiled {
class Map;
class Tile;
class TileLayer;
class Tileset;

namespace Internal {

class RoomDefecator;

class RoomDefecatorFill
{
public:
    RoomDefecatorFill(RoomDefecator *rd);
    void floodFillScanlineStack(int x, int y);
    QVector<QRect> cleanupRegion();
    bool shouldVisit(int x1, int y1, int x2, int y2);
    bool shouldVisit(int x1, int y1);
    bool push(int x, int y);
    bool pop(int &x, int &y);
    void emptyStack();

    RoomDefecator *mRoomDefecator;
    QRegion mRegion;
    enum { STACK_SIZE = 300 * 300 };
    int stack[STACK_SIZE];
    int stackPointer;
};

class RoomDefecator
{
public:
    RoomDefecator(Map *map, int level, const QRect &bounds);
    void initTiles();
    bool didTile(int x, int y);
    bool isInRoom(int x, int y);
    bool shouldVisit(int x1, int y1, int x2, int y2);
    void addTile(int x, int y);
    bool isValidPos(int x, int y);
    bool isWestWall(int x, int y);
    bool isNorthWall(int x, int y);

    Map *mMap;
    TileLayer *mLayerFloor;
    TileLayer *mLayerWalls;
    QRect mBounds;
    QList<QRegion> mRegions;
    QList<QRegion> mIgnoreRegions;
    QMap<QString,Tileset*> mTilesets;
    QSet<Tile*> mWestWallTiles;
    QSet<Tile*> mNorthWallTiles;
};

} // namespace Internal
} // namespace Tiled;

#endif // ROOMDEFECATOR_H
