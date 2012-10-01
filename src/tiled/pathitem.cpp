#include "pathitem.h"

#include "mapdocument.h"
#include "maprenderer.h"
#include "pathlayeritem.h"

using namespace Tiled;
using namespace Internal;

PathItem::PathItem(Path *path, MapDocument *mapDocument, PathLayerItem *parent)
    : QGraphicsItem(parent)
    , mPath(path)
    , mMapDocument(mapDocument)
{
    mBoundingRect = shape().boundingRect();
    mColor = Qt::lightGray;
}

QRectF PathItem::boundingRect() const
{
    return mBoundingRect;
}

QPainterPath PathItem::shape() const
{
    return mMapDocument->renderer()->shape(mPath);
}

void PathItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                     QWidget *widget)
{
    Q_UNUSED(widget)
    mMapDocument->renderer()->drawPath(painter, mPath, mColor);
}
