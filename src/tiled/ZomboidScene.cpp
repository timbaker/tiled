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
	DummyGraphicsItem()
		: QGraphicsItem()
	{
		// Since we don't do any painting, we can spare us the call to paint()
		setFlag(QGraphicsItem::ItemHasNoContents);
	}
		
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
	if (TileLayer *tl = layer->asTileLayer()) {
		uint group;
		if (groupForTileLayer(tl, &group)) {
			if (mTileLayerGroupItems[group] == 0) {
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

bool ZomboidScene::groupForTileLayer(TileLayer *tl, uint *group)
{
	// See if the layer name matches "0_foo" or "1_bar" etc.
	const QString& name = tl->name();
	QStringList sl = name.trimmed().split(QLatin1Char('_'));
	if (sl.count() > 1) {
		bool conversionOK;
		(*group) = sl[0].toUInt(&conversionOK);
		return (conversionOK && !sl[1].isEmpty());
	}
	return false;
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
	if (TileLayer *tl = layer->asTileLayer()) {
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
 * A layer has changed. This can mean that the layer visibility, opacity or
 * name has changed.
 */
void ZomboidScene::layerChanged(int index)
{
#if 1
	MapScene::layerChanged(index);

    Layer *layer = mMapDocument->map()->layerAt(index);
	if (TileLayer *tl = layer->asTileLayer()) {
		foreach (ZTileLayerGroupItem *layerGroup, mTileLayerGroupItems) {
			layerGroup->tileLayerChanged(tl);
		}
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

void ZomboidScene::layerRenamed(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
	if (TileLayer *tl = layer->asTileLayer()) {
		uint group;
		bool hasGroup = groupForTileLayer(tl, &group);
		ZTileLayerGroupItem *oldOwner = 0, *newOwner = 0;

		// Find the old TileLayerGroup owner
		foreach (ZTileLayerGroupItem *layerGroup, mTileLayerGroupItems) {
			if (layerGroup->ownsTileLayer(tl)) {
				oldOwner = layerGroup;
				break;
			}
		}

		// Find (or create) the new TileLayerGroup owner
		if (hasGroup && mTileLayerGroupItems[group] == 0) {
			ZTileLayerGroup *layerGroup = new ZomboidTileLayerGroup();
			mTileLayerGroupItems[group] = new ZTileLayerGroupItem(layerGroup, mMapDocument->renderer());
			addItem(mTileLayerGroupItems[group]);
		}
		if (hasGroup)
			newOwner = mTileLayerGroupItems[group];

		// Handle rename changing ownership
		if (oldOwner != newOwner) {
			if (oldOwner)
				oldOwner->removeTileLayer(tl);
			if (newOwner)
				newOwner->addTileLayer(tl, index);

			// If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
			// managed by the base class.
			// If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
			// managed by the base class (MapScene) See createLayerItem().
			if (oldOwner && !newOwner) {
				delete mLayerItems[index]; // DummyGraphicsItem
				mLayerItems[index] = new TileLayerItem(tl, mMapDocument->renderer());
				mLayerItems[index]->setVisible(layer->isVisible());
				addItem(mLayerItems[index]);
			}
			if (!oldOwner && newOwner) {
				delete mLayerItems[index]; // TileLayerItem
				mLayerItems[index] = new DummyGraphicsItem();
				addItem(mLayerItems[index]);
			}
		}
	}
#if 0
    int z = 0;
    foreach (QGraphicsItem *item, mLayerItems)
        item->setZValue(z++);
#endif
}