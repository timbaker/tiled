/*
 * ztilelayergroup.h
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

#ifndef ZTILELAYERGROUPITEM_H
#define ZTILELAYERGROUPITEM_H

#include <QGraphicsItem>

namespace Tiled {

class MapRenderer;
class TileLayer;
class ZTileLayerGroup;

namespace Internal {

class MapDocument;

class ZTileLayerGroupItem : public QGraphicsItem
{
public:
    ZTileLayerGroupItem(ZTileLayerGroup *layerGroup, MapDocument *mapDoc);

	void tileLayerChanged(TileLayer *layer);

	bool ownsTileLayer(TileLayer *layer);

    virtual void syncWithTileLayers();

	ZTileLayerGroup *getTileLayerGroup() const { return mLayerGroup; }

    // QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

protected:
    ZTileLayerGroup *mLayerGroup;
    MapDocument *mMapDocument;
	MapRenderer *mRenderer;
    QRectF mBoundingRect;
};

} // namespace Internal
} // namespace Tiled

#endif // ZTILELAYERGROUPITEM_H
