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

#include "buildingtilesetdock.h"
#include "ui_buildingtilesetdock.h"

#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "tileset.h"

#include <QScrollBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTilesetDock::BuildingTilesetDock(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::BuildingTilesetDock),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    mZoomable->setScale(BuildingPreferences::instance()->tileScale());
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)),
            BuildingPreferences::instance(), SLOT(setTileScale(qreal)));
    connect(BuildingPreferences::instance(), SIGNAL(tileScaleChanged(qreal)),
            SLOT(tileScaleChanged(qreal)));

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));

    ui->tiles->model()->setShowHeaders(false);
    connect(ui->tiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(Preferences::instance(), SIGNAL(autoSwitchLayerChanged(bool)),
            SLOT(autoSwitchLayerChanged(bool)));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));
}

BuildingTilesetDock::~BuildingTilesetDock()
{
    delete ui;
}

void BuildingTilesetDock::firstTimeSetup()
{
    if (!ui->tilesets->count())
        setTilesetList(); // TileMetaInfoMgr signals might have done this already.
}

void BuildingTilesetDock::setTilesetList()
{
    ui->tilesets->clear();

    int width = 64;
    QFontMetrics fm = ui->tilesets->fontMetrics();
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesets->verticalScrollBar()->sizeHint().width();
    ui->tilesets->setFixedWidth(width + 16 + sbw);
}

void BuildingTilesetDock::setTilesList()
{
    MixedTilesetModel *model = ui->tiles->model();
    model->setShowLabels(Preferences::instance()->autoSwitchLayer());

    if (!mCurrentTileset || mCurrentTileset->isMissing())
        model->setTiles(QList<Tile*>());
    else {
        QStringList labels;
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            Tile *tile = mCurrentTileset->tileAt(i);
            QString label = TilesetManager::instance()->layerName(tile);
            if (label.isEmpty())
                label = tr("???");
            labels += label;
        }
        model->setTileset(mCurrentTileset, labels);
    }
}

void BuildingTilesetDock::switchLayerForTile(Tiled::Tile *tile)
{
#if 0
    QString layerName = TilesetManager::instance()->layerName(tile);
    if (!layerName.isEmpty()) {
        if (view()->scene()->layerNames().contains(layerName))
            mDocument->setCurrentLayer(layerName);
    }
#endif
}

void BuildingTilesetDock::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    setTilesList();
}

void BuildingTilesetDock::tileSelectionChanged()
{
    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    if (selection.size()) {
        QModelIndex index = selection.first();
        if (Tile *tile = ui->tiles->model()->tileAt(index)) {
            QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
            DrawTileTool::instance()->setTile(tileName);

            switchLayerForTile(tile);
        }
    }
}

void BuildingTilesetDock::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesets->setCurrentRow(row);
}

void BuildingTilesetDock::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesets->takeItem(row);
}

// Called when a tileset image changes or a missing tileset was found.
void BuildingTilesetDock::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        setTilesList();

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesets->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
}

void BuildingTilesetDock::autoSwitchLayerChanged(bool autoSwitch)
{
    ui->tiles->model()->setShowLabels(autoSwitch);
}


void BuildingTilesetDock::tileScaleChanged(qreal scale)
{
    mZoomable->setScale(scale);
}
