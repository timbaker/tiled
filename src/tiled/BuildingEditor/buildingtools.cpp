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

#include "buildingtools.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "FloorEditor.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QUndoStack>

using namespace BuildingEditor;

/////

BaseTool::BaseTool() :
    QObject(0),
    mEditor(0)
{
    ToolManager::instance()->addTool(this);
}

void BaseTool::setEditor(FloorEditor *editor)
{
    mEditor = editor;

    connect(mEditor, SIGNAL(documentChanged()), SLOT(documentChanged()));
}

void BaseTool::setEnabled(bool enabled)
{
    if (enabled != mAction->isEnabled()) {
        mAction->setEnabled(enabled);
        ToolManager::instance()->toolEnabledChanged(this, enabled);
    }
}

void BaseTool::makeCurrent()
{
    ToolManager::instance()->activateTool(this);
}

/////

ToolManager *ToolManager::mInstance = 0;

ToolManager *ToolManager::instance()
{
    if (!mInstance)
        mInstance = new ToolManager;
    return mInstance;
}

ToolManager::ToolManager() :
    QObject(),
    mCurrentTool(0)
{
}

void ToolManager::addTool(BaseTool *tool)
{
    mTools += tool;
}

void ToolManager::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
    }

    mCurrentTool = tool;

    if (mCurrentTool) {
        mCurrentTool->activate();
        mCurrentTool->action()->setChecked(true);
    }

    emit currentToolChanged(mCurrentTool);
}

void ToolManager::toolEnabledChanged(BaseTool *tool, bool enabled)
{
    if (!enabled && tool == mCurrentTool) {
        foreach (BaseTool *tool2, mTools) {
            if (tool2 != tool && tool2->action()->isEnabled()) {
                activateTool(tool2);
                return;
            }
        }
        mCurrentTool = 0;
        emit currentToolChanged(mCurrentTool);
    }
}


/////

PencilTool *PencilTool::mInstance = 0;

PencilTool *PencilTool::instance()
{
    if (!mInstance)
        mInstance = new PencilTool();
    return mInstance;
}

PencilTool::PencilTool() :
    BaseTool(),
    mMouseDown(false),
    mCursor(0)
{
}

void PencilTool::documentChanged()
{
    mCursor = 0; // it was deleted from the editor
}

void PencilTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());

    if (event->button() == Qt::RightButton) {
        Room *room = mEditor->document()->currentFloor()->GetRoomAt(tilePos);
        if (room) {
            BuildingEditorWindow::instance->setCurrentRoom(room);
            updateCursor(event->scenePos());
        }
        return;
    }

    mInitialPaint = true;
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->document()->currentFloor()->GetRoomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
        mEditor->document()->undoStack()->push(new PaintRoom(mEditor->document(),
                                                             mEditor->document()->currentFloor(),
                                                             tilePos,
                                                             BuildingEditorWindow::instance->currentRoom()));
        mInitialPaint = false;
    }
    mMouseDown = true;
}

void PencilTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateCursor(event->scenePos());

    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->document()->currentFloor()->GetRoomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
            PaintRoom *cmd = new PaintRoom(mEditor->document(),
                                           mEditor->document()->currentFloor(),
                                           tilePos,
                                           BuildingEditorWindow::instance->currentRoom());
            cmd->setMergeable(!mInitialPaint);
            mEditor->document()->undoStack()->push(cmd);
            mInitialPaint = false;
        }
    }
}

void PencilTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        mMouseDown = false;
    }
}

void PencilTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void PencilTool::deactivate()
{
    mEditor->removeItem(mCursor);
}

void PencilTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!mCursor) {
        mCursor = new QGraphicsRectItem;
        mCursor->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mCursor->setRect(mEditor->tileToSceneRect(tilePos).adjusted(0,0,-1,-1));
    mCursor->setBrush(QColor(BuildingEditorWindow::instance->currentRoom()->Color));
    mCursor->setVisible(mEditor->currentFloorContains(tilePos));
}

