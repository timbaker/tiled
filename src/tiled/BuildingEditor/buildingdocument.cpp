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
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "buildingwriter.h"
#include "furnituregroups.h"

#include <QMessageBox>
#include <QUndoStack>

using namespace BuildingEditor;

BuildingDocument::BuildingDocument(Building *building, const QString &fileName) :
    QObject(),
    mBuilding(building),
    mFileName(fileName),
    mUndoStack(new QUndoStack(this))
{
    connect(BuildingTilesMgr::instance(), SIGNAL(entryTileChanged(BuildingTileEntry*)),
            SLOT(entryTileChanged(BuildingTileEntry*)));
    connect(FurnitureGroups::instance(),
            SIGNAL(furnitureTileChanged(FurnitureTile*)),
            SLOT(furnitureTileChanged(FurnitureTile*)));
    connect(FurnitureGroups::instance(),
            SIGNAL(furnitureLayerChanged(FurnitureTiles*)),
            SLOT(furnitureLayerChanged(FurnitureTiles*)));
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
    if (fileName.endsWith(QLatin1String(".autosave")))
        return true;
    mFileName = fileName;
    mUndoStack->setClean();
    return true;
}

void BuildingDocument::setCurrentFloor(BuildingFloor *floor)
{
    mCurrentFloor = floor;
    emit currentFloorChanged();
}

int BuildingDocument::currentLevel() const
{
    Q_ASSERT(mCurrentFloor);
    return mCurrentFloor->level();
}

bool BuildingDocument::currentFloorIsTop()
{
    return mCurrentFloor == mBuilding->floors().last();
}

bool BuildingDocument::currentFloorIsBottom()
{
    return mCurrentFloor == mBuilding->floors().first();
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

BuildingTileEntry *BuildingDocument::changeEWall(BuildingTileEntry *tile)
{
    BuildingTileEntry *old = mBuilding->exteriorWall();
    mBuilding->setExteriorWall(tile);
    emit roomDefinitionChanged();
    return old;
}

BuildingTileEntry *BuildingDocument::changeWallForRoom(Room *room, BuildingTileEntry *tile)
{
    BuildingTileEntry *old = room->Wall;
    room->Wall = tile;
    emit roomDefinitionChanged();
    return old;
}

BuildingTileEntry *BuildingDocument::changeFloorForRoom(Room *room, BuildingTileEntry *tile)
{
    BuildingTileEntry *old = room->Floor;
    room->Floor = tile;
    emit roomDefinitionChanged();
    return old;
}

void BuildingDocument::insertFloor(int index, BuildingFloor *floor)
{
    building()->insertFloor(index, floor);
    emit floorAdded(floor);
}

BuildingFloor *BuildingDocument::removeFloor(int index)
{
    BuildingFloor *floor = building()->floor(index);
    if (floor->floorAbove())
        setCurrentFloor(floor->floorAbove());
    else if (floor->floorBelow())
        setCurrentFloor(floor->floorBelow());

    floor = building()->removeFloor(index);
    emit floorRemoved(floor);
    return floor;
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
    emit objectRemoved(object);
    return object;
}

QPoint BuildingDocument::moveObject(BuildingObject *object, const QPoint &pos)
{
    QPoint old = object->pos();
    object->setPos(pos);
    emit objectMoved(object);
    return old;
}

BuildingTileEntry *BuildingDocument::changeObjectTile(BuildingObject *object,
                                                      BuildingTileEntry *tile,
                                                      int alternate)
{
    BuildingTileEntry *old = object->tile(alternate);
    object->setTile(tile, alternate);
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

FurnitureTile *BuildingDocument::changeFurnitureTile(FurnitureObject *object,
                                                     FurnitureTile *ftile)
{
    FurnitureTile *old = object->furnitureTile();
    object->setFurnitureTile(ftile);
    emit objectTileChanged(object);
    return old;
}

void BuildingDocument::resizeRoof(RoofObject *roof, int &width, int &height)
{
    int oldWidth = roof->bounds().width();
    int oldHeight = roof->bounds().height();

    roof->resize(width, height);

    emit objectMoved(roof);

    width = oldWidth;
    height = oldHeight;
}

void BuildingDocument::furnitureTileChanged(FurnitureTile *ftile)
{
    foreach (BuildingFloor *floor, mBuilding->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (FurnitureObject *furniture = object->asFurniture()) {
                if (furniture->furnitureTile() == ftile) {
                    emit objectTileChanged(furniture);
                }
            }
        }
    }
}

void BuildingDocument::furnitureLayerChanged(FurnitureTiles *ftiles)
{
    foreach (FurnitureTile *ftile, ftiles->tiles())
        furnitureTileChanged(ftile);
}

void BuildingDocument::entryTileChanged(BuildingTileEntry *entry)
{
    foreach (BuildingFloor *floor, mBuilding->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (object->tile() == entry ||
                    object->tile(1) == entry ||
                    object->tile(2) == entry) {
                emit objectTileChanged(object);
            }
        }
    }
}
