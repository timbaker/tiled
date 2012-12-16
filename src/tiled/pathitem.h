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

    void setEditable(bool editable);

    bool isEditable() const
    { return mEditable; }

    void setDragging(bool dragging);

    void setDragOffset(const QPoint &offset);

    QPoint dragOffset() const
    { return mDragOffset; }

    void syncWithPath();

private:
    Path *mPath;
    MapDocument *mMapDocument;
    QRectF mBoundingRect;
    QColor mColor;
    bool mEditable;
    bool mDragging;
    QPoint mDragOffset;
    QPolygon mPolygon;
};

} // namespace Internal
} // namespace Tiled

#endif // PATHITEM_H
