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

#include "buildingpreferences.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

using namespace BuildingEditor;

BuildingPreferencesDialog::BuildingPreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingPreferencesDialog)
{
    ui->setupUi(this);

    QString configPath = BuildingPreferences::instance()->configPath();
    ui->configDirEdit->setText(QDir::toNativeSeparators(configPath));
}

BuildingPreferencesDialog::~BuildingPreferencesDialog()
{
    delete ui;
}

void BuildingPreferencesDialog::accept()
{
    QDialog::accept();
}
