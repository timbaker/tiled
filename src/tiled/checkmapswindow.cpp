/*
 * Copyright 2016, Tim Baker <treectrl@users.sf.net>
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

#include "checkmapswindow.h"
#include "ui_checkmapswindow.h"

#include "bmpblender.h"
#include "documentmanager.h"
#include "filesystemwatcher.h"
#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "maprenderer.h"
#include "preferences.h"
#include "rearrangetiles.h"
#include "tilemetainfomgr.h"
#include "tmxmapreader.h"
#include "zprogress.h"

#include "BuildingEditor/buildingtiles.h"

#include "tile.h"
#include "tileset.h"

#include <QFileDialog>
#include <QMessageBox>

using namespace Tiled;
using namespace Tiled::Internal;

CheckMapsWindow::CheckMapsWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CheckMapsWindow),
    mFileSystemWatcher(new FileSystemWatcher(this))
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    connect(ui->dirBrowse, SIGNAL(clicked()), SLOT(browse()));
    connect(ui->checkNow, SIGNAL(clicked()), SLOT(check()));
    connect(ui->checkCurrent, SIGNAL(clicked()), SLOT(checkCurrent()));
    connect(ui->treeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), SLOT(itemActivated(QTreeWidgetItem*,int)));

    ui->dirEdit->setText(Preferences::instance()->mapsDirectory());

    ui->treeWidget->setColumnCount(1);

    connect(mFileSystemWatcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, SIGNAL(timeout()), SLOT(fileChangedTimeout()));
}

CheckMapsWindow::~CheckMapsWindow()
{
    qDeleteAll(mFiles);
    delete ui;
}

void CheckMapsWindow::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->dirEdit->text());
    if (!f.isEmpty()) {
        ui->dirEdit->setText(QDir::toNativeSeparators(f));
    }
}

void CheckMapsWindow::check()
{
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        if (ts->isMissing()) {
            PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
            TileMetaInfoMgr::instance()->loadTilesets();
            break;
        }
    }

    QDir dir(ui->dirEdit->text());

    PROGRESS progress(tr("Checking"), this);
    ui->treeWidget->clear();
    qDeleteAll(mFiles);
    mFiles.clear();

    foreach (QString path, mWatchedFiles)
        mFileSystemWatcher->removePath(path);
    mWatchedFiles.clear();

    QStringList filters;
    filters << QLatin1String("*.tmx");
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::Readable | QDir::Writable);

    foreach (QString fileName, dir.entryList()) {
        progress.update(tr("Checking %1").arg(fileName));
        QString filePath = dir.filePath(fileName);
        check(filePath);
        mFileSystemWatcher->addPath(filePath);
        mWatchedFiles += filePath;
    }
}

#include "documentmanager.h"

void CheckMapsWindow::checkCurrent()
{
    MapDocument *doc = DocumentManager::instance()->currentDocument();
    if (!doc)
        return;
    check(doc->fileName());
}

void CheckMapsWindow::itemActivated(QTreeWidgetItem *item, int column)
{
    if (item->parent() == 0)
        return;
    Issue &issue = mFiles[ui->treeWidget->indexOfTopLevelItem(item->parent())]->issues[item->parent()->indexOfChild(item)];
    MainWindow::instance()->openFile(issue.file->path);
    int docIndex = DocumentManager::instance()->findDocument(issue.file->path);
    MapDocument *doc = DocumentManager::instance()->documents().at(docIndex);
    MapView *mapView = DocumentManager::instance()->documentView(doc);
    mapView->centerOn(doc->renderer()->tileToPixelCoords(issue.x, issue.y, issue.z));
}

void CheckMapsWindow::fileChanged(const QString &path)
{
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void CheckMapsWindow::fileChangedTimeout()
{
    foreach (const QString &path, mChangedFiles) {
//        qDebug() << "CHANGED " << path;
        mFileSystemWatcher->removePath(path);
        mWatchedFiles.removeOne(path);
        QFileInfo info(path);
        if (info.exists()) {
            mFileSystemWatcher->addPath(path);
            mWatchedFiles += path;
            foreach (IssueFile *file, mFiles) {
                if (file->path == path) {
                    check(path);
                    updateList(file);
                    syncList(file);
                    break;
                }
            }
        }
    }

    mChangedFiles.clear();
}

void CheckMapsWindow::check(const QString &fileName)
{
    TmxMapReader reader;
    Map *map = reader.read(fileName);
    if (!map) {
        QMessageBox::critical(this, tr("Error Loading Map"), reader.errorString());
        return;
    }

    bool showAdjacentMaps = Preferences::instance()->showAdjacentMaps();
    Preferences::instance()->setShowAdjacentMaps(false);
    QScopedPointer<MapDocument> doc(new MapDocument(map, fileName));
    Preferences::instance()->setShowAdjacentMaps(showAdjacentMaps);

    check(doc.data());
}

void CheckMapsWindow::check(MapDocument *doc)
{
    MapComposite *mc = doc->mapComposite();
    Map *map = mc->map();

    if (mc->bmpBlender())
        mc->bmpBlender()->flush(QRect(0, 0, map->width() - 1, map->height() - 1));

    mCurrentIssueFile = 0;
    foreach (IssueFile *file, mFiles) {
        if (file->path == doc->fileName()) {
            mCurrentIssueFile = file;
            file->issues.clear();
            break;
        }
    }
    if (mCurrentIssueFile == 0) {
        mCurrentIssueFile = new IssueFile(doc->fileName());
        mFiles += mCurrentIssueFile;
    }

    RearrangeTiles::instance()->readTxtIfNeeded();

    for (int level = 0; level < mc->layerGroupCount(); level++) {
        CompositeLayerGroup *lg = mc->tileLayersForLevel(level);
        lg->prepareDrawing2();
    }

    QVector<const Cell*> cells;
    int numLevels = mc->layerGroupCount();
    for (int level = numLevels - 1; level >= 0; --level) {
        CompositeLayerGroup *lg = mc->tileLayersForLevel(level);
        int oldGrass = 0;
        for (int y = 0; y < map->height(); y++) {
            for (int x = 0; x < map->width(); x++) {
                cells.clear();
                if (!lg->orderedCellsAt2(QPoint(x, y), cells))
                    continue;
                foreach (const Cell *cell, cells) {
                    if (cell->isEmpty())
                        continue;
                    if (cell->tile->image().isNull()) {
                        issue(Issue::Bogus, tr("invisible tile"), x, y, level);
                    } else if (cell->tile->tileset()->name() == QLatin1Literal("vegetation_groundcover_01")) {
                        if ((cell->tile->id() < 6) || (cell->tile->id() >= 44 && cell->tile->id() <= 46)) {
                            if (oldGrass < 10) {
                                issue(Issue::Bogus, tr("old grass tile, fix with replace_vegetation_groundcover.lua"), x, y, level);
                                oldGrass++;
                            }
                        } else if (cell->tile->id() >= 18 && cell->tile->id() <= 23) {
                            // Don't allow plant and tree on the same square (only one erosion object per square is supported)
                            foreach (const Cell *cell2, cells) {
                                if (cell2 != cell && !cell2->isEmpty() && cell2->tile->tileset()->name().startsWith(QLatin1Literal("vegetation_trees_01"))) {
                                    issue(Issue::Bogus, tr("tree and plant on the same square, fix with fix_tree_and_plant.lua"), x, y, level);
                                    break;
                                }
                            }

                            // Only one plant on a square
                            foreach (const Cell *cell2, cells) {
                                if (cell2 != cell && !cell2->isEmpty() && (cell2->tile->tileset()->name() == QLatin1Literal("vegetation_groundcover_01")) &&
                                        (cell2->tile->id() >= 18 && cell2->tile->id() <= 23)) {
                                    issue(Issue::Bogus, tr("two plants on the same square"), x, y, level);
                                    break;
                                }
                                if (cell2 != cell && !cell2->isEmpty() && (cell2->tile->tileset()->name() == QLatin1Literal("vegetation_foliage_01"))) {
                                    issue(Issue::Bogus, tr("two plants on the same square"), x, y, level);
                                    break;
                                }
                            }

                            // Plant must be on blends_natural or else Erosion ignores it
                            bool foundBlendsNatural = false;
                            for (int i = 0; i < cells.size(); i++) {
                                if (!cells[i]->isEmpty() && cells[i]->tile->tileset()->name().startsWith(QLatin1Literal("blends_natural"))) {
                                    int id = cells[i]->tile->id();
                                    if ((((id % 8) == 0) || ((id % 8) >= 5)) && (id / 8 % 2 == 0)) { // solid tile, not blend edge
                                        foundBlendsNatural = true;
                                        break;
                                    }
                                }
                            }
                            /*
                            if (!foundBlendsNatural && mc->bmpBlender()) {
                                QStringList blendLayerNames = mc->bmpBlender()->blendLayers();
                                    foreach (TileLayer *blendLayer, lg->bmpBlendLayers()) {
                                        if (blendLayer && blendLayer->contains(x, y)) {
                                            if (!blendLayerNames.contains(blendLayer->name()) || !map->noBlend(blendLayer->name())->get(x, y)) {
                                                if (Tile *blendTile = blendLayer->cellAt(x, y).tile) {
                                                    if (blendTile->tileset()->name().startsWith(QLatin1Literal("blends_natural"))) {
                                                        foundBlendsNatural = true;
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                        if (foundBlendsNatural)
                                            break;
                                    }
                            }
                            */
                            if (!foundBlendsNatural) {
                                issue(Issue::Bogus, tr("vegetation_groundcover plant must be on blends_natural"), x, y, level);
                            }
                        } else {
                            // vegetation_groundcover_01_16,17
//                            issue(Issue::Bogus, tr("no 2x equivalent for vegetation_groundcover"), x, y, level);
                        }
                    } else if (cell->tile->tileset()->name() == QLatin1Literal("vegetation_foliage_01")) {

                        // Bush on plant not allowed
                        foreach (const Cell *cell2, cells) {
                            if (cell2 != cell && !cell2->isEmpty() && (cell2->tile->tileset()->name() == QLatin1Literal("vegetation_groundcover_01")) &&
                                    (cell2->tile->id() >= 16 && cell2->tile->id() <= 17)) {
                                issue(Issue::Bogus, tr("bush and plant on the same square"), x, y, level);
                                break;
                            }
                        }


                        bool foundBlendsNatural = false;
                        for (int i = 0; i < cells.size(); i++) {
                            if (!cells[i]->isEmpty() && cells[i]->tile->tileset()->name().startsWith(QLatin1Literal("blends_natural"))) {
                                int id = cells[i]->tile->id();
                                if ((((id % 8) == 0) || ((id % 8) >= 5)) && (id / 8 % 2 == 0)) { // solid tile, not blend edge
                                    foundBlendsNatural = true;
                                    break;
                                }
                            }
                        }
                        /*
                        if (!foundBlendsNatural && mc->bmpBlender()) {
                            QStringList blendLayerNames = mc->bmpBlender()->blendLayers();
                                foreach (TileLayer *blendLayer, lg->bmpBlendLayers()) {
                                    if (blendLayer && blendLayer->contains(x, y)) {
                                        if (!blendLayerNames.contains(blendLayer->name()) || !map->noBlend(blendLayer->name())->get(x, y)) {
                                            if (Tile *blendTile = blendLayer->cellAt(x, y).tile) {
                                                if (blendTile->tileset()->name().startsWith(QLatin1Literal("blends_natural"))) {
                                                    foundBlendsNatural = true;
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                    if (foundBlendsNatural)
                                        break;
                                }
                        }
                        */
                        if (!foundBlendsNatural) {
                            issue(Issue::Bogus, tr("vegetation_foliage must be on blends_natural"), x, y, level);
                        }
                    } else if (RearrangeTiles::instance()->isRearranged(cell->tile)) {
                        issue(Issue::Bogus, tr("Rearranged tile (%1)").arg(BuildingEditor::BuildingTilesMgr::instance()->nameForTile(cell->tile)), x, y, level);
                    }
                }
            }
        }
    }

    updateList(mCurrentIssueFile);
    syncList(mCurrentIssueFile);
}

