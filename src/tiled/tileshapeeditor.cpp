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
    mScene(scene),
    mZ(0)
{

}

QRectF TileShapeGrid::boundingRect() const
{
    return mScene->boundingRect(QRect(0,0,1,1), 0) | mScene->boundingRect(QRect(0,0,1,1), 3);
}

void TileShapeGrid::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setPen(Qt::lightGray);
    painter->drawRect(boundingRect());

    for (int y = 0; y < 32; y++)
        painter->drawLine(mScene->toScene(0, y / 32.0, mZ), mScene->toScene(1, y / 32.0, mZ));
    painter->drawLine(mScene->toScene(0, 1, mZ), mScene->toScene(1, 1, mZ));
    for (int x = 0; x < 32; x++)
        painter->drawLine(mScene->toScene(x / 32.0, 0, mZ), mScene->toScene(x / 32.0, 1, mZ));
    painter->drawLine(mScene->toScene(1, 0, mZ), mScene->toScene(1, 1, mZ));

    painter->setPen(Qt::black);

    QPolygonF poly = mScene->toScene(QRectF(0,0,1,1), 0);
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(0,0,1,1), 1);
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(0,0,1,1), 2);
    painter->drawPolygon(poly);
    poly = mScene->toScene(QRectF(0,0,1,1), 3);
    painter->drawPolygon(poly);
}

void TileShapeGrid::setGridZ(qreal z)
{
    if (z == mZ)
        return;
    mZ = z;
    update();
}

/////

TileShapeItem::TileShapeItem(TileShapeScene *scene, TileShape *shape, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mScene(scene),
    mShape(shape),
    mSelectedElement(-1),
    mHasCursorPoint(false)
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
        TileShape::Element e = mShape->mElements[mSelectedElement];
        QVector<QVector3D> tilePoly = e.mGeom;
        if (mHasCursorPoint)
            tilePoly += mCursorPoint;
        QPen pen(QColor(Qt::green).darker(), 0.5);
        painter->setPen(pen);
        painter->drawPolygon(mScene->toScene(tilePoly));
    }
}

void TileShapeItem::setSelectedElement(int elementIndex)
{
    mSelectedElement = elementIndex;
    update();
}

void TileShapeItem::setCursorPoint(const QVector3D &pt)
{
    if (pt != mCursorPoint) {
        mCursorPoint = pt;
        mHasCursorPoint = true;
        shapeChanged();
    }
}

void TileShapeItem::clearCursorPoint()
{
    if (mHasCursorPoint) {
        mHasCursorPoint = false;
        shapeChanged();
    }
}

void TileShapeItem::shapeChanged()
{
    prepareGeometryChange();
    mBoundingRect = mScene->boundingRect(mShape);
    if (mHasCursorPoint) {
        QPointF p = mScene->toScene(mCursorPoint);
        mBoundingRect.setLeft(qMin(mBoundingRect.left(), p.x()));
        mBoundingRect.setRight(qMax(mBoundingRect.right(), p.x()));
        mBoundingRect.setTop(qMin(mBoundingRect.top(), p.y()));
        mBoundingRect.setBottom(qMax(mBoundingRect.bottom(), p.y()));
    }
    update();
}

/////

TileShapeScene::TileShapeScene(QObject *parent) :
    QGraphicsScene(parent),
    mGridItem(new TileShapeGrid(this)),
    mActiveTool(0)
{
    setSceneRect(mGridItem->boundingRect().adjusted(-16, -16, 16, 16));
//    mGridItem->setPos(-mGridItem->boundingRect().topLeft());
    addItem(mGridItem);

    TileShape *shape = new TileShape(QLatin1String("SceneShape"));
    TileShape::Element e;
    e.mGeom << QVector3D(0, 0, 0) << QVector3D(0, 0, 3) << QVector3D(1, 0, 3) << QVector3D(1, 0, 0);
    shape->mElements += e;
    addItem(mShapeItem = new TileShapeItem(this, shape));
}

void TileShapeScene::setTileShape(TileShape *shape)
{
    mShapeItem->tileShape()->mElements = shape->mElements;
    mShapeItem->shapeChanged();
}

QPointF TileShapeScene::toScene(qreal x, qreal y, qreal z)
{
    const int tileWidth = 64, tileHeight = 32;
    const int originX = tileWidth / 2;
    const int originY = 0;
    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2 + originY + 96 - z * 32);
}

