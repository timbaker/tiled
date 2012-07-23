/*
 * zgriditem.h
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

#ifndef ZGRIDITEM_H
#define ZGRIDITEM_H

#include <QGraphicsItem>

namespace Tiled {

class Layer;

namespace Internal {

class MapDocument;

/**
 * The tile grid.
 */
class ZGridItem : public QGraphicsItem
{
public:
    ZGridItem();

    void setMapDocument(MapDocument *mapDocument);
	void currentLayerIndexChanged();

    // QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    void updateBoundingRect();

	QRectF mBoundingRect;
    MapDocument *mMapDocument;
};

} // namespace Internal
} // namespace Tiled

#endif // ZGRIDITEM_H
