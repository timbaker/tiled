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

#include "buildingpreferencesdialog.h"
#include "ui_buildingpreferencesdialog.h"

#include "buildingpreferences.h"

#include "preferences.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QTableView>

using namespace BuildingEditor;
using namespace Tiled::Internal;

BuildingPreferencesDialog::BuildingPreferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingPreferencesDialog)
{
    ui->setupUi(this);

    QString configPath = prefs()->configPath();
    ui->configDirEdit->setText(QDir::toNativeSeparators(configPath));

    ui->gridColor->setColor(BuildingPreferences::instance()->gridColor());

    ui->tilesetColorButton->setColor(Preferences::instance()->tilesetBackgroundColor());
    connect(ui->tilesetDefaultButton, &QAbstractButton::clicked, this, &BuildingPreferencesDialog::setDefaultTilesetBackground);

    mUseOpenGL = prefs()->useOpenGL();
    ui->useOpenGL->setChecked(mUseOpenGL);
    connect(ui->useOpenGL, &QAbstractButton::toggled, this, &BuildingPreferencesDialog::setUseOpenGL);

    ui->isometric->setChecked(!prefs()->levelIsometric());
    ui->levelIsometric->setChecked(prefs()->levelIsometric());

    ui->themeCombo->setCurrentText(Preferences::instance()->theme());
    connect(ui->themeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &BuildingPreferencesDialog::themeChanged);
}

BuildingPreferencesDialog::~BuildingPreferencesDialog()
{
    delete ui;
}

BuildingPreferences *BuildingPreferencesDialog::prefs() const
{
    return BuildingPreferences::instance();
}

void BuildingPreferencesDialog::setUseOpenGL(bool useOpenGL)
{
    mUseOpenGL = useOpenGL;
}

void BuildingPreferencesDialog::themeChanged(int index)
{
    Q_UNUSED(index)
    QString text = ui->themeCombo->currentText();
    Preferences::instance()->setTheme(text);
}

void BuildingPreferencesDialog::setDefaultTilesetBackground()
{
    const QPalette& palette = ui->listView->palette();
    const QColor tableBgColor = palette.color(QPalette::Active, QPalette::Base);
    ui->tilesetColorButton->setColor(tableBgColor);
}

void BuildingPreferencesDialog::accept()
{
    prefs()->setGridColor(ui->gridColor->color());
    Preferences::instance()->setTilesetBackgroundColor(ui->tilesetColorButton->color());
    prefs()->setUseOpenGL(mUseOpenGL);
    prefs()->setLevelIsometric(ui->levelIsometric->isChecked());
    QDialog::accept();
}
