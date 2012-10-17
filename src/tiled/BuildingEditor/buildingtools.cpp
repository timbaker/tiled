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

#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingundoredo.h"
#include "FloorEditor.h"

#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QUndoStack>

using namespace BuildingEditor;

/////

PencilTool *PencilTool::mInstance = 0;

PencilTool *PencilTool::instance()
{
    if (!mInstance)
        mInstance = new PencilTool();
    return mInstance;
}

PencilTool::PencilTool()
    : BaseTool()
{
}

void PencilTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
#if 0
    QImage *bmp = mEditor->currentFloor->getBMP();
#endif
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->currentFloor->layout()->roomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
#if 1
        mEditor->document()->undoStack()->push(new PaintRoom(mEditor->document(),
                                                             mEditor->currentFloor,
                                                             tilePos,
                                                             BuildingEditorWindow::instance->currentRoom()));
#else
        QString roomName = BuildingEditorWindow::instance->currentRoom();
        bmp->setPixel(tilePos, RoomDefinitionManager::instance->Get(roomName));
        mEditor->currentFloor->UpdateLayout(mEditor->currentFloor->layout->exteriorWall);
        mEditor->mCurrentFloorItem->update();
#endif
    }
    mMouseDown = true;
}

void PencilTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->currentFloor->layout()->roomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
            PaintRoom *cmd = new PaintRoom(mEditor->document(),
                                           mEditor->currentFloor,
                                           tilePos,
                                           BuildingEditorWindow::instance->currentRoom());
            cmd->setMergeable(true);
            mEditor->document()->undoStack()->push(cmd);
        }
    }
}

void PencilTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mEditor->UpdateMetaBuilding();
        mMouseDown = false;
    }
}

void PencilTool::activate()
{
    mEditor->activateTool(this);
}

void PencilTool::deactivate()
{
}

/////

EraserTool *EraserTool::mInstance = 0;

EraserTool *EraserTool::instance()
{
    if (!mInstance)
        mInstance = new EraserTool();
    return mInstance;
}

EraserTool::EraserTool()
    : BaseTool()
{
}

void EraserTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
#if 0
    QImage *bmp = mEditor->currentFloor->getBMP();
#endif
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->currentFloor->layout()->roomAt(tilePos) != 0) {
#if 1
        mEditor->document()->undoStack()->push(new EraseRoom(mEditor->document(),
                                                             mEditor->currentFloor,
                                                             tilePos));
#else
        bmp->setPixel(tilePos, qRgb(0, 0, 0));
        mEditor->currentFloor->UpdateLayout(mEditor->currentFloor->layout->exteriorWall);
        mEditor->mCurrentFloorItem->update();
#endif
    }
    mMouseDown = true;
}

void EraserTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->currentFloor->layout()->roomAt(tilePos) != 0) {
            EraseRoom *cmd = new EraseRoom(mEditor->document(),
                                                      mEditor->currentFloor,
                                                      tilePos);
            cmd->setMergeable(true);
            mEditor->document()->undoStack()->push(cmd);
        }
    }
}

void EraserTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mEditor->UpdateMetaBuilding();
        mMouseDown = false;
    }
}

void EraserTool::activate()
{
    mEditor->activateTool(this);
}

void EraserTool::deactivate()
{
}

/////

