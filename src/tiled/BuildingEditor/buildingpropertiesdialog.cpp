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
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "choosebuildingtiledialog.h"

#include "preferences.h"
#include "tile.h"

#include <QPainter>
#include <QPushButton>
#include <QRandomGenerator>

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

    connect(ui->tilesList, &QListWidget::itemSelectionChanged,
            this, &BuildingPropertiesDialog::tileSelectionChanged);
    connect(ui->tilesList, &QAbstractItemView::activated, this, &BuildingPropertiesDialog::chooseTile);
    connect(ui->clearTile, &QAbstractButton::clicked, this, &BuildingPropertiesDialog::clearTile);
    connect(ui->randomTile, &QAbstractButton::clicked, this, &BuildingPropertiesDialog::randomTile);
    connect(ui->chooseTile, &QAbstractButton::clicked, this, &BuildingPropertiesDialog::chooseTile);

    connect(ui->rooms, &QAbstractButton::clicked,
            BuildingEditorWindow::instance(), &BuildingEditorWindow::roomsDialog);
    connect(ui->makeTemplate, &QAbstractButton::clicked,
            BuildingEditorWindow::instance(), &BuildingEditorWindow::templateFromBuilding);

    connect(ui->buttonBox, &QDialogButtonBox::clicked,
            this, &BuildingPropertiesDialog::bbclicked);

    mTiles = mDocument->building()->tiles();

    ui->tilesList->setCurrentRow(0);
    synchUI();

    readSettings();
}

BuildingPropertiesDialog::~BuildingPropertiesDialog()
{
    delete ui;
}

void BuildingPropertiesDialog::synchUI()
{
    if ((selectedTile() == nullptr) || selectedTile()->isNone()) {
        ui->clearTile->setEnabled(false);
    } else {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mDocument->building()->categoryEnum(mTileRow));
        ui->clearTile->setEnabled(category->canAssignNone());
    }
    ui->randomTile->setEnabled(mTileRow != -1);
    ui->chooseTile->setEnabled(mTileRow != -1);
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
        QPixmap pixmap(64, 128);
        pixmap.fill(Tiled::Internal::Preferences::instance()->tilesetBackgroundColor());
        QPainter painter(&pixmap);
        painter.drawImage(0, 0, tile->finalImage(64, 128));
        painter.end();
        ui->tileLabel->setPixmap(pixmap);
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTileEntry *BuildingPropertiesDialog::selectedTile()
{
    if (mTileRow == -1)
        return nullptr;

    BuildingTileEntry *entry = mTiles[mTileRow];
    return entry ? entry : BuildingTilesMgr::instance()->noneTileEntry();
}

void BuildingPropertiesDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("BuildingPropertiesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void BuildingPropertiesDialog::readSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("BuildingPropertiesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
}

void BuildingPropertiesDialog::accept()
{
    apply();
    saveSettings();
    QDialog::accept();
}

void BuildingPropertiesDialog::reject()
{
    saveSettings();
    QDialog::reject();
}

void BuildingPropertiesDialog::clearTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mDocument->building()->categoryEnum(mTileRow));
    if (category->canAssignNone()) {
        mTiles[mTileRow] = BuildingTilesMgr::instance()->noneTileEntry();
        synchUI();
    }
}

void BuildingPropertiesDialog::randomTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mDocument->building()->categoryEnum(mTileRow));
    QList<BuildingTileEntry*> entries = category->entries();
    if (category->canAssignNone()) {
        entries += category->noneTileEntry();
    }
    QRandomGenerator *rand = QRandomGenerator::global();
    mTiles[mTileRow] = entries.at(rand->bounded(entries.size()));
    synchUI();
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
            synchUI();
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
