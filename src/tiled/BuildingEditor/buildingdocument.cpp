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
#include "buildingobjects.h"
#include "buildingreader.h"
#include "buildingtemplates.h"
#include "buildingwriter.h"

#include <QMessageBox>
#include <QUndoStack>

using namespace BuildingEditor;

BuildingDocument::BuildingDocument(Building *building, const QString &fileName) :
    QObject(),
    mBuilding(building),
    mFileName(fileName),
    mUndoStack(new QUndoStack(this))
{
}

BuildingDocument *BuildingDocument::read(const QString &fileName, QString &error)
{
    BuildingReader reader;
    if (Building *building = reader.read(fileName)) {
        BuildingDocument *doc = new BuildingDocument(building, fileName);
        return doc;
    }
    error = reader.errorString();
    return 0;
}

bool BuildingDocument::write(const QString &fileName, QString &error)
{
    BuildingWriter w;
    if (!w.write(mBuilding, fileName)) {
        error = w.errorString();
        return false;
    }
    mFileName = fileName;
    mUndoStack->setClean();
    return true;
}

void BuildingDocument::setCurrentFloor(BuildingFloor *floor)
{
    mCurrentFloor = floor;
    emit currentFloorChanged();
}

bool BuildingDocument::isModified() const
{
    return !mUndoStack->isClean();
}

void BuildingDocument::setSelectedObjects(const QSet<BuildingObject *> &selection)
{
    mSelectedObjects = selection;
    emit selectedObjectsChanged();
}

Room *BuildingDocument::changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room)
{
    Room *old = floor->GetRoomAt(pos);
    floor->SetRoomAt(pos, room);
    emit roomAtPositionChanged(floor, pos);
    return old;
}

BuildingTile *BuildingDocument::changeEWall(BuildingTile *tile)
{
    BuildingTile *old = mBuilding->exteriorWall();
    mBuilding->setExteriorWall(tile);
    emit roomDefinitionChanged();
    return old;
}

BuildingTile *BuildingDocument::changeWallForRoom(Room *room, BuildingTile *tile)
{
    BuildingTile *old = room->Wall;
    room->Wall = tile;
    emit roomDefinitionChanged();
    return old;
}

BuildingTile *BuildingDocument::changeFloorForRoom(Room *room, BuildingTile *tile)
{
    BuildingTile *old = room->Floor;
    room->Floor = tile;
    emit roomDefinitionChanged();
    return old;
}

void BuildingDocument::insertFloor(int index, BuildingFloor *floor)
{
    building()->insertFloor(index, floor);
    emit floorAdded(floor);
}

void BuildingDocument::insertObject(BuildingFloor *floor, int index, BuildingObject *object)
{
    Q_ASSERT(object->floor() == floor);
    floor->insertObject(index, object);
    emit objectAdded(object);
}

BuildingObject *BuildingDocument::removeObject(BuildingFloor *floor, int index)
{
    BuildingObject *object = floor->object(index);

    if (mSelectedObjects.contains(object)) {
        mSelectedObjects.remove(object);
        emit selectedObjectsChanged();
    }

    emit objectAboutToBeRemoved(object);
    floor->removeObject(index);
    emit objectRemoved(floor, index);
    return object;
}

QPoint BuildingDocument::moveObject(BuildingObject *object, const QPoint &pos)
{
    QPoint old = object->pos();
    object->setPos(pos);
    emit objectMoved(object);
    return old;
}

BuildingTile *BuildingDocument::changeDoorTile(Door *door, BuildingTile *tile,
                                               bool isFrame)
{
    BuildingTile *old = door->tile();
    isFrame ? door->setFrameTile(tile) : door->setTile(tile);
    emit objectTileChanged(door);
    return old;
}

BuildingTile *BuildingDocument::changeObjectTile(BuildingObject *object, BuildingTile *tile)
{
    BuildingTile *old = object->tile();
    object->setTile(tile);
    emit objectTileChanged(object);
    return old;
}

void BuildingDocument::insertRoom(int index, Room *room)
{
    mBuilding->insertRoom(index, room);
    emit roomAdded(room);
}

Room *BuildingDocument::removeRoom(int index)
{
    Room *room = building()->room(index);
    emit roomAboutToBeRemoved(room);
    building()->removeRoom(index);
    emit roomRemoved(room);
    return room;
}

int BuildingDocument::reorderRoom(int index, Room *room)
{
    int oldIndex = building()->rooms().indexOf(room);
    building()->removeRoom(oldIndex);
    building()->insertRoom(index, room);
    emit roomsReordered();
    return oldIndex;
}

Room *BuildingDocument::changeRoom(Room *room, const Room *data)
{
    Room *old = new Room(room);
    room->copy(data);
    emit roomChanged(room);
    delete data;
    return old;
}

QVector<QVector<Room*> > BuildingDocument::swapFloorGrid(BuildingFloor *floor,
                                                         const QVector<QVector<Room*> > &grid)
{
    QVector<QVector<Room*> > old = floor->grid();
    floor->setGrid(grid);
    emit floorEdited(floor);
    return old;
}

QSize BuildingDocument::resizeBuilding(const QSize &newSize)
{
    QSize old = building()->size();
    building()->resize(newSize);
    return old;
}

QVector<QVector<Room *> > BuildingDocument::resizeFloor(BuildingFloor *floor,
                                                        const QVector<QVector<Room *> > &grid)
{
    QVector<QVector<Room *> > old = floor->grid();
    floor->setGrid(grid);
    return old;
}

void BuildingDocument::rotateBuilding(bool right)
{
    mBuilding->rotate(right);
    foreach (BuildingFloor *floor, mBuilding->floors())
        floor->rotate(right);
    emit buildingRotated();
}

void BuildingDocument::flipBuilding(bool horizontal)
{
    mBuilding->flip(horizontal);
    foreach (BuildingFloor *floor, mBuilding->floors())
        floor->flip(horizontal);
    emit buildingRotated();
}
