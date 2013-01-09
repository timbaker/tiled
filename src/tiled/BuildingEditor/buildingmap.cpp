/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "buildingmap.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtiles.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

#include <QDebug>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingMap::BuildingMap(Building *building) :
    mBuilding(building),
    mMapComposite(0),
    mMap(0),
    mBlendMapComposite(0),
    mBlendMap(0),
    mMapRenderer(0),
    pending(false),
    pendingRecreateAll(false),
    pendingBuildingResized(false),
    mCursorObjectFloor(0),
    mCursorObjectBuilding(0)
{
    BuildingToMap();
}

BuildingMap::~BuildingMap()
{
    if (mMapComposite) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;

        delete mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;

        delete mMapRenderer;
    }
}

QString BuildingMap::buildingTileAt(int x, int y, int level, const QString &layerName)
{
    CompositeLayerGroup *layerGroup = mBlendMapComposite->layerGroupForLevel(level);

    QString tileName;

    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            if (tl->contains(x, y)) {
                Tile *tile = tl->cellAt(x, y).tile;
                if (tile)
                    tileName = BuildingTilesMgr::nameForTile(tile);
            }
            break;
        }
    }

    return tileName;
}

// The order must match the LayerIndexXXX constants.
// FIXME: add user-defined layers as well from TMXConfig.txt.
static const char *gLayerNames[] = {
    "Floor",
    "FloorGrime",
    "FloorGrime2",
    "Walls",
    "Walls2",
    "RoofCap",
    "RoofCap2",
    "WallOverlay",
    "WallOverlay2",
    "WallGrime",
    "WallFurniture",
    "Frames",
    "Doors",
    "Curtains",
    "Furniture",
    "Furniture2",
    "Curtains2",
    "Roof",
    "Roof2",
    "RoofTop",
    0
};

QStringList BuildingMap::layerNames(int level)
{
    Q_UNUSED(level)
//    if (!mDocument)
//        return QStringList();

    QStringList ret;
    for (int i = 0; gLayerNames[i]; i++)
        ret += QLatin1String(gLayerNames[i]);
    return ret;
}

/**
  This method requires a bit of explanation.  The purpose is to show the result of
  adding or resizing a building object in real time.  BuildingFloor::LayoutToSquares
  is rather slow and doesn't support updating a sub-area of a floor.  This method
  creates a new tiny building that is just a bit larger than the object being
  created or resized.  The tiny building is given a copy of only those objects
  that overlap its bounds.  LayoutToSquares is then run just on the floor in the
  tiny building, and those squares are later used by BuildingSquaresToTileLayers.
  There are still issues with objects that should affect the floors above/below
  like stairs.
  */
void BuildingMap::setCursorObject(BuildingFloor *floor, BuildingObject *object,
                                  const QRect &bounds)
{
    if (mCursorObjectFloor) {
        BuildingFloor *floor2 = mBuilding->floor(mCursorObjectFloor->level());
        pendingSquaresToTileLayers[floor2] |= mCursorObjectFloor->bounds().translated(mCursorObjectPos);
        if (!pending) {
            QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
            pending = true;
        }
        delete mCursorObjectFloor;
        mCursorObjectFloor = 0;
        delete mCursorObjectBuilding;
        mCursorObjectBuilding = 0;
    }
    if (object/* && floor->bounds().intersects(object->bounds())*/) {
        // When resizing a wall object, we must call SquaresToTileLayers with
        // the combined area of the wall object's original bounds and the
        // current bounds during resizing.
        mCursorObjectBounds = bounds.isNull() ? object->bounds() : bounds;
        QRect r = mCursorObjectBounds.adjusted(-2, -2, 2, 2) & floor->bounds();
        mCursorObjectBuilding = new Building(r.width(), r.height());
        mCursorObjectBuilding->setExteriorWall(floor->building()->exteriorWall());
        foreach (Room *room, floor->building()->rooms())
            mCursorObjectBuilding->insertRoom(mCursorObjectBuilding->roomCount(),
                                              room);
        foreach (BuildingFloor *floor2, floor->building()->floors()) {
            BuildingFloor *clone = new BuildingFloor(mCursorObjectBuilding, floor2->level());
            mCursorObjectBuilding->insertFloor(clone->level(), clone);
            // TODO: clone stairs/roofs on floor below
            if (floor2 == floor)
                mCursorObjectFloor = clone;
        }

        for (int x = r.x(); x <= r.right(); x++) {
            for (int y = r.y(); y <= r.bottom(); y++) {
                mCursorObjectFloor->SetRoomAt(x - r.x(), y - r.y(), floor->GetRoomAt(x, y));
            }
        }
        foreach (BuildingObject *object, floor->objects()) {
            if (r.adjusted(0,0,1,1) // some objects can be on the edge of the building
                    .intersects(object->bounds())) {
                BuildingObject *clone = object->clone();
                clone->setPos(clone->pos() - r.topLeft());
                clone->setFloor(mCursorObjectFloor);
                mCursorObjectFloor->insertObject(mCursorObjectFloor->objectCount(),
                                                 clone);
            }
        }
        BuildingObject *clone = object->clone();
        clone->setPos(clone->pos() - r.topLeft());
        clone->setFloor(mCursorObjectFloor);
        mCursorObjectFloor->insertObject(mCursorObjectFloor->objectCount(), clone);
        mCursorObjectFloor->LayoutToSquares();
        mCursorObjectPos = r.topLeft();

        pendingSquaresToTileLayers[floor] |= r;
        if (!pending) {
            QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
            pending = true;
        }
    }
}

