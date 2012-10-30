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

#include "buildingtilesdialog.h"
#include "ui_buildingtilesdialog.h"

#include "buildingtiles.h"
#include "furnituregroups.h"
#include "furnitureview.h"

#include "zoomable.h"

#include "tileset.h"

#include <QDebug>
#include <QScrollBar>
#include <QSettings>
#include <QToolBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTilesDialog::BuildingTilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTilesDialog),
    mZoomable(new Zoomable(this)),
    mCategory(0),
    mFurnitureGroup(0),
    mChanges(false)
{
    ui->setupUi(this);

    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    qreal scale = settings.value(QLatin1String("CategoryScale"), 0.5).toReal();
    settings.endGroup();
    mZoomable->setScale(scale);

    connect(ui->addTiles, SIGNAL(clicked()), SLOT(addTiles()));
    connect(ui->removeTiles, SIGNAL(clicked()), SLOT(removeTiles()));

    ui->tilesetView->setZoomable(mZoomable);
    connect(ui->tilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->furnitureView->model(), SIGNAL(edited()), SLOT(furnitureEdited()));
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    setCategoryList();
    connect(ui->categoryList, SIGNAL(currentRowChanged(int)),
            SLOT(categoryChanged(int)));
    connect(ui->categoryList, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(categoryNameEdited(QListWidgetItem*)));
//    mCategory = BuildingTiles::instance()->categories().at(ui->categoryList->currentRow());

    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->listWidget->fontMetrics();
    foreach (Tileset *tileset, BuildingTiles::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        ui->listWidget->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    int sbw = ui->listWidget->verticalScrollBar()->sizeHint().width();
    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    ui->listWidget->setFixedWidth(width + 16 + sbw);

    connect(ui->listWidget, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));

    ui->tableWidget->setZoomable(mZoomable);
    ui->tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tableWidget->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    ui->tableWidget->setDragEnabled(true);

    /////
    QToolBar *toolBar = new QToolBar();
    toolBar->addAction(ui->actionNewCategory);
    toolBar->addAction(ui->actionRemoveCategory);
    toolBar->addAction(ui->actionToggleCorners);
    connect(ui->actionRemoveCategory, SIGNAL(triggered()), SLOT(removeCategory()));
    connect(ui->actionNewCategory, SIGNAL(triggered()), SLOT(newCategory()));
    connect(ui->actionToggleCorners, SIGNAL(triggered()), SLOT(toggleCorners()));
    ui->categoryToolbarLayout->addWidget(toolBar);
    /////

    synchUI();

    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();

    restoreSplitterSizes(ui->overallSplitter);
    restoreSplitterSizes(ui->categorySplitter);
    restoreSplitterSizes(ui->tilesetSplitter);
}

BuildingTilesDialog::~BuildingTilesDialog()
{
    delete ui;
}

void BuildingTilesDialog::setCategoryList()
{
    ui->categoryList->clear();

    foreach (BuildingTileCategory *category, BuildingTiles::instance()->categories())
        ui->categoryList->addItem(category->label());

    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(group->mLabel);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->categoryList->addItem(item);
    }
}

void BuildingTilesDialog::setCategoryTiles()
{
    QList<Tiled::Tile*> tiles;
    foreach (BuildingTile *tile, mCategory->tiles()) {
        if (!tile->mAlternates.count() || (tile == tile->mAlternates.first()))
            tiles += BuildingTiles::instance()->tileFor(tile);
    }
    ui->tilesetView->model()->setTiles(tiles);
}

void BuildingTilesDialog::setFurnitureTiles()
{
    ui->furnitureView->model()->setTiles(mFurnitureGroup->mTiles);
}

void BuildingTilesDialog::saveSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    settings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    settings.endGroup();
}

void BuildingTilesDialog::restoreSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QVariant v = settings.value(tr("%1/sizes").arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
    settings.endGroup();
}

void BuildingTilesDialog::synchUI()
{
    bool add = false;
    bool remove = false;
    if (mFurnitureGroup) {
        add = true;
        remove = ui->furnitureView->selectionModel()->selectedIndexes().count();
    } else if (mCategory) {
        add = ui->tableWidget->selectionModel()->selectedIndexes().count();
        remove = ui->tilesetView->selectionModel()->selectedIndexes().count();
    }
    ui->addTiles->setEnabled(add);
    ui->removeTiles->setEnabled(remove);

    ui->actionRemoveCategory->setEnabled(mFurnitureGroup != 0);
    ui->actionToggleCorners->setEnabled(mFurnitureGroup && remove);
}

