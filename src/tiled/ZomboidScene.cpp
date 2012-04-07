/*
 * ZomboidScene.cpp
 * Copyright 2008-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
 *
 * This file is part of Tiled.
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

#include "ZomboidScene.h"

#include "map.h"
#include "mapdocument.h"
#include "tilelayer.h"
#include "tilelayeritem.h"

using namespace Tiled;
using namespace Tiled::Internal;

///// ///// ///// ///// /////

ZomboidTileLayerGroup::ZomboidTileLayerGroup()
	: ZTileLayerGroup()
{
}

bool ZomboidTileLayerGroup::orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const
{
	cells.clear();
	foreach (TileLayer *tl, mLayers)
	{
		if (!tl->isVisible())
			continue;
		QPoint pos = point - tl->position();
		if (tl->contains(pos))
			cells.append(&tl->cellAt(pos));
	}
	return !cells.empty();
}

///// ///// ///// ///// /////

ZomboidScene::ZomboidScene(QObject *parent)
    : MapScene(parent)
{
}

ZomboidScene::~ZomboidScene()
{
	// delete mLayerGroupItems[0-10]
}

void ZomboidScene::refreshScene()
{
	foreach (ZTileLayerGroupItem *grp, mTileLayerGroupItems)
		delete grp;
	mTileLayerGroupItems.clear();

	MapScene::refreshScene();
}

class DummyGraphicsItem : public QGraphicsItem
{
public:
    // QGraphicsItem
    QRectF boundingRect() const
	{
		return QRectF();
	}
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0)
	{
	}
};

QGraphicsItem *ZomboidScene::createLayerItem(Layer *layer)
{
	if (TileLayer *tl = layer->asTileLayer())
	{
		const QString& name = layer->name();
		if (name.at(0).isDigit() && name.at(1).toAscii() == '_')
		{
			int group = name.at(0).digitValue();
			if (mTileLayerGroupItems[group] == 0)
			{
				ZTileLayerGroup *layerGroup = new ZomboidTileLayerGroup();
				mTileLayerGroupItems[group] = new ZTileLayerGroupItem(layerGroup, mMapDocument->renderer());
				addItem(mTileLayerGroupItems[group]);
			}
			int index = mMapDocument->map()->layers().indexOf(layer);
			mTileLayerGroupItems[group]->addTileLayer(tl, index);
			return new DummyGraphicsItem();
		}
	}
	return MapScene::createLayerItem(layer);
}

void ZomboidScene::layerAdded(int index)
{
#if 1
	MapScene::layerAdded(index);
#else
    Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = createLayerItem(layer);
    addItem(layerItem);
    mLayerItems.insert(index, layerItem);

    int z = 0;
    foreach (QGraphicsItem *item, mLayerItems)
        item->setZValue(z++);
#endif
}

void ZomboidScene::layerAboutToBeRemoved(int index)
{
	Layer *layer = mMapDocument->map()->layerAt(index);
	if (TileLayer *tl = layer->asTileLayer())
	{
		foreach (ZTileLayerGroupItem *grp, mTileLayerGroupItems)
			grp->removeTileLayer(tl);
	}
}

void ZomboidScene::layerRemoved(int index)
{
#if 1
	MapScene::layerRemoved(index);
#else
	delete mLayerItems.at(index);
	mLayerItems.remove(index);
#endif
}

/**
 * A layer has changed. This can mean that the layer visibility or opacity has
 * changed.
 */
void ZomboidScene::layerChanged(int index)
{
#if 1
	MapScene::layerChanged(index);

	foreach (ZTileLayerGroupItem *grp, mTileLayerGroupItems) {
		grp->update(grp->boundingRect());
	}
#else
    const Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = mLayerItems.at(index);

    layerItem->setVisible(layer->isVisible());

    qreal multiplier = 1;
    if (mHighlightCurrentLayer && mMapDocument->currentLayerIndex() < index)
        multiplier = opacityFactor;

    layerItem->setOpacity(layer->opacity() * multiplier);
#endif
}
