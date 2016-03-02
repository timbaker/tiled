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

#include "luaworlddialog.h"
#include "ui_luaworlddialog.h"

#include "luaconsole.h"
#include "mainwindow.h"
#include "mapdocument.h"
#include "mapmanager.h"
#include "preferences.h"
#include "tilesetmanager.h"
#include "tmxmapreader.h"
#include "tmxmapwriter.h"
#include "zprogress.h"

#include "worlded/world.h"
#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QScopedPointer>
#include <QSettings>
#include <QTemporaryFile>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

LuaWorldDialog::LuaWorldDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LuaWorldDialog)
{
    ui->setupUi(this);

    connect(ui->backupsBrowse, SIGNAL(clicked()), SLOT(backupsBrowse()));
    connect(ui->scriptBrowse, SIGNAL(clicked()), SLOT(scriptBrowse()));

    QSettings settings;

    setList();
    QString f = settings.value(QLatin1String("LuaWorldDialog/SelectedWorld")).toString();
    int row = -1;
    for (int i = 0; i < ui->listPZW->count(); i++) {
        if (ui->listPZW->item(i)->text() == f) {
            row = i;
            break;
        }
    }
    ui->listPZW->setCurrentRow(row);

    f = settings.value(QLatin1String("LuaWorldDialog/BackupDirectory")).toString();
    if (!f.isEmpty() && QFileInfo(f).exists())
        ui->backupsEdit->setText(QDir::toNativeSeparators(f));

    bool checked = settings.value(QLatin1String("LuaWorldDialog/BackupsChecked"),
                                  true).toBool();
    ui->backupsGroupBox->setChecked(checked);

    f = settings.value(QLatin1String("LuaWorldDialog/Script")).toString();
    if (!f.isEmpty() && QFileInfo(f).exists())
        ui->scriptEdit->setText(QDir::toNativeSeparators(f));

    ui->listPZW->setFocus();
}

LuaWorldDialog::~LuaWorldDialog()
{
    delete ui;
}

void LuaWorldDialog::setList()
{
    ui->listPZW->clear();
    for (int i = 0; i < WorldEd::WorldEdMgr::instance()->worldCount(); i++) {
        QString f = WorldEd::WorldEdMgr::instance()->worldFileName(i);
        ui->listPZW->addItem(QDir::toNativeSeparators(f));
    }
}

bool LuaWorldDialog::processMap(const QString &mapFilePath)
{
    TmxMapReader reader;
    Map *map = reader.read(mapFilePath);
    if (!map) {
        QMessageBox::critical(this, tr("Error Loading Map"),
                              reader.errorString());
        return false;
    }

    bool showAdjacentMaps = Preferences::instance()->showAdjacentMaps();
    Preferences::instance()->setShowAdjacentMaps(false);
    QScopedPointer<MapDocument> doc(new MapDocument(map, mapFilePath));
    Preferences::instance()->setShowAdjacentMaps(showAdjacentMaps);

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

void LuaWorldDialog::accept()
{
    int row = ui->listPZW->currentRow();
    if (row == -1) {
        QMessageBox::critical(this, tr("No World Selected"),
                              tr("Please choose a world from the list.  You can add more worlds in the Preferences."));
        return;
    }

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
    QString f = ui->listPZW->item(row)->text();
    settings.setValue(QLatin1String("LuaWorldDialog/SelectedWorld"), f);
    settings.setValue(QLatin1String("LuaWorldDialog/BackupsChecked"),
                      ui->backupsGroupBox->isChecked());
    settings.setValue(QLatin1String("LuaWorldDialog/BackupDirectory"),
                      ui->backupsEdit->text());
    settings.setValue(QLatin1String("LuaWorldDialog/Script"),
                      ui->scriptEdit->text());

    PROGRESS progress(QLatin1String("Running LUA Script"), this);

    World *world = WorldEd::WorldEdMgr::instance()->worldAt(row);
    for (int y = 0; y < world->height(); y++) {
        for (int x = 0; x < world->width(); x++) {
            WorldCell *cell = world->cellAt(x, y);
            QString filePath = cell->mapFilePath();
            if (filePath.isEmpty() ||
                    !QFileInfo(filePath).exists())
                continue;
            progress.update(QString(QLatin1String("Cell %1,%2")).arg(x).arg(y));
            LuaConsole::instance()->write(filePath, Qt::blue);
            if (!processMap(filePath))
                break;
        }
    }

    QDialog::accept();
}

void LuaWorldDialog::backupsBrowse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->backupsEdit->text());
    if (!f.isEmpty()) {
        ui->backupsEdit->setText(QDir::toNativeSeparators(f));
    }
}

void LuaWorldDialog::scriptBrowse()
{
    QString f = QFileDialog::getOpenFileName(this, tr("Choose Lua Script"),
                                             ui->scriptEdit->text(),
                                             tr("Lua files (*.lua)"));
    if (!f.isEmpty()) {
        ui->scriptEdit->setText(QDir::toNativeSeparators(f));
    }
}
