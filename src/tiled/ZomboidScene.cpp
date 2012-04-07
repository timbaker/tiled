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

static const qreal darkeningFactor = 0.6;
static const qreal opacityFactor = 0.4;


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
	foreach (ZomboidLayerGroupItem *grp, mLayerGroupItems)
		delete grp;
	mLayerGroupItems.clear();

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
			if (mLayerGroupItems[group] == 0)
			{
				mLayerGroupItems[group] = new ZomboidLayerGroupItem(mMapDocument->renderer());
				addItem(mLayerGroupItems[group]);
			}
			mLayerGroupItems[group]->addLayer(tl);
		}
		return new DummyGraphicsItem();
	}
	return MapScene::createLayerItem(layer);
}

void ZomboidScene::layerAdded(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = createLayerItem(layer);
    addItem(layerItem);
    mLayerItems.insert(index, layerItem);

    int z = 0;
    foreach (QGraphicsItem *item, mLayerItems)
        item->setZValue(z++);
}

void ZomboidScene::layerRemoved(int index)
{
    delete mLayerItems.at(index);
    mLayerItems.remove(index);
}

/**
 * A layer has changed. This can mean that the layer visibility or opacity has
 * changed.
 */
void ZomboidScene::layerChanged(int index)
{
    const Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = mLayerItems.at(index);

    layerItem->setVisible(layer->isVisible());

    qreal multiplier = 1;
    if (mHighlightCurrentLayer && mMapDocument->currentLayerIndex() < index)
        multiplier = opacityFactor;

    layerItem->setOpacity(layer->opacity() * multiplier);
}