void BuildingTilesDialog::categoryChanged(int index)
{
    mCategory = 0;
    mFurnitureGroup = 0;
    if (index < 0) {
        // only happens when setting the list again
    } else if (index < BuildingTiles::instance()->categories().count()) {
        mCategory = BuildingTiles::instance()->categories().at(index);
        ui->categoryStack->setCurrentIndex(0);
        setCategoryTiles();
        tilesetSelectionChanged();
    } else {
        index -= BuildingTiles::instance()->categories().count();
        mFurnitureGroup = FurnitureGroups::instance()->groups().at(index);
        setFurnitureTiles();
        ui->categoryStack->setCurrentIndex(1);
    }
    synchUI();
}

void BuildingTilesDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->listWidget->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        QRect bounds;
        if (mCategory)
            bounds = mCategory->categoryBounds();

        int row = ui->listWidget->row(item);
        MixedTilesetModel *model = ui->tableWidget->model();
        Tileset *tileset = BuildingTiles::instance()->tilesets().at(row);
        model->setTileset(tileset);
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (mCategory && mCategory->usesTile(tile))
                model->setCategoryBounds(tile, bounds);
        }
    }
    synchUI();
}

void BuildingTilesDialog::addTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureTiles *tiles = new FurnitureTiles;
        tiles->mTiles[0] = new FurnitureTile(tiles, FurnitureTile::FurnitureW);
        tiles->mTiles[1] = new FurnitureTile(tiles, FurnitureTile::FurnitureN);
        tiles->mTiles[2] = new FurnitureTile(tiles, FurnitureTile::FurnitureE);
        tiles->mTiles[3] = new FurnitureTile(tiles, FurnitureTile::FurnitureS);
        mFurnitureGroup->mTiles += tiles;
        ui->furnitureView->model()->setTiles(mFurnitureGroup->mTiles);
        mChanges = true;
        return;
    }

    if (!mCategory)
        return;

    QModelIndexList selection = ui->tableWidget->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        Tile *tile = ui->tableWidget->model()->tileAt(index);
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        if (!mCategory->usesTile(tile)) {
            mCategory->add(tileName);
            mChanges = true;
        }
    }

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}

void BuildingTilesDialog::removeTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTiles*> remove;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *tile = v->model()->tileAt(index);
            if (tile->owner()->isEmpty()) {
                if (!remove.contains(tile->owner()))
                    remove += tile->owner();
            } else {
                tile->clear();
                v->update(index);
            }
        }
        foreach (FurnitureTiles *tiles, remove) {
            mFurnitureGroup->mTiles.removeOne(tiles);
            v->model()->removeTiles(tiles);
#if 0
            delete tiles; // don't delete, may still be in use by furniture objects
#endif
        }
        mChanges = true;
        return;
    }

    if (!mCategory)
        return;

    MixedTilesetView *v = ui->tilesetView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        Tile *tile = v->model()->tileAt(index);
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        mCategory->remove(tileName);
        mChanges = true;
    }

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}

void BuildingTilesDialog::furnitureEdited()
{
    mChanges = true;
}

void BuildingTilesDialog::categoryNameEdited(QListWidgetItem *item)
{
    int row = ui->categoryList->row(item);
    row -= BuildingTiles::instance()->categories().count();
    FurnitureGroup *category = FurnitureGroups::instance()->groups().at(row);
    category->mLabel = item->text();
    mChanges = true;
}

void BuildingTilesDialog::newCategory()
{
    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("New Category");
    FurnitureGroups::instance()->addGroup(group);
    setCategoryList();
    ui->categoryList->setCurrentRow(ui->categoryList->count() - 1);
    mChanges = true;
    synchUI();
}

void BuildingTilesDialog::removeCategory()
{
    if (!mFurnitureGroup)
        return;

    FurnitureGroups::instance()->removeGroup(mFurnitureGroup);
    delete ui->categoryList->currentItem();
    mChanges = true;
    synchUI();
}

void BuildingTilesDialog::toggleCorners()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTiles*> toggle;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *tile = v->model()->tileAt(index);
            if (!toggle.contains(tile->owner()))
                toggle += tile->owner();
        }
        foreach (FurnitureTiles *tiles, toggle) {
            tiles->toggleCorners();
            v->update(v->model()->index(tiles->mTiles[0]));
            v->update(v->model()->index(tiles->mTiles[1]));
            v->update(v->model()->index(tiles->mTiles[2]));
            v->update(v->model()->index(tiles->mTiles[3]));
        }
        mChanges = true;
        return;
    }
}

void BuildingTilesDialog::accept()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();

    saveSplitterSizes(ui->overallSplitter);
    saveSplitterSizes(ui->categorySplitter);
    saveSplitterSizes(ui->tilesetSplitter);

    QDialog::accept();
}
