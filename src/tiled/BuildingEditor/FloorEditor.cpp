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

#include "FloorEditor.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingtools.h"
#include "buildingeditorwindow.h"

#include "zoomable.h"

#include <QAction>
#include <QDebug>
#include <QStyleOptionGraphicsItem>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

using namespace BuildingEditor;

using namespace Tiled;
using namespace Internal;

GraphicsFloorItem::GraphicsFloorItem(BuildingFloor *floor) :
    QGraphicsItem(),
    mFloor(floor),
    mBmp(new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32))
{
    setFlag(ItemUsesExtendedStyleOption);
    mBmp->fill(Qt::black);
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return QRectF(0, 0, mFloor->width() * 30, mFloor->height() * 30);
}

void GraphicsFloorItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
{
    int minX = option->exposedRect.left() / 30 - 1;
    int maxX = option->exposedRect.right() / 30 + 1;
    int minY = option->exposedRect.top() / 30 - 1;
    int maxY = option->exposedRect.bottom() / 30 + 1;

    minX = qMax(0, minX);
    maxX = qMin(maxX, mFloor->width());
    minY = qMax(0, minY);
    maxY = qMin(maxY, mFloor->height());

    for (int x = minX; x < maxX; x++) {
        for (int y = 0; y < maxY; y++) {
            QRgb c = mBmp->pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue;
            painter->fillRect(x * 30, y * 30, 30, 30, c);
        }
    }
}

/////

GraphicsGridItem::GraphicsGridItem(int width, int height) :
    QGraphicsItem(),
    mWidth(width),
    mHeight(height)
{
    setFlag(ItemUsesExtendedStyleOption);
}

QRectF GraphicsGridItem::boundingRect() const
{
    return QRectF(0, 0, mWidth * 30, mHeight * 30);
}

void GraphicsGridItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *option,
                             QWidget *)
{
    QPen pen(QColor(128, 128, 220, 80));
    pen.setWidth(2);
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);

    int minX = option->exposedRect.left() / 30 - 1;
    int maxX = option->exposedRect.right() / 30 + 1;
    int minY = option->exposedRect.top() / 30 - 1;
    int maxY = option->exposedRect.bottom() / 30 + 1;

    minX = qMax(0, minX);
    maxX = qMin(maxX, mWidth);
    minY = qMax(0, minY);
    maxY = qMin(maxY, mHeight);

    for (int x = minX; x <= maxX; x++)
        painter->drawLine(x * 30, minY * 30, x * 30, maxY * 30);

    for (int y = minY; y <= maxY; y++)
        painter->drawLine(minX * 30, y * 30, maxX * 30, y * 30);
}

/////

GraphicsObjectItem::GraphicsObjectItem(FloorEditor *editor, BaseMapObject *object) :
    QGraphicsItem(),
    mEditor(editor),
    mObject(object),
    mSelected(false),
    mDragging(false)
{
    synchWithObject();
}

QPainterPath GraphicsObjectItem::shape() const
{
    QPainterPath path;
    QPoint dragOffset = mDragging ? mDragOffset : QPoint();

    // Screw you, polymorphism!!!
    if (Door *door = dynamic_cast<Door*>(mObject)) {
        if (door->dir() == BaseMapObject::N) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            path.addRect(p.x(), p.y() - 5, 30, 10);
        }
        if (door->dir() == BaseMapObject::W) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            path.addRect(p.x() - 5, p.y(), 10, 30);
        }
    }

    if (Window *window = dynamic_cast<Window*>(mObject)) {
        if (window->dir() == BaseMapObject::N) {
            QPointF p = mEditor->tileToScene(window->pos() + dragOffset);
            path.addRect(p.x() + 7, p.y() - 3, 16, 6);
        }
        if (window->dir() == BaseMapObject::W) {
            QPointF p = mEditor->tileToScene(window->pos() + dragOffset);
            path.addRect(p.x() - 3, p.y() + 7, 6, 16);
        }
    }

    if (Stairs *stairs = dynamic_cast<Stairs*>(mObject)) {
        if (stairs->dir() == BaseMapObject::N) {
            QPointF p = mEditor->tileToScene(stairs->pos() + dragOffset);
            path.addRect(p.x(), p.y(), 30, 30 * 5);
        }
        if (stairs->dir() == BaseMapObject::W) {
            QPointF p = mEditor->tileToScene(stairs->pos() + dragOffset);
            path.addRect(p.x(), p.y(), 30 * 5, 30);
        }
    }

    return path;
}