void CheckMapsWindow::issue(Issue::Type type, const QString &detail, int x, int y, int z)
{
    mCurrentIssueFile->issues += Issue(mCurrentIssueFile, type, detail, x, y, z);
}

void CheckMapsWindow::updateList(CheckMapsWindow::IssueFile *file)
{
    QTreeWidgetItem *fileItem = ui->treeWidget->topLevelItem(mFiles.indexOf(file));
    if (fileItem == 0) {
        fileItem = new QTreeWidgetItem(QStringList() << QFileInfo(file->path).fileName());
        ui->treeWidget->addTopLevelItem(fileItem);
        fileItem->setExpanded(true);
    }
    while (fileItem->childCount() > 0)
        delete fileItem->takeChild(0);
    for (int j = 0; j < file->issues.size(); j++) {
        QTreeWidgetItem *issueItem = new QTreeWidgetItem(QStringList() << file->issues[j].toString());
        fileItem->addChild(issueItem);
    }
}

void CheckMapsWindow::syncList(CheckMapsWindow::IssueFile *file)
{
    int rowMin = 0, rowMax = mFiles.size() - 1;
    if (file != 0)
        rowMin = rowMax = mFiles.indexOf(file);
    for (int row = rowMin; row <= rowMax; row++) {
        QTreeWidgetItem *fileItem = ui->treeWidget->topLevelItem(row);
        bool anyVisible = false;
        for (int i = 0; i < fileItem->childCount(); i++) {
            Issue &issue = mFiles[row]->issues[i];
            bool visible = true;
            QTreeWidgetItem *issueItem = fileItem->child(i);
            issueItem->setHidden(!visible);
            if (visible)
                anyVisible = true;
        }
        fileItem->setHidden(!anyVisible);
    }
}
