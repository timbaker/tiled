/*
 * zlotmanager.cpp
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

#include "zlotmanager.hpp"

#include "map.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "mapreader.h"
#include "objectgroup.h"
#include "preferences.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zprogress.hpp"

#include <QDir>
#include <QFileInfo>

// FIXME: Duplicated in ZomboidScene.cpp and mapcomposite.cpp
static bool levelForLayer(Tiled::Layer *layer, int *level)
{
    (*level) = 0;

    // See if the layer name matches "0_foo" or "1_bar" etc.
    const QString& name = layer->name();
    QStringList sl = name.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        (*level) = sl[0].toInt(&conversionOK);
        return conversionOK;
    }
    return false;
}

namespace Tiled {

using namespace Internal;

ZLotManager::ZLotManager()
    : mMapDocument(0)
{
    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(mapsDirectoryChanged()), SLOT(onMapsDirectoryChanged()));
}

ZLotManager::~ZLotManager()
{
    Preferences *prefs = Preferences::instance();
    prefs->disconnect(this);

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
        }
    }
}

void ZLotManager::handleMapObject(MapObject *mapObject)
{
    const QString& name = mapObject->name();
    const QString& type = mapObject->type();

    MapComposite *currLot = 0, *newLot = 0;

    if (mMapObjectToLot.contains(mapObject))
        currLot = mMapObjectToLot[mapObject];

    if (name == QLatin1String("lot") && !type.isEmpty()) {

        QString mapName = type/* + QLatin1String(".tmx")*/;
        MapInfo *newMapInfo = MapManager::instance()->loadMap(mapName,
                                                     mMapDocument->map()->orientation(),
                                                     mMapDocument->fileName());

        if (newMapInfo) {
            if (!currLot || (currLot->map() != newMapInfo->map())) {
                int level;
                (void) levelForLayer(mapObject->objectGroup(), &level);
                newLot = mMapDocument->mapComposite()->addMap(newMapInfo,
                                                              mapObject->position().toPoint(),
                                                              level);
            } else
                newLot = currLot;
        } else {
            if (currLot && (currLot->mapInfo() == newMapInfo))
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
            emit lotAdded(newLot, mapObject); // add to scene
        }
    } else if (currLot) {
        if (currLot->origin() != mapObject->position().toPoint())
            mMapDocument->mapComposite()->moveSubMap(currLot, mapObject->position().toPoint());
        else if (currLot->isVisible() != mapObject->isVisible()) {
            currLot->setVisible(mapObject->isVisible());
            foreach (CompositeLayerGroup *g, mMapDocument->mapComposite()->layerGroups())
                g->synch();
        }
        emit lotUpdated(currLot, mapObject); // position change, etc
    }
}

void ZLotManager::onMapsDirectoryChanged()
{
    // This will try to load any lot files that couldn't be loaded from the old directory.
    // Lot files that were already loaded won't be affected.
    Map *map = mapDocument()->map();
    foreach (ObjectGroup *og, map->objectGroups())
        onObjectsChanged(og->objects());
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

void ZLotManager::onObjectsRemoved(const QList<MapObject*> &objects)
{
    foreach (MapObject *mapObject, objects) {
        QMap<MapObject*,MapComposite*>::iterator it = mMapObjectToLot.find(mapObject);
        if (it != mMapObjectToLot.end()) {
            MapComposite *lot = it.value();
            mMapDocument->mapComposite()->removeMap(lot); // deletes lot!
            mMapObjectToLot.erase(it);
            emit lotRemoved(lot, mapObject);
        }
    }
}

} // namespace Tiled
