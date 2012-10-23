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

#include "choosebuildingtiledialog.h"
#include "ui_choosebuildingtiledialog.h"

#include "buildingeditorwindow.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"

#include "zoomable.h"

#include <QSettings>

using namespace BuildingEditor;
using namespace Tiled::Internal;

ChooseBuildingTileDialog::ChooseBuildingTileDialog(const QString &prompt,
                                                   const QString &categoryName,
                                                   BuildingTile *initialTile, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChooseBuildingTileDialog),
    mCategoryName(categoryName),
    mZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    ui->prompt->setText(prompt);

    ui->tableView->setZoomable(mZoomable);
    mZoomable->setScale(QSettings().value(QLatin1String("BuildingEditor/MainWindow/CategoryScale"), 0.5).toReal());

    connect(ui->tilesButton, SIGNAL(clicked()), SLOT(tilesDialog()));

    setTilesList(mCategoryName, initialTile);

    connect(ui->tableView, SIGNAL(activated(QModelIndex)), SLOT(accept()));
}

ChooseBuildingTileDialog::~ChooseBuildingTileDialog()
{
    delete ui;
}

BuildingTile *ChooseBuildingTileDialog::selectedTile() const
{
    QModelIndexList selection = ui->tableView->selectionModel()->selectedIndexes();
    if (selection.count()) {
        QModelIndex index = selection.first();
        Tiled::Tile *tile = ui->tableView->model()->tileAt(index);
        return mBuildingTiles.at(mTiles.indexOf(tile));
    }
    return 0;
}

void ChooseBuildingTileDialog::setTilesList(const QString &categoryName,
                                            BuildingTile *initialTile)
{
    Tiled::Tile *tile = 0;

    mTiles.clear();

    MixedTilesetView *v = ui->tableView;
    BuildingTiles::Category *category = BuildingTiles::instance()->category(categoryName);
    foreach (BuildingTile *btile, category->tiles()) {
        if (!btile->mAlternates.count() || (btile == btile->mAlternates.first())) {
            mTiles += BuildingTiles::instance()->tileFor(btile);
            mBuildingTiles += btile;
            if (btile == initialTile)
                tile = mTiles.last();
        }
    }
    v->model()->setTiles(mTiles);

    if (tile != 0)
        v->setCurrentIndex(v->model()->index(tile));
    else
        v->setCurrentIndex(v->model()->index(0, 0));
}

void ChooseBuildingTileDialog::tilesDialog()
{
    BuildingTilesDialog dialog(this);
    dialog.exec();

    BuildingTiles::instance()->writeBuildingTilesTxt();

    setTilesList(mCategoryName);
}

void ChooseBuildingTileDialog::accept()
{
    QDialog::accept();
}
