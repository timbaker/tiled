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

#include "betterselectionrectangle.h"

#include "maprenderer.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>

using namespace Tiled::Internal;

BetterSelectionRectangle::BetterSelectionRectangle(QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mRenderer(0)
{
    setZValue(10000);
}

void BetterSelectionRectangle::setRectangle(const QRectF &rectangle)
{
    QPolygonF polygon = tileRectToPolygon(rectangle, 0);
    if (polygon != mPolygon) {
        prepareGeometryChange();
        mPolygon = polygon;
        mBoundingRect = mPolygon.boundingRect().adjusted(-4, -4, 4, 4);
    }
}

void BetterSelectionRectangle::paint(QPainter *painter,
                                     const QStyleOptionGraphicsItem *,
                                     QWidget *)
{
    if (mPolygon.isEmpty())
        return;

    // Draw a shadow
    QColor black(Qt::black);
    black.setAlpha(128);
    QPen pen(black, 2, Qt::DotLine);
    painter->setPen(pen);
    painter->drawPolygon(mPolygon.translated(1, 1));

    // Draw a rectangle in the highlight color
    QColor highlight = QApplication::palette().highlight().color();
    pen.setColor(highlight);
    highlight.setAlpha(32);
    painter->setPen(pen);
    painter->setBrush(highlight);
    painter->drawPolygon(mPolygon);

}

QPolygonF BetterSelectionRectangle::tileRectToPolygon(const QRectF &rect, int level) const
{
    QPolygonF polygon;
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.topLeft(), level));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.topRight(), level));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.bottomRight(), level));
    polygon << QPointF(mRenderer->tileToPixelCoords(rect.bottomLeft(), level));
    polygon << polygon[0];
    return polygon;
}
