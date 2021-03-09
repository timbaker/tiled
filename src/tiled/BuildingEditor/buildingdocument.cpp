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
#include "buildingmap.h"
#include "buildingobjects.h"
#include "buildingreader.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "buildingwriter.h"
#include "furnituregroups.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QUndoStack>

using namespace BuildingEditor;

BuildingDocument::BuildingDocument(Building *building, const QString &fileName) :
    QObject(),
    mBuilding(building),
    mFileName(fileName),
    mUndoStack(new QUndoStack(this)),
    mTileChanges(false),
    mCurrentFloor(nullptr),
    mCurrentRoom(nullptr),
    mClipboardTiles(nullptr),
    mMapRotation(Tiled::MapRotation::NotRotated)
{
    // Roof tiles need to be non-none to enable the roof tools.
    // Old templates will have 'none' for these tiles.
    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
    if (building->roofCapTile()->isNone())
        building->setRoofCapTile(btiles->defaultRoofCapTiles());
    if (building->roofSlopeTile()->isNone())
        building->setRoofSlopeTile(btiles->defaultRoofSlopeTiles());
#if 0
    if (building->roofTopTile()->isNone())
        building->setRoofTopTile(btiles->defaultRoofTopTiles());
#endif

    // Handle reading old buildings
    if (building->usedTiles().isEmpty()) {
        QList<BuildingTileEntry*> entries;
        QList<FurnitureTiles*> furniture;
        foreach (BuildingFloor *floor, building->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (FurnitureObject *fo = object->asFurniture()) {
                    if (FurnitureTile *ftile = fo->furnitureTile()) {
                        if (!furniture.contains(ftile->owner()))
                            furniture += ftile->owner();
                    }
                    continue;
                }
                foreach (BuildingTileEntry *entry, object->tiles()) {
                    if (entry && !entry->isNone() && !entries.contains(entry))
                        entries += entry;
                }
            }
        }
        BuildingTileEntry *entry = building->exteriorWall();
        if (entry && !entry->isNone() && !entries.contains(entry))
            entries += entry;
        foreach (Room *room, building->rooms()) {
            foreach (BuildingTileEntry *entry, room->tiles()) {
                if (entry && !entry->isNone() && !entries.contains(entry))
                    entries += entry;
            }
        }
        building->setUsedTiles(entries);
        building->setUsedFurniture(furniture);
    }

    setCurrentFloor(building->floor(0));
    setCurrentRoom(building->roomCount() ? building->room(0) : 0);
    QStringList layerNames = BuildingMap::layerNames(currentLevel());
    if (layerNames.size())
        setCurrentLayer(layerNames.first());

    connect(BuildingTilesMgr::instance(), SIGNAL(entryTileChanged(BuildingTileEntry*)),
            SLOT(entryTileChanged(BuildingTileEntry*)));
    connect(FurnitureGroups::instance(),
            SIGNAL(furnitureTileChanged(FurnitureTile*)),
            SLOT(furnitureTileChanged(FurnitureTile*)));
    connect(FurnitureGroups::instance(),
            SIGNAL(furnitureLayerChanged(FurnitureTiles*)),
            SLOT(furnitureLayerChanged(FurnitureTiles*)));
}

BuildingDocument::~BuildingDocument()
{
    delete mClipboardTiles;
}

QString BuildingDocument::displayName() const
{
    QString displayName = QFileInfo(mFileName).fileName();
    if (displayName.isEmpty())
        displayName = tr("untitled.tbx");
    return displayName;
}