void BuildingMap::buildingRotated()
{
    pendingBuildingResized = true;

    // When rotating or flipping, all the user tiles are cleared.
    // However, no signal is emitted until the buildingRotated signal.
    pendingEraseUserTiles = mBuilding->floors().toSet();

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::buildingResized()
{
    pendingBuildingResized = true;
    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::BuildingToMap()
{
    if (mMapComposite) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;

        delete mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;

        delete mMapRenderer;
    }

    Map::Orientation orient = Map::LevelIsometric;

    int maxLevel =  mBuilding->floorCount() - 1;
    int extraForWalls = 1;
    int extra = (orient == Map::LevelIsometric)
            ? extraForWalls : maxLevel * 3 + extraForWalls;
    QSize mapSize(mBuilding->width() + extra,
                  mBuilding->height() + extra);

    mMap = new Map(orient,
                   mapSize.width(), mapSize.height(),
                   64, 32);

    // Add tilesets from Tilesets.txt
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets())
        mMap->addTileset(ts);
    TilesetManager::instance()->addReferences(mMap->tilesets());

    switch (mMap->orientation()) {
    case Map::Isometric:
        mMapRenderer = new IsometricRenderer(mMap);
        break;
    case Map::LevelIsometric:
        mMapRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return;
    }

    Q_ASSERT(sizeof(gLayerNames)/sizeof(gLayerNames[0]) == BuildingFloor::Square::MaxSection + 1);

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        foreach (QString name, layerNames(floor->level())) {
            QString layerName = tr("%1_%2").arg(floor->level()).arg(name);
            TileLayer *tl = new TileLayer(layerName,
                                          0, 0, mapSize.width(), mapSize.height());
            mMap->addLayer(tl);
        }
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);
    mMapComposite = new MapComposite(mapInfo);

    // Synch layer opacity with the floor.
    foreach (CompositeLayerGroup *layerGroup, mMapComposite->layerGroups()) {
        BuildingFloor *floor = mBuilding->floor(layerGroup->level());
        foreach (TileLayer *tl, layerGroup->layers()) {
            QString layerName = MapComposite::layerNameWithoutPrefix(tl);
            layerGroup->setLayerOpacity(tl, floor->layerOpacity(layerName));
        }
    }

    // This map displays the automatically-generated tiles from the building.
    mBlendMap = mMap->clone();
    TilesetManager::instance()->addReferences(mBlendMap->tilesets());
    mapInfo = MapManager::instance()->newFromMap(mBlendMap);
    mBlendMapComposite = new MapComposite(mapInfo);
    mMapComposite->setBlendOverMap(mBlendMapComposite);

    // Set the automatically-generated tiles.
    foreach (CompositeLayerGroup *layerGroup, mBlendMapComposite->layerGroups()) {
        BuildingFloor *floor = mBuilding->floor(layerGroup->level());
        floor->LayoutToSquares();
        BuildingSquaresToTileLayers(floor, floor->bounds(), layerGroup);
    }

    // Set the user-drawn tiles.
    foreach (BuildingFloor *floor, mBuilding->floors()) {
        foreach (QString layerName, floor->grimeLayers())
            userTilesToLayer(floor, layerName, floor->bounds().adjusted(0, 0, 1, 1));
    }

    // Do this before calculating the bounds of CompositeLayerGroupItem
    mMapRenderer->setMaxLevel(mMapComposite->maxLevel());
}

