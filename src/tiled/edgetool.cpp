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

#include "bmpblender.h"
#include "bmptool.h"
#include "edgetooldialog.h"
#include "erasetiles.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"

#include "BuildingEditor/buildingtiles.h"

#include "maplevel.h"
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
                     QIcon(QLatin1String(":/images/22x22/edge-tool.png")),
                     QKeySequence(/*tr("E")*/),
                     parent),
    mScene(0),
    mToolTileLayerGroup(0),
    mToolTiles(QString(), 0, 0, 1, 1),
    mInitialClick(false),
    mEdges(0),
    mDashLen(0),
    mDashGap(0),
    mSuppressBlendTiles(false),
    mCursorItem(new QGraphicsPathItem)
{
    mCursorItem->setPen(QPen(QColor(0,255,0,96), 1));
    mCursorItem->setBrush(QColor(0,255,0,64));
}

void EdgeTool::activate(MapScene *scene)
{
    Q_ASSERT(mScene == 0);
    mScene = scene;
    mInitialClick = false;
    scene->addItem(mCursorItem);
    AbstractTileTool::activate(scene);
    EdgeToolDialog::instance()->setVisibleLater(true);
}

void EdgeTool::deactivate(MapScene *scene)
{
    Q_ASSERT(mScene == scene);
    if (mToolTileLayerGroup != 0) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = 0;
        mToolTiles.erase();
    }

    EdgeToolDialog::instance()->setVisibleLater(false);
    scene->removeItem(mCursorItem);
    mScene = 0;
    AbstractTileTool::deactivate(scene);
}

void EdgeTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    QPointF tilePosF = renderer->pixelToTileCoords(pos, layer ? layer->level() : 0);
    QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
    QPainterPath path;

    CompositeLayerGroup *lg = mapDocument()->mapComposite()->layerGroupForLevel(mapDocument()->currentLevelIndex());
    if (mToolTileLayerGroup != nullptr) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = nullptr;
        mToolTiles.erase();
    }
    QPoint topLeft;
    QVector<QVector<Cell> > toolTiles;

    if (!mInitialClick) {
        if (dW < dE) {
            mEdge = (dW < dN && dW < dS) ? EdgeW : ((dN < dS) ? EdgeN : EdgeS);
        } else {
            mEdge = (dE < dN && dE < dS) ? EdgeE : ((dN < dS) ? EdgeN : EdgeS);
        }

        QVector<Tile*> tiles = resolveEdgeTiles(mEdges);

        QPolygonF polyN = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 1, 0.25), layer->level());
        QPolygonF polyS = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y() + 0.75, 1, 0.25), layer->level());
        QPolygonF polyW = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 0.25, 1), layer->level());
        QPolygonF polyE = renderer->tileToPixelCoords(QRectF(tilePos.x() + 0.75, tilePos.y(), 0.25, 1), layer->level());
        polyN += polyN.first(), polyS += polyS.first(), polyW += polyW.first(), polyE += polyE.first();
        int shape = -1;
        if ((mEdge == EdgeN && dW < 0.5) || (mEdge == EdgeW && dN < 0.5)) {
            path.addPolygon(polyN), path.addPolygon(polyW);
            shape = Edges::OuterNW;
        }
        if ((mEdge == EdgeS && dW < 0.5) || (mEdge == EdgeW && dS < 0.5)) {
            path.addPolygon(polyS), path.addPolygon(polyW);
            shape = Edges::OuterSW;
        }
        if ((mEdge == EdgeN && dE < 0.5) || (mEdge == EdgeE && dN < 0.5)) {
            path.addPolygon(polyN), path.addPolygon(polyE);
            shape = Edges::OuterNE;
        }
        if ((mEdge == EdgeS && dE < 0.5) || (mEdge == EdgeE && dS < 0.5)) {
            path.addPolygon(polyS), path.addPolygon(polyE);
            shape = Edges::OuterSE;
        }
        if (shape != -1 && tiles[shape] != (Tile*)-1) {
            topLeft = tilePos;
            mToolTiles.resize(QSize(1, 1), QPoint());
            mToolTiles.setCell(0, 0, Cell(tiles[shape]));
        }
    } else {
        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        m = mStartTilePosF - mStartTilePos;
        if (qAbs(dx) > qAbs(dy)) {
            qreal dy = 0;
            if (m.y() >= 0.5) dy = 0.75;
            QRectF r = QRectF(mStartTilePos.x(), mStartTilePos.y() + dy, 1, 0.25)
                    | QRectF(tilePos.x(), mStartTilePos.y() + dy, 1, 0.25);
            QPolygonF poly = renderer->tileToPixelCoords(r, layer->level());
            path.addPolygon(poly);

            topLeft = QPoint(qMin(mStartTilePos.x(), tilePos.x()), mStartTilePos.y());
            mToolTiles.resize(QSize(qAbs(tilePos.x() - mStartTilePos.x()) + 1, 1), QPoint());
            QMap<QString,QRegion> eraseRgn, noBlendRgn;
            tilePosF.setY(mStartTilePosF.y());
            getMapChanges(mStartTilePosF, tilePosF, dy ? EdgeS : EdgeN, mToolTiles, eraseRgn, noBlendRgn);
        } else {
            qreal dx = 0;
            if (m.x() >= 0.5) dx = 0.75;
            QRectF r = QRectF(mStartTilePos.x() + dx, mStartTilePos.y(), 0.25, 1)
                    | QRectF(mStartTilePos.x() + dx, tilePos.y(), 0.25, 1);
            QPolygonF poly = renderer->tileToPixelCoords(r, layer->level());
            path.addPolygon(poly);

            topLeft = QPoint(mStartTilePos.x(), qMin(mStartTilePos.y(), tilePos.y()));
            mToolTiles.resize(QSize(1, qAbs(tilePos.y() - mStartTilePos.y()) + 1), QPoint());
            QMap<QString,QRegion> eraseRgn, noBlendRgn;
            tilePosF.setX(mStartTilePosF.x());
            getMapChanges(mStartTilePosF, tilePosF, dx ? EdgeE : EdgeW, mToolTiles, eraseRgn, noBlendRgn);
        }
    }

    if (!mToolTiles.isEmpty()) {
        QSize tilesSize(mToolTiles.width(), mToolTiles.height());
        lg->setToolTiles(&mToolTiles, topLeft, QRect(topLeft, tilesSize), currentTileLayer());
        mToolTilesRect = renderer->boundingRect(QRect(topLeft.x(), topLeft.y(), mToolTiles.width(), mToolTiles.height()),
                mapDocument()->currentLevelIndex()).adjusted(-3, -(128-32) - 3, 3, 3); // use mMap->drawMargins()
        mToolTileLayerGroup = lg;
        mScene->update(mToolTilesRect);
    }

    path.setFillRule(Qt::WindingFill);
    path = path.simplified();
    mCursorItem->setPath(path);

    AbstractTileTool::mouseMoved(pos, modifiers);
}

void EdgeTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const MapRenderer *renderer = mapDocument()->renderer();
        Layer *layer = currentTileLayer();
        QPointF tilePosF = renderer->pixelToTileCoords(event->scenePos(),
                                                       layer ? layer->level() : 0);
        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
        if (mInitialClick) {
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
    if (event->button() == Qt::LeftButton && mInitialClick) {
        const MapRenderer *renderer = mapDocument()->renderer();
        Layer *layer = currentTileLayer();
        QPointF tilePosF = renderer->pixelToTileCoords(event->scenePos(),
                                                       layer ? layer->level() : 0);
//        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
//        QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
        QPointF m = mStartTilePosF - mStartTilePos;
        // Click a second time to draw
        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        mapDocument()->undoStack()->beginMacro(tr("Draw Edge"));
        if (qAbs(dx) > qAbs(dy)) {
            tilePosF.setY(mStartTilePosF.y());
            if (m.y() < 0.5)
                drawEdge(mStartTilePosF, tilePosF, EdgeN);
            else
                drawEdge(mStartTilePosF, tilePosF, EdgeS);
        } else {
            tilePosF.setX(mStartTilePosF.x());
            if (m.x() < 0.5)
                drawEdge(mStartTilePosF, tilePosF, EdgeW);
            else
                drawEdge(mStartTilePosF, tilePosF, EdgeE);
        }
        mapDocument()->undoStack()->endMacro();
        mInitialClick = false;
        mouseMoved(event->scenePos(), event->modifiers());
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
    Q_UNUSED(tilePos)
}

QVector<Tile *> EdgeTool::resolveEdgeTiles(Edges *edges)
{
    QVector<Tile *> ret(Edges::ShapeCount, (Tile*)-1);
    if (!edges)
        return ret;
    Map *map = mapDocument()->map();
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, map->tilesets())
        tilesets[ts->name()] = ts;

    for (int i = 0; i < Edges::ShapeCount; i++) {
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
    QPoint origin(qMin(sx, ex), qMin(sy, ey));
    int width, height;
    if (edge == EdgeN || edge == EdgeS) {
        width = qAbs(ex - sx) + 1;
        height = 1;
    } else {
        width = 1;
        height = qAbs(ey - sy) + 1;
    }
    TileLayer stamp(QString(), 0, 0, width, height);
    QMap<QString,QRegion> eraseRgn, noBlendRgn;
    getMapChanges(start, end, edge, stamp, eraseRgn, noBlendRgn);

    if (!stamp.isEmpty()) {
        PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), currentTileLayer(),
                                                 origin.x(), origin.y(), &stamp,
                                                 stamp.region().translated(origin),
                                                 false);
//      cmd->setMergeable(mergeable);
        mapDocument()->undoStack()->push(cmd);
        mapDocument()->emitRegionEdited(stamp.region(), currentTileLayer());
    }

    int levelIndex = currentTileLayer()->level();
    MapLevel *mapLevel = mapDocument()->map()->levelAt(levelIndex);

    const QStringList layerNames1 = eraseRgn.keys();
    for (const QString &layerName : layerNames1) {
        int index = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        if (index >= 0) {
            TileLayer *tl = mapLevel->layerAt(index)->asTileLayer();
            EraseTiles *cmd = new EraseTiles(mapDocument(), tl, eraseRgn[layerName]);
            mapDocument()->undoStack()->push(cmd);
        }
    }

    const QStringList layerNames2 = noBlendRgn.keys();
    for (const QString &layerName : layerNames2) {
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
}

