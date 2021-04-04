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
#include "buildingorthoview.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "furnituregroups.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QStyleOptionGraphicsItem>
#include <QUndoStack>

using namespace BuildingEditor;

/////

BaseTool::BaseTool() :
    QObject(ToolManager::instance()),
    mEditor(nullptr),
    mHandCursor(HandNone)
{
    ToolManager::instance()->addTool(this);
}

void BaseTool::setEditor(BuildingBaseScene *editor)
{
    if (mEditor) {
        deactivate();
    }

    mEditor = editor;

    if (mEditor) {
        activate();
    }
}

void BaseTool::setAction(QAction *action)
{
    mAction = action;
    connect(mAction, SIGNAL(triggered()), SLOT(makeCurrent()));
}

void BaseTool::setEnabled(bool enabled)
{
    if (enabled != mAction->isEnabled()) {
        mAction->setEnabled(enabled);
        ToolManager::instance()->toolEnabledChanged(this, enabled);
    }
}

Qt::KeyboardModifiers BaseTool::keyboardModifiers() const
{
    return ToolManager::instance()->keyboardModifiers();
}

bool BaseTool::controlModifier() const
{
    return (keyboardModifiers() & Qt::ControlModifier) != 0;
}

bool BaseTool::shiftModifier() const
{
    return (keyboardModifiers() & Qt::ShiftModifier) != 0;
}

void BaseTool::setStatusText(const QString &text)
{
    mStatusText = text;
    emit statusTextChanged();
}

BuildingDocument *BaseTool::document() const
{
    return mEditor->document();
}

BuildingFloor *BaseTool::floor() const
{
    return mEditor->document()->currentFloor();
}

QString BaseTool::layerName() const
{
    return mEditor->currentLayerName();
}

QUndoStack *BaseTool::undoStack() const
{
    return mEditor->document()->undoStack();
}

bool BaseTool::isCurrent()
{
    return ToolManager::instance()->currentTool() == this;
}

void BaseTool::setHandCursor(HandCursor cursor)
{
    if (!mEditor || !mEditor->views().size() || cursor == mHandCursor)
        return;
    mHandCursor = cursor;
    if (mHandCursor == HandOpen)
        mEditor->views().at(0)->viewport()->setCursor(Qt::OpenHandCursor);
    else if (mHandCursor == HandClosed)
        mEditor->views().at(0)->viewport()->setCursor(Qt::ClosedHandCursor);
    else
        mEditor->views().at(0)->viewport()->unsetCursor();
}

void BaseTool::makeCurrent()
{
    ToolManager::instance()->activateTool(this);
}

void BaseTool::activate()
{
    connect(document(), &BuildingDocument::objectAboutToBeRemoved,
            this, &BaseTool::objectAboutToBeRemoved);
}

void BaseTool::deactivate()
{
    if (mHandCursor != HandNone) {
        if (mEditor->views().size())
            mEditor->views().at(0)->viewport()->unsetCursor();
        mHandCursor = HandNone;
    }
    document()->disconnect(this);
}

/////

ToolManager *ToolManager::mInstance = nullptr;

ToolManager *ToolManager::instance()
{
    if (!mInstance)
        mInstance = new ToolManager;
    return mInstance;
}

void ToolManager::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

ToolManager::ToolManager() :
    QObject(),
    mCurrentTool(nullptr),
    mCurrentEditor(nullptr)
{
}

void ToolManager::addTool(BaseTool *tool)
{
    mTools += tool;
}

void ToolManager::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->setEditor(nullptr);
        mCurrentTool->action()->setChecked(false);
        mCurrentTool->disconnect(this);
    }

    mCurrentTool = tool;

    if (mCurrentTool) {
        connect(mCurrentTool, &BaseTool::statusTextChanged,
                this, &ToolManager::currentToolStatusTextChanged);
        Q_ASSERT(mCurrentEditor != nullptr);
        mCurrentTool->setEditor(mCurrentEditor);
        mCurrentTool->action()->setChecked(true);
    }

//    qDebug() << "ToolManager::mCurrentTool" << mCurrentTool << "mCurrentEditor" << mCurrentEditor;

    emit currentToolChanged(mCurrentTool);
}

void ToolManager::toolEnabledChanged(BaseTool *tool, bool enabled)
{
    if (enabled && !mCurrentTool) {
        activateTool(tool);
    } else if (!enabled && tool == mCurrentTool) {
        foreach (BaseTool *tool2, mTools) {
            if (tool2 != tool && tool2->action()->isEnabled()) {
                activateTool(tool2);
                return;
            }
        }
        activateTool(nullptr);
    }
}

void ToolManager::checkKeyboardModifiers(Qt::KeyboardModifiers modifiers)
{
    if (modifiers == mCurrentModifiers)
        return;
    mCurrentModifiers = modifiers;
    if (mCurrentTool)
        mCurrentTool->currentModifiersChanged(modifiers);
}

void ToolManager::clearDocument()
{
    // Avoid a race condition when a document is closed.
    // When updateActions() calls setEnabled(false) on each tool one-by-one,
    // another tool is activated (see toolEnabledChanged()).
    // No tool should become active when a document is closing.
    activateTool(nullptr);
    foreach (BaseTool *tool, mTools)
        tool->action()->setEnabled(false);
    mCurrentEditor = nullptr;
}

void ToolManager::setEditor(BuildingBaseScene *editor)
{
    if (mCurrentEditor)
        clearDocument();

    mCurrentEditor = editor;

    if (mCurrentEditor) {

    }

    emit currentEditorChanged();
}

void ToolManager::currentToolStatusTextChanged()
{
    emit statusTextChanged(mCurrentTool);
}

/////

PencilTool *PencilTool::mInstance = nullptr;

PencilTool *PencilTool::instance()
{
    if (!mInstance)
        mInstance = new PencilTool();
    return mInstance;
}

PencilTool::PencilTool() :
    BaseTool(),
    mMouseDown(false),
    mErasing(false),
    mCursor(nullptr)
{
    updateStatusText();
}

void PencilTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint tilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());

    if (event->button() == Qt::RightButton) {
        // Right-click to cancel drawing/erasing.
        if (mMouseDown) {
            mMouseDown = false;
            mErasing = controlModifier();
            updateCursor(event->scenePos());
            updateStatusText();
            return;
        }
        if (!mEditor->currentFloorContains(tilePos))
            return;
        if (Room *room = floor()->GetRoomAt(tilePos)) {
            BuildingEditorWindow::instance()->setCurrentRoom(room);
            updateCursor(event->scenePos());
        }
        return;
    }

    mErasing = controlModifier();
    mStartTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
    mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds();
    mMouseDown = true;
    updateStatusText();
}

void PencilTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mMouseScenePos = event->scenePos();
    updateCursor(event->scenePos());
}

void PencilTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        QRect r = mCursorTileBounds;
        bool changed = false;
        QVector<QVector<Room*> > grid = floor()->grid();
        for (int x = r.left(); x <= r.right(); x++) {
            for (int y = r.top(); y <= r.bottom(); y++) {
                if (mErasing) {
                    if (grid[x][y] != nullptr) {
                        grid[x][y] = nullptr;
                        changed = true;
                    }
                } else {
                    if (grid[x][y] != BuildingEditorWindow::instance()->currentRoom()) {
                        grid[x][y] = BuildingEditorWindow::instance()->currentRoom();
                        changed = true;
                    }
                }
            }
        }
        if (changed)
            undoStack()->push(new SwapFloorGrid(mEditor->document(), floor(),
                                                grid, mErasing ? "Erase Rooms"
                                                               : "Draw Room"));
        mMouseDown = false;
        mErasing = controlModifier();
        updateCursor(event->scenePos());
        updateStatusText();
    }
}

void PencilTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!mMouseDown) {
        mErasing = controlModifier();
        updateCursor(mMouseScenePos);
    }
}

void PencilTool::activate()
{
    BaseTool::activate();
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void PencilTool::deactivate()
{
    BaseTool::deactivate();
    if (mCursor)
        mEditor->removeItem(mCursor);
}

void PencilTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos, mEditor->currentLevel());
    if (!mCursor) {
        mCursor = new QGraphicsPolygonItem;
        mCursor->setZValue(mEditor->ZVALUE_CURSOR);
    }

    QPolygonF polygon;
    if (mMouseDown) {
        mCursorTileBounds = QRect(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                                  qMin(mStartTilePos.y(), tilePos.y())),
                                  QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                                  qMax(mStartTilePos.y(), tilePos.y())));
        mCursorTileBounds &= floor()->bounds();
        polygon = mEditor->tileToScenePolygon(mCursorTileBounds, mEditor->currentLevel());
        updateStatusText();
    } else
        polygon = mEditor->tileToScenePolygon(tilePos, mEditor->currentLevel());

    mCursor->setPolygon(polygon);

    if (mErasing) {
        QPen pen(QColor(255,0,0,128));
        pen.setWidth(3);
        mCursor->setBrush(QColor(0,0,0,128));
        mCursor->setPen(pen);
    } else {
        mCursor->setPen(QColor(Qt::black));
        mCursor->setBrush(QColor(BuildingEditorWindow::instance()->currentRoom()->Color));
    }

    mCursor->setVisible(mMouseDown || mEditor->currentFloorContains(tilePos));

    // See NOTE-SCENE-CORRUPTION
    if (mCursor->boundingRect() != mCursorSceneRect) {
        // Don't call mCursor->update(), it isn't the same.
        mEditor->update(mCursorSceneRect);
        mCursorSceneRect = mCursor->boundingRect();
    }
}

void PencilTool::updateStatusText()
{
    if (mMouseDown)
        setStatusText(tr("Width,Height = %1,%2.  Right-click to cancel.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height()));
    else
        setStatusText(tr("Left-click to draw a room.  CTRL-Left-click to erase.  Right-click to switch to room under pointer."));
}

/////

SelectMoveRoomsTool *SelectMoveRoomsTool::mInstance = nullptr;

SelectMoveRoomsTool *SelectMoveRoomsTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveRoomsTool;
    return mInstance;
}

SelectMoveRoomsTool::SelectMoveRoomsTool() :
    BaseTool(),
    mMode(NoMode),
    mMouseDown(false),
    mMouseOverSelection(false),
    mCursorItem(nullptr)
{
    updateStatusText();
}

void SelectMoveRoomsTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMouseDown = true;
        mStartScenePos = event->scenePos();
        mStartTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
        if (mMouseOverSelection) {
            setHandCursor(HandClosed);
            startMoving();
        }
        updateStatusText();
    }
    if (event->button() == Qt::RightButton) {
        if (mMode == Moving)
            cancelMoving();
    }
}

void SelectMoveRoomsTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();

    updateCursor(pos);

    bool mouseOverSelection = selectedArea().contains(mEditor->sceneToTile(pos, mEditor->currentLevel()));
    if (mouseOverSelection != mMouseOverSelection) {
        mMouseOverSelection = mouseOverSelection;
        updateStatusText();
        if (mMode == NoMode)
            setHandCursor(mMouseOverSelection ? HandOpen : HandNone);
    }

    if (mMode == NoMode && mMouseDown) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            QPoint tilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
            if (selectedArea().contains(tilePos))
                startMoving();
            else
                startSelecting();
            updateStatusText();
        }
    }

    switch (mMode) {
    case Selecting: {
        QPoint tilePos = mEditor->sceneToTile(pos, mEditor->currentLevel());
        QRect tileBounds = QRect(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                                  qMin(mStartTilePos.y(), tilePos.y())),
                                  QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                                  qMax(mStartTilePos.y(), tilePos.y())));
        QRegion selection = QRegion(tileBounds);
        if (selection != selectedArea()) {
            document()->setRoomSelection(selection);
            updateStatusText();
        }
        break;
    }
    case Moving: {
        QPoint startTilePos = mStartTilePos;
        QPoint currentTilePos = mEditor->sceneToTile(pos, mEditor->currentLevel());
        QPoint offset = currentTilePos - startTilePos;
        if (offset != mDragOffset) {
            mDragOffset = offset;
            updateMovingItems();
        }
        break;
    }
    case CancelMoving:
        break;
    case NoMode:
        break;
    }
}

void SelectMoveRoomsTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode: // TODO: single-click to select adjoining tiles of a room
        if (!selectedArea().isEmpty() && !selectedArea().contains(mStartTilePos)) {
            document()->setRoomSelection(QRegion());
        }
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
//        mEditor->removeItem(mSelectionItem);
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
    updateCursor(event->scenePos());
    setHandCursor(mMouseOverSelection ? HandOpen : HandNone);
    updateStatusText();
}

void SelectMoveRoomsTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    if (mMode == Moving)
        updateMovingItems();
}

QRegion SelectMoveRoomsTool::selectedArea() const
{
    return document()->roomSelection();
}

void SelectMoveRoomsTool::updateStatusText()
{
    if (mMode == Moving) {
        setStatusText(tr("CTRL moves rooms on all floors.  SHIFT moves objects as well.  Right-click to cancel."));
    } else if (mMode == Selecting) {
        setStatusText(tr("Width,Height=%1,%2")
                      .arg(selectedArea().boundingRect().width())
                      .arg(selectedArea().boundingRect().height()));
    } else if (mMouseOverSelection) {
        setStatusText(tr("Left-click-drag selection to move rooms."));
    } else {
        setStatusText(tr("Left-click-drag to select."));
    }
}

void SelectMoveRoomsTool::activate()
{
    BaseTool::activate();
}

void SelectMoveRoomsTool::deactivate()
{
    if (mCursorItem) {
        delete mCursorItem;
        mCursorItem = nullptr;
    }
    mMouseOverSelection = false;
    BaseTool::deactivate();
}

void SelectMoveRoomsTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos, mEditor->currentLevel());
    if (/*!force && */(tilePos == mCursorTilePos))
        return;
    mCursorTilePos = tilePos;

    if (!mCursorItem) {
        mCursorItem = new BuildingRegionItem(mEditor);
        mCursorItem->setColor(QColor(0,0,255,64));
        mCursorItem->setZValue(mEditor->ZVALUE_CURSOR);
        mEditor->addItem(mCursorItem);
    }

    mCursorTileBounds = QRect(mCursorTilePos, QSize(1, 1));
    mCursorTileBounds &= floor()->bounds(1, 1);

    mCursorItem->setRegion(QRegion(mCursorTileBounds), mEditor->currentLevel());

    QPoint offset = (mMode == Moving) ? mDragOffset : QPoint();
    mCursorItem->setVisible((mMode != Moving) && !selectedArea().translated(offset)
                            .contains(mCursorTilePos));
}

void SelectMoveRoomsTool::updateSelection(const QPointF &pos,
                                          Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(pos)
    Q_UNUSED(modifiers)
}

void SelectMoveRoomsTool::startSelecting()
{
    mMode = Selecting;
    document()->setRoomSelection(QRegion(QRect(mStartTilePos, QSize(1,1))));
}

void SelectMoveRoomsTool::startMoving()
{
    mMode = Moving;
    mDragOffset = QPoint();

    foreach (BuildingFloor *floor, mEditor->building()->floors()) {
        GraphicsFloorItem *item = mEditor->itemForFloor(floor);
        QImage *bmp = new QImage(item->bmp()->copy());
        item->setDragBmp(bmp);
    }
}

void SelectMoveRoomsTool::updateMovingItems()
{
    bool allFloors = controlModifier();
    bool objectsToo = shiftModifier();

    for (BuildingFloor *floor : mEditor->building()->floors()) {
        GraphicsFloorItem *floorItem = mEditor->itemForFloor(floor);
        QImage *bmp = floorItem->bmp();
        QImage *dragBmp = floorItem->dragBmp();

        QVector<QVector<Room*> > dragGrid = floor->grid();
        QMap<QString,FloorTileGrid*> dragTiles = floor->grimeClone();

        *dragBmp = *bmp;

        bool moveThisFloor = (floor == this->floor()) || allFloors;
        if (moveThisFloor) {

            // Erase the area being moved.
            QRect floorBounds = floor->bounds();
            for (const QRect &r1 : selectedArea()) {
                QRect r = r1 & floorBounds;
                for (int x = r.left(); x <= r.right(); x++)
                    for (int y = r.top(); y <= r.bottom(); y++) {
                        dragBmp->setPixel(x, y, qRgb(0,0,0));
                        dragGrid[x][y] = nullptr;
                        for (const QString &layerName : dragTiles.keys())
                            dragTiles[layerName]->replace(x, y, BuildingCell());
                    }
            }

            // Copy the moved area to its new location.
            for (const QRect &src1 : selectedArea()) {
                QRect src = src1 & floorBounds;
                for (int x = src.left(); x <= src.right(); x++) {
                    for (int y = src.top(); y <= src.bottom(); y++) {
                        QPoint p = QPoint(x, y) + mDragOffset;
                        if (floorBounds.contains(p)) {
                            dragBmp->setPixel(p, bmp->pixel(x, y));
                            dragGrid[p.x()][p.y()] = floor->GetRoomAt(x, y);
                            for (const QString &layerName : dragTiles.keys())
                                dragTiles[layerName]->replace(p.x(), p.y(), floor->grimeAt(layerName, x, y));
                        }
                    }
                }
            }

            mEditor->changeFloorGrid(floor, dragGrid);
            mEditor->changeUserTiles(floor, dragTiles);
        }

        floorItem->update();

        // Update objects
        foreach (BuildingObject *object, floor->objects()) {
            GraphicsObjectItem *objectItem = floorItem->itemForObject(object);
            bool moveThisObject = moveThisFloor && objectsToo &&
                    selectedArea().intersects(object->bounds());
            objectItem->setDragOffset(mDragOffset);
            objectItem->setDragging(moveThisObject);
            mEditor->dragObject(floor, object, moveThisObject ? mDragOffset : QPoint());
        }
    }

    mEditor->roomSelectionItem()->setDragOffset(mDragOffset);
}

void SelectMoveRoomsTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    bool allFloors = controlModifier();
    bool objectsToo = shiftModifier();

    foreach (BuildingFloor *floor, mEditor->building()->floors()) {
        GraphicsFloorItem *item = mEditor->itemForFloor(floor);
        delete item->dragBmp();
        item->setDragBmp(nullptr);
        mEditor->resetFloorGrid(floor);
        mEditor->resetUserTiles(floor);
        foreach (BuildingObject *object, floor->objects()) {
            item->itemForObject(object)->setDragging(false);
            mEditor->resetDrag(floor, object);
        }
    }

    if (mDragOffset.isNull()) // Move is a no-op
        return;

    undoStack()->beginMacro(tr(objectsToo ? "Move Rooms and Objects"
                                          : "Move Rooms"));

    if (allFloors) {
        foreach (BuildingFloor *floor, mEditor->building()->floors())
            finishMovingFloor(floor, objectsToo);
    } else {
        finishMovingFloor(floor(), objectsToo);
    }

    // Final position of the selection.
    mEditor->roomSelectionItem()->setDragOffset(QPoint());
    undoStack()->push(new ChangeRoomSelection(document(),
                                              selectedArea().translated(mDragOffset)));

    undoStack()->endMacro();
}

void SelectMoveRoomsTool::cancelMoving()
{
    foreach (BuildingFloor *floor, mEditor->building()->floors()) {
        GraphicsFloorItem *item = mEditor->itemForFloor(floor);
        delete item->dragBmp();
        item->setDragBmp(nullptr);
        mEditor->resetFloorGrid(floor);
        mEditor->resetUserTiles(floor);
        foreach (BuildingObject *object, floor->objects()) {
            item->itemForObject(object)->setDragging(false);
            mEditor->resetDrag(floor, object);
        }
    }

    mEditor->roomSelectionItem()->setDragOffset(QPoint());

    mMode = CancelMoving;
}

void SelectMoveRoomsTool::finishMovingFloor(BuildingFloor *floor, bool objectsToo)
{
    // Move the rooms
    QVector<QVector<Room*> > grid = floor->grid();

    QRect floorBounds = floor->bounds();
    for (const QRect& src1 : selectedArea()) {
        QRect src = src1 & floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                grid[x][y] = nullptr;
            }
        }
    }

    for (const QRect& src1 : selectedArea()) {
        QRect src = src1 & floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                QPoint p = QPoint(x, y) + mDragOffset;
                if (floorBounds.contains(p))
                    grid[p.x()][p.y()] = floor->grid()[x][y];
            }
        }
    }

    undoStack()->push(new SwapFloorGrid(mEditor->document(), floor, grid,
                                        "Move Rooms"));

    // Move the user-placed tiles
    QMap<QString,FloorTileGrid*> grime = floor->grimeClone();

    floorBounds = floor->bounds(1, 1);
    for (const QRect& src1 : selectedArea()) {
        QRect src = src1 & floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                for (FloorTileGrid *stg : grime.values())
                    stg->replace(x, y, BuildingCell());
            }
        }
    }

    for (const QRect& src1 : selectedArea()) {
        QRect src = src1 & floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                QPoint p = QPoint(x, y) + mDragOffset;
                if (floorBounds.contains(p)) {
                    for (const QString &key : grime.keys())
                        grime[key]->replace(p.x(), p.y(), floor->grimeAt(key, x, y));
                }
            }
        }
    }

    undoStack()->push(new SwapFloorGrime(mEditor->document(), floor, grime,
                                        "Move Tiles", true));

    if (objectsToo) {
        QList<BuildingObject*> objects = floor->objects();
        foreach (BuildingObject *object, objects) {
            if (selectedArea().intersects(object->bounds())) {
                if (object->isValidPos(mDragOffset))
                    undoStack()->push(new MoveObject(mEditor->document(),
                                                     object,
                                                     object->pos() + mDragOffset));
                else
                    undoStack()->push(new RemoveObject(mEditor->document(),
                                                       object->floor(),
                                                       object->index()));
            }
        }
    }
}

