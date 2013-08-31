/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "curbtool.h"

#include "bmpblender.h"
#include "bmptool.h"
#include "curbtooldialog.h"
#include "erasetiles.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"

#include "BuildingEditor/buildingtiles.h"

#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"

#include <qmath.h>
#include <QDebug>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

SINGLETON_IMPL(CurbTool)

CurbTool::CurbTool(QObject *parent) :
    AbstractTileTool(tr("Draw Curb"),
                     QIcon(QLatin1String(":/images/22x22/curb-tool.png")),
                     QKeySequence(/*tr("E")*/),
                     parent),
    mScene(0),
    mToolTileLayerGroup(0),
    mInitialClick(false),
    mCurb(0),
    mSuppressBlendTiles(false),
    mCursorItem(new QGraphicsPolygonItem)
{
    mCursorItem->setPen(QPen(QColor(0,255,0,96), 1));
    mCursorItem->setBrush(QColor(0,255,0,64));
}

void CurbTool::activate(MapScene *scene)
{
    Q_ASSERT(mScene == 0);
    mScene = scene;
    mInitialClick = false;
    scene->addItem(mCursorItem);
    AbstractTileTool::activate(scene);
    CurbToolDialog::instance()->setVisibleLater(true);
}

void CurbTool::deactivate(MapScene *scene)
{
    Q_ASSERT(mScene == scene);
    if (mToolTileLayerGroup != 0) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = 0;
    }

    CurbToolDialog::instance()->setVisibleLater(false);
    scene->removeItem(mCursorItem);
    mScene = 0;
    AbstractTileTool::deactivate(scene);
}

static QVector<QVector<Cell> > tileLayerToVector(TileLayer &tl)
{
    QVector<QVector<Cell> > ret(tl.width());
    for (int x = 0; x < tl.width(); x++)
        ret[x].resize(tl.height());
    for (int y = 0; y < tl.height(); y++) {
        for (int x = 0; x < tl.width(); x++) {
            ret[x][y] = tl.cellAt(x, y);
        }
    }
    return ret;
}

void CurbTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    QPointF tilePosF;
    Corner corner;
    toCorner(pos, tilePosF, corner);
    QPoint tilePos(qFloor(tilePosF.x()), qFloor(tilePosF.y()));

    CompositeLayerGroup *lg = mapDocument()->mapComposite()->layerGroupForLevel(mapDocument()->currentLevel());
    if (mToolTileLayerGroup != 0) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = 0;
    }
    QPoint topLeft;
    QVector<QVector<Cell> > toolTiles;

    if (!mInitialClick) {
        qreal dx = 0, dy = 0;
        if (corner == CornerNE || corner == CornerSE) dx = 0.5;
        if (corner == CornerSW || corner == CornerSE) dy = 0.5;
        QPolygonF poly = renderer->tileToPixelCoords(QRectF(tilePos.x() + dx, tilePos.y() + dy, 0.5, 0.5), layer->level());
        mCursorItem->setPolygon(poly);
    } else {

        {
            qreal dx = tilePosF.x() - mStartTilePosF.x();
            qreal dy = tilePosF.y() - mStartTilePosF.y();
            if (qAbs(dx) > qAbs(dy)) {
                tilePosF.setY(mStartTilePosF.y());
                tilePos.setY(mStartTilePos.y());
                if ((corner == CornerNW) && (mStartCorner == CornerSW || mStartCorner == CornerSE))
                    corner = CornerSW;
                else if ((corner == CornerNE) && (mStartCorner == CornerSW || mStartCorner == CornerSE))
                    corner = CornerSE;
                else if ((corner == CornerSW) && (mStartCorner == CornerNW || mStartCorner == CornerNE))
                    corner = CornerNW;
                else if ((corner == CornerSE) && (mStartCorner == CornerNW || mStartCorner == CornerNE))
                    corner = CornerNE;
            } else {
                tilePosF.setX(mStartTilePosF.x());
                tilePos.setX(mStartTilePos.x());
                if ((corner == CornerNW) && (mStartCorner == CornerNE || mStartCorner == CornerSE))
                    corner = CornerNE;
                else if ((corner == CornerNE) && (mStartCorner == CornerNW || mStartCorner == CornerSW))
                    corner = CornerNW;
                else if ((corner == CornerSW) && (mStartCorner == CornerNE || mStartCorner == CornerSE))
                    corner = CornerSE;
                else if ((corner == CornerSE) && (mStartCorner == CornerSW || mStartCorner == CornerNW))
                    corner = CornerSW;
            }
            topLeft = QPoint(qMin(mStartTilePos.x(), tilePos.x()), qMin(mStartTilePos.y(), tilePos.y()));
            bool far = (modifiers & Qt::AltModifier) != 0;
            TileLayer stamp(QString(), 0, 0, qAbs(tilePos.x() - mStartTilePos.x()) + 2, qAbs(tilePos.y() - mStartTilePos.y()) + 2);
            QMap<QString,QRegion> eraseRgn, noBlendRgn;
            if (modifiers & Qt::ControlModifier) {
                stamp.setCells(-topLeft.x(), -topLeft.y(), currentTileLayer(), QRegion(0, 0, stamp.width(), stamp.height()));
                raiseLowerChanges(mStartTilePosF, tilePosF, stamp, eraseRgn);
                QRegion region = eraseRgn.size() ? eraseRgn.values().first() : QRegion();
                stamp.erase(region.translated(-topLeft));
            } else {
                getMapChanges(mStartTilePosF, mStartCorner, tilePosF, corner, far, stamp, eraseRgn, noBlendRgn);
                // FIXME: should handle eraseRgn and noBlendRgn (noBlend in multiple layers)
                stamp.resize(QSize(stamp.width() - 1, stamp.height() - 1), QPoint());
            }
            toolTiles = tileLayerToVector(stamp);
        }

        qreal dx = 0, dy = 0;
        if (corner == CornerNE || corner == CornerSE) dx = 0.5;
        if (corner == CornerSW || corner == CornerSE) dy = 0.5;
        QRectF tileRectEnd(tilePos.x() + dx, tilePos.y() + dy, 0.5, 0.5);
        dx = dy = 0;
        if (mStartCorner == CornerNE || mStartCorner == CornerSE) dx = 0.5;
        if (mStartCorner == CornerSW || mStartCorner == CornerSE) dy = 0.5;
        QRectF tileRectStart(mStartTilePos.x() + dx, mStartTilePos.y() + dy, 0.5, 0.5);
        QPolygonF poly = renderer->tileToPixelCoords(tileRectEnd | tileRectStart, layer->level());
        mCursorItem->setPolygon(poly);
    }

    if (toolTiles.size()) {
        lg->setToolTiles(toolTiles, topLeft, currentTileLayer());
        mToolTilesRect = renderer->boundingRect(QRect(topLeft.x(), topLeft.y(), toolTiles.size(), toolTiles[0].size()),
                mapDocument()->currentLevel()).adjusted(-3, -(128-32) - 3, 3, 3); // use mMap->drawMargins()
        mToolTileLayerGroup = lg;
        mScene->update(mToolTilesRect);
    }

    AbstractTileTool::mouseMoved(pos, modifiers);
}

void CurbTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF tilePosF;
        Corner corner;
        toCorner(event->scenePos(), tilePosF, corner);
        if (mInitialClick) {
        } else {
            mStartTilePosF = tilePosF;
            mStartTilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
            mStartCorner = corner;
            mInitialClick = true;
        }
//        mCursorItem->setPen(QPen(Qt::blue, 2));
    }

    if (event->button() == Qt::RightButton) {
        if (mInitialClick) {
            mInitialClick = false;
            mouseMoved(event->scenePos(), event->modifiers());
        }
    }
}

void CurbTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mInitialClick) {
        QPointF tilePosF;
        Corner corner;
        toCorner(event->scenePos(), tilePosF, corner);
        // Click a second time to draw
        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        if (qAbs(dx) > qAbs(dy))
            tilePosF.setY(mStartTilePosF.y());
        else
            tilePosF.setX(mStartTilePosF.x());
        mapDocument()->undoStack()->beginMacro(tr("Draw Curb"));
        if (event->modifiers() & Qt::ControlModifier) {
            raiseLower(mStartTilePosF, tilePosF);
        } else {
            drawEdge(mStartTilePosF, mStartCorner, tilePosF, corner,
                     (event->modifiers() & Qt::AltModifier) != 0);
        }
        mapDocument()->undoStack()->endMacro();
        mInitialClick = false;
        mouseMoved(event->scenePos(), event->modifiers());
    }

//    AbstractTileTool::mouseReleased(event);
}

void CurbTool::languageChanged()
{
    setName(tr("Draw Curb"));
    setShortcut(QKeySequence(/*tr("E")*/));
}

