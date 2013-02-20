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

#include "buildingorthoview.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingtools.h"
#include "buildingtemplates.h"
#include "furnituregroups.h"

#include "zoomable.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QStyleOptionGraphicsItem>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <qmath.h>

using namespace BuildingEditor;

using namespace Tiled;
using namespace Internal;

/////

void BuildingRenderer::drawObject(QPainter *painter, BuildingObject *mObject,
                                  const QPoint &dragOffset, bool mValidPos,
                                  bool mSelected, bool mMouseOver, int level)
{
    QPainterPath path;
    {
        QPolygonF tilePolygon = mObject->calcShape().translated(dragOffset);
        QPolygonF scenePolygon = tileToScenePolygon(tilePolygon, level);
        path.addPolygon(scenePolygon); // FIXME: cache this, pass as arg?
    }
    QColor color = mMouseOver ? Qt::white : QColor(225, 225, 225);
    painter->fillPath(path, color);
    QPen pen(mValidPos ? (mSelected ? Qt::cyan : Qt::blue) : Qt::red);

    if (Stairs *stairs = mObject->asStairs()) {
        QRectF r = stairs->bounds().translated(dragOffset); //path.boundingRect();
        if (stairs->isW()) {
            for (int x = 30; x <= 120; x += 10)
                drawLine(painter, r.left()+x/30.0, r.top(), r.left()+x/30.0,r.bottom(),
                                  level);
        } else {
            for (int y = 30; y <= 120; y += 10)
                drawLine(painter, r.left(), r.top()+y/30.0, r.right(),r.top()+y/30.0,
                                  level);

        }
    }

    // Draw line(s) indicating the orientation of the furniture tile.
    if (FurnitureObject *object = mObject->asFurniture()) {
        QRectF r = object->bounds().translated(dragOffset);
        r.adjust(2/30.0, 2/30.0, -2/30.0, -2/30.0);

        bool lineW = false, lineN = false, lineE = false, lineS = false;
        switch (object->furnitureTile()->orient()) {
        case FurnitureTile::FurnitureN:
            lineN = true;
            break;
        case FurnitureTile::FurnitureS:
            lineS = true;
            break;
        case FurnitureTile::FurnitureNW:
            lineN = true;
            // fall through
        case FurnitureTile::FurnitureW:
            lineW = true;
            break;
        case FurnitureTile::FurnitureNE:
            lineN = true;
            // fall through
        case FurnitureTile::FurnitureE:
            lineE = true;
            break;
        case FurnitureTile::FurnitureSE:
            lineS = true;
            lineE = true;
            break;
        case FurnitureTile::FurnitureSW:
            lineS = true;
            lineW = true;
            break;
        }

        FurnitureTiles::FurnitureLayer layer = object->furnitureTile()->owner()->layer();
        if (layer == FurnitureTiles::LayerFrames ||
                layer == FurnitureTiles::LayerDoors ||
                layer == FurnitureTiles::LayerWalls ||
                layer == FurnitureTiles::LayerRoofCap ||
                layer == FurnitureTiles::LayerRoof)
            lineW = lineN = lineE = lineS = false;

        QPainterPath path2;
        if (lineW)
            path2.addPolygon(tileToScenePolygonF(QRectF(r.left() + 2/30.0, r.top() + 2/30.0, 2/30.0, r.height() - 4/30.0),
                                                          level));
        if (lineE)
            path2.addPolygon(tileToScenePolygonF(QRectF(r.right() - 4/30.0, r.top() + 2/30.0, 2/30.0, r.height() - 4/30.0),
                                                          level));
        if (lineN)
            path2.addPolygon(tileToScenePolygonF(QRectF(r.left() + 2/30.0, r.top() + 2/30.0, r.width() - 4/30.0, 2/30.0),
                                                          level));
        if (lineS)
            path2.addPolygon(tileToScenePolygonF(QRectF(r.left() + 2/30.0, r.bottom() - 4/30.0, r.width() - 4/30.0, 2/30.0),
                                                          level));
        painter->fillPath(path2, pen.color());
    }

    if (RoofObject *roof = mObject->asRoof()) {
        QColor colorDark(Qt::darkGray);
        QColor colorLight(Qt::lightGray);
        QColor colorMid(Qt::gray);

        if (mMouseOver) {
            colorDark = colorDark.lighter(112);
            colorLight = colorLight.lighter(112);
            colorMid = colorMid.lighter(112);
        }

        QRectF ne = roof->northEdge().translated(dragOffset);
        QRectF se = roof->southEdge().translated(dragOffset);
        if ((roof->roofType() == RoofObject::PeakWE) && roof->isHalfDepth()) {
            ne.adjust(0,0,0,-0.5);
            se.adjust(0,0.5,0,0);
        }
        QPainterPath path2;
        path2.addPolygon(tileToScenePolygonF(ne, level));
        painter->fillPath(path2, colorDark);

        path2 = QPainterPath();
        path2.addPolygon(tileToScenePolygonF(se, level));
        painter->fillPath(path2, colorLight);

        QRectF we = roof->westEdge().translated(dragOffset);
        QRectF ee = roof->eastEdge().translated(dragOffset);
        if ((roof->roofType() == RoofObject::PeakNS) && roof->isHalfDepth()) {
            we.adjust(0,0,-0.5,0);
            ee.adjust(0.5,0,0,0);
        }

        path2 = QPainterPath();
        path2.addPolygon(tileToScenePolygonF(we, level));
        painter->fillPath(path2, colorDark);

        path2 = QPainterPath();
        path2.addPolygon(tileToScenePolygonF(ee, level));
        painter->fillPath(path2, colorLight);

        path2 = QPainterPath();
        path2.addPolygon(tileToScenePolygonF(roof->flatTop().translated(dragOffset),
                                                      level));
        painter->fillPath(path2, colorMid);
    }

    painter->setPen(pen);
    painter->drawPath(path);

    if (!mValidPos) {
        painter->fillPath(path, QColor(255, 0, 0, 128));
    }

    if (mSelected) {
        painter->setOpacity(0.5);
        painter->fillPath(path, QApplication::palette().highlight().color());
    }
}