/////

BaseObjectTool::BaseObjectTool() :
    BaseTool(),
    mTileEdge(TileEdge::Center),
    mTileCenters(false),
    mCursorObject(nullptr),
    mCursorItem(nullptr),
    mEyedrop(false),
    mMouseDown(false),
    mRightClicked(false),
    mPlaceOnRelease(false),
    mMouseOverObject(false)
{
}

void BaseObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (mMouseDown) {
            mRightClicked = true;
            return;
        }
        if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
            undoStack()->push(new RemoveObject(mEditor->document(), floor(),
                                               object->index()));
        }
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (mEyedrop) {
        if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
            eyedrop(object);
        }
        return;
    }

    mMouseDown = true;

    if (!mCursorItem || !mCursorItem->isVisible() || !mCursorItem->isValidPos())
        return;

    if (mPlaceOnRelease) {
        mEditor->setHighlightRoomLock(true);
        updateStatusText();
    } else
        placeObject();
}

void BaseObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown)
        return;

    if (mEyedrop) {
        BuildingObject *object = mEditor->topmostObjectAt(event->scenePos());
        if (object && (object->asRoof() /*|| object->asWall()*/))
            object = nullptr;
        bool mouseOverObject = object != nullptr;
        if (mouseOverObject != mMouseOverObject)
            mMouseOverObject = mouseOverObject;
        mEditor->setMouseOverObject(object);
    }

    QPoint oldTilePos = mTilePos;
    TileEdge oldEdge = mTileEdge;

    mTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());

    QPointF p = mEditor->sceneToTileF(event->scenePos(), mEditor->currentLevel());
    QPointF m(p.x() - int(p.x()), p.y() - int(p.y()));
    if (mTileCenters) {
        TileEdge xEdge = TileEdge::Center, yEdge = TileEdge::Center;
        if (m.x() < 0.25)
            xEdge = TileEdge::W;
        else if (m.x() >= 0.75)
            xEdge = TileEdge::E;
        if (m.y() < 0.25)
            yEdge = TileEdge::N;
        else if (m.y() >= 0.75)
            yEdge = TileEdge::S;
        if ((xEdge == TileEdge::Center && yEdge == TileEdge::Center) || (xEdge != TileEdge::Center && yEdge != TileEdge::Center))
            mTileEdge = TileEdge::Center;
        else if (xEdge != TileEdge::Center)
            mTileEdge = xEdge;
        else
            mTileEdge = yEdge;
    } else {
        qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
        if (dW < dE) {
            mTileEdge = (dW < dN && dW < dS) ? TileEdge::W : ((dN < dS) ? TileEdge::N : TileEdge::S);
        } else {
            mTileEdge = (dE < dN && dE < dS) ? TileEdge::E : ((dN < dS) ? TileEdge::N : TileEdge::S);
        }
    }

    if (mTilePos == oldTilePos && mTileEdge == oldEdge)
        return;

    updateCursorObject();
}

void BaseObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)

    if (event->button() == Qt::LeftButton) {
        if (mPlaceOnRelease)
            mEditor->setHighlightRoomLock(false);
        if (!mRightClicked && mMouseDown && mPlaceOnRelease)
            placeObject();
        mMouseDown = false;
        mRightClicked = false;
        updateStatusText();
    }
}


void BaseObjectTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    bool eyedrop = (modifiers & Qt::AltModifier) != 0;
    if (eyedrop != mEyedrop) {
        mEyedrop = eyedrop;
        if (mEyedrop) {
            mEditor->views().at(0)->viewport()->setCursor(Qt::PointingHandCursor);
        } else {
            mEditor->views().at(0)->viewport()->unsetCursor();
            mEditor->setMouseOverObject(nullptr);
        }
    }
    updateCursorObject();
}

void BaseObjectTool::activate()
{
    BaseTool::activate();
    currentModifiersChanged(qApp->keyboardModifiers());
}

void BaseObjectTool::deactivate()
{
    BaseTool::deactivate();
    if (mEyedrop) {
        mEditor->views().at(0)->viewport()->unsetCursor();
        mEditor->setMouseOverObject(nullptr);
        mEyedrop = false;
    }
    if (mCursorItem) {
        mEditor->removeItem(mCursorItem);
        delete mCursorItem;
        mCursorItem = nullptr;
        mEditor->setCursorObject(nullptr);
    }
}

void BaseObjectTool::setCursorObject(BuildingObject *object)
{
    if (object) {
        mCursorObject = object;
        if (!mCursorItem) {
            mCursorItem = new GraphicsObjectItem(mEditor, mCursorObject);
            mCursorItem->synchWithObject();
            mCursorItem->setZValue(mEditor->ZVALUE_CURSOR);
            mEditor->addItem(mCursorItem);
        }
        mCursorItem->setObject(mCursorObject);
    } else {
        // Note: not setting mCursorObject=0 here!
        if (mCursorItem)
            mCursorItem->setVisible(false);
    }

    // See NOTE-SCENE-CORRUPTION
    if (mCursorItem && mCursorItem->boundingRect() != mCursorSceneRect) {
        // Don't call mCursor->update(), it isn't the same.
        mEditor->update(mCursorSceneRect);
        mCursorSceneRect = mCursorItem->boundingRect();
    }

    mEditor->setCursorObject(object);
}

void BaseObjectTool::eyedrop(BuildingObject *object)
{
    emit objectPicked(object);
    updateCursorObject();

    mEditor->document()->setSelectedObjects(QSet<BuildingObject*>());

    if (FurnitureObject *fobj = object->asFurniture())
        FurnitureTool::instance()->setCurrentTile(fobj->furnitureTile());

    BaseTool *tool = nullptr;
    if (object->asDoor()) tool = DoorTool::instance();
    if (object->asFurniture()) tool = FurnitureTool::instance();
//    if (object->asRoof()) tool = RoofTool::instance();
    if (object->asStairs()) tool = StairsTool::instance();
    if (object->asWall()) tool = WallTool::instance();
    if (object->asWindow()) tool = WindowTool::instance();
    if (tool && !tool->isCurrent() && tool->action()->isEnabled())
        QMetaObject::invokeMethod(tool, "makeCurrent", Qt::QueuedConnection);

    mEditor->document()->emitObjectPicked(object);
}

/////

DoorTool *DoorTool::mInstance = nullptr;

DoorTool *DoorTool::instance()
{
    if (!mInstance)
        mInstance = new DoorTool();
    return mInstance;
}

DoorTool::DoorTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place a door.  Right-click to remove any object.  ALT = eyedrop."));
}

void DoorTool::placeObject()
{
    BuildingFloor *floor = this->floor();
    Door *door = new Door(floor, mCursorObject->x(), mCursorObject->y(),
                          mCursorObject->dir());
    door->setTile(mEditor->building()->doorTile());
    door->setTile(mEditor->building()->doorFrameTile(), 1);
    undoStack()->push(new AddObject(mEditor->document(), floor,
                                    floor->objectCount(), door));
}