void CurbTool::setCurb(Curb *curb)
{
    mCurb = curb;
}

void CurbTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
}

QVector<Tile *> CurbTool::resolveTiles(Curb *curb)
{
    QVector<Tile *> ret(Curb::ShapeCount, (Tile*)-1);
    if (!mCurb)
        return ret;
    Map *map = mapDocument()->map();
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, map->tilesets())
        tilesets[ts->name()] = ts;

    for (int i = 0; i < Curb::ShapeCount; i++) {
        if (curb->mTileNames[i].isEmpty())
            continue;
        QString tilesetName;
        int index;
        if (BuildingEditor::BuildingTilesMgr::instance()->parseTileName(curb->mTileNames[i], tilesetName, index)) {
            if (tilesets.contains(tilesetName))
                ret[i] = tilesets[tilesetName]->tileAt(index);
        }
    }

    return ret;
}

void CurbTool::toCorner(const QPointF &scenePos, QPointF &tilePosF, Corner &corner)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    tilePosF = renderer->pixelToTileCoords(scenePos, layer->level());
    QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
    qreal dW = m.x(), dN = m.y()/*, dE = 1.0 - dW, dS = 1.0 - dN*/;
    if (dW < 0.5 && dN < 0.5) {
        corner = CornerNW;
        return;
    }
    if (dW >= 0.5 && dN >= 0.5) {
        corner = CornerSE;
        return;
    }
    if (dW < 0.5) {
        corner = CornerSW;
        return;
    }
    corner = CornerNE;
}

void CurbTool::drawEdge(const QPointF &start, Corner cornerStart,
                        const QPointF &end, Corner cornerEnd,
                        bool far)
{
#if 1
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());
    QPoint origin(qMin(sx, ex), qMin(sy, ey));
    int width, height;
    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y()))
        width = qAbs(ex - sx) + 2, height = 2;
    else
        width = 2, height = qAbs(ey - sy) + 2;

    TileLayer stamp(QString(), 0, 0, width, height);
    QMap<QString,QRegion> eraseRgn, noBlendRgn;
    getMapChanges(start, cornerStart, end, cornerEnd, far, stamp, eraseRgn, noBlendRgn);

    if (!stamp.isEmpty()) {
        PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), currentTileLayer(),
                                                 origin.x(), origin.y(), &stamp,
                                                 stamp.region().translated(origin),
                                                 false);
//      cmd->setMergeable(mergeable);
        mapDocument()->undoStack()->push(cmd);
        mapDocument()->emitRegionEdited(stamp.region(), currentTileLayer());
    }

    foreach (QString layerName, eraseRgn.keys()) {
        int index = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index >= 0) {
            TileLayer *tl = mapDocument()->map()->layerAt(index)->asTileLayer();
            EraseTiles *cmd = new EraseTiles(mapDocument(), tl, eraseRgn[layerName]);
            mapDocument()->undoStack()->push(cmd);
        }
    }

    foreach (QString layerName, noBlendRgn.keys()) {
        QRegion rgn = noBlendRgn[layerName];
        QRect r = rgn.boundingRect();
        MapNoBlend *noBlend = mapDocument()->map()->noBlend(layerName);
        MapNoBlend bits(QString(), r.width(), r.height());
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++)
                if (rgn.contains(QPoint(x, y)))
                    bits.set(x - r.x(), y - r.y(), true);
        }
        PaintNoBlend *cmd = new PaintNoBlend(mapDocument(), noBlend, bits, rgn);
        mapDocument()->undoStack()->push(cmd);
    }