/////

BuildingBaseScene::BuildingBaseScene(QObject *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMouseOverObject(0),
    mEditingTiles(false)
{
}

Building *BuildingBaseScene::building() const
{
    return mDocument ? mDocument->building() : 0;
}

int BuildingBaseScene::currentLevel() const
{
    return mDocument ? mDocument->currentLevel() : -1;
}

BuildingFloor *BuildingBaseScene::currentFloor()
{
    return mDocument ? mDocument->currentFloor() : 0;
}

QString BuildingBaseScene::currentLayerName() const
{
    return mDocument ? mDocument->currentLayer() : QString();
}

bool BuildingBaseScene::currentFloorContains(const QPoint &tilePos, int dw, int dh)
{
    return currentFloor()->bounds(dw, dh).contains(tilePos);
}

GraphicsFloorItem *BuildingBaseScene::itemForFloor(BuildingFloor *floor)
{
    return mFloorItems[floor->level()];
}

GraphicsObjectItem *BuildingBaseScene::itemForObject(BuildingObject *object)
{
    return itemForFloor(object->floor())->itemForObject(object);
}

void BuildingBaseScene::buildingResized()
{
    foreach (GraphicsFloorItem *item, mFloorItems) {
        item->synchWithFloor();
        item->floorEdited();
    }
}

void BuildingBaseScene::buildingRotated()
{
    foreach (GraphicsFloorItem *item, mFloorItems) {
        item->synchWithFloor();
        item->floorEdited();
    }
}

void BuildingBaseScene::mapResized()
{
    foreach (GraphicsFloorItem *item, mFloorItems)
        item->mapResized();

}

void BuildingBaseScene::floorAdded(BuildingFloor *floor)
{
    GraphicsFloorItem *item = new GraphicsFloorItem(this, floor);
    mFloorItems.insert(floor->level(), item);
    addItem(item);

    foreach (GraphicsFloorItem *item, mFloorItems)
        item->setZValue(floor->building()->floorCount() + item->floor()->level());

    floorEdited(floor);

    foreach (BuildingObject *object, floor->objects())
        objectAdded(object);
}

void BuildingBaseScene::floorRemoved(BuildingFloor *floor)
{
    Q_ASSERT(mFloorItems[floor->level()]
             && mFloorItems[floor->level()]->floor() == floor);
    foreach (GraphicsObjectItem *item, mFloorItems[floor->level()]->objectItems())
        mSelectedObjectItems.remove(item);
    delete mFloorItems.takeAt(floor->level());

    foreach (GraphicsFloorItem *item, mFloorItems)
        item->setZValue(floor->building()->floorCount() + item->floor()->level());
}

void BuildingBaseScene::floorEdited(BuildingFloor *floor)
{
    itemForFloor(floor)->floorEdited();
}

void BuildingBaseScene::objectAdded(BuildingObject *object)
{
    Q_ASSERT(!itemForObject(object));
    GraphicsObjectItem *item = createItemForObject(object);
    item->synchWithObject();
    itemForFloor(object->floor())->objectAdded(item);
}

void BuildingBaseScene::objectAboutToBeRemoved(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    mSelectedObjectItems.remove(item); // paranoia
    itemForFloor(object->floor())->objectAboutToBeRemoved(item);
    removeItem(item);
    delete item;

    if (object == mMouseOverObject)
        mMouseOverObject = 0;
}

void BuildingBaseScene::objectMoved(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
}

void BuildingBaseScene::objectTileChanged(BuildingObject *object)
{
    // FurnitureObject might change size/orientation so redisplay
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
    item->update();
}

// This is for roofs being edited via handles
void BuildingBaseScene::objectChanged(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->update();
}

void BuildingBaseScene::selectedObjectsChanged()
{
    QSet<BuildingObject*> selectedObjects = mDocument->selectedObjects();
    QSet<GraphicsObjectItem*> selectedItems;

    foreach (BuildingObject *object, selectedObjects)
        selectedItems += itemForObject(object);

    foreach (GraphicsObjectItem *item, mSelectedObjectItems - selectedItems)
        item->setSelected(false);
    foreach (GraphicsObjectItem *item, selectedItems - mSelectedObjectItems)
        item->setSelected(true);

    mSelectedObjectItems = selectedItems;
}

GraphicsObjectItem *BuildingBaseScene::createItemForObject(BuildingObject *object)
{
    GraphicsObjectItem *item;
    if (RoofObject *roof = object->asRoof())
        item = new GraphicsRoofItem(this, roof);
    else if (WallObject *wall = object->asWall())
        item = new GraphicsWallItem(this, wall);
    else
        item = new GraphicsObjectItem(this, object);
    return item;
}

BuildingObject *BuildingBaseScene::topmostObjectAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, items(scenePos)) {
        if (GraphicsObjectItem *objectItem = dynamic_cast<GraphicsObjectItem*>(item)) {
            if (objectItem->object()->floor() == mDocument->currentFloor())
                return objectItem->object();
        }
    }
    return 0;
}

