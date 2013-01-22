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

#include "maprenderer.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPolygonItem>
#include <QStyleOptionGraphicsItem>
#include <QUndoStack>

using namespace BuildingEditor;

/////

DrawTileToolCursor::DrawTileToolCursor(BuildingBaseScene *editor,
                                       QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mEditor(editor)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
}

QRectF DrawTileToolCursor::boundingRect() const
{
    return mBoundingRect;
}

void DrawTileToolCursor::paint(QPainter *painter,
                               const QStyleOptionGraphicsItem *option,
                               QWidget *)
{
    Q_UNUSED(option)
    mEditor->drawTileSelection(painter, mRegion, mColor, option->exposedRect,
                               mEditor->currentLevel());
}

void DrawTileToolCursor::setColor(const QColor &color)
{
    if (color != mColor) {
        mColor = color;
        update();
    }
}

void DrawTileToolCursor::setTileRegion(const QRegion &tileRgn)
{
    if (tileRgn != mRegion) {
        QPolygonF polygon = mEditor->tileToScenePolygon(tileRgn.boundingRect(),
                                                        mEditor->currentLevel());
        QRectF bounds = polygon.boundingRect();

        // Add tile bounds and pen width to the shape.
        bounds.adjust(-4, -(128-32), 5, 5);

        if (bounds != mBoundingRect) {
            // NOTE-SCENE-CORRUPTION
            // Schedule a redraw of the current area.  Although prepareGeometryChange()
            // marks the current area as needing to be redrawn by setting the
            // paintedViewBoundingRectsNeedRepaint flag, any additional call to
            // QGraphicsScene::update will result in paintedViewBoundingRects getting
            // set the *new* location of this item before _q_processDirtyItems gets
            // called, so the current area never gets repainted.
            if (scene())
                scene()->update(mBoundingRect);

            prepareGeometryChange();
            mBoundingRect = bounds;
        }

        mRegion = tileRgn;

        const QRect changedArea = tileRgn.xored(mRegion).boundingRect();
        update(mEditor->tileToScenePolygon(changedArea,
                                           mEditor->currentLevel()).boundingRect());
    }
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
    BaseTool(),
    mMouseDown(false),
    mMouseMoved(false),
    mErasing(false),
    mCursor(0),
    mCapturing(false),
    mCaptureTiles(0)
{
    updateStatusText();
}

void DrawTileTool::documentChanged()
{
//    mCursor = 0; // it was deleted from the editor
}

void DrawTileTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
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
        if (!mEditor->currentFloorContains(tilePos, 1, 1))
            return;
        beginCapture();
        return;
    }

    mStartScenePos = event->scenePos();
    mStartTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
    mMouseDown = true;
    mMouseMoved = false;
    mErasing = controlModifier();
    if (!mCaptureTiles)
        mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
    updateCursor(mStartScenePos);
    updateStatusText();
}

void DrawTileTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    bool mouseMoved = mMouseMoved;
    if (mMouseDown && !mMouseMoved) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance())
            mMouseMoved = true;
    }
    mMouseScenePos = event->scenePos();
    updateCursor(event->scenePos(), mouseMoved != mMouseMoved);
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
        if (!r.isEmpty()) {
            bool changed = false;
            FloorTileGrid *tiles = floor()->grimeAt(layerName(), r);
            QRegion rgn;
            if (!mCaptureTiles) {
                QString tileName = mErasing ? QString() : mTileName;
                changed = tiles->replace(tileName);
                rgn = QRegion(r);
            } else if (mErasing) {
                // Erase in the capture-tiles region
                changed = tiles->replace(mCaptureTilesRgn, QString());
                rgn = mCaptureTilesRgn.translated(mCursorTileBounds.topLeft()) & r;
            } else {
                QRect clipRect = r.translated(-mCursorTileBounds.topLeft());
                FloorTileGrid *clipped = mCaptureTiles->clone(clipRect, mCaptureTilesRgn);
                rgn = mCaptureTilesRgn.intersected(clipRect);
                rgn.translate(-clipRect.topLeft());
                changed = tiles->replace(rgn, QPoint(0, 0), clipped);
                delete clipped;
                rgn = mCaptureTilesRgn.translated(mCursorTileBounds.topLeft()) & r;
            }
            if (changed)
                undoStack()->push(new PaintFloorTiles(mEditor->document(), floor(),
                                                      layerName(), rgn, r.topLeft(),
                                                      tiles,
                                                      mErasing ? "Erase Tiles"
                                                               : "Draw Tiles"));
        }
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
        updateStatusText();
    }
}

