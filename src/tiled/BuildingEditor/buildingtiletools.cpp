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

#include "buildingtiletools.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingdocument.h"
#include "buildingtilemodeview.h"
#include "buildingundoredo.h"

#include <QAction>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPolygonItem>
#include <QUndoStack>

using namespace BuildingEditor;

/////

BaseTileTool::BaseTileTool() :
    QObject(0),
    mEditor(0)
{
    TileToolManager::instance()->addTool(this);
}

void BaseTileTool::setEditor(BuildingTileModeScene *editor)
{
    mEditor = editor;

    connect(mEditor, SIGNAL(documentChanged()), SLOT(documentChanged()));
}

void BaseTileTool::setEnabled(bool enabled)
{
    if (enabled != mAction->isEnabled()) {
        mAction->setEnabled(enabled);
        TileToolManager::instance()->toolEnabledChanged(this, enabled);
    }
}

Qt::KeyboardModifiers BaseTileTool::keyboardModifiers() const
{
    return TileToolManager::instance()->keyboardModifiers();
}

bool BaseTileTool::controlModifier() const
{
    return (keyboardModifiers() & Qt::ControlModifier) != 0;
}

bool BaseTileTool::shiftModifier() const
{
    return (keyboardModifiers() & Qt::ShiftModifier) != 0;
}

void BaseTileTool::setStatusText(const QString &text)
{
    mStatusText = text;
    emit statusTextChanged();
}

BuildingFloor *BaseTileTool::floor() const
{
    return mEditor->document()->currentFloor();
}

QUndoStack *BaseTileTool::undoStack() const
{
    return mEditor->document()->undoStack();
}

QString BaseTileTool::layerName() const
{
    return mEditor->currentLayerName();
}

bool BaseTileTool::isCurrent()
{
    return TileToolManager::instance()->currentTool() == this;
}

void BaseTileTool::makeCurrent()
{
    TileToolManager::instance()->activateTool(this);
}

/////

TileToolManager *TileToolManager::mInstance = 0;

TileToolManager *TileToolManager::instance()
{
    if (!mInstance)
        mInstance = new TileToolManager;
    return mInstance;
}

TileToolManager::TileToolManager() :
    QObject(),
    mCurrentTool(0)
{
}

void TileToolManager::addTool(BaseTileTool *tool)
{
    mTools += tool;
}

void TileToolManager::activateTool(BaseTileTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
        mCurrentTool->disconnect(this);
    }

    mCurrentTool = tool;

    if (mCurrentTool) {
        connect(mCurrentTool, SIGNAL(statusTextChanged()),
                SLOT(currentToolStatusTextChanged()));
        mCurrentTool->activate();
        mCurrentTool->action()->setChecked(true);
    }

    emit currentToolChanged(mCurrentTool);
}

void TileToolManager::toolEnabledChanged(BaseTileTool *tool, bool enabled)
{
    if (enabled && !mCurrentTool)
        activateTool(tool);

    if (!enabled && tool == mCurrentTool) {
        foreach (BaseTileTool *tool2, mTools) {
            if (tool2 != tool && tool2->action()->isEnabled()) {
                activateTool(tool2);
                return;
            }
        }
        activateTool(0);
//        emit currentToolChanged(mCurrentTool);
    }
}

void TileToolManager::checkKeyboardModifiers(Qt::KeyboardModifiers modifiers)
{
    if (modifiers == mCurrentModifiers)
        return;
    mCurrentModifiers = modifiers;
    if (mCurrentTool)
        mCurrentTool->currentModifiersChanged(modifiers);
}

void TileToolManager::currentToolStatusTextChanged()
{
    emit statusTextChanged(mCurrentTool);
}

/////

DrawTileToolCursor::DrawTileToolCursor(BuildingTileModeScene *scene,
                                       QGraphicsItem *parent) :
    QGraphicsPolygonItem(parent),
    mScene(scene)
{
}

QRectF DrawTileToolCursor::boundingRect() const
{
    return mBoundingRect;
}

void DrawTileToolCursor::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *option,
                               QWidget *widget)
{
    QGraphicsPolygonItem::paint(painter, option, widget);
}

void DrawTileToolCursor::setPolygonFromTileRect(const QRect &tileRect)
{
    QPolygonF polygon = mScene->tileToScenePolygon(tileRect);
    QRectF bounds = polygon.boundingRect();

    // Add tile bounds to the shape.
    bounds.adjust(-4, -(128-32), 4, 4);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }

    QGraphicsPolygonItem::setPolygon(polygon);
}

/////

DrawTileTool *DrawTileTool::mInstance = 0;

DrawTileTool *DrawTileTool::instance()
{
    if (!mInstance)
        mInstance = new DrawTileTool();
    return mInstance;
}

DrawTileTool::DrawTileTool() :
    BaseTileTool(),
    mMouseDown(false),
    mErasing(false),
    mCursor(0)
{
    updateStatusText();
}

