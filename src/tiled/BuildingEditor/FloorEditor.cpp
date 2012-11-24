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
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingtools.h"
#include "buildingtemplates.h"
#include "furnituregroups.h"

#include "zoomable.h"

#include <QAction>
#include <QDebug>
#include <QStyleOptionGraphicsItem>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <qmath.h>

using namespace BuildingEditor;

using namespace Tiled;
using namespace Internal;

GraphicsFloorItem::GraphicsFloorItem(BuildingFloor *floor) :
    QGraphicsItem(),
    mFloor(floor),
    mBmp(new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32)),
    mDragBmp(0)
{
    setFlag(ItemUsesExtendedStyleOption);
    mBmp->fill(Qt::black);
}

GraphicsFloorItem::~GraphicsFloorItem()
{
    delete mBmp;
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return QRectF(0, 0, mFloor->width() * 30, mFloor->height() * 30);
}

void GraphicsFloorItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
{
    int minX = qFloor(option->exposedRect.left() / 30) - 1;
    int maxX = qCeil(option->exposedRect.right() / 30) + 1;
    int minY = qFloor(option->exposedRect.top() / 30) - 1;
    int maxY = qCeil(option->exposedRect.bottom() / 30) + 1;

    minX = qMax(0, minX);
    maxX = qMin(maxX, mFloor->width());
    minY = qMax(0, minY);
    maxY = qMin(maxY, mFloor->height());

    QImage *bmp = mDragBmp ? mDragBmp : mBmp;
    for (int x = minX; x < maxX; x++) {
        for (int y = 0; y < maxY; y++) {
            QRgb c = bmp->pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue;
            painter->fillRect(x * 30, y * 30, 30, 30, c);
        }
    }
}