QSet<BuildingObject*> BuildingBaseScene::objectsInRect(const QRectF &tileRect)
{
    QSet<BuildingObject*> objects;
    QPolygonF polygon = tileToScenePolygonF(tileRect, currentLevel());
    foreach (QGraphicsItem *item, items(polygon)) {
        if (GraphicsObjectItem *objectItem = dynamic_cast<GraphicsObjectItem*>(item)) {
            if (objectItem->object()->floor() == mDocument->currentFloor())
                objects += objectItem->object();
        }
    }
    return objects;
}

void BuildingBaseScene::setMouseOverObject(BuildingObject *object)
{
    if (object != mMouseOverObject) {
        if (mMouseOverObject)
            itemForObject(mMouseOverObject)->setMouseOver(false);
        mMouseOverObject = object;
        if (mMouseOverObject)
            itemForObject(mMouseOverObject)->setMouseOver(true);
    }
}

void BuildingBaseScene::setEditingTiles(bool editing)
{
    mEditingTiles = editing;
}

bool BuildingBaseScene::shouldShowFloorItem(BuildingFloor *floor) const
{
    return floor->level() <= currentLevel();
}

bool BuildingBaseScene::shouldShowObjectItem(BuildingObject *object) const
{
    // Cursor items are always visible.
    if (!object->floor())
        return true;

    return BuildingPreferences::instance()->showObjects() &&
            (currentLevel() <= object->floor()->level());
}

void BuildingBaseScene::synchObjectItemVisibility()
{
    foreach (GraphicsFloorItem *item, mFloorItems)
        item->synchVisibility();
}

void BuildingOrthoScene::setToolTiles(const FloorTileGrid *tiles, const QPoint &pos,
                                   const QString &layerName)
{
    Q_UNUSED(tiles)
    Q_UNUSED(pos)
    Q_UNUSED(layerName)
}

void BuildingOrthoScene::clearToolTiles()
{
}

QString BuildingOrthoScene::buildingTileAt(int x, int y)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    return QString();
}

void BuildingOrthoScene::drawTileSelection(QPainter *painter, const QRegion &region,
                                    const QColor &color, const QRectF &exposed,
                                    int level) const
{
    Q_UNUSED(painter)
    Q_UNUSED(region)
    Q_UNUSED(color)
    Q_UNUSED(exposed)
    Q_UNUSED(level)
}

/////

GraphicsFloorItem::GraphicsFloorItem(BuildingBaseScene *editor, BuildingFloor *floor) :
    QGraphicsItem(),
    mEditor(editor),
    mFloor(floor),
    mBmp(new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32)),
    mDragBmp(0)
{
    setFlag(ItemUsesExtendedStyleOption);
    if (mEditor->renderer()->asIso())
        setFlag(ItemDoesntPropagateOpacityToChildren);
    mBmp->fill(Qt::black);

    if (mEditor->renderer()->asIso())
        setOpacity(0.05);
    else
        setOpacity(0.25);
}

GraphicsFloorItem::~GraphicsFloorItem()
{
    delete mBmp;
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return mEditor->tileToScenePolygon(mFloor->bounds(), mFloor->level()).boundingRect();
}

void GraphicsFloorItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
{
    painter->setPen(Qt::NoPen);

    QImage *bmp = mDragBmp ? mDragBmp : mBmp;
    for (int x = 0; x < mFloor->width(); x++) {
        for (int y = 0; y < mFloor->height(); y++) {
            QRgb c = bmp->pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue;
            painter->setBrush(QColor(c));
            QPolygonF polygon = mEditor->tileToScenePolygon(QPoint(x, y), mFloor->level());
            if (option->exposedRect.intersects(polygon.boundingRect()))
                painter->drawConvexPolygon(polygon);
        }
    }
}

void GraphicsFloorItem::objectAdded(GraphicsObjectItem *item)
{
    BuildingObject *object = item->object();
    Q_ASSERT(!itemForObject(object));
    item->setParentItem(this);
    mObjectItems.insert(object->index(), item);

    for (int i = object->index(); i < mObjectItems.count(); i++)
        mObjectItems[i]->setZValue(i);
}

