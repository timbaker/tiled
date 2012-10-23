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

#include "zoomable.h"

#include "tileset.h"

#include <QDebug>
#include <QScrollBar>
#include <QSettings>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTilesDialog::BuildingTilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTilesDialog),
    mZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    qreal scale = settings.value(QLatin1String("CategoryScale"), 0.5).toReal();
    settings.endGroup();
    mZoomable->setScale(scale);

    connect(ui->addTiles, SIGNAL(clicked()), SLOT(addTiles()));
    connect(ui->removeTiles, SIGNAL(clicked()), SLOT(removeTiles()));

    // Add tile categories to the gui
    while (ui->toolBox->count()) {
        QWidget *w = ui->toolBox->widget(0);
        ui->toolBox->removeItem(0);
        delete w;
    }
    foreach (BuildingTiles::Category *category, BuildingTiles::instance()->categories()) {
        Tiled::Internal::MixedTilesetView *w
                = new Tiled::Internal::MixedTilesetView(mZoomable, ui->toolBox);
        w->setSelectionMode(QAbstractItemView::ExtendedSelection);
        ui->toolBox->addItem(w, category->label());

        connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                SLOT(synchUI()));

        setCategoryTiles(category->name());
    }
    connect(ui->toolBox, SIGNAL(currentChanged(int)), SLOT(categoryChanged(int)));
    mCategoryName = BuildingTiles::instance()->categories().at(ui->toolBox->currentIndex())->name();

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

    synchUI();
}

BuildingTilesDialog::~BuildingTilesDialog()
{
    delete ui;
}

void BuildingTilesDialog::setCategoryTiles(const QString &categoryName)
{
    BuildingTiles::Category *category = BuildingTiles::instance()->category(categoryName);
    int categoryIndex = BuildingTiles::instance()->categories().indexOf(category);
    MixedTilesetView *v = static_cast<MixedTilesetView*>(ui->toolBox->widget(categoryIndex));

    QList<Tiled::Tile*> tiles;
    foreach (BuildingTile *tile, category->tiles()) {
        if (!tile->mAlternates.count() || (tile == tile->mAlternates.first()))
            tiles += BuildingTiles::instance()->tileFor(tile);
    }
    v->model()->setTiles(tiles);
}

void BuildingTilesDialog::synchUI()
{
    ui->addTiles->setEnabled(!mCategoryName.isEmpty() &&
                             ui->tableWidget->selectionModel()->selectedIndexes().count());
    ui->removeTiles->setEnabled(!mCategoryName.isEmpty() &&
                                static_cast<MixedTilesetView*>(ui->toolBox->currentWidget())->selectionModel()->selectedIndexes().count());
}

void BuildingTilesDialog::categoryChanged(int index)
{
    mCategoryName = BuildingTiles::instance()->categories().at(index)->name();
    tilesetSelectionChanged();
    synchUI();
}

void BuildingTilesDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->listWidget->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        BuildingTiles::Category *category = BuildingTiles::instance()->category(mCategoryName);
        QRect bounds = category->categoryBounds();
        int row = ui->listWidget->row(item);
        MixedTilesetModel *model = ui->tableWidget->model();
        Tileset *tileset = BuildingTiles::instance()->tilesets().at(row);
        model->setTileset(tileset);
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (category->usesTile(tile))
                model->setCategoryBounds(tile, bounds);
        }
    }
}

void BuildingTilesDialog::addTiles()
{
    if (mCategoryName.isEmpty())
        return;
    BuildingTiles::Category *category = BuildingTiles::instance()->category(mCategoryName);

    QModelIndexList selection = ui->tableWidget->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        Tile *tile = ui->tableWidget->model()->tileAt(index);
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        if (!category->usesTile(tile))
            category->add(tileName);
    }

    tilesetSelectionChanged();
    setCategoryTiles(mCategoryName);
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}

void BuildingTilesDialog::removeTiles()
{
    if (mCategoryName.isEmpty())
        return;
    BuildingTiles::Category *category = BuildingTiles::instance()->category(mCategoryName);

    MixedTilesetView *v = static_cast<MixedTilesetView*>(ui->toolBox->currentWidget());
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        Tile *tile = v->model()->tileAt(index);
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        category->remove(tileName);
    }

    tilesetSelectionChanged();
    setCategoryTiles(mCategoryName);
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}
