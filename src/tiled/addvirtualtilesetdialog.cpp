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

#include "addvirtualtilesetdialog.h"
#include "ui_addvirtualtilesetdialog.h"

#include "preferences.h"

#include <QDir>
#include <QImageReader>
#include <QPushButton>

AddVirtualTilesetDialog::AddVirtualTilesetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddVirtualTilesetDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Add Virtual Tileset"));

    ui->columnCount->setValue(8);
    ui->rowCount->setValue(8);
    ui->nameEdit->setFocus();

    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(updateActions()));

    updateActions();
}

AddVirtualTilesetDialog::AddVirtualTilesetDialog(const QString &name, int columnCount,
                                                 int rowCount, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddVirtualTilesetDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Edit Virtual Tileset"));

    ui->columnCount->setValue(columnCount);
    ui->rowCount->setValue(rowCount);
    ui->nameEdit->setText(name);
    ui->nameEdit->setFocus();

    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(updateActions()));

    updateActions();
}

AddVirtualTilesetDialog::~AddVirtualTilesetDialog()
{
    delete ui;
}

QString AddVirtualTilesetDialog::name() const
{
    return ui->nameEdit->text();
}

int AddVirtualTilesetDialog::columnCount() const
{
    return ui->columnCount->value();
}

int AddVirtualTilesetDialog::rowCount() const
{
    return ui->rowCount->value();
}

void AddVirtualTilesetDialog::updateActions()
{
    int diskColCount = -1, diskRowCount = -1;
    QString tilesDir = Tiled::Internal::Preferences::instance()->tilesDirectory();
    if (!tilesDir.isEmpty() && !name().isEmpty()) {
        QString fileName = QDir(tilesDir).filePath(name() + QLatin1String(".png"));
        QSize size = QImageReader(fileName).size();
        if (size.isValid()) {
            diskColCount = size.width() / 64;
            diskRowCount = size.height() / 128;
        }
    }
    if (diskColCount != -1) {
        ui->diskColumnCount->setText(tr("Disk image: <b>%1</b>").arg(diskColCount));
        ui->diskRowCount->setText(tr("Disk image: <b>%1</b>").arg(diskRowCount));
    } else {
        ui->diskColumnCount->clear();
        ui->diskRowCount->clear();
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(name().length());
}
