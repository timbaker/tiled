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

#include "BuildingEditor/buildingpreferences.h"

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
    connect(ui->addTileset, SIGNAL(clicked()), SLOT(addTileset()));
    connect(ui->removeTileset, SIGNAL(clicked()), SLOT(removeTileset()));

    ui->directory->setText(PathGeneratorMgr::instance()->tilesDirectory());
    connect(ui->chooseDirectory, SIGNAL(clicked()), SLOT(chooseTilesDirectory()));

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
    foreach (Tileset *ts, PathGeneratorMgr::instance()->tilesets()) {
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

void PathGeneratorTilesDialog::addTileset()
{
    const QString tilesDir = PathGeneratorMgr::instance()->tilesDirectory();
    const QString filter = Utils::readableImageFormatsFilter();
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Tileset Image"),
                                                          tilesDir,
                                                          filter);
    QFileInfoList add;
    foreach (QString f, fileNames) {
        QFileInfo info(f);
        QString name = info.completeBaseName();
        if (Tileset * ts = PathGeneratorMgr::instance()->tilesetFor(name)) {
            // If the tileset was missing, then we will try to load it again.
            if (!ts->isMissing())
                continue; // FIXME: duplicate tileset names not allowed, even in different directories
        }
        add += info;
    }

    foreach (QFileInfo info, add) {
        if (Tiled::Tileset *ts = PathGeneratorMgr::instance()->loadTileset(info.canonicalFilePath())) {
            PathGeneratorMgr::instance()->addOrReplaceTileset(ts);
            setTilesetList();
            ui->tilesets->setCurrentRow(PathGeneratorMgr::instance()->indexOf(ts));
        } else {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 PathGeneratorMgr::instance()->errorString());
        }
    }
}

void PathGeneratorTilesDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->tilesets->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->tilesets->row(item);
        Tileset *tileset = PathGeneratorMgr::instance()->tilesets().at(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?").arg(tileset->name()),
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        PathGeneratorMgr::instance()->removeTileset(tileset);
        delete item;
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
    if (Tileset *ts = PathGeneratorMgr::instance()->tilesetFor(tilesetName)) {
        int row = PathGeneratorMgr::instance()->tilesets().indexOf(ts);
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
        mCurrentTileset = PathGeneratorMgr::instance()->tilesets().at(row);
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
    accept();
}

void PathGeneratorTilesDialog::chooseTilesDirectory()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Directory"),
                                                  ui->chooseDirectory->text());
    if (!f.isEmpty()) {
        ui->directory->setText(QDir::toNativeSeparators(f));
        PathGeneratorMgr::instance()->setTilesDirectory(f);
    }
}

void PathGeneratorTilesDialog::updateUI()
{
    ui->removeTileset->setEnabled(mCurrentTileset != 0);
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