void DrawTileTool::setTile(const QString &tileName)
{
    mTileName = tileName;
    clearCaptureTiles();
}

void DrawTileTool::setCaptureTiles(FloorTileGrid *tiles, const QRegion &rgn)
{
    Q_ASSERT(tiles->bounds().contains(rgn.boundingRect()));

    clearCaptureTiles();
    mCaptureTiles = tiles;
    mCaptureTilesRgn = rgn/*.translated(-rgn.boundingRect().topLeft())*/;
    updateCursor(mMouseScenePos);
}

void DrawTileTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
    updateStatusText();
}

void DrawTileTool::deactivate()
{
    if (mCursor) {
        mEditor->removeItem(mCursor);
        mEditor->clearToolTiles();
    }
    mMouseDown = false;
}

void DrawTileTool::beginCapture()
{
    if (mMouseDown)
        return;

    clearCaptureTiles();

    mEditor->clearToolTiles();

    mCapturing = true;
    mMouseDown = true;
    mMouseMoved = false;

    mStartScenePos = mMouseScenePos;
    mStartTilePos = mEditor->sceneToTile(mMouseScenePos, mEditor->currentLevel());
    mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
    updateStatusText();

    updateCursor(mMouseScenePos);
}

void DrawTileTool::endCapture()
{
    if (!mCapturing)
        return;

    if (!mMouseMoved)
        clearCaptureTiles();
    else {
        mCaptureTiles = floor()->grimeAt(layerName(), mCursorTileBounds);
        mCaptureTilesRgn = QRegion(mCaptureTiles->bounds());
    }

    mCapturing = false;
    mMouseDown = false;

    updateCursor(mMouseScenePos);
    updateStatusText();
}

void DrawTileTool::clearCaptureTiles()
{
    delete mCaptureTiles;
    mCaptureTiles = 0;
}

void DrawTileTool::updateCursor(const QPointF &scenePos, bool force)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos, mEditor->currentLevel());
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
        mCursorTileBounds &= floor()->bounds(1, 1); // before updateStatusText
        updateStatusText();
    } else {
        mCursorTileBounds = QRect(tilePos, QSize(1, 1));
        mCursorTileBounds &= floor()->bounds(1, 1);
    }

    if (mCaptureTiles) {
        mCursorTileBounds.setLeft(tilePos.x() - mCaptureTiles->width() / 2);
        mCursorTileBounds.setTop(tilePos.y() - mCaptureTiles->height() / 2);
        mCursorTileBounds.setWidth(mCaptureTiles->width());
        mCursorTileBounds.setHeight(mCaptureTiles->height());

        // Set the tool-tiles to represent what would be seen if the tiles
        // were painted at the current location.
        FloorTileGrid *tiles = floor()->grimeAt(layerName(), mCursorTileBounds);
        if (mErasing)
            tiles->replace(mCaptureTilesRgn, QString());
        else
            tiles->replace(mCaptureTilesRgn, QPoint(0, 0), mCaptureTiles);
        for (int x = 0; x < mCursorTileBounds.width(); x++) {
            for (int y = 0; y < mCursorTileBounds.height(); y++) {
                // Building-generated tiles can't be replaced with nothing.
                if (tiles->at(x, y).isEmpty())
                    tiles->replace(x, y, mEditor->buildingTileAt(
                                        mCursorTileBounds.x() + x,
                                        mCursorTileBounds.y() + y));
            }
        }
        mEditor->setToolTiles(tiles, mCursorTileBounds.topLeft(), layerName());
        delete tiles;
    }

    mCursor->setTileRegion(mCaptureTiles ?
                               mCaptureTilesRgn.translated(mCursorTileBounds.topLeft())
                             : mCursorTileBounds);
    if (mErasing) {
        mCursor->setColor(QColor(0,0,0,64));
    } else if (mMouseDown) {
        mCursor->setColor(QColor(0,0,255,64));
    } else {
        QColor highlight = QApplication::palette().highlight().color();
        highlight.setAlpha(64);
        mCursor->setColor(highlight);
    }

    if (mCapturing && !mMouseMoved)
        mCursor->setVisible(false);
    else
        mCursor->setVisible(mMouseDown || mCaptureTiles
                            || mEditor->currentFloorContains(tilePos, 1, 1));

    if (mCapturing || mCaptureTiles)
        return;

    if (mCursorTileBounds.isEmpty()) {
        mEditor->clearToolTiles();
        return;
    }

    QRect r = mCursorTileBounds;
    FloorTileGrid tiles(r.width(), r.height());
    if (mErasing) {
        for (int x = 0; x < r.width(); x++) {
            for (int y = 0; y < r.height(); y++)
                tiles.replace(x, y, mEditor->buildingTileAt(r.x() + x, r.y() + y));
        }
    } else {
        tiles.replace(mTileName);
    }
    mEditor->setToolTiles(&tiles, mCursorTileBounds.topLeft(), layerName());
}