QRectF GraphicsObjectItem::boundingRect() const
{
    return mBoundingRect;
}

void GraphicsObjectItem::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *,
                               QWidget *)
{
    QPainterPath path = shape();
    painter->fillPath(path, mSelected ? Qt::cyan : Qt::white);
    QPen pen(Qt::blue);
    painter->setPen(pen);
    painter->drawPath(path);
}

void GraphicsObjectItem::setObject(BaseMapObject *object)
{
    mObject = object;
    synchWithObject();
    update();
}

void GraphicsObjectItem::synchWithObject()
{
    QRectF bounds = shape().boundingRect();
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void GraphicsObjectItem::setSelected(bool selected)
{
    mSelected = selected;
    update();
}

void GraphicsObjectItem::setDragging(bool dragging)
{
    mDragging = dragging;
    synchWithObject();
}

void GraphicsObjectItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    synchWithObject();
}

/////

const int FloorEditor::ZVALUE_GRID = 20;
const int FloorEditor::ZVALUE_CURSOR = 100;

FloorEditor::FloorEditor(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mCurrentTool(0)
{
    setBackgroundBrush(Qt::black);

}

void FloorEditor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void FloorEditor::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void FloorEditor::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void FloorEditor::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    clear();

    mFloorItems.clear();
    mObjectItems.clear();
    mSelectedObjectItems.clear();

    if (mDocument) {
        foreach (BuildingFloor *floor, building()->floors())
            floorAdded(floor);

        GraphicsGridItem *item = new GraphicsGridItem(building()->width(),
                                                      building()->height());
        item->setZValue(ZVALUE_GRID);
        addItem(item);

        setSceneRect(-10, -10,
                     building()->width() * 30 + 10,
                     building()->height() * 30 + 10);

        connect(mDocument, SIGNAL(currentFloorChanged()),
                SLOT(currentFloorChanged()));
        connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
                SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));
        connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
                SLOT(floorAdded(BuildingFloor*)));
        connect(mDocument, SIGNAL(objectAdded(BaseMapObject*)),
                SLOT(objectAdded(BaseMapObject*)));
        connect(mDocument, SIGNAL(objectAboutToBeRemoved(BaseMapObject*)),
                SLOT(objectAboutToBeRemoved(BaseMapObject*)));
        connect(mDocument, SIGNAL(objectMoved(BaseMapObject*)),
                SLOT(objectMoved(BaseMapObject*)));
        connect(mDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(selectedObjectsChanged()));
    }

    emit documentChanged();
}

void FloorEditor::clearDocument()
{
    setDocument(0);
}

Building *FloorEditor::building() const
{
    return mDocument ? mDocument->building() : 0;
}

void FloorEditor::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
    }

    mCurrentTool = tool;

    if (mCurrentTool)
        mCurrentTool->action()->setChecked(true);
}

QPoint FloorEditor::sceneToTile(const QPointF &scenePos)
{
    // FIXME: x/y < 0 rounds up to zero
    return QPoint(scenePos.x() / 30, scenePos.y() / 30);
}

QPointF FloorEditor::sceneToTileF(const QPointF &scenePos)
{
    return scenePos / 30;
}

QRect FloorEditor::sceneToTileRect(const QRectF &sceneRect)
{
    QPoint topLeft = sceneToTile(sceneRect.topLeft());
    QPoint botRight = sceneToTile(sceneRect.bottomRight());
    return QRect(topLeft, botRight);
}

QPointF FloorEditor::tileToScene(const QPoint &tilePos)
{
    return tilePos * 30;
}

