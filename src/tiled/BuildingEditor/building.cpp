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

#include "building.h"

#include "buildingfloor.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"

using namespace BuildingEditor;

Building::Building(int width, int height, BuildingTemplate *btemplate) :
    mWidth(width),
    mHeight(height)
{
    if (btemplate) {
        mExteriorWall = btemplate->Wall;
        mDoorTile = btemplate->DoorTile;
        mDoorFrameTile = btemplate->DoorFrameTile;
        mWindowTile = btemplate->WindowTile;
        mStairsTile = btemplate->StairsTile;
        foreach (Room *room, btemplate->RoomList)
            insertRoom(mRooms.count(), new Room(room));
    } else {
        mExteriorWall = BuildingTiles::instance()->defaultExteriorWall();
        mDoorTile = BuildingTiles::instance()->defaultDoorTile();
        mDoorFrameTile = BuildingTiles::instance()->defaultDoorFrameTile();
        mWindowTile = BuildingTiles::instance()->defaultWindowTile();
        mStairsTile = BuildingTiles::instance()->defaultStairsTile();
    }
}

void Building::insertFloor(int index, BuildingFloor *floor)
{
    mFloors.insert(index, floor);
}

BuildingFloor *Building::removeFloor(int index)
{
    return mFloors.takeAt(index);
}

void Building::insertRoom(int index, Room *room)
{
    mRooms.insert(index, room);
}

Room *Building::removeRoom(int index)
{
    return mRooms.takeAt(index);
}

void Building::rotate(bool right)
{
    qSwap(mWidth, mHeight);
}
