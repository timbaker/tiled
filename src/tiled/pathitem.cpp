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

#include "pathitem.h"

#include "mapdocument.h"
#include "maprenderer.h"
#include "pathlayer.h"
#include "pathlayeritem.h"

using namespace Tiled;
using namespace Internal;

PathItem::PathItem(Path *path, MapDocument *mapDocument, PathLayerItem *parent)
    : QGraphicsItem(parent)
    , mPath(path)
    , mMapDocument(mapDocument)
    , mEditable(false)
    , mDragging(false)
{
    mColor = Qt::lightGray;
    syncWithPath();
}

QRectF PathItem::boundingRect() const
{
    return mBoundingRect;
}

QPainterPath PathItem::shape() const
{
    return mMapDocument->renderer()->shape(mPath,
                                           mDragging ? mDragOffset : QPoint());
}

void PathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    mMapDocument->renderer()->drawPath(painter, mPath, mColor,
                                       mDragging ? mDragOffset : QPoint());

    if (mEditable) {
        painter->translate(mBoundingRect.topLeft());

        QRectF bounds = mBoundingRect.translated(-mBoundingRect.topLeft());

        QLineF top(bounds.topLeft(), bounds.topRight());
        QLineF left(bounds.topLeft(), bounds.bottomLeft());
        QLineF right(bounds.topRight(), bounds.bottomRight());
        QLineF bottom(bounds.bottomLeft(), bounds.bottomRight());

        QPen dashPen(Qt::DashLine);
        dashPen.setDashOffset(qMax(qreal(0), x()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << top << bottom);

        dashPen.setDashOffset(qMax(qreal(0), y()));
        painter->setPen(dashPen);
        painter->drawLines(QVector<QLineF>() << left << right);
    }
}

void PathItem::setEditable(bool editable)
{
    mEditable = editable;
    update();
}

void PathItem::setDragging(bool dragging)
{
    mDragging = dragging;
    syncWithPath();
}

void PathItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    syncWithPath();
}

void PathItem::syncWithPath()
{
    // Figure out if this is the mOverlayPath used by CreatePathTool.
    bool isToolPath = mPath->pathLayer()->map() == 0;

    setVisible(mPath->isVisible() && (isToolPath ||
               mPath->pathLayer() == mMapDocument->currentLayer()));

    if (mPolygon != mPath->polygon()) {
        mPolygon = mPath->polygon();
        update();
    }

    // FIXME: the bounds must include any tiles generated!
    QRectF bounds = shape().boundingRect().adjusted(-2, -2, 3, 3);;
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}