void GraphicsFloorItem::objectAdded(GraphicsObjectItem *item)
{
    BuildingObject *object = item->object();
    Q_ASSERT(!itemForObject(object));
    item->setParentItem(this);
    item->setVisible(BuildingPreferences::instance()->showObjects());
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

void GraphicsFloorItem::setDragBmp(QImage *bmp)
{
    mDragBmp = bmp;
    update();
}

void GraphicsFloorItem::showObjectsChanged(bool show)
{
    foreach (GraphicsObjectItem *item, mObjectItems)
        item->setVisible(show);
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
#if 1
    QBrush brush(QColor(128, 128, 220, 80), Qt::Dense4Pattern);
    brush.setTransform(QTransform::fromScale(1/painter->transform().m11(),
                                             1/painter->transform().m22()));
    pen.setBrush(brush);
#else
    pen.setWidth(2);
    pen.setStyle(Qt::DotLine); // FIXME: causes graphics corruption at some scales
#endif
    painter->setPen(pen);

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

#if 0
    static QColor dbg = Qt::blue;
    painter->fillRect(option->exposedRect, dbg);
    if (dbg == Qt::blue) dbg = Qt::green; else dbg = Qt::blue;
#endif
}

void GraphicsGridItem::setSize(int width, int height)
{
    prepareGeometryChange();
    mWidth = width;
    mHeight = height;
}

/////

GraphicsObjectItem::GraphicsObjectItem(FloorEditor *editor, BuildingObject *object) :
    QGraphicsItem(),
    mEditor(editor),
    mObject(object),
    mSelected(false),
    mDragging(false),
    mValidPos(true)
{
    synchWithObject();
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
                               const QStyleOptionGraphicsItem *option,
                               QWidget *)
{
    QPainterPath path = shape();
    QColor color = Qt::white;
    painter->fillPath(path, color);
    QPen pen(mValidPos ? (mSelected ? Qt::cyan : Qt::blue) : Qt::red);

    QPoint dragOffset = mDragging ? mDragOffset : QPoint();

    if (Stairs *stairs = mObject->asStairs()) {
        QRectF r = path.boundingRect();
        if (stairs->isW()) {
            for (int x = 30; x <= 120; x += 10)
                painter->drawLine(r.left()+x, r.top(), r.left()+x,r.bottom());
        } else {
            for (int y = 30; y <= 120; y += 10)
                painter->drawLine(r.left(), r.top()+y, r.right(),r.top()+y);

        }
    }

    // Draw line(s) indicating the orientation of the furniture tile.
    if (FurnitureObject *object = mObject->asFurniture()) {
        QRectF r = mEditor->tileToSceneRect(object->bounds().translated(dragOffset));
        r.adjust(2, 2, -2, -2);

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
                layer == FurnitureTiles::LayerWalls)
            lineW = lineN = lineE = lineS = false;

        QPainterPath path2;
        if (lineW)
            path2.addRect(r.left() + 2, r.top() + 2, 2, r.height() - 4);
        if (lineE)
            path2.addRect(r.right() - 4, r.top() + 2, 2, r.height() - 4);
        if (lineN)
            path2.addRect(r.left() + 2, r.top() + 2, r.width() - 4, 2);
        if (lineS)
            path2.addRect(r.left() + 2, r.bottom() - 4, r.width() - 4, 2);
        painter->fillPath(path2, pen.color());
    }

    if (RoofObject *roof = mObject->asRoof()) {
        QRectF ne = roof->northEdge().translated(dragOffset);
        QRectF se = roof->southEdge().translated(dragOffset);
        if ((roof->roofType() == RoofObject::PeakWE) && roof->isHalfDepth()) {
            ne.adjust(0,0,0,-0.5);
            se.adjust(0,0.5,0,0);
        }
        painter->fillRect(mEditor->tileToSceneRectF(ne), Qt::darkGray);
        painter->fillRect(mEditor->tileToSceneRectF(se), Qt::lightGray);
        QRectF we = roof->westEdge().translated(dragOffset);
        QRectF ee = roof->eastEdge().translated(dragOffset);
        if ((roof->roofType() == RoofObject::PeakNS) && roof->isHalfDepth()) {
            we.adjust(0,0,-0.5,0);
            ee.adjust(0.5,0,0,0);
        }
        painter->fillRect(mEditor->tileToSceneRectF(we), Qt::darkGray);
        painter->fillRect(mEditor->tileToSceneRectF(ee), Qt::lightGray);
        painter->fillRect(mEditor->tileToSceneRectF(roof->flatTop().translated(dragOffset)), Qt::gray);
    }

    painter->setPen(pen);
    painter->drawPath(path);

    if (!mValidPos) {
        painter->fillPath(path, QColor(255, 0, 0, 128));
    }

    if (mSelected) {
        painter->setOpacity(0.5);
        painter->fillPath(path, option->palette.highlight());
    }
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

    // Screw you, polymorphism!!!
    if (Door *door = mObject->asDoor()) {
        if (door->dir() == BuildingObject::N) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            path.addRect(p.x(), p.y() - 5, 30, 10);
        }
        if (door->dir() == BuildingObject::W) {
            QPointF p = mEditor->tileToScene(door->pos() + dragOffset);
            path.addRect(p.x() - 5, p.y(), 10, 30);
        }
    }

    if (Window *window = mObject->asWindow()) {
        if (window->dir() == BuildingObject::N) {
            QPointF p = mEditor->tileToScene(window->pos() + dragOffset);
            path.addRect(p.x() + 7, p.y() - 3, 16, 6);
        }
        if (window->dir() == BuildingObject::W) {
            QPointF p = mEditor->tileToScene(window->pos() + dragOffset);
            path.addRect(p.x() - 3, p.y() + 7, 6, 16);
        }
    }

    if (Stairs *stairs = mObject->asStairs()) {
        if (stairs->dir() == BuildingObject::N) {
            QPointF p = mEditor->tileToScene(stairs->pos() + dragOffset);
            path.addRect(p.x(), p.y(), 30, 30 * 5);
        }
        if (stairs->dir() == BuildingObject::W) {
            QPointF p = mEditor->tileToScene(stairs->pos() + dragOffset);
            path.addRect(p.x(), p.y(), 30 * 5, 30);
        }
    }

    if (FurnitureObject *object = mObject->asFurniture()) {
        QRectF r = mEditor->tileToSceneRect(object->bounds().translated(dragOffset));
        if (object->inWallLayer()) {
            if (object->furnitureTile()->isW())
                r.setRight(r.left() + 10);
            else if (object->furnitureTile()->isN())
                r.setBottom(r.top() + 10);
            else if (object->furnitureTile()->isE())
                r.setLeft(r.right() - 10);
            else if (object->furnitureTile()->isS())
                r.setTop(r.bottom() - 10);
            path.addRect(r);
            return path;
        }
        if (object->furnitureTile()->owner()->layer() == FurnitureTiles::LayerWalls) {
            if (object->furnitureTile()->isW()) {
                r.setRight(r.left() + 10);
                r.translate(-5, 0);
            } else if (object->furnitureTile()->isE()) {
                r.setLeft(r.right() - 10);
                r.translate(5, 0);
            } else if (object->furnitureTile()->isN()) {
                r.setBottom(r.top() + 10);
                r.translate(0, -5);
            } else if (object->furnitureTile()->isS()) {
                r.setTop(r.bottom() - 10);
                r.translate(0, 5);
            }
            path.addRect(r);
            return path;
        }
        if (object->furnitureTile()->owner()->layer() == FurnitureTiles::LayerFrames) {
            // Mimic window shape
            if (object->furnitureTile()->isW()) {
                r.setRight(r.left() + 6);
                r.adjust(0,7,0,-7);
                r.translate(-3, 0);
            } else if (object->furnitureTile()->isE()) {
                r.setLeft(r.right() - 6);
                r.adjust(0,7,0,-7);
                r.translate(3, 0);
            } else if (object->furnitureTile()->isN()) {
                r.adjust(7,0,-7,0);
                r.setBottom(r.top() + 6);
                r.translate(0, -3);
            } else if (object->furnitureTile()->isS()) {
                r.adjust(7,0,-7,0);
                r.setTop(r.bottom() - 6);
                r.translate(0, 3);
            }
            path.addRect(r);
            return path;
        }
        r.adjust(2, 2, -2, -2);
        path.addRect(r);
    }

    if (RoofObject *roof = mObject->asRoof()) {
        path.addRect(mEditor->tileToSceneRect(roof->bounds().translated(dragOffset)));
    }

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

