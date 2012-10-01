#include "pathlayeritem.h"

using namespace Tiled;
using namespace Tiled::Internal;

PathLayerItem::PathLayerItem(PathLayer *pathLayer)
{
    // Since we don't do any painting, we can spare us the call to paint()
    setFlag(QGraphicsItem::ItemHasNoContents);
}

QRectF PathLayerItem::boundingRect() const
{
    return QRectF();
}

void PathLayerItem::paint(QPainter *,
                            const QStyleOptionGraphicsItem *,
                            QWidget *)
{
}
