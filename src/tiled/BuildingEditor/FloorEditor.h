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

#ifndef FLOOREDITOR_H
#define FLOOREDITOR_H

#include <QGraphicsItem>
//#include <QGraphicsEllipseItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QSet>

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingObject;
class BaseTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class FloorEditor;
class RoofObject;
class Room;
class WallObject;

class GraphicsObjectItem;
class GraphicsRoofBaseItem;
class GraphicsRoofCornerItem;
class GraphicsRoofItem;
class GraphicsWallItem;

/////

class GraphicsFloorItem : public QGraphicsItem
{
public:
    GraphicsFloorItem(BuildingFloor *floor);
    ~GraphicsFloorItem();

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    BuildingFloor *floor() const
    { return mFloor; }

    QImage *bmp() const
    { return mBmp; }

    const QList<GraphicsObjectItem*> &objectItems() const
    { return mObjectItems; }

    void objectAdded(GraphicsObjectItem *item);
    void objectAboutToBeRemoved(GraphicsObjectItem *item);
    GraphicsObjectItem *itemForObject(BuildingObject *object) const;

    void synchWithFloor();

    void setDragBmp(QImage *bmp);

    QImage *dragBmp() const
    { return mDragBmp; }

    void showObjectsChanged(bool show);

private:
    BuildingFloor *mFloor;
    QImage *mBmp;
    QImage *mDragBmp;
    QList<GraphicsObjectItem*> mObjectItems;
};

class GraphicsGridItem : public QGraphicsItem
{
public:
    GraphicsGridItem(int width, int height);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSize(int width, int height);

private:
    int mWidth, mHeight;
};

class GraphicsObjectItem : public QGraphicsItem
{
public:
    GraphicsObjectItem(FloorEditor *editor, BuildingObject *object);

    QPainterPath shape() const;

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setObject(BuildingObject *object);

    BuildingObject *object() const
    { return mObject; }

    virtual void synchWithObject();
    virtual QPainterPath calcShape();

    void setSelected(bool selected);

    bool isSelected() const
    { return mSelected; }

    void setDragging(bool dragging);
    void setDragOffset(const QPoint &offset);

    bool isValidPos() const
    { return mValidPos; }

    virtual GraphicsRoofItem *asRoof() { return 0; }
    virtual GraphicsRoofCornerItem *asRoofCorner() { return 0; }
    virtual GraphicsWallItem *asWall() { return 0; }

protected:
    void initialize();

protected:
    FloorEditor *mEditor;
    BuildingObject *mObject;
    QRectF mBoundingRect;
    bool mSelected;
    bool mDragging;
    QPoint mDragOffset;
    QPainterPath mShape;
    bool mValidPos;
};

class GraphicsRoofHandleItem : public QGraphicsItem
{
public:
    enum Type {
        Resize,
        DepthUp,
        DepthDown,
        CappedW,
        CappedN,
        CappedE,
        CappedS,
        Orient
    };
    GraphicsRoofHandleItem(GraphicsRoofItem *roofItem, Type type);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QString statusText() const
    { return mStatusText; }

    void synchWithObject();

    void setHighlight(bool highlight);

private:
    QRectF calcBoundingRect();

private:
    GraphicsRoofItem *mRoofItem;
    Type mType;
    bool mHighlight;
    QString mStatusText;
    QRectF mBoundingRect;
};

class GraphicsRoofItem : public GraphicsObjectItem
{
public:
    GraphicsRoofItem(FloorEditor *editor, RoofObject *roof);

    void synchWithObject();

    GraphicsRoofItem *asRoof() { return this; }

    void setShowHandles(bool show);

    bool handlesVisible() const
    { return mShowHandles; }

    GraphicsRoofHandleItem *resizeHandle() const
    { return mResizeItem; }

    GraphicsRoofHandleItem *depthUpHandle() const
    { return mDepthUpItem; }

    GraphicsRoofHandleItem *depthDownHandle() const
    { return mDepthDownItem; }

    GraphicsRoofHandleItem *cappedWHandle() const
    { return mCappedWItem; }

