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

#include "buildingpreferencesdialog.h"
#include "ui_buildingpreferencesdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

static const char *KEY_TILES_DIR = "BuildingEditor/TilesDirectory";

BuildingPreferencesDialog::BuildingPreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingPreferencesDialog)
{
    ui->setupUi(this);

    connect(ui->browseTiles, SIGNAL(clicked()), SLOT(browseTiles()));

    QString tilesDir = mSettings.value(QLatin1String(KEY_TILES_DIR)).toString();
    if (QDir(tilesDir).exists())
        ui->editTiles->setText(mSettings.value(QLatin1String(KEY_TILES_DIR)).toString());
}

BuildingPreferencesDialog::~BuildingPreferencesDialog()
{
    delete ui;
}

void BuildingPreferencesDialog::browseTiles()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Directory"),
                                                  ui->editTiles->text());
    if (!f.isEmpty())
        ui->editTiles->setText(f);
}

void BuildingPreferencesDialog::accept()
{
    if (ui->editTiles->text().isEmpty()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             tr("Please choose a valid Tiles directory."));
        return;
    }

    QSettings settings;
    settings.setValue(QLatin1String(KEY_TILES_DIR), ui->editTiles->text());

    QDialog::accept();
}