#else
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());

    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y())) {
        // South edge
        // East-to-West
        if (start.x() > end.x()) {
            for (int x = sx; x >= ex; x--) {
                bool half = ((sx == ex) && (cornerStart == cornerEnd))
                        || ((sx != ex && x == sx) && (cornerStart == CornerSW || cornerStart == CornerNW))
                        || ((sx != ex && x == ex) && (cornerEnd == CornerNE || cornerEnd == CornerSE));
                drawEdgeTile(x, sy, EdgeS, half, far);
            }
            return;
        }
        // West-to-East
        for (int x = sx; x <= ex; x++) {
            bool half = ((sx == ex) && (cornerStart == cornerEnd))
                    || ((sx != ex && x == sx) && (cornerStart == CornerNE || cornerStart == CornerSE))
                    || ((sx != ex && x == ex) && (cornerEnd == CornerSW || cornerEnd == CornerNW));
            drawEdgeTile(x, sy, EdgeS, half, far);
        }
    } else {
        // East edge
        // South-to-North
        if (start.y() > end.y()) {
            for (int y = sy; y >= ey; y--) {
                bool half = ((sy == ey) && (cornerStart == cornerEnd))
                        || ((sy != ey && y == sy) && (cornerStart == CornerNW || cornerStart == CornerNE))
                        || ((sy != ey && y == ey) && (cornerEnd == CornerSW || cornerEnd == CornerSE));
                drawEdgeTile(sx, y, EdgeE, half, far);
            }
            return;
        }
        // North-to-South
        for (int y = sy; y <= ey; y++) {
            bool half = ((sy == ey) && (cornerStart == cornerEnd))
                    || ((sy != ey && y == sy) && (cornerStart == CornerSW || cornerStart == CornerSE))
                    || ((sy != ey && y == ey) && (cornerEnd == CornerNW || cornerEnd == CornerNE));
            drawEdgeTile(sx, y, EdgeE, half, far);
        }
    }
#endif
}

