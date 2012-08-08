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

#include "convertorientationdialog.h"
#include "ui_convertorientationdialog.h"

#include "mapmanager.h"
#include "preferences.h"
#include "tilesetmanager.h"
#include "tmxmapwriter.h"
#include "zprogress.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QScopedPointer>

using namespace Tiled;
using namespace Internal;

ConvertOrientationDialog::ConvertOrientationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConvertOrientationDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, SIGNAL(clicked()), SLOT(browse()));
    connect(ui->selectAll, SIGNAL(clicked()), SLOT(selectAll()));
    connect(ui->selectNone, SIGNAL(clicked()), SLOT(selectNone()));

    Preferences *prefs = Preferences::instance();
    QString mapsDir = prefs->mapsDirectory();
    if (!mapsDir.isEmpty()) {
        ui->directoryEdit->setText(mapsDir);
        QMetaObject::invokeMethod(this, "setList", Qt::QueuedConnection);
    }
}

ConvertOrientationDialog::~ConvertOrientationDialog()
{
    delete ui;
}

void ConvertOrientationDialog::setList()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    ui->mapsList->clear();

    QDir dir(ui->directoryEdit->text());

    QStringList filters;
    filters << QLatin1String("*.tmx");
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::Readable | QDir::Writable);

    foreach (QString fileName, dir.entryList()) {
        QString filePath = dir.filePath(fileName);
        MapInfo *mapInfo = MapManager::instance()->mapInfo(filePath);
        if (!mapInfo)
            continue;
        if (mapInfo->orientation() != Map::Isometric)
            continue;

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, fileName);
        item->setCheckState(0, Qt::Checked);
        ui->mapsList->addTopLevelItem(item);
    }

    QApplication::restoreOverrideCursor();
}

struct ScopedPointerCustomDeleter
{
    static inline void cleanup(Map *map)
    {
        TilesetManager *tilesetMgr = TilesetManager::instance();
        tilesetMgr->removeReferences(map->tilesets());
        delete map;
    }
};

bool ConvertOrientationDialog::convertMap(const QString &mapFilePath)
{
    MapManager *mapMgr = MapManager::instance();
    MapInfo *mapInfo = mapMgr->loadMap(mapFilePath);

    if (!mapInfo) {
        QMessageBox::critical(this, tr("Error Loading Map"),
                              mapMgr->errorString());
        return false;
    }

    QScopedPointer<Tiled::Map,ScopedPointerCustomDeleter>
            map(mapMgr->convertOrientation(mapInfo->map(),
                                           Map::LevelIsometric));

    // Restore the message since loadMap() will change it.
    QString msg = QString(QLatin1String("Writing %1"))
            .arg(QFileInfo(mapFilePath).fileName());
    ZProgressManager::instance()->update(msg);

    QFileInfo info(mapInfo->path());
    QString convertedPath = info.absolutePath() + QLatin1Char('/') +
            info.completeBaseName() + QLatin1String(".converted.tmx");

    TmxMapWriter w;
    if (!w.write(map.data(), convertedPath)) {
        QMessageBox::critical(this, tr("Error Writing Map"),
                              w.errorString());
        return false;
    }

    // foo.tmx -> foo.original.tmx
    QString backupPath = info.absolutePath() + QLatin1Char('/') +
            info.completeBaseName() + QLatin1String(".original.tmx");
    QFile::remove(backupPath);
    QFile backup(mapFilePath);
    if (!backup.rename(backupPath)) {
        QFile::remove(convertedPath);
        QString msg = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2"))
                .arg(info.fileName())
                .arg(QFileInfo(backupPath).fileName());
        QMessageBox::critical(this, tr("Error Writing Map"), msg);
        return false;
    }

    // foo.converted.tmx -> foo.tmx
    QFile converted(convertedPath);
    if (!converted.rename(mapFilePath)) {
        QFile::remove(convertedPath);
        backup.rename(mapFilePath);
        QString msg = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2"))
                .arg(QFileInfo(convertedPath).fileName())
                .arg(info.fileName());
        QMessageBox::critical(this, tr("Error Writing Map"), msg);
        return false;
    }

    return true;
}

void ConvertOrientationDialog::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->directoryEdit->text());
    if (!f.isEmpty()) {
        ui->directoryEdit->setText(f);
        setList();
    }
}

void ConvertOrientationDialog::accept()
{
    QDir dir(ui->directoryEdit->text());

    PROGRESS progress(QLatin1String("Converting maps"));

    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = view->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked)
            continue;

        progress.update(QString(QLatin1String("Converting %1")).arg(item->text(0)));

        QString filePath = dir.filePath(item->text(0));
        if (!convertMap(filePath))
            break;
    }

    QDialog::accept();
}

void ConvertOrientationDialog::selectAll()
{
    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void ConvertOrientationDialog::selectNone()
{
    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}
