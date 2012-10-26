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

#include "buildingundoredo.h"

#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"

#include <QCoreApplication>

using namespace BuildingEditor;

ChangeRoomAtPosition::ChangeRoomAtPosition(BuildingDocument *doc, BuildingFloor *floor,
                                           const QPoint &pos, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room At Position")),
    mDocument(doc),
    mFloor(floor),
    mMergeable(false)
{
    Changed changed;
    changed.mPosition = pos;
    changed.mRoom = room;
    mChanged += changed;
}

bool ChangeRoomAtPosition::mergeWith(const QUndoCommand *other)
{
    const ChangeRoomAtPosition *o = static_cast<const ChangeRoomAtPosition*>(other);
    if (!(mFloor == o->mFloor &&
            o->mMergeable))
        return false;

    foreach (Changed changedOther, o->mChanged) {
        bool ignore = false;
        foreach (Changed changedSelf, mChanged) {
            if (changedSelf.mPosition == changedOther.mPosition) {
                ignore = true;
                break;
            }
        }
        if (!ignore)
            mChanged += changedOther;
    }

    return true;
}

void ChangeRoomAtPosition::swap()
{
    QVector<Changed> old;

    foreach (Changed changed, mChanged) {
        Changed changed2;
        changed2.mRoom = mDocument->changeRoomAtPosition(mFloor, changed.mPosition, changed.mRoom);
        changed2.mPosition = changed.mPosition;
        old += changed2;
    }

    mChanged = old;
}

/////

PaintRoom::PaintRoom(BuildingDocument *doc, BuildingFloor *floor, const QPoint &pos, Room *room) :
    ChangeRoomAtPosition(doc, floor, pos, room)
{
    setText(QCoreApplication::translate("Undo Commands", "Paint Room"));
}

EraseRoom::EraseRoom(BuildingDocument *doc, BuildingFloor *floor, const QPoint &pos) :
    ChangeRoomAtPosition(doc, floor, pos, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Erase Room"));
}

/////

ChangeEWall::ChangeEWall(BuildingDocument *doc, BuildingTile *tile) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change External Wall")),
    mDocument(doc),
    mTile(tile)
{
}

void ChangeEWall::swap()
{
    mTile = mDocument->changeEWall(mTile);
}

/////

ChangeWallForRoom::ChangeWallForRoom(BuildingDocument *doc, Room *room, BuildingTile *tile) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room's Wall")),
    mDocument(doc),
    mRoom(room),
    mTile(tile)
{
}

void ChangeWallForRoom::swap()
{
    mTile = mDocument->changeWallForRoom(mRoom, mTile);
}

/////

ChangeFloorForRoom::ChangeFloorForRoom(BuildingDocument *doc, Room *room, BuildingTile *tile) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room's Floor")),
    mDocument(doc),
    mRoom(room),
    mTile(tile)
{
}

void ChangeFloorForRoom::swap()
{
    mTile = mDocument->changeFloorForRoom(mRoom, mTile);
}

/////

AddRemoveObject::AddRemoveObject(BuildingDocument *doc, BuildingFloor *floor,
                                 int index, BaseMapObject *object) :
    QUndoCommand(),
    mDocument(doc),
    mFloor(floor),
    mIndex(index),
    mObject(object)
{
}

AddRemoveObject::~AddRemoveObject()
{
    delete mObject;
}

void AddRemoveObject::add()
{
    mDocument->insertObject(mFloor, mIndex, mObject);
    mObject = 0;
}

void AddRemoveObject::remove()
{
    mObject = mDocument->removeObject(mFloor, mIndex);
}

AddObject::AddObject(BuildingDocument *doc, BuildingFloor *floor, int index,
                     BaseMapObject *object) :
    AddRemoveObject(doc, floor, index, object)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Object"));
}

RemoveObject::RemoveObject(BuildingDocument *doc, BuildingFloor *floor,
                           int index) :
    AddRemoveObject(doc, floor, index, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Object"));
}

/////

MoveObject::MoveObject(BuildingDocument *doc, BaseMapObject *object, const QPoint &pos) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Move Object")),
    mDocument(doc),
    mObject(object),
    mPos(pos)
{
}

void MoveObject::swap()
{
    mPos = mDocument->moveObject(mObject, mPos);
}

/////

ChangeDoorTile::ChangeDoorTile(BuildingDocument *doc, Door *door,
                               BuildingTile *tile, bool isFrame) :
    QUndoCommand(QCoreApplication::translate("Undo Commands",
                                             isFrame ? "Change Door Frame Tile"
                                                     : "Change Door Tile")),
    mDocument(doc),
    mDoor(door),
    mTile(tile),
    mIsFrame(isFrame)
{
}

void ChangeDoorTile::swap()
{
    mTile = mDocument->changeDoorTile(mDoor, mTile, mIsFrame);
}

/////

ChangeObjectTile::ChangeObjectTile(BuildingDocument *doc, BaseMapObject *object,
                               BuildingTile *tile) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Window Tile")),
    mDocument(doc),
    mObject(object),
    mTile(tile)
{
}

void ChangeObjectTile::swap()
{
    mTile = mDocument->changeObjectTile(mObject, mTile);
}

/////

AddRemoveRoom::AddRemoveRoom(BuildingDocument *doc, int index, Room *room) :
    QUndoCommand(),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

AddRemoveRoom::~AddRemoveRoom()
{
    delete mRoom;
}

void AddRemoveRoom::add()
{
    mDocument->insertRoom(mIndex, mRoom);
    mRoom = 0;
}

void AddRemoveRoom::remove()
{
    mRoom = mDocument->removeRoom(mIndex);
}

AddRoom::AddRoom(BuildingDocument *doc, int index, Room *room) :
    AddRemoveRoom(doc, index, room)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Room"));
}

RemoveRoom::RemoveRoom(BuildingDocument *doc, int index) :
    AddRemoveRoom(doc, index, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Room"));
}

/////

ReorderRoom::ReorderRoom(BuildingDocument *doc, int index, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Rooms")),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

void ReorderRoom::swap()
{
    mIndex = mDocument->reorderRoom(mIndex, mRoom);
}

/////

ChangeRoom::ChangeRoom(BuildingDocument *doc, Room *room, const Room *data) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room")),
    mDocument(doc),
    mRoom(room),
    mData(new Room(data))
{
}

ChangeRoom::~ChangeRoom()
{
    delete mData;
}

void ChangeRoom::swap()
{
    mData = mDocument->changeRoom(mRoom, mData);
}

/////

SwapFloorGrid::SwapFloorGrid(BuildingDocument *doc, BuildingFloor *floor,
                             const QVector<QVector<Room *> > &grid) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Swap Floor Grid")),
    mDocument(doc),
    mFloor(floor),
    mGrid(grid)
{
}

void SwapFloorGrid::swap()
{
    mGrid = mDocument->swapFloorGrid(mFloor, mGrid);
}

/////

RotateBuilding::RotateBuilding(BuildingDocument *doc, bool right) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Rotate Building")),
    mDocument(doc),
    mRight(right)
{
}

void RotateBuilding::swap()
{
    mDocument->rotateBuilding(mRight);
    mRight = !mRight;
}


/////