void GraphicsFloorItem::objectAboutToBeRemoved(GraphicsObjectItem *item)
{
    BuildingObject *object = item->object();
    mObjectItems.removeAll(item);

    for (int i = object->index(); i < mObjectItems.count(); i++)
        mObjectItems[i]->setZValue(i);
}

GraphicsObjectItem *GraphicsFloorItem::itemForObject(BuildingObject *object) const
{
    foreach (GraphicsObjectItem *item, mObjectItems) {
        if (item->object() == object)
            return item;
    }
    return 0;
}

void GraphicsFloorItem::synchWithFloor()
{
    delete mBmp;
    mBmp = new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32);

    foreach (GraphicsObjectItem *item, mObjectItems)
        item->synchWithObject();
}

void GraphicsFloorItem::mapResized()
{
    // When a building is resized, GraphicsObjectItems are updated *before* the
    // BuildingMap::mMap has been resized to match the building size.  When the
    // map is finally resized, we must update the GraphicsObjectItems again.
    // This also needs doing when the max level changes.
    foreach (GraphicsObjectItem *item, mObjectItems)
        item->synchWithObject();
}

void GraphicsFloorItem::floorEdited()
{
    mBmp->fill(Qt::black);
    for (int x = 0; x < mFloor->width(); x++) {
        for (int y = 0; y < mFloor->height(); y++) {
            if (Room *room = mFloor->GetRoomAt(x, y))
                mBmp->setPixel(x, y, room->Color);
        }
    }
    update();
}

void GraphicsFloorItem::roomChanged(Room *room)
{
    for (int x = 0; x < mFloor->width(); x++) {
        for (int y = 0; y < mFloor->height(); y++) {
            if (mFloor->GetRoomAt(x, y) == room)
                mBmp->setPixel(x, y, room->Color);
        }
    }
    update(); // FIXME: only affected area
}

void GraphicsFloorItem::roomAtPositionChanged(const QPoint &pos)
{
    Room *room = mFloor->GetRoomAt(pos);
    mBmp->setPixel(pos, room ? room->Color : qRgb(0, 0, 0));
    update(); // FIXME: only affected area
}

void GraphicsFloorItem::setDragBmp(QImage *bmp)
{
    mDragBmp = bmp;
    update();
}

void GraphicsFloorItem::synchVisibility()
{
    setVisible(mEditor->shouldShowFloorItem(mFloor));
    foreach (GraphicsObjectItem *item, mObjectItems)
        item->setVisible(mEditor->shouldShowObjectItem(item->object()));
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
    return QRectF(-2, -2, mWidth * 30 + 4, mHeight * 30 + 4);
}

void GraphicsGridItem::paint(QPainter *painter,
                             const QStyleOptionGraphicsItem *option,
                             QWidget *)
{
    QPen pen(QColor(128, 128, 220, 80));
    painter->setPen(pen);
    QBrush brush(QColor(128, 128, 220, 80), Qt::Dense4Pattern);
    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
                                             1/painter->transform().m22()));
    pen.setBrush(brush);

    int minX = qFloor(option->exposedRect.left() / 30) - 1;
    int maxX = qCeil(option->exposedRect.right() / 30) + 1;
    int minY = qFloor(option->exposedRect.top() / 30) - 1;
    int maxY = qCeil(option->exposedRect.bottom() / 30) + 1;

    minX = qMax(0, minX);
    maxX = qMin(maxX, mWidth);
    minY = qMax(0, minY);
    maxY = qMin(maxY, mHeight);

    for (int x = minX; x <= maxX; x++)
        painter->drawLine(x * 30, minY * 30, x * 30, maxY * 30);

    for (int y = minY; y <= maxY; y++)
        painter->drawLine(minX * 30, y * 30, maxX * 30, y * 30);
}

void GraphicsGridItem::setSize(int width, int height)
{
    prepareGeometryChange();
    mWidth = width;
    mHeight = height;
}

/////

GraphicsObjectItem::GraphicsObjectItem(BuildingBaseScene *editor, BuildingObject *object) :
    QGraphicsItem(),
    mEditor(editor),
    mObject(object),
    mSelected(false),
    mDragging(false),
    mValidPos(true),
    mMouseOver(false)
{
    // Cursor objects and Ortho-view objects should be fully visible
    if (mEditor->renderer()->asIso()) {
        if (mObject->floor()) {
            setOpacity(0.25);
        } else {
            if (mObject->asFurniture())
                setOpacity(0.25);
            else
                setOpacity(0.55);
        }
    }

    setVisible(mEditor->shouldShowObjectItem(mObject));
}

QPainterPath GraphicsObjectItem::shape() const
{
    return mShape;
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

    // Cursor objects have no floor.
    int level = mObject->floor() ? mObject->floor()->level() : mEditor->currentLevel();

    mEditor->renderer()->drawObject(painter, mObject, dragOffset, mValidPos,
                                    mSelected, mMouseOver, level);
}

