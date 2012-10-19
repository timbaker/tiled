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

#include "buildingdocument.h"

#include "building.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"

#include <QUndoStack>

using namespace BuildingEditor;

BuildingDocument::BuildingDocument(Building *building, const QString &fileName) :
    QObject(),
    mBuilding(building),
    mUndoStack(new QUndoStack(this))
{
}

void BuildingDocument::setCurrentFloor(BuildingFloor *floor)
{
    mCurrentFloor = floor;
    emit currentFloorChanged();
}

void BuildingDocument::setSelectedObjects(const QSet<BaseMapObject *> &selection)
{
    mSelectedObjects = selection;
    emit selectedObjectsChanged();
}

Room *BuildingDocument::changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room)
{
    Layout *layout = floor->layout();
    int old = layout->grid[pos.x()][pos.y()];
    layout->grid[pos.x()][pos.y()] = room
            ? RoomDefinitionManager::instance->GetIndex(room)
            : -1;
    emit roomAtPositionChanged(floor, pos);
    return (old >= 0) ? RoomDefinitionManager::instance->getRoom(old) : 0;
}

QString BuildingDocument::changeEWall(const QString &tileName)
{
    QString old = RoomDefinitionManager::instance->ExteriorWall;
    RoomDefinitionManager::instance->ExteriorWall = tileName;
    emit roomDefinitionChanged();
    return old;
}

QString BuildingDocument::changeWallForRoom(Room *room, const QString &tileName)
{
    QString old = room->Wall;
    RoomDefinitionManager::instance->setWallForRoom(room, tileName);
    emit roomDefinitionChanged();
    return old;
}

QString BuildingDocument::changeFloorForRoom(Room *room, const QString &tileName)
{
    QString old = room->Floor;
    RoomDefinitionManager::instance->setFloorForRoom(room, tileName);
    emit roomDefinitionChanged();
    return old;
}

void BuildingDocument::insertFloor(int index, BuildingFloor *floor)
{
    building()->insertFloor(index, floor);
    emit floorAdded(floor);
}

void BuildingDocument::insertObject(BuildingFloor *floor, int index, BaseMapObject *object)
{
    Q_ASSERT(object->floor() == floor);
    floor->insertObject(index, object);
    emit objectAdded(object);
}

BaseMapObject *BuildingDocument::removeObject(BuildingFloor *floor, int index)
{
    BaseMapObject *object = floor->object(index);

    if (mSelectedObjects.contains(object)) {
        mSelectedObjects.remove(object);
        emit selectedObjectsChanged();
    }

    emit objectAboutToBeRemoved(object);
    floor->removeObject(index);
    emit objectRemoved(floor, index);
    return object;
}

QPoint BuildingDocument::moveObject(BaseMapObject *object, const QPoint &pos)
{
    QPoint old = object->pos();
    object->setPos(pos);
    emit objectMoved(object);
    return old;
}

BuildingTile *BuildingDocument::changeDoorTile(Door *door, BuildingTile *tile)
{
    BuildingTile *old = door->mDoorTile;
    door->mDoorTile = tile;
    emit doorTileChanged(door);
    return old;
}
