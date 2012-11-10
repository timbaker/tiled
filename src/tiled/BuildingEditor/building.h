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
#include <QSize>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTemplate;
class BuildingTile;
class Room;

class Building
{
public:
    Building(int width, int height, BuildingTemplate *btemplate = 0);

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    QSize size() const { return QSize(mWidth, mHeight); }

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

    BuildingTile *exteriorWall() const
    { return mExteriorWall; }

    void setExteriorWall(BuildingTile *tileName)
    { mExteriorWall = tileName; }

    BuildingTile *doorTile() const
    { return mDoorTile; }

    void setDoorTile(BuildingTile *tileName)
    { mDoorTile = tileName; }

    BuildingTile *doorFrameTile() const
    { return mDoorFrameTile; }

    void setDoorFrameTile(BuildingTile *tileName)
    { mDoorFrameTile = tileName; }

    BuildingTile *windowTile() const
    { return mWindowTile; }

    void setWindowTile(BuildingTile *tileName)
    { mWindowTile = tileName; }

    BuildingTile *curtainsTile() const
    { return mCurtainsTile; }

    void setCurtainsTile(BuildingTile *tileName)
    { mCurtainsTile = tileName; }

    BuildingTile *stairsTile() const
    { return mStairsTile; }

    void setStairsTile(BuildingTile *tileName)
    { mStairsTile = tileName; }

    void resize(const QSize &newSize);
    void rotate(bool right);
    void flip(bool horizontal);

private:
    int mWidth, mHeight;
    QList<BuildingFloor*> mFloors;
    QList<Room*> mRooms;
    BuildingTile *mExteriorWall;
    BuildingTile *mDoorTile;
    BuildingTile *mDoorFrameTile;
    BuildingTile *mWindowTile;
    BuildingTile *mCurtainsTile;
    BuildingTile *mStairsTile;
};

} // namespace BuildingEditor

#endif // BUILDING_H