void EdgeTool::getMapChanges(const QPointF &start, const QPointF &end,
                             EdgeTool::Edge edge, TileLayer &stamp,
                             QMap<QString, QRegion> &eraseRgn,
                             QMap<QString, QRegion> &noBlendRgn)
{
    int sx = qFloor(start.x()), sy = qFloor(start.y());
    int ex = qFloor(end.x()), ey = qFloor(end.y());
    QPoint origin(qMin(sx, ex), qMin(sy, ey));

    if (mDashLen > 0 && mDashGap > 0) {
        if (edge == EdgeN || edge == EdgeS) {
            if (sx > ex) {
                for (int x = sx; x >= ex; ) {
                    for (int n = 0; n < mDashLen && x >= ex; n++, x--)
                        drawEdgeTile(origin, x, sy, edge, stamp, eraseRgn, noBlendRgn);
                    for (int n = 0; n < mDashGap && x >= ex; n++, x--)
                        drawGapTile(x, sy, stamp, eraseRgn, noBlendRgn);
                }
                return;
            }
            for (int x = sx; x <= ex; ) {
                for (int n = 0; n < mDashLen && x <= ex; n++, x++)
                    drawEdgeTile(origin, x, sy, edge, stamp, eraseRgn, noBlendRgn);
                for (int n = 0; n < mDashGap && x <= ex; n++, x++)
                    drawGapTile(x, sy, stamp, eraseRgn, noBlendRgn);
            }
        } else {
            if (sy > ey) {
                for (int y = sy; y >= ey; ) {
                    for (int n = 0; n < mDashLen && y >= ey; n++, y--)
                        drawEdgeTile(origin, sx, y, edge, stamp, eraseRgn, noBlendRgn);
                    for (int n = 0; n < mDashGap && y >= ey; n++, y--)
                        drawGapTile(sx, y, stamp, eraseRgn, noBlendRgn);
                }
                return;
            }
            for (int y = sy; y <= ey; ) {
                for (int n = 0; n < mDashLen && y <= ey; n++, y++)
                    drawEdgeTile(origin, sx, y, edge, stamp, eraseRgn, noBlendRgn);
                for (int n = 0; n < mDashGap && y <= ey; n++, y++)
                    drawGapTile(sx, y, stamp, eraseRgn, noBlendRgn);
            }
        }
        return;
    }

    if (edge == EdgeN || edge == EdgeS) {
        if (sx > ex) qSwap(sx, ex);
        for (int x = sx; x <= ex; x++)
            drawEdgeTile(origin, x, sy, edge, stamp, eraseRgn, noBlendRgn);
    } else {
        if (sy > ey) qSwap(sy, ey);
        for (int y = sy; y <= ey; y++)
            drawEdgeTile(origin, sx, y, edge, stamp, eraseRgn, noBlendRgn);
    }
}