void DrawTileTool::updateStatusText()
{
    if (mCapturing && !mMouseMoved) {
        setStatusText(tr("Drag to capture tiles.  Release button to cancel."));
    } else if (mCapturing)
        setStatusText(tr("Width,Height = %1,%2.  Release button to capture tiles.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height()));
    else if (mMouseDown && mCaptureTiles)
        setStatusText(tr("Release to %1 tiles.  Right-click to cancel.")
                      .arg(QLatin1String(mErasing ? "erase" : "draw")));
    else if (mMouseDown)
        setStatusText(tr("Width,Height = %1,%2.  Release button to %3 tiles.  Right-click to cancel.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height())
                      .arg(QLatin1String(mErasing ? "erase" : "draw")));
    else if (controlModifier())
        setStatusText(tr("Left-click to erase tiles.  Right-click to capture tiles."));
    else
        setStatusText(tr("Left-click to draw tiles.  CTRL=erase.  Right-click to capture tiles."));
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
    BaseTool(),
    mMouseDown(false),
    mMouseMoved(false),
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

        mStartScenePos = event->scenePos();
        mStartTilePos = mEditor->sceneToTile(event->scenePos(), mEditor->currentLevel());
        mCursorTileBounds = QRect(mStartTilePos, QSize(1, 1)) & floor()->bounds(1, 1);
        mMouseDown = true;
        mMouseMoved = mSelectionMode != Replace;
        updateCursor(mStartScenePos);
        updateStatusText();
    }
}

void SelectTileTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    bool mouseMoved = mMouseMoved;
    if (mMouseDown && !mMouseMoved) {
        const int dragDistance = (mStartScenePos - event->scenePos()).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance())
            mMouseMoved = true;
    }
    mMouseScenePos = event->scenePos();
    updateCursor(event->scenePos(), mouseMoved != mMouseMoved);
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

        if (!mMouseMoved)
            selection = QRegion();

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
    QPoint tilePos = mEditor->sceneToTile(scenePos, mEditor->currentLevel());
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
        mCursorTileBounds &= floor()->bounds(1, 1);
        updateStatusText();
    } else {
        mCursorTileBounds = QRect(tilePos, QSize(1, 1));
        mCursorTileBounds &= floor()->bounds(1, 1);
    }

    mCursor->setTileRegion(mCursorTileBounds);

    if ((mMouseDown && mSelectionMode == Subtract) || (!mMouseDown && controlModifier())) {
        mCursor->setColor(QColor(0,0,0,128));
    } else {
        mCursor->setColor(QColor(0,0,255,128));
    }

    mCursor->setVisible(mMouseDown ? mMouseMoved : mEditor->currentFloorContains(tilePos, 1, 1));
}

void SelectTileTool::updateStatusText()
{
    if (mMouseDown && !mMouseMoved) {
        setStatusText(tr("Drag to modify selection.  Release button to clear selection.  Right-click to cancel."));
    } else if (mMouseDown)
        setStatusText(tr("Width,Height = %1,%2.  Right-click to cancel.")
                      .arg(mCursorTileBounds.width())
                      .arg(mCursorTileBounds.height()));
    else
        setStatusText(tr("Left-click-drag to select.  CTRL=subtract.  SHIFT=add.  CTRL+SHIFT=intersect."));
}
