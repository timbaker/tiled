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
#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"

#include "zoomable.h"

#include <QSettings>

using namespace BuildingEditor;
using namespace Tiled::Internal;

ChooseBuildingTileDialog::ChooseBuildingTileDialog(const QString &prompt,
                                                   BuildingTileCategory *category,
                                                   BuildingTileEntry *initialTile,
                                                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChooseBuildingTileDialog),
    mCategory(category),
    mZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    ui->prompt->setText(prompt);

    ui->tableView->setZoomable(mZoomable);
    mZoomable->connectToComboBox(ui->scaleCombo);

    connect(ui->tilesButton, SIGNAL(clicked()), SLOT(tilesDialog()));

    setTilesList(mCategory, initialTile);

    connect(ui->tableView, SIGNAL(activated(QModelIndex)), SLOT(accept()));

    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("ChooseBuildingTileDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    qreal scale = settings.value(QLatin1String("scale"),
                                 BuildingPreferences::instance()->tileScale()).toReal();
    mZoomable->setScale(scale);
    settings.endGroup();
}

ChooseBuildingTileDialog::~ChooseBuildingTileDialog()
{
    delete ui;
}

BuildingTileEntry *ChooseBuildingTileDialog::selectedTile() const
{
    QModelIndexList selection = ui->tableView->selectionModel()->selectedIndexes();
    if (selection.count()) {
        QModelIndex index = selection.first();
#if 1
        return ui->tableView->entry(index);
#elif 0
        return static_cast<BuildingTileEntry*>(ui->tableView->model()->userDataAt(index));
#else
        Tiled::Tile *tile = ui->tableView->model()->tileAt(index);
        return mBuildingTiles.at(mTiles.indexOf(tile));
#endif
    }
    return 0;
}

void ChooseBuildingTileDialog::setTilesList(BuildingTileCategory *category,
                                            BuildingTileEntry *initialTile)
{
#if 1
    QList<BuildingTileEntry*> entries;

    if (category->canAssignNone())
        entries += BuildingTilesMgr::instance()->noneTileEntry();

    QMap<QString,BuildingTileEntry*> entryMap;
    int i = 0;
    foreach (BuildingTileEntry *entry, category->entries()) {
        QString key = entry->displayTile()->name() + QString::number(i++);
        entryMap[key] = entry;
    }
    entries += entryMap.values();

    ui->tableView->setEntries(entries);

    if (entries.contains(initialTile))
        ui->tableView->setCurrentIndex(ui->tableView->index(initialTile));
#else
    Tiled::Tile *tile = 0;

    mTiles.clear();
    mBuildingTiles.clear();
    QList<void*> userData;

    if (category->canAssignNone()) {
        mTiles += BuildingTilesMgr::instance()->noneTiledTile();
        mBuildingTiles += BuildingTilesMgr::instance()->noneTileEntry();
        userData += BuildingTilesMgr::instance()->noneTileEntry();
        if (initialTile == mBuildingTiles[0])
            tile = mTiles[0];
    }

    MixedTilesetView *v = ui->tableView;
    QMap<QString,BuildingTileEntry*> entryMap;
    int i = 0;
    foreach (BuildingTileEntry *entry, category->entries()) {
        QString key = entry->displayTile()->name() + QString::number(i++);
        entryMap[key] = entry;
    }
    foreach (BuildingTileEntry *entry, entryMap.values()) {
        mTiles += BuildingTilesMgr::instance()->tileFor(entry->displayTile());
        userData += entry;
        mBuildingTiles += entry;
        if (entry == initialTile)
            tile = mTiles.last();
    }
    v->setTiles(mTiles, userData);

    if (tile != 0)
        v->setCurrentIndex(v->model()->index(tile));
    else
        v->setCurrentIndex(v->model()->index(0, 0));
#endif
}

void ChooseBuildingTileDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("ChooseBuildingTileDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("scale"), mZoomable->scale());
    settings.endGroup();
}

void ChooseBuildingTileDialog::tilesDialog()
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();
    dialog->selectCategory(mCategory);

    QWidget *saveParent = dialog->parentWidget();
    dialog->reparent(this);
    dialog->exec();
    dialog->reparent(saveParent);

    if (dialog->changes()) {
        setTilesList(mCategory);
    }
}

void ChooseBuildingTileDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void ChooseBuildingTileDialog::reject()
{
    saveSettings();
    QDialog::reject();
}
