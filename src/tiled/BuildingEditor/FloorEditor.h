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
class RoofCornerObject;
class RoofObject;
class Room;

class GraphicsObjectItem;
class GraphicsRoofBaseItem;
class GraphicsRoofCornerItem;
class GraphicsRoofItem;

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

    void synchWithFloor();

private:
    BuildingFloor *mFloor;
    QImage *mBmp;
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
    QPainterPath calcShape();

    void setSelected(bool selected);

    bool isSelected() const
    { return mSelected; }

    void setDragging(bool dragging);
    void setDragOffset(const QPoint &offset);

    void setValidPos(bool valid);
    bool isValidPos() const
    { return mValidPos; }

    virtual GraphicsRoofItem *asRoof() { return 0; }
    virtual GraphicsRoofCornerItem *asRoofCorner() { return 0; }

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
        Width1,
        Width2,
        HeightUp,
        HeightDown,
        Capped,
        Orient
    };
    GraphicsRoofHandleItem(GraphicsRoofBaseItem *roofItem, Type type);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QString statusText() const
    { return mStatusText; }

    void synchWithObject();

    void setHighlight(bool highlight);

private:
    QRectF calcBoundingRect();

private:
    GraphicsRoofBaseItem *mRoofItem;
    Type mType;
    bool mHighlight;
    QString mStatusText;
    QRectF mBoundingRect;
};

class GraphicsRoofBaseItem : public GraphicsObjectItem
{
public:
    GraphicsRoofBaseItem(FloorEditor *editor, BuildingObject *object);

    void setShowHandles(bool show);

    bool handlesVisible() const
    { return mShowHandles; }

protected:
    bool mShowHandles;
};

class GraphicsRoofItem : public GraphicsRoofBaseItem
{
public:
    GraphicsRoofItem(FloorEditor *editor, RoofObject *roof);

    void synchWithObject();

    GraphicsRoofItem *asRoof() { return this; }

    GraphicsRoofHandleItem *resizeHandle() const
    { return mHandleItem; }

    GraphicsRoofHandleItem *width1Handle() const
    { return mWidth1Item; }

    GraphicsRoofHandleItem *width2Handle() const
    { return mWidth2Item; }

    GraphicsRoofHandleItem *heightUpHandle() const
    { return mHeightUpItem; }

    GraphicsRoofHandleItem *heightDownHandle() const
    { return mHeightDownItem; }

    GraphicsRoofHandleItem *cappedHandle() const
    { return mCappedItem; }

private:
    GraphicsRoofHandleItem *mHandleItem;
    GraphicsRoofHandleItem *mWidth1Item;
    GraphicsRoofHandleItem *mWidth2Item;
    GraphicsRoofHandleItem *mHeightUpItem;
    GraphicsRoofHandleItem *mHeightDownItem;
    GraphicsRoofHandleItem *mCappedItem;
};

class GraphicsRoofCornerItem : public GraphicsRoofBaseItem
{
public:
    GraphicsRoofCornerItem(FloorEditor *editor, RoofCornerObject *roof);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void synchWithObject();

    GraphicsRoofCornerItem *asRoofCorner() { return this; }

    GraphicsRoofHandleItem *resizeHandle() const
    { return mHandleItem; }

    GraphicsRoofHandleItem *heightUpHandle() const
    { return mHeightUpItem; }

    GraphicsRoofHandleItem *heightDownHandle() const
    { return mHeightDownItem; }

    GraphicsRoofHandleItem *orientHandle() const
    { return mOrientItem; }

private:
    GraphicsRoofHandleItem *mHandleItem;
    GraphicsRoofHandleItem *mHeightUpItem;
    GraphicsRoofHandleItem *mHeightDownItem;
    GraphicsRoofHandleItem *mOrientItem;
};

class FloorEditor : public QGraphicsScene
{
    Q_OBJECT

public:
    static const int ZVALUE_CURSOR;
    static const int ZVALUE_GRID;

    explicit FloorEditor(QWidget *parent = 0);

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

    void ProcessMove(int x, int y);

    QPoint sceneToTile(const QPointF &scenePos);
    QPointF sceneToTileF(const QPointF &scenePos);
    QRect sceneToTileRect(const QRectF &sceneRect);
    QPointF tileToScene(const QPoint &tilePos);
    QRectF tileToSceneRect(const QPoint &tilePos);
    QRectF tileToSceneRect(const QRect &tileRect);
    QRectF tileToSceneRectF(const QRectF &tileRect);
    bool currentFloorContains(const QPoint &tilePos);

    GraphicsObjectItem *itemForObject(BuildingObject *object);

    QSet<BuildingObject*> objectsInRect(const QRectF &sceneRect);

    BuildingObject *topmostObjectAt(const QPointF &scenePos);

signals:
    void documentChanged();

private slots:
    void currentToolChanged(BaseTool *tool);

    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);

    void floorAdded(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);
    void selectedObjectsChanged();

    void roomChanged(Room *room);
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();

    void buildingResized();
    void buildingRotated();

private:
    BuildingDocument *mDocument;
    GraphicsGridItem *mGridItem;
    QList<GraphicsFloorItem*> mFloorItems;
    QList<GraphicsObjectItem*> mObjectItems;
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
