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
    Preferences::instance()->setShowAdjacentMaps(true);

    MapComposite *mc = doc.data()->mapComposite();

    mCurrentIssueFile = 0;
    foreach (IssueFile *file, mFiles) {
        if (file->path == fileName) {
            mCurrentIssueFile = file;
            file->issues.clear();
            break;
        }
    }
    if (mCurrentIssueFile == 0) {
        mCurrentIssueFile = new IssueFile(fileName);
        mFiles += mCurrentIssueFile;
    }

    RearrangeTiles::instance()->readTxtIfNeeded();

    QVector<const Cell*> cells;
    QVector<qreal> opacities;
    int numLevels = mc->layerGroupCount();
    for (int level = numLevels - 1; level >= 0; --level) {
        CompositeLayerGroup *lg = mc->tileLayersForLevel(level);
        int oldGrass = 0;
        for (int y = 0; y < map->height(); y++) {
            for (int x = 0; x < map->width(); x++) {
                cells.clear();
                if (!lg->orderedCellsAt(QPoint(x, y), cells, opacities))
                    continue;
                foreach (const Cell *cell, cells) {
                    if (cell->isEmpty())
                        continue;
                    if (cell->tile->image().isNull()) {
                        issue(Issue::Bogus, tr("invisible tile"), x, y, level);
                    } else if (oldGrass < 10 && cell->tile->tileset()->name() == QLatin1Literal("vegetation_groundcover_01")) {
                        if ((cell->tile->id() < 6) || (cell->tile->id() >= 44 && cell->tile->id() <= 46)) {
                            issue(Issue::Bogus, tr("old grass tile, fix with replace_vegetation_groundcover.lua"), x, y, level);
                            oldGrass++;
                        }
                    } else if (RearrangeTiles::instance()->isRearranged(cell->tile)) {
                        issue(Issue::Bogus, tr("tile marked 'rearranged' needs looking at"), x, y, level);
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
