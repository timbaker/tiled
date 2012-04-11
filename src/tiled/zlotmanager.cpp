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

#include <QDir>
#include <QFileInfo>
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "mapreader.h"
#include "objectgroup.h"
#include "preferences.h"
#include "zlot.hpp"

namespace Tiled {

using namespace Internal;

ZLotManager::ZLotManager()
	: mMapDocument(0)
{
	Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(lotDirectoryChanged()), SLOT(onLotDirectoryChanged()));
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
			connect(mapDocument(), SIGNAL(objectsAdded(QList<MapObject*>)),
					this, SLOT(onObjectsAdded(QList<MapObject*>)));
			connect(mapDocument(), SIGNAL(objectsChanged(QList<MapObject*>)),
					this, SLOT(onObjectsChanged(QList<MapObject*>)));
			connect(mapDocument(), SIGNAL(objectsRemoved(QList<MapObject*>)),
				this, SLOT(onObjectsRemoved(QList<MapObject*>)));

			Map *map = mapDocument()->map();
			foreach (Layer *layer, map->layers()) {
				if (ObjectGroup *og = layer->asObjectGroup()) {
					onObjectsAdded(og->objects());
				}
			}
		}
	}
}

void ZLotManager::handleMapObject(MapObject *mapObject)
{
	const QString& name = mapObject->name();
	const QString& type = mapObject->type();

	ZLot *currLot = 0, *newLot = 0;

	if (mMapObjectToLot.contains(mapObject))
		currLot = mMapObjectToLot[mapObject];

	if (name == QLatin1String("lot") && !type.isEmpty()) {

		if (mLots.keys().contains(type)) {
			newLot = mLots[type];
		} else {
			Preferences *prefs = Preferences::instance();
			QDir lotDirectory(prefs->lotDirectory());
			if (lotDirectory.exists()) {
				QFileInfo fileInfo(lotDirectory, type + QLatin1String(".tmx"));
				if (fileInfo.exists()) {
					MapReader reader;
					Map *map = reader.readMap(fileInfo.absoluteFilePath());
					if (map) {
						// TODO: sanity check the lot-map tile width and height against the current map
						mLots[type] = new ZLot(map);
						newLot = mLots[type];
					} else {
						// TODO: Add error handling
					}
				}
			}
		}
	}

	if (currLot != newLot) {
		if (currLot) {
			QMap<MapObject*,ZLot*>::iterator it = mMapObjectToLot.find(mapObject);
			mMapObjectToLot.erase(it);
			emit lotRemoved(currLot, mapObject); // remove from scene
		}
		if (newLot) {
			mMapObjectToLot[mapObject] = newLot;
			emit lotAdded(newLot, mapObject); // add to scene
		}
	} else if (currLot) {
		emit lotUpdated(currLot, mapObject); // position change, etc
	}
}

void ZLotManager::onLotDirectoryChanged()
{
	// This will try to load any lot files that couldn't be loaded from the old directory.
	// Lot files that were already loaded won't be affected.
	Map *map = mapDocument()->map();
	foreach (Layer *layer, map->layers()) {
		if (ObjectGroup *og = layer->asObjectGroup()) {
			onObjectsChanged(og->objects());
		}
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
		QMap<MapObject*,ZLot*>::iterator it = mMapObjectToLot.find(mapObject);
		if (it != mMapObjectToLot.end()) {
			ZLot *lot = it.value();
			mMapObjectToLot.erase(it);
			emit lotRemoved(lot, mapObject);
		}
	}
}

} // namespace Tiled