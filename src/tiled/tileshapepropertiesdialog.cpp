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

#include "tileshapepropertiesdialog.h"
#include "ui_tileshapepropertiesdialog.h"

#include "virtualtileset.h"

#include <QPushButton>


using namespace Tiled::Internal;

TileShapePropertiesDialog::TileShapePropertiesDialog(TileShape *shape, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileShapePropertiesDialog),
    mShape(shape)
{
    ui->setupUi(this);

    ui->nameEdit->setText(mShape->name());
    ui->prompt->clear();

    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));
}

TileShapePropertiesDialog::~TileShapePropertiesDialog()
{
    delete ui;
}

QString TileShapePropertiesDialog::name() const
{
    return ui->nameEdit->text();
}

void TileShapePropertiesDialog::nameEdited(const QString &text)
{
    ui->prompt->clear();
    bool ok = true;
    if (text.isEmpty()) {
        ui->prompt->setText(tr("Empty name isn't allowed."));
        ok = false;
    }
    if (TileShape *shape = VirtualTilesetMgr::instance().tileShape(text))
        if (shape != mShape) {
            ui->prompt->setText(tr("That name is already used."));
            ok = false;
        }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(ok);
}
