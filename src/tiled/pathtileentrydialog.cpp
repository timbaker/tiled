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

#include "pathtileentrydialog.h"
#include "ui_pathtileentrydialog.h"

#include "pathgeneratormgr.h"
#include "zoomable.h"

#include "pathgenerator.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/buildingtilesdialog.h"

#include <QSettings>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

PathTileEntryDialog::PathTileEntryDialog(const QString &prompt, const QString &category,
                                         PGP_TileEntry *initialTile,
                                         QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PathTileEntryDialog),
    mCategory(category),
    mZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    ui->prompt->setText(prompt);

    ui->tableView->setZoomable(mZoomable);
    mZoomable->setScale(0.5); // FIXME

    connect(ui->tilesButton, SIGNAL(clicked()), SLOT(tilesDialog()));

    setTilesList(mCategory, initialTile);

    connect(ui->tableView, SIGNAL(activated(QModelIndex)), SLOT(accept()));

    QSettings settings;
    settings.beginGroup(QLatin1String("PathTileEntryDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();

#if 1
    if (category == QLatin1String("grime_wall")) {
        foreach (PGP_TileEntry *entry, getEntriesInCategory(QLatin1String("walls"))) {
            PG_WallGrime::registerWallTiles(entry);
            delete entry;
        }
    }

#endif
}

PathTileEntryDialog::~PathTileEntryDialog()
{
    qDeleteAll(mEntries);
    delete ui;
}

PGP_TileEntry *PathTileEntryDialog::selectedEntry() const
{
    QModelIndexList selection = ui->tableView->selectionModel()->selectedIndexes();
    if (selection.count()) {
        QModelIndex index = selection.first();
        return static_cast<PGP_TileEntry*>(ui->tableView->model()->userDataAt(index));
    }
    return 0;
}

void PathTileEntryDialog::setTilesList(const QString &category,
                                       PGP_TileEntry *initialEntry = 0)
{
    Tiled::Tile *initialTile = 0;

    mTiles.clear();
    mEntries.clear();
    QList<void*> userData;

    QList<PGP_TileEntry*> entries = getEntriesInCategory(category);

    MixedTilesetView *v = ui->tableView;
    QMap<QString,PGP_TileEntry*> entryMap;
    int i = 0;
    foreach (PGP_TileEntry *entry, entries) {
        QString key = entry->displayTile()->tileName() + QString::number(i++);
        entryMap[key] = entry;
    }
    foreach (PGP_TileEntry *entry, entryMap.values()) {
        PGP_Tile *prop = entry->displayTile();
        Tile *tile = PathGeneratorMgr::instance()->tileFor(prop->mTilesetName, prop->mTileID);
        mTiles += tile ? tile : PathGeneratorMgr::instance()->missingTile();
        userData += entry;
        mEntries += entry;
        if (initialEntry && matches(entry, initialEntry))
            initialTile = mTiles.last();
    }
    v->model()->setTiles(mTiles, userData);

    if (initialTile != 0)
        v->setCurrentIndex(v->model()->index(initialTile));
    else
        v->setCurrentIndex(QModelIndex());
}

QList<PGP_TileEntry *> PathTileEntryDialog::getEntriesInCategory(const QString &category)
{
    QList<PGP_TileEntry*> entries;
    if (category == QLatin1String("walls")) {
        foreach (BuildingTileEntry *bentry, BuildingTilesMgr::instance()->catEWalls()->entries())
            entries += BuildingEntryToPathEntry(bentry);
        foreach (BuildingTileEntry *bentry, BuildingTilesMgr::instance()->catIWalls()->entries()) {
            PGP_TileEntry *entry = BuildingEntryToPathEntry(bentry);
            if (matches(entry, entries))
                delete entry;
            else
                entries += entry;
        }
    }
    if (category == QLatin1String("grime_wall")) {
        foreach (BuildingTileEntry *bentry, BuildingTilesMgr::instance()->catGrimeWall()->entries())
            entries += BuildingEntryToPathEntry(bentry);
    }
    return entries;
}

PGP_TileEntry *PathTileEntryDialog::BuildingEntryToPathEntry(BuildingTileEntry *entry)
{
    PGP_TileEntry *prop = new PGP_TileEntry(QLatin1String("entry"));
    prop->setCategory(mCategory);

    for (int i = 0; i < entry->category()->enumCount(); i++) {
        QString name = entry->category()->enumToString(i);
        if (PathGeneratorProperty *prop2 = prop->property(name))
            prop2->asTile()->valueFromString(entry->tile(i)->name());
        else
            qFatal("unhandled BuildingTileCategory enum in PathTileEntryDialog::BuildingEntryToPathEntry");
    }
    return prop;
}

bool PathTileEntryDialog::matches(PGP_TileEntry *entry, PGP_TileEntry *other)
{
    for (int i = 0; i < entry->properties().size(); i++) {
        if (entry->property(i)->asTile()->tileName() != other->property(i)->asTile()->tileName())
            return false;
    }
    return true;
}

bool PathTileEntryDialog::matches(PGP_TileEntry *entry, QList<PGP_TileEntry *> &entries)
{
    foreach (PGP_TileEntry *entry2, entries) {
        if (matches(entry, entry2))
            return true;
    }
    return false;
}

void PathTileEntryDialog::tilesDialog()
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();
#if 0 // FIXME
    dialog->selectCategory(mCategory);
#endif
    QWidget *saveParent = dialog->parentWidget();
    dialog->reparent(this);
    dialog->exec();
    dialog->reparent(saveParent);

    if (dialog->changes()) {
        setTilesList(mCategory);
    }
}

void PathTileEntryDialog::accept()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("PathTileEntryDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();

    QDialog::accept();
}
