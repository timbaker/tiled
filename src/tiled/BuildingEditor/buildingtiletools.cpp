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

BuildingDocument *BaseTileTool::document() const
{
    return mEditor->document();
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
    mCursor(0),
    mCapturing(false)
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
        beginCapture();
        return;
    }

    if (!mCaptureTiles.isEmpty()) {
        mMouseDown = true;
        return;
    }

    mErasing = controlModifier();
    mStartTilePos = mEditor->sceneToTile(event->scenePos());
    mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
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
    if (mCapturing) {
        endCapture();
        return;
    }
    if (mMouseDown) {
        QRect r = mCursorTileBounds & floor()->bounds(1, 1);
        bool changed = false;
        QVector<QVector<QString> > grid;
        if (mCaptureTiles.isEmpty()) {
            QString tileName = mErasing ? QString() : mTileName;
            grid.resize(r.width());
            for (int x = 0; x < r.width(); x++)
                grid[x].fill(tileName, r.height());
            for (int x = r.left(); x <= r.right(); x++) {
                for (int y = r.top(); y <= r.bottom(); y++) {
                    if (floor()->grimeAt(layerName(), x, y) != tileName) {
                        changed = true;
                        break;
                    }
                }
            }
        } else {
            grid = floor()->grimeAt(layerName(), r);
            changed = mergeTiles(clipTiles(mCursorTileBounds.topLeft(), mCaptureTiles, r), grid);
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
        mCursorTilePos = QPoint(-666,-666);
        updateCursor(mMouseScenePos);
    }
}

void DrawTileTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
    updateStatusText();
}

void DrawTileTool::deactivate()
{
    if (mCursor)
        mEditor->removeItem(mCursor);
    mMouseDown = false;
}

void DrawTileTool::beginCapture()
{
    if (mMouseDown)
        return;

    mEditor->clearToolTiles();

    mCapturing = true;
    mMouseDown = true;

    mCaptureTiles.clear();

    mStartTilePos = mEditor->sceneToTile(mMouseScenePos);
    mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
    updateStatusText();

    updateCursor(mMouseScenePos);
}

void DrawTileTool::endCapture()
{
    if (!mCapturing)
        return;

    mCaptureTiles = floor()->grimeAt(layerName(), mCursorTileBounds);

    mCapturing = false;
    mMouseDown = false;

    updateCursor(mMouseScenePos);
    updateStatusText();
}

// Put non-empty tiles in 'above' into 'below'.
// Return true if any tiles in below were replaced with a different one.
bool DrawTileTool::mergeTiles(const QVector<QVector<QString> > &above,
                              QVector<QVector<QString> > &below)
{
    bool changed = false;
    for (int x = 0; x < above.size(); x++) {
        for (int y = 0; y < above[x].size(); y++) {
            if (!above[x][y].isEmpty() && above[x][y] != below[x][y]) {
                below[x][y] = above[x][y];
                changed = true;
            }
        }
    }
    return changed;
}

QVector<QVector<QString> > DrawTileTool::clipTiles(const QPoint &p,
                                                   const QVector<QVector<QString> > &tiles,
                                                   const QRect &bounds)
{
    QVector<QVector<QString> > ret;
    QRect r1 = QRect(p.x(), p.y(), tiles.size(), tiles[0].size());
    QRect r = bounds & r1;
    if (r.isValid()) {
        ret.resize(r.width());
        for (int x = r.x(); x <= r.right(); x++) {
            ret[x-r.x()].resize(r.height());
            for (int y = r.y(); y <= r.bottom(); y++)
                ret[x-r.x()][y-r.y()] = tiles[x-p.x()][y-p.y()];
        }
    }
    return ret;
}

void DrawTileTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (tilePos == mCursorTilePos)
        return;
    mCursorTilePos = tilePos;

    if (!mCursor) {
        mCursor = new DrawTileToolCursor(mEditor);
        mCursor->setZValue(mEditor->ZVALUE_CURSOR);
    }

    if (mMouseDown) {
        mCursorTileBounds = QRect(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                                  qMin(mStartTilePos.y(), tilePos.y())),
                                  QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                                  qMax(mStartTilePos.y(), tilePos.y())));
        updateStatusText();
    } else {
        mCursorTileBounds = QRect(tilePos, QSize(1, 1));
    }
    mCursorTileBounds &= floor()->bounds(1, 1);

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

    if (!mCaptureTiles.isEmpty()) {
        mCursorTileBounds.setLeft(tilePos.x() - mCaptureTiles.size() / 2);
        mCursorTileBounds.setTop(tilePos.y() - mCaptureTiles[0].size() / 2);
        mCursorTileBounds.setWidth(mCaptureTiles.size());
        mCursorTileBounds.setHeight(mCaptureTiles[0].size());
        QVector<QVector<QString> > merged = floor()->grimeAt(layerName(), mCursorTileBounds);
        mergeTiles(mCaptureTiles, merged);
        mEditor->setToolTiles(merged, mCursorTileBounds.topLeft(), layerName());
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
    mCursor->setVisible(mMouseDown || !mCaptureTiles.isEmpty() || mEditor->currentFloorContains(tilePos, 1, 1));

    if (mCapturing || !mCaptureTiles.isEmpty())
        return;

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

/////

SelectTileTool *SelectTileTool::mInstance = 0;

SelectTileTool *SelectTileTool::instance()
{
    if (!mInstance)
        mInstance = new SelectTileTool();
    return mInstance;
}

SelectTileTool::SelectTileTool() :
    BaseTileTool(),
    mMouseDown(false),
    mSelectionMode(Replace),
    mCursor(0)
{
    updateStatusText();
}

void SelectTileTool::documentChanged()
{
//    mCursor = 0; // it was deleted from the editor
}

void SelectTileTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (button == Qt::RightButton) {
        // Right-click to cancel.
        if (mMouseDown) {
            mMouseDown = false;
            updateCursor(event->scenePos());
            updateStatusText();
            return;
        }
        return;
    }

    if (button == Qt::LeftButton) {
        if (modifiers == Qt::ControlModifier) {
            mSelectionMode = Subtract;
        } else if (modifiers == Qt::ShiftModifier) {
            mSelectionMode = Add;
        } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
            mSelectionMode = Intersect;
        } else {
            mSelectionMode = Replace;
        }

        mStartTilePos = mEditor->sceneToTile(event->scenePos());
        mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
        mMouseDown = true;
        updateStatusText();
    }
}

void SelectTileTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mMouseScenePos = event->scenePos();
    updateCursor(event->scenePos(), false);
}

void SelectTileTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        QRegion selection = document()->tileSelection();
        const QRect area(mCursorTileBounds);

        switch (mSelectionMode) {
        case Replace:   selection = area; break;
        case Add:       selection += area; break;
        case Subtract:  selection -= area; break;
        case Intersect: selection &= area; break;
        }

        if (selection != document()->tileSelection())
            undoStack()->push(new ChangeTileSelection(document(), selection));

        mMouseDown = false;
        updateCursor(event->scenePos());
        updateStatusText();
    }
}

void SelectTileTool::currentModifiersChanged(Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    if (!mMouseDown) {
        updateCursor(mMouseScenePos);
    }
}

void SelectTileTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
    updateStatusText();
}

void SelectTileTool::deactivate()
{
    if (mCursor)
        mEditor->removeItem(mCursor);
    mMouseDown = false;
}

void SelectTileTool::updateCursor(const QPointF &scenePos, bool force)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!force && (tilePos == mCursorTilePos))
        return;
    mCursorTilePos = tilePos;

    if (!mCursor) {
        mCursor = new DrawTileToolCursor(mEditor);
        mCursor->setZValue(mEditor->ZVALUE_CURSOR);
    }

    if (mMouseDown) {
        mCursorTileBounds = QRect(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                                  qMin(mStartTilePos.y(), tilePos.y())),
                                  QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                                  qMax(mStartTilePos.y(), tilePos.y())));
        updateStatusText();
    } else {
        mCursorTileBounds = QRect(tilePos, QSize(1, 1));
    }
    mCursorTileBounds &= floor()->bounds(1, 1);

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

    if ((mMouseDown && mSelectionMode == Subtract) || (!mMouseDown && controlModifier())) {
        QPen pen(QColor(255,0,0,128));
        mCursor->setPen(pen);
        mCursor->setBrush(QColor(0,0,0,128));
    } else {
        QPen pen(qRgba(0, 0, 255, 200));
        pen.setWidth(3);
        mCursor->setPen(pen);
        mCursor->setBrush(QColor(0,0,255,128));
    }

    mCursor->setVisible(mMouseDown || mEditor->currentFloorContains(tilePos, 1, 1));
}

void SelectTileTool::updateStatusText()
{
    if (mMouseDown)
        setStatusText(tr("Width,Height = %1,%2.  Right-click to cancel.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height()));
    else
        setStatusText(tr("Left-click-drag to select.  CTRL=subtract.  SHIFT=add.  CTRL+SHIFT=intersect."));
}
