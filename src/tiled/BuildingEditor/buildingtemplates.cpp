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

#include "buildingeditorwindow.h"
#include "buildingtemplates.h"

using namespace BuildingEditor;

/////

QList<BuildingTemplate*> BuildingTemplate::mTemplates;
QMap<QString,BuildingTemplate*> BuildingTemplate::DefinitionMap;

/////

#if 0

RoomDefinitionManager *RoomDefinitionManager::instance = new RoomDefinitionManager;


void RoomDefinitionManager::Init(BuildingTemplate *definition)
{
    mBuildingDefinition = definition;
    ColorToRoom.clear();
    RoomToColor.clear();

    ExteriorWall = definition->Wall;
    mDoorTile = QLatin1String("fixtures_doors_01_0");
    mDoorFrameTile = QLatin1String("fixtures_doors_frames_01_0");
    mWindowTile = QLatin1String("fixtures_windows_01_0");
    mStairsTile = QLatin1String("fixtures_stairs_01_0");

    foreach (Room *room, definition->RoomList) {
        ColorToRoom[room->Color] = room;
        RoomToColor[room] = room->Color;
    }
}

QStringList RoomDefinitionManager::FillCombo()
{
    QStringList roomNames;
    foreach (Room *room, mBuildingDefinition->RoomList)
        roomNames += room->Name;
    return roomNames;
}

int RoomDefinitionManager::GetIndex(QRgb col)
{
    return mBuildingDefinition->RoomList.indexOf(ColorToRoom[col]);
}

int RoomDefinitionManager::GetIndex(Room *room)
{
    return mBuildingDefinition->RoomList.indexOf(room);
}

Room *RoomDefinitionManager::getRoom(int index)
{
    if (index < 0)
        return 0;
    return mBuildingDefinition->RoomList.at(index);
}

int RoomDefinitionManager::getRoomCount()
{
    return mBuildingDefinition->RoomList.count();
}

#endif
