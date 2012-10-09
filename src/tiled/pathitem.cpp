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
    mBoundingRect = shape().boundingRect().adjusted(-2, -2, 3, 3);;
    mColor = Qt::lightGray;
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
    setVisible(mPath->isVisible());

    // FIXME: the bounds must include any tiles generated!
    QRectF bounds = shape().boundingRect().adjusted(-2, -2, 3, 3);;
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}