void EdgeTool::drawEdgeTile(const QPoint &origin, int x, int y, EdgeTool::Edge edge,
                            TileLayer &stamp,
                            QMap<QString, QRegion> &eraseRgn,
                            QMap<QString, QRegion> &noBlendRgn)
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return;

    Tile *tile = nullptr;
    Tile *currentW = tileLayer->contains(x - 1, y) ? tileLayer->cellAt(x - 1, y).tile : nullptr;
    Tile *currentE = tileLayer->contains(x + 1, y) ? tileLayer->cellAt(x + 1, y).tile : nullptr;
    Tile *currentN = tileLayer->contains(x, y - 1) ? tileLayer->cellAt(x, y - 1).tile : nullptr;
    Tile *currentS = tileLayer->contains(x, y + 1) ? tileLayer->cellAt(x, y + 1).tile : nullptr;

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

    if (tile == (Tile*) -1) {
        drawGapTile(x, y, stamp, eraseRgn, noBlendRgn);
        return;
    }

    stamp.setCell(x - origin.x(), y - origin.y(), Cell(tile));

    if (!suppressBlendTiles())
        return;

    // Erase user-drawn blend tiles in all blend layers.
    // Set NoBlend flag in all blend layers.
    QSet<Tile*> blendTiles = mapDocument()->mapComposite()->bmpBlender()->knownBlendTiles();

    MapLevel *mapLevel = mapDocument()->map()->levelAt(0);

    const QStringList layerNames = mapDocument()->mapComposite()->bmpBlender()->blendLayers();
    for (const QString &layerName : layerNames) {
        int index = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        if (index >= 0) {
            TileLayer *tl = mapLevel->layerAt(index)->asTileLayer();
            if (blendTiles.contains(tl->cellAt(x, y).tile))
                eraseRgn[layerName] += QRect(x, y, 1, 1);
        }

        MapNoBlend *noBlend = mapDocument()->map()->noBlend(layerName);
        if (noBlend->get(x, y) == false)
            noBlendRgn[layerName] += QRect(x, y, 1, 1);
    }
}

void EdgeTool::drawGapTile(int x, int y, TileLayer &stamp,
                           QMap<QString, QRegion> &eraseRgn,
                           QMap<QString, QRegion> &noBlendRgn)
{
    Q_UNUSED(stamp)
    Q_UNUSED(noBlendRgn)
    eraseRgn[currentTileLayer()->name()] += QRect(x, y, 1, 1);
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

    mFileName = path;

//    int version = simple.version();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("edges")) {
            QStringList keys;
            keys << QLatin1String("label")
                 << QLatin1String("layer")
                 << QLatin1String("w")
                 << QLatin1String("n")
                 << QLatin1String("e")
                 << QLatin1String("s");
            if (!validKeys(block, keys))
                return false;

            Edges *edges = new Edges;
            edges->mLabel = block.value("label");
            edges->mLayer = block.value("layer");
            if (block.hasValue("first")) {

            } else {
                // TODO: validate tile names
                edges->mTileNames[Edges::EdgeW] = block.value("w");
                edges->mTileNames[Edges::EdgeN] = block.value("n");
                edges->mTileNames[Edges::EdgeE] = block.value("e");
                edges->mTileNames[Edges::EdgeS] = block.value("s");
                foreach (SimpleFileBlock block2, block.blocks) {
                    QStringList keys;
                    keys << QLatin1String("nw")
                         << QLatin1String("ne")
                         << QLatin1String("se")
                         << QLatin1String("sw");
                    if (block2.name == QLatin1String("inner")) {
                        if (!validKeys(block2, keys))
                            return false;
                        edges->mTileNames[Edges::InnerNW] = block2.value("nw");
                        edges->mTileNames[Edges::InnerNE] = block2.value("ne");
                        edges->mTileNames[Edges::InnerSE] = block2.value("se");
                        edges->mTileNames[Edges::InnerSW] = block2.value("sw");
                    } else if (block2.name == QLatin1String("outer")) {
                        if (!validKeys(block2, keys))
                            return false;
                        edges->mTileNames[Edges::OuterNW] = block2.value("nw");
                        edges->mTileNames[Edges::OuterNE] = block2.value("ne");
                        edges->mTileNames[Edges::OuterSE] = block2.value("se");
                        edges->mTileNames[Edges::OuterSW] = block2.value("sw");

                    } else {
                        mError = tr("Line %1: Unknown block name '%2'.\n%3")
                                .arg(block2.lineNumber)
                                .arg(block2.name)
                                .arg(path);
                        return false;
                    }
                }

                SimpleFileBlock inner = block.block("inner");
                SimpleFileBlock outer = block.block("outer");
            }

            mEdges += edges;
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

QList<Edges *> EdgeFile::takeEdges()
{
    QList<Edges *> ret = mEdges;
    mEdges.clear();
    return ret;
}

bool EdgeFile::validKeys(SimpleFileBlock &block, const QStringList &keys)
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
