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

#include "buildingpropertiesdialog.h"
#include "ui_buildingpropertiesdialog.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "choosebuildingtiledialog.h"

#include "tile.h"

#include <QPushButton>

using namespace BuildingEditor;

BuildingPropertiesDialog::BuildingPropertiesDialog(BuildingDocument *doc,
                                                   QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingPropertiesDialog),
    mDocument(doc),
    mTileRow(-1),
    mTiles(Building::TileCount)
{
    ui->setupUi(this);

    ui->tilesList->clear();
    ui->tilesList->addItems(BuildingTemplate::enumTileNames());

    connect(ui->tilesList, SIGNAL(itemSelectionChanged()),
            SLOT(tileSelectionChanged()));
    connect(ui->tilesList, SIGNAL(activated(QModelIndex)), SLOT(chooseTile()));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));

    connect(ui->rooms, SIGNAL(clicked()),
            BuildingEditorWindow::instance(), SLOT(roomsDialog()));
    connect(ui->makeTemplate, SIGNAL(clicked()),
            BuildingEditorWindow::instance(), SLOT(templateFromBuilding()));

    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)),
            SLOT(bbclicked(QAbstractButton*)));

    mTiles = mDocument->building()->tiles();

    ui->tilesList->setCurrentRow(0);
    synchUI();
}

BuildingPropertiesDialog::~BuildingPropertiesDialog()
{
    delete ui;
}

void BuildingPropertiesDialog::synchUI()
{
    setTilePixmap();
}

void BuildingPropertiesDialog::tileSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mTileRow = item ? ui->tilesList->row(item) : -1;
    synchUI();
}

void BuildingPropertiesDialog::setTilePixmap()
{
    if (BuildingTileEntry *entry = selectedTile()) {
        Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile());
        ui->tileLabel->setPixmap(QPixmap::fromImage(tile->finalImage(64, 128)));
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTileEntry *BuildingPropertiesDialog::selectedTile()
{
    if (mTileRow == -1)
        return 0;

    BuildingTileEntry *entry = mTiles[mTileRow];
    return entry ? entry : BuildingTilesMgr::instance()->noneTileEntry();
}

void BuildingPropertiesDialog::accept()
{
    apply();
    QDialog::accept();
}

void BuildingPropertiesDialog::chooseTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(
                mDocument->building()->categoryEnum(mTileRow));
    ChooseBuildingTileDialog dialog(tr("Choose %1 tile for building")
                                    .arg(category->label()),
                                    category,
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTileEntry *entry = dialog.selectedTile()) {
            mTiles[mTileRow] = entry;
            setTilePixmap();
        }
    }
}

void BuildingPropertiesDialog::bbclicked(QAbstractButton *button)
{
    if (button == ui->buttonBox->button(QDialogButtonBox::Apply))
        apply();
}

void BuildingPropertiesDialog::apply()
{
    QVector<int> changed;
    for (int e = 0; e < mTiles.size(); e++) {
        if (mTiles[e] != mDocument->building()->tile(e))
            changed += e;
    }
    if (changed.size()) {
        QUndoStack *undoStack = mDocument->undoStack();
        undoStack->beginMacro(tr("Change Building Tiles"));
        foreach (int e, changed) {
            undoStack->push(new ChangeBuildingTile(mDocument, e, mTiles[e],
                                                   false));
            if (e == Building::RoofCap || e == Building::RoofSlope) {
                int which = (e == Building::RoofCap) ? RoofObject::TileCap
                                                     : RoofObject::TileSlope;
                // Change the tiles for each roof object.
                foreach (BuildingObject *object, mDocument->building()->objects()) {
                    if (RoofObject *roof = object->asRoof()) {
                        if (roof->tile(which) != mTiles[e]) {
                            undoStack->push(new ChangeObjectTile(mDocument,
                                                                 roof, mTiles[e],
                                                                 false, which));
                        }
                    }
                }
            }
        }
        undoStack->endMacro();
    }
}
