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

#include "vtsprefsdialog.h"
#include "ui_vtsprefsdialog.h"

#include "preferences.h"

#include <QFileDialog>

using namespace Tiled::Internal;

VirtualTilesetPrefsDialog::VirtualTilesetPrefsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VirtualTilesetPrefsDialog)
{
    ui->setupUi(this);

    connect(ui->gameDirBrowse, SIGNAL(clicked()), SLOT(browse()));

    ui->gameDirEdit->setText(Preferences::instance()->alternateVTSDir());
}

VirtualTilesetPrefsDialog::~VirtualTilesetPrefsDialog()
{
    delete ui;
}

QString VirtualTilesetPrefsDialog::gameDir() const
{
    return ui->gameDirEdit->text();
}

void VirtualTilesetPrefsDialog::browse()
{
    QString initial = ui->gameDirEdit->text();
    if (initial.isEmpty()) {
        initial = Preferences::instance()->tilesDirectory();
    }

    QString f = QFileDialog::getExistingDirectory(this, tr("Choose Directory"), initial);
    if (!f.isEmpty())
        ui->gameDirEdit->setText(QDir::toNativeSeparators(f));
}