/////

GraphicsRoofHandleItem::GraphicsRoofHandleItem(GraphicsRoofItem *roofItem,
                                               GraphicsRoofHandleItem::Type type) :
    QGraphicsItem(roofItem),
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

QRectF GraphicsRoofHandleItem::boundingRect() const
{
    return mBoundingRect;
}

void GraphicsRoofHandleItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    QRectF r = mBoundingRect;
    painter->fillRect(r, mHighlight ? Qt::white : Qt::gray);
    painter->drawRect(r);

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
        painter->drawLine(r.topLeft(), r.bottomRight());
        painter->drawLine(r.topRight(), r.bottomLeft());
    }
}

void GraphicsRoofHandleItem::synchWithObject()
{
    QRectF r = calcBoundingRect();
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
                roof->roofType() != RoofObject::PeakNS;
        break;
    case CappedN:
        visible = roof->roofType() != RoofObject::SlopeN &&
                roof->roofType() != RoofObject::PeakWE;
        break;
    case CappedE:
        visible = roof->roofType() != RoofObject::SlopeE &&
                roof->roofType() != RoofObject::PeakNS;
        break;
    case CappedS:
        visible = roof->roofType() != RoofObject::SlopeS &&
                roof->roofType() != RoofObject::PeakWE;
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
    QRectF r = mRoofItem->boundingRect().translated(-mRoofItem->pos());

    switch (mType) {
    case Resize:
        r.setLeft(r.right() - 15);
        r.setTop(r.bottom() - 15);
        break;
    case CappedW:
        r.setRight(r.left() + 15);
        r.adjust(0,15,0,-15);
        break;
    case CappedN:
        r.setBottom(r.top() + 15);
        r.adjust(15,0,-15,0);
        break;
    case CappedE:
        r.setLeft(r.right() - 15);
        r.adjust(0,15,0,-15);
        break;
    case CappedS:
        r.setTop(r.bottom() - 15);
        r.adjust(15,0,-15,0);
        break;
    case DepthUp:
        r = QRectF(r.center().x()-7,r.center().y()-14,
                  14, 14);
        break;
    case DepthDown:
        r = QRectF(r.center().x()-7,r.center().y(),
                  14, 14);
        break;
    case Orient:
        r.setRight(r.left() + 15);
        r.setBottom(r.top() + 15);
        break;
    }

    return r;
}

/////

GraphicsRoofItem::GraphicsRoofItem(FloorEditor *editor, RoofObject *roof) :
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

const int FloorEditor::ZVALUE_GRID = 20;
const int FloorEditor::ZVALUE_CURSOR = 100;

