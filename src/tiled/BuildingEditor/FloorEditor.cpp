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

#include <QAction>
#include <QDebug>
#include <QPainter>

using namespace BuildingEditor;

GraphicsFloorItem::GraphicsFloorItem(BuildingFloor *floor) :
    QGraphicsItem(),
    mFloor(floor),
    mBmp(new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32))
{
    mBmp->fill(Qt::black);
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return QRectF(0, 0, mFloor->width() * 30, mFloor->height() * 30);
}

void GraphicsFloorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                              QWidget *)
{
    for (int x = 0; x < mFloor->width(); x++) {
        for (int y = 0; y < mFloor->height(); y++) {
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
}

QRectF GraphicsGridItem::boundingRect() const
{
    return QRectF(0, 0, mWidth * 30, mHeight * 30);
}

void GraphicsGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                             QWidget *)
{
    QPen pen(QColor(128, 128, 220, 80));
    pen.setWidth(2);
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);

    for (int x = 0; x <= mWidth; x++)
        painter->drawLine(x * 30, 0, x * 30, mHeight * 30);

    for (int y = 0; y <= mHeight; y++)
        painter->drawLine(0, y * 30, mWidth * 30, y * 30);
}

/////

GraphicsObjectItem::GraphicsObjectItem(FloorEditor *editor, BaseMapObject *object) :
    QGraphicsItem(),
    mEditor(editor),
    mObject(object),
    mDragging(false)
{
    synchWithObject();
}

QRectF GraphicsObjectItem::boundingRect() const
{
    return mBoundingRect;
}

void GraphicsObjectItem::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *,
                               QWidget *)
{
    QPoint dragOffset = mDragging ? mDragOffset : QPoint();

    // Screw you, polymorphism!!!
    if (Door *door = dynamic_cast<Door*>(mObject)) {
        if (door->dir() == Door::N) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            painter->fillRect(p.x(), p.y() - 5, 30, 10, Qt::white);
            QPen pen(Qt::blue);
            painter->setPen(pen);
            painter->drawRect(p.x(), p.y() - 5, 30, 10);
        }
        if (door->dir() == Door::W) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            painter->fillRect(p.x() - 5, p.y(), 10, 30, Qt::white);
            QPen pen(Qt::blue);
            painter->setPen(pen);
            painter->drawRect(p.x() - 5, p.y(), 10, 30);
        }
    }
}

void GraphicsObjectItem::setObject(BaseMapObject *object)
{
    mObject = object;
    synchWithObject();
    update();
}

void GraphicsObjectItem::synchWithObject()
{
    QRectF bounds = mEditor->tileToSceneRect(mObject->bounds()
                                             .translated(mDragging ? mDragOffset : QPoint()));
    bounds.adjust(-10, -10, 10, 10);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
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

void FloorEditor::UpdateMetaBuilding()
{
#if 0
    QString wallName = RoomDefinitionManager::instance->ExteriorWall;
    WallType *exteriorWall = WallTypes::instance->getEWallFromName(wallName);

    QList<Layout*> layouts;
    foreach (EditorFloor *fl, floors) {
        fl->UpdateLayout(exteriorWall);
        layouts += fl->layout;
    }

    building()->recreate(layouts, exteriorWall);

    if (Form1.instance.preview == null)
        return;

    Form1.instance.preview.panel1.building = building;
    Form1.instance.preview.panel1.Invalidate();
    Invalidate();
#endif
}

void FloorEditor::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    clear();

    mFloorItems.clear();
    foreach (BuildingFloor *floor, building()->floors())
        floorAdded(floor);

    GraphicsGridItem *item = new GraphicsGridItem(building()->width(),
                                                  building()->height());
    item->setZValue(20);
    addItem(item);

    setSceneRect(-10, -10,
                 building()->width() * 30 + 10,
                 building()->height() * 30 + 10);

    if (mDocument) {
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
    }
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
    QRect tileRect = sceneToTileRect(sceneRect);
    foreach (BaseMapObject *object, mDocument->currentFloor()->objects()) {
        if (object->bounds().intersects(tileRect))
            objects.insert(object);
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
    removeItem(item);
}

void FloorEditor::objectMoved(BaseMapObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
}

/////