void CurbTool::drawEdgeTile(const QPoint &origin, int x, int y, Edge edge, bool half, bool far,
                            TileLayer &stamp, QMap<QString,QRegion> &eraseRgn,
                            QMap<QString,QRegion> &noBlendRgn)
{
//    qDebug() << "CurbTool::drawEdgeTile" << x << y << edge;

    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return;

    Tile *tile = 0;
    Tile *CURRENT = tileLayer->cellAt(x, y).tile;
    Tile *currentW = tileLayer->contains(x - 1, y) ? tileLayer->cellAt(x - 1, y).tile : 0;
    Tile *currentE = tileLayer->contains(x + 1, y) ? tileLayer->cellAt(x + 1, y).tile : 0;
    Tile *currentN = tileLayer->contains(x, y - 1) ? tileLayer->cellAt(x, y - 1).tile : 0;
    Tile *currentS = tileLayer->contains(x, y + 1) ? tileLayer->cellAt(x, y + 1).tile : 0;

    if (stamp.contains(x - origin.x() - 1, y - origin.y()) && stamp.cellAt(x - origin.x() - 1, y - origin.y()).tile)
        currentW = stamp.cellAt(x - origin.x() - 1, y - origin.y()).tile;
    if (stamp.contains(x - origin.x() + 1, y - origin.y()) && stamp.cellAt(x - origin.x() + 1, y - origin.y()).tile)
        currentE = stamp.cellAt(x - origin.x() + 1, y - origin.y()).tile;
    if (stamp.contains(x - origin.x(), y - origin.y() - 1) && stamp.cellAt(x - origin.x(), y - origin.y() - 1).tile)
        currentN = stamp.cellAt(x - origin.x(), y - origin.y() - 1).tile;
    if (stamp.contains(x - origin.x(), y - origin.y() + 1) && stamp.cellAt(x - origin.x(), y - origin.y() + 1).tile)
        currentS = stamp.cellAt(x - origin.x(), y - origin.y() + 1).tile;

    QVector<Tile*> tiles = resolveTiles(mCurb);
    Tile *farE = tiles[Curb::FarE];
    Tile *farS = tiles[Curb::FarS];
    Tile *farSE = tiles[Curb::FarSE];
    Tile *farJoinSE = tiles[Curb::FarJoinSE];
    Tile *farEJoinS = tiles[Curb::FarEJoinS];
    Tile *farSJoinE = tiles[Curb::FarSJoinE];
    Tile *nearE = tiles[Curb::NearE];
    Tile *nearS = tiles[Curb::NearS];
    Tile *nearSE = tiles[Curb::NearSE];
    Tile *nearJoinSE = tiles[Curb::NearJoinSE];
    Tile *nearEJoinS = tiles[Curb::NearEJoinS];
    Tile *nearSJoinE = tiles[Curb::NearSJoinE];

    if (edge == EdgeE) {
        // far
        if (!half && (currentE == nearS || currentE == nearSJoinE || currentE == nearSE))
            tile = farEJoinS;
#if 0
        else if (currentS == nearE || currentS == nearSE)
            tile = farSJoinE;
#endif
        else if (!half && (currentW == farJoinSE || currentW == farS))
            tile = farSE;
        else if (half && (currentE == farS || currentE == farSE))
            tile = farJoinSE;
        else if (half && (currentW == farS || currentW == farJoinSE || CURRENT == farS))
            tile = farSJoinE;
        else if (!half && (currentS == farE || currentS == farSE || currentS == farEJoinS
                           || currentN == farJoinSE || currentN == farE || currentN == nearSJoinE))
            tile = farE;
        else if (!half && (CURRENT == farS))
            tile = farSE;

        // near
        else if (!half && (currentE == farS || currentE == farSJoinE))
            tile = nearEJoinS;
        else if (half && (currentS == nearE || currentS == nearSE || currentE == nearS || currentE == nearSE))
            tile = nearJoinSE;
        else if (half && (CURRENT == nearS))
            tile = nearSJoinE;
        else if (!half && (currentW == nearS || currentW == nearJoinSE))
            tile = nearSE;
        else if (!half && (currentS == nearE || currentS == nearSE))
            tile = nearE;
        else if (!half && (CURRENT == nearS))
            tile = nearSE;
        else if (!half)
            tile = far ? farE : nearE;
    } else if (edge == EdgeS) {
        // far
        if (!half && (currentS == nearEJoinS || currentS == nearSE || currentS == nearE || currentS == nearSE))
            tile = farSJoinE;
        else if (half && (currentN == farE || currentN == farJoinSE || currentN == nearSJoinE || CURRENT == farE))
            tile = farEJoinS;
        else if (half && (currentS == farE || currentS == farSE || currentS == farEJoinS
                          || currentE == farS || currentE == farSE || currentE == farSJoinE))
            tile = farJoinSE;
        else if (!half && (currentN == farJoinSE || currentN == farE))
            tile = farSE;
        else if (!half && (currentW == farS || currentW == farJoinSE|| currentW == nearEJoinS
                           || currentE == farS || currentE == farSJoinE || currentE == farSE))
            tile = farS;

        // near
        else if (!half && (currentS == farE || currentS == farSE || currentS == farEJoinS))
            tile = nearSJoinE;
        else if (half && (currentS == nearE || currentS == nearSE || currentS == nearEJoinS
                          || currentE == nearS || currentE == nearSE || currentE == nearSJoinE))
            tile = nearJoinSE;
        else if (!half && (currentN == nearE || currentN == farSJoinE || currentN == nearJoinSE))
            tile = nearSE;
        else if (half && (CURRENT == nearE))
            tile = nearEJoinS;
        else if (!half && (CURRENT == nearE || CURRENT == nearEJoinS))
            tile = nearSE;
        else if (!half)
            tile = far ? farS : nearS;
    }

    if (tile == (Tile*) -1) {
        return;
    }

    stamp.setCell(x - origin.x(), y - origin.y(), Cell(tile));

    if (!suppressBlendTiles())
        return;

    // Erase user-drawn blend tiles in all blend layers.
    // Set NoBlend flag in all blend layers.
    // Erase below/right of near tiles.  Erase same spot as far tiles
    bool isFar = tiles.indexOf(tile) < Curb::FirstNear;
    if (!isFar) {
        if (tile == nearJoinSE) return;
        if (edge == EdgeE) x += 1;
        else y += 1;
    }

    QSet<Tile*> blendTiles = mapDocument()->mapComposite()->bmpBlender()->knownBlendTiles();

    foreach (QString layerName, mapDocument()->mapComposite()->bmpBlender()->blendLayers()) {
        int index = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index >= 0) {
            TileLayer *tl = mapDocument()->map()->layerAt(index)->asTileLayer();
            if (blendTiles.contains(tl->cellAt(x, y).tile))
                eraseRgn[layerName] += QRect(x, y, 1, 1);
        }

        MapNoBlend *noBlend = mapDocument()->map()->noBlend(layerName);
        if (noBlend->get(x, y) == false)
            noBlendRgn[layerName] += QRect(x, y, 1, 1);
    }
}

void CurbTool::raiseLower(const QPointF &start, const QPointF &end)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());
    QPoint origin(qMin(sx, ex), qMin(sy, ey));
    int width, height;
    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y()))
        width = qAbs(ex - sx) + 2, height = 2;
    else
        width = 2, height = qAbs(ey - sy) + 2;

    TileLayer stamp(QString(), 0, 0, width, height);
    QMap<QString,QRegion> eraseRgn;
    raiseLowerChanges(start, end, stamp, eraseRgn);

    if (!stamp.isEmpty()) {
        PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), currentTileLayer(),
                                                 origin.x(), origin.y(), &stamp,
                                                 stamp.region().translated(origin),
                                                 false);
