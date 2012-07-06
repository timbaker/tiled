/*
 * tilelayeritem.cpp
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "ztilelayergroupitem.h"

#include "tilelayer.h"
#include "ztilelayergroup.h"
#include "map.h"
#include "mapdocument.h"
#include "maprenderer.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;
using namespace Tiled::Internal;

ZTileLayerGroupItem::ZTileLayerGroupItem(ZTileLayerGroup *layerGroup, MapDocument *mapDoc)
    : mLayerGroup(layerGroup)
    , mMapDocument(mapDoc)
    , mRenderer(mapDoc->renderer())
{
    Q_ASSERT(layerGroup);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

//    syncWithTileLayers();
}

// The opacity, visibility, or name of a layer has changed.
void ZTileLayerGroupItem::tileLayerChanged(TileLayer *layer)
{
    if (mLayerGroup->mLayers.contains(layer)) {
        update(); // force a redraw at the old bounds
        syncWithTileLayers(); // update the bounds
        update(); // force a redraw at the new bounds
    }
}

bool ZTileLayerGroupItem::ownsTileLayer(TileLayer *layer)
{
    return mLayerGroup->mLayers.contains(layer);
}

void ZTileLayerGroupItem::syncWithTileLayers()
{
    prepareGeometryChange();
    QRect tileBounds = mLayerGroup->bounds();
    mBoundingRect = mRenderer->boundingRect(tileBounds, mLayerGroup->level());

    QMargins drawMargins = mLayerGroup->drawMargins();

    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.

    mBoundingRect.adjust(-drawMargins.left(),
                -qMax(0, drawMargins.top() - mMapDocument->map()->tileHeight()),
                qMax(0, drawMargins.right() - mMapDocument->map()->tileWidth()),
                drawMargins.bottom());
}

QRectF ZTileLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void ZTileLayerGroupItem::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *)
{
    if (mBoundingRect.isNull())
        return;
    mRenderer->drawTileLayerGroup(painter, mLayerGroup, option->exposedRect);
#ifdef _DEBUG
#if 0
    Layer *layer = mLayerGroup->mLayers.isEmpty() ? 0 : mLayerGroup->mLayers.first();
    if (!layer || !layer->isVisible()) return;

    qreal left = mRenderer->tileToPixelCoords(0, layer->height(), layer).x();
    qreal top = mRenderer->tileToPixelCoords(0, 0, layer).y();
    qreal right = mRenderer->tileToPixelCoords(layer->width(), 0, layer).x();
    qreal bottom = mRenderer->tileToPixelCoords(layer->width(), layer->height(), layer).y();
    painter->drawRect(left, top, right-left, bottom-top);
#endif
    painter->drawRect(mBoundingRect);
#endif
}
