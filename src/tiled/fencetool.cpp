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

#include "fencetool.h"

#include "bmpblender.h"
#include "bmptool.h"
#include "fencetooldialog.h"
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

SINGLETON_IMPL(FenceTool)

FenceTool::FenceTool(QObject *parent) :
    AbstractTileTool(tr("Draw Fence"),
                     QIcon(QLatin1String(":/images/22x22/fence-tool.png")),
                     QKeySequence(/*tr("E")*/),
                     parent),
    mScene(0),
    mToolTileLayerGroup(0),
    mInitialClick(false),
    mFence(0),
    mCursorItem(new QGraphicsPathItem)
{
    mCursorItem->setPen(QPen(QColor(0,255,0,96), 1));
    mCursorItem->setBrush(QColor(0,255,0,64));
}

void FenceTool::activate(MapScene *scene)
{
    Q_ASSERT(mScene == 0);
    mScene = scene;
    mInitialClick = false;
    scene->addItem(mCursorItem);
    AbstractTileTool::activate(scene);
    FenceToolDialog::instance()->setVisibleLater(true);
}

void FenceTool::deactivate(MapScene *scene)
{
    Q_ASSERT(mScene == scene);
    if (mToolTileLayerGroup != 0) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = 0;
    }

    FenceToolDialog::instance()->setVisibleLater(false);
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

void FenceTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentTileLayer();
    const QPointF tilePosF = renderer->pixelToTileCoords(pos, layer ? layer->level() : 0);
    QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
    QPainterPath path;

    CompositeLayerGroup *lg = mapDocument()->mapComposite()->layerGroupForLevel(mapDocument()->currentLevel());
    if (mToolTileLayerGroup != 0) {
        mToolTileLayerGroup->clearToolTiles();
        mScene->update(mToolTilesRect);
        mToolTileLayerGroup = 0;
    }
    QPoint topLeft;
    QVector<QVector<Cell> > toolTiles;

    if (!mInitialClick) {
        QPolygonF polyN = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 1, 0.25), layer->level());
        QPolygonF polyS = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y() + 0.75, 1, 0.25), layer->level());
        QPolygonF polyW = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 0.25, 1), layer->level());
        QPolygonF polyE = renderer->tileToPixelCoords(QRectF(tilePos.x() + 0.75, tilePos.y(), 0.25, 1), layer->level());
        polyN += polyN.first(), polyS += polyS.first(), polyW += polyW.first(), polyE += polyE.first();
        bool west = dW < dE && dW < dN && dW < dS;
        bool east = dE <= dW && dE < dN && dE < dS;
        bool south = (dW < dE && !west && dS <= dN) || (dW >= dE && !east && dS <= dN);
        if (modifiers & Qt::AltModifier) {
            path.addPolygon((west || south) ? polyW : polyN);
            if (Tile *tile = gateTile(tilePos.x(), tilePos.y(), west || south)) {
                topLeft = tilePos;
                toolTiles.resize(1);
                toolTiles[0].resize(1);
                toolTiles[0][0] = Cell(tile);
            }
        } else if (modifiers & Qt::ControlModifier) {
            QVector<Tile*> tiles = resolveTiles(mFence);
            if (Tile *tile = tiles[Fence::Post]) {
                topLeft = tilePos;
                toolTiles.resize(1);
                toolTiles[0].resize(1);
                if (!currentTileLayer()->contains(tilePos) ||
                        currentTileLayer()->cellAt(tilePos).tile != tile)
                toolTiles[0][0] = Cell(tile);
            }
        } else {
            path.addPolygon(polyN), path.addPolygon(polyW);
            if (Tile *tile = fenceTile(tilePos.x(), tilePos.y(), west || south)) {
                topLeft = tilePos;
                toolTiles.resize(1);
                toolTiles[0].resize(1);
                toolTiles[0][0] = Cell(tile);
            }
        }
    } else {
        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        m = mStartTilePosF - mStartTilePos;
        if (qAbs(dx) > qAbs(dy)) {
            qreal dy = 0;
//            if (m.y() >= 0.5) dy = 0.75;
            QRectF r = QRectF(mStartTilePos.x(), mStartTilePos.y() + dy, 1, 0.25)
                    | QRectF(tilePos.x(), mStartTilePos.y() + dy, 1, 0.25);
            QPolygonF poly = renderer->tileToPixelCoords(r, layer->level());
            path.addPolygon(poly);

            topLeft = QPoint(qMin(mStartTilePos.x(), tilePos.x()), mStartTilePos.y());
            TileLayer stamp(QString(), 0, 0, qAbs(tilePos.x() - mStartTilePos.x()) + 1, 1);
            getNorthEdgeTiles(mStartTilePos.x(), mStartTilePos.y(), tilePos.x(), stamp);
            toolTiles = tileLayerToVector(stamp);
        } else {
            qreal dx = 0;
//            if (m.x() >= 0.5) dx = 0.75;
            QRectF r = QRectF(mStartTilePos.x() + dx, mStartTilePos.y(), 0.25, 1)
                    | QRectF(mStartTilePos.x() + dx, tilePos.y(), 0.25, 1);
            QPolygonF poly = renderer->tileToPixelCoords(r, layer->level());
            path.addPolygon(poly);

            topLeft = QPoint(mStartTilePos.x(), qMin(mStartTilePos.y(), tilePos.y()));
            TileLayer stamp(QString(), 0, 0, 1, qAbs(tilePos.y() - mStartTilePos.y()) + 1);
            getWestEdgeTiles(mStartTilePos.x(), mStartTilePos.y(), tilePos.y(), stamp);
            toolTiles = tileLayerToVector(stamp);
        }
    }

    if (toolTiles.size()) {
        lg->setToolTiles(toolTiles, topLeft, currentTileLayer());
        mToolTilesRect = renderer->boundingRect(QRect(topLeft.x(), topLeft.y(), toolTiles.size(), toolTiles[0].size()),
                mapDocument()->currentLevel()).adjusted(-3, -(128-32) - 3, 3, 3); // use mMap->drawMargins()
        mToolTileLayerGroup = lg;
        mScene->update(mToolTilesRect);
    }

    path.setFillRule(Qt::WindingFill);
    path = path.simplified();
    mCursorItem->setPath(path);

    AbstractTileTool::mouseMoved(pos, modifiers);
}

void FenceTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const MapRenderer *renderer = mapDocument()->renderer();
        Layer *layer = currentTileLayer();
        QPointF tilePosF = renderer->pixelToTileCoords(event->scenePos(),
                                                       layer ? layer->level() : 0);
        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
//        QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
        QPointF m = mStartTilePosF - mStartTilePos;
#if 0
        if (mInitialClick) {
            // Click a second time to draw
            qreal dx = tilePosF.x() - mStartTilePosF.x();
            qreal dy = tilePosF.y() - mStartTilePosF.y();
            mapDocument()->undoStack()->beginMacro(tr("Draw Edge"));
            if (qAbs(dx) > qAbs(dy)) {
                drawNorthEdge(mStartTilePos.x(), mStartTilePos.y(), tilePos.x());
            } else {
                drawWestEdge(mStartTilePos.x(), mStartTilePos.y(), tilePos.y());
            }
            mapDocument()->undoStack()->endMacro();
            mInitialClick = false;
            mouseMoved(event->scenePos(), event->modifiers());
        } else
#endif
        {
            if (event->modifiers() & Qt::ControlModifier) {
                drawPost(tilePos.x(), tilePos.y());
                return;
            }
            if (event->modifiers() & Qt::AltModifier) {
                m = tilePosF - tilePos;
                qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;
                bool west = dW < dE && dW < dN && dW < dS;
                bool east = dE <= dW && dE < dN && dE < dS;
                bool south = (dW < dE && !west && dS <= dN) || (dW >= dE && !east && dS <= dN);
                drawGate(tilePos.x(), tilePos.y(), west || south);
                return;
            }
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

void FenceTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mInitialClick) {
        const MapRenderer *renderer = mapDocument()->renderer();
        Layer *layer = currentTileLayer();
        QPointF tilePosF = renderer->pixelToTileCoords(event->scenePos(),
                                                       layer ? layer->level() : 0);
        QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
//        QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
        QPointF m = mStartTilePosF - mStartTilePos;

        qreal dx = tilePosF.x() - mStartTilePosF.x();
        qreal dy = tilePosF.y() - mStartTilePosF.y();
        mapDocument()->undoStack()->beginMacro(tr("Draw Edge"));
        if (qAbs(dx) > qAbs(dy)) {
            drawNorthEdge(mStartTilePos.x(), mStartTilePos.y(), tilePos.x());
        } else {
            drawWestEdge(mStartTilePos.x(), mStartTilePos.y(), tilePos.y());
        }
        mapDocument()->undoStack()->endMacro();
        mInitialClick = false;
        mouseMoved(event->scenePos(), event->modifiers());
    }