void GraphicsObjectItem::setObject(BuildingObject *object)
{
    mObject = object;
    synchWithObject();
    update();
}

void GraphicsObjectItem::synchWithObject()
{
    mValidPos = mObject->isValidPos(mDragging ? mDragOffset : QPoint(),
                                    // This is a hack for cursor objects
                                    // that haven't been added to the floor.
                                    mEditor->document()->currentFloor());

    QPainterPath shape = calcShape();
    QRectF bounds = shape.boundingRect();
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
        mShape = shape;
    }
}

QPainterPath GraphicsObjectItem::calcShape()
{
    QPainterPath path;
    QPoint dragOffset = mDragging ? mDragOffset : QPoint();

    // Cursor objects have no floor.
    int level = mObject->floor() ? mObject->floor()->level() : mEditor->currentLevel();

    QPolygonF tilePolygon = mObject->calcShape().translated(dragOffset);
    QPolygonF scenePolygon = mEditor->tileToScenePolygon(tilePolygon, level);
    path.addPolygon(scenePolygon);
    return path;
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

void GraphicsObjectItem::setMouseOver(bool mouseOver)
{
    if (mouseOver != mMouseOver) {
        mMouseOver = mouseOver;
        if (dynamic_cast<OrthoBuildingRenderer*>(mEditor->renderer()) == 0)
            setOpacity(mMouseOver ? 0.55 : 0.25);
        update();
    }
}

/////

GraphicsRoofHandleItem::GraphicsRoofHandleItem(GraphicsRoofItem *roofItem, Type type) :
    QGraphicsItem(roofItem),
    mEditor(roofItem->editor()),
    mRoofItem(roofItem),
    mType(type),
    mHighlight(false)
{
    switch (mType) {
    case Resize:
        mStatusText = QCoreApplication::translate("Tools", "Left-click-drag to resize the roof.");
        break;
    case DepthUp:
        mStatusText = QCoreApplication::translate("Tools", "Left-click to increase height.");
        break;
    case DepthDown:
        mStatusText = QCoreApplication::translate("Tools", "Left-click to decrease height.");
        break;
    case CappedW:
    case CappedN:
    case CappedE:
    case CappedS:
        mStatusText = QCoreApplication::translate("Tools", "Left-click to toggle end cap.");
        break;
    case Orient:
        mStatusText = QCoreApplication::translate("Tools", "Left-click to toggle orientation.");
        break;
    }

    synchWithObject();
}

QPainterPath GraphicsRoofHandleItem::shape() const
{
    // Cursor objects have no floor.
    int level = mRoofItem->object()->floor() ? mRoofItem->object()->floor()->level()
                                             : mEditor->currentLevel();

    QPainterPath path;
    path.addPolygon(mEditor->tileToScenePolygonF(mTileBounds, level));
    return path;
}

QRectF GraphicsRoofHandleItem::boundingRect() const
{
    return mBoundingRect;
}

void GraphicsRoofHandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // Cursor objects have no floor.
    int level = mRoofItem->object()->floor() ? mRoofItem->object()->floor()->level()
                                             : mEditor->currentLevel();

    QPolygonF polygon = mEditor->tileToScenePolygonF(mTileBounds, level);
    painter->setBrush(mHighlight ? Qt::white : Qt::gray);
    painter->drawConvexPolygon(polygon);
//    painter->drawRect(r);

    bool cross = false;
    RoofObject *roof = mRoofItem->object()->asRoof();
    switch (mType) {
    case Resize:
        break;
    case DepthUp:
        cross = roof->isDepthMax();
        break;
    case DepthDown:
        cross = roof->isDepthMin();
        break;
    case CappedW:
        cross = roof->isCappedW() == false;
        break;
    case CappedN:
        cross = roof->isCappedN() == false;
        break;
    case CappedE:
        cross = roof->isCappedE() == false;
        break;
    case CappedS:
        cross = roof->isCappedS() == false;
        break;
    case Orient:
        break;
    }

    if (cross) {
        mEditor->drawLine(painter, mTileBounds.topLeft(), mTileBounds.bottomRight(),
                          level);
        mEditor->drawLine(painter, mTileBounds.topRight(), mTileBounds.bottomLeft(),
                          level);
    }
}

