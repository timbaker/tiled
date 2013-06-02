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

#include "zlotmanager.h"

#include "map.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "mapreader.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zprogress.h"

#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"

#include <QDir>
#include <QFileInfo>

using namespace Tiled;
using namespace Tiled::Internal;

ZLotManager::ZLotManager()
    : mMapDocument(0)
{
    connect(MapManager::instance(), SIGNAL(mapLoaded(MapInfo*)),
            SLOT(mapLoaded(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapFailedToLoad(MapInfo*)),
            SLOT(mapFailedToLoad(MapInfo*)));
}

ZLotManager::~ZLotManager()
{
    if (mapDocument())
        mapDocument()->disconnect(this);
}

void ZLotManager::setMapDocument(MapDocument *mapDoc)
{
    if (mapDoc != mMapDocument) {

        if (mMapDocument) {
            mapDocument()->disconnect(this);
        }

        mMapDocument = mapDoc;

        if (mMapDocument) {
            connect(mapDocument(), SIGNAL(layerAdded(int)),
                    this, SLOT(onLayerAdded(int)));
            connect(mapDocument(), SIGNAL(layerAboutToBeRemoved(int)),
                    this, SLOT(onLayerAboutToBeRemoved(int)));
            connect(mapDocument(), SIGNAL(objectsAdded(QList<MapObject*>)),
                    this, SLOT(onObjectsAdded(QList<MapObject*>)));
            connect(mapDocument(), SIGNAL(objectsChanged(QList<MapObject*>)),
                    this, SLOT(onObjectsChanged(QList<MapObject*>)));
            connect(mapDocument(), SIGNAL(objectsRemoved(QList<MapObject*>)),
                this, SLOT(onObjectsRemoved(QList<MapObject*>)));

            Map *map = mapDocument()->map();
            foreach (ObjectGroup *og, map->objectGroups())
                onObjectsAdded(og->objects());

#if 1
            if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mMapDocument->fileName())) {
                foreach (WorldCellLot *lot, cell->lots()) {
                    MapInfo *mapInfo = MapManager::instance()->loadMap(lot->mapName(), QString(),
                                                                       true, MapManager::PriorityLow);
                    if (mapInfo) {
                        if (mapInfo->isLoading())
                            mMapsLoading2 += MapLoading2(mapInfo, lot);
                        else
                            setMapInfo(lot, mapInfo);
                    }
                }
            }
#endif
        }
    }
}

void ZLotManager::worldCellLevelChanged(int level, bool visible)
{
    foreach (WorldCellLot *lot, mWorldCellLotToMC.keys()) {
        if (lot->level() == level) {
            mWorldCellLotToMC[lot]->setGroupVisible(visible);
            emit lotUpdated(mWorldCellLotToMC[lot], lot);
        }
    }
}

void ZLotManager::worldCellLotChanged(WorldCellLot *lot)
{
    if (mWorldCellLotToMC.contains(lot))
        setMapComposite(lot, mWorldCellLotToMC[lot]);
}

