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
#include "mapdocument.h"
#include "mapobject.h"
#include "mapreader.h"
#include "objectgroup.h"
#include "preferences.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlot.hpp"
#include "zprogress.hpp"

#include <QDir>
#include <QFileInfo>

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

	qDeleteAll(mTilesets);
	qDeleteAll(mLots);
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

	bool progress = false;

	if (name == QLatin1String("lot") && !type.isEmpty()) {

		if (mLots.keys().contains(type)) {
			newLot = mLots[type];
		} else {
			Preferences *prefs = Preferences::instance();
			QDir lotDirectory(prefs->lotDirectory());
			if (lotDirectory.exists()) {
				QFileInfo fileInfo(lotDirectory, type + QLatin1String(".tmx"));
				if (fileInfo.exists()) {
					ZProgressManager::instance()->begin(QLatin1String("Reading ") + fileInfo.fileName());
					progress = true;
					MapReader reader;
					Map *map = reader.readMap(fileInfo.absoluteFilePath());
					if (map) {
						Map::Orientation orient = map->orientation();
						shareTilesets(map);
						convertOrientation(map, mMapDocument->map());
						// TODO: sanity check the lot-map tile width and height against the current map
						mLots[type] = new ZLot(map, orient);
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

	if (progress)
		ZProgressManager::instance()->end();
}

void ZLotManager::shareTilesets(Map *map)
{
	foreach (Tileset *ts0, map->tilesets()) {
		Tileset *ts1 = ts0->findSimilarTileset(mTilesets);
		if (ts1) {
			map->replaceTileset(ts0, ts1);
			delete ts0;
		} else {
			mTilesets.append(ts0);
		}
	}
}

// FIXME: Duplicated in ZomboidScene.cpp and zlot.cpp
static bool levelForLayer(Layer *layer, int *level)
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

void ZLotManager::convertOrientation(Map *map0, const Map *map1)
{
	if (!map0 || !map1)
		return;
	if (map0->orientation() == map1->orientation())
		return;
	if (map0->orientation() == Map::Isometric && map1->orientation() == Map::LevelIsometric) {
		QPoint offset(3, 3);
		foreach (Layer *layer, map0->layers()) {
			if (TileLayer *tl = layer->asTileLayer()) {
				int level;
				if (levelForLayer(tl, &level) && level > 0) {
					tl->offset(offset * level, tl->bounds(), false, false);
				}
			}
		}
		map0->setOrientation(map1->orientation());
		return;
	}
	if (map0->orientation() == Map::LevelIsometric && map1->orientation() == Map::Isometric) {
		QPoint offset(3, 3);
#if 1
		int level, maxLevel = 0;
		foreach (Layer *layer, map0->layers()) {
			if (levelForLayer(layer, &level) && level > 0)
				layer->setPosition(-offset * level);
		}
#else
		int level, maxLevel = 0;
		foreach (Layer *layer, map0->layers()) {
			if (levelForLayer(layer, &level) && (level > maxLevel))
				maxLevel = level;
		}
		if (maxLevel > 0) {
			QSize size(map0->width() + maxLevel * 3, map0->height() + maxLevel * 3);
			foreach (Layer *layer, map0->layers()) {
				(void) levelForLayer(layer, &level);
				layer->resize(size, offset * (maxLevel - level));
				layer->setPosition(map0->width() - size.width(), map0->height() - size.height());
			}
			map0->setWidth(size.width());
			map0->setHeight(size.height());
		}
#endif
		map0->setOrientation(map1->orientation());
		return;
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
		QMap<MapObject*,ZLot*>::iterator it = mMapObjectToLot.find(mapObject);
		if (it != mMapObjectToLot.end()) {
			ZLot *lot = it.value();
			mMapObjectToLot.erase(it);
			emit lotRemoved(lot, mapObject);
		}
	}
}

} // namespace Tiled