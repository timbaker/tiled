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

#include "CurbTool.h"

//#include "curbtooldialog.h"
#include "mapdocument.h"
#include "mapscene.h"

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
                     QIcon(QLatin1String(
                               ":/BuildingEditor/icons/icon_corner_innerNW.png")),
                     QKeySequence(/*tr("E")*/),
                     parent),
    mInitialClick(false),
    mCurb(0),
    mLineItem(new QGraphicsPolygonItem)
{
    mLineItem->setPen(QPen(Qt::green, 2));

    CurbFile curbFile;
    if (curbFile.read(qApp->applicationDirPath() + QLatin1String("/Curbs.txt"))) {
        mCurb = curbFile.takeCurbs().first();
    }

    mValidAdjacent.resize(Curb::ShapeCount);

    mValidAdjacent[Curb::FarE].n << Curb::FarE << Curb::FarJoinSE << Curb::NearSJoinE;
    mValidAdjacent[Curb::FarE].s << Curb::FarE << Curb::FarSE << Curb::FarEJoinS;

    mValidAdjacent[Curb::FarS].w << Curb::FarS << Curb::FarJoinSE << Curb::NearEJoinS;
    mValidAdjacent[Curb::FarS].e << Curb::FarS << Curb::FarSE << Curb::FarSJoinE;

    mValidAdjacent[Curb::FarSE].w << Curb::FarS << Curb::FarJoinSE << Curb::NearEJoinS;
    mValidAdjacent[Curb::FarSE].n << Curb::FarE << Curb::FarJoinSE << Curb::NearSJoinE;

    mValidAdjacent[Curb::FarJoinSE].e << Curb::FarS << Curb::FarSE << Curb::FarSJoinE;
    mValidAdjacent[Curb::FarJoinSE].s << Curb::FarE << Curb::FarSE << Curb::FarEJoinS;

    mValidAdjacent[Curb::FarEJoinS].n << Curb::FarE << Curb::FarJoinSE << Curb::NearSJoinE;
    mValidAdjacent[Curb::FarEJoinS].e << Curb::NearS << Curb::NearSE << Curb::NearSJoinE;

    mValidAdjacent[Curb::FarSJoinE].w << Curb::FarS << Curb::FarJoinSE << Curb::NearEJoinS;
    mValidAdjacent[Curb::FarSJoinE].s << Curb::NearE << Curb::NearSE << Curb::NearEJoinS;

    /////

    mValidAdjacent[Curb::NearE].n << Curb::NearE << Curb::NearJoinSE << Curb::FarSJoinE;
    mValidAdjacent[Curb::NearE].s << Curb::NearE << Curb::NearSE << Curb::NearEJoinS;

    mValidAdjacent[Curb::NearS].w << Curb::NearS << Curb::NearJoinSE << Curb::FarEJoinS;
    mValidAdjacent[Curb::NearS].e << Curb::NearS << Curb::NearSE << Curb::NearSJoinE;

    mValidAdjacent[Curb::NearSE].w << Curb::NearS << Curb::NearJoinSE << Curb::FarEJoinS;
    mValidAdjacent[Curb::NearSE].n << Curb::NearE << Curb::NearJoinSE << Curb::FarSJoinE;

    mValidAdjacent[Curb::NearJoinSE].e << Curb::NearS << Curb::NearSE << Curb::NearSJoinE;
    mValidAdjacent[Curb::NearJoinSE].s << Curb::NearE << Curb::NearSE << Curb::NearEJoinS;

    mValidAdjacent[Curb::NearEJoinS].n << Curb::NearE << Curb::NearJoinSE << Curb::FarSJoinE;
    mValidAdjacent[Curb::NearEJoinS].e << Curb::FarS << Curb::FarSJoinE << Curb::FarSE;

    mValidAdjacent[Curb::NearSJoinE].w << Curb::NearS << Curb::NearJoinSE << Curb::FarEJoinS;
    mValidAdjacent[Curb::NearSJoinE].s << Curb::FarE << Curb::FarEJoinS << Curb::FarSE;

    for (int i = 0; i < mValidAdjacent.size(); i++) {
        Curb::Shapes sh = static_cast<Curb::Shapes>(i);
        foreach (Curb::Shapes w, mValidAdjacent[i].w) Q_ASSERT(mValidAdjacent[w].e.contains(sh));
        foreach (Curb::Shapes n, mValidAdjacent[i].n) Q_ASSERT(mValidAdjacent[n].s.contains(sh));
        foreach (Curb::Shapes e, mValidAdjacent[i].e) Q_ASSERT(mValidAdjacent[e].w.contains(sh));
        foreach (Curb::Shapes s, mValidAdjacent[i].s) Q_ASSERT(mValidAdjacent[s].n.contains(sh));
    }
}

void CurbTool::activate(MapScene *scene)
{
    mInitialClick = false;
    scene->addItem(mLineItem);
    AbstractTileTool::activate(scene);
//    CurbToolDialog::instance()->setVisibleLater(true);
}

void CurbTool::deactivate(MapScene *scene)
{
//    CurbToolDialog::instance()->setVisibleLater(false);
    scene->removeItem(mLineItem);
    AbstractTileTool::deactivate(scene);
}

void CurbTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    QPointF tilePosF;
    Corner corner;
    toCorner(pos, tilePosF, corner);
    QPoint tilePos(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    if (!mInitialClick) {
        qreal dx = 0, dy = 0;
        if (corner == CornerNE || corner == CornerSE) dx = 0.5;
        if (corner == CornerSW || corner == CornerSE) dy = 0.5;
        QPolygonF poly = renderer->tileToPixelCoords(QRectF(tilePos.x() + dx, tilePos.y() + dy, 0.5, 0.5), layer->level());
        mLineItem->setPolygon(poly);
    } else {
        qreal dx = 0, dy = 0;
        if (corner == CornerNE || corner == CornerSE) dx = 0.5;
        if (corner == CornerSW || corner == CornerSE) dy = 0.5;
        QRectF tileRectEnd(tilePos.x() + dx, tilePos.y() + dy, 0.5, 0.5);
        dx = dy = 0;
        if (mStartCorner == CornerNE || mStartCorner == CornerSE) dx = 0.5;
        if (mStartCorner == CornerSW || mStartCorner == CornerSE) dy = 0.5;
        QRectF tileRectStart(mStartTilePos.x() + dx, mStartTilePos.y() + dy, 0.5, 0.5);
        QPolygonF poly = renderer->tileToPixelCoords(tileRectEnd | tileRectStart, layer->level());
        mLineItem->setPolygon(poly);
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
            // Click a second time to draw
            qreal dx = tilePosF.x() - mStartTilePosF.x();
            qreal dy = tilePosF.y() - mStartTilePosF.y();
            mapDocument()->undoStack()->beginMacro(tr("Draw Curb"));
            if (qAbs(dx) > qAbs(dy)) {
                drawEdge(mStartTilePosF, mStartCorner, tilePosF, corner,
                         (event->modifiers() & Qt::AltModifier) != 0);
            } else {
                drawEdge(mStartTilePosF, mStartCorner, tilePosF, corner,
                         (event->modifiers() & Qt::AltModifier) != 0);
            }
            mapDocument()->undoStack()->endMacro();
            mInitialClick = false;
            mouseMoved(event->scenePos(), event->modifiers());
        } else {
            mStartTilePosF = tilePosF;
            mStartTilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
            mStartCorner = corner;
            mInitialClick = true;
        }
        mLineItem->setPen(QPen(Qt::blue, 2));
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
    if (event->button() == Qt::LeftButton) {
        mLineItem->setPen(QPen(Qt::green, 2));
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
}

QVector<Tile *> CurbTool::resolveTiles(Curb *curb)
{
    QVector<Tile *> ret(Curb::ShapeCount);
    Map *map = mapDocument()->map();
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, map->tilesets())
        tilesets[ts->name()] = ts;

    for (int i = 0; i < Curb::ShapeCount; i++) {
        ret[i] = (Tile*) -1;
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
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
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

// Get a CurbSpot for each recognized tile in an area
QVector<CurbTool::CurbSpot> CurbTool::mapToSpots(int x, int y, int w, int h)
{
    TileLayer *tileLayer = currentTileLayer();
    QVector<Tile*> tiles = resolveTiles(mCurb);

    QVector<CurbSpot> ret(w * h);
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (tileLayer->contains(x + i, y + j)) {
                Tile *tile = tileLayer->cellAt(x + i, y + j).tile;
                ret[i + j * w] = toCurbSpot(tile, tiles);
            }
        }
    }

    // Bleed edges from surrounding spots to the ones being drawn
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (i == 0 && (j >= 1 && j < h - 1)) {

            }
            if (i == w - 1 && (j >= 1 && j < h - 1)) {

            }
        }
    }

    return ret;
}