BuildingDocument *BuildingDocument::read(const QString &fileName, QString &error)
{
    BuildingReader reader;
    if (Building *building = reader.read(fileName)) {
        reader.fix(building);
        BuildingMap::loadNeededTilesets(building);
        BuildingDocument *doc = new BuildingDocument(building, fileName);
        if (fileName.endsWith(QLatin1String(".autosave")))
            doc->mFileName.clear();
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
    emit fileNameChanged();
    mUndoStack->setClean();
    if (mTileChanges) {
        mTileChanges = false;
        emit cleanChanged();
    }
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

void BuildingDocument::setCurrentRoom(Room *room)
{
    mCurrentRoom = room;
    emit currentRoomChanged();
}

void BuildingDocument::setCurrentLayer(const QString &layerName)
{
    mCurrentLayerName = layerName;
    emit currentLayerChanged();
}

void BuildingDocument::setLayerOpacity(BuildingFloor *floor,
                                       const QString &layerName, qreal opacity)
{
    if (floor->layerOpacity(layerName) != opacity) {
        floor->setLayerOpacity(layerName, opacity);
        emit layerOpacityChanged(floor, layerName);
    }
}

void BuildingDocument::setLayerVisibility(BuildingFloor *floor,
                                          const QString &layerName, bool visible)
{
    if (floor->layerVisibility(layerName) != visible) {
        floor->setLayerVisibility(layerName, visible);
        emit layerVisibilityChanged(floor, layerName);
    }
}

bool BuildingDocument::isModified() const
{
    // Editing tiles in the Tiles dialog might mean we must save the document.
    return !mUndoStack->isClean() || mTileChanges;
}

void BuildingDocument::setSelectedObjects(const QSet<BuildingObject *> &selection)
{
    if (selection == mSelectedObjects) return;
    mSelectedObjects = selection;
    emit selectedObjectsChanged();
}

void BuildingDocument::setClipboardTiles(FloorTileGrid *tiles, const QRegion &rgn)
{
    Q_ASSERT(tiles->bounds().contains(rgn.boundingRect()));
    delete mClipboardTiles;
    mClipboardTiles = tiles;
    mClipboardTilesRgn = rgn;
    emit clipboardTilesChanged();
}

Room *BuildingDocument::changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room)
{
    Room *old = floor->GetRoomAt(pos);
    floor->SetRoomAt(pos, room);
    emit roomAtPositionChanged(floor, pos);
    return old;
}

BuildingTileEntry *BuildingDocument::changeBuildingTile(int tileEnum,
                                                        BuildingTileEntry *tile)
{
    BuildingTileEntry *old = mBuilding->tile(tileEnum);
    mBuilding->setTile(tileEnum, tile);
    emit roomDefinitionChanged();

    checkUsedTile(tile);

    return old;
}

BuildingTileEntry *BuildingDocument::changeRoomTile(Room *room, int tileEnum,
                                                    BuildingTileEntry *tile)
{
    BuildingTileEntry *old = room->tile(tileEnum);
    room->setTile(tileEnum, tile);
    emit roomDefinitionChanged();

    checkUsedTile(tile);

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

void BuildingDocument::reorderFloor(int oldIndex, int newIndex)
{
    BuildingFloor *floor = removeFloor(oldIndex);
    insertFloor(newIndex, floor);
}

void BuildingDocument::insertObject(BuildingFloor *floor, int index, BuildingObject *object)
{
    Q_ASSERT(object->floor() == floor);
    floor->insertObject(index, object);

    if (FurnitureObject *fo = object->asFurniture()) {
        FurnitureTiles *ftiles = fo->furnitureTile() ? fo->furnitureTile()->owner() : 0;
        checkUsedFurniture(ftiles);
    } else {
        foreach (BuildingTileEntry *entry, object->tiles()) {
            if (entry)
                checkUsedTile(entry);
        }
    }

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

    checkUsedTile(tile);

    return old;
}

void BuildingDocument::insertRoom(int index, Room *room)
{
    mBuilding->insertRoom(index, room);
    emit roomAdded(room);

    foreach (BuildingTileEntry *entry, room->tiles())
        checkUsedTile(entry);
}

Room *BuildingDocument::removeRoom(int index)
{
    Room *room = building()->room(index);
    if (room == currentRoom()) {
        if (index < building()->roomCount() - 1)
            setCurrentRoom(building()->room(index + 1));
        else if (index > 0)
            setCurrentRoom(building()->room(index - 1));
        else
            setCurrentRoom(0);
    }
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

    foreach (BuildingTileEntry *entry, room->tiles())
        checkUsedTile(entry);

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

QMap<QString, FloorTileGrid *> BuildingDocument::swapFloorTiles(BuildingFloor *floor,
                                           const QMap<QString, FloorTileGrid*> &grid,
                                           bool emitSignal)
{
    QMap<QString,FloorTileGrid*> old = floor->setGrime(grid);
    if (emitSignal) // The signal should not be emitted when flipping/resizing/rotating.
        emit floorTilesChanged(floor);
    return old;
}

FloorTileGrid *BuildingDocument::swapFloorTiles(BuildingFloor *floor,
                                                const QString &layerName,
                                                const QRegion &rgn,
                                                const QPoint &pos,
                                                const FloorTileGrid *tiles)
{
    FloorTileGrid *old = floor->grimeAt(layerName, tiles->bounds().translated(pos), rgn);
    floor->setGrime(layerName, rgn, pos, tiles);
    emit floorTilesChanged(floor, layerName, rgn.boundingRect());
    return old;
}

QSize BuildingDocument::resizeBuilding(const QSize &newSize)
{
    QSize old = building()->size();
    building()->resize(newSize);
    return old;
}

QVector<QVector<Room *> > BuildingDocument::resizeFloor(BuildingFloor *floor,
                                                        const QVector<QVector<Room *> > &grid,
                                                        QMap<QString,FloorTileGrid*> &grime)
{
    QVector<QVector<Room *> > old = floor->grid();
    floor->setGrid(grid);
    grime = floor->setGrime(grime);
    return old;
}

void BuildingDocument::rotateBuilding(bool right)
{
    mBuilding->rotate(right);
    foreach (BuildingFloor *floor, mBuilding->floors())
        floor->rotate(right);
}

void BuildingDocument::flipBuilding(bool horizontal)
{
    mBuilding->flip(horizontal);
    foreach (BuildingFloor *floor, mBuilding->floors())
        floor->flip(horizontal);
}

FurnitureTile *BuildingDocument::changeFurnitureTile(FurnitureObject *object,
                                                     FurnitureTile *ftile)
{
    FurnitureTile *old = object->furnitureTile();
    object->setFurnitureTile(ftile);
    emit objectTileChanged(object);
    checkUsedFurniture(ftile->owner());
    return old;
}

void BuildingDocument::resizeRoof(RoofObject *roof, int &width, int &height,
                                  bool &halfDepth)
{
    int oldWidth = roof->bounds().width();
    int oldHeight = roof->bounds().height();
    bool oldHalfDepth = roof->isHalfDepth();

    roof->resize(width, height, halfDepth);

    emit objectMoved(roof);

    width = oldWidth;
    height = oldHeight;
    halfDepth = oldHalfDepth;
}

int BuildingDocument::resizeWall(WallObject *wall, int length)
{
    int old = wall->length();
    wall->setLength(length);
    emit objectMoved(wall);
    return old;
}

QList<BuildingTileEntry *> BuildingDocument::changeUsedTiles(const QList<BuildingTileEntry *> &tiles)
{
    QList<BuildingTileEntry *> old = mBuilding->usedTiles();
    mBuilding->setUsedTiles(tiles);
    emit usedTilesChanged();
    return old;
}

QList<FurnitureTiles *> BuildingDocument::changeUsedFurniture(const QList<FurnitureTiles *> &tiles)
{
    QList<FurnitureTiles *> old = mBuilding->usedFurniture();
    mBuilding->setUsedFurniture(tiles);
    emit usedFurnitureChanged();
    return old;
}

QRegion BuildingDocument::setRoomSelection(const QRegion &selection)
{
    QRegion old = mRoomSelection;
    mRoomSelection = selection;
    emit roomSelectionChanged(old);
    return old;
}

QRegion BuildingDocument::setTileSelection(const QRegion &selection)
{
    QRegion old = mTileSelection;
    mTileSelection = selection;
    emit tileSelectionChanged(old);
    return old;
}

void BuildingDocument::furnitureTileChanged(FurnitureTile *ftile)
{
    foreach (BuildingFloor *floor, mBuilding->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (FurnitureObject *furniture = object->asFurniture()) {
                if (furniture->furnitureTile() == ftile) {
                    emit objectTileChanged(furniture);
                    if (!mTileChanges) {
                        mTileChanges = true;
                        emit cleanChanged();
                    }
                }
            }
        }
    }

    if (mBuilding->usedFurniture().contains(ftile->owner())) {
        emit usedFurnitureChanged();
        if (!mTileChanges) {
            mTileChanges = true;
            emit cleanChanged();
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
            if (object->tiles().contains(entry)) {
                emit objectTileChanged(object);
                if (!mTileChanges) {
                    mTileChanges = true;
                    emit cleanChanged();
                }
            }
        }
    }

    if (mBuilding->usedTiles().contains(entry)) {
        emit usedTilesChanged();
        if (!mTileChanges) {
            mTileChanges = true;
            emit cleanChanged();
        }
    }
}

void BuildingDocument::checkUsedTile(BuildingTileEntry *tile)
{
    QList<BuildingTileEntry*> used = mBuilding->usedTiles();
    if (tile && !tile->isNone() && !used.contains(tile)) {
        mBuilding->setUsedTiles(used << tile);
        emit usedTilesChanged();
        if (!mTileChanges) {
            mTileChanges = true;
            emit cleanChanged();
        }
    }
}

void BuildingDocument::checkUsedFurniture(FurnitureTiles *ftiles)
{
    QList<FurnitureTiles*> used = mBuilding->usedFurniture();
    if (ftiles && !used.contains(ftiles)) {
        mBuilding->setUsedFurniture(used << ftiles);
        emit usedFurnitureChanged();
        if (!mTileChanges) {
            mTileChanges = true;
            emit cleanChanged();
        }
    }
}
