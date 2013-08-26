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

#include "edgetool.h"

#include "edgetooldialog.h"
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

SINGLETON_IMPL(EdgeTool)

EdgeTool::EdgeTool(QObject *parent) :
    AbstractTileTool(tr("Draw Edge"),
                     QIcon(QLatin1String(
                               ":/BuildingEditor/icons/icon_corner_outerNW.png")),
                     QKeySequence(/*tr("E")*/),
                     parent),
    mInitialClick(false),
    mEdges(0),
    mDashLen(0),
    mDashGap(0),
    mLineItem(new QGraphicsPolygonItem)
{
    mLineItem->setPen(QPen(Qt::green, 2));
}

void EdgeTool::activate(MapScene *scene)
{
    mInitialClick = false;
    scene->addItem(mLineItem);
    AbstractTileTool::activate(scene);
    EdgeToolDialog::instance()->setVisibleLater(true);
}

void EdgeTool::deactivate(MapScene *scene)
{
    EdgeToolDialog::instance()->setVisibleLater(false);
    scene->removeItem(mLineItem);
    AbstractTileTool::deactivate(scene);
}

void EdgeTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    const QPointF tilePosF = renderer->pixelToTileCoords(pos, layer ? layer->level() : 0);
    QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
    if (!mInitialClick) {
        if (dW < dE) {
            mEdge = (dW < dN && dW < dS) ? EdgeW : ((dN < dS) ? EdgeN : EdgeS);
        } else {
            mEdge = (dE < dN && dE < dS) ? EdgeE : ((dN < dS) ? EdgeN : EdgeS);
        }

        QPolygonF poly = renderer->tileToPixelCoords(QRect(tilePos.x(), tilePos.y(), 1, 1), layer->level());
        if ((mEdge == EdgeN && dW < 0.5) || (mEdge == EdgeW && dN < 0.5))
            mLineItem->setPolygon(QPolygonF() << poly[3] << poly[0] << poly[1]);
        if ((mEdge == EdgeS && dW < 0.5) || (mEdge == EdgeW && dS < 0.5))
            mLineItem->setPolygon(QPolygonF() << poly[0] << poly[3] << poly[2]);
        if ((mEdge == EdgeN && dE < 0.5) || (mEdge == EdgeE && dN < 0.5))
            mLineItem->setPolygon(QPolygonF() << poly[0] << poly[1] << poly[2]);
        if ((mEdge == EdgeS && dE < 0.5) || (mEdge == EdgeE && dS < 0.5))
            mLineItem->setPolygon(QPolygonF() << poly[1] << poly[2] << poly[3]);
    } else {
        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        m = mStartTilePosF - mStartTilePos;
        if (qAbs(dx) > qAbs(dy)) {
            QPolygonF poly = renderer->tileToPixelCoords(QRect(mStartTilePos, QSize(1,1))
                                                     | QRect(tilePos.x(), mStartTilePos.y(), 1, 1), layer->level());
            if (m.y() < 0.5)
                mLineItem->setPolygon(QPolygonF() << poly[0] << poly[1]); // north
            else
                mLineItem->setPolygon(QPolygonF() << poly[3] << poly[2]); // south
        } else {
            QPolygonF poly = renderer->tileToPixelCoords(QRect(mStartTilePos, QSize(1,1))
                                                     | QRect(mStartTilePos.x(), tilePos.y(), 1, 1), layer->level());
            if (m.x() < 0.5)
                mLineItem->setPolygon(QPolygonF() << poly[0] << poly[3]); // west
            else
                mLineItem->setPolygon(QPolygonF() << poly[1] << poly[2]); // east
        }
    }

    AbstractTileTool::mouseMoved(pos, modifiers);
}

void Tiled::Internal::EdgeTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const MapRenderer *renderer = mapDocument()->renderer();
        Layer *layer = currentTileLayer();
        QPointF tilePosF = renderer->pixelToTileCoords(event->scenePos(),
                                                       layer ? layer->level() : 0);
        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
