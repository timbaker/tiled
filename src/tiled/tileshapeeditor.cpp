#include "tileshapeeditor.h"
#include "ui_tileshapeeditor.h"

#include <qmath.h>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QMouseEvent>
#include <QScrollBar>

using namespace Tiled;
using namespace Internal;

TileShapeGrid::TileShapeGrid(TileShapeScene *scene, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene)
{

}

QRectF TileShapeGrid::boundingRect() const
{
    return mScene->boundingRect(QRect(3,3,1,1)) | mScene->boundingRect(QRect(0,0,1,1));
}

void TileShapeGrid::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::lightGray);
    painter->drawRect(boundingRect());

    for (int y = 0; y < 32; y++) {
        painter->drawLine(mScene->toScene(0, y / 32.0), mScene->toScene(1, y / 32.0));
        painter->drawLine(mScene->toScene(0+1, y / 32.0 + 1), mScene->toScene(1+1, y / 32.0 + 1));
        painter->drawLine(mScene->toScene(0+2, y / 32.0 + 2), mScene->toScene(1+2, y / 32.0 + 2));
        painter->drawLine(mScene->toScene(0+3, y / 32.0 + 3), mScene->toScene(1+3, y / 32.0 + 3));
    }
    for (int x = 0; x < 32; x++) {
        painter->drawLine(mScene->toScene(x / 32.0, 0), mScene->toScene(x / 32.0, 1));
        painter->drawLine(mScene->toScene(x / 32.0 + 1, 0+1), mScene->toScene(x / 32.0 + 1, 1+1));
        painter->drawLine(mScene->toScene(x / 32.0 + 2, 0+2), mScene->toScene(x / 32.0 + 2, 1+2));
        painter->drawLine(mScene->toScene(x / 32.0 + 3, 0+3), mScene->toScene(x / 32.0 + 3, 1+3));
    }

    painter->setPen(Qt::black);

    QPolygonF poly = mScene->toScene(QRectF(3,3,1,1));
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(2,2,1,1));
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(1,1,1,1));
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(0,0,1,1));
    painter->drawPolygon(poly);
}

/////

TileShapeItem::TileShapeItem(TileShapeScene *scene, TileShape *shape, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mShape(shape),
    mSelectedElement(-1)
{
    mBoundingRect = mScene->boundingRect(mShape);
}

QRectF TileShapeItem::boundingRect() const
{
    return mBoundingRect;
}

void TileShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::lightGray);
    painter->drawRect(boundingRect());

#if 1
    QPen pen(/*(mSelectedElement >= 0) ? QColor(Qt::blue).lighter() :*/ Qt::blue, 0.5);
//    pen.setCosmetic(true);
    painter->setPen(pen);

#else
    QPen pen(Qt::black, 1.0);
    pen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(pen);
#endif

    int index = 0;
    foreach (TileShape::Element e, mShape->mElements) {
        if (index != mSelectedElement)
            painter->drawPolygon(mScene->toScene(e.mGeom));
        index++;
    }

    if (mSelectedElement >= 0 && mSelectedElement < mShape->mElements.size()) {
        QPen pen(QColor(Qt::green).darker(), 0.5);
        painter->setPen(pen);
        painter->drawPolygon(mScene->toScene(mShape->mElements[mSelectedElement].mGeom));
    }
}

void TileShapeItem::setSelectedElement(int elementIndex)
{
    mSelectedElement = elementIndex;
    update();
}

void TileShapeItem::shapeChanged()
{
    prepareGeometryChange();
    mBoundingRect = mScene->boundingRect(mShape);
    update();
}

/////

TileShapeScene::TileShapeScene(QObject *parent) :
    QGraphicsScene(parent),
    mGridItem(new TileShapeGrid(this)),
    mActiveTool(0)
{
//    setSceneRect(mGridItem->boundingRect());
    mGridItem->setPos(-mGridItem->boundingRect().topLeft());
    addItem(mGridItem);

    TileShape *shape = new TileShape;
    TileShape::Element e;
    e.mGeom << QPointF(0, 0) << QPointF(1, 0) << QPointF(3, 2) << QPointF(2, 2);
    shape->mElements += e;
    addItem(mShapeItem = new TileShapeItem(this, shape));
}

