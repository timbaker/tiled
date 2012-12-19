/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef BETTERSELECTIONRECTANGLE_H
#define BETTERSELECTIONRECTANGLE_H

#include <QGraphicsItem>

namespace Tiled {

class MapRenderer;

namespace Internal {

/**
 * Like SelectionRectangle, but works better for isometric views.
 */
class BetterSelectionRectangle : public QGraphicsItem
{
public:
    BetterSelectionRectangle(QGraphicsItem *parent = 0);

    void setRenderer(MapRenderer *renderer)
    {
        mRenderer = renderer;
    }

    void setRectangle(const QRectF &rectangle);

    QRectF boundingRect() const
    { return mBoundingRect; }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    QPolygonF tileRectToPolygon(const QRectF &rect, int level) const;

    QPolygonF polygon() const
    { return mPolygon; }

private:
    MapRenderer *mRenderer;
    QPolygonF mPolygon;
    QRectF mBoundingRect;
};

} // namespace Internal
} // namespace Tiled

#endif // BETTERSELECTIONRECTANGLE_H
