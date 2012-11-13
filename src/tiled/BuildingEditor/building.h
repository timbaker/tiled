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

#ifndef BUILDING_H
#define BUILDING_H

#include <QList>
#include <QRect>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTemplate;
class BuildingTileEntry;
class Room;

class Building
{
public:
    Building(int width, int height, BuildingTemplate *btemplate = 0);

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QSize size() const { return QSize(mWidth, mHeight); }

    QRect bounds() const { return QRect(0, 0, mWidth, mHeight); }

    const QList<BuildingFloor*> &floors() const
    { return mFloors; }

    BuildingFloor *floor(int index)
    { return mFloors.at(index); }

    int floorCount() const
    { return mFloors.count(); }

    void insertFloor(int index, BuildingFloor *floor);
    BuildingFloor *removeFloor(int index);

    const QList<Room*> &rooms() const
    { return mRooms; }

    Room *room(int index) const
    { return mRooms.at(index); }

    int roomCount() const
    { return mRooms.count(); }

    int indexOf(Room *room) const
    { return mRooms.indexOf(room); }

    void insertRoom(int index, Room *room);
    Room *removeRoom(int index);

    BuildingTileEntry *exteriorWall() const
    { return mExteriorWall; }

    void setExteriorWall(BuildingTileEntry *entry)
    { mExteriorWall = entry; }

    BuildingTileEntry *doorTile() const
    { return mDoorTile; }

    void setDoorTile(BuildingTileEntry *entry)
    { mDoorTile = entry; }

    BuildingTileEntry *doorFrameTile() const
    { return mDoorFrameTile; }

    void setDoorFrameTile(BuildingTileEntry *entry)
    { mDoorFrameTile = entry; }

    BuildingTileEntry *windowTile() const
    { return mWindowTile; }

    void setWindowTile(BuildingTileEntry *entry)
    { mWindowTile = entry; }

    BuildingTileEntry *curtainsTile() const
    { return mCurtainsTile; }

    void setCurtainsTile(BuildingTileEntry *entry)
    { mCurtainsTile = entry; }

    BuildingTileEntry *stairsTile() const
    { return mStairsTile; }

    void setStairsTile(BuildingTileEntry *entry)
    { mStairsTile = entry; }

    void resize(const QSize &newSize);
    void rotate(bool right);
    void flip(bool horizontal);

private:
    int mWidth, mHeight;
    QList<BuildingFloor*> mFloors;
    QList<Room*> mRooms;
    BuildingTileEntry *mExteriorWall;
    BuildingTileEntry *mDoorTile;
    BuildingTileEntry *mDoorFrameTile;
    BuildingTileEntry *mWindowTile;
    BuildingTileEntry *mCurtainsTile;
    BuildingTileEntry *mStairsTile;
    BuildingTileEntry *mRoofCap;
    BuildingTileEntry *mRoofSlope;
};

} // namespace BuildingEditor

#endif // BUILDING_H