/////

EraserTool *EraserTool::mInstance = 0;

EraserTool *EraserTool::instance()
{
    if (!mInstance)
        mInstance = new EraserTool();
    return mInstance;
}

EraserTool::EraserTool() :
    BaseTool(),
    mMouseDown(false),
    mCursor(0)
{
}

void EraserTool::documentChanged()
{
    mCursor = 0; // it was deleted from the editor
}

void EraserTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    mInitialPaint = true;
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->document()->currentFloor()->GetRoomAt(tilePos) != 0) {
        mEditor->document()->undoStack()->push(new EraseRoom(mEditor->document(),
                                                             mEditor->document()->currentFloor(),
                                                             tilePos));
        mInitialPaint = false;
    }
    mMouseDown = true;
}

void EraserTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateCursor(event->scenePos());

    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->document()->currentFloor()->GetRoomAt(tilePos) != 0) {
            EraseRoom *cmd = new EraseRoom(mEditor->document(),
                                                      mEditor->document()->currentFloor(),
                                                      tilePos);
            cmd->setMergeable(!mInitialPaint);
            mEditor->document()->undoStack()->push(cmd);
            mInitialPaint = false;
        }
    }
}

void EraserTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        mMouseDown = false;
    }
}

void EraserTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void EraserTool::deactivate()
{
    mEditor->removeItem(mCursor);
}

void EraserTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!mCursor) {
        QPen pen(QColor(255,0,0,128));
        pen.setWidth(3);
        mCursor = new QGraphicsRectItem;
        mCursor->setPen(pen);
        mCursor->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mCursor->setRect(mEditor->tileToSceneRect(tilePos).adjusted(0,0,-1,-1));
    mCursor->setVisible(mEditor->currentFloorContains(tilePos));
}

/////

BaseObjectTool::BaseObjectTool() :
    BaseTool(),
    mTileEdge(Center),
    mCursorItem(0)
{
}

void BaseObjectTool::documentChanged()
{
    mCursorItem = 0; // it was deleted from the editor
}

void BaseObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::RightButton)
        return;

    if (BaseMapObject *object = mEditor->topmostObjectAt(event->scenePos())) {
        BuildingFloor *floor = mEditor->document()->currentFloor();
        mEditor->document()->undoStack()->push(new RemoveObject(mEditor->document(),
                                                                floor,
                                                                floor->indexOf(object)));
    }
}

void BaseObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mTilePos = mEditor->sceneToTile(event->scenePos());

    QPointF p = mEditor->sceneToTileF(event->scenePos());
    QPointF m(p.x() - int(p.x()), p.y() - int(p.y()));
    TileEdge xEdge = Center, yEdge = Center;
    if (m.x() < 0.25)
        xEdge = W;
    else if (m.x() >= 0.75)
        xEdge = E;
    if (m.y() < 0.25)
        yEdge = N;
    else if (m.y() >= 0.75)
        yEdge = S;
    if (xEdge == Center && yEdge == Center || (xEdge != Center && yEdge != Center))
        mTileEdge = Center;
    else if (xEdge != Center)
        mTileEdge = xEdge;
    else
        mTileEdge = yEdge;
}

void BaseObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}

void BaseObjectTool::activate()
{
}

void BaseObjectTool::deactivate()
{
    if (mCursorItem) {
        mEditor->removeItem(mCursorItem);
        mCursorItem = 0;
    }
}

void BaseObjectTool::setCursorObject(BaseMapObject *object)
{
    if (!mCursorItem) {
        mCursorItem = new GraphicsObjectItem(mEditor, object);
        mCursorItem->setZValue(FloorEditor::ZVALUE_CURSOR);
        mEditor->addItem(mCursorItem);
    }
    mCursorItem->setObject(object);
}

/////

DoorTool *DoorTool::mInstance = 0;