//    AbstractTileTool::mouseReleased(event);
}

void FenceTool::languageChanged()
{
    setName(tr("Draw Fence"));
    setShortcut(QKeySequence(/*tr("E")*/));
}

void FenceTool::setFence(Fence *fence)
{
    mFence = fence;
}

void FenceTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
}

QVector<Tile *> FenceTool::resolveTiles(Fence *fence)
{
    QVector<Tile *> ret(Fence::ShapeCount);
    if (!fence) return ret;
    Map *map = mapDocument()->map();
    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, map->tilesets())
        tilesets[ts->name()] = ts;

    for (int i = 0; i < Fence::ShapeCount; i++) {
//        ret[i] = (Tile*) -1;
        if (fence->mTileNames[i].isEmpty())
            continue;
        QString tilesetName;
        int index;
        if (BuildingEditor::BuildingTilesMgr::instance()->parseTileName(fence->mTileNames[i], tilesetName, index)) {
            if (tilesets.contains(tilesetName))
                ret[i] = tilesets[tilesetName]->tileAt(index);
        }
    }

    return ret;
}

void FenceTool::drawWestEdge(int sx, int sy, int ey)
{
#if 1
    TileLayer stamp(QString(), 0, 0, 1, qAbs(ey - sy) + 1);
    getWestEdgeTiles(sx, sy, ey, stamp);

    TileLayer *tileLayer = currentTileLayer();
#else
    QVector<Tile*> tiles = resolveTiles(mFence);
    Tile *t[2] = { tiles[Fence::West1], tiles[Fence::West2] };

    TileLayer stamp(QString(), 0, 0, 1, qAbs(ey - sy) + 1);

    TileLayer *tileLayer = currentTileLayer();

    if (sy > ey) {
        int n = 1;
        for (int y = sy; y >= ey; y--) {
            Tile *CURRENT = tileLayer->cellAt(sx, y).tile;
            if (CURRENT == tiles[Fence::North1])
                stamp.setCell(0, y - ey, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(0, y - ey, Cell(t[n]));
            n = !n;
        }
    } else {
        int n = 0;
        for (int y = sy; y <= ey; y++) {
            Tile *CURRENT = tileLayer->cellAt(sx, y).tile;
            if (CURRENT == tiles[Fence::North1])
                stamp.setCell(0, y - sy, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(0, y - sy, Cell(t[n]));
            n = !n;
        }
    }
#endif

    PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), tileLayer,
                                             sx, qMin(sy, ey), &stamp,
                                             QRect(sx, qMin(sy, ey), stamp.width(), stamp.height()),
                                             false);
//    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
    mapDocument()->emitRegionEdited(QRect(sx, qMin(sy, ey), stamp.width(), stamp.height()), tileLayer);
}

void FenceTool::drawNorthEdge(int sx, int sy, int ex)
{
#if 1
    TileLayer stamp(QString(), 0, 0, qAbs(ex - sx) + 1, 1);
    getNorthEdgeTiles(sx, sy, ex, stamp);

    TileLayer *tileLayer = currentTileLayer();
#else
    QVector<Tile*> tiles = resolveTiles(mFence);
    Tile *t[2] = { tiles[Fence::North1], tiles[Fence::North2] };

    TileLayer stamp(QString(), 0, 0, qAbs(ex - sx) + 1, 1);

    TileLayer *tileLayer = currentTileLayer();

    if (sx > ex) {
        int n = 1;
        for (int x = sx; x >= ex; x--) {
            Tile *CURRENT = tileLayer->cellAt(x, sy).tile;
            if (CURRENT == tiles[Fence::West1])
                stamp.setCell(x - ex, 0, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(x - ex, 0, Cell(t[n]));
            n = !n;
        }
    } else {
        int n = 0;
        for (int x = sx; x <= ex; x++) {
            Tile *CURRENT = tileLayer->cellAt(x, sy).tile;
            if (CURRENT == tiles[Fence::West1])
                stamp.setCell(x - sx, 0, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(x - sx, 0, Cell(t[n]));
            n = !n;
        }
    }
#endif

    PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), tileLayer,
                                             qMin(sx, ex), sy, &stamp,
                                             QRect(qMin(sx, ex), sy, stamp.width(), stamp.height()),
                                             false);
//    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
    mapDocument()->emitRegionEdited(QRect(qMin(sx, ex), sy, stamp.width(), stamp.height()), tileLayer);
}