void BuildingMap::BuildingSquaresToTileLayers(BuildingFloor *floor,
                                              const QRect &area,
                                              CompositeLayerGroup *layerGroup)
{
    int maxLevel = floor->building()->floorCount() - 1;
    int offset = (mMap->orientation() == Map::LevelIsometric)
            ? 0 : (maxLevel - floor->level()) * 3;

#if 1
    QRect cursorBounds;
    if (mCursorObjectFloor && mCursorObjectFloor->level() == floor->level()) {
        cursorBounds = mCursorObjectBounds.adjusted(-1,-1,1,1) & floor->bounds(1, 1);
    }
#endif

    int section = 0;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (area == floor->bounds())
            tl->erase();
        else
            tl->erase(area.adjusted(0,0,1,1));
        for (int x = area.x(); x <= area.right() + 1; x++) {
            for (int y = area.y(); y <= area.bottom() + 1; y++) {
#if 1
                const BuildingFloor::Square &square = cursorBounds.contains(x, y)
                        ? mCursorObjectFloor->squares[x - mCursorObjectPos.x()][y - mCursorObjectPos.y()]
                        : floor->squares[x][y];
#else
                const BuildingFloor::Square &square = floor->squares[x][y];
#endif
                if (BuildingTile *btile = square.mTiles[section]) {
                    if (!btile->isNone()) {
                        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(btile))
                            tl->setCell(x + offset, y + offset, Cell(tile));
                    }
                    continue;
                }
                if (BuildingTileEntry *entry = square.mEntries[section]) {
                    int tileOffset = square.mEntryEnum[section];
                    if (entry->isNone() || entry->tile(tileOffset)->isNone())
                        continue;
                    if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->tile(tileOffset)))
                        tl->setCell(x + offset, y + offset, Cell(tile));
                }

            }
        }
        layerGroup->regionAltered(tl); // possibly set mNeedsSynch
        section++;
    }
}

void BuildingMap::userTilesToLayer(BuildingFloor *floor,
                                   const QString &layerName,
                                   const QRect &bounds)
{
    CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
    TileLayer *layer = 0;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    QMap<QString,Tileset*> tilesetByName;
    foreach (Tileset *ts, mMap->tilesets())
        tilesetByName[ts->name()] = ts;

    for (int x = bounds.left(); x <= bounds.right(); x++) {
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            QString tileName = floor->grimeAt(layerName, x, y);
            Tile *tile = 0;
            if (!tileName.isEmpty()) {
                tile = TilesetManager::instance()->missingTile();
                QString tilesetName;
                int index;
                if (BuildingTilesMgr::parseTileName(tileName, tilesetName, index)) {
                    if (tilesetByName.contains(tilesetName)) {
                        tile = tilesetByName[tilesetName]->tileAt(index);
                    }
                }
            }
            layer->setCell(x, y, Cell(tile));
        }
    }

    layerGroup->regionAltered(layer); // possibly set mNeedsSynch
}

void BuildingMap::floorAdded(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    recreateAllLater();
}

void BuildingMap::floorRemoved(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    recreateAllLater();
}