QRectF FloorEditor::tileToSceneRect(const QPoint &tilePos)
{
    return QRectF(tilePos.x() * 30, tilePos.y() * 30, 30, 30);
}

QRectF FloorEditor::tileToSceneRect(const QRect &tileRect)
{
    return QRectF(tileRect.x() * 30, tileRect.y() * 30,
                  tileRect.width() * 30, tileRect.height() * 30);
}

bool FloorEditor::currentFloorContains(const QPoint &tilePos)
{
    int x = tilePos.x(), y = tilePos.y();
    if (x < 0 || y < 0
            || x >= mDocument->currentFloor()->width()
            || y >= mDocument->currentFloor()->height())
        return false;
    return true;
}

GraphicsObjectItem *FloorEditor::itemForObject(BaseMapObject *object)
{
    foreach (GraphicsObjectItem *item, mObjectItems) {
        if (item->object() == object)
            return item;
    }
    return 0;
}

QSet<BaseMapObject*> FloorEditor::objectsInRect(const QRectF &sceneRect)
{
    QSet<BaseMapObject*> objects;
    foreach (QGraphicsItem *item, items(sceneRect)) {
        if (GraphicsObjectItem *objectItem = dynamic_cast<GraphicsObjectItem*>(item)) {
            if (objectItem->object()->floor() == mDocument->currentFloor())
                objects += objectItem->object();
        }
    }
    return objects;
}

void FloorEditor::currentFloorChanged()
{
    int level = mDocument->currentFloor()->level();
    for (int i = 0; i <= level; i++) {
        mFloorItems[i]->setOpacity((i == level) ? 1.0 : 0.15);
        mFloorItems[i]->setVisible(true);
    }
    for (int i = level + 1; i < building()->floorCount(); i++)
        mFloorItems[i]->setVisible(false);
}

void FloorEditor::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    int index = floor->building()->floors().indexOf(floor);
    Room *room = floor->layout()->roomAt(pos);
//    qDebug() << floor << pos << room;
    mFloorItems[index]->bmp()->setPixel(pos, room ? room->Color : qRgb(0, 0, 0));
    mFloorItems[index]->update();
}

void FloorEditor::floorAdded(BuildingFloor *floor)
{
    GraphicsFloorItem *item = new GraphicsFloorItem(floor);
    mFloorItems.insert(floor->level(), item);
    addItem(item);
}

void FloorEditor::objectAdded(BaseMapObject *object)
{
    Q_ASSERT(!itemForObject(object));
    GraphicsObjectItem *item = new GraphicsObjectItem(this, object);
    item->setParentItem(mFloorItems[object->floor()->level()]);
    mObjectItems.insert(object->index(), item);
    addItem(item);

    for (int i = object->index(); i < mObjectItems.count(); i++)
        mObjectItems[i]->setZValue(i);
}

void FloorEditor::objectAboutToBeRemoved(BaseMapObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    mObjectItems.removeAll(item);
    mSelectedObjectItems.remove(item); // paranoia
    removeItem(item);
}

void FloorEditor::objectMoved(BaseMapObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
}

void FloorEditor::selectedObjectsChanged()
{
    QSet<BaseMapObject*> selectedObjects = mDocument->selectedObjects();
    QSet<GraphicsObjectItem*> selectedItems;

    foreach (BaseMapObject *object, selectedObjects)
        selectedItems += itemForObject(object);

    foreach (GraphicsObjectItem *item, mSelectedObjectItems - selectedItems)
        item->setSelected(false);
    foreach (GraphicsObjectItem *item, selectedItems - mSelectedObjectItems)
        item->setSelected(true);

    mSelectedObjectItems = selectedItems;
}

/////

FloorView::FloorView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this))
{
    // Alignment of the scene within the view
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

void FloorView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));

    QPoint tilePos = scene()->sceneToTile(mLastMouseScenePos);
    if (tilePos != mLastMouseTilePos) {
        mLastMouseTilePos = tilePos;
        emit mouseCoordinateChanged(mLastMouseTilePos);
    }
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void FloorView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(event->delta());

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

void FloorView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}

/////