//        QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
        QPointF m = mStartTilePosF - mStartTilePos;
        if (mInitialClick) {
            // Click a second time to draw
            qreal dx = tilePosF.x() - mStartTilePosF.x();
            qreal dy = tilePosF.y() - mStartTilePosF.y();
            mapDocument()->undoStack()->beginMacro(tr("Draw Edge"));
            if (qAbs(dx) > qAbs(dy)) {
                if (m.y() < 0.5)
                    drawEdge(mStartTilePosF, tilePosF, EdgeN);
                else
                    drawEdge(mStartTilePosF, tilePosF, EdgeS);
            } else {
                if (m.x() < 0.5)
                    drawEdge(mStartTilePosF, tilePosF, EdgeW);
                else
                    drawEdge(mStartTilePosF, tilePosF, EdgeE);
            }
            mapDocument()->undoStack()->endMacro();
            mInitialClick = false;
            mouseMoved(event->scenePos(), event->modifiers());
        } else {
            mStartTilePosF = tilePosF;
            mStartTilePos = tilePos;
            mInitialClick = true;
        }
    }

    if (event->button() == Qt::RightButton) {
        if (mInitialClick) {
            mInitialClick = false;
            mouseMoved(event->scenePos(), event->modifiers());
        }
    }
}

void EdgeTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
    }

//    AbstractTileTool::mouseReleased(event);
}

void EdgeTool::languageChanged()
{
    setName(tr("Draw Edge"));
    setShortcut(QKeySequence(/*tr("E")*/));
}

void EdgeTool::setEdges(Edges *edges)
{
    mEdges = edges;
}

void EdgeTool::setDash(int len, int gap)
{
    mDashLen = len;
    mDashGap = gap;
}

void EdgeTool::tilePositionChanged(const QPoint &tilePos)
{
}

QVector<Tile *> EdgeTool::resolveEdgeTiles(Edges *edges)
{
    QVector<Tile *> ret(Edges::EdgeCount);
    Map *map = mapDocument()->map();
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, map->tilesets())
        tilesets[ts->name()] = ts;

    for (int i = 0; i < Edges::EdgeCount; i++) {
        ret[i] = (Tile*) -1;
        if (edges->mTileNames[i].isEmpty())
            continue;
        QString tilesetName;
        int index;
        if (BuildingEditor::BuildingTilesMgr::instance()->parseTileName(edges->mTileNames[i], tilesetName, index)) {
            if (tilesets.contains(tilesetName))
                ret[i] = tilesets[tilesetName]->tileAt(index);
        }
    }

    return ret;
}

void EdgeTool::drawEdge(const QPointF &start, const QPointF &end, Edge edge)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());

    if (mDashLen > 0 && mDashGap > 0) {
        if (edge == EdgeN || edge == EdgeS) {
            if (sx > ex) {
                for (int x = sx; x >= ex; x -= mDashGap) {
                    for (int n = 0; n < mDashLen && x >= ex; n++, x--)
                        drawEdgeTile(x, sy, edge);
                }
                return;
            }
            for (int x = sx; x <= ex; x += mDashGap) {
                for (int n = 0; n < mDashLen && x <= ex; n++, x++)
                    drawEdgeTile(x, sy, edge);
            }
        } else {
            if (sy > ey) {
                for (int y = sy; y >= ey; y -= mDashGap)
                    for (int n = 0; n < mDashLen && y >= ey; n++, y--)
                        drawEdgeTile(sx, y, edge);
                return;
            }
            for (int y = sy; y <= ey; y += mDashGap)
                for (int n = 0; n < mDashLen && y <= ey; n++, y++)
                    drawEdgeTile(sx, y, edge);
        }
        return;
    }

    if (edge == EdgeN || edge == EdgeS) {
        if (sx > ex) qSwap(sx, ex);
        for (int x = sx; x <= ex; x++)
            drawEdgeTile(x, sy, edge);
    } else {
        if (sy > ey) qSwap(sy, ey);
        for (int y = sy; y <= ey; y++)
            drawEdgeTile(sx, y, edge);
    }
}

