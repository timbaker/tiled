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
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtiles.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

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
    pendingBuildingResized(false)
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

    // The order must match the LayerIndexXXX constants.
    // FIXME: add user-defined layers as well from TMXConfig.txt.
    const char *layerNames[] = {
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
    Q_ASSERT(sizeof(layerNames)/sizeof(layerNames[0]) == BuildingFloor::Square::MaxSection + 1);

    foreach (BuildingFloor *floor, mBuilding->floors()) {
        for (int i = 0; layerNames[i]; i++) {
            QString name = QLatin1String(layerNames[i]);
            QString layerName = tr("%1_%2").arg(floor->level()).arg(name);
            TileLayer *tl = new TileLayer(layerName,
                                          0, 0, mapSize.width(), mapSize.height());
            mMap->addLayer(tl);
        }
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);
    mMapComposite = new MapComposite(mapInfo);

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
        BuildingSquaresToTileLayers(floor, layerGroup);
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
                                              CompositeLayerGroup *layerGroup)
{
    int maxLevel = floor->building()->floorCount() - 1;
    int offset = (mMap->orientation() == Map::LevelIsometric)
            ? 0 : (maxLevel - floor->level()) * 3;

    int section = 0;
    foreach (TileLayer *tl, layerGroup->layers()) {
        tl->erase();
        for (int x = 0; x <= floor->width(); x++) {
            for (int y = 0; y <= floor->height(); y++) {
                const BuildingFloor::Square &square = floor->squares[x][y];
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
    QSet<int> updatedLevels;

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
            BuildingSquaresToTileLayers(floor, layerGroup); // TODO: only affected region
            if (layerGroup->needsSynch())
                mMapComposite->layerGroupForLevel(floor->level())->setNeedsSynch(true);
            updatedLevels.insert(floor->level());
        }
    }

    if (!pendingEraseUserTiles.isEmpty()) {
        foreach (BuildingFloor *floor, pendingEraseUserTiles) {
            CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
            foreach (TileLayer *tl, layerGroup->layers())
                tl->erase();
            foreach (QString layerName, floor->grimeLayers())
                pendingUserTilesToLayer[floor][layerName] = floor->bounds();
            updatedLevels.insert(floor->level());
        }
    }

    if (!pendingUserTilesToLayer.isEmpty()) {
        foreach (BuildingFloor *floor, pendingUserTilesToLayer.keys()) {
            foreach (QString layerName, pendingUserTilesToLayer[floor].keys()) {
                foreach (QRect r, pendingUserTilesToLayer[floor][layerName].rects())
                    userTilesToLayer(floor, layerName, r);
            }
            updatedLevels.insert(floor->level());
        }
    }

    if (pendingRecreateAll)
        emit layersRecreated();
    else if (pendingBuildingResized)
        emit mapResized();

    foreach (int level, updatedLevels)
        emit layersUpdated(level);

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