FloorEditor::FloorEditor(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mCurrentTool(0)
{
    setBackgroundBrush(Qt::black);

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

bool FloorEditor::eventFilter(QObject *watched, QEvent *event)
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
    mSelectedObjectItems.clear();

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

        BuildingPreferences *prefs = BuildingPreferences::instance();
        connect(prefs, SIGNAL(showObjectsChanged(bool)), SLOT(showObjectsChanged(bool)));
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

void FloorEditor::currentToolChanged(BaseTool *tool)
{
    mCurrentTool = tool;
}

QPoint FloorEditor::sceneToTile(const QPointF &scenePos)
{
    // FIXME: x/y < 0 rounds up to zero
    qreal x = scenePos.x() / 30, y = scenePos.y() / 30;
    if (x < 0)
        x = -qCeil(qAbs(x));
    if (y < 0)
        y = -qCeil(qAbs(y));
    return QPoint(x, y);
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

QRectF FloorEditor::tileToSceneRectF(const QRectF &tileRect)
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

GraphicsFloorItem *FloorEditor::itemForFloor(BuildingFloor *floor)
{
    return mFloorItems[floor->level()];
}

GraphicsObjectItem *FloorEditor::itemForObject(BuildingObject *object)
{
    return itemForFloor(object->floor())->itemForObject(object);
}

QSet<BuildingObject*> FloorEditor::objectsInRect(const QRectF &sceneRect)
{
    QSet<BuildingObject*> objects;
    foreach (QGraphicsItem *item, items(sceneRect)) {
        if (GraphicsObjectItem *objectItem = dynamic_cast<GraphicsObjectItem*>(item)) {
            if (objectItem->object()->floor() == mDocument->currentFloor())
                objects += objectItem->object();
        }
    }
    return objects;
}

BuildingObject *FloorEditor::topmostObjectAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, items(scenePos)) {
        if (GraphicsObjectItem *objectItem = dynamic_cast<GraphicsObjectItem*>(item)) {
            if (objectItem->object()->floor() == mDocument->currentFloor())
                return objectItem->object();
        }
    }
    return 0;
}

void FloorEditor::currentFloorChanged()
{
    int level = mDocument->currentLevel();
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
    Room *room = floor->GetRoomAt(pos);
//    qDebug() << floor << pos << room;
    mFloorItems[index]->bmp()->setPixel(pos, room ? room->Color : qRgb(0, 0, 0));
    mFloorItems[index]->update();
}

void FloorEditor::floorAdded(BuildingFloor *floor)
{
    GraphicsFloorItem *item = new GraphicsFloorItem(floor);
    mFloorItems.insert(floor->level(), item);
    addItem(item);

    foreach (GraphicsFloorItem *item, mFloorItems)
        item->setZValue(item->floor()->level());

    floorEdited(floor);

    foreach (BuildingObject *object, floor->objects())
        objectAdded(object);
}

void FloorEditor::floorRemoved(BuildingFloor *floor)
{
    Q_ASSERT(mFloorItems[floor->level()]
             && mFloorItems[floor->level()]->floor() == floor);
    delete mFloorItems.takeAt(floor->level());

    foreach (GraphicsFloorItem *item, mFloorItems)
        item->setZValue(item->floor()->level());
}

void FloorEditor::floorEdited(BuildingFloor *floor)
{
    int index = floor->building()->floors().indexOf(floor);
    GraphicsFloorItem *item = mFloorItems[index];

    QImage *bmp = item->bmp();
    bmp->fill(Qt::black);
    for (int x = 0; x < floor->width(); x++) {
        for (int y = 0; y < floor->height(); y++) {
            if (Room *room = floor->GetRoomAt(x, y))
                bmp->setPixel(x, y, room->Color);
        }
    }

    item->update();
}

void FloorEditor::objectAdded(BuildingObject *object)
{
    Q_ASSERT(!itemForObject(object));
    GraphicsObjectItem *item;
    if (RoofObject *roof = object->asRoof())
        item = new GraphicsRoofItem(this, roof);
    else
        item = new GraphicsObjectItem(this, object);
    itemForFloor(object->floor())->objectAdded(item);
}

void FloorEditor::objectAboutToBeRemoved(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    mSelectedObjectItems.remove(item); // paranoia
    itemForFloor(object->floor())->objectAboutToBeRemoved(item);
    removeItem(item);
    delete item;
}

void FloorEditor::objectMoved(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
}

void FloorEditor::objectTileChanged(BuildingObject *object)
{
    // FurnitureObject might change size/orientation so redisplay
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->synchWithObject();
    item->update();
}

// This is for roofs being edited via handles
void FloorEditor::objectChanged(BuildingObject *object)
{
    GraphicsObjectItem *item = itemForObject(object);
    Q_ASSERT(item);
    item->update();
}

void FloorEditor::selectedObjectsChanged()
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

void FloorEditor::roomChanged(Room *room)
{
    foreach (GraphicsFloorItem *item, mFloorItems) {
        QImage *bmp = item->bmp();
//        bmp->fill(Qt::black);
        BuildingFloor *floor = item->floor();
        for (int x = 0; x < building()->width(); x++) {
            for (int y = 0; y < building()->height(); y++) {
                if (floor->GetRoomAt(x, y) == room)
                    bmp->setPixel(x, y, room->Color);
            }
        }
        item->update();
    }
}

void FloorEditor::roomAdded(Room *room)
{
    Q_UNUSED(room)
    // This is only to support undoing removing a room.
    // When the room is re-added, the floor grid gets put
    // back the way it was, so we have to update the bitmap.
}

void FloorEditor::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, building()->floors())
        floorEdited(floor);
}

void FloorEditor::roomsReordered()
{
}

void FloorEditor::buildingResized()
{
    buildingRotated();
}

void FloorEditor::buildingRotated()
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

void FloorEditor::showObjectsChanged(bool show)
{
    foreach (GraphicsFloorItem *item, mFloorItems)
        item->showObjectsChanged(show);
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