//      cmd->setMergeable(mergeable);
        mapDocument()->undoStack()->push(cmd);
        mapDocument()->emitRegionEdited(stamp.region(), currentTileLayer());
    }

    foreach (QString layerName, eraseRgn.keys()) {
        int index = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index >= 0) {
            TileLayer *tl = mapDocument()->map()->layerAt(index)->asTileLayer();
            EraseTiles *cmd = new EraseTiles(mapDocument(), tl, eraseRgn[layerName]);
            mapDocument()->undoStack()->push(cmd);
        }
    }
}

void CurbTool::raiseLowerChanges(const QPointF &start, const QPointF &end,
                                 TileLayer &stamp, QMap<QString, QRegion> &eraseRgn)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());
    if (sx > ex) qSwap(sx, ex);
    if (sy > ey) qSwap(sy, ey);

    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y())) {
        for (int x = sx; x <= ex; x++)
            raiseLowerTile(sx, sy, ex, ey, x, sy, stamp, eraseRgn);
    } else {
        for (int y = sy; y <= ey; y++)
            raiseLowerTile(sx, sy, ex, ey, sx, y, stamp, eraseRgn);
    }
}

void CurbTool::raiseLowerTile(int sx, int sy, int ex, int ey, int x, int y,
                              TileLayer &stamp, QMap<QString,QRegion> &eraseRgn)
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return;

    Tile *tile = 0;
    Tile *CURRENT = tileLayer->cellAt(x, y).tile;
    Tile *currentE = tileLayer->contains(x + 1, y) ? tileLayer->cellAt(x + 1, y).tile : 0;
    Tile *currentS = tileLayer->contains(x, y + 1) ? tileLayer->cellAt(x, y + 1).tile : 0;

    QVector<Tile*> tiles = resolveTiles(mCurb);
    Tile *farE = tiles[Curb::FarE];
    Tile *farS = tiles[Curb::FarS];
    Tile *nearE = tiles[Curb::NearE];
    Tile *nearS = tiles[Curb::NearS];

    bool lower = true;
    int dx = 0, dy = 0;
    // Far horizontal
    if (x == sx && CURRENT == farS) {
        tile = tiles[Curb::FarSunkenJoinW];
        dy = 1;
    } else if (x == ex && CURRENT == farS) {
        tile = tiles[Curb::FarSunkenJoinE];
        dy = 1;
    } else if (x > sx && x < ex && CURRENT == farS) {
        tile = tiles[Curb::FarSunkenN];
        dy = 1;
    } else if (currentS == tiles[Curb::FarSunkenJoinW]
               || currentS == tiles[Curb::FarSunkenJoinE]
               || currentS == tiles[Curb::FarSunkenN]) {
        tile = farS;
        dy = 1;
        lower = false;

    // Near horizontal
    } else if (x == sx && CURRENT == nearS) {
        tile = tiles[Curb::NearSunkenJoinW];
    } else if (x == ex && CURRENT == nearS) {
        tile = tiles[Curb::NearSunkenJoinE];
    } else if (x > sx && x < ex && CURRENT == nearS) {
        tile = tiles[Curb::NearSunkenS];
    } else if (CURRENT == tiles[Curb::NearSunkenJoinW]
               || CURRENT == tiles[Curb::NearSunkenJoinE]
               || CURRENT == tiles[Curb::NearSunkenS]) {
        tile = nearS;
        lower = false;
    }

    // Far vertical
    if (y == sy && CURRENT == farE) {
        tile = tiles[Curb::FarSunkenJoinN];
        dx = 1;
    } else if (y == ey && CURRENT == farE) {
        tile = tiles[Curb::FarSunkenJoinS];
        dx = 1;
    } else if (y > sy && y < ey && CURRENT == farE) {
        tile = tiles[Curb::FarSunkenW];
        dx = 1;
    } else if (currentE == tiles[Curb::FarSunkenJoinN]
               || currentE == tiles[Curb::FarSunkenJoinS]
               || currentE == tiles[Curb::FarSunkenW]) {
        tile = farE;
        dx = 1;
        lower = false;

    // Near vertical
    } else if (y == sy && CURRENT == nearE) {
        tile = tiles[Curb::NearSunkenJoinN];
    } else if (y == ey && CURRENT == nearE) {
        tile = tiles[Curb::NearSunkenJoinS];
    } else if (y > sy && y < ey && CURRENT == nearE) {
        tile = tiles[Curb::NearSunkenE];
    } else if (CURRENT == tiles[Curb::NearSunkenJoinN]
               || CURRENT == tiles[Curb::NearSunkenJoinS]
               || CURRENT == tiles[Curb::NearSunkenE]) {
        tile = nearE;
        lower = false;
    }

    if (tile == 0 || tile == (Tile*) -1) {
        return;
    }

    if (dx == 1) {
        x += lower ? 0 : 1; // FIXME: what if not valid x-coord?
        eraseRgn[tileLayer->name()] += QRect(x, y, 1, 1);
        x += lower ? 1 : -1; // FIXME: what if not valid x-coord?
    }
    if (dy == 1) {
        y += lower ? 0 : 1; // FIXME: what if not valid y-coord?
        eraseRgn[tileLayer->name()] += QRect(x, y, 1, 1);
        y += lower ? 1 : -1; // FIXME: what if not valid y-coord?
    }

    QPoint origin(qMin(sx, ex), qMin(sy, ey));

    stamp.setCell(x - origin.x(), y - origin.y(), Cell(tile));
}