void GraphicsRoofHandleItem::synchWithObject()
{
    // Cursor objects have no floor.
    int level = mRoofItem->object()->floor() ? mRoofItem->object()->floor()->level()
                                             : mEditor->currentLevel();

    mTileBounds = calcBoundingRect();
    QRectF r = mEditor->tileToScenePolygonF(mTileBounds, level).boundingRect();
    if (r != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = r;
    }

    bool visible = true;
    RoofObject *roof = mRoofItem->object()->asRoof();
    switch (mType) {
    case Resize:
        break;
    case DepthUp:
        visible = roof->roofType() == RoofObject::FlatTop;
        break;
    case DepthDown:
        visible = roof->roofType() == RoofObject::FlatTop;
        break;
    case CappedW:
        visible = roof->roofType() != RoofObject::SlopeW &&
                roof->roofType() != RoofObject::PeakNS &&
                roof->roofType() != RoofObject::CornerOuterNW &&
                roof->roofType() != RoofObject::CornerOuterSW;
        break;
    case CappedN:
        visible = roof->roofType() != RoofObject::SlopeN &&
                roof->roofType() != RoofObject::PeakWE &&
                roof->roofType() != RoofObject::CornerOuterNW &&
                roof->roofType() != RoofObject::CornerOuterNE;
        break;
    case CappedE:
        visible = roof->roofType() != RoofObject::SlopeE &&
                roof->roofType() != RoofObject::PeakNS &&
                roof->roofType() != RoofObject::CornerOuterNE &&
                roof->roofType() != RoofObject::CornerOuterSE;
        break;
    case CappedS:
        visible = roof->roofType() != RoofObject::SlopeS &&
                roof->roofType() != RoofObject::PeakWE &&
                roof->roofType() != RoofObject::CornerOuterSW &&
                roof->roofType() != RoofObject::CornerOuterSE;
        break;
    case Orient:
        break;
    }
    setVisible(mRoofItem->handlesVisible() && visible);
}

void GraphicsRoofHandleItem::setHighlight(bool highlight)
{
    if (highlight == mHighlight)
        return;
    mHighlight = highlight;
    update();
}

QRectF GraphicsRoofHandleItem::calcBoundingRect()
{
    QRectF r = mRoofItem->object()->bounds();

    switch (mType) {
    case Resize:
        r.setLeft(r.right() - 15/30.0);
        r.setTop(r.bottom() - 15/30.0);
        break;
    case CappedW:
        r.setRight(r.left() + 15/30.0);
        r.adjust(0,15/30.0,0,-15/30.0);
        break;
    case CappedN:
        r.setBottom(r.top() + 15/30.0);
        r.adjust(15/30.0,0,-15/30.0,0);
        break;
    case CappedE:
        r.setLeft(r.right() - 15/30.0);
        r.adjust(0,15/30.0,0,-15/30.0);
        break;
    case CappedS:
        r.setTop(r.bottom() - 15/30.0);
        r.adjust(15/30.0,0,-15/30.0,0);
        break;
    case DepthUp:
        r = QRectF(r.center().x()-7/30.0,r.center().y()-14/30.0,
                  14/30.0, 14/30.0);
        break;
    case DepthDown:
        r = QRectF(r.center().x()-7/30.0,r.center().y(),
                  14/30.0, 14/30.0);
        break;
    case Orient:
        r.setRight(r.left() + 15/30.0);
        r.setBottom(r.top() + 15/30.0);
        break;
    }

    return r;
}

/////

GraphicsRoofItem::GraphicsRoofItem(BuildingBaseScene *editor, RoofObject *roof) :
    GraphicsObjectItem(editor, roof),
    mShowHandles(false),
    mResizeItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::Resize)),
    mDepthUpItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::DepthUp)),
    mDepthDownItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::DepthDown)),
    mCappedWItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::CappedW)),
    mCappedNItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::CappedN)),
    mCappedEItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::CappedE)),
    mCappedSItem(new GraphicsRoofHandleItem(this, GraphicsRoofHandleItem::CappedS))
{
    mResizeItem->setCursor(Qt::SizeAllCursor);
}

void GraphicsRoofItem::synchWithObject()
{
    GraphicsObjectItem::synchWithObject();

    mResizeItem->synchWithObject();
    mDepthUpItem->synchWithObject();
    mDepthDownItem->synchWithObject();
    mCappedWItem->synchWithObject();
    mCappedNItem->synchWithObject();
    mCappedEItem->synchWithObject();
    mCappedSItem->synchWithObject();
}

void GraphicsRoofItem::setShowHandles(bool show)
{
    if (mShowHandles == show)
        return;
    mShowHandles = show;
    synchWithObject();
}

/////

GraphicsWallHandleItem::GraphicsWallHandleItem(GraphicsWallItem *wallItem) :
    QGraphicsItem(wallItem),
    mWallItem(wallItem),
    mHighlight(false)
{
    setCursor(Qt::SizeAllCursor);
}

QRectF GraphicsWallHandleItem::boundingRect() const
{
    return mBoundingRect;
}

void GraphicsWallHandleItem::paint(QPainter *painter,
                                   const QStyleOptionGraphicsItem *option,
                                   QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // Cursor objects have no floor.
    int level = mWallItem->object()->floor() ? mWallItem->object()->floor()->level()
                                             : mWallItem->editor()->currentLevel();

    QPolygonF polygon = mWallItem->editor()->tileToScenePolygonF(mTileRect, level);
    painter->setBrush(mHighlight ? Qt::white : Qt::gray);
    painter->drawConvexPolygon(polygon);
//    painter->drawRect(r);
}

