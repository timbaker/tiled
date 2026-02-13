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

#include "customtilesize.h"
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
    scene->views().first()->setCursor(Qt::PointingHandCursor);
}

void PickTileTool::deactivate(MapScene *scene)
{
    scene->views().first()->unsetCursor();
    AbstractTileTool::deactivate(scene);
}

void PickTileTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    pickTile(pos); // display debug info
}

void PickTileTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (Tiled::Tile *tile = pickTile(event->scenePos())) {
        emit tilePicked(tile);
    }
}

void PickTileTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
}

Tile *PickTileTool::pickTile(const QPointF &pos)
{
    if (mapDocument() == nullptr) {
        return nullptr;
    }
    MapComposite *mc = mapDocument()->mapComposite();
    Tiled::Tile *tile = nullptr;
    QPoint imagePos;
    QRgb imageRgb;
    int x = pos.x(), y = pos.y();
    bool highlightLevel = Preferences::instance()->highlightCurrentLayer();
    QVector<const Cell*> cells;
    QVector<qreal> opacities;
    OrderedCellsTemporaries vars;
    foreach (CompositeLayerGroup *lg, mc->layerGroups()) {
        if (!lg->isVisible()) continue;
        if (highlightLevel && lg->level() > mapDocument()->currentLevel()) continue;
        QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(pos, lg->level());
        const int DXY = 16; // must handle JUMBOXXL
        lg->prepareDrawing(mapDocument()->renderer(),
                           mapDocument()->renderer()->boundingRect(
                               QRect(tilePos - QPoint(DXY, DXY), QSize(DXY*2+1, DXY*2+1)), lg->level()));
        for (int ty = tilePos.y() - DXY; ty <= tilePos.y() + DXY; ty++) {
            for (int tx = tilePos.x() - DXY; tx <= tilePos.x() + DXY; tx++) {
                QRectF tileBox = mapDocument()->renderer()->boundingRect(QRect(tx, ty, 1, 1), lg->level());
                cells.resize(0);
                if (!lg->orderedCellsAt(QPoint(tx, ty), cells, opacities, reinterpret_cast<ZTileLayerGroupRenderData*>(&vars)))
                    continue;
                for (int i = 0; i < cells.size(); i++) {
                    Tile *test = cells[i]->tile;
                    Tile *realTile = test;
                    if (test->properties().contains(QLatin1String("invisible"))) {
                        test = TilesetManager::instance()->invisibleTile();
                    }
                    if (test->image().isNull()) {
                        test = TilesetManager::instance()->missingTile();
                    }
                    QRect imageBox(test->offset(), test->image().size());
                    QPoint p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height()));
                    QSize customSize = CustomTileSize::forTileset(test->tileset()->name());
                    if (!customSize.isEmpty()) {
                        QRectF tileBox2 = tileBox.translated(-(customSize.width() - 64), 0);
                        p = QPoint(x, y) - (tileBox2.bottomLeft().toPoint() - QPoint(0, test->height()));
                    }
                    else if (test->width() == qRound(tileBox.width()) / 2) {
                        p = QPoint(x, y) - (tileBox.bottomLeft().toPoint() - QPoint(0, test->height() * 2));
                        p /= 2;
                    }
                    if (imageBox.contains(p.x(), p.y())) {
                        QRgb pixel = test->image().pixel(p.x() - imageBox.x(), p.y() - imageBox.y());
                        if (qAlpha(pixel) > 0) {
                            tile = realTile;
                            imagePos = QPoint(p.x() - imageBox.x(), p.y() - imageBox.y());
                            imageRgb = pixel;
                        }
                    }
                }
            }
        }
    }
    if (tile) {
        qDebug() << QStringLiteral("%1_%2 %3,%4 %5,%6,%7,%8")
                    .arg(tile->tileset()->name()).arg(tile->id())
                    .arg(imagePos.x()).arg(imagePos.y())
                    .arg(qRed(imageRgb)).arg(qGreen(imageRgb)).arg(qBlue(imageRgb)).arg(qAlpha(imageRgb));
    }
    return tile;
}
