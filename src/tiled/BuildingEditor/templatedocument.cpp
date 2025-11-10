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

#include "templatedocument.h"

#include "buildingtemplates.h"

using namespace BuildingEditor;

TemplateDocument::TemplateDocument(BuildingTemplate *templ8) :
    mTemplate(templ8)
{

}

TemplateDocument::~TemplateDocument()
{

}

void TemplateDocument::insertRoom(int index, Room *room)
{
    mTemplate->insertRoom(index, room);
    emit roomAdded(room);
}

Room *TemplateDocument::removeRoom(int index)
{
    Room *room = mTemplate->room(index);
    emit roomAboutToBeRemoved(room);
    mTemplate->removeRoom(index);
    emit roomRemoved(room);
    return room;
}

int TemplateDocument::reorderRoom(int index, Room *room)
{
    int oldIndex = mTemplate->rooms().indexOf(room);
    mTemplate->removeRoom(oldIndex);
    mTemplate->insertRoom(index, room);
    emit roomsReordered();
    return oldIndex;
}

Room *TemplateDocument::changeRoom(Room *room, const Room *data)
{
    Room *old = new Room(room);
    room->copy(data);
    emit roomChanged(room);
    delete data;
    return old;
}
