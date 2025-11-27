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

#include "worldlottool.h"

#include "mainwindow.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "preferences.h"
#include "worldconstants.h"
#include "ZomboidScene.h"

#include "worlded/world.h"
#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"

#include "maprenderer.h"

#include <QDir>
#include <QGraphicsPolygonItem>
#include <QMenu>

using namespace Tiled;
using namespace Tiled::Internal;

WorldLotTool *WorldLotTool::mInstance = 0;

WorldLotTool *WorldLotTool::instance()
{
    if (!mInstance)
        mInstance = new WorldLotTool;
    return mInstance;
}

void WorldLotTool::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

WorldLotTool::WorldLotTool(QObject *parent) :
    AbstractTool(tr("WorldEd Lot Tool"),
                 QIcon(QLatin1String(":images/worlded-icon-16.png")),
                 QKeySequence(), parent),
    mScene(0),
    mCell(0),
    mHoverItem(0),
    mHoverLot(0),
    mShowingContextMenu(false)
{
    connect(WorldEd::WorldEdMgr::instance(), &WorldEd::WorldEdMgr::beforeWorldChanged,
            this, &WorldLotTool::beforeWorldChanged);
    connect(WorldEd::WorldEdMgr::instance(), &WorldEd::WorldEdMgr::afterWorldChanged,
            this, &WorldLotTool::afterWorldChanged);
    connect(WorldEd::WorldEdMgr::instance(), &WorldEd::WorldEdMgr::lotVisibilityChanged,
            this, &WorldLotTool::lotVisibilityChanged);
}

WorldLotTool::~WorldLotTool()
{
}

void WorldLotTool::activate(MapScene *scene)
{
    mScene = scene;
    mCell = WorldEd::WorldEdMgr::instance()->cellForMap(mScene->mapDocument()->fileName());
}

void WorldLotTool::deactivate(MapScene *scene)
{
    Q_UNUSED(scene)
    if (mHoverItem) {
        delete mHoverItem;
        mHoverItem = 0;
        mHoverLot = 0;
    }
    mScene = 0;
    mCell = 0;
}

void WorldLotTool::mouseEntered()
{
}

void WorldLotTool::mouseLeft()
{
    updateHoverItem(0);
}

void WorldLotTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)
    updateHoverItem(topmostLotAt(pos));
    mLastScenePos = pos;
}

void WorldLotTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        updateHoverItem(topmostLotAt(event->scenePos()));
        QSet<WorldCellLot*> selection;
        if (mHoverLot && (mHoverLot->cell() == mCell || ((ZomboidScene*)mScene)->lotManager().overlappingLots().contains(mHoverLot))) {
            selection << mHoverLot;
        }
        WorldEd::WorldEdMgr::instance()->setSelectedLots(selection);
    }

    if (event->button() == Qt::RightButton) {
        updateHoverItem(topmostLotAt(event->scenePos()));
        if (mHoverLot) {
            WorldCellLot *hover = mHoverLot;

            QMenu menu;
            QAction *hideAction = menu.addAction(tr("Hide Lot"));
            QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
            QAction *openAction = menu.addAction(tiledIcon, tr("Open in TileZed"));
            QString fileName = mHoverLot->mapName();
            openAction->setEnabled(QFileInfo::exists(fileName));

            if (mHoverLot->cell() != mCell)
                hideAction->setVisible(false);

            mShowingContextMenu = true;
            QAction *selected = menu.exec(event->screenPos());
            mShowingContextMenu = false;

            if (selected == hideAction)
                WorldEd::WorldEdMgr::instance()->setLotVisible(hover, false);
            else if (selected == openAction)
                MainWindow::instance()->openFile(hover->mapName());
        } else if (WorldCell *cell = adjacentCellAt(event->scenePos())) {
            QMenu menu;
            QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
            QAction *openAction = menu.addAction(tiledIcon, tr("Open in TileZed"));
            QString fileName = cell->mapFilePath();
            openAction->setEnabled(QFileInfo::exists(fileName));

            mShowingContextMenu = true;
            QAction *selected = menu.exec(event->screenPos());
            mShowingContextMenu = false;

            if (selected == openAction)
                MainWindow::instance()->openFile(fileName);
        }
    }
}

void WorldLotTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}

void WorldLotTool::languageChanged()
{
}

WorldCellLot *WorldLotTool::topmostLotAt(const QPointF &scenePos)
{
    WorldCellLot *hover = 0;
    if (mCell) {
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                if (!Preferences::instance()->showAdjacentMaps() && !(x == 0 && y == 0)) {
                    continue;
                }
                WorldCell *cell = mCell->world()->cellAt(mCell->pos() + QPoint(x, y));
                if (!cell) continue;
                foreach (WorldCellLevel *level, cell->levels()) {
                    if (!level->isVisible())
                        continue;
                    if (Preferences::instance()->highlightCurrentLayer()) {
                        int currentLevel = mapDocument()->currentLevel();
//                        if (currentLevel == INVALID_LEVEL) {
//                            if (Layer *layer = mapDocument()->currentLayer()) {
//                                currentLevel = layer->level();
//                            }
//                        }
                        if ((currentLevel != INVALID_LEVEL) && (level->z() > currentLevel)) {
                            continue;
                        }
                    }
                    QPoint tilePos = mScene->mapDocument()->renderer()->pixelToTileCoordsInt(scenePos, level->z());
                    foreach (WorldCellLot *lot, level->lots()) {
                        if (!lot->isVisible())
                            continue;
                        QPoint origin;
                        switch (x) {
                        case -1: origin.setX(-300); break;
                        case 1: origin.setX(300); break;
                        }
                        switch (y) {
                        case -1: origin.setY(-300); break;
                        case 1: origin.setY(300); break;
                        }
                        if (lot->bounds().contains(tilePos - origin)) {
                            hover = lot; // keep going to find the top-most one
                        }
                    }
                }
            }
        }
        for (WorldCellLot *lot : ((ZomboidScene*)mScene)->lotManager().overlappingLots()) {
            if (!lot->isVisible())
                continue;
            if (WorldCellLevel *worldLevel = mCell->levelForZ(lot->level())) {
                if (!worldLevel->isVisible()) {
                    continue;
                }
            }
            QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(scenePos, lot->level());
            if (lot->bounds().translated((lot->cell()->pos() - mCell->pos()) * 300).contains(tilePos)) {
                hover = lot;
            }
        }
    }
    return hover;
}

WorldCell *WorldLotTool::adjacentCellAt(const QPointF &scenePos)
{
    if (!Preferences::instance()->showAdjacentMaps()) {
        return nullptr;
    }
    if (mCell) {
        QPoint tilePos = mScene->mapDocument()->renderer()->pixelToTileCoordsInt(
                    scenePos, mScene->mapDocument()->currentLevel());
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                if (!x && !y) continue;
                WorldCell *cell = mCell->world()->cellAt(mCell->pos() + QPoint(x, y));
                if (!cell) continue;
                QPoint origin;
                switch (x) {
                case -1: origin.setX(-300); break;
                case 1: origin.setX(300); break;
                }
                switch (y) {
                case -1: origin.setY(-300); break;
                case 1: origin.setY(300); break;
                }
                if (QRect(origin, QSize(300,300)).contains(tilePos))
                    return cell;
            }
        }
    }
    return 0;
}

void WorldLotTool::updateHoverItem(WorldCellLot *lot)
{
    // Ignore mouse-leave event
    if (mShowingContextMenu)
        return;

    if (lot != mHoverLot) {
        if (mHoverLot) {
            mHoverItem->setVisible(false);
            mHoverLot = 0;
        }
        if (lot) {
            mHoverLot = lot;
            if (!mHoverItem) {
                mHoverItem = new QGraphicsPolygonItem;
                mHoverItem->setPen(Qt::NoPen);
                mHoverItem->setBrush(QColor(128, 128, 128, 128));
                mHoverItem->setZValue(1000);
                mScene->addItem(mHoverItem);
            }

            QPoint origin;
            switch (lot->cell()->x() - mCell->x()) {
            case -1: origin.setX(-300); break;
            case 1: origin.setX(300); break;
            }
            switch (lot->cell()->y() - mCell->y()) {
            case -1: origin.setY(-300); break;
            case 1: origin.setY(300); break;
            }
            QRect lotBounds = lot->bounds().translated(origin);

            QPolygonF polygon = mScene->mapDocument()->renderer()->tileToPixelCoords(lotBounds, lot->level());
            mHoverItem->setPolygon(polygon);
            mHoverItem->setVisible(true);
            mHoverItem->setToolTip(QDir::toNativeSeparators(lot->mapName()));
        }
    }
}

void WorldLotTool::beforeWorldChanged()
{
    mCell = nullptr;
    mHoverLot = nullptr;
}

void WorldLotTool::afterWorldChanged()
{
    if (!mScene)
        return;
    mCell = WorldEd::WorldEdMgr::instance()->cellForMap(mScene->mapDocument()->fileName());
}

void WorldLotTool::lotVisibilityChanged(WorldCellLot *lot)
{
    if (!mScene)
        return;
    if (lot == mHoverLot && !lot->isVisible())
        updateHoverItem(0);
}