void GraphicsWallHandleItem::synchWithObject()
{
    // Cursor objects have no floor.
    int level = mWallItem->object()->floor() ? mWallItem->object()->floor()->level()
                                             : mWallItem->editor()->currentLevel();

    mTileRect = calcBoundingRect();
    QRectF r = mWallItem->editor()->tileToScenePolygonF(mTileRect, level).boundingRect();
    if (r != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = r;
    }

    setVisible(mWallItem->handlesVisible());
}

void GraphicsWallHandleItem::setHighlight(bool highlight)
{
    if (highlight == mHighlight)
        return;
    mHighlight = highlight;
    update();
}

QRectF GraphicsWallHandleItem::calcBoundingRect()
{
    QRectF r = mWallItem->object()->calcShape().boundingRect();

    r.setLeft(r.right() - 12/30.0);
    r.setTop(r.bottom() - 12/30.0);

    return r;
}

/////

GraphicsWallItem::GraphicsWallItem(BuildingBaseScene *editor, WallObject *wall) :
    GraphicsObjectItem(editor, wall),
    mShowHandles(false),
    mResizeItem(new GraphicsWallHandleItem(this))
{
}

void GraphicsWallItem::synchWithObject()
{
    GraphicsObjectItem::synchWithObject();
    mResizeItem->synchWithObject();
}

void GraphicsWallItem::setShowHandles(bool show)
{
    if (mShowHandles == show)
        return;
    mShowHandles = show;
    synchWithObject();
}

/////

BuildingOrthoScene::BuildingOrthoScene(QObject *parent) :
    BuildingBaseScene(parent),
    mCurrentTool(0)
{
    ZVALUE_GRID = 20;
    ZVALUE_CURSOR = 100;

    mRenderer = new OrthoBuildingRenderer;

    setBackgroundBrush(Qt::black);

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

bool BuildingOrthoScene::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            ToolManager::instance()->checkKeyboardModifiers(keyEvent->modifiers());
        }
        break;
    default:
        break;
    }

    return false;
}

void BuildingOrthoScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void BuildingOrthoScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void BuildingOrthoScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void BuildingOrthoScene::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    clear();

    mFloorItems.clear();
    mSelectedObjectItems.clear();
    mMouseOverObject = 0;

    if (mDocument) {
        foreach (BuildingFloor *floor, building()->floors())
            floorAdded(floor);
        currentFloorChanged();

        mGridItem = new GraphicsGridItem(building()->width(),
                                         building()->height());
        mGridItem->setZValue(ZVALUE_GRID);
        addItem(mGridItem);

        setSceneRect(-10, -10,
                     building()->width() * 30 + 20,
                     building()->height() * 30 + 20);

        connect(mDocument, SIGNAL(currentFloorChanged()),
                SLOT(currentFloorChanged()));

        connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
                SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));

        connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
                SLOT(floorAdded(BuildingFloor*)));
        connect(mDocument, SIGNAL(floorRemoved(BuildingFloor*)),
                SLOT(floorRemoved(BuildingFloor*)));
        connect(mDocument, SIGNAL(floorEdited(BuildingFloor*)),
                SLOT(floorEdited(BuildingFloor*)));

        connect(mDocument, SIGNAL(objectAdded(BuildingObject*)),
                SLOT(objectAdded(BuildingObject*)));
        connect(mDocument, SIGNAL(objectAboutToBeRemoved(BuildingObject*)),
                SLOT(objectAboutToBeRemoved(BuildingObject*)));
        connect(mDocument, SIGNAL(objectMoved(BuildingObject*)),
                SLOT(objectMoved(BuildingObject*)));
        connect(mDocument, SIGNAL(objectTileChanged(BuildingObject*)),
                SLOT(objectTileChanged(BuildingObject*)));
        connect(mDocument, SIGNAL(objectChanged(BuildingObject*)),
                SLOT(objectChanged(BuildingObject*)));
        connect(mDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(selectedObjectsChanged()));

        connect(mDocument, SIGNAL(roomChanged(Room*)),
                SLOT(roomChanged(Room*)));
        connect(mDocument, SIGNAL(roomAdded(Room*)),
                SLOT(roomAdded(Room*)));
        connect(mDocument, SIGNAL(roomRemoved(Room*)),
                SLOT(roomRemoved(Room*)));
        connect(mDocument, SIGNAL(roomsReordered()),
                SLOT(roomsReordered()));

        connect(mDocument, SIGNAL(buildingResized()), SLOT(buildingResized()));
        connect(mDocument, SIGNAL(buildingRotated()), SLOT(buildingRotated()));
    }

    emit documentChanged();
}

void BuildingOrthoScene::clearDocument()
{
    setDocument(0);
}

void BuildingOrthoScene::currentToolChanged(BaseTool *tool)
{
    mCurrentTool = tool;
}

QPoint OrthoBuildingRenderer::sceneToTile(const QPointF &scenePos, int level)
{
    Q_UNUSED(level)
    // x/y < 0 rounds up to zero
    qreal x = scenePos.x() / 30, y = scenePos.y() / 30;
    if (x < 0)
        x = -qCeil(qAbs(x));
    if (y < 0)
        y = -qCeil(qAbs(y));
    return QPoint(x, y);
}