void FenceTool::drawPost(int x, int y)
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return;

    Tile *CURRENT = tileLayer->cellAt(x, y).tile;

    QVector<Tile*> tiles = resolveTiles(mFence);

    // Drawing a post on an exising one erases it.
    if (CURRENT == tiles[Fence::Post]) {
        EraseTiles *cmd = new EraseTiles(mapDocument(), tileLayer, QRect(x, y, 1, 1));
        cmd->setText(tr("Remove Fence Post"));
        mapDocument()->undoStack()->push(cmd);
        mapDocument()->emitRegionEdited(QRect(x, y, 1, 1), tileLayer);
        return;
    }

    TileLayer stamp(QString(), 0, 0, 1, 1);
    stamp.setCell(0, 0, Cell(tiles[Fence::Post]));
    PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), tileLayer,
                                             x, y, &stamp,
                                             QRect(x, y, stamp.width(), stamp.height()),
                                             false);
    cmd->setText(tr("Draw Fence Post"));
//    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
    mapDocument()->emitRegionEdited(QRect(x, y, stamp.width(), stamp.height()), tileLayer);
}

void FenceTool::drawGate(int x, int y, bool west)
{
    TileLayer *tileLayer = currentTileLayer();
#if 1
    Tile *tile = gateTile(x, y, west);
    if (!tile) return;
#else
    if (!tileLayer->contains(x, y))
        return;

    QVector<Tile*> tiles = resolveTiles(mFence);

    Tile *CURRENT = tileLayer->cellAt(x, y).tile;
    Tile *tile = 0;
    if (CURRENT == tiles[Fence::West1] || CURRENT == tiles[Fence::West2])
        tile = tiles[Fence::GateSpaceW];
    else if (CURRENT == tiles[Fence::North1] || CURRENT == tiles[Fence::North2])
        tile = tiles[Fence::GateSpaceN];
    else
        tile = tiles[west ? Fence::GateDoorW : Fence::GateDoorN];
#endif

    TileLayer stamp(QString(), 0, 0, 1, 1);
    stamp.setCell(0, 0, Cell(tile));
    PaintTileLayer *cmd = new PaintTileLayer(mapDocument(), tileLayer,
                                             x, y, &stamp,
                                             QRect(x, y, stamp.width(), stamp.height()),
                                             false);
//    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
    mapDocument()->emitRegionEdited(QRect(x, y, stamp.width(), stamp.height()), tileLayer);
}

Tile *FenceTool::fenceTile(int x, int y, bool west)
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return 0;

    QVector<Tile*> tiles = resolveTiles(mFence);

    Tile *CURRENT = tileLayer->cellAt(x, y).tile;
    Tile *tile = 0;
    if (CURRENT ==  (west ? tiles[Fence::North1] : tiles[Fence::West1]))
        tile = tiles[Fence::NorthWest];
    else
        tile = tiles[west ? Fence::West1 : Fence::North1];

    return tile;
}

