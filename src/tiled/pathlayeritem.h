#ifndef PATHLAYERITEM_H
#define PATHLAYERITEM_H

#include <QGraphicsItem>

namespace Tiled {
class PathLayer;

namespace Internal {

class PathLayerItem : public QGraphicsItem
{
public:
    PathLayerItem(PathLayer *pathLayer);

    PathLayer *pathLayer() const
    { return mPathLayer; }

    // QGraphicsItem
    QRectF boundingRect() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

private:
    PathLayer *mPathLayer;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHLAYERITEM_H