void TileShapeScene::setTileShape(TileShape *shape)
{
    mShapeItem->tileShape()->mElements = shape->mElements;
    mShapeItem->shapeChanged();
}

QPointF TileShapeScene::toScene(qreal x, qreal y)
{
    const int tileWidth = 64, tileHeight = 32;
    const int originX = tileWidth / 2;
    const int originY = 0;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY);
}

QPointF TileShapeScene::toScene(const QPointF &p)
{
    return toScene(p.x(), p.y());
}

QPolygonF TileShapeScene::toScene(const QRectF &tileRect) const
{
    QPolygonF polygon;
    polygon << QPointF(toScene(tileRect.topLeft()));
    polygon << QPointF(toScene(tileRect.topRight()));
    polygon << QPointF(toScene(tileRect.bottomRight()));
    polygon << QPointF(toScene(tileRect.bottomLeft()));
    return polygon;
}

QPolygonF TileShapeScene::toScene(const QPolygonF &tilePoly) const
{
    QPolygonF scenePoly;
    foreach (QPointF tilePos, tilePoly)
        scenePoly += toScene(tilePos);
    return scenePoly;
}

QPointF TileShapeScene::toTile(qreal x, qreal y)
{
    const int tileWidth = 64;
    const int tileHeight = 32;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QPointF TileShapeScene::toTile(const QPointF &scenePos)
{
    return toTile(scenePos.x(), scenePos.y());
}

QPolygonF TileShapeScene::toTile(const QPolygonF &scenePoly)
{
    QPolygonF tilePoly;
    foreach (QPointF scenePos, scenePoly)
        tilePoly += toTile(scenePos);
    return tilePoly;
}

QRectF TileShapeScene::boundingRect(const QRectF &rect) const
{
    return toScene(rect).boundingRect();
#if 0
    const int tileWidth = 64;
    const int tileHeight = 32;

    const int originX = tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

    return QRect(pos, size);
#endif
}

QRectF TileShapeScene::boundingRect(TileShape *shape) const
{
    QRectF r;
    foreach (TileShape::Element e, shape->mElements) {
        r |= toScene(e.mGeom).boundingRect();
    }
    return r;
}

#include <QVector2D>
static float minimum_distance(QVector2D v, QVector2D w, QVector2D p) {
    // Return minimum distance between line segment vw and point p
    const qreal l2 = (v - w).lengthSquared();  // i.e. |w-v|^2 -  avoid a sqrt
    if (l2 == 0.0) return (p - v).length();   // v == w case
    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line.
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2
    const qreal t = QVector2D::dotProduct(p - v, w - v) / l2;
    if (t < 0.0) return (p - v).length();       // Beyond the 'v' end of the segment
    else if (t > 1.0) return (p - w).length();  // Beyond the 'w' end of the segment
    const QVector2D projection = v + t * (w - v);  // Projection falls on the segment
    return (p - projection).length();
}

int TileShapeScene::topmostElementAt(const QPointF &scenePos, int *indexPtr)
{
    qreal radius = 2;

    // Do distance check in scene coords otherwise isometric testing is 1/2 height
    QVector2D p(scenePos);
    qreal min = 1000;
    int closest = -1;
    int elementIndex = 0;
    foreach (TileShape::Element e, tileShape()->mElements) {
        QPolygonF tilePoly = e.mGeom;
        tilePoly += e.mGeom.first();
        QPolygonF scenePoly = toScene(tilePoly);
        for (int i = 0; i < scenePoly.size() - 1; i++) {
            QVector2D v(scenePoly[i]);
            QVector2D w(scenePoly[i+1]);
            qreal dist = minimum_distance(v, w, p);
            if (dist <= radius && dist <= min) {
                min = dist;
                closest = elementIndex;
                if (indexPtr) *indexPtr = i;
            }
        }
        ++elementIndex;
    }
    return closest;
}

void TileShapeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mActiveTool) {
        mActiveTool->mousePressEvent(event);
        if (event->isAccepted())
            return;
    }

    QGraphicsScene::mousePressEvent(event);
}

void TileShapeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mActiveTool) {
        mActiveTool->mouseMoveEvent(event);
        if (event->isAccepted())
            return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void TileShapeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mActiveTool) {
        mActiveTool->mouseReleaseEvent(event);
        if (event->isAccepted())
            return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

void TileShapeScene::activateTool(BaseTileShapeTool *tool)
{
    if (mActiveTool) {
        mActiveTool->deactivate();
        mActiveTool->action()->setChecked(false);
    }

    mActiveTool = tool;

    if (mActiveTool) {
        mActiveTool->action()->setChecked(true);
        mActiveTool->activate();
    }
}

/////

TileShapeView::TileShapeView(QWidget *parent) :
    QGraphicsView(parent),
    mScene(new TileShapeScene(this)),
    mHandScrolling(false)
{
    setScene(mScene);
    setMouseTracking(true);
    setTransform(QTransform::fromScale(8, 8));
}

void TileShapeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void TileShapeView::mouseMoveEvent(QMouseEvent *event)
{
    if (mHandScrolling) {
        QScrollBar *hBar = horizontalScrollBar();
        QScrollBar *vBar = verticalScrollBar();
        const QPoint d = event->globalPos() - mLastMousePos;
        hBar->setValue(hBar->value() + (isRightToLeft() ? d.x() : -d.x()));
        vBar->setValue(vBar->value() - d.y());

        mLastMousePos = event->globalPos();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);

    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));
}

void TileShapeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(false);
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void TileShapeView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

//        mZoomable->handleWheelDelta(event->delta());

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMousePos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void TileShapeView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
    setInteractive(!mHandScrolling);

    if (mHandScrolling) {
        mLastMousePos = QCursor::pos();
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        viewport()->grabMouse();
    } else {
        viewport()->releaseMouse();
        QApplication::restoreOverrideCursor();
    }
}

/////

TileShapeEditor::TileShapeEditor(TileShape *shape, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileShapeEditor)
{
    ui->setupUi(this);

    if (shape)
        ui->graphicsView->scene()->setTileShape(shape);
    mCreateElementTool = new CreateTileShapeElementTool(ui->graphicsView->scene());
    mEditElementTool = new EditTileShapeElementTool(ui->graphicsView->scene());
    ui->graphicsView->scene()->activateTool(mCreateElementTool);

    ui->toolBar->addAction(mCreateElementTool->action());
    ui->toolBar->addAction(mEditElementTool->action());

    connect(mCreateElementTool->action(), SIGNAL(toggled(bool)), SLOT(toolActivated(bool)));
    connect(mEditElementTool->action(), SIGNAL(toggled(bool)), SLOT(toolActivated(bool)));
}

TileShapeEditor::~TileShapeEditor()
{
    delete ui;
}

TileShape *TileShapeEditor::tileShape() const
{
    return ui->graphicsView->scene()->tileShape();
}

void TileShapeEditor::toolActivated(bool active)
{
    if (!active) return;
    if (sender() == mCreateElementTool->action())
        ui->graphicsView->scene()->activateTool(mCreateElementTool);
    if (sender() == mEditElementTool->action())
        ui->graphicsView->scene()->activateTool(mEditElementTool);
}

/////

BaseTileShapeTool::BaseTileShapeTool(TileShapeScene *scene) :
    mScene(scene),
    mAction(new QAction(scene)) // FIXME: fix parent
{
    mAction->setCheckable(true);
}

/////

CreateTileShapeElementTool::CreateTileShapeElementTool(TileShapeScene *scene) :
    BaseTileShapeTool(scene),
    mElementItem(0),
    mCursorItemX(new QGraphicsLineItem),
    mCursorItemY(new QGraphicsLineItem)
{
    mAction->setText(QLatin1String("CreateFace"));

    QPen pen(Qt::blue);
    pen.setCosmetic(true);
    mCursorItemX->setPen(pen);
    mCursorItemY->setPen(pen);
}

void CreateTileShapeElementTool::activate()
{
    mScene->addItem(mCursorItemX);
    mScene->addItem(mCursorItemY);
}

void CreateTileShapeElementTool::deactivate()
{
    mScene->removeItem(mCursorItemX);
    mScene->removeItem(mCursorItemY);
}

void CreateTileShapeElementTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF tilePos = mScene->toTile(event->scenePos());
        tilePos.rx() = qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0;
        tilePos.ry() = qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0;
        if (mElementItem) {
            TileShape::Element &e = mElementItem->tileShape()->mElements.first();
            if (tilePos == e.mGeom.first()) {
                if (e.mGeom.size() >= 3) {
                    e.mGeom.remove(e.mGeom.size()-1);
                    mScene->tileShape()->mElements += e;
                    mScene->tileShapeItem()->shapeChanged();
                }
                mScene->removeItem(mElementItem);
                TileShape *shape = mElementItem->tileShape();
                delete mElementItem;
                delete shape;
                mElementItem = 0;
                return;
            }
            e.mGeom.last() = tilePos;
            e.mGeom += tilePos;
            mElementItem->shapeChanged();
        } else {
            TileShape *shape = new TileShape;
            TileShape::Element e;
            e.mGeom << tilePos << tilePos;
            shape->mElements += e;
            mElementItem = new TileShapeItem(mScene, shape);
            mElementItem->setSelectedElement(0);
            mScene->addItem(mElementItem);
        }
    }
    if (event->button() == Qt::RightButton) {
        if (mElementItem) {
            TileShape::Element &e = mElementItem->tileShape()->mElements.first();
            if (e.mGeom.size() >= 3) {
                e.mGeom.remove(e.mGeom.size()-1);
                mScene->tileShape()->mElements += e;
                mScene->tileShapeItem()->shapeChanged();
            }
            mScene->removeItem(mElementItem);
            TileShape *shape = mElementItem->tileShape();
            delete mElementItem;
            delete shape;
            mElementItem = 0;
        }
    }
}

void CreateTileShapeElementTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF tilePos = mScene->toTile(event->scenePos());
    tilePos.rx() = qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0;
    tilePos.ry() = qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0;
    if (mElementItem) {
        TileShape::Element &e = mElementItem->tileShape()->mElements.first();
        e.mGeom.last() = tilePos;
        mElementItem->shapeChanged();
    }
    mCursorItemX->setLine(QLineF(mScene->toScene(tilePos.x(), tilePos.y() - 0.5),
                          mScene->toScene(tilePos.x(), tilePos.y() + 0.5)));
    mCursorItemY->setLine(QLineF(mScene->toScene(tilePos.x() - 0.5, tilePos.y()),
                          mScene->toScene(tilePos.x() + 0.5, tilePos.y())));
}

void CreateTileShapeElementTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{

}

/////

TileShapeHandle::TileShapeHandle(TileShapeScene *scene, int elementIndex, int pointIndex) :
    QGraphicsItem(),
    mScene(scene),
    mElementIndex(elementIndex),
    mPointIndex(pointIndex),
    mSelected(false)
{
    setFlags(QGraphicsItem::ItemIgnoresTransformations |
             QGraphicsItem::ItemIgnoresParentOpacity);
    setZValue(10000);
    setCursor(Qt::SizeAllCursor);

    setPos(mScene->toScene(mScene->tileShape()->mElements[mElementIndex].mGeom[mPointIndex]));
}

QRectF TileShapeHandle::boundingRect() const
{
    return QRectF(-5, -5, 10 + 1, 10 + 1);
}

void TileShapeHandle::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::black);
    if (mSelected) {
        painter->setBrush(QApplication::palette().highlight());
        painter->drawRect(QRectF(-4, -4, 8, 8));
    } else {
        painter->setBrush(Qt::lightGray);
        painter->drawRect(QRectF(-3, -3, 6, 6));
    }
}

void TileShapeHandle::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void TileShapeHandle::setDragOffset(const QPointF &delta)
{
    mScene->tileShape()->mElements[mElementIndex].mGeom[mPointIndex] = mScene->toTile(mDragOrigin + delta);
    mScene->tileShapeItem()->shapeChanged();
    setPos(mDragOrigin + delta);
}

/////

template <class T>
static T *first(const QList<QGraphicsItem *> &items)
{
    foreach (QGraphicsItem *item, items) {
        if (T *t = dynamic_cast<T*>(item))
            return t;
    }
    return 0;
}

EditTileShapeElementTool::EditTileShapeElementTool(TileShapeScene *scene) :
    BaseTileShapeTool(scene),
    mMode(NoMode),
    mClickedHandle(0)
{
    mAction->setText(QLatin1String("EditFace"));
}

void EditTileShapeElementTool::activate()
{
}

