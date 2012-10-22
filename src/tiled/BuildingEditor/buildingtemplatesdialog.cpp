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

#include "buildingtemplatesdialog.h"
#include "ui_buildingtemplatesdialog.h"

#include "buildingtemplates.h"
#include "buildingeditorwindow.h"
#include "choosebuildingtiledialog.h"
#include "roomsdialog.h"

#include "tile.h"

using namespace BuildingEditor;

static const char *categoryNames[] = {
    "exterior_walls",
    "doors",
    "door_frames",
    "windows",
    "stairs"
};

BuildingTemplatesDialog::BuildingTemplatesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTemplatesDialog),
    mTemplate(0),
    mTileRow(-1)
{
    ui->setupUi(this);

    foreach (BuildingTemplate *btemplate, BuildingTemplate::mTemplates) {
        BuildingTemplate *clone = new BuildingTemplate(btemplate);
        mTemplates += clone;
        ui->templatesList->addItem(btemplate->Name);
    }

    connect(ui->templatesList, SIGNAL(itemSelectionChanged()),
            SLOT(templateSelectionChanged()));
    connect(ui->tilesList, SIGNAL(itemSelectionChanged()),
            SLOT(tileSelectionChanged()));
    connect(ui->editRooms, SIGNAL(clicked()), SLOT(editRooms()));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));

    ui->templatesList->setCurrentRow(0);
    ui->tilesList->setCurrentRow(0);

//    synchUI();
}

BuildingTemplatesDialog::~BuildingTemplatesDialog()
{
    delete ui;
}

void BuildingTemplatesDialog::templateSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->templatesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item == 0) {
        mTemplate = 0;
        synchUI();
        return;
    }

    int row = ui->templatesList->row(item);
    mTemplate = mTemplates.at(row);
    synchUI();
}

void BuildingTemplatesDialog::tileSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mTileRow = item ? ui->tilesList->row(item) : -1;
    synchUI();
}

void BuildingTemplatesDialog::editRooms()
{
    RoomsDialog dialog(mTemplate->RoomList, this);
    if (dialog.exec() == QDialog::Accepted) {
    }
}

void BuildingTemplatesDialog::chooseTile()
{
    ChooseBuildingTileDialog dialog(QLatin1String(categoryNames[mTileRow]),
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTile *btile = dialog.selectedTile()) {
            switch (mTileRow) {
            case 0: mTemplate->Wall = btile->name(); break;
            case 1: mTemplate->DoorTile = btile->name(); break;
            case 2: mTemplate->DoorFrameTile = btile->name(); break;
            case 3: mTemplate->WindowTile = btile->name(); break;
            case 4: mTemplate->StairsTile = btile->name(); break;
            }
            setTilePixmap();
        }
    }
}

void BuildingTemplatesDialog::synchUI()
{
    ui->name->setEnabled(mTemplate != 0);
    ui->tilesList->setEnabled(mTemplate != 0);
    ui->chooseTile->setEnabled(mTemplate != 0 && mTileRow != -1);
    ui->editRooms->setEnabled(mTemplate != 0);

    if (mTemplate != 0) {
        ui->name->setText(mTemplate->Name);
    } else {
        ui->name->clear();
        ui->tilesList->clearSelection();
    }
    setTilePixmap();
}

void BuildingTemplatesDialog::setTilePixmap()
{
    if (BuildingTile *btile = selectedTile()) {
        Tiled::Tile *tile = BuildingTiles::instance()->tileFor(btile);
        ui->tileLabel->setPixmap(tile->image());
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTile *BuildingTemplatesDialog::selectedTile()
{
    if (mTemplate == 0 || mTileRow == -1)
        return 0;

    QString tileName;
    switch (mTileRow) {
    case 0: tileName = mTemplate->Wall; break;
    case 1: tileName = mTemplate->DoorTile; break;
    case 2: tileName = mTemplate->DoorFrameTile; break;
    case 3: tileName = mTemplate->WindowTile; break;
    case 4: tileName = mTemplate->StairsTile; break;
    }

    return BuildingTiles::instance()->get(
                QLatin1String(categoryNames[mTileRow]), tileName);
}
