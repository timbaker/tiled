/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "removetilesetsdialog.h"
#include "ui_removetilesetsdialog.h"

RemoveTilesetsDialog::RemoveTilesetsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RemoveTilesetsDialog)
{
    ui->setupUi(this);

    connect(ui->checkAll_2, &QAbstractButton::clicked, this, &RemoveTilesetsDialog::checkAll);
    connect(ui->uncheckAll_2, &QAbstractButton::clicked, this, &RemoveTilesetsDialog::uncheckAll);
}

RemoveTilesetsDialog::~RemoveTilesetsDialog()
{
    delete ui;
}

void RemoveTilesetsDialog::setTilesets(const QStringList &tilesets)
{
    ui->files_2->clear();
    ui->files_2->addItems(tilesets);
    for (int i = 0; i < ui->files_2->count(); i++) {
        QListWidgetItem* item = ui->files_2->item(i);
        item->setCheckState(Qt::CheckState::Unchecked);
    }
}

QStringList RemoveTilesetsDialog::tilesetsToRemove()
{
    QStringList result;
    for (int i = 0; i < ui->files_2->count(); i++) {
        const QListWidgetItem* item = ui->files_2->item(i);
        if (item->checkState() == Qt::Checked) {
            result += item->text();
        }
    }
    return result;
}

void RemoveTilesetsDialog::checkAll()
{
    for (int i = 0; i < ui->files_2->count(); i++) {
        ui->files_2->item(i)->setCheckState(Qt::Checked);
    }
}

void RemoveTilesetsDialog::uncheckAll()
{
    for (int i = 0; i < ui->files_2->count(); i++) {
        ui->files_2->item(i)->setCheckState(Qt::Unchecked);
    }
}