DoorTool *DoorTool::instance()
{
    if (!mInstance)
        mInstance = new DoorTool();
    return mInstance;
}

DoorTool::DoorTool() :
    BaseObjectTool(),
    mCursorObject(0)
{
}

void DoorTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    // Right-click to delete objects.
    if (event->button() == Qt::RightButton) {
        BaseObjectTool::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (mTileEdge == Center)
        return;

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    BuildingFloor *floor = mEditor->document()->currentFloor();
    Door *door = new Door(floor, x, y, dir);
    door->mTile = BuildingTiles::instance()->tileForDoor(door,
                                                         mEditor->building()->doorTile());
    door->mFrameTile = BuildingTiles::instance()->tileForDoor(door,
                                                              mEditor->building()->doorFrameTile(),
                                                              true);
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         door));
}

void DoorTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    BaseObjectTool::mouseMoveEvent(event);

    if (mTileEdge == Center) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = mEditor->document()->currentFloor();
        mCursorObject = new Door(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

WindowTool *WindowTool::mInstance = 0;

WindowTool *WindowTool::instance()
{
    if (!mInstance)
        mInstance = new WindowTool();
    return mInstance;
}

WindowTool::WindowTool() :
    BaseObjectTool(),
    mCursorObject(0)
{
}

void WindowTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    // Right-click to delete objects.
    if (event->button() == Qt::RightButton) {
        BaseObjectTool::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (mTileEdge == Center)
        return;

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    BuildingFloor *floor = mEditor->document()->currentFloor();
    Window *window = new Window(floor, x, y, dir);
    window->mTile = BuildingTiles::instance()->tileForWindow(window,
                                                             mEditor->building()->windowTile());
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         window));
}

void WindowTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    BaseObjectTool::mouseMoveEvent(event);

    if (mTileEdge == Center) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = mEditor->document()->currentFloor();
        mCursorObject = new Window(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

StairsTool *StairsTool::mInstance = 0;

StairsTool *StairsTool::instance()
{
    if (!mInstance)
        mInstance = new StairsTool();
    return mInstance;
}

StairsTool::StairsTool() :
    BaseObjectTool(),
    mCursorObject(0)
{
}

void StairsTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    // Right-click to delete objects.
    if (event->button() == Qt::RightButton) {
        BaseObjectTool::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (mTileEdge == Center)
        return;

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    BuildingFloor *floor = mEditor->document()->currentFloor();
    Stairs *stairs = new Stairs(floor, x, y, dir);
    stairs->mTile = BuildingTiles::instance()->tileForStairs(stairs,
                                                             mEditor->building()->stairsTile());
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         stairs));
}

void StairsTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    BaseObjectTool::mouseMoveEvent(event);

    if (mTileEdge == Center) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BaseMapObject::Direction dir = BaseMapObject::N;

    if (mTileEdge == W)
        dir = BaseMapObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BaseMapObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = mEditor->document()->currentFloor();
        mCursorObject = new Stairs(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

SelectMoveObjectTool *SelectMoveObjectTool::mInstance = 0;

SelectMoveObjectTool *SelectMoveObjectTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveObjectTool;
    return mInstance;
}

SelectMoveObjectTool::SelectMoveObjectTool() :
    BaseTool(),
    mMode(NoMode),
    mMouseDown(false),
    mClickedObject(0),
    mSelectionRectItem(0)
{
}

void SelectMoveObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMouseDown = true;
        mStartScenePos = event->scenePos();
        mClickedObject = mEditor->topmostObjectAt(mStartScenePos);
    }
    if (event->button() == Qt::RightButton) {
        if (mMode == Moving)
            cancelMoving();
    }
}

void SelectMoveObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();

    if (mMode == NoMode && mMouseDown) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedObject)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
        mSelectionRectItem->setRect(QRectF(mStartScenePos, pos).normalized());
        break;
    case Moving:
        updateMovingItems(pos, event->modifiers());
        break;
    case CancelMoving:
        break;
    case NoMode:
        break;
    }
}

void SelectMoveObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedObject) {
            QSet<BaseMapObject*> selection = mEditor->document()->selectedObjects();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedObject))
                    selection.remove(mClickedObject);
                else
                    selection.insert(mClickedObject);
            } else {
                selection.clear();
                selection.insert(mClickedObject);
            }
            mEditor->document()->setSelectedObjects(selection);
        } else {
            mEditor->document()->setSelectedObjects(QSet<BaseMapObject*>());
        }
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
        mEditor->removeItem(mSelectionRectItem);
        mMode = NoMode;
        break;
    case Moving:
        mMouseDown = false;
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMouseDown = false;
    mClickedObject = 0;
}

void SelectMoveObjectTool::documentChanged()
{
    mSelectionRectItem = 0;
}

void SelectMoveObjectTool::activate()
{
}

void SelectMoveObjectTool::deactivate()
{
}

void SelectMoveObjectTool::updateSelection(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
    QRectF rect = QRectF(mStartScenePos, pos).normalized();

    // Make sure the rect has some contents, otherwise intersects returns false
    rect.setWidth(qMax(qreal(1), rect.width()));
    rect.setHeight(qMax(qreal(1), rect.height()));

    QSet<BaseMapObject*> selectedObjects;

    foreach (BaseMapObject *object, mEditor->objectsInRect(rect))
        selectedObjects.insert(object);

    const QSet<BaseMapObject*> oldSelection = mEditor->document()->selectedObjects();
    QSet<BaseMapObject*> newSelection;

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        newSelection = oldSelection | selectedObjects;
    } else {
        newSelection = selectedObjects;
    }

    mEditor->document()->setSelectedObjects(newSelection);
}

void SelectMoveObjectTool::startSelecting()
{
    mMode = Selecting;
    if (mSelectionRectItem == 0) {
        mSelectionRectItem = new QGraphicsRectItem();
        mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
        mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
        mSelectionRectItem->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mEditor->addItem(mSelectionRectItem);
}

void SelectMoveObjectTool::startMoving()
{
    mMovingObjects = mEditor->document()->selectedObjects();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingObjects.contains(mClickedObject)) {
        mMovingObjects.clear();
        mMovingObjects.insert(mClickedObject);
        mEditor->document()->setSelectedObjects(mMovingObjects);
    }

    mMode = Moving;
    mDragOffset = QPoint();
}

void SelectMoveObjectTool::updateMovingItems(const QPointF &pos,
                                             Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startTilePos = mEditor->sceneToTile(mStartScenePos);
    QPoint currentTilePos = mEditor->sceneToTile(pos);
    mDragOffset = currentTilePos - startTilePos;

    foreach (BaseMapObject *object, mMovingObjects) {
        mEditor->itemForObject(object)->setDragging(true);
        mEditor->itemForObject(object)->setDragOffset(mDragOffset);
    }
}

void SelectMoveObjectTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (BaseMapObject *object, mMovingObjects)
        mEditor->itemForObject(object)->setDragging(false);

    if (mDragOffset.isNull()) // Move is a no-op
        return;

    QUndoStack *undoStack = mEditor->document()->undoStack();
    undoStack->beginMacro(tr("Move %n Object(s)", "", mMovingObjects.size()));
    foreach (BaseMapObject *object, mMovingObjects) {
        undoStack->push(new MoveObject(mEditor->document(), object,
                                       object->pos() + mDragOffset));
    }
    undoStack->endMacro();

    mMovingObjects.clear();
}

void SelectMoveObjectTool::cancelMoving()
{
    foreach (BaseMapObject *object, mMovingObjects)
        mEditor->itemForObject(object)->setDragging(false);

    mMovingObjects.clear();

    mMode = CancelMoving;
}

/////