void DoorTool::updateCursorObject()
{
    if (mEyedrop || mTileEdge == TileEdge::Center || !mEditor->currentFloorContains(mTilePos)) {
        setCursorObject(nullptr);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::Direction::N;

    if (mTileEdge == TileEdge::W) {
        dir = BuildingObject::Direction::W;
    } else if (mTileEdge == TileEdge::E) {
        dir = BuildingObject::Direction::E;
    } else if (mTileEdge == TileEdge::S) {
        dir = BuildingObject::Direction::S;
    }

    if (!mCursorObject) {
        BuildingFloor *floor = nullptr; //floor();
        mCursorObject = new Door(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);
    mCursorObject->setTile(mEditor->building()->doorTile());
    mCursorObject->setTile(mEditor->building()->doorFrameTile(), 1);

    setCursorObject(mCursorObject);
}

/////

WindowTool *WindowTool::mInstance = nullptr;

WindowTool *WindowTool::instance()
{
    if (!mInstance)
        mInstance = new WindowTool();
    return mInstance;
}

WindowTool::WindowTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place a window.  Right-click to remove any object.  ALT = eyedrop."));
}

void WindowTool::placeObject()
{
    BuildingFloor *floor = this->floor();
    Window *window = new Window(floor, mCursorObject->x(), mCursorObject->y(),
                                mCursorObject->dir());
    window->setTile(mEditor->building()->windowTile());
    window->setTile(mEditor->building()->curtainsTile(), Window::TileCurtains);
    window->setTile(mEditor->building()->tile(Building::Shutters), Window::TileShutters);
    undoStack()->push(new AddObject(mEditor->document(), floor,
                                    floor->objectCount(), window));
}

void WindowTool::updateCursorObject()
{
    if (mEyedrop || mTileEdge == TileEdge::Center || !mEditor->currentFloorContains(mTilePos)) {
        setCursorObject(nullptr);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::Direction::N;

    if (mTileEdge == TileEdge::W) {
        dir = BuildingObject::Direction::W;
    } else if (mTileEdge == TileEdge::E) {
        dir = BuildingObject::Direction::E;
    } else if (mTileEdge == TileEdge::S) {
        dir = BuildingObject::Direction::S;
    }

    if (!mCursorObject) {
        BuildingFloor *floor = nullptr; //floor();
        mCursorObject = new Window(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);
    mCursorObject->setTile(mEditor->building()->windowTile());
    mCursorObject->setTile(mEditor->building()->curtainsTile(), Window::TileCurtains);
    mCursorObject->setTile(mEditor->building()->tile(Building::Shutters), Window::TileShutters);

    setCursorObject(mCursorObject);
}

/////

StairsTool *StairsTool::mInstance = nullptr;

StairsTool *StairsTool::instance()
{
    if (!mInstance)
        mInstance = new StairsTool();
    return mInstance;
}

StairsTool::StairsTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place stairs.  Right-click to remove any object.  ALT = eyedrop."));
}

void StairsTool::placeObject()
{
    BuildingFloor *floor = this->floor();
    Stairs *stairs = new Stairs(floor, mCursorObject->x(), mCursorObject->y(),
                                mCursorObject->dir());
    stairs->setTile(mEditor->building()->stairsTile());
    undoStack()->push(new AddObject(mEditor->document(), floor,
                                    floor->objectCount(), stairs));
}

void StairsTool::updateCursorObject()
{
    if (mEyedrop || mTileEdge == TileEdge::Center || !mEditor->currentFloorContains(mTilePos)) {
        setCursorObject(nullptr);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::Direction::N;

    if (mTileEdge == TileEdge::E || mTileEdge == TileEdge::N)
        dir = BuildingObject::Direction::W;

    if (!mCursorObject) {
        BuildingFloor *floor = nullptr; //floor();
        mCursorObject = new Stairs(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);
    mCursorObject->setTile(mEditor->building()->stairsTile());

    setCursorObject(mCursorObject);
}

/////

FurnitureTool *FurnitureTool::mInstance = nullptr;

FurnitureTool *FurnitureTool::instance()
{
    if (!mInstance)
        mInstance = new FurnitureTool();
    return mInstance;
}

FurnitureTool::FurnitureTool() :
    BaseObjectTool(),
    mCurrentTile(nullptr)
{
    mPlaceOnRelease = true;
    updateStatusText();
}

void FurnitureTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown && mCursorObject) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
        if (tilePos != mTilePos) {
            QPointF tilePosF = mEditor->sceneToTileF(event->scenePos(), mEditor->currentLevel());
            QLineF line(QPointF(mTilePos) + QPointF(0.5, 0.5), tilePosF);
            FurnitureTile::FurnitureOrientation orient = FurnitureTile::FurnitureE;
            if (mCurrentTile->owner()->hasCorners()) {
                qreal angle = line.angle() + 45.0 / 2;
                if (angle > 360) angle -= 360.0;
                int sector = angle / 45.0;
                if (sector == 0) orient = FurnitureTile::FurnitureE;
                else if (sector == 1) orient = FurnitureTile::FurnitureNE;
                else if (sector == 2) orient = FurnitureTile::FurnitureN;
                else if (sector == 3) orient = FurnitureTile::FurnitureNW;
                else if (sector == 4) orient = FurnitureTile::FurnitureW;
                else if (sector == 5) orient = FurnitureTile::FurnitureSW;
                else if (sector == 6) orient = FurnitureTile::FurnitureS;
                else if (sector == 7) orient = FurnitureTile::FurnitureSE;
            } else {
                qreal angle = line.angle() + 90.0 / 2;
                if (angle > 360) angle -= 360.0;
                int sector = angle / 90.0;
                if (sector == 0) orient = FurnitureTile::FurnitureE;
                else if (sector == 1) orient = FurnitureTile::FurnitureN;
                else if (sector == 2) orient = FurnitureTile::FurnitureW;
                else if (sector == 3) orient = FurnitureTile::FurnitureS;
            }

            FurnitureTiles *ftiles = mCurrentTile->owner();
            mCursorObject->asFurniture()->setFurnitureTile(ftiles->tile(orient));
            setCursorObject(mCursorObject);
        }
        return;
    }

    BaseObjectTool::mouseMoveEvent(event);
}

void FurnitureTool::placeObject()
{
    BuildingFloor *floor = this->floor();
    FurnitureObject *object = new FurnitureObject(floor,
                                                  mCursorObject->x(),
                                                  mCursorObject->y());
    object->setFurnitureTile(mCursorObject->asFurniture()->furnitureTile());
    object->setDir(mCursorObject->dir());
    object->setVersion(3);
    undoStack()->push(new AddObject(mEditor->document(), floor,
                                    floor->objectCount(), object));
}

void FurnitureTool::updateCursorObject()
{
    if (mEyedrop || !mEditor->currentFloorContains(mTilePos)) {
        setCursorObject(nullptr);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    if (!mCursorObject) {
        BuildingFloor *floor = nullptr; //floor();
        FurnitureObject *object = new FurnitureObject(floor, x, y);
        object->setVersion(3);
        mCursorObject = object;
    }

    FurnitureTiles *ftiles = mCurrentTile->owner();
    FurnitureTile::FurnitureOrientation orient = mCurrentTile->orient();
    if (!controlModifier()) {
        Orient wallOrient = calcOrient(x, y);
        switch (wallOrient) {
        case OrientNone: break;
        case OrientNW: orient = ftiles->hasCorners()
                    ? FurnitureTile::FurnitureNW
                    : ((mTileEdge == TileEdge::W || mTileEdge == TileEdge::S)
                       ? FurnitureTile::FurnitureW : FurnitureTile::FurnitureN);
            break;
        case OrientNE: orient = ftiles->hasCorners()
                    ? FurnitureTile::FurnitureNE
                    : ((mTileEdge == TileEdge::E || mTileEdge == TileEdge::S)
                       ? FurnitureTile::FurnitureE : FurnitureTile::FurnitureN);
            break;
        case OrientSW: orient = ftiles->hasCorners()
                    ? FurnitureTile::FurnitureSW
                    : ((mTileEdge == TileEdge::W || mTileEdge == TileEdge::N)
                       ? FurnitureTile::FurnitureW : FurnitureTile::FurnitureS);
            break;
        case OrientSE: orient = ftiles->hasCorners()
                    ? FurnitureTile::FurnitureSE
                    : ((mTileEdge == TileEdge::E || mTileEdge == TileEdge::N)
                       ? FurnitureTile::FurnitureE : FurnitureTile::FurnitureS);
            break;
        case OrientW: orient = FurnitureTile::FurnitureW; break;
        case OrientN: orient = FurnitureTile::FurnitureN; break;
        case OrientE: orient = FurnitureTile::FurnitureE; break;
        case OrientS: orient = FurnitureTile::FurnitureS; break;
        }

        // If the furniture is larger than one tile adjust the position to
        // keep it aligned with the wall under the mouse pointer.
        QSize size = ftiles->tile(orient)->resolved()->size();
        if (size.width() > 1) {
            switch (wallOrient) {
            case OrientE:
            case OrientNE:
            case OrientSE:
                x -= size.width() - 1;
                break;
            default:
                break;
            }
        }
        if (size.height() > 1) {
            switch (wallOrient) {
            case OrientS:
            case OrientSE:
            case OrientSW:
                y -= size.height() - 1;
                break;
            default:
                break;
            }
        }
    }
    mCursorObject->asFurniture()->setFurnitureTile(ftiles->tile(orient));

    mCursorObject->setPos(x, y);

    if (ftiles->layer() == FurnitureTiles::LayerWalls) {
        switch (mTileEdge) {
        case TileEdge::N:
            mCursorObject->setDir(BuildingObject::Direction::N);
            break;
        case TileEdge::S:
            mCursorObject->setDir(BuildingObject::Direction::S);
            break;
        case TileEdge::W:
            mCursorObject->setDir(BuildingObject::Direction::W);
            break;
        case TileEdge::E:
            mCursorObject->setDir(BuildingObject::Direction::E);
            break;
        default:
            break;
        }
    }

    setCursorObject(mCursorObject);
#if 0
    QRegion rgn;
    if (FloorTileGrid *tiles = ftiles->tile(orient)->resolved()->toFloorTileGrid(rgn)) {
        QString layerName;
        switch (ftiles->layer()) {
        case FurnitureTiles::LayerWalls: layerName = QLatin1String("Walls"); break;
        case FurnitureTiles::LayerRoofCap: layerName = QLatin1String("RoofCap"); break;
        case FurnitureTiles::LayerWallOverlay: layerName = QLatin1String("WallOverlay"); break;
        case FurnitureTiles::LayerWallFurniture: layerName = QLatin1String("WallFurniture"); break;
        case FurnitureTiles::LayerFrames: layerName = QLatin1String("Frames"); break;
        case FurnitureTiles::LayerDoors: layerName = QLatin1String("Doors"); break;
        case FurnitureTiles::LayerFurniture: layerName = QLatin1String("Furniture"); break;
        case FurnitureTiles::LayerRoof: layerName = QLatin1String("Roof"); break;
        }
        mEditor->setToolTiles(tiles, mCursorObject->pos(), layerName);
    }
#endif
}

void FurnitureTool::eyedrop(BuildingObject *object)
{
    BaseObjectTool::eyedrop(object);
    if (FurnitureObject *fobj = object->asFurniture()) {
        mCurrentTile = fobj->furnitureTile();
    }
}

void FurnitureTool::setCurrentTile(FurnitureTile *tile)
{
    mCurrentTile = tile;
    if (mCursorObject)
        mCursorObject->asFurniture()->setFurnitureTile(tile);
}

static FurnitureTool::Orient wallOrient(const BuildingSquare &square)
{
    if (square.HasWallN() && square.HasWallW()) {
        return FurnitureTool::OrientNW;
    }
    if (square.HasWallN()) {
        return FurnitureTool::OrientN;
    }
    if (square.HasWallW()) {
        return FurnitureTool::OrientW;
    }
#if 0
    if ((square.mEntries[BuildingFloor::Square::SectionWall] &&
         !square.mEntries[BuildingFloor::Square::SectionWall]->isNone()) ||
            square.mTiles[BuildingFloor::Square::SectionWall])
        switch (square.mWallOrientation) {
        case BuildingFloor::Square::WallOrientInvalid:
            break;
        case BuildingFloor::Square::WallOrientW:
            return FurnitureTool::OrientW;
        case BuildingFloor::Square::WallOrientN:
            return FurnitureTool::OrientN;
        case BuildingFloor::Square::WallOrientNW:
            return FurnitureTool::OrientNW;
        case BuildingFloor::Square::WallOrientSE:
            return FurnitureTool::OrientSE;
        }
#endif
    return FurnitureTool::OrientNone;
}

FurnitureTool::Orient FurnitureTool::calcOrient(int x, int y)
{
    BuildingFloor *floor = this->floor();
    Orient orient[9];
    orient[OrientNW] = (x && y) ? wallOrient(floor->squares[x-1][y-1]) : OrientNone;
    orient[OrientN] = y ? wallOrient(floor->squares[x][y-1]) : OrientNone;
    orient[OrientNE] = y ? wallOrient(floor->squares[x+1][y-1]) : OrientNone;
    orient[OrientW] = x ? wallOrient(floor->squares[x-1][y]) : OrientNone;
    orient[OrientNone] = wallOrient(floor->squares[x][y]);
    orient[OrientE] = wallOrient(floor->squares[x+1][y]);
    orient[OrientSW] = x ? wallOrient(floor->squares[x-1][y+1]) : OrientNone;
    orient[OrientS] = wallOrient(floor->squares[x][y+1]);
    orient[OrientSE] = wallOrient(floor->squares[x+1][y+1]);

    if (orient[OrientNone] == OrientNW) return OrientNW;
    if (orient[OrientSE] == OrientSE) return OrientSE;
    if (orient[OrientNone] == OrientN && (orient[OrientE] == OrientW || orient[OrientE] == OrientNW)) return OrientNE;
    if (orient[OrientNone] == OrientN) return OrientN;
    if (orient[OrientNone] == OrientW && (orient[OrientS] == OrientN || orient[OrientS] == OrientNW)) return OrientSW;
    if (orient[OrientNone] == OrientW) return OrientW;

//    if (orient[OrientN] == OrientN && orient[OrientE] == OrientW) return OrientNE;
//    if (orient[OrientS] == OrientN && orient[OrientE] == OrientW) return OrientSE;

    if ((orient[OrientS] == OrientN || orient[OrientS] == OrientNW)
            && (orient[OrientE] == OrientW || orient[OrientE] == OrientNW))
        return OrientSE;
    if (orient[OrientE] == OrientW || orient[OrientE] == OrientNW) return OrientE;
    if (orient[OrientS] == OrientN || orient[OrientS] == OrientNW) return OrientS;

    return OrientNone;
}

void FurnitureTool::updateStatusText()
{
    if (mMouseDown)
        setStatusText(tr("Drag to change orientation. Right-click to cancel."));
    else
        setStatusText(tr("Left-click to place furniture. Right-click to remove any object. CTRL disables auto-align. ALT = eyedrop."));
}

/////

RoofTool *RoofTool::mInstance = nullptr;

RoofTool *RoofTool::instance()
{
    if (!mInstance)
        mInstance = new RoofTool;
    return mInstance;
}

RoofTool::RoofTool() :
    BaseTool(),
    mRoofType(RoofObject::PeakNS),
    mMode(NoMode),
    mObject(nullptr),
    mItem(nullptr),
    mCursorItem(nullptr),
    mObjectItem(nullptr),
    mHandleObject(nullptr),
    mHandleItem(nullptr),
    mMouseOverHandle(false)
{
}

void RoofTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode)
            return; // ignore clicks when creating/resizing
        mStartTilePosF = mEditor->sceneToTileF(event->scenePos(), mEditor->currentLevel());
        mCurrentTilePosF = mStartTilePosF;
        mStartPos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
        mCurrentPos = mStartPos;
        if (mMouseOverHandle) {
            if (mHandleItem == mObjectItem->depthUpHandle()) {
                depthUp();
                return;
            }
            if (mHandleItem == mObjectItem->depthDownHandle()) {
                depthDown();
                return;
            }
            if (mHandleItem == mObjectItem->cappedWHandle()) {
                toggleCappedW();
                return;
            }
            if (mHandleItem == mObjectItem->cappedNHandle()) {
                toggleCappedN();
                return;
            }
            if (mHandleItem == mObjectItem->cappedEHandle()) {
                toggleCappedE();
                return;
            }
            if (mHandleItem == mObjectItem->cappedSHandle()) {
                toggleCappedS();
                return;
            }
            mOriginalWidth = mHandleObject->width();
            mOriginalHeight = mHandleObject->height();
            mOriginalHalfDepth = mHandleObject->isHalfDepth();
            mMode = Resize;
            updateStatusText();

            mEditor->setCursorObject(mHandleObject);
            return;
        }
        if (!mEditor->currentFloorContains(mCurrentPos))
            return;
        mObject = new RoofObject(floor(),
                                 mStartPos.x(), mStartPos.y(),
                                 /*width=*/1, /*height=*/1,
                                 /*type=*/mRoofType, /*depth=*/RoofObject::InvalidDepth,
                                 /*cappedW=*/true, /*cappedN=*/true,
                                 /*cappedE=*/true, /*cappedS=*/true);
        mItem = new GraphicsObjectItem(mEditor, mObject);
        mItem->synchWithObject();
        mItem->setZValue(mEditor->ZVALUE_CURSOR);
        mEditor->addItem(mItem);
        mMode = Create;
        updateStatusText();

        mObject->setCapTiles(mEditor->building()->roofCapTile());
        mObject->setSlopeTiles(mEditor->building()->roofSlopeTile());
        mObject->setTopTiles(mEditor->building()->roofTopTile());
        mObject->setDefaultCaps();
        mEditor->setCursorObject(mObject);
    }

    if (event->button() == Qt::RightButton) {
        if (mMode == Resize) {
            mHandleObject->resize(mOriginalWidth, mOriginalHeight, mOriginalHalfDepth);
            mObjectItem->synchWithObject();
            mMode = NoMode;
            updateStatusText();
            mEditor->setCursorObject(nullptr);
            return;
        }
        if (mMode == Create) {
            delete mObject;
            mObject = nullptr;
            delete mItem;
            mItem = nullptr;
            mMode = NoMode;
            updateStatusText();
            mEditor->setCursorObject(nullptr);
            return;
        }
        if (mMode != NoMode)
            return; // ignore clicks when creating/resizing
        if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
            undoStack()->push(new RemoveObject(mEditor->document(), floor(),
                                               object->index()));
        }
    }
}

void RoofTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mCurrentTilePosF = mEditor->sceneToTileF(event->scenePos(), mEditor->currentLevel());
    mCurrentPos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());

    if (mMode == NoMode) {
        if (!mCursorItem) {
            mCursorItem = new QGraphicsPolygonItem;
            mCursorItem->setZValue(mEditor->ZVALUE_CURSOR);
            mCursorItem->setBrush(QColor(0,255,0,128));
            mEditor->addItem(mCursorItem);
        }

        mCursorItem->setPolygon(mEditor->tileToScenePolygon(mCurrentPos, mEditor->currentLevel()));

        updateHandle(event->scenePos());

        mCursorItem->setVisible(mEditor->currentFloorContains(mCurrentPos) &&
                                !mMouseOverHandle);

        // See NOTE-SCENE-CORRUPTION
        if (mCursorItem->boundingRect() != mCursorSceneRect) {
            // Don't call mCursor->update(), it isn't the same.
            mEditor->update(mCursorSceneRect);
            mCursorSceneRect = mCursorItem->boundingRect();
        }

        return;
    }

    QPoint diff = mCurrentPos - mStartPos;

    if (mMode == Resize) {
        if (mCurrentPos.x() < mHandleObject->x())
            diff.setX(mHandleObject->x() - mStartPos.x() + (mHandleObject->isN() ? 1 : 0));
        if (mCurrentPos.y() < mHandleObject->y())
            diff.setY(mHandleObject->y() - mStartPos.y() + (mHandleObject->isW() ? 1 : 0));
        if (mCurrentPos.x() >= floor()->width())
            diff.setX(floor()->width() - mStartPos.x() - 1);
        if (mCurrentPos.y() >= floor()->height())
            diff.setY(floor()->height() - mStartPos.y() - 1);

#if 1
        // Fugly code to determine if a corner should be half-depth.
        bool halfDepth = false;
        if (mHandleObject->roofType() == RoofObject::CornerOuterSE
                || mHandleObject->roofType() == RoofObject::CornerInnerNW) {
            QRect bounds = mHandleObject->bounds().adjusted(0, 0, diff.x(), diff.y());
            if (bounds.width() > 3) bounds.setWidth(3);
            if (bounds.height() > 3) bounds.setHeight(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = d.x() < 0.5 && d.y() < 0.5;
            qreal angle = QLineF(mHandleObject->bounds().topLeft(), mCurrentTilePosF).angle();
            if (angle >= 315)
                halfDepth = bounds.contains(mCurrentPos) && d.x() < 0.5;
            else if (angle >= 270)
                halfDepth = bounds.contains(mCurrentPos) && d.y() < 0.5;
            else
                halfDepth = true;
        }
        if (mHandleObject->roofType() == RoofObject::SlopeW ||
                mHandleObject->roofType() == RoofObject::SlopeE) {
            QRect bounds = mHandleObject->bounds().adjusted(0, 0, diff.x(), diff.y());
            if (bounds.width() > 3) bounds.setWidth(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = bounds.contains(mCurrentPos) ? d.x() < 0.5 : mCurrentPos.x() < mHandleObject->x();
            if (mHandleObject->roofType() == RoofObject::SlopeE)
                halfDepth = !halfDepth;
        }
        if (mHandleObject->roofType() == RoofObject::SlopeN ||
                mHandleObject->roofType() == RoofObject::SlopeS) {
            QRect bounds = mHandleObject->bounds().adjusted(0, 0, diff.x(), diff.y());
            if (bounds.height() > 3) bounds.setHeight(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = bounds.contains(mCurrentPos) ? d.y() < 0.5 : mCurrentPos.y() < mHandleObject->y();
            if (mHandleObject->roofType() == RoofObject::SlopeS)
                halfDepth = !halfDepth;
        }
#endif

        resizeRoof(mHandleObject->width() + diff.x(),
                   mHandleObject->height() + diff.y(),
                   halfDepth);
        updateStatusText();

        mEditor->setCursorObject(mHandleObject);
        return;
    }

    if (mMode == Create) {
        if (!mEditor->currentFloorContains(mCurrentPos))
            return;

        QPoint pos = mStartPos;

#if 1
        // Fugly code to determine if a roof should be half-depth.
        bool halfDepth = false;
        if (mObject->roofType() == RoofObject::CornerOuterSE
                || mObject->roofType() == RoofObject::CornerInnerNW) {
            QRect bounds = QRect(mObject->x(), mObject->y(), qAbs(diff.x()) + 1, qAbs(diff.y()) + 1);
            if (bounds.width() > 3) bounds.setWidth(3);
            if (bounds.height() > 3) bounds.setHeight(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = d.x() < 0.5 && d.y() < 0.5;
            qreal angle = QLineF(mObject->bounds().topLeft(), mCurrentTilePosF).angle();
            if (angle >= 315) {
                halfDepth = bounds.contains(mCurrentPos) && d.x() < 0.5;
                if (diff.x() < 0 || diff.y() < 0)
                    halfDepth = !halfDepth;
            } else if (angle >= 270) {
                halfDepth = bounds.contains(mCurrentPos) && d.y() < 0.5;
                if (diff.x() < 0 || diff.y() < 0)
                    halfDepth = !halfDepth;
            } else
                halfDepth = true;
        }
        if (mObject->roofType() == RoofObject::SlopeW ||
                mObject->roofType() == RoofObject::SlopeE) {
            QRect bounds = QRect(mObject->x(), mObject->y(), qAbs(diff.x()) + 1, qAbs(diff.y()) + 1);
            if (bounds.width() > 3) bounds.setWidth(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = bounds.contains(mCurrentPos) ? d.x() < 0.5 : mCurrentPos.x() < mObject->x();
            if (mObject->roofType() == RoofObject::SlopeE)
                halfDepth = !halfDepth;
        }
        if (mObject->roofType() == RoofObject::SlopeN ||
                mObject->roofType() == RoofObject::SlopeS) {
            QRect bounds = QRect(mObject->x(), mObject->y(), qAbs(diff.x()) + 1, qAbs(diff.y()) + 1);
            if (bounds.height() > 3) bounds.setHeight(3);
            QPointF d = mCurrentTilePosF - mCurrentPos;
            halfDepth = bounds.contains(mCurrentPos) ? d.y() < 0.5 : mCurrentPos.y() < mObject->y();
            if (mObject->roofType() == RoofObject::SlopeS)
                halfDepth = !halfDepth;
        }
#endif

        // This call might restrict the width and/or height.
        mObject->resize(qAbs(diff.x()) + 1, qAbs(diff.y()) + 1, halfDepth);

        if (diff.x() < 0)
            pos.setX(mStartPos.x() - mObject->width() + 1);
        if (diff.y() < 0)
            pos.setY(mStartPos.y() - mObject->height() + 1);
        mObject->setPos(pos);

        mItem->synchWithObject();
        mItem->update();

        updateStatusText();

        mEditor->setCursorObject(mObject);
    }
}

void RoofTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (mMode == Resize) {
        mMode = NoMode;
        int width = mHandleObject->width(), height = mHandleObject->height();
        bool halfDepth = mHandleObject->isHalfDepth();
        if (width != mOriginalWidth || height != mOriginalHeight
                || halfDepth != mOriginalHalfDepth) {
            mHandleObject->resize(mOriginalWidth, mOriginalHeight, mOriginalHalfDepth);
            undoStack()->push(new ResizeRoof(mEditor->document(), mHandleObject,
                                             width, height, halfDepth));
        }
        mEditor->setCursorObject(nullptr);
    }

    if (mMode == Create) {
        mMode = NoMode;
        if (mObject->isValidPos()) {
            mObject->setCapTiles(mEditor->building()->roofCapTile());
            mObject->setSlopeTiles(mEditor->building()->roofSlopeTile());
            mObject->setTopTiles(mEditor->building()->roofTopTile());
            mObject->setDefaultCaps();
            BuildingFloor *floor = this->floor();
            undoStack()->push(new AddObject(mEditor->document(), floor,
                                            floor->objectCount(), mObject));
        } else
            delete mObject;
        mObject = nullptr;
        delete mItem;
        mItem = nullptr;

        mEditor->setCursorObject(nullptr);
    }

    updateHandle(event->scenePos());
    updateStatusText();
}

void RoofTool::activate()
{
    BaseTool::activate();
    updateStatusText();
    if (mCursorItem)
        mEditor->addItem(mCursorItem);
}

void RoofTool::deactivate()
{
    BaseTool::deactivate();
    if (mCursorItem)
        mEditor->removeItem(mCursorItem);

    if (mMouseOverHandle) {
        mHandleItem->setHighlight(false);
        mMouseOverHandle = false;
    }
    mHandleObject = nullptr;
    if (mObjectItem) {
        mObjectItem->setShowHandles(false);
        mObjectItem->setZValue(mObjectItem->object()->index());
        mObjectItem = nullptr;
    }

    if (mMode == Create) {
        delete mObject;
        mObject = nullptr;
        delete mItem;
        mItem = nullptr;
        mMode = NoMode;
        mEditor->setCursorObject(nullptr);
    }

    mMode = NoMode;
}

void RoofTool::objectAboutToBeRemoved(BuildingObject *object)
{
    if (object == mHandleObject) {
        mHandleObject = nullptr;
        mObjectItem = nullptr;
        mMouseOverHandle = false;
        mMode = NoMode;
    }
}

RoofObject *RoofTool::topmostRoofAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mEditor->items(scenePos)) {
        if (GraphicsRoofItem *roofItem = dynamic_cast<GraphicsRoofItem*>(item)) {
            if (roofItem->object()->floor() == floor()) {
                return roofItem->object()->asRoof();
            }
        }
    }
    return nullptr;
}

void RoofTool::updateHandle(const QPointF &scenePos)
{
    RoofObject *ro = topmostRoofAt(scenePos);

    if (mMouseOverHandle) {
        mHandleItem->setHighlight(false);
        mMouseOverHandle = false;
        updateStatusText();
    }
    mHandleItem = nullptr;
    if (ro && (ro == mHandleObject)) {
        foreach (QGraphicsItem *item, mEditor->items(scenePos)) {
            if (GraphicsRoofHandleItem *handle = dynamic_cast<GraphicsRoofHandleItem*>(item)) {
                if (handle->parentItem() == mObjectItem) {
                    mHandleItem = handle;
                    mMouseOverHandle = true;
                    mHandleItem->setHighlight(true);
                    updateStatusText();
                    break;
                }
            }
        }
        return;
    }

    if (mObjectItem) {
        mObjectItem->setShowHandles(false);
        mObjectItem->setZValue(mObjectItem->object()->index());
        mObjectItem = nullptr;
    }

    if (ro) {
        mObjectItem = mEditor->itemForObject(ro)->asRoof();
        mObjectItem->setShowHandles(true);
        mObjectItem->setZValue(mObjectItem->object()->floor()->objectCount());
    }
    mHandleObject = ro;

    mEditor->setMouseOverObject(ro);
}

void RoofTool::resizeRoof(int width, int height, bool halfDepth)
{
    if (width < 1 || height < 1)
        return;

    RoofObject *roof = mHandleObject;

    int oldWidth = roof->width(), oldHeight = roof->height();
    bool oldHalfDepth = roof->isHalfDepth();
    roof->resize(width, height, halfDepth);
    if (!roof->isValidPos()) {
        roof->resize(oldWidth, oldHeight, oldHalfDepth);
        return;
    }


    mEditor->itemForObject(roof)->synchWithObject();
    mStartPos = roof->bounds().bottomRight();
}

void RoofTool::toggleCappedW()
{
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::ToggleCappedW));
}