void DrawTileTool::documentChanged()
{
//    mCursor = 0; // it was deleted from the editor
}

void DrawTileTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());

    if (event->button() == Qt::RightButton) {
        // Right-click to cancel drawing/erasing.
        if (mMouseDown) {
            mMouseDown = false;
            mErasing = controlModifier();
            updateCursor(event->scenePos());
            updateStatusText();
            return;
        }
        if (!mEditor->currentFloorContains(tilePos, 1, 1))
            return;
        return;
    }

    mErasing = controlModifier();
    mStartTilePos = mEditor->sceneToTile(event->scenePos());
    mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds();
    mMouseDown = true;
    updateStatusText();
}

void DrawTileTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mMouseScenePos = event->scenePos();
    updateCursor(event->scenePos());
}

void DrawTileTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        QRect r = mCursorTileBounds;
        bool changed = false;
        QVector<QVector<QString> > grid;
        grid.resize(r.width());
        for (int x = 0; x < r.width(); x++)
            grid[x].resize(r.height());
        for (int x = r.left(); x <= r.right(); x++) {
            for (int y = r.top(); y <= r.bottom(); y++) {
                if (mErasing) {
                    if (floor()->grimeAt(layerName(), x, y) != QString()) {
                        changed = true;
                    }
                } else {
                    if (floor()->grimeAt(layerName(), x, y) != mTileName) {
                        grid[x - r.x()][y - r.y()] = mTileName;
                        changed = true;
                    }
                }
            }
        }
        if (changed)
            undoStack()->push(new PaintFloorTiles(mEditor->document(), floor(),
                                                  layerName(), r, grid,
                                                  mErasing ? "Erase Tiles"
                                                           : "Draw Tiles"));
        mMouseDown = false;
        mErasing = controlModifier();
        updateCursor(event->scenePos());
        updateStatusText();
    }
}

void DrawTileTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!mMouseDown) {
        mErasing = controlModifier();
        updateCursor(mMouseScenePos);
    }
}

void DrawTileTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void DrawTileTool::deactivate()
{
    if (mCursor)
        mEditor->removeItem(mCursor);
    mMouseDown = false;
}

void DrawTileTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!mCursor) {
        mCursor = new DrawTileToolCursor(mEditor);
        mCursor->setZValue(mEditor->ZVALUE_CURSOR);
    }

    if (mMouseDown) {
        mCursorTileBounds = QRect(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                                  qMin(mStartTilePos.y(), tilePos.y())),
                                  QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                                  qMax(mStartTilePos.y(), tilePos.y())));
        mCursorTileBounds &= floor()->bounds().adjusted(0, 0, 1, 1);
        updateStatusText();
    } else {
        mCursorTileBounds = QRect(tilePos, QSize(1, 1));
        mCursorTileBounds &= floor()->bounds().adjusted(0, 0, 1, 1);
    }

    // This crap is all to work around a bug when the view was scrolled and
    // then the item moves which led to areas not being repainted.  Each item
    // remembers where it was last drawn in a view, but those rectangles are
    // not updated when the view scrolls.
    QPolygonF polygon = mEditor->tileToScenePolygon(mCursorTileBounds);
    QRectF sceneRect = polygon.boundingRect().adjusted(-4, -(128-32), 4, 4);
    QRectF viewRect = mEditor->views()[0]->mapFromScene(sceneRect).boundingRect();
    if (viewRect != mCursorViewRect) {
        mCursorViewRect = viewRect;
        mCursor->update();
    }

    mCursor->setPolygonFromTileRect(mCursorTileBounds);
    if (mErasing) {
        QPen pen(QColor(255,0,0,128));
        mCursor->setBrush(QColor(0,0,0,128));
        mCursor->setPen(pen);
    } else {
        QPen pen(qRgba(0, 0, 255, 200));
        pen.setWidth(3);
        mCursor->setPen(pen);
        mCursor->setBrush(QColor(0,0,255,128));
    }
    mCursor->setVisible(mMouseDown || mEditor->currentFloorContains(tilePos, 1, 1));


    QRect r = mCursorTileBounds;
    QVector<QVector<QString> > tiles(r.width());
    if (mErasing) {
        for (int x = 0; x < r.width(); x++) {
            tiles[x].resize(r.height());
            for (int y = 0; y < r.height(); y++)
                tiles[x][y] = mEditor->buildingTileAt(r.x() + x, r.y() + y);
        }
    } else {
        for (int x = 0; x < r.width(); x++)
            tiles[x].fill(mTileName, r.height());
    }
    mEditor->setToolTiles(tiles, mCursorTileBounds.topLeft(), layerName());
}

void DrawTileTool::updateStatusText()
{
    if (mMouseDown)
        setStatusText(tr("Width,Height = %1,%2.  Right-click to cancel.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height()));
    else
        setStatusText(tr("Left-click to draw tiles.  CTRL-Left-click to erase."));
}