QPointF OrthoBuildingRenderer::sceneToTileF(const QPointF &scenePos, int level)
{
    Q_UNUSED(level)
    return scenePos / 30;
}

QRect OrthoBuildingRenderer::sceneToTileRect(const QRectF &sceneRect, int level)
{
    QPoint topLeft = sceneToTile(sceneRect.topLeft(), level);
    QPoint botRight = sceneToTile(sceneRect.bottomRight(), level);
    return QRect(topLeft, botRight);
}

QRectF OrthoBuildingRenderer::sceneToTileRectF(const QRectF &sceneRect, int level)
{
    QPointF topLeft = sceneToTileF(sceneRect.topLeft(), level);
    QPointF botRight = sceneToTileF(sceneRect.bottomRight(), level);
    return QRectF(topLeft, botRight);
}

QPointF OrthoBuildingRenderer::tileToScene(const QPoint &tilePos, int level)
{
    Q_UNUSED(level)
    return tilePos * 30;
}

QPointF OrthoBuildingRenderer::tileToSceneF(const QPointF &tilePos, int level)
{
    Q_UNUSED(level)
    return tilePos * 30;
}

QPolygonF OrthoBuildingRenderer::tileToScenePolygon(const QPoint &tilePos, int level)
{
    QPolygonF polygon;
    polygon += tilePos;
    polygon += tilePos + QPoint(1, 0);
    polygon += tilePos + QPoint(1, 1);
    polygon += tilePos + QPoint(0, 1);
    polygon += polygon.first();
    return tileToScenePolygon(polygon, level);
}

QPolygonF OrthoBuildingRenderer::tileToScenePolygon(const QRect &tileRect, int level)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight() + QPoint(1, 0);
    polygon += tileRect.bottomRight() + QPoint(1, 1);
    polygon += tileRect.bottomLeft() + QPoint(0, 1);
    polygon += polygon.first();
    return tileToScenePolygon(polygon, level);
}

QPolygonF OrthoBuildingRenderer::tileToScenePolygonF(const QRectF &tileRect, int level)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight();
    polygon += tileRect.bottomRight();
    polygon += tileRect.bottomLeft();
    polygon += polygon.first();
    return tileToScenePolygon(polygon, level);
}

QPolygonF OrthoBuildingRenderer::tileToScenePolygon(const QPolygonF &tilePolygon, int level)
{
    QPolygonF polygon(tilePolygon.size());
    for (int i = tilePolygon.size() - 1; i >= 0; --i)
        polygon[i] = tileToSceneF(tilePolygon[i], level);
    return polygon;
}

void OrthoBuildingRenderer::drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level)
{
    painter->drawLine(tileToSceneF(QPointF(x1, y1), level), tileToSceneF(QPointF(x2, y2), level));
}

void BuildingOrthoScene::currentFloorChanged()
{
    int level = mDocument->currentLevel();
    for (int i = 0; i <= level; i++) {
        mFloorItems[i]->setOpacity((i == level) ? 1.0 : 0.15);
        mFloorItems[i]->setVisible(true);
    }
    for (int i = level + 1; i < building()->floorCount(); i++)
        mFloorItems[i]->setVisible(false);
}

void BuildingOrthoScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    itemForFloor(floor)->roomAtPositionChanged(pos);
}

void BuildingOrthoScene::roomChanged(Room *room)
{
    foreach (GraphicsFloorItem *item, mFloorItems)
        item->roomChanged(room);
}

void BuildingOrthoScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    // This is only to support undoing removing a room.
    // When the room is re-added, the floor grid gets put
    // back the way it was, so we have to update the bitmap.
}

void BuildingOrthoScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, building()->floors())
        floorEdited(floor);
}

void BuildingOrthoScene::roomsReordered()
{
}

void BuildingOrthoScene::buildingResized()
{
    buildingRotated();
}

void BuildingOrthoScene::buildingRotated()
{
    foreach (GraphicsFloorItem *item, mFloorItems) {
        item->synchWithFloor();
        floorEdited(item->floor());
    }

    mGridItem->setSize(building()->width(), building()->height());

    setSceneRect(-10, -10,
                 building()->width() * 30 + 20,
                 building()->height() * 30 + 20);
}

/////

BuildingOrthoView::BuildingOrthoView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this)),
    mHandScrolling(false)
{
    // Alignment of the scene within the view
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

void BuildingOrthoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void BuildingOrthoView::mouseMoveEvent(QMouseEvent *event)
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

    QPoint tilePos = scene()->sceneToTile(mLastMouseScenePos, scene()->currentLevel());
    if (tilePos != mLastMouseTilePos) {
        mLastMouseTilePos = tilePos;
        emit mouseCoordinateChanged(mLastMouseTilePos);
    }
}

void BuildingOrthoView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(false);
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void BuildingOrthoView::wheelEvent(QWheelEvent *event)
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

void BuildingOrthoView::setHandScrolling(bool handScrolling)
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

void BuildingOrthoView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}

/////
