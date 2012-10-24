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
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"
#include "roomsdialog.h"

#include "tile.h"

#include <QMessageBox>

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

    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        BuildingTemplate *clone = new BuildingTemplate(btemplate);
        mTemplates += clone;
        ui->templatesList->addItem(btemplate->Name);
    }

    connect(ui->templatesList, SIGNAL(itemSelectionChanged()),
            SLOT(templateSelectionChanged()));
    connect(ui->add, SIGNAL(clicked()), SLOT(addTemplate()));
    connect(ui->remove, SIGNAL(clicked()), SLOT(removeTemplate()));
    connect(ui->duplicate, SIGNAL(clicked()), SLOT(duplicateTemplate()));
    connect(ui->name, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));
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
    qDeleteAll(mTemplates);
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

void BuildingTemplatesDialog::addTemplate()
{
    BuildingTemplate *btemplate = new BuildingTemplate;
    btemplate->Name = QLatin1String("New Template");
    btemplate->Wall = BuildingTiles::instance()->defaultExteriorWall();
    btemplate->DoorTile = BuildingTiles::instance()->defaultDoorTile();
    btemplate->DoorFrameTile = BuildingTiles::instance()->defaultDoorFrameTile();
    btemplate->WindowTile = BuildingTiles::instance()->defaultWindowTile();
    btemplate->StairsTile = BuildingTiles::instance()->defaultStairsTile();

    mTemplates += btemplate;
    ui->templatesList->addItem(btemplate->Name);
    ui->templatesList->setCurrentRow(ui->templatesList->count() - 1);
}

void BuildingTemplatesDialog::removeTemplate()
{
    if (!mTemplate)
        return;

    if (QMessageBox::question(this, tr("Remove Template"),
                              tr("Really remove the template '%1'?").arg(mTemplate->Name),
                              QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
        return;

    int index = mTemplates.indexOf(mTemplate);
    delete mTemplates.takeAt(index);
    delete ui->templatesList->takeItem(index);
}

void BuildingTemplatesDialog::duplicateTemplate()
{
    if (!mTemplate)
        return;

    BuildingTemplate *btemplate = new BuildingTemplate(mTemplate);
    mTemplates += btemplate;
    ui->templatesList->addItem(btemplate->Name);
    ui->templatesList->setCurrentRow(ui->templatesList->count() - 1);
}

void BuildingTemplatesDialog::nameEdited(const QString &name)
{
    if (!mTemplate)
        return;

    int index = mTemplates.indexOf(mTemplate);
    mTemplate->Name = name;
    ui->templatesList->item(index)->setText(name);
}

void BuildingTemplatesDialog::editRooms()
{
    RoomsDialog dialog(mTemplate->RoomList, this);
    dialog.setWindowTitle(tr("Rooms in '%1'").arg(mTemplate->Name));
    if (dialog.exec() == QDialog::Accepted) {
        qDeleteAll(mTemplate->RoomList);
        mTemplate->RoomList.clear();
        foreach (Room *dialogRoom, dialog.rooms())
            mTemplate->RoomList += new Room(dialogRoom);
    }
}

void BuildingTemplatesDialog::chooseTile()
{
    static const char *titles[] = {
        "Wall", "Door", "Door frame", "Window", "Stairs"
    };
    ChooseBuildingTileDialog dialog(tr("Choose %1 tile for '%2'")
                                    .arg(QLatin1String(titles[mTileRow]))
                                    .arg(mTemplate->Name),
                                    QLatin1String(categoryNames[mTileRow]),
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTile *btile = dialog.selectedTile()) {
            switch (mTileRow) {
            case 0: mTemplate->Wall = btile; break;
            case 1: mTemplate->DoorTile = btile; break;
            case 2: mTemplate->DoorFrameTile = btile; break;
            case 3: mTemplate->WindowTile = btile; break;
            case 4: mTemplate->StairsTile = btile; break;
            }
            setTilePixmap();
        }
    }
}

void BuildingTemplatesDialog::synchUI()
{
    ui->name->setEnabled(mTemplate != 0);
    ui->remove->setEnabled(mTemplate != 0);
    ui->duplicate->setEnabled(mTemplate != 0);
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

    switch (mTileRow) {
    case 0: return mTemplate->Wall;
    case 1: return mTemplate->DoorTile;
    case 2: return mTemplate->DoorFrameTile;
    case 3: return mTemplate->WindowTile;
    case 4: return mTemplate->StairsTile;
    }

    return 0;
}
