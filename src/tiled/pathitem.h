#ifndef PATHITEM_H
#define PATHITEM_H

#include <QGraphicsItem>

namespace Tiled {
class Path;

namespace Internal {
class MapDocument;
class PathLayerItem;

class PathItem : public QGraphicsItem
{
public:
    PathItem(Path *path, MapDocument *mapDocument, PathLayerItem *parent = 0);

    // QGraphicsItem
    QRectF boundingRect() const;
    QPainterPath shape() const;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    Path *path() const
    { return mPath; }

private:
    Path *mPath;
    MapDocument *mMapDocument;
    QRectF mBoundingRect;
    QColor mColor;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHITEM_H