QPointF TileShapeScene::toScene(const QVector3D &v)
{
    return toScene(v.x(), v.y(), v.z());
}

QPolygonF TileShapeScene::toScene(const QRectF &tileRect, qreal z) const
{
    QPolygonF polygon;
    polygon << QPointF(toScene(tileRect.x(), tileRect.y(), z));
    polygon << QPointF(toScene(tileRect.right(), tileRect.y(), z));
    polygon << QPointF(toScene(tileRect.right(), tileRect.bottom(), z));
    polygon << QPointF(toScene(tileRect.x(), tileRect.bottom(), z));
    return polygon;
}

QPolygonF TileShapeScene::toScene(const QVector<QVector3D> &tilePoly) const
{
    QPolygonF scenePoly;
    foreach (QVector3D tilePos, tilePoly)
        scenePoly += toScene(tilePos);
    return scenePoly;
}

QVector3D TileShapeScene::toTile(qreal x, qreal y, qreal z)
{
    const int tileWidth = 64;
    const int tileHeight = 32;
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QVector3D(mx / tileHeight - 3,
                     my / tileHeight - 3,
                     z / 32.0);
}

QVector3D TileShapeScene::toTile(const QPointF &scenePos, qreal z)
{
    return toTile(scenePos.x(), scenePos.y(), z);
}

QVector<QVector3D> TileShapeScene::toTile(const QPolygonF &scenePoly, qreal z)
{
    QVector<QVector3D> tilePoly;
    foreach (QPointF scenePos, scenePoly)
        tilePoly += toTile(scenePos, z);
    return tilePoly;
}

QRectF TileShapeScene::boundingRect(const QRectF &rect, qreal z) const
{
    return toScene(rect, z).boundingRect();
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
    const qreal len2 = (v - w).lengthSquared();  // i.e. |w-v|^2 -  avoid a sqrt
    if (len2 == 0.0) return (p - v).length();   // v == w case
    // Consider the line extending the segment, parameterized as v + t (w - v).
    // We find projection of point p onto the line.
    // It falls where t = [(p-v) . (w-v)] / |w-v|^2
    const qreal t = QVector2D::dotProduct(p - v, w - v) / len2;
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
        QVector<QVector3D> tilePoly = e.mGeom;
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
    setTransform(QTransform::fromScale(7, 7));
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
    mUVTool = new TileShapeUVTool(ui->graphicsView->scene());

    ui->graphicsView->scene()->activateTool(mCreateElementTool);

    ui->toolBar->addAction(mCreateElementTool->action());
    ui->toolBar->addAction(mEditElementTool->action());
    ui->toolBar->addAction(mUVTool->action());

    connect(mCreateElementTool->action(), SIGNAL(toggled(bool)), SLOT(toolActivated(bool)));
    connect(mCreateElementTool, SIGNAL(statusTextChanged(QString)), ui->status, SLOT(setText(QString)));

    connect(mEditElementTool->action(), SIGNAL(toggled(bool)), SLOT(toolActivated(bool)));
    connect(mEditElementTool, SIGNAL(statusTextChanged(QString)), ui->status, SLOT(setText(QString)));

    connect(mUVTool->action(), SIGNAL(toggled(bool)), SLOT(toolActivated(bool)));
    connect(mUVTool, SIGNAL(statusTextChanged(QString)), ui->status, SLOT(setText(QString)));
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
    if (sender() == mUVTool->action())
        ui->graphicsView->scene()->activateTool(mUVTool);
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
    mCursorItemY(new QGraphicsLineItem),
    mMode(NoMode)
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

// Start in Mode==NoMode
// First click: set point's XY
// Move mouse up/down to change Z
// Second click: set point's Z

void CreateTileShapeElementTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        switch (mMode) {
        case NoMode: {
            qreal z = 0;
            QVector3D tilePos = mScene->toTile(event->scenePos(), z);
            tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
            tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
            TileShape *shape = new TileShape(QLatin1String("CreateTileShapeElementTool-Shape"));
            TileShape::Element e;
            e.mGeom += tilePos;
            e.mUV += QPointF();
            shape->mElements += e;
            mElementItem = new TileShapeItem(mScene, shape);
            mElementItem->setSelectedElement(0);
            mScene->addItem(mElementItem);
            mScene->gridItem()->setGridZ(z);
            mMode = SetZ;
            break;
        }
        case SetXY: {
            TileShape::Element &e = mElementItem->tileShape()->mElements.first();
#if 1
            e.mGeom += mElementItem->cursorPoint();
            e.mUV += QPointF();
#else
            qreal z = e.mGeom.last().z();
            QVector3D tilePos = mScene->toTile(event->scenePos(), z);
            tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
            tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
            e.mGeom += tilePos;
#endif
            mElementItem->shapeChanged();
            mMode = SetZ;
            // TODO: if have 3 points, constrain x/y to keep plane
            break;
        }
        case SetZ: {
            TileShape::Element &e = mElementItem->tileShape()->mElements.first();
            e.mGeom.last() = mElementItem->cursorPoint();
            mElementItem->shapeChanged();
            mMode = SetXY;
            break;
        }
        }
        mouseMoveEvent(event);
#if 0
        QVector3D tilePos = mScene->toTile(event->scenePos(), z);
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
#endif
    }
    if (event->button() == Qt::RightButton) {
        if (mElementItem) {
            TileShape::Element &e = mElementItem->tileShape()->mElements.first();
            if (e.mGeom.size() >= 3) {
                mScene->tileShape()->mElements += e;
                mScene->tileShapeItem()->shapeChanged();
            }
            mScene->removeItem(mElementItem);
            TileShape *shape = mElementItem->tileShape();
            delete mElementItem;
            delete shape;
            mElementItem = 0;
            mScene->gridItem()->setGridZ(0);
            mMode = NoMode;
        }
    }
}

void CreateTileShapeElementTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    qreal z = 0;
    QVector3D tilePos = mScene->toTile(event->scenePos(), z);
    QString msg;
    switch (mMode) {
    case NoMode: {
        tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
        tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
        msg = tr("Click to start a new point.");
        break;
    }
    case SetXY: {
        TileShape::Element &e = mElementItem->tileShape()->mElements.first();
        tilePos.setX(tilePos.x() + e.mGeom.last().z()); tilePos.setY(tilePos.y() + e.mGeom.last().z());
        tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
        tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
        tilePos.setZ(e.mGeom.last().z());
        z = tilePos.z();
        mElementItem->setCursorPoint(tilePos);
        mElementItem->shapeChanged();
//        mScene->gridItem()->setGridZ(tilePos.z());
        msg = tr("Click to set xy coordinate.");
        break;
    }
    case SetZ: {
        TileShape::Element &e = mElementItem->tileShape()->mElements.first();
        tilePos = e.mGeom.last();
        z = -(event->scenePos().y() - event->buttonDownScenePos(Qt::LeftButton).y());
        z /= 32.0; // FIXME: snap to 1/32 like xy
        z += e.mGeom.last().z();
//        z = qBound(0.0, z, 3.0);
        z = qFloor(z) + qFloor((z - qFloor(z)) * 32) / 32.0;
        tilePos.setZ(z);
        mElementItem->setCursorPoint(tilePos);
        mElementItem->shapeChanged();
        mScene->gridItem()->setGridZ(z);
        msg = tr("Click to set z coordinate.");
        break;
    }
    }

    mCursorItemX->setLine(QLineF(mScene->toScene(tilePos.x(), tilePos.y() - 0.5, z),
                          mScene->toScene(tilePos.x(), tilePos.y() + 0.5, z)));
    mCursorItemY->setLine(QLineF(mScene->toScene(tilePos.x() - 0.5, tilePos.y(), z),
                          mScene->toScene(tilePos.x() + 0.5, tilePos.y(), z)));

    emit statusTextChanged(QString::fromLatin1("%1,%2,%3: %4").arg(tilePos.x()).arg(tilePos.y()).arg(tilePos.z()).arg(msg));
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
//    setCursor(Qt::SizeAllCursor);

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

QVector3D TileShapeHandle::tilePos() const
{
     return mScene->tileShape()->mElements[mElementIndex].mGeom[mPointIndex];
}

void TileShapeHandle::setDragOffset(const QVector3D &delta)
{
    mScene->tileShape()->mElements[mElementIndex].mGeom[mPointIndex] = mDragOrigin + delta;
    mScene->tileShapeItem()->shapeChanged();
    setPos(mScene->toScene(mDragOrigin + delta));
}