void EditTileShapeElementTool::deactivate()
{
    qDeleteAll(mHandles);
    mHandles.clear();
    mSelectedHandles.clear();
    mClickedHandle = 0;

    mScene->tileShapeItem()->setSelectedElement(-1);
}

void EditTileShapeElementTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mStartScenePos = event->scenePos();
        const QList<QGraphicsItem *> items = mScene->items(mStartScenePos);
        mClickedHandle = first<TileShapeHandle>(items);
        if (!mClickedHandle) {
            int elementIndex = mScene->topmostElementAt(event->scenePos(), 0);
            if (mScene->tileShapeItem()->selectedElement() != elementIndex) {
                mScene->tileShapeItem()->setSelectedElement(elementIndex);
                updateHandles();
            }
            setSelectedHandles(QSet<TileShapeHandle*>());
        }
    }
}

void EditTileShapeElementTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && (event->buttons() & Qt::LeftButton)) {
        const int dragDistance = (event->buttonDownScreenPos(Qt::LeftButton) - event->screenPos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedHandle)
                startMoving();
            else
                startSelecting();
        }
    }

    if (mMode == Moving) {
        updateMovingItems(event->scenePos());
    }
}

void EditTileShapeElementTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == NoMode && event->button() == Qt::LeftButton) {
        if (mClickedHandle) {
            if (event->modifiers() & Qt::ControlModifier) {
                if (mClickedHandle->isSelected())
                    setSelectedHandles(mSelectedHandles - (QSet<TileShapeHandle*> () << mClickedHandle));
                else
                    setSelectedHandles(mSelectedHandles + (QSet<TileShapeHandle*> () << mClickedHandle));
            } else if (event->modifiers() & Qt::ShiftModifier)
                setSelectedHandles(mSelectedHandles + (QSet<TileShapeHandle*> () << mClickedHandle));
            else if (mClickedHandle)
                setSelectedHandles(QSet<TileShapeHandle*> () << mClickedHandle);
        }
    }
    if (mMode == Moving) {
        finishMoving(event->scenePos());
    }
}

void EditTileShapeElementTool::updateHandles()
{
    qDeleteAll(mHandles);
    mHandles.clear();
    mSelectedHandles.clear();
    mClickedHandle = 0;

    int elementIndex = mScene->tileShapeItem()->selectedElement();
    if (elementIndex != -1) {
        for (int j = 0; j < mScene->tileShape()->mElements[elementIndex].mGeom.size(); j++) {
            TileShapeHandle *handle = new TileShapeHandle(mScene, elementIndex, j);
            mHandles += handle;
            mScene->addItem(handle);
        }
    }
}

void EditTileShapeElementTool::startMoving()
{
    if (!mSelectedHandles.contains(mClickedHandle))
        setSelectedHandles(QSet<TileShapeHandle*>() << mClickedHandle);

    foreach (TileShapeHandle *handle, mSelectedHandles)
        handle->setDragOrigin(handle->pos());

    mMode = Moving;
}

void EditTileShapeElementTool::updateMovingItems(const QPointF &scenePos)
{
    QPointF tilePos = mScene->toTile(scenePos);
    tilePos.rx() = qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0;
    tilePos.ry() = qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0;
    QPointF clampPos = mScene->toScene(tilePos);

    tilePos = mScene->toTile(mStartScenePos);
    tilePos.rx() = qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0;
    tilePos.ry() = qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0;
    QPointF clampStartPos = mScene->toScene(tilePos);

    QPointF diff = clampPos - clampStartPos;

    foreach (TileShapeHandle *handle, mSelectedHandles) {
        handle->setDragOffset(diff);
    }
}

void EditTileShapeElementTool::finishMoving(const QPointF &scenePos)
{
    mMode = NoMode;

    foreach (TileShapeHandle *handle, mSelectedHandles) {
        handle->setDragOrigin(handle->pos());
        handle->setDragOffset(QPointF());
    }
}

void EditTileShapeElementTool::startSelecting()
{

}

void EditTileShapeElementTool::setSelectedHandles(const QSet<TileShapeHandle *> &handles)
{
    foreach (TileShapeHandle *handle, mSelectedHandles)
        if (!handles.contains(handle))
            handle->setSelected(false);

    foreach (TileShapeHandle *handle, handles)
        if (!mSelectedHandles.contains(handle))
            handle->setSelected(true);

    mSelectedHandles = handles;
}
