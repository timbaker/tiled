/*
 * ZomboidScene.h
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

#ifndef ZOMBOIDSCENE_H
#define ZOMBOIDSCENE_H

#include "mapscene.h"
#include <QMap>

#include "zlotmanager.hpp"
#include "ztilelayergroup.h"
#include "ztilelayergroupitem.h"

namespace Tiled {

class Layer;
class MapObject;
class Tileset;
class ZLot;

namespace Internal {

///// ///// ///// ///// /////

class ZomboidScene;
class ZomboidTileLayerGroup : public ZTileLayerGroup
{
public:
	ZomboidTileLayerGroup(ZomboidScene *mapScene, int level);
	bool orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const;

	// Layer
	virtual QRect bounds() const;

	// TileLayer
	virtual QMargins drawMargins() const;

	// ZTileLayerGroup
	virtual void prepareDrawing(const MapRenderer *renderer, const QRect &rect);

	ZomboidScene *mMapScene;
	int mLevel;

	struct LotLayers
	{
		LotLayers()
		{
			mLayerGroup = 0;
		}
		LotLayers(const QPoint& pos, const ZTileLayerGroup *layerGroup)
			: mMapObjectPos(pos)
			, mLayerGroup(layerGroup)
		{
		}
		QPoint mMapObjectPos;
		const ZTileLayerGroup *mLayerGroup;
	};
	QVector<LotLayers> mPreparedLotLayers;
};

///// ///// ///// ///// /////
/**
 * A graphics scene that represents the contents of a map.
 */
class ZomboidScene : public MapScene
{
    Q_OBJECT

public:
    /**
     * Constructor.
     */
    ZomboidScene(QObject *parent);

    /**
     * Destructor.
     */
    ~ZomboidScene();

	// accessed by ZomboidTileLayerGroup
	QList<MapObject*> mLotMapObjects;
	QMap<MapObject*,ZLot*> mMapObjectToLot;

private slots:
    virtual void refreshScene();

    virtual void layerAdded(int index);
    virtual void layerAboutToBeRemoved(int index);
    virtual void layerRemoved(int index);
    virtual void layerChanged(int index);
    virtual void layerRenamed(int index);

	void onLotAdded(ZLot *lot, MapObject *mapObject);
	void onLotRemoved(ZLot *lot, MapObject *mapObject);
	void onLotUpdated(ZLot *lot, MapObject *mapObject);
protected:
    virtual QGraphicsItem *createLayerItem(Layer *layer);
	bool groupForTileLayer(TileLayer *tl, uint *group);
private:
	QMap<int,ZTileLayerGroupItem*> mTileLayerGroupItems;
	ZLotManager mLotManager;
};

} // namespace Internal
} // namespace Tiled

#endif // ZOMBOIDSCENE_H
