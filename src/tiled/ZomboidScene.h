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
#include <QSet>

// TODO: move to ZomboidLayerGroupItem.h
#include <QGraphicsItem>
#include <QStyleOptionGraphicsItem>
#include "maprenderer.h"
#include "tilelayer.h"

namespace Tiled {

class Layer;
class MapObject;
class Tileset;

namespace Internal {

class AbstractTool;
class MapDocument;
class MapObjectItem;
class MapScene;
class ObjectGroupItem;

///// ///// ///// ///// /////

class ZomboidLayerGroupItem : public QGraphicsItem
{
public:
	ZomboidLayerGroupItem(MapRenderer *renderer)
		: mRenderer(renderer)
	{
		setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

		syncWithTileLayer();
	}

	void addLayer(TileLayer *layer)
	{
		if (mLayers.contains(layer))
			return;
		mLayers.append(layer);
		syncWithTileLayer();
	}

	void removeLayer(TileLayer *layer)
	{
		if (mLayers.contains(layer))
			mLayers.remove(mLayers.indexOf(layer));
		syncWithTileLayer();
	}

	void syncWithTileLayer()
	{
		prepareGeometryChange();
		mBoundingRect.setCoords(0,0,0,0);
		foreach (TileLayer *layer, mLayers)
			mBoundingRect |= mRenderer->boundingRect(layer->bounds());
	}

    // QGraphicsItem
    QRectF boundingRect() const
	{
		return mBoundingRect;
	}
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0)
	{
#if 1
		foreach (TileLayer *layer, mLayers)
		{
			mRenderer->drawTileLayer(painter, layer, option->exposedRect);
		}
#else
		// Ask the renderer to draw our TileLayerGroup
		// TileLayerGroup::getOrderedLayers(int x, int y) <<-- this is where the Zomboid render order is calculated
		mRenderer->drawTileLayerGroup(painter, mLayers, option->exposedRect);
#endif
	}

	MapRenderer *mRenderer;
	QRectF mBoundingRect;
	QVector<TileLayer*> mLayers; // All the layers that are on this story (0 story = ground level)
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

signals:
    void selectedObjectItemsChanged();

private slots:
    virtual void refreshScene();

    virtual void layerAdded(int index);
    virtual void layerRemoved(int index);
    virtual void layerChanged(int index);

protected:
    virtual QGraphicsItem *createLayerItem(Layer *layer);
private:
	QMap<int,ZomboidLayerGroupItem*> mLayerGroupItems;
};

} // namespace Internal
} // namespace Tiled

#endif // ZOMBOIDSCENE_H
