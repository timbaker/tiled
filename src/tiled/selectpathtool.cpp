/*
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2012, Tim Baker
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

#include "selectpathtool.h"

#include "layer.h"
#include "map.h"
#include "mapdocument.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "movepath.h"
#include "pathitem.h"
#include "pathlayer.h"
#include "preferences.h"
#include "selectionrectangle.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

SelectPathTool::SelectPathTool(QObject *parent)
    : AbstractPathTool(tr("Select Paths"),
          QIcon(QLatin1String(":images/22x22/tool-select-objects.png")),
          QKeySequence(tr("S")),
          parent)
    , mSelectionRectangle(new SelectionRectangle)
    , mMousePressed(false)
    , mClickedItem(0)
    , mMode(NoMode)
{
}

SelectPathTool::~SelectPathTool()
{
    delete mSelectionRectangle;
}

void SelectPathTool::mouseEntered()
{
}

void SelectPathTool::mouseMoved(const QPointF &pos,
                                     Qt::KeyboardModifiers modifiers)
{
    AbstractPathTool::mouseMoved(pos, modifiers);

    if (mMode == NoMode && mMousePressed) {
        const int dragDistance = (mStartPos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedItem)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
        mSelectionRectangle->setRectangle(QRectF(mStartPos, pos).normalized());
        break;
    case Moving:
        updateMovingItems(pos, modifiers);
        break;
    case CancelMoving:
        break;
    case NoMode:
        break;
    }
}

void SelectPathTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMousePressed = true;
        mStartPos = event->scenePos();
        mClickedItem = topMostPathItemAt(mStartPos);
        break;
    case Qt::RightButton:
        if (mMode == Moving)
            cancelMoving();
        else
            AbstractPathTool::mousePressed(event); // context menu
        break;
    default:
        AbstractPathTool::mousePressed(event);
        break;
    }
}

void SelectPathTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedItem) {
            QSet<PathItem*> selection = mapScene()->selectedPathItems();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedItem))
                    selection.remove(mClickedItem);
                else
                    selection.insert(mClickedItem);
            } else {
                selection.clear();
                selection.insert(mClickedItem);
            }
            mapScene()->setSelectedPathItems(selection);
        } else {
            mapScene()->setSelectedPathItems(QSet<PathItem*>());
        }
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
        mapScene()->removeItem(mSelectionRectangle);
        mMode = NoMode;
        break;
    case Moving:
        // WTF: The sceneRect may change when moving a Lot, which calls
        // QGraphicsView::replayLastMouseEvent, which calls our mouseMoved(), which
        // starts the drag *again* because mMousePressed hasn't been cleared yet.
        mMousePressed = false;
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMousePressed = false;
    mClickedItem = 0;
}

void SelectPathTool::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    mModifiers = modifiers;
}

void SelectPathTool::languageChanged()
{
    setName(tr("Select Paths"));
//    setShortcut(QKeySequence(tr("S")));
}

void SelectPathTool::updateSelection(const QPointF &pos,
                                     Qt::KeyboardModifiers modifiers)
{
    QRectF rect = QRectF(mStartPos, pos).normalized();

    // Make sure the rect has some contents, otherwise intersects returns false
    rect.setWidth(qMax(qreal(1), rect.width()));
    rect.setHeight(qMax(qreal(1), rect.height()));

    QSet<PathItem*> selectedItems;

    foreach (QGraphicsItem *item, mapScene()->items(rect)) {
        PathItem *pathItem = dynamic_cast<PathItem*>(item);
        if (pathItem)
            selectedItems.insert(pathItem);
    }

    const QSet<PathItem*> oldSelection = mapScene()->selectedPathItems();
    QSet<PathItem*> newSelection;

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        newSelection = oldSelection | selectedItems;
    } else {
        newSelection = selectedItems;
    }

    mapScene()->setSelectedPathItems(newSelection);
}

void SelectPathTool::startSelecting()
{
    mMode = Selecting;
    mapScene()->addItem(mSelectionRectangle);
}

void SelectPathTool::startMoving()
{
    mMovingItems = mapScene()->selectedPathItems();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingItems.contains(mClickedItem)) {
        mMovingItems.clear();
        mMovingItems.insert(mClickedItem);
        mapScene()->setSelectedPathItems(mMovingItems);
    }

    mMode = Moving;
}

void SelectPathTool::updateMovingItems(const QPointF &pos,
                                       Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(pos)
    Q_UNUSED(modifiers)

    MapRenderer *renderer = mapDocument()->renderer();
    // Don't use toPoint, it rounds up
    QPoint startPos = renderer->pixelToTileCoordsInt(mStartPos, currentPathLayer()->level());
    QPoint curPos = renderer->pixelToTileCoordsInt(pos, currentPathLayer()->level());
    QPoint diff = curPos - startPos;

    foreach (PathItem *pathItem, mMovingItems) {
        pathItem->setDragging(true);
        pathItem->setDragOffset(diff);
    }
}

void SelectPathTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (PathItem *pathItem, mMovingItems)
        pathItem->setDragging(false);

    QPoint delta = mClickedItem->dragOffset();
    if (delta.isNull()) // Move is a no-op
        return;

    QUndoStack *undoStack = mapDocument()->undoStack();
    undoStack->beginMacro(tr("Move %n Path(s)", "", mMovingItems.size()));
    int i = 0;
    foreach (PathItem *pathItem, mMovingItems) {
        Path *path = pathItem->path();
        undoStack->push(new MovePath(mapDocument(), path, delta));
        ++i;
    }
    undoStack->endMacro();

    mMovingItems.clear();
}

void SelectPathTool::cancelMoving()
{
    foreach (PathItem *pathItem, mMovingItems) {
        pathItem->setDragging(false);
    }

    mMovingItems.clear();

    mMode = CancelMoving;
}