void CurbTool::getMapChanges(const QPointF &start, CurbTool::Corner cornerStart,
                             const QPointF &end, CurbTool::Corner cornerEnd, bool far,
                             TileLayer &stamp, QMap<QString, QRegion> &eraseRgn,
                             QMap<QString, QRegion> &noBlendRgn)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());
    QPoint origin(qMin(sx, ex), qMin(sy, ey));

    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y())) {
        // South edge
        // East-to-West
        if (start.x() > end.x()) {
            for (int x = sx; x >= ex; x--) {
                bool half = ((sx == ex) && (cornerStart == cornerEnd))
                        || ((sx != ex && x == sx) && (cornerStart == CornerSW || cornerStart == CornerNW))
                        || ((sx != ex && x == ex) && (cornerEnd == CornerNE || cornerEnd == CornerSE));
                drawEdgeTile(origin, x, sy, EdgeS, half, far, stamp, eraseRgn, noBlendRgn);
            }
            return;
        }
        // West-to-East
        for (int x = sx; x <= ex; x++) {
            bool half = ((sx == ex) && (cornerStart == cornerEnd))
                    || ((sx != ex && x == sx) && (cornerStart == CornerNE || cornerStart == CornerSE))
                    || ((sx != ex && x == ex) && (cornerEnd == CornerSW || cornerEnd == CornerNW));
            drawEdgeTile(origin, x, sy, EdgeS, half, far, stamp, eraseRgn, noBlendRgn);
        }
    } else {
        // East edge
        // South-to-North
        if (start.y() > end.y()) {
            for (int y = sy; y >= ey; y--) {
                bool half = ((sy == ey) && (cornerStart == cornerEnd))
                        || ((sy != ey && y == sy) && (cornerStart == CornerNW || cornerStart == CornerNE))
                        || ((sy != ey && y == ey) && (cornerEnd == CornerSW || cornerEnd == CornerSE));
                drawEdgeTile(origin, sx, y, EdgeE, half, far, stamp, eraseRgn, noBlendRgn);
            }
            return;
        }
        // North-to-South
        for (int y = sy; y <= ey; y++) {
            bool half = ((sy == ey) && (cornerStart == cornerEnd))
                    || ((sy != ey && y == sy) && (cornerStart == CornerSW || cornerStart == CornerSE))
                    || ((sy != ey && y == ey) && (cornerEnd == CornerNW || cornerEnd == CornerNE));
            drawEdgeTile(origin, sx, y, EdgeE, half, far, stamp, eraseRgn, noBlendRgn);
        }
    }
}

/////

#include "BuildingEditor/simplefile.h"

#include <QFileInfo>

CurbFile::CurbFile()
{
}

CurbFile::~CurbFile()
{
    qDeleteAll(mCurbs);
}

bool CurbFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mFileName = path;

