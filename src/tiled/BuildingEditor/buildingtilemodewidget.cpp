/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "buildingtilemodewidget.h"
#include "ui_buildingtilemodewidget.h"

#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingtilemodeview.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"

#include "mapcomposite.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "tilelayer.h"
#include "tileset.h"

#include <QScrollBar>
#include <QToolBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTileModeWidget::BuildingTileModeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BuildingTileModeWidget),
    mDocument(0),
    mCurrentTileset(0)
{
    ui->setupUi(this);

    mToolBar = new QToolBar;
    mToolBar->setObjectName(QString::fromUtf8("ToolBar"));
    mToolBar->addAction(ui->actionPecil);

    ui->view->setScene(new BuildingTileModeScene(this));

    ui->overallSplitter->setStretchFactor(0, 1);

    connect(ui->actionPecil, SIGNAL(triggered()),
            DrawTileTool::instance(), SLOT(makeCurrent()));
    DrawTileTool::instance()->setEditor(view()->scene());
    DrawTileTool::instance()->setAction(ui->actionPecil);

    connect(ui->layers, SIGNAL(currentRowChanged(int)),
            SLOT(currentLayerChanged(int)));
    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));
}

BuildingTileModeWidget::~BuildingTileModeWidget()
{
    delete ui;
}

BuildingTileModeView *BuildingTileModeWidget::view()
{
    return ui->view;
}

void BuildingTileModeWidget::setDocument(BuildingDocument *doc)
{
    mDocument = doc;
    view()->scene()->setDocument(doc);
    setLayersList();
}

void BuildingTileModeWidget::clearDocument()
{
    mDocument = 0;
    view()->scene()->clearDocument();
    setLayersList();
}

void BuildingTileModeWidget::switchTo()
{
    if (!ui->tilesets->count())
        setTilesetList();
}

void BuildingTileModeWidget::setLayersList()
{
    ui->layers->clear();
    if (mDocument) {
        int level = mDocument->currentLevel();
        if (CompositeLayerGroup *lg = view()->scene()->mapComposite()->layerGroupForLevel(level)) {
            foreach (TileLayer *tl, lg->layers()) {
                QListWidgetItem *item = new QListWidgetItem;
                item->setText(MapComposite::layerNameWithoutPrefix(tl));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
                ui->layers->insertItem(0, item);
            }
        }
    }
}

void BuildingTileModeWidget::setTilesetList()
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

void BuildingTileModeWidget::setTilesList()
{
    MixedTilesetModel *model = ui->tiles->model();
    if (!mCurrentTileset || mCurrentTileset->isMissing())
        model->setTiles(QList<Tile*>());
    else
        model->setTileset(mCurrentTileset);
}

void BuildingTileModeWidget::currentLayerChanged(int row)
{
    if (!mDocument)
        return;
    QString layerName;
    if (row >= 0)
        layerName = ui->layers->item(row)->text();
    mDocument->setCurrentLayer(layerName);
}

void BuildingTileModeWidget::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    setTilesList();
}

void BuildingTileModeWidget::tileSelectionChanged()
{
    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    if (selection.size()) {
        QModelIndex index = selection.first();
        if (Tile *tile = ui->tiles->model()->tileAt(index)) {
            QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
            DrawTileTool::instance()->setTile(tileName);
        }
    }
}

void BuildingTileModeWidget::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesets->setCurrentRow(row);
}

void BuildingTileModeWidget::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesets->takeItem(row);
}

// Called when a tileset image changes or a missing tileset was found.
void BuildingTileModeWidget::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        setTilesList();

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesets->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
}