void TileShapeHandle::setUV(const QPointF &uv)
{
    mScene->tileShape()->mElements[mElementIndex].mUV[mPointIndex] = uv;
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
        if (mMode == NoMode) {
            const QList<QGraphicsItem *> items = mScene->items(mStartScenePos);
            mClickedHandle = first<TileShapeHandle>(items);
            if (mClickedHandle) {
                if (!mSelectedHandles.contains(mClickedHandle) && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                    setSelectedHandles(QSet<TileShapeHandle*>() << mClickedHandle);
                QVector3D tilePos = mClickedHandle->tilePos();
                emit statusTextChanged(QString::fromLatin1("%1,%2,%3 = Selected handle x,y,z").arg(tilePos.x()).arg(tilePos.y()).arg(tilePos.z()));
            } else {
                int elementIndex = mScene->topmostElementAt(event->scenePos(), 0);
                if (mScene->tileShapeItem()->selectedElement() != elementIndex) {
                    mScene->tileShapeItem()->setSelectedElement(elementIndex);
                    updateHandles();
                }
                else if (elementIndex != -1)
                    setSelectedHandles(mHandles.toSet());
                else
                    setSelectedHandles(QSet<TileShapeHandle*>());
            }
        }
    }

    if (event->button() == Qt::RightButton) {
        if (mMode == MoveXY || mMode == MoveZ)
            cancelMoving();
    }
}

void EditTileShapeElementTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
#if 0
    if (mMode == NoMode && (event->buttons() & Qt::LeftButton)) {
        const int dragDistance = (event->buttonDownScreenPos(Qt::LeftButton) - event->screenPos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedHandle)
                startMoving();
            else
                startSelecting();
        }
    }
#endif

    if (mMode == MoveXY || mMode == MoveZ) {
        updateMovingItems(event->scenePos());
    }
}

void EditTileShapeElementTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode == NoMode) {
            if (mClickedHandle) {
                if (event->modifiers() & Qt::ControlModifier) {
                    if (mClickedHandle->isSelected())
                        setSelectedHandles(mSelectedHandles - (QSet<TileShapeHandle*> () << mClickedHandle));
                    else
                        setSelectedHandles(mSelectedHandles + (QSet<TileShapeHandle*> () << mClickedHandle));
                } else if (event->modifiers() & Qt::ShiftModifier)
                    setSelectedHandles(mSelectedHandles + (QSet<TileShapeHandle*> () << mClickedHandle));
                else {
                    startMoving();
                    mScene->gridItem()->setGridZ(mClickedHandle->tilePos().z());
//                    setSelectedHandles(QSet<TileShapeHandle*> () << mClickedHandle);
                    emit statusTextChanged(tr("Click to set XY."));
                }
            }
        } else if (mMode == MoveXY) {
            mMode = MoveZ;
            emit statusTextChanged(tr("Click to set Z."));
        } else if (mMode == MoveZ) {
            finishMoving(event->scenePos());
            emit statusTextChanged(QString());
        }
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
        handle->setDragOrigin(handle->tilePos());

    mMode = MoveXY;
}

void EditTileShapeElementTool::updateMovingItems(const QPointF &scenePos)
{
    QVector3D delta;

    if (mMode == MoveXY) {
        qreal z = 0;
        QVector3D tilePos = mScene->toTile(scenePos, z);
        tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
        tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
        QVector3D clampPos = tilePos; //mScene->toScene(tilePos, z);

        tilePos = mScene->toTile(mStartScenePos, z);
        tilePos.setX( qFloor(tilePos.x()) + qFloor((tilePos.x() - qFloor(tilePos.x())) * 32) / 32.0 );
        tilePos.setY( qFloor(tilePos.y()) + qFloor((tilePos.y() - qFloor(tilePos.y())) * 32) / 32.0 );
        QVector3D clampStartPos = tilePos;// mScene->toScene(tilePos, z);

        delta = clampPos - clampStartPos;
        mDragOffsetXY = delta.toPointF();
    }
    if (mMode == MoveZ) {
        qreal z = -(scenePos.y() - mStartScenePos.y());
        z /= 32.0;
//        z = qBound(0.0, z, 3.0);
        z += mClickedHandle->dragOrigin().z();
        z = qFloor(z) + qFloor((z - qFloor(z)) * 32) / 32.0;
        delta = QVector3D(mDragOffsetXY.x(), mDragOffsetXY.y(), z - mClickedHandle->dragOrigin().z());
        mScene->gridItem()->setGridZ(mClickedHandle->dragOrigin().z() + delta.z());
    }

    foreach (TileShapeHandle *handle, mSelectedHandles) {
        handle->setDragOffset(delta);
    }
}