void BuildingMap::floorEdited(BuildingFloor *floor)
{
    pendingLayoutToSquares.insert(floor);
    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::floorTilesChanged(BuildingFloor *floor)
{
    pendingEraseUserTiles.insert(floor);
    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                                    const QRect &bounds)
{
    pendingUserTilesToLayer[floor][layerName] |= bounds;
    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::objectAdded(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::objectAboutToBeRemoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::objectRemoved(BuildingObject *object)
{
    Q_UNUSED(object)
}

void BuildingMap::objectMoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::objectTileChanged(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    pendingLayoutToSquares.insert(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            pendingLayoutToSquares.insert(floorAbove);
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

// When tilesets are added/removed, BuildingTile -> Tiled::Tile needs to be
// redone.
void BuildingMap::tilesetAdded(Tileset *tileset)
{
    int index = mMap->tilesets().indexOf(tileset);
    if (index >= 0)
        return;

    mMap->addTileset(tileset);
    TilesetManager::instance()->addReference(tileset);

    mBlendMap->addTileset(tileset);
    TilesetManager::instance()->addReference(tileset);

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        pendingSquaresToTileLayers[floor] = floor->bounds();
        foreach (QString layerName, floor->grimeLayers())
            pendingUserTilesToLayer[floor][layerName] = floor->bounds();
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int index = mMap->tilesets().indexOf(tileset);
    if (index < 0)
        return;

    mMap->removeTilesetAt(index);
    TilesetManager::instance()->removeReference(tileset);

    mBlendMap->removeTilesetAt(index);
    TilesetManager::instance()->removeReference(tileset);

    // Erase every layer to get rid of Tiles from the tileset.
    foreach (CompositeLayerGroup *lg, mMapComposite->layerGroups())
        foreach (TileLayer *tl, lg->layers())
            tl->erase();
    foreach (CompositeLayerGroup *lg, mBlendMapComposite->layerGroups())
        foreach (TileLayer *tl, lg->layers())
            tl->erase();

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        pendingSquaresToTileLayers[floor] = floor->bounds();
        foreach (QString layerName, floor->grimeLayers())
            pendingUserTilesToLayer[floor][layerName] = floor->bounds();
    }

    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

void BuildingMap::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
}

// FIXME: tilesetChanged?

void BuildingMap::handlePending()
{
    QMap<int,QRegion> updatedLevels;

    if (pendingRecreateAll) {
        emit aboutToRecreateLayers(); // LayerGroupItems need to get ready
        BuildingToMap();
        pendingBuildingResized = false;
        pendingEraseUserTiles.clear(); // no need to erase, we just recreated the layers
    }

    if (pendingRecreateAll || pendingBuildingResized) {
        pendingLayoutToSquares = mBuilding->floors().toSet();
        pendingUserTilesToLayer.clear();
        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (QString layerName, floor->grimeLayers()) {
                pendingUserTilesToLayer[floor][layerName] = floor->bounds();
            }
        }
    }

    if (pendingBuildingResized) {
        int extra = (mMap->orientation() == Map::LevelIsometric) ?
                    1 : mMapComposite->maxLevel() * 3 + 1;
        int width = mBuilding->width() + extra;
        int height = mBuilding->height() + extra;

        foreach (Layer *layer, mMap->layers())
            layer->resize(QSize(width, height), QPoint());
        mMap->setWidth(width);
        mMap->setHeight(height);

        foreach (Layer *layer, mBlendMap->layers())
            layer->resize(QSize(width, height), QPoint());
        mBlendMap->setWidth(width);
        mBlendMap->setHeight(height);
    }

    if (!pendingLayoutToSquares.isEmpty()) {
        foreach (BuildingFloor *floor, pendingLayoutToSquares) {
            floor->LayoutToSquares(); // not sure this belongs in this class
            pendingSquaresToTileLayers[floor] = floor->bounds();
        }
    }

    if (!pendingSquaresToTileLayers.isEmpty()) {
        foreach (BuildingFloor *floor, pendingSquaresToTileLayers.keys()) {
            CompositeLayerGroup *layerGroup = mBlendMapComposite->layerGroupForLevel(floor->level());
            QRect area = pendingSquaresToTileLayers[floor].boundingRect(); // TODO: only affected region
            BuildingSquaresToTileLayers(floor, area, layerGroup);
            if (layerGroup->needsSynch()) {
                mMapComposite->layerGroupForLevel(floor->level())->setNeedsSynch(true);
                layerGroup->synch(); // Don't really need to synch the blend-over-map, but do need
                                     // to update its draw margins so MapComposite::regionAltered
                                     // doesn't set mNeedsSynch repeatedly.
            }
            updatedLevels[floor->level()] |= area;
        }
    }

    if (!pendingEraseUserTiles.isEmpty()) {
        foreach (BuildingFloor *floor, pendingEraseUserTiles) {
            CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
            foreach (TileLayer *tl, layerGroup->layers())
                tl->erase();
            foreach (QString layerName, floor->grimeLayers())
                pendingUserTilesToLayer[floor][layerName] = floor->bounds();
            updatedLevels[floor->level()] |= floor->bounds();
        }
    }

    if (!pendingUserTilesToLayer.isEmpty()) {
        foreach (BuildingFloor *floor, pendingUserTilesToLayer.keys()) {
            foreach (QString layerName, pendingUserTilesToLayer[floor].keys()) {
                QRegion rgn = pendingUserTilesToLayer[floor][layerName];
                foreach (QRect r, rgn.rects())
                    userTilesToLayer(floor, layerName, r);
                updatedLevels[floor->level()] |= rgn;
            }
        }
    }

    if (pendingRecreateAll)
        emit layersRecreated();
    else if (pendingBuildingResized)
        emit mapResized();

    foreach (int level, updatedLevels.keys())
        emit layersUpdated(level, updatedLevels[level]);

    pending = false;
    pendingRecreateAll = false;
    pendingBuildingResized = false;
    pendingLayoutToSquares.clear();
    pendingSquaresToTileLayers.clear();
    pendingEraseUserTiles.clear();
    pendingUserTilesToLayer.clear();
}

void BuildingMap::recreateAllLater()
{
    pendingRecreateAll = true;
    if (!pending) {
        QMetaObject::invokeMethod(this, "handlePending", Qt::QueuedConnection);
        pending = true;
    }
}

