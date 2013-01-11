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
class FloorTileGrid;
class RoofObject;
class Room;
class WallObject;

class BaseFloorEditor;
class GraphicsObjectItem;
class GraphicsRoofBaseItem;
class GraphicsRoofCornerItem;
class GraphicsRoofItem;
class GraphicsWallItem;
class IsoBuildingRenderer;
class OrthoBuildingRenderer;

/////

class GraphicsFloorItem : public QGraphicsItem
{
public:
    GraphicsFloorItem(BaseFloorEditor *editor, BuildingFloor *floor);
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

    void mapResized();

    void floorEdited();
    void roomChanged(Room *room);
    void roomAtPositionChanged(const QPoint &pos);

    void setDragBmp(QImage *bmp);

    QImage *dragBmp() const
    { return mDragBmp; }

    void synchVisibility();

private:
    BaseFloorEditor *mEditor;
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
    GraphicsObjectItem(BaseFloorEditor *editor, BuildingObject *object);

    QPainterPath shape() const;

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    BaseFloorEditor *editor() const
    { return mEditor; }

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

    void setMouseOver(bool mouseOver);

    bool mouseOver() const
    { return mMouseOver; }

    bool isValidPos() const
    { return mValidPos; }

    virtual GraphicsRoofItem *asRoof() { return 0; }
    virtual GraphicsRoofCornerItem *asRoofCorner() { return 0; }
    virtual GraphicsWallItem *asWall() { return 0; }

protected:
    void initialize();

protected:
    BaseFloorEditor *mEditor;
    BuildingObject *mObject;
    QRectF mBoundingRect;
    bool mSelected;
    bool mDragging;
    QPoint mDragOffset;
    QPainterPath mShape;
    bool mValidPos;
    bool mMouseOver;
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

    QPainterPath shape() const;

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QString statusText() const
    { return mStatusText; }

    void synchWithObject();

    void setHighlight(bool highlight);

private:
    QRectF calcBoundingRect();

private:
    BaseFloorEditor *mEditor;
    GraphicsRoofItem *mRoofItem;
    Type mType;
    bool mHighlight;
    QString mStatusText;
    QRectF mTileBounds;
    QRectF mBoundingRect;
};

class GraphicsRoofItem : public GraphicsObjectItem
{
public:
    GraphicsRoofItem(BaseFloorEditor *editor, RoofObject *roof);

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
    QRectF mTileRect;
    QRectF mBoundingRect;
};

class GraphicsWallItem : public GraphicsObjectItem
{
public:
    GraphicsWallItem(BaseFloorEditor *editor, WallObject *wall);

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

class BuildingRenderer
{
public:
    virtual QPoint sceneToTile(const QPointF &scenePos, int level) = 0;
    virtual QPointF sceneToTileF(const QPointF &scenePos, int level) = 0;
    virtual QRect sceneToTileRect(const QRectF &sceneRect, int level) = 0;
    virtual QRectF sceneToTileRectF(const QRectF &sceneRect, int level) = 0;
    virtual QPointF tileToScene(const QPoint &tilePos, int level) = 0;
    virtual QPointF tileToSceneF(const QPointF &tilePos, int level) = 0;
    virtual QPolygonF tileToScenePolygon(const QPoint &tilePos, int level) = 0;
    virtual QPolygonF tileToScenePolygon(const QRect &tileRect, int level) = 0;
    virtual QPolygonF tileToScenePolygonF(const QRectF &tileRect, int level) = 0;
    virtual QPolygonF tileToScenePolygon(const QPolygonF &tilePolygon, int level) = 0;

    virtual void drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level) = 0;

    void drawLine(QPainter *painter, const QPointF &p1, const QPointF &p2, int level)
    { drawLine(painter, p1.x(), p1.y(), p2.x(), p2.y(), level); }

    virtual void drawObject(QPainter *painter, BuildingObject *mObject,
                            const QPoint &mDragOffset, bool mValidPos,
                            bool mSelected, bool mMouseOver, int level);

    virtual OrthoBuildingRenderer *asOrtho() { return 0; }
    virtual IsoBuildingRenderer *asIso() { return 0; }
};

class OrthoBuildingRenderer : public BuildingRenderer
{
public:
    QPoint sceneToTile(const QPointF &scenePos, int level);
    QPointF sceneToTileF(const QPointF &scenePos, int level);
    QRect sceneToTileRect(const QRectF &sceneRect, int level);
    QRectF sceneToTileRectF(const QRectF &sceneRect, int level);
    QPointF tileToScene(const QPoint &tilePos, int level);
    QPointF tileToSceneF(const QPointF &tilePos, int level);
    QPolygonF tileToScenePolygon(const QPoint &tilePos, int level);
    QPolygonF tileToScenePolygon(const QRect &tileRect, int level);
    QPolygonF tileToScenePolygonF(const QRectF &tileRect, int level);
    QPolygonF tileToScenePolygon(const QPolygonF &tilePolygon, int level);

    void drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level);

    OrthoBuildingRenderer *asOrtho() { return this; }
};

class BaseFloorEditor : public QGraphicsScene
{
    Q_OBJECT
public:
    int ZVALUE_CURSOR;
    int ZVALUE_GRID;

    BaseFloorEditor(QObject *parent = 0);

    BuildingDocument *document() const
    { return mDocument; }

    Building *building() const;

    int currentLevel() const;
    BuildingFloor *currentFloor();
    QString currentLayerName() const;