void ZLotManager::handleMapObject(MapObject *mapObject)
{
#if 1
    const QString& name = mapObject->name();
    const QString& type = mapObject->type();

    MapInfo *newInfo = 0;

    if (name == QLatin1String("lot") && !type.isEmpty()) {
        QString mapName = type;
        if (!mMapDocument->fileName().isEmpty()) {
            QDir mapDir = QFileInfo(mMapDocument->fileName()).absoluteDir();
            if (QDir::isRelativePath(mapName))
                mapName = mapDir.filePath(mapName);
        }
        if (QDir::isAbsolutePath(mapName))
            newInfo = MapManager::instance()->loadMap(mapName, QString(),
                                                      true, MapManager::PriorityMedium);
    }

    if (newInfo && newInfo->isLoading()) {
        int n = findLoading(mapObject);
        if (n != -1)
            mMapsLoading[n].info = newInfo;
        else
            mMapsLoading += MapLoading(newInfo, mapObject);
    } else if (!newInfo || !newInfo->isLoading())
        setMapInfo(mapObject, newInfo);
#else
    const QString& name = mapObject->name();
    const QString& type = mapObject->type();

    MapComposite *currLot = 0, *newLot = 0;

    if (mMapObjectToLot.contains(mapObject))
        currLot = mMapObjectToLot[mapObject];

    if (name == QLatin1String("lot") && !type.isEmpty()) {

        QString mapName = type/* + QLatin1String(".tmx")*/;
        if (!mMapDocument->fileName().isEmpty()) {
            QDir mapDir = QFileInfo(mMapDocument->fileName()).absoluteDir();
            if (QDir::isRelativePath(mapName))
                mapName = mapDir.filePath(mapName);
        }
        MapInfo *newMapInfo = 0;
        if (QDir::isAbsolutePath(mapName))
            newMapInfo = MapManager::instance()->mapInfo(mapName);

        if (newMapInfo) {
            if (!currLot || (currLot->map() != newMapInfo->map())) {
                int level;
                (void) MapComposite::levelForLayer(mapObject->objectGroup(), &level);
                newLot = mMapDocument->mapComposite()->addMap(newMapInfo,
                                                              mapObject->position().toPoint(),
                                                              level);
            } else
                newLot = currLot;
        } else {
            if (currLot && QFileInfo(currLot->mapInfo()->path()) == QFileInfo(mapName))
                newLot = currLot;
        }
    }

    if (currLot != newLot) {
        if (currLot) {
            QMap<MapObject*,MapComposite*>::iterator it = mMapObjectToLot.find(mapObject);
            mMapDocument->mapComposite()->removeMap((*it)); // deletes currLot!
            mMapObjectToLot.erase(it);
            emit lotRemoved(currLot, mapObject); // remove from scene
        }
        if (newLot) {
            mMapObjectToLot[mapObject] = newLot;
            newLot->setGroupVisible(mapObject->objectGroup()->isVisible());
            emit lotAdded(newLot, mapObject); // add to scene
        }
    } else if (currLot) {
        if (currLot->origin() != mapObject->position().toPoint())
            mMapDocument->mapComposite()->moveSubMap(currLot, mapObject->position().toPoint());
        else if (currLot->isVisible() != mapObject->isVisible()) {
            currLot->setVisible(mapObject->isVisible());
        }
        emit lotUpdated(currLot, mapObject); // position change, etc
    }
#endif
}

void ZLotManager::setMapInfo(MapObject *mapObject, MapInfo *mapInfo)
{
    MapInfo *currInfo = 0, *newInfo = mapInfo;

    if (mMapObjectToInfo.contains(mapObject))
        currInfo = mMapObjectToInfo[mapObject];

    MapComposite *newLot = 0;

    if (currInfo != newInfo) {
        if (currInfo) {
            mMapObjectToInfo.remove(mapObject);
        }
        if (newInfo) {
            mMapObjectToInfo[mapObject] = newInfo;
            int level;
            (void) MapComposite::levelForLayer(mapObject->objectGroup(), &level);
            newLot = mMapDocument->mapComposite()->addMap(newInfo,
                                                          mapObject->position().toPoint(),
                                                          level);
        }
    } else {
        if (mMapObjectToLot.contains(mapObject))
            newLot = mMapObjectToLot[mapObject];
    }

    setMapComposite(mapObject, newLot);
}

void ZLotManager::setMapComposite(MapObject *mapObject, MapComposite *mapComposite)
{
    MapComposite *currLot = 0, *newLot = mapComposite;

    if (mMapObjectToLot.contains(mapObject))
        currLot = mMapObjectToLot[mapObject];

    if (currLot != newLot) {
        if (currLot) {
            QMap<MapObject*,MapComposite*>::iterator it = mMapObjectToLot.find(mapObject);
            mMapDocument->mapComposite()->removeMap((*it)); // deletes currLot!
            mMapObjectToLot.erase(it);
            emit lotRemoved(currLot, mapObject); // remove from scene
        }
        if (newLot) {
            mMapObjectToLot[mapObject] = newLot;
            newLot->setGroupVisible(mapObject->objectGroup()->isVisible());
            emit lotAdded(newLot, mapObject); // add to scene
        }
    } else if (currLot) {
        if (currLot->origin() != mapObject->position().toPoint())
            mMapDocument->mapComposite()->moveSubMap(currLot, mapObject->position().toPoint());
        else if (currLot->isVisible() != mapObject->isVisible()) {
            currLot->setVisible(mapObject->isVisible());
        }
        emit lotUpdated(currLot, mapObject); // position change, etc
    }
}

int ZLotManager::findLoading(MapObject *mapObject)
{
    for (int i = 0; i < mMapsLoading.size(); i++) {
        MapLoading &ml = mMapsLoading[i];
        if (ml.object == mapObject)
            return i;
    }
    return -1;
}

void ZLotManager::setMapInfo(WorldCellLot *lot, MapInfo *mapInfo)
{
    MapInfo *currInfo = 0, *newInfo = mapInfo;

    if (mWorldCellLotToMI.contains(lot))
        currInfo = mWorldCellLotToMI[lot];

    MapComposite *newLot = 0;

    if (currInfo != newInfo) {
        if (currInfo) {
            mWorldCellLotToMI.remove(lot);
        }
        if (newInfo) {
            mWorldCellLotToMI[lot] = newInfo;
            newLot = mMapDocument->mapComposite()->addMap(newInfo,
                                                          lot->pos(),
                                                          lot->level());
        }
    } else {
        if (mWorldCellLotToMC.contains(lot))
            newLot = mWorldCellLotToMC[lot];
    }

    setMapComposite(lot, newLot);
}

