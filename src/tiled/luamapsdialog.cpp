/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "luamapsdialog.h"
#include "ui_luamapsdialog.h"

#include "luaconsole.h"
#include "mainwindow.h"
#include "mapdocument.h"
#include "mapmanager.h"
#include "preferences.h"
#include "tilesetmanager.h"
#include "tmxmapreader.h"
#include "tmxmapwriter.h"
#include "zprogress.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QScopedPointer>
#include <QSettings>
#include <QTemporaryFile>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

LuaMapsDialog::LuaMapsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LuaMapsDialog)
{
    ui->setupUi(this);

    connect(ui->browseButton, SIGNAL(clicked()), SLOT(dirBrowse()));
    connect(ui->selectAll, SIGNAL(clicked()), SLOT(selectAll()));
    connect(ui->selectNone, SIGNAL(clicked()), SLOT(selectNone()));

    connect(ui->backupsBrowse, SIGNAL(clicked()), SLOT(backupsBrowse()));

    connect(ui->scriptBrowse, SIGNAL(clicked()), SLOT(scriptBrowse()));

    Preferences *prefs = Preferences::instance();
    QSettings settings;
    QString f = settings.value(QLatin1String("LuaMapsDialog/Directory"),
                               prefs->mapsDirectory()).toString();
    if (f.isEmpty())
        f = prefs->mapsDirectory();
    if (!f.isEmpty() && QFileInfo(f).exists()) {
        ui->directoryEdit->setText(QDir::toNativeSeparators(f));
        QMetaObject::invokeMethod(this, "setList", Qt::QueuedConnection);
    }

    f = settings.value(QLatin1String("LuaMapsDialog/BackupDirectory")).toString();
    if (!f.isEmpty() && QFileInfo(f).exists())
        ui->backupsEdit->setText(QDir::toNativeSeparators(f));

    bool checked = settings.value(QLatin1String("LuaMapsDialog/BackupsChecked"),
                                  true).toBool();
    ui->backupsGroupBox->setChecked(checked);

    f = settings.value(QLatin1String("LuaMapsDialog/Script")).toString();
    if (!f.isEmpty() && QFileInfo(f).exists())
        ui->scriptEdit->setText(QDir::toNativeSeparators(f));
}

LuaMapsDialog::~LuaMapsDialog()
{
    delete ui;
}

void LuaMapsDialog::setList()
{
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

        QTreeWidgetItem *item = new QTreeWidgetItem();
        item->setText(0, fileName);
        item->setCheckState(0, Qt::Checked);
        ui->mapsList->addTopLevelItem(item);
    }
}

bool LuaMapsDialog::processMap(const QString &mapFilePath)
{
    TmxMapReader reader;
    Map *map = reader.read(mapFilePath);
    if (!map) {
        QMessageBox::critical(this, tr("Error Loading Map"),
                              reader.errorString());
        return false;
    }

    QScopedPointer<MapDocument> doc(new MapDocument(map, mapFilePath));
    if (!MainWindow::instance()->LuaScript(doc.data(), ui->scriptEdit->text())) {
        QMessageBox::critical(this, tr("LUA Error"),
                              tr("The LUA script returned an error.\nCheck the console."));
        return false;
    }

    // MainWindow::LuaScript() creates a macro containing zero or more undo commands.
    // If the LUA script had no effect, don't write the map again.
    if (doc.data()->undoStack()->isClean() ||
            (doc.data()->undoStack()->count() == 1 &&
            doc.data()->undoStack()->command(0)->childCount() == 0)) {
        LuaConsole::instance()->write(mapFilePath + tr(" is unchanged."), Qt::blue);
        return true;
    }

    QFileInfo info(mapFilePath);

    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        QMessageBox::critical(this, tr("Error Writing Map"),
                              tempFile.errorString());
        return false;
    }
    TmxMapWriter w;
    if (!w.write(map, tempFile, info.absolutePath())) {
        QMessageBox::critical(this, tr("Error Writing Map"),
                              w.errorString());
        return false;
    }

    // foo.tmx -> backupDir/foo.tmx(.bak)
    QFile backup(mapFilePath);
    QDir backupDir(info.absoluteDir());
    if (ui->backupsGroupBox->isChecked())
        backupDir.setPath(ui->backupsEdit->text());
    QString backupPath = backupDir.filePath(info.fileName());
    if (backupDir == info.absoluteDir())
        backupPath += QLatin1String(".bak");
    QFile::remove(backupPath);
    if (!backup.rename(backupPath)) {
        QString msg = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2"))
                .arg(info.fileName())
                .arg(QFileInfo(backupPath).fileName());
        QMessageBox::critical(this, tr("Error Writing Map"), msg);
        return false;
    }

    // /tmp/tempXYZ -> foo.tmx
    if (!tempFile.rename(mapFilePath)) {
        backup.rename(mapFilePath);
        QString msg = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2"))
                .arg(QFileInfo(tempFile).fileName())
                .arg(info.fileName());
        QMessageBox::critical(this, tr("Error Writing Map"), msg);
        return false;
    }

    // If anything above failed, the temp file should auto-remove, but not after
    // a successful save.
    tempFile.setAutoRemove(false);

    return true;
}

