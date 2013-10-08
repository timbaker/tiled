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

#include "texturepropertiesdialog.h"
#include "ui_texturepropertiesdialog.h"

#include "texturemanager.h"

using namespace Tiled::Internal;

TexturePropertiesDialog::TexturePropertiesDialog(TextureInfo *tex, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TexturePropertiesDialog)
{
    ui->setupUi(this);

    ui->name->setText(tex->name());
    ui->tileWidth->setValue(tex->tileWidth());
    ui->tileHeight->setValue(tex->tileHeight());
}

TexturePropertiesDialog::~TexturePropertiesDialog()
{
    delete ui;
}

int TexturePropertiesDialog::tileWidth() const
{
    return ui->tileWidth->value();
}

int TexturePropertiesDialog::tileHeight() const
{
    return ui->tileHeight->value();
}
