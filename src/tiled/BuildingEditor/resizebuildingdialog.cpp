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

#include "resizebuildingdialog.h"
#include "ui_resizebuildingdialog.h"

#include "building.h"

using namespace BuildingEditor;

ResizeBuildingDialog::ResizeBuildingDialog(Building *building, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ResizeBuildingDialog)
{
    ui->setupUi(this);

    ui->oldWidth->setText(QString::number(building->width()));
    ui->oldHeight->setText(QString::number(building->height()));

    ui->newWidth->setValue(building->width());
    ui->newHeight->setValue(building->height());
}

ResizeBuildingDialog::~ResizeBuildingDialog()
{
    delete ui;
}

QSize ResizeBuildingDialog::buildingSize() const
{
    return QSize(ui->newWidth->value(), ui->newHeight->value());
}
