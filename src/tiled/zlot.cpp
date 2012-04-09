/*
 * zlot.cpp
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

#include "zlot.hpp"

#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "tilelayer.h"
#include "ztilelayergroup.h"
#include "ztilelayergroupitem.h"

namespace Tiled {

class LotTileLayerGroup : public ZTileLayerGroup
{
public:
	LotTileLayerGroup(MapObject *mapObject, MapRenderer *mapRenderer)
		: mMapObject(mapObject)
		, mMapRenderer(mapRenderer)
	{
	}

	QRect bounds() const
	{
		QRect r;
		foreach (TileLayer *tl, mLayers)
			r |= tl->bounds();
		QPoint mapObjectPos = mMapObject->position().toPoint(); // tile coords of the map object
		r.translate(mapObjectPos);
		return r;
	}

	virtual bool orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const
	{
		QPoint mapObjectPos = mMapObject->position().toPoint(); // tile coords of the map object
		cells.clear();
		foreach (TileLayer *tl, mLayers)
		{
//			if (!tl->isVisible())
//				continue;
#if 0 // DO NOT USE - VERY SLOW
			if (tl->isEmpty())
				continue;
#endif
			QPoint pos = point - tl->position() - mapObjectPos;
			if (tl->contains(pos))
				cells.append(&tl->cellAt(pos));
		}
		return !cells.empty();
	}

	MapObject *mMapObject;
	MapRenderer *mMapRenderer;
};

class LotTileLayerGroupItem : public Internal::ZTileLayerGroupItem
{
public:
	LotTileLayerGroupItem(MapObject *mapObject, ZTileLayerGroup *layerGroup, MapRenderer *renderer)
		: Internal::ZTileLayerGroupItem(layerGroup, renderer)
		, mMapObject(mapObject)
	{
	}

	virtual void syncWithTileLayers()
	{
		prepareGeometryChange();
		QRect tileBounds(0,0,-1,-1);
		foreach (TileLayer *tl, mLayerGroup->mLayers)
			tileBounds |= tl->bounds();

		QPoint mapObjectPos = mMapObject->position().toPoint(); // tile coords of the map object
		tileBounds.translate(mapObjectPos);
		mBoundingRect = mRenderer->boundingRect(tileBounds);
	}

	MapObject *mMapObject;
};

// Not derived from QGraphicsItem
class ZLotSceneItem
{
public:
	ZLotSceneItem(Map *map)
		: mMap(map)
	{
	}
	// FIXME: duplicated in ZomboidScene
	bool groupForTileLayer(TileLayer *tl, uint *group)
	{
		// See if the layer name matches "0_foo" or "1_bar" etc.
		const QString& name = tl->name();
		QStringList sl = name.trimmed().split(QLatin1Char('_'));
		if (sl.count() > 1 && !sl[1].isEmpty()) {
			bool conversionOK;
			(*group) = sl[0].toUInt(&conversionOK);
			return conversionOK;
		}
		return false;
	}
	void addToScene(Internal::MapScene *mapScene, MapObject *mapObject)
	{
		foreach (Layer *layer, mMap->layers()) {
			if (TileLayer *tl = layer->asTileLayer()) {
				uint group;
				if (groupForTileLayer(tl, &group)) {
					if (mTileLayerGroupItems[group] == 0) {
						ZTileLayerGroup *layerGroup = new LotTileLayerGroup(mapObject, mapScene->mapDocument()->renderer());
						mTileLayerGroupItems[group] = new LotTileLayerGroupItem(mapObject, layerGroup, mapScene->mapDocument()->renderer());
						mapScene->addItem(mTileLayerGroupItems[group]);
					}
					int index = mMap->layers().indexOf(layer);
					mTileLayerGroupItems[group]->addTileLayer(tl, index);
				}
			}
		}
	}
	void removeFromScene(Internal::MapScene *mapScene, MapObject *mapObject)
	{
		foreach (Internal::ZTileLayerGroupItem *tlgi, mTileLayerGroupItems) {
			delete tlgi;
		}
	}
	void updateInScene(Internal::MapScene *mapScene, MapObject *mapObject)
	{
		foreach (Internal::ZTileLayerGroupItem *tlgi, mTileLayerGroupItems) {
			tlgi->syncWithTileLayers(); // update bounds
		}
	}
	Map *mMap;
	QMap<int,Internal::ZTileLayerGroupItem*> mTileLayerGroupItems;
};

ZLot::ZLot(Map *map)
	: mMap(map)
{
}

ZLot::~ZLot()
{
}

void ZLot::addToScene(Internal::MapScene *mapScene, MapObject *mapObject)
{
	Q_ASSERT(mMapObjectToSceneItem.keys().contains(mapObject) == false);
	mMapObjectToSceneItem[mapObject] = new ZLotSceneItem(mMap);
	mMapObjectToSceneItem[mapObject]->addToScene(mapScene, mapObject);
}

void ZLot::removeFromScene(Internal::MapScene *mapScene, MapObject *mapObject)
{
	Q_ASSERT(mMapObjectToSceneItem.keys().contains(mapObject) == true);
	mMapObjectToSceneItem[mapObject]->removeFromScene(mapScene, mapObject);
	delete mMapObjectToSceneItem[mapObject];
	QMap<MapObject*,ZLotSceneItem*>::iterator it = mMapObjectToSceneItem.find(mapObject);
	mMapObjectToSceneItem.erase(it);
}

void ZLot::updateInScene(Internal::MapScene *mapScene, MapObject *mapObject)
{
	Q_ASSERT(mMapObjectToSceneItem.keys().contains(mapObject) == true);
	mMapObjectToSceneItem[mapObject]->updateInScene(mapScene, mapObject);
}

} // namespace Tiled