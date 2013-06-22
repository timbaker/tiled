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
#include "worldeddock.h"

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
    mHoverItem(0),
    mHoverLot(0),
    mShowingContextMenu(false)
{
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(lotVisibilityChanged(WorldCellLot*)),
            SLOT(lotVisibilityChanged(WorldCellLot*)));
}

WorldLotTool::~WorldLotTool()
{
}

void WorldLotTool::activate(MapScene *scene)
{
    mScene = scene;
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
        if (mHoverLot)
            WorldEd::WorldEdMgr::instance()->setSelectedLots(QSet<WorldCellLot*>() << mHoverLot);
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
            openAction->setEnabled(QFileInfo(fileName).exists());

            mShowingContextMenu = true;
            QAction *selected = menu.exec(event->screenPos());
            mShowingContextMenu = false;

            if (selected == hideAction)
                WorldEd::WorldEdMgr::instance()->setLotVisible(hover, false);
            else if (selected == openAction)
                MainWindow::instance()->openFile(hover->mapName());
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
    if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mScene->mapDocument()->fileName())) {
        foreach (WorldCellLevel *level, cell->levels()) {
            if (!level->isVisible()) continue;
            foreach (WorldCellLot *lot, level->lots()) {
                if (!lot->isVisible()) continue;
                QPoint tilePos = mScene->mapDocument()->renderer()->pixelToTileCoordsInt(scenePos, level->z());
                if (lot->bounds().contains(tilePos))
                    hover = lot; // keep going to find the top-most one
            }
        }
    }
    return hover;
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
            QPolygonF polygon = mScene->mapDocument()->renderer()->tileToPixelCoords(lot->bounds());
            mHoverItem->setPolygon(polygon);
            mHoverItem->setVisible(true);
            mHoverItem->setToolTip(QDir::toNativeSeparators(lot->mapName()));
        }
    }
}

void WorldLotTool::lotVisibilityChanged(WorldCellLot *lot)
{
    if (lot == mHoverLot && !lot->isVisible())
        updateHoverItem(0);
}