    BuildingRenderer *renderer() const
    { return mRenderer; }

    QPoint sceneToTile(const QPointF &scenePos, int level) { return mRenderer->sceneToTile(scenePos, level); }
    QPointF sceneToTileF(const QPointF &scenePos, int level) { return mRenderer->sceneToTileF(scenePos, level); }
    QRect sceneToTileRect(const QRectF &sceneRect, int level) { return mRenderer->sceneToTileRect(sceneRect, level); }
    QRectF sceneToTileRectF(const QRectF &sceneRect, int level) { return mRenderer->sceneToTileRectF(sceneRect, level); }
    QPointF tileToScene(const QPoint &tilePos, int level) { return mRenderer->tileToScene(tilePos, level); }
    QPointF tileToSceneF(const QPointF &tilePos, int level) { return mRenderer->tileToSceneF(tilePos, level); }
    QPolygonF tileToScenePolygon(const QPoint &tilePos, int level) { return mRenderer->tileToScenePolygon(tilePos, level); }
    QPolygonF tileToScenePolygon(const QRect &tileRect, int level) { return mRenderer->tileToScenePolygon(tileRect, level); }
    QPolygonF tileToScenePolygonF(const QRectF &tileRect, int level) { return mRenderer->tileToScenePolygonF(tileRect, level); }
    QPolygonF tileToScenePolygon(const QPolygonF &tilePolygon, int level) { return mRenderer->tileToScenePolygon(tilePolygon, level); }

    void drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level)
    { return mRenderer->drawLine(painter, x1, y1, x2, y2, level); }

    void drawLine(QPainter *painter, const QPointF &p1, const QPointF &p2, int level)
    { return mRenderer->drawLine(painter, p1, p2, level); }

    void drawObject(QPainter *painter, BuildingObject *object);

    bool currentFloorContains(const QPoint &tilePos, int dw = 0, int dh = 0);

    GraphicsFloorItem *itemForFloor(BuildingFloor *floor);
    GraphicsObjectItem *itemForObject(BuildingObject *object);

    GraphicsObjectItem *createItemForObject(BuildingObject *object);

    BuildingObject *topmostObjectAt(const QPointF &scenePos);

    QSet<BuildingObject*> objectsInRect(const QRectF &tileRect);

    void setMouseOverObject(BuildingObject *object);
    BuildingObject *mouseOverObject() const
    { return mMouseOverObject; }

    virtual void setCursorObject(BuildingObject *object)
    { Q_UNUSED(object) }

    virtual void dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset)
    { Q_UNUSED(floor) Q_UNUSED(object) Q_UNUSED(offset) }
    virtual void resetDrag(BuildingFloor *floor, BuildingObject *object)
    { Q_UNUSED(floor) Q_UNUSED(object) }
    virtual void changeFloorGrid(BuildingFloor *floor,
                                 const QVector<QVector<Room*> > &grid)
    { Q_UNUSED(floor) Q_UNUSED(grid) }
    virtual void resetFloorGrid(BuildingFloor *floor)
    { Q_UNUSED(floor) }

    virtual void setEditingTiles(bool editing);
    bool editingTiles() const { return mEditingTiles; }

    virtual bool shouldShowFloorItem(BuildingFloor *floor) const;
    virtual bool shouldShowObjectItem(BuildingObject *object) const;

    void synchObjectItemVisibility();

    /////
    // Tile-editing-only methods
    virtual void setToolTiles(const FloorTileGrid *tiles,
                      const QPoint &pos, const QString &layerName) = 0;
    virtual void clearToolTiles() = 0;
    virtual QString buildingTileAt(int x, int y) = 0;
    virtual void drawTileSelection(QPainter *painter, const QRegion &region,
                                   const QColor &color, const QRectF &exposed,
                                   int level = 0) const = 0;
    /////

signals:
    void documentChanged();

protected slots:
    void buildingResized();
    void buildingRotated();

    void mapResized();

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);
    void objectChanged(BuildingObject *object);

    void selectedObjectsChanged();

protected:
    BuildingDocument *mDocument;
    BuildingRenderer *mRenderer;
    QList<GraphicsFloorItem*> mFloorItems;
    QSet<GraphicsObjectItem*> mSelectedObjectItems;
    BuildingObject *mMouseOverObject;
    bool mEditingTiles;
};

class FloorEditor : public BaseFloorEditor
{
    Q_OBJECT

public:

    explicit FloorEditor(QObject *parent = 0);

    bool eventFilter(QObject *watched, QEvent *event);

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
    { mousePressEvent(event); }

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    void setToolTiles(const FloorTileGrid *tiles, const QPoint &pos,
                      const QString &layerName);
    void clearToolTiles();
    QString buildingTileAt(int x, int y);
    void drawTileSelection(QPainter *painter, const QRegion &region,
                           const QColor &color, const QRectF &exposed,
                           int level = 0) const;

private slots:
    void currentToolChanged(BaseTool *tool);

    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);

    void roomChanged(Room *room);
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();

    void buildingResized();
    void buildingRotated();

private:
    GraphicsGridItem *mGridItem;
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

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void wheelEvent(QWheelEvent *event);

    void setHandScrolling(bool handScrolling);

signals:
    void mouseCoordinateChanged(const QPoint &tilePos);

private slots:
    void adjustScale(qreal scale);

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    QPoint mLastMouseTilePos;
    bool mHandScrolling;
};

} // namespace BuildingEditor

#endif // FLOOREDITOR_H
