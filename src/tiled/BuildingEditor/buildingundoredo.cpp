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

ChangeEWall::ChangeEWall(BuildingDocument *doc, const QString &tileName) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change External Wall")),
    mDocument(doc),
    mTileName(tileName)
{
}

void ChangeEWall::swap()
{
    mTileName = mDocument->changeEWall(mTileName);
}

/////

ChangeWallForRoom::ChangeWallForRoom(BuildingDocument *doc, Room *room, const QString &tileName) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room's Wall")),
    mDocument(doc),
    mRoom(room),
    mTileName(tileName)
{
}

void ChangeWallForRoom::swap()
{
    mTileName = mDocument->changeWallForRoom(mRoom, mTileName);
}

/////

ChangeFloorForRoom::ChangeFloorForRoom(BuildingDocument *doc, Room *room, const QString &tileName) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room's Floor")),
    mDocument(doc),
    mRoom(room),
    mTileName(tileName)
{
}

void ChangeFloorForRoom::swap()
{
    mTileName = mDocument->changeFloorForRoom(mRoom, mTileName);
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