void FenceTool::getWestEdgeTiles(int sx, int sy, int ey, TileLayer &stamp)
{
    QVector<Tile*> tiles = resolveTiles(mFence);
    Tile *t[2] = { tiles[Fence::West1], tiles[Fence::West2] };

    TileLayer *tileLayer = currentTileLayer();

    if (sy > ey) {
        int n = 1;
        for (int y = sy; y >= ey; y--) {
            if (!tileLayer->contains(sx, y))
                continue;
            Tile *CURRENT = tileLayer->cellAt(sx, y).tile;
            if (CURRENT == tiles[Fence::North1])
                stamp.setCell(0, y - ey, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(0, y - ey, Cell(t[n]));
            n = !n;
        }
    } else {
        int n = 0;
        for (int y = sy; y <= ey; y++) {
            if (!tileLayer->contains(sx, y))
                continue;
            Tile *CURRENT = tileLayer->cellAt(sx, y).tile;
            if (CURRENT == tiles[Fence::North1])
                stamp.setCell(0, y - sy, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(0, y - sy, Cell(t[n]));
            n = !n;
        }
    }
}

void FenceTool::getNorthEdgeTiles(int sx, int sy, int ex, TileLayer &stamp)
{
    QVector<Tile*> tiles = resolveTiles(mFence);
    Tile *t[2] = { tiles[Fence::North1], tiles[Fence::North2] };

    TileLayer *tileLayer = currentTileLayer();

    if (sx > ex) {
        int n = 1;
        for (int x = sx; x >= ex; x--) {
            if (!tileLayer->contains(x, sy))
                continue;
            Tile *CURRENT = tileLayer->cellAt(x, sy).tile;
            if (CURRENT == tiles[Fence::West1])
                stamp.setCell(x - ex, 0, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(x - ex, 0, Cell(t[n]));
            n = !n;
        }
    } else {
        int n = 0;
        for (int x = sx; x <= ex; x++) {
            if (!tileLayer->contains(x, sy))
                continue;
            Tile *CURRENT = tileLayer->cellAt(x, sy).tile;
            if (CURRENT == tiles[Fence::West1])
                stamp.setCell(x - sx, 0, Cell(tiles[Fence::NorthWest]));
            else if (CURRENT != tiles[Fence::NorthWest])
                stamp.setCell(x - sx, 0, Cell(t[n]));
            n = !n;
        }
    }
}

Tile *FenceTool::gateTile(int x, int y, bool west)
{
    TileLayer *tileLayer = currentTileLayer();
    if (!tileLayer->contains(x, y))
        return 0;

    QVector<Tile*> tiles = resolveTiles(mFence);

    Tile *CURRENT = tileLayer->cellAt(x, y).tile;
    Tile *tile = 0;
    if (CURRENT == tiles[Fence::West1] || CURRENT == tiles[Fence::West2])
        tile = tiles[Fence::GateSpaceW];
    else if (CURRENT == tiles[Fence::North1] || CURRENT == tiles[Fence::North2])
        tile = tiles[Fence::GateSpaceN];
    else
        tile = tiles[west ? Fence::GateDoorW : Fence::GateDoorN];

    return tile;
}

/////

#include "BuildingEditor/simplefile.h"

#include <QFileInfo>

FenceFile::FenceFile()
{
}

FenceFile::~FenceFile()
{
    qDeleteAll(mFences);
}

bool FenceFile::read(const QString &fileName)
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
        if (block.name == QLatin1String("fence")) {
            QStringList keys;
            keys << QLatin1String("label")
                 << QLatin1String("layer")
                 << QLatin1String("west1")
                 << QLatin1String("west2")
                 << QLatin1String("north1")
                 << QLatin1String("north2")
                 << QLatin1String("gate-space-w")
                 << QLatin1String("gate-space-n")
                 << QLatin1String("gate-door-w")
                 << QLatin1String("gate-door-n")
                 << QLatin1String("nw")
                 << QLatin1String("post");
            if (!validKeys(block, keys))
                return false;

            Fence *fence = new Fence;
            fence->mLabel = block.value("label");
            fence->mLayer = block.value("layer");

            // TODO: validate tile names
            fence->mTileNames[Fence::West1] = block.value("west1");
            fence->mTileNames[Fence::West2] = block.value("west2");
            fence->mTileNames[Fence::North1] = block.value("north1");
            fence->mTileNames[Fence::North2] = block.value("north2");
            fence->mTileNames[Fence::NorthWest] = block.value("nw");
            fence->mTileNames[Fence::GateSpaceW] = block.value("gate-space-w");
            fence->mTileNames[Fence::GateSpaceN] = block.value("gate-space-n");
            fence->mTileNames[Fence::GateDoorW] = block.value("gate-door-w");
            fence->mTileNames[Fence::GateDoorN] = block.value("gate-door-n");
            fence->mTileNames[Fence::Post] = block.value("post");

            mFences += fence;
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

QList<Fence *> FenceFile::takeFences()
{
    QList<Fence *> ret = mFences;
    mFences.clear();
    return ret;
}

bool FenceFile::validKeys(SimpleFileBlock &block, const QStringList &keys)
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
