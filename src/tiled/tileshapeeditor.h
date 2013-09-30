#ifndef TILESHAPEEDITOR_H
#define TILESHAPEEDITOR_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDialog>
#include <QSet>

namespace Ui {
class TileShapeEditor;
}

namespace Tiled {
namespace Internal {

class TileShapeScene;

class TileShape
{
public:
    TileShape() {}

    class Element
    {
    public:
        QPolygonF mUV;
        QPolygonF mGeom;
    };

    QList<Element> mElements;
};

class TileShapeGrid : public QGraphicsItem
{
public:
    TileShapeGrid(TileShapeScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

private:
    TileShapeScene *mScene;
};

class TileShapeItem : public QGraphicsItem
{
public:
    TileShapeItem(TileShapeScene *scene, TileShape *shape, QGraphicsItem *parent = 0);

    TileShape *tileShape() const { return mShape; }

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSelectedElement(int elementIndex);
    int selectedElement() const { return mSelectedElement; }

    void shapeChanged();

private:
    TileShapeScene *mScene;
    TileShape *mShape;
    QRectF mBoundingRect;
    int mSelectedElement;
};

class BaseTileShapeTool
{
public:
    BaseTileShapeTool(TileShapeScene *scene);

    virtual void activate() {}
    virtual void deactivate() {}

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    QAction *action() { return mAction; }

protected:
    TileShapeScene *mScene;
    QAction *mAction;
};

class CreateTileShapeElementTool : public BaseTileShapeTool
{
public:
    CreateTileShapeElementTool(TileShapeScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    TileShapeItem *mElementItem;
    QGraphicsLineItem *mCursorItemX, *mCursorItemY;
};

class TileShapeHandle : public QGraphicsItem
{
public:
    TileShapeHandle(TileShapeScene *scene, int elementIndex, int pointIndex);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSelected(bool selected);
    bool isSelected() const { return mSelected; }

    void setDragOrigin(const QPointF &pos) { mDragOrigin = pos; }
    void setDragOffset(const QPointF &delta);

private:
    TileShapeScene *mScene;
    int mElementIndex;
    int mPointIndex;
    bool mSelected;
    QPointF mDragOrigin;
};

class EditTileShapeElementTool : public BaseTileShapeTool
{
public:
    EditTileShapeElementTool(TileShapeScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void updateHandles();

    enum Mode {
        NoMode,
        Selecting,
        Moving
    };

    void startMoving();
    void updateMovingItems(const QPointF &scenePos);
    void finishMoving(const QPointF &scenePos);
    void startSelecting();
    void setSelectedHandles(const QSet<TileShapeHandle *> &handles);

    Mode mMode;
    QList<TileShapeHandle*> mHandles;
    QSet<TileShapeHandle*> mSelectedHandles;
    QPointF mStartScenePos;
    TileShapeHandle *mClickedHandle;
    QGraphicsLineItem *mCursorItemX, *mCursorItemY;
};

class TileShapeScene : public QGraphicsScene
{
    Q_OBJECT
public:
    TileShapeScene(QObject *parent = 0);

    void setTileShape(TileShape *shape);

    TileShapeItem *tileShapeItem() const { return mShapeItem; }
    TileShape *tileShape() const { return mShapeItem->tileShape(); }

    static QPointF toScene(qreal x, qreal y);
    static QPointF toScene(const QPointF &p);
    QPolygonF toScene(const QRectF &tileRect) const;
    QPolygonF toScene(const QPolygonF &tilePoly) const;

    static QPointF toTile(qreal x, qreal y);
    static QPointF toTile(const QPointF &scenePos);
    static QPolygonF toTile(const QPolygonF &scenePoly);

    QRectF boundingRect(const QRectF &rect) const;
    QRectF boundingRect(TileShape *shape) const;

    int topmostElementAt(const QPointF &scenePos, int *indexPtr);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void activateTool(BaseTileShapeTool *tool);

private:
    TileShapeGrid *mGridItem;
    TileShapeItem *mShapeItem;
    BaseTileShapeTool *mActiveTool;
};

class TileShapeView : public QGraphicsView
{
    Q_OBJECT
public:
    TileShapeView(QWidget *parent = 0);

    TileShapeScene *scene() { return mScene; }

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void wheelEvent(QWheelEvent *event);

    void setHandScrolling(bool handScrolling);

private:
    TileShapeScene *mScene;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    bool mHandScrolling;
};

class TileShapeEditor : public QDialog
{
    Q_OBJECT

public:
    explicit TileShapeEditor(TileShape *shape, QWidget *parent = 0);
    ~TileShapeEditor();

    TileShape *tileShape() const;

public slots:
    void toolActivated(bool active);

private:
    Ui::TileShapeEditor *ui;
    CreateTileShapeElementTool *mCreateElementTool;
    EditTileShapeElementTool *mEditElementTool;
};

} // namespace Internal
} // namespace Tiled

#endif // TILESHAPEEDITOR_H
