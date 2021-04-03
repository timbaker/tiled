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

#include "picktiletool.h"

#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "preferences.h"
#include "tilesetmanager.h"

#include "maprenderer.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QDebug>
#include <QGraphicsView>

using namespace Tiled;
using namespace Tiled::Internal;

SINGLETON_IMPL(PickTileTool)

PickTileTool::PickTileTool(QObject *parent) :
    AbstractTileTool(tr("Pick Tile"),
                     QIcon(QLatin1String(":/BuildingEditor/icons/icon_eyedrop.png")),
                     QKeySequence(), parent)
{
}

void PickTileTool::activate(MapScene *scene)
{
    AbstractTileTool::activate(scene);
    const QList<QGraphicsView*> views = scene->views();
    views.first()->setCursor(Qt::PointingHandCursor);
}

void PickTileTool::deactivate(MapScene *scene)
{
    const QList<QGraphicsView*> views = scene->views();
    views.first()->unsetCursor();
    AbstractTileTool::deactivate(scene);
}

void PickTileTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!mapDocument()) return;
    MapComposite *mc = mapDocument()->mapComposite();

    Tile *tile = 0;
    int x = event->scenePos().x(), y = event->scenePos().y();
    bool highlightLevel = Preferences::instance()->highlightCurrentLayer();

    QVector<const Cell*> cells;
    QVector<qreal> opacities;
    foreach (CompositeLayerGroup *lg, mc->layerGroups()) {
        if (!lg->isVisible()) continue;
        if (highlightLevel && lg->level() > mapDocument()->currentLevelIndex()) continue;
        QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(event->scenePos(), lg->level());
        lg->prepareDrawing(mapDocument()->renderer(),
                           mapDocument()->renderer()->boundingRect(
                               QRect(tilePos - QPoint(4, 4), QSize(9, 9)), lg->level()));
        for (int ty = tilePos.y() - 4; ty <= tilePos.y() + 4; ty++) {
            for (int tx = tilePos.x() - 4; tx <= tilePos.x() + 4; tx++) {
                QRectF tileBox = mapDocument()->renderer()->boundingRect(QRect(tx, ty, 1, 1), lg->level());
                cells.resize(0);
                if (!lg->orderedCellsAt(mapDocument()->renderer(), QPoint(tx, ty), cells, opacities))
                    continue;
                for (int i = 0; i < cells.size(); i++) {
                    Tile *test = cells[i]->tile;
                    Tile *realTile = test;
                    if (test->image().isNull()) {
                        test = TilesetManager::instance()->missingTile();
                    }
                    QRect imageBox(test->offset(), test->image().size());
                    QPoint p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height()));
                    if (test->width() == tileBox.width() / 2) {
                        p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height() * 2));
                        p /= 2;
                    }
                    if (imageBox.contains(p.x(), p.y())) {
                        QRgb pixel = test->image().pixel(p.x() - imageBox.x(), p.y() - imageBox.y());
                        if (qAlpha(pixel) > 0)
                            tile = realTile;
                    }
                }
            }
        }
    }

    if (tile)
        emit tilePicked(tile);
}
/*
#include "brushitem.h"
void PickTileTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    MapComposite *mc = mapDocument()->mapComposite();

    Tile *tile = 0;
    QPoint _tilePos;
    int x = pos.x(), y = pos.y();
    bool highlightLevel = Preferences::instance()->highlightCurrentLayer();

    QVector<const Cell*> cells;
    QVector<qreal> opacities;
    foreach (CompositeLayerGroup *lg, mc->layerGroups()) {
        if (!lg->isVisible()) continue;
        if (highlightLevel && lg->level() > mapDocument()->currentLevel()) continue;
        QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(pos, lg->level());
        lg->prepareDrawing(mapDocument()->renderer(),
                           mapDocument()->renderer()->boundingRect(
                               QRect(tilePos - QPoint(4, 4), QSize(9, 9)), lg->level()));
        for (int ty = tilePos.y() - 4; ty <= tilePos.y() + 4; ty++) {
            for (int tx = tilePos.x() - 4; tx <= tilePos.x() + 4; tx++) {
                QRectF tileBox = mapDocument()->renderer()->boundingRect(QRect(tx, ty, 1, 1), lg->level());
                cells.resize(0);
                if (!lg->orderedCellsAt(QPoint(tx, ty), cells, opacities))
                    continue;
                for (int i = 0; i < cells.size(); i++) {
                    Tile *test = cells[i]->tile;
                    QRect imageBox(QPoint(), test->image().size());
                    QPoint p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height()));
                    if (test->width() == mc->map()->tileWidth() / 2) {
                        p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height() * 2));
                        p /= 2;
                    }
                    if (imageBox.contains(p.x(), p.y())) {
                        QRgb pixel = test->image().pixel(p.x(), p.y());
                        if (qAlpha(pixel) > 0) {
                            qDebug() << tilePos << pos << tileBox;
                            tile = test;
                            _tilePos = QPoint(tx, ty);
                        }
                    }
                }
            }
        }
    }

    TileLayer *stamp = new TileLayer(QString(), 0, 0, 1, 1);
    if (tile)
        stamp->setCell(0, 0, Cell(tile));
    brushItem()->setTileLayer(stamp);
//    brushItem()->setTileRegion(QRect(tilePos, QSize(1, 1)));
    brushItem()->setTileLayerPosition(_tilePos);

    AbstractTileTool::mouseMoved(pos, modifiers);
}
*/
void PickTileTool::tilePositionChanged(const QPoint &tilePos)
{
}