void EditTileShapeElementTool::finishMoving(const QPointF &scenePos)
{
    mMode = NoMode;

    foreach (TileShapeHandle *handle, mSelectedHandles) {
        handle->setDragOrigin(handle->tilePos());
        handle->setDragOffset(QVector3D());
    }

    mScene->gridItem()->setGridZ(0);
}

void EditTileShapeElementTool::cancelMoving()
{
    mMode = NoMode;
    foreach (TileShapeHandle *handle, mSelectedHandles)
        handle->setDragOffset(QVector3D());

    mScene->gridItem()->setGridZ(0);
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

/////

TileShapeUVGuide::TileShapeUVGuide() :
    QGraphicsItem()
{
}

QRectF TileShapeUVGuide::boundingRect() const
{
    return QRectF(0, 0, 32, 96);
}

void TileShapeUVGuide::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    painter->setPen(Qt::gray);

    painter->fillRect(boundingRect(), Qt::lightGray);

    for (int x = 0; x < 32; x++)
        painter->drawLine(x, 0, x, 96);
    for (int y = 0; y < 96; y++)
        painter->drawLine(0, y, 32, y);

    painter->fillRect(mCurrentUV.x() * 32 - 0.25, mCurrentUV.y() * 96 - 0.25, 0.5, 0.5, Qt::gray);
    painter->fillRect(mCursorUV.x() * 32 - 0.25, mCursorUV.y() * 96 - 0.25, 0.5, 0.5, Qt::blue);
}

QPointF TileShapeUVGuide::toUV(const QPointF &scenePos)
{
    QPointF itemPos = mapFromScene(scenePos);
    return QPointF(itemPos.x() / boundingRect().width(),
                   itemPos.y() / boundingRect().height());
}

void TileShapeUVGuide::setCurrentUV(const QPointF &uv)
{
    if (mCurrentUV != uv) {
        mCurrentUV = uv;
        update();
    }
}

void TileShapeUVGuide::setCursorUV(const QPointF &uv)
{
    if (mCursorUV != uv) {
        mCursorUV = uv;
        update();
    }
}

/////

TileShapeUVTool::TileShapeUVTool(TileShapeScene *scene) :
    BaseTileShapeTool(scene),
    mGuide(new TileShapeUVGuide),
    mMode(NoMode),
    mClickedHandle(0)
{
    mAction->setText(QLatin1String("EditUV"));

    mGuide->setPos(16, 16);
    mGuide->setZValue(100);
}

void TileShapeUVTool::activate()
{
    mMode = NoMode;
    mGuide->hide();
    mScene->addItem(mGuide);
}

void TileShapeUVTool::deactivate()
{
    mScene->removeItem(mGuide);
}

void TileShapeUVTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode == NoMode) {
            const QList<QGraphicsItem *> items = mScene->items(event->scenePos());
            mClickedHandle = first<TileShapeHandle>(items);
            if (mClickedHandle) {
                setSelectedHandles(QSet<TileShapeHandle*>() << mClickedHandle);
                mGuide->show();
                mMode = SetUV;
            } else {
                int elementIndex = mScene->topmostElementAt(event->scenePos(), 0);
                if (mScene->tileShapeItem()->selectedElement() != elementIndex) {
                    mScene->tileShapeItem()->setSelectedElement(elementIndex);
                    updateHandles();
                } else
                    setSelectedHandles(QSet<TileShapeHandle*>());
            }
        } else if (mMode == SetUV) {
            QPointF uv = mGuide->toUV(event->scenePos());
            mClickedHandle->setUV(uv);
            mGuide->hide();
            mMode = NoMode;
        }
    }
}

void TileShapeUVTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMode == SetUV) {
        QPointF uv = mGuide->toUV(event->scenePos());
        mGuide->setCursorUV(uv);
    }
}

void TileShapeUVTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{

}

void TileShapeUVTool::updateHandles()
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

void TileShapeUVTool::setSelectedHandles(const QSet<TileShapeHandle *> &handles)
{
    foreach (TileShapeHandle *handle, mSelectedHandles)
        if (!handles.contains(handle))
            handle->setSelected(false);

    foreach (TileShapeHandle *handle, handles)
        if (!mSelectedHandles.contains(handle))
            handle->setSelected(true);

    mSelectedHandles = handles;
}