#include "BuildingEditor/buildingtiles.h"
#include "painttilelayer.h"
void EdgeTool::drawEdgeTile(int x, int y, Edge edge)
{
//    qDebug() << "EdgeTool::drawEdgeTile" << x << y << edge;

    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return;

    Tile *tile = 0;
    Tile *currentW = tileLayer->contains(x - 1, y) ? tileLayer->cellAt(x - 1, y).tile : 0;
    Tile *currentE = tileLayer->contains(x + 1, y) ? tileLayer->cellAt(x + 1, y).tile : 0;
    Tile *currentN = tileLayer->contains(x, y - 1) ? tileLayer->cellAt(x, y - 1).tile : 0;
    Tile *currentS = tileLayer->contains(x, y + 1) ? tileLayer->cellAt(x, y + 1).tile : 0;
#if 1
    QVector<Tile*> tiles = resolveEdgeTiles(mEdges);
    Tile *w = tiles[Edges::EdgeW];
    Tile *n = tiles[Edges::EdgeN];
    Tile *e = tiles[Edges::EdgeE];
    Tile *s = tiles[Edges::EdgeS];
    Tile *nwOuter = tiles[Edges::OuterNW];
    Tile *neOuter = tiles[Edges::OuterNE];
    Tile *seOuter = tiles[Edges::OuterSE];
    Tile *swOuter = tiles[Edges::OuterSW];
    Tile *nwInner = tiles[Edges::InnerNW];
    Tile *neInner = tiles[Edges::InnerNE];
    Tile *seInner = tiles[Edges::InnerSE];
    Tile *swInner = tiles[Edges::InnerSW];
#else
    Tile *w = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_0"));
    Tile *n = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_1"));
    Tile *e = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_2"));
    Tile *s = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_3"));
    Tile *nwOuter = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_4"));
    Tile *neOuter = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_5"));
    Tile *seOuter = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_6"));
    Tile *swOuter = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_7"));
    Tile *nwInner = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_10"));
    Tile *neInner = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_11"));
    Tile *seInner = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_8"));
    Tile *swInner = BuildingEditor::BuildingTilesMgr::instance()->tileFor(QLatin1String("edges_9"));
#endif
    if (edge == EdgeW) {
        tile = w;
        if (currentW == n || currentW == nwOuter || currentW == swInner)
            tile = seInner;
        else if (currentW == s || currentW == swOuter || currentW == nwInner)
            tile = neInner;
        else if (currentE == n || currentE == neOuter || currentE == seInner)
            tile = nwOuter;
        else if (currentE == s || currentE == seOuter || currentE == neInner)
            tile = swOuter;
    } else if (edge == EdgeN) {
        tile = n;
        if (currentN == e || currentN == neOuter || currentN == nwInner)
            tile = swInner;
        else if (currentN == w || currentN == nwOuter || currentN == neInner)
            tile = seInner;
        else if (currentS == w || currentS == swOuter || currentS == seInner)
            tile = nwOuter;
        else if (currentS == e || currentS == seOuter || currentS == swInner)
            tile = neOuter;
    } else if (edge == EdgeE) {
        tile = e;
        if (currentE == n || currentE == neOuter || currentE == seInner)
            tile = swInner;
        else if (currentE == s || currentE == seOuter || currentE == neInner)
            tile = nwInner;
        else if (currentW == n || currentW == nwOuter || currentW == swInner)
            tile = neOuter;
        else if (currentW == s || currentW == swOuter || currentW == nwInner)
            tile = seOuter;
    } else if (edge == EdgeS) {
        tile = s;
        if (currentS == e || currentS == seOuter || currentS == swInner)
            tile = nwInner;
        else if (currentS == w || currentS == swOuter || currentS == seInner)
            tile = neInner;
        else if (currentN == w || currentN == nwOuter || currentN == neInner)
            tile = swOuter;
        else if (currentN == e || currentN == neOuter || currentN == nwInner)
            tile = seOuter;
    }

    if (tile == (Tile*) -1) tile = 0;

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

EdgeFile::EdgeFile()
{
}

EdgeFile::~EdgeFile()
{
    qDeleteAll(mEdges);
}

bool EdgeFile::read(const QString &fileName)
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
        if (block.name == QLatin1String("edges")) {
            Edges *edges = new Edges;
            edges->mLabel = block.value("label");
            if (block.hasValue("first")) {

            } else {
                // TODO: validate tile names
                edges->mTileNames[Edges::EdgeW] = block.value("w");
                edges->mTileNames[Edges::EdgeN] = block.value("n");
                edges->mTileNames[Edges::EdgeE] = block.value("e");
                edges->mTileNames[Edges::EdgeS] = block.value("s");
                SimpleFileBlock inner = block.block("inner");
                edges->mTileNames[Edges::InnerNW] = inner.value("nw");
                edges->mTileNames[Edges::InnerNE] = inner.value("ne");
                edges->mTileNames[Edges::InnerSE] = inner.value("se");
                edges->mTileNames[Edges::InnerSW] = inner.value("sw");
                SimpleFileBlock outer = block.block("outer");
                edges->mTileNames[Edges::OuterNW] = outer.value("nw");
                edges->mTileNames[Edges::OuterNE] = outer.value("ne");
                edges->mTileNames[Edges::OuterSE] = outer.value("se");
                edges->mTileNames[Edges::OuterSW] = outer.value("sw");
            }

            mEdges += edges;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

QList<Edges *> EdgeFile::takeEdges()
{
    QList<Edges *> ret = mEdges;
    mEdges.clear();
    return ret;
}