void RoofTool::toggleCappedN()
{
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::ToggleCappedN));
}

void RoofTool::toggleCappedE()
{
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::ToggleCappedE));
}

void RoofTool::toggleCappedS()
{
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::ToggleCappedS));
}

void RoofTool::depthUp()
{
    if (mHandleObject->isDepthMax())
        return;
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::IncrDepth));
}

void RoofTool::depthDown()
{
    if (mHandleObject->isDepthMin())
        return;
    undoStack()->push(new HandleRoof(mEditor->document(), mHandleObject,
                                     HandleRoof::DecrDepth));
}

void RoofTool::updateStatusText()
{
    if (mMode == Create)
        setStatusText(tr("Width,Height=%1,%2.  Right-click to cancel.")
                      .arg(mObject->width())
                      .arg(mObject->height()));
    else if (mMode == Resize)
        setStatusText(tr("Width,Height=%1,%2.  Right-click to cancel.")
                      .arg(mHandleObject->width())
                      .arg(mHandleObject->height()));
    else if (mMouseOverHandle)
        setStatusText(mHandleItem->statusText());
    else
        setStatusText(tr("Left-click-drag to place a roof.  Right-click to remove any object."));

}

/////

RoofShallowTool *RoofShallowTool::mInstance = nullptr;

RoofShallowTool *RoofShallowTool::instance()
{
    if (!mInstance)
        mInstance = new RoofShallowTool;
    return mInstance;
}

RoofShallowTool::RoofShallowTool()
    : RoofTool()
{
    setRoofType(RoofObject::ShallowPeakNS);
}

/////

RoofCornerTool *RoofCornerTool::mInstance = nullptr;

RoofCornerTool *RoofCornerTool::instance()
{
    if (!mInstance)
        mInstance = new RoofCornerTool;
    return mInstance;
}

RoofCornerTool::RoofCornerTool()
    : RoofTool()
{
    setRoofType(RoofObject::CornerInnerNW);
}

/////

class SetSelectedObjects : public QUndoCommand
{
public:
    SetSelectedObjects(BuildingDocument *doc, const QSet<BuildingObject*> &selection) :
        mDocument(doc),
        mSelectedObjects(selection)
    {}

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap()
    {
        QSet<BuildingObject*> old = mDocument->selectedObjects();
        mDocument->setSelectedObjects(mSelectedObjects);
        mSelectedObjects = old;
    }

    BuildingDocument *mDocument;
    QSet<BuildingObject*> mSelectedObjects;
};

/////

SelectMoveObjectTool *SelectMoveObjectTool::mInstance = nullptr;

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
    mMouseOverObject(false),
    mMouseOverSelection(false),
    mClickedObject(nullptr),
    mHoverObject(nullptr),
    mSelectionRectItem(nullptr)
{
    updateStatusText();
}

void SelectMoveObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMouseDown = true;
        mStartScenePos = event->scenePos();
        mClickedObject = mEditor->topmostObjectAt(mStartScenePos);
        if (mMouseOverObject && !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            startMoving();
    }
    if (event->button() == Qt::RightButton) {
        if (mMode == Moving)
            cancelMoving();
    }
}

void SelectMoveObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();

    if (!mMouseDown) {
        BuildingObject *object = mEditor->topmostObjectAt(pos);
        bool mouseOverObject = object != nullptr;
        bool mouseOverSelection = object && mEditor->document()->selectedObjects().contains(object);
        if (mouseOverObject != mMouseOverObject ||
                mouseOverSelection != mMouseOverSelection) {
            mMouseOverObject = mouseOverObject;
            mMouseOverSelection = mouseOverSelection;
            updateStatusText();
        }
        setHandCursor(mMouseOverSelection ? HandOpen : HandNone);

        mEditor->setMouseOverObject(object);
    }

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
    case Selecting: {
        QRectF tileRect = mEditor->sceneToTileRectF(QRectF(mStartScenePos, pos), mEditor->currentLevel());
        mSelectionRectItem->setPolygon(mEditor->tileToScenePolygonF(tileRect.normalized(), mEditor->currentLevel()));
        break;
    }
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
            QSet<BuildingObject*> selection = mEditor->document()->selectedObjects();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & Qt::ShiftModifier) {
                selection.insert(mClickedObject);
            } else if (modifiers & Qt::ControlModifier) {
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
            mEditor->document()->setSelectedObjects(QSet<BuildingObject*>());
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
    mClickedObject = nullptr;
    mouseMoveEvent(event); // update mMouseOverXXX
    updateStatusText();
}

void SelectMoveObjectTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    if (mMode == Moving) {
        bool duplicate = controlModifier();
        if (duplicate && mClones.isEmpty()) {
            int index = floor()->objectCount();
            foreach (BuildingObject *object, objectsInOrder(mMovingObjects)) {
                BuildingObject *clone = object->clone();
                clone->setFloor(nullptr);
                GraphicsObjectItem *item = mEditor->createItemForObject(clone);
                item->setSelected(true);
                item->setDragging(true);
                item->setZValue(mEditor->ZVALUE_CURSOR + index++);
                if (object == mEditor->mouseOverObject())
                    item->setMouseOver(true);
                mEditor->addItem(item);
                mClones += item;
            }
        }
        // When duplicating, the original objects are left at their original
        // locations.
        foreach (BuildingObject *object, mMovingObjects) {
            GraphicsObjectItem *item = mEditor->itemForObject(object);
            item->setSelected(!duplicate);
            item->setDragging(!duplicate);
            item->setDragOffset(mDragOffset);
            if (object == mEditor->mouseOverObject())
                item->setMouseOver(!duplicate);
            mEditor->dragObject(object->floor(), object, duplicate ? QPoint(0,0) : mDragOffset);
        }
        foreach (GraphicsObjectItem *item, mClones) {
            item->setVisible(duplicate);
            item->setDragOffset(mDragOffset);
            mEditor->dragObject(floor(), item->object(), mDragOffset);
        }
    }
}