void LuaMapsDialog::dirBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->directoryEdit->text());
    if (!f.isEmpty()) {
        ui->directoryEdit->setText(f);
        setList();
    }
}

void LuaMapsDialog::accept()
{
    if (ui->backupsGroupBox->isChecked() && (ui->backupsEdit->text().isEmpty() ||
                                             !QFileInfo(ui->backupsEdit->text()).exists())) {
        QMessageBox::critical(this, tr("Backup Directory Invalid"),
                              tr("Please choose a directory to write backups files into.\nOr uncheck the backup files checkbox."));
        return;
    }
    if (ui->scriptEdit->text().isEmpty() || !QFileInfo(ui->scriptEdit->text()).exists()) {
        QMessageBox::critical(this, tr("LUA Script Invalid"),
                              tr("Please choose a LUA script to be run."));
        return;
    }

    QSettings settings;
    settings.setValue(QLatin1String("LuaMapsDialog/Directory"),
                      ui->directoryEdit->text());
    settings.setValue(QLatin1String("LuaMapsDialog/BackupsChecked"),
                      ui->backupsGroupBox->isChecked());
    settings.setValue(QLatin1String("LuaMapsDialog/BackupDirectory"),
                      ui->backupsEdit->text());
    settings.setValue(QLatin1String("LuaMapsDialog/Script"),
                      ui->scriptEdit->text());

    QDir dir(ui->directoryEdit->text());

    PROGRESS progress(QLatin1String("Running LUA Script"), this);

    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++) {
        QTreeWidgetItem *item = view->topLevelItem(i);
        if (item->checkState(0) != Qt::Checked)
            continue;

        progress.update(QString(QLatin1String("%1")).arg(item->text(0)));

        QString filePath = dir.filePath(item->text(0));
        LuaConsole::instance()->write(filePath, Qt::blue);
        if (!processMap(filePath))
            break;
    }

    QDialog::accept();
}

void LuaMapsDialog::selectAll()
{
    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void LuaMapsDialog::selectNone()
{
    QTreeWidget *view = ui->mapsList;
    for (int i = 0; i < view->topLevelItemCount(); i++)
        view->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}

void LuaMapsDialog::backupsBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->backupsEdit->text());
    if (!f.isEmpty()) {
        ui->backupsEdit->setText(QDir::toNativeSeparators(f));
    }
}

void LuaMapsDialog::scriptBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose Lua Script"),
                                             ui->scriptEdit->text(),
                                             tr("Lua files (*.lua)"));
    if (!f.isEmpty()) {
        ui->scriptEdit->setText(QDir::toNativeSeparators(f));
    }
}