void ZLotManager::setMapComposite(WorldCellLot *lot, MapComposite *mapComposite)
{
    MapComposite *currLot = 0, *newLot = mapComposite;

    if (mWorldCellLotToMC.contains(lot))
        currLot = mWorldCellLotToMC[lot];

    if (currLot != newLot) {
        if (currLot) {
            QMap<WorldCellLot*,MapComposite*>::iterator it = mWorldCellLotToMC.find(lot);
            mMapDocument->mapComposite()->removeMap((*it)); // deletes currLot!
            mWorldCellLotToMC.erase(it);
//            emit lotRemoved(currLot, lot); // remove from scene
            emit lotUpdated(currLot, lot); // position change, etc
        }
        if (newLot) {
            mWorldCellLotToMC[lot] = newLot;
//            newLot->setGroupVisible(lot->objectGroup()->isVisible());
//            emit lotAdded(newLot, lot); // add to scene
            emit lotUpdated(currLot, lot); // position change, etc
        }
    } else if (currLot) {
        if (currLot->origin() != lot->pos())
            mMapDocument->mapComposite()->moveSubMap(currLot, lot->pos());
        else if (currLot->isVisible() != lot->isVisible()) {
            currLot->setVisible(lot->isVisible());
        }
        emit lotUpdated(currLot, lot); // position change, etc
    }
}

void ZLotManager::onLayerAdded(int index)
{
    Layer *layer = mapDocument()->map()->layerAt(index);
    // Moving a layer first removes it, then adds it again
    if (ObjectGroup *og = layer->asObjectGroup()) {
        onObjectsAdded(og->objects());
    }
}

void ZLotManager::onLayerAboutToBeRemoved(int index)
{
    Layer *layer = mapDocument()->map()->layerAt(index);
    // Moving a layer first removes it, then adds it again
    if (ObjectGroup *og = layer->asObjectGroup()) {
        onObjectsRemoved(og->objects());
    }
}

void ZLotManager::onObjectsAdded(const QList<MapObject*> &objects)
{
    foreach (MapObject *mapObj, objects)
        handleMapObject(mapObj);
}

void ZLotManager::onObjectsChanged(const QList<MapObject*> &objects)
{
    foreach (MapObject *mapObj, objects)
        handleMapObject(mapObj);
}

// FIXME: should forget stuff when a map document is closed
void ZLotManager::onObjectsRemoved(const QList<MapObject*> &objects)
{
    foreach (MapObject *mapObject, objects) {
        mMapObjectToInfo.remove(mapObject);
        int n = findLoading(mapObject);
        if (n != -1)
            mMapsLoading.removeAt(n);
        QMap<MapObject*,MapComposite*>::iterator it = mMapObjectToLot.find(mapObject);
        if (it != mMapObjectToLot.end()) {
            MapComposite *lot = it.value();
            mMapDocument->mapComposite()->removeMap(lot); // deletes lot!
            mMapObjectToLot.erase(it);
            emit lotRemoved(lot, mapObject);
        }
    }
}

void ZLotManager::mapLoaded(MapInfo *mapInfo)
{
    for (int i = 0; i < mMapsLoading.size(); i++) {
        MapLoading &ml = mMapsLoading[i];
        if (ml.info == mapInfo) {
            setMapInfo(ml.object, ml.info);
            mMapsLoading.removeAt(i);
            --i;
        }
    }

    for (int i = 0; i < mMapsLoading2.size(); i++) {
        MapLoading2 &ml = mMapsLoading2[i];
        if (ml.info == mapInfo) {
            setMapInfo(ml.lot, ml.info);
            mMapsLoading2.removeAt(i);
            --i;
        }
    }
}

void ZLotManager::mapFailedToLoad(MapInfo *mapInfo)
{
    for (int i = 0; i < mMapsLoading.size(); i++) {
        MapLoading &ml = mMapsLoading[i];
        if (ml.info == mapInfo) {
            mMapsLoading.removeAt(i);
            --i;
        }
    }

    for (int i = 0; i < mMapsLoading2.size(); i++) {
        MapLoading2 &ml = mMapsLoading2[i];
        if (ml.info == mapInfo) {
            mMapsLoading2.removeAt(i);
            --i;
        }
    }
}