void SelectMoveObjectTool::activate()
{
    BaseTool::activate();
}

void SelectMoveObjectTool::deactivate()
{
    if (mMode == Moving)
        cancelMoving();
    BaseTool::deactivate();
}

void SelectMoveObjectTool::objectAboutToBeRemoved(BuildingObject *object)
{
    if (mMovingObjects.contains(object)) {
        mMovingObjects.remove(object);
    }
}

void SelectMoveObjectTool::updateSelection(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
    QRectF sceneRect = QRectF(mStartScenePos, pos);
    QRectF tileRect = mEditor->sceneToTileRectF(sceneRect, mEditor->currentLevel()).normalized();
#if 0
    // Make sure the rect has some contents, otherwise intersects returns false
    tileRect.setWidth(qMax(qreal(1), tileRect.width()));
    tileRect.setHeight(qMax(qreal(1), tileRect.height()));
#endif
    QSet<BuildingObject*> selectedObjects;

    foreach (BuildingObject *object, mEditor->objectsInRect(tileRect))
        selectedObjects.insert(object);

    const QSet<BuildingObject*> oldSelection = mEditor->document()->selectedObjects();
    QSet<BuildingObject*> newSelection;

    if (modifiers & Qt::ShiftModifier) {
        newSelection = oldSelection | selectedObjects;
    } else if (modifiers & Qt::ControlModifier) {
        QSet<BuildingObject*> select, deselect;
        foreach (BuildingObject *object, selectedObjects) {
            if (oldSelection.contains(object))
                deselect.insert(object);
            else
                select.insert(object);
        }
        newSelection = oldSelection - deselect + select;
    } else {
        newSelection = selectedObjects;
    }

    mEditor->document()->setSelectedObjects(newSelection);
}

void SelectMoveObjectTool::startSelecting()
{
    mMode = Selecting;
    if (mSelectionRectItem == nullptr) {
        mSelectionRectItem = new QGraphicsPolygonItem();
        mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
        mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
        mSelectionRectItem->setZValue(mEditor->ZVALUE_CURSOR);
    }
    mEditor->addItem(mSelectionRectItem);
    updateStatusText();
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
    setHandCursor(HandClosed);
    updateStatusText();
}

void SelectMoveObjectTool::updateMovingItems(const QPointF &pos,
                                             Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startTilePos = mEditor->sceneToTile(mStartScenePos, mEditor->currentLevel());
    QPoint currentTilePos = mEditor->sceneToTile(pos, mEditor->currentLevel());
    mDragOffset = currentTilePos - startTilePos;

    currentModifiersChanged(modifiers);
}

void SelectMoveObjectTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (BuildingObject *object, mMovingObjects) {
        GraphicsObjectItem *item = mEditor->itemForObject(object);
        item->setDragging(false);
        item->setSelected(true);
        mEditor->resetDrag(object->floor(), object);
    }
    foreach (GraphicsObjectItem *item, mClones) {
        BuildingObject *clone = item->object();
        mEditor->resetDrag(floor(), clone);
        delete item;
        delete clone;
    }
    mClones.clear();

    if (mDragOffset.isNull()) // Move is a no-op
        return;

    QUndoStack *undoStack = this->undoStack();
    if (controlModifier()) {
        undoStack->beginMacro(tr("Copy %n Object(s)", "", mMovingObjects.size()));
        QSet<BuildingObject*> clones;
        foreach (BuildingObject *object, objectsInOrder(mMovingObjects)) {
            if (object->isValidPos(mDragOffset)) {
                BuildingObject *clone = object->clone();
                clone->setPos(object->pos() + mDragOffset);
                undoStack->push(new AddObject(mEditor->document(), object->floor(),
                                              object->floor()->objectCount(),
                                              clone));
                clones.insert(clone);
            }
        }
        undoStack->push(new SetSelectedObjects(mEditor->document(), clones));
        undoStack->endMacro();
    } else {
        // Handle 'delete' while moving
        if (mMovingObjects.isEmpty())
            return;

        undoStack->beginMacro(tr("Move %n Object(s)", "", mMovingObjects.size()));
        foreach (BuildingObject *object, mMovingObjects) {
            if (!object->isValidPos(mDragOffset))
                undoStack->push(new RemoveObject(mEditor->document(),
                                                 object->floor(),
                                                 object->index()));
            else
                undoStack->push(new MoveObject(mEditor->document(), object,
                                               object->pos() + mDragOffset));
        }
        undoStack->endMacro();
    }

    mMovingObjects.clear();
}

void SelectMoveObjectTool::cancelMoving()
{
    foreach (BuildingObject *object, mMovingObjects) {
        GraphicsObjectItem *item = mEditor->itemForObject(object);
        item->setDragging(false);
        item->setSelected(true);
        mEditor->resetDrag(object->floor(), object);
    }
    foreach (GraphicsObjectItem *item, mClones) {
        BuildingObject *clone = item->object();
        mEditor->resetDrag(floor(), clone);
        delete item;
        delete clone;
    }
    mClones.clear();

    mMovingObjects.clear();

    mMode = CancelMoving;
}

void SelectMoveObjectTool::updateStatusText()
{
    if (mMode == Moving) {
        setStatusText(tr("CTRL to duplicate objects.  Right-click to cancel."));
    } else if (mMode == Selecting) {
        setStatusText(tr("CTRL to toggle selected state.  SHIFT to add to selection."));
    } else if (mMouseOverSelection) {
        setStatusText(tr("Left-click-drag to move selected objects.  CTRL to duplicate objects."));
    } else if (mMouseOverObject) {
        setStatusText(tr("Left-click to select.  Left-click-drag to select and move.  CTRL=toggle.  SHIFT=add."));
    } else
        setStatusText(tr("Left-click-drag to select."));
}

QList<BuildingObject*> SelectMoveObjectTool::objectsInOrder(const QSet<BuildingObject*> &objects)
{
    QMap<int,BuildingObject*> map;
    foreach (BuildingObject *object, objects)
        map[object->index()] = object;
    return map.values();
}

/////

/////

WallTool *WallTool::mInstance = nullptr;

WallTool *WallTool::instance()
{
    if (!mInstance)
        mInstance = new WallTool;
    return mInstance;
}

WallTool::WallTool() :
    BaseTool(),
    mMode(NoMode),
    mTileEdge(BuildingObject::Direction::Invalid),
    mObject(nullptr),
    mItem(nullptr),
    mCursorItem(nullptr),
    mEyedrop(false),
    mMouseOverObject(false),
    mObjectItem(nullptr),
    mHandleObject(nullptr),
    mHandleItem(nullptr),
    mMouseOverHandle(false),
    mCurrentExteriorTile(BuildingTilesMgr::instance()->noneTileEntry()),
    mCurrentInteriorTile(BuildingTilesMgr::instance()->noneTileEntry()),
    mCurrentExteriorTrim(BuildingTilesMgr::instance()->noneTileEntry()),
    mCurrentInteriorTrim(BuildingTilesMgr::instance()->noneTileEntry())
{
}

void WallTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mEyedrop) {
            if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
                eyedrop(object);
            }
            return;
        }
        if (mMode != NoMode)
            return; // ignore clicks when creating/resizing
        mStartTilePos = mCurrentTilePos;
        if (mMouseOverHandle) {
            mOriginalPos = mHandleObject->pos();
            mOriginalLength = mHandleObject->length();
            mMode = Resize;
            updateStatusText();

            mEditor->setCursorObject(mHandleObject);
            return;
        }

        if (!isValidPos(mCurrentTilePos, mTileEdge))
            return;

        mObject = new WallObject(floor(),
                                 mStartTilePos.x(), mStartTilePos.y(),
                                 mTileEdge,
                                 /*length=*/1);
        mItem = new GraphicsWallItem(mEditor, mObject);
        mItem->setZValue(mEditor->ZVALUE_CURSOR);
        mEditor->addItem(mItem);
        mMode = Create;
        updateStatusText();

        mObject->setTile(mCurrentExteriorTile, WallObject::TileExterior);
        mObject->setTile(mCurrentInteriorTile, WallObject::TileInterior);
        mObject->setTile(mCurrentExteriorTrim, WallObject::TileExteriorTrim);
        mObject->setTile(mCurrentInteriorTrim, WallObject::TileInteriorTrim);
        mEditor->setCursorObject(mObject);
    }

    if (event->button() == Qt::RightButton) {
        if (mMode == Create) {
            delete mObject;
            mObject = nullptr;
            delete mItem;
            mItem = nullptr;
            mMode = NoMode;
            updateStatusText();
            mEditor->setCursorObject(nullptr);
            return;
        }
        if (mMode == Resize) {
            mHandleObject->setPos(mOriginalPos);
            mHandleObject->setLength(mOriginalLength);
            mObjectItem->synchWithObject();
            mMode = NoMode;
            updateStatusText();
            mEditor->setCursorObject(nullptr);
            return;
        }
        if (mMode != NoMode)
            return; // ignore clicks when creating/resizing
        if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
            undoStack()->push(new RemoveObject(mEditor->document(), floor(),
                                               object->index()));
        }
    }
}

void WallTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mEyedrop) {
        BuildingObject *object = mEditor->topmostObjectAt(event->scenePos());
        if (object && (object->asRoof() /*|| object->asWall()*/))
            object = nullptr;
        bool mouseOverObject = object != nullptr;
        if (mouseOverObject != mMouseOverObject)
            mMouseOverObject = mouseOverObject;
        mEditor->setMouseOverObject(object);
    }

    mCurrentTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
    QPointF p = mEditor->sceneToTileF(event->scenePos(), mEditor->currentLevel());
    QPointF m(p.x() - int(p.x()), p.y() - int(p.y()));
#if 0
    if (m.x() > 0.5)
        mCurrentTilePos.setX(mCurrentTilePos.x() + 1);
    if (m.y() >= 0.5)
        mCurrentTilePos.setY(mCurrentTilePos.y() + 1);
#endif
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
    if (dW < dE) {
        mTileEdge = (dW < dN && dW < dS) ? BuildingObject::Direction::W : ((dN < dS) ? BuildingObject::Direction::N : BuildingObject::Direction::S);
    } else {
        mTileEdge = (dE < dN && dE < dS) ? BuildingObject::Direction::E : ((dN < dS) ? BuildingObject::Direction::N : BuildingObject::Direction::S);
    }

    mScenePos = event->scenePos();

    updateCursor();
}

void WallTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (mMode == Resize) {
        mMode = NoMode;
        QPoint pos = mHandleObject->pos();
        int length = mHandleObject->length();
        if (pos != mOriginalPos || length != mOriginalLength) {
            mHandleObject->setPos(mOriginalPos);
            mHandleObject->setLength(mOriginalLength);
            undoStack()->beginMacro(tr("Resize Wall"));
            undoStack()->push(new MoveObject(mEditor->document(), mHandleObject,
                                             pos));
            undoStack()->push(new ResizeWall(mEditor->document(), mHandleObject,
                                             length));
            undoStack()->endMacro();
            updateStatusText();
        }
        mEditor->setCursorObject(nullptr);
        return;
    }

    if (mMode == Create) {
        mMode = NoMode;
        if (mObject->isValidPos()) {
            mObject->setTile(mCurrentExteriorTile, WallObject::TileExterior);
            mObject->setTile(mCurrentInteriorTile, WallObject::TileInterior);
            mObject->setTile(mCurrentExteriorTrim, WallObject::TileExteriorTrim);
            mObject->setTile(mCurrentInteriorTrim, WallObject::TileInteriorTrim);
            BuildingFloor *floor = this->floor();
            undoStack()->push(new AddObject(mEditor->document(), floor,
                                            floor->objectCount(), mObject));
        } else {
            delete mObject;
        }
        mObject = nullptr;
        delete mItem;
        mItem = nullptr;
        updateStatusText();
        mEditor->setCursorObject(nullptr);
    }
}

void WallTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    bool eyedrop = (modifiers & Qt::AltModifier) != 0;
    if (eyedrop != mEyedrop) {
        mEyedrop = eyedrop;
        if (mEyedrop) {
            mEditor->views().at(0)->viewport()->setCursor(Qt::PointingHandCursor);
        } else {
            mEditor->views().at(0)->viewport()->unsetCursor();
            mEditor->setMouseOverObject(nullptr);
        }
    }
    updateCursor();
}

void WallTool::activate()
{
    BaseTool::activate();
    updateStatusText();
    if (mCursorItem)
        mEditor->addItem(mCursorItem);
    currentModifiersChanged(qApp->keyboardModifiers());
}

void WallTool::deactivate()
{
    BaseTool::deactivate();
    if (mEyedrop) {
        mEditor->views().at(0)->viewport()->unsetCursor();
        mEditor->setMouseOverObject(nullptr);
        mEyedrop = false;
    }
    if (mCursorItem)
        mEditor->removeItem(mCursorItem);
}

void WallTool::objectAboutToBeRemoved(BuildingObject *object)
{
    if (object == mHandleObject) {
        mHandleObject = nullptr;
        mObjectItem = nullptr;
        mMouseOverHandle = false;
        mMode = NoMode;
    }
}

void WallTool::eyedrop(BuildingObject *object)
{
    updateCursor();

    mEditor->document()->setSelectedObjects(QSet<BuildingObject*>());

    if (FurnitureObject *fobj = object->asFurniture())
        FurnitureTool::instance()->setCurrentTile(fobj->furnitureTile());

    BaseTool *tool = nullptr;
    if (object->asDoor()) tool = DoorTool::instance();
    if (object->asFurniture()) tool = FurnitureTool::instance();
//    if (object->asRoof()) tool = RoofTool::instance();
    if (object->asStairs()) tool = StairsTool::instance();
    if (object->asWall()) tool = WallTool::instance();
    if (object->asWindow()) tool = WindowTool::instance();
    if (tool && !tool->isCurrent() && tool->action()->isEnabled())
        QMetaObject::invokeMethod(tool, "makeCurrent", Qt::QueuedConnection);

    mEditor->document()->emitObjectPicked(object);
}

WallObject *WallTool::topmostWallAt(const QPointF &scenePos)
{
    const QList<QGraphicsItem*> items = mEditor->items(scenePos);
    for (QGraphicsItem *item : items) {
        if (GraphicsWallItem *wallItem = dynamic_cast<GraphicsWallItem*>(item)) {
            if (wallItem->object()->floor() == floor()) {
                return wallItem->object()->asWall();
            }
        }
    }
    return nullptr;
}

void WallTool::updateCursor()
{
    if (mMode == NoMode) {
        if (!mCursorItem) {
            mCursorItem = new QGraphicsPolygonItem;
            mCursorItem->setZValue(mEditor->ZVALUE_CURSOR);
            mCursorItem->setBrush(QColor(0,255,0,128));
            mEditor->addItem(mCursorItem);
        }

        QRectF rect;
        switch (mTileEdge) {
        case BuildingObject::Direction::N:
            rect.setRect(mCurrentTilePos.x() - 0.0/30.0, mCurrentTilePos.y() - 0.0/30.0, 30.0/30.0, 12.0/30.0);
            break;
        case BuildingObject::Direction::E:
            rect.setRect(mCurrentTilePos.x() + 1 - 12.0/30.0, mCurrentTilePos.y() - 0.0/30.0, 12.0/30.0, 30.0/30.0);
            break;
        case BuildingObject::Direction::S:
            rect.setRect(mCurrentTilePos.x() - 0.0/30.0, mCurrentTilePos.y() + 1 - 12.0/30.0, 30.0/30.0, 12.0/30.0);
            break;
        case BuildingObject::Direction::W:
            rect.setRect(mCurrentTilePos.x() - 0.0/30.0, mCurrentTilePos.y() - 0.0/30.0, 12.0/30.0, 30.0/30.0);
            break;
        default:
            //Q_ASSERT(false);
            return;
        }
        mCursorItem->setPolygon(mEditor->tileToScenePolygonF(rect, mEditor->currentLevel()));

        updateHandle(mScenePos);

        mCursorItem->setVisible(isValidPos(mCurrentTilePos, mTileEdge) &&
                                !mMouseOverHandle && !mEyedrop);

        // See NOTE-SCENE-CORRUPTION
        if (mCursorItem->boundingRect() != mCursorSceneRect) {
            // Don't call mCursor->update(), it isn't the same.
            mEditor->update(mCursorSceneRect);
            mCursorSceneRect = mCursorItem->boundingRect();
        }
        return;
    }

    QPoint diff = mCurrentTilePos - mStartTilePos;

    if (mMode == Resize) {
        QPoint start = mHandleObject->pos();
        QPoint end = mHandleObject->bounds().bottomRight();
        if (mHandleItem == mObjectItem->resizeHandle1()) {
            if (mHandleObject->isW() || mHandleObject->isE()) {
                start.ry() = mCurrentTilePos.y();
                if (start.y() < 0)
                    start.setY(0);
                if (start.y() > end.y())
                    start.setY(end.y());
                mHandleObject->setPos(start);
                resizeWall(end.y() - start.y() + 1);
            } else {
                start.rx() = mCurrentTilePos.x();
                if (start.x() < 0)
                    start.setX(0);
                if (start.x() >= end.x())
                    start.setX(end.x());
                mHandleObject->setPos(start);
                resizeWall(end.x() - start.x() + 1);
            }
        } else {
            if (mHandleObject->isW() || mHandleObject->isE()) {
                end.ry() = mCurrentTilePos.y();
                if (end.y() < start.y())
                    end.setY(start.y());
                if (end.y() >= floor()->height())
                    end.setY(floor()->height() - 1);
                resizeWall(end.y() - start.y() + 1);
            } else {
                end.rx() = mCurrentTilePos.x();
                if (end.x() < start.x())
                    end.setX(start.x());
                if (end.x() >= floor()->width())
                    end.setX(floor()->width() - 1);
                resizeWall(end.x() - start.x() + 1);
            }
        }
        mEditor->setCursorObject(mHandleObject);
        return;
    }

    if (mMode == Create) {
        if (!isValidPos(mCurrentTilePos, mObject->dir()))
            return;

        QPoint pos = mStartTilePos;

        if (mObject->isN() || mObject->isS()) {
            mObject->setLength(qAbs(diff.x()) + 1);
            if (diff.x() < 0) {
                pos.setX(mStartTilePos.x() - mObject->length() + 1);
            }
        } else {
            mObject->setLength(qAbs(diff.y()) + 1);
            if (diff.y() < 0) {
                pos.setY(mStartTilePos.y() - mObject->length() + 1);
            }
        }

        mObject->setPos(pos);

        mItem->synchWithObject();
        mItem->update();

        mEditor->setCursorObject(mObject);
    }
}

void WallTool::updateHandle(const QPointF &scenePos)
{
    WallObject *wall = topmostWallAt(scenePos);
    if (mEyedrop)
        wall = nullptr;

    if (mMouseOverHandle) {
        mHandleItem->setHighlight(false);
        mMouseOverHandle = false;
        updateStatusText();
    }
    mHandleItem = nullptr;
    if (wall && (wall == mHandleObject)) {
        const QList<QGraphicsItem*> items = mEditor->items(scenePos);
        for (QGraphicsItem *item : items) {
            if (GraphicsWallHandleItem *handle = dynamic_cast<GraphicsWallHandleItem*>(item)) {
                if (handle->parentItem() == mObjectItem) {
                    mHandleItem = handle;
                    mMouseOverHandle = true;
                    mHandleItem->setHighlight(true);
                    updateStatusText();
                    break;
                }
            }
        }
        return;
    }

    if (mObjectItem) {
        mObjectItem->setShowHandles(false);
        mObjectItem->setZValue(mObjectItem->object()->index());
        mObjectItem = nullptr;
    }

    if (wall) {
        mObjectItem = mEditor->itemForObject(wall)->asWall();
        mObjectItem->setShowHandles(true);
        mObjectItem->setZValue(wall->floor()->objectCount());
    }
    mHandleObject = wall;

    if (!mEyedrop)
        mEditor->setMouseOverObject(wall);
}

void WallTool::updateStatusText()
{
    if (mMode == Create || mMode == Resize)
        setStatusText(tr("Right-click to cancel."));
    else if (mMouseOverHandle)
        setStatusText(tr("Left-click-drag to resize wall."));
    else
        setStatusText(tr("Left-click-drag to place a wall.  Right-click to delete any object.  ALT = eyedrop."));
}

void WallTool::resizeWall(int length)
{
    if (length < 1)
        return;

    WallObject *wall = mHandleObject;

    int oldLength = wall->length();
    wall->setLength(length);
    if (!wall->isValidPos()) {
        wall->setLength(oldLength);
        return;
    }

    mEditor->itemForObject(wall)->synchWithObject();
    mStartTilePos = wall->pos() + ((wall->isW() || wall->isE())
            ? QPoint(0, wall->length())
            : QPoint(wall->length(), 0));
}

bool WallTool::isValidPos(const QPoint &pos, BuildingObject::Direction dir)
{
    QRect rectValid = floor()->bounds();
    switch (dir) {
    case BuildingObject::Direction::N:
        rectValid.adjust(0, 0, 0, 1);
        break;
    case BuildingObject::Direction::S:
        break;
    case BuildingObject::Direction::W:
        rectValid.adjust(0, 0, 1, 0);
        break;
    case BuildingObject::Direction::E:
        break;
    default:
        break;
    }
    return rectValid.contains(pos);
}