    GraphicsRoofHandleItem *cappedNHandle() const
    { return mCappedNItem; }

    GraphicsRoofHandleItem *cappedEHandle() const
    { return mCappedEItem; }

    GraphicsRoofHandleItem *cappedSHandle() const
    { return mCappedSItem; }

private:
    bool mShowHandles;
    GraphicsRoofHandleItem *mResizeItem;
    GraphicsRoofHandleItem *mDepthUpItem;
    GraphicsRoofHandleItem *mDepthDownItem;
    GraphicsRoofHandleItem *mCappedWItem;
    GraphicsRoofHandleItem *mCappedNItem;
    GraphicsRoofHandleItem *mCappedEItem;
    GraphicsRoofHandleItem *mCappedSItem;
};

class GraphicsWallHandleItem : public QGraphicsItem
{
public:
    GraphicsWallHandleItem(GraphicsWallItem *wallItem);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void synchWithObject();

    void setHighlight(bool highlight);

private:
    QRectF calcBoundingRect();

private:
    GraphicsWallItem *mWallItem;
    bool mHighlight;
    QRectF mBoundingRect;
};

class GraphicsWallItem : public GraphicsObjectItem
{
public:
    GraphicsWallItem(FloorEditor *editor, WallObject *wall);

    void synchWithObject();
    QPainterPath calcShape();

    GraphicsWallItem *asWall() { return this; }

    void setShowHandles(bool show);

    bool handlesVisible() const
    { return mShowHandles; }

    GraphicsWallHandleItem *resizeHandle() const
    { return mResizeItem; }

private:
    bool mShowHandles;
    GraphicsWallHandleItem *mResizeItem;
};

class FloorEditor : public QGraphicsScene
{
    Q_OBJECT

public:
    static const int ZVALUE_CURSOR;
    static const int ZVALUE_GRID;

    explicit FloorEditor(QWidget *parent = 0);

    bool eventFilter(QObject *watched, QEvent *event);

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
    { mousePressEvent(event); }

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    BuildingDocument *document() const
    { return mDocument; }

    Building *building() const;

    QPoint sceneToTile(const QPointF &scenePos);
    QPointF sceneToTileF(const QPointF &scenePos);
    QRect sceneToTileRect(const QRectF &sceneRect);
    QPointF tileToScene(const QPoint &tilePos);
    QRectF tileToSceneRect(const QPoint &tilePos);
    QRectF tileToSceneRect(const QRect &tileRect);
    QRectF tileToSceneRectF(const QRectF &tileRect);
    bool currentFloorContains(const QPoint &tilePos);

    GraphicsFloorItem *itemForFloor(BuildingFloor *floor);
    GraphicsObjectItem *itemForObject(BuildingObject *object);

    QSet<BuildingObject*> objectsInRect(const QRectF &sceneRect);

    BuildingObject *topmostObjectAt(const QPointF &scenePos);

    GraphicsObjectItem *createItemForObject(BuildingObject *object);

signals:
    void documentChanged();

private slots:
    void currentToolChanged(BaseTool *tool);

    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);
    void objectChanged(BuildingObject *object);

    void selectedObjectsChanged();

    void roomChanged(Room *room);
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();

    void buildingResized();
    void buildingRotated();

    void showObjectsChanged(bool show);

private:
    BuildingDocument *mDocument;
    GraphicsGridItem *mGridItem;
    QList<GraphicsFloorItem*> mFloorItems;
    QSet<GraphicsObjectItem*> mSelectedObjectItems;
    BaseTool *mCurrentTool;
};

class FloorView : public QGraphicsView
{
    Q_OBJECT
public:
    FloorView(QWidget *parent = 0);

    FloorEditor *scene() const
    { return dynamic_cast<FloorEditor*>(QGraphicsView::scene()); }

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

signals:
    void mouseCoordinateChanged(const QPoint &tilePos);

private slots:
    void adjustScale(qreal scale);

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    QPoint mLastMouseTilePos;
};

} // namespace BuildingEditor

#endif // FLOOREDITOR_H