CurbTool::CurbSpot CurbTool::toCurbSpot(Tile *tile, QVector<Tile *> tiles)
{
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

    CurbSpot ret;
    if (tile == farE || tile == farEJoinS) {
        ret.far = true;
        ret.ne[1] = true;
        ret.se[1] = true;
    } else if (tile == farS || tile == farSJoinE) {
        ret.far = true;
        ret.sw[0] = true;
        ret.se[0] = true;
    } else if (tile == farSE) {
        ret.far = true;
        ret.sw[0] = true;
        ret.se[0] = true;
        ret.ne[1] = true;
        ret.se[1] = true;
    } else if (tile == farJoinSE) {
        ret.far = true;
        ret.se[0] = true;
        ret.se[1] = true;
    }

    if (tile == nearE || tile == nearEJoinS) {
        ret.far = false;
        ret.ne[1] = true;
        ret.se[1] = true;
    } else if (tile == nearS || tile == nearSJoinE) {
        ret.far = false;
        ret.sw[0] = true;
        ret.se[0] = true;
    } else if (tile == nearSE) {
        ret.far = false;
        ret.sw[0] = true;
        ret.se[0] = true;
        ret.ne[1] = true;
        ret.se[1] = true;
    } else if (tile == nearJoinSE) {
        ret.far = false;
        ret.se[0] = true;
        ret.se[1] = true;
    }

    return ret;
}

void Tiled::Internal::CurbTool::spotToTile(int x, int y, Tiled::Internal::CurbTool::CurbSpot &spot)
{
}

void CurbTool::drawEdge(const QPointF &start, Corner cornerStart,
                        const QPointF &end, Corner cornerEnd,
                        bool far)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());

#if 1
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
#else
    if (qAbs(start.x() - end.x()) > qAbs(start.y() - end.y())) {
        if (sx > ex) qSwap(sx, ex);
        int len = ex - sx + 1;
        QVector<CurbSpot> spots = mapToSpots(sx - 1, sy - 1, len + 2, 3);
        for (int x = sx; x <= ex; x++) {
            int i = 1 + x - sx, j = 1;
            CurbSpot &spot = spots[i + j * (len + 2)];
//            spot.far = ???;
            spot.sw[0] = true; // south edge
            spot.se[0] = true; // south edge
            spotToTile(sx, sy, spot);
//            drawEdgeTile(x, sy, EdgeS);
        }
    } else {
        if (sy > ey) qSwap(sy, ey);
        int len = ey - sy + 1;
        QVector<CurbSpot> spots = mapToSpots(sx - 1, sy - 1, 3, len + 2);
        for (int y = sy; y <= ey; y++) {
            int i = 1, j = 1 + y - sy;
            CurbSpot &spot = spots[i + j * 3];
//            spot.far = ???;
            spot.ne[1] = true; // east edge
            spot.se[1] = true; // east edge
            spotToTile(sx, y, spot);
//            drawEdgeTile(sx, y, EdgeE);
        }
    }
#endif
}

#include "BuildingEditor/buildingtiles.h"
#include "erasetiles.h"
#include "painttilelayer.h"
void CurbTool::drawEdgeTile(int x, int y, Edge edge, bool half, bool far)
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

#if 0
    if (edge == EdgeE) {
    } else if (edge == EdgeS) {

    }
#else
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
        else if (!half && (currentW == farS || currentW == farJoinSE || currentE == farS || currentE == farSJoinE
                           || currentW == nearEJoinS))
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
#endif

    if (tile == (Tile*) -1) {
        return;
    }

    // FIXME: one undo command for the whole edge
    TileLayer stamp(QString(), 0, 0, 1, 1);
    stamp.setCell(0, 0, Cell(tile));
    PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), tileLayer,
                                             x, y, &stamp,
                                             QRect(x, y, 1, 1),
                                             false);
//    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
    mapDocument()->emitRegionEdited(QRect(x, y, 1, 1), tileLayer);
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

    int version = simple.version();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("curb")) {
            Curb *curb = new Curb;
            curb->mLabel = block.value("label");

            // TODO: validate tile names
            SimpleFileBlock inner = block.block("far");
            curb->mTileNames[Curb::FarE] = inner.value("e");
            curb->mTileNames[Curb::FarS] = inner.value("s");
            curb->mTileNames[Curb::FarSE] = inner.value("se");
            curb->mTileNames[Curb::FarJoinSE] = inner.value("join-se");
            curb->mTileNames[Curb::FarEJoinS] = inner.value("e-join-s");
            curb->mTileNames[Curb::FarSJoinE] = inner.value("s-join-e");
            SimpleFileBlock outer = block.block("near");
            curb->mTileNames[Curb::NearE] = outer.value("e");
            curb->mTileNames[Curb::NearS] = outer.value("s");
            curb->mTileNames[Curb::NearSE] = outer.value("se");
            curb->mTileNames[Curb::NearJoinSE] = outer.value("join-se");
            curb->mTileNames[Curb::NearEJoinS] = outer.value("e-join-s");
            curb->mTileNames[Curb::NearSJoinE] = outer.value("s-join-e");

            mCurbs += curb;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
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
