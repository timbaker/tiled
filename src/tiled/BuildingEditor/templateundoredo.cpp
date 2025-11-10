/*
 * Copyright 2025, Tim Baker <treectrl@users.sf.net>
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

#include "templateundoredo.h"

#include "buildingtemplates.h"
#include "templatedocument.h"

#include <QCoreApplication>

using namespace BuildingEditor;

/////

TemplateAddRemoveRoom::TemplateAddRemoveRoom(TemplateDocument *doc, int index, Room *room) :
    QUndoCommand(),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

TemplateAddRemoveRoom::~TemplateAddRemoveRoom()
{
    delete mRoom;
}

void TemplateAddRemoveRoom::add()
{
    mDocument->insertRoom(mIndex, mRoom);
    mRoom = 0;
}

void TemplateAddRemoveRoom::remove()
{
    mRoom = mDocument->removeRoom(mIndex);
}

TemplateAddRoom::TemplateAddRoom(TemplateDocument *doc, int index, Room *room) :
    TemplateAddRemoveRoom(doc, index, room)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Room"));
}

TemplateRemoveRoom::TemplateRemoveRoom(TemplateDocument *doc, int index) :
    TemplateAddRemoveRoom(doc, index, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Room"));
}

/////

TemplateReorderRoom::TemplateReorderRoom(TemplateDocument *doc, int index, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Rooms")),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

void TemplateReorderRoom::swap()
{
    mIndex = mDocument->reorderRoom(mIndex, mRoom);
}

/////

TemplateChangeRoom::TemplateChangeRoom(TemplateDocument *doc, Room *room, const Room *data, Change change, int tileIndex) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room")),
    mDocument(doc),
    mChange(change),
    mTileIndex(tileIndex),
    mRoom(room),
    mData(new Room(data))
{
}

TemplateChangeRoom::~TemplateChangeRoom()
{
    delete mData;
}

int TemplateChangeRoom::id() const
{
    return static_cast<int>(TemplateUndoRedo::UndoCmd_ChangeRoom);
}

bool TemplateChangeRoom::mergeWith(const QUndoCommand *other)
{
    if (other->id() != id()) {
        return false;
    }
    if (mChange == Change::Tile) {
        return false;
    }
    const TemplateChangeRoom *other1 = static_cast<const TemplateChangeRoom*>(other);
    if (other1->mRoom != mRoom || other1->mChange != mChange || other1->mTileIndex != mTileIndex) {
        return false;
    }
    // other->redo() was called to change the room, we don't need to udpate mData here, as that is the original state of mRoom.
    return true;
}

void TemplateChangeRoom::swap()
{
    mData = mDocument->changeRoom(mRoom, mData);
}

/////
