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

#include <QPushButton>

AddVirtualTilesetDialog::AddVirtualTilesetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddVirtualTilesetDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Add Virtual Tileset"));

    ui->columnCount->setValue(8);
    ui->rowCount->setValue(8);

    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(updateActions()));
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
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(name().length());
}
