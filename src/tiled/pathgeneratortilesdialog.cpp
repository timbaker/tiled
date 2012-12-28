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

#include "pathgeneratortilesdialog.h"
#include "ui_pathgeneratortilesdialog.h"

#include "pathgeneratormgr.h"
#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"

#include "utils.h"
#include "zoomable.h"

#include "tileset.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled;
using namespace Tiled::Internal;

PathGeneratorTilesDialog *PathGeneratorTilesDialog::mInstance = 0;

PathGeneratorTilesDialog *PathGeneratorTilesDialog::instance()
{
    if (!mInstance)
        mInstance = new PathGeneratorTilesDialog;
    return mInstance;
}

void PathGeneratorTilesDialog::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

static void restoreSplitterSizes(QSettings &settings, QSplitter *splitter)
{
    QVariant v = settings.value(QString(QLatin1String("%1/sizes")).arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
}

PathGeneratorTilesDialog::PathGeneratorTilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PathGeneratorTilesDialog),
    mZoomable(new Zoomable(this)),
    mCurrentTileset(0),
    mCurrentTile(0)
{
    ui->setupUi(this);

    mZoomable->setScale(0.5); // FIXME
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    ui->tiles->model()->setShowHeaders(false);

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles, SIGNAL(activated(QModelIndex)),
            SLOT(tileActivated(QModelIndex)));
    connect(ui->tilesetMgr, SIGNAL(clicked()), SLOT(manageTilesets()));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));

    setTilesetList();

    QSettings settings;
    settings.beginGroup(QLatin1String("PathGeneratorTilesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    restoreSplitterSizes(settings, ui->splitter);
    settings.endGroup();

}

PathGeneratorTilesDialog::~PathGeneratorTilesDialog()
{
    delete ui;
}

void PathGeneratorTilesDialog::setTilesetList()
{
    ui->tilesets->clear();
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name());
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
    }
}

void PathGeneratorTilesDialog::setTilesList()
{
    if (mCurrentTileset) {
        ui->tiles->model()->setTileset(mCurrentTileset);
    } else {
        ui->tiles->model()->setTiles(QList<Tile*>());
    }
}

void PathGeneratorTilesDialog::reparent(QWidget *parent)
{
    QPoint savePosition = pos();
    setParent(parent, windowFlags());
    move(savePosition);
}

void PathGeneratorTilesDialog::setInitialTile(const QString &tilesetName, int tileID)
{
    if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
        int row = TileMetaInfoMgr::instance()->indexOf(ts);
        ui->tilesets->setCurrentRow(row);
        if (Tile *tile = ts->tileAt(tileID))
            ui->tiles->setCurrentIndex(ui->tiles->model()->index(tile));
    } else {
        ui->tiles->setCurrentIndex(QModelIndex());
    }
}

Tile *PathGeneratorTilesDialog::selectedTile()
{
    QModelIndex current = ui->tiles->currentIndex();
    if (Tile *tile = ui->tiles->model()->tileAt(current))
        return tile;
    return 0;
}

void PathGeneratorTilesDialog::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0) {
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    }
    setTilesList();
    updateUI();
}

void PathGeneratorTilesDialog::currentTileChanged(const QModelIndex &index)
{
    mCurrentTile = ui->tiles->model()->tileAt(index);
}

void PathGeneratorTilesDialog::tileActivated(const QModelIndex &index)
{
    Q_UNUSED(index)
    accept();
}

void PathGeneratorTilesDialog::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();
}

void PathGeneratorTilesDialog::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesets->setCurrentRow(row);
    updateUI();
}

void PathGeneratorTilesDialog::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesets->takeItem(row);
    updateUI();
}

void PathGeneratorTilesDialog::updateUI()
{
}

static void saveSplitterSizes(QSettings &settings, QSplitter *splitter)
{
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    settings.setValue(QString(QLatin1String("%1/sizes")).arg(splitter->objectName()), v);
}

void PathGeneratorTilesDialog::accept()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("PathGeneratorTilesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
//    settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
    saveSplitterSizes(settings, ui->splitter);
    settings.endGroup();

    QDialog::accept();
}