//    int version = simple.version();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("curb")) {
            QStringList keys;
            keys << QLatin1String("label") << QLatin1String("layer");
            if (!validKeys(block, keys))
                return false;

            Curb *curb = new Curb;
            curb->mLabel = block.value("label");
            curb->mLayer = block.value("layer");

            foreach (SimpleFileBlock block2, block.blocks) {
                QStringList keys;
                keys << QLatin1String("e")
                     << QLatin1String("s")
                     << QLatin1String("se")
                     << QLatin1String("join-se")
                     << QLatin1String("e-join-s")
                     << QLatin1String("s-join-e");
                QStringList keys2;
                keys2 << QLatin1String("w")
                      << QLatin1String("e")
                      << QLatin1String("n")
                      << QLatin1String("s")
                      << QLatin1String("join-w")
                      << QLatin1String("join-e")
                      << QLatin1String("join-n")
                      << QLatin1String("join-s");
                // TODO: validate tile names
                if (block2.name == QLatin1String("far")) {
                    if (!validKeys(block2, keys))
                        return false;
                    curb->mTileNames[Curb::FarE] = block2.value("e");
                    curb->mTileNames[Curb::FarS] = block2.value("s");
                    curb->mTileNames[Curb::FarSE] = block2.value("se");
                    curb->mTileNames[Curb::FarJoinSE] = block2.value("join-se");
                    curb->mTileNames[Curb::FarEJoinS] = block2.value("e-join-s");
                    curb->mTileNames[Curb::FarSJoinE] = block2.value("s-join-e");
                    foreach (SimpleFileBlock block3, block2.blocks) {
                        if (block3.name == QLatin1String("sunken")) {
                            if (!validKeys(block3, keys2))
                                return false;
                            curb->mTileNames[Curb::FarSunkenW] = block3.value("w");
                            curb->mTileNames[Curb::FarSunkenN] = block3.value("n");
                            curb->mTileNames[Curb::FarSunkenJoinW] = block3.value("join-w");
                            curb->mTileNames[Curb::FarSunkenJoinE] = block3.value("join-e");
                            curb->mTileNames[Curb::FarSunkenJoinN] = block3.value("join-n");
                            curb->mTileNames[Curb::FarSunkenJoinS] = block3.value("join-s");
                        } else {
                            mError = tr("Line %1: Unknown block name '%2'.\n%3")
                                    .arg(block3.lineNumber)
                                    .arg(block3.name)
                                    .arg(path);
                            return false;
                        }
                    }
                } else if (block2.name == QLatin1String("near")) {
                    if (!validKeys(block2, keys))
                        return false;
                    curb->mTileNames[Curb::NearE] = block2.value("e");
                    curb->mTileNames[Curb::NearS] = block2.value("s");
                    curb->mTileNames[Curb::NearSE] = block2.value("se");
                    curb->mTileNames[Curb::NearJoinSE] = block2.value("join-se");
                    curb->mTileNames[Curb::NearEJoinS] = block2.value("e-join-s");
                    curb->mTileNames[Curb::NearSJoinE] = block2.value("s-join-e");
                    foreach (SimpleFileBlock block3, block2.blocks) {
                        if (block3.name == QLatin1String("sunken")) {
                            if (!validKeys(block3, keys2))
                                return false;
                            curb->mTileNames[Curb::NearSunkenE] = block3.value("e");
                            curb->mTileNames[Curb::NearSunkenS] = block3.value("s");
                            curb->mTileNames[Curb::NearSunkenJoinW] = block3.value("join-w");
                            curb->mTileNames[Curb::NearSunkenJoinE] = block3.value("join-e");
                            curb->mTileNames[Curb::NearSunkenJoinN] = block3.value("join-n");
                            curb->mTileNames[Curb::NearSunkenJoinS] = block3.value("join-s");
                        } else {
                            mError = tr("Line %1: Unknown block name '%2'.\n%3")
                                    .arg(block3.lineNumber)
                                    .arg(block3.name)
                                    .arg(path);
                            return false;
                        }
                    }
                } else {
                    mError = tr("Line %1: Unknown block name '%2'.\n%3")
                            .arg(block2.lineNumber)
                            .arg(block2.name)
                            .arg(path);
                    return false;
                }
            }

            mCurbs += curb;
        } else {
            mError = tr("Line %1: Unknown block name '%2'.\n%3")
                    .arg(block.lineNumber)
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

QList<Curb *> CurbFile::takeCurbs()
{
    QList<Curb *> ret = mCurbs;
    mCurbs.clear();
    return ret;
}

bool CurbFile::validKeys(SimpleFileBlock &block, const QStringList &keys)
{
    foreach (SimpleFileKeyValue kv, block.values) {
        if (!keys.contains(kv.name)) {
            mError = tr("Line %1: Unknown attribute '%2'.\n%3")
                    .arg(kv.lineNumber)
                    .arg(kv.name)
                    .arg(mFileName);
            return false;
        }
    }
    return true;
}
