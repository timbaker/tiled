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

#include "roomsdialog.h"
#include "ui_roomsdialog.h"

#include "buildingtemplates.h"
#include "buildingeditorwindow.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"

#include "tile.h"

using namespace BuildingEditor;

static const char *categoryNames[] = {
    "interior_walls",
    "floors"
};

RoomsDialog::RoomsDialog(const QList<Room*> &rooms, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RoomsDialog),
    mRoom(0),
    mRoomItem(0),
    mTileRow(-1)
{
    ui->setupUi(this);

    foreach (Room *room, rooms) {
        Room *copy = new Room();
        *copy = *room;
        mRooms += copy;
        mRoomsMap[copy] = room;
    }

    setRoomsList();

    synchUI();

    connect(ui->listWidget, SIGNAL(itemSelectionChanged()),
            SLOT(roomSelectionChanged()));
    connect(ui->add, SIGNAL(clicked()), SLOT(addRoom()));
    connect(ui->remove, SIGNAL(clicked()), SLOT(removeRoom()));
    connect(ui->moveUp, SIGNAL(clicked()), SLOT(moveRoomUp()));
    connect(ui->moveDown, SIGNAL(clicked()), SLOT(moveRoomDown()));

    connect(ui->name, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));
    connect(ui->internalName, SIGNAL(textEdited(QString)), SLOT(internalNameEdited(QString)));
    connect(ui->color, SIGNAL(colorChanged(QColor)), SLOT(colorChanged(QColor)));
    connect(ui->tilesList, SIGNAL(itemSelectionChanged()),
            SLOT(tileSelectionChanged()));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));

    if (rooms.count()) {
        ui->listWidget->setCurrentRow(0);
        ui->tilesList->setCurrentRow(0);
    }
}

RoomsDialog::~RoomsDialog()
{
    delete ui;
    qDeleteAll(mRooms);
}

Room *RoomsDialog::originalRoom(Room *dialogRoom) const
{
    return mRoomsMap[dialogRoom];
}

void RoomsDialog::setRoomsList()
{
    QListWidget *w = ui->listWidget;
    w->clear();
    foreach (Room *room, mRooms) {
        QListWidgetItem *item = new QListWidgetItem(room->Name);
        item->setData(Qt::DecorationRole, QColor(room->Color));
        w->addItem(item);
    }
}

void RoomsDialog::synchUI()
{
    int roomIndex = mRoom ? mRooms.indexOf(mRoom) : -1;
    ui->remove->setEnabled(mRoom != 0);
    ui->moveUp->setEnabled(roomIndex > 0);
    ui->moveDown->setEnabled(roomIndex >= 0 && roomIndex < mRooms.count() - 1);

    ui->name->setEnabled(mRoom != 0);
    ui->internalName->setEnabled(mRoom != 0);
    ui->color->setEnabled(mRoom != 0);
    ui->tilesList->setEnabled(mRoom != 0);
    ui->chooseTile->setEnabled(mRoom != 0);

    if (mRoom) {
        ui->name->setText(mRoom->Name);
        ui->internalName->setText(mRoom->internalName);
        ui->color->setColor(mRoom->Color);
    } else {
        ui->name->clear();
        ui->internalName->clear();
    }
    setTilePixmap();
}

void RoomsDialog::roomSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->listWidget->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item != 0) {
        mRoomItem = item;
        mRoom = mRooms.at(ui->listWidget->row(mRoomItem));
    } else {
        mRoomItem = 0;
        mRoom = 0;
    }
    synchUI();
}

void RoomsDialog::addRoom()
{
    // Assign an unused color from this default list of room colors.
    // TODO: Add more colors.
    QList<QRgb> colors;
    colors += qRgb(200, 200, 200);
    colors += qRgb(128, 128, 128);
    colors += qRgb(255, 0, 0);
    colors += qRgb(0, 255, 0);
    colors += qRgb(0, 0, 255);
    colors += qRgb(0, 255, 255);
    colors += qRgb(255, 255, 0);
    colors += qRgb(255, 200, 0);
    colors += qRgb(255, 150, 0);
    colors += qRgb(255, 110, 0);

    QRgb color = qRgb(255,255,255);
    foreach (Room *room, mRooms)
        colors.removeOne(room->Color);
    if (colors.count())
        color = colors.first();

    Room *room = new Room;
    room->Name = QLatin1String("New Room");
    room->internalName = QLatin1String("newroom");
    room->Color = color;
    BuildingTile *btile = BuildingTiles::instance()->defaultInteriorWall();
    room->Wall = btile;
    btile = BuildingTiles::instance()->defaultFloorTile();
    room->Floor = btile;

    mRooms += room;
    mRoomsMap[room] = 0;

    setRoomsList();
    ui->listWidget->setCurrentRow(mRooms.count() - 1);

    ui->name->setFocus();
    ui->name->selectAll();
}

void RoomsDialog::removeRoom()
{
    if (!mRoom)
        return;

    int index = mRooms.indexOf(mRoom);
    mRooms.removeAt(index);
    delete mRoom;
    mRoom = 0;
    mRoomItem = 0;

    setRoomsList();
    if (index == mRooms.count())
        index = mRooms.count() - 1;
    ui->listWidget->setCurrentRow(index);
}

void RoomsDialog::moveRoomUp()
{
    if (!mRoom)
         return;
    int index = mRooms.indexOf(mRoom);
    if (index == 0)
        return;
    mRooms.takeAt(index);
    mRooms.insert(index - 1, mRoom);

    setRoomsList();
    ui->listWidget->setCurrentRow(index - 1);
}

void RoomsDialog::moveRoomDown()
{
    if (!mRoom)
         return;
    int index = mRooms.indexOf(mRoom);
    if (index == mRooms.count() - 1)
        return;
    mRooms.takeAt(index);
    mRooms.insert(index + 1, mRoom);

    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);
}

void RoomsDialog::nameEdited(const QString &name)
{
    if (mRoom != 0) {
        mRoom->Name = name;
        mRoomItem->setText(name);
    }
}

void RoomsDialog::internalNameEdited(const QString &name)
{
    if (mRoom != 0) {
        mRoom->internalName = name;
    }
}

void RoomsDialog::colorChanged(const QColor &color)
{
    if (mRoom != 0) {
        mRoom->Color = color.rgba();
        mRoomItem->setData(Qt::DecorationRole, color);
    }
}

void RoomsDialog::tileSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mTileRow = item ? ui->tilesList->row(item) : -1;
    synchUI();
}

void RoomsDialog::setTilePixmap()
{
    if (BuildingTile *btile = selectedTile()) {
        Tiled::Tile *tile = BuildingTiles::instance()->tileFor(btile);
        ui->tileLabel->setPixmap(tile->image());
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTile *RoomsDialog::selectedTile()
{
    if (mRoom == 0 || mTileRow == -1)
        return 0;

    switch (mTileRow) {
    case 0: return mRoom->Wall;
    case 1: return mRoom->Floor;
    }

    return 0;
}

void RoomsDialog::chooseTile()
{
    static const char *titles[] = { "Wall", "Floor" };
    ChooseBuildingTileDialog dialog(tr("Choose %1 tile for '%2'")
                                    .arg(QLatin1String(titles[mTileRow]))
                                    .arg(mRoom->Name),
                                    QLatin1String(categoryNames[mTileRow]),
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTile *btile = dialog.selectedTile()) {
            switch (mTileRow) {
            case 0: mRoom->Wall = btile; break;
            case 1: mRoom->Floor = btile; break;
            }
            setTilePixmap();
        }
    }
}

