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

#include "tileshapegrouppropertiesdialog.h"
#include "ui_tileshapegrouppropertiesdialog.h"

#include "virtualtileset.h"

using namespace Tiled::Internal;

TileShapeGroupPropertiesDialog::TileShapeGroupPropertiesDialog(TileShapeGroup *g, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileShapeGroupPropertiesDialog)
{
    ui->setupUi(this);

    ui->name->setText(g->label());
    ui->width->setValue(g->columnCount());
    ui->height->setValue(g->rowCount());
}

TileShapeGroupPropertiesDialog::~TileShapeGroupPropertiesDialog()
{
    delete ui;
}

QString TileShapeGroupPropertiesDialog::label() const
{
    return ui->name->text();
}

int TileShapeGroupPropertiesDialog::columnCount() const
{
    return ui->width->value();
}

int TileShapeGroupPropertiesDialog::rowCount() const
{
    return ui->height->value();
}
