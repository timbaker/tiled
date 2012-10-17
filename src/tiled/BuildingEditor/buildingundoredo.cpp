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

#include <QCoreApplication>

using namespace BuildingEditor;

ChangeRoomAtPosition::ChangeRoomAtPosition(BuildingDocument *doc, BuildingFloor *floor,
                                           const QPoint &pos, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room At Position")),
    mDocument(doc),
    mFloor(floor),
    mPosition(pos),
    mRoom(room)
{
}

void ChangeRoomAtPosition::swap()
{
    mRoom = mDocument->changeRoomAtPosition(mFloor, mPosition, mRoom);
}

