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

#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"

#include "preferences.h"
#include "simplefile.h"
#include "tile.h"

#include <QCompleter>
#include <QDebug>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QToolBar>

using namespace BuildingEditor;

RoomsDialog::RoomsDialog(const QList<Room*> &rooms, Room *initialRoom, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RoomsDialog),
    mRoom(0),
    mRoomItem(0),
    mTileRow(-1),
    mRoomColorSet(compareQColors)
{
    ui->setupUi(this);

    ui->tilesList->clear();
    ui->tilesList->addItems(Room::enumLabels());

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAdd);
    toolBar->addAction(ui->actionDuplicate);
    toolBar->addAction(ui->actionRemove);
#if 1
    toolBar->addSeparator();
#else
    QWidget *spacerWidget = new QWidget(this);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    toolBar->addWidget(spacerWidget);
#endif
    toolBar->addAction(ui->actionMoveUp);
    toolBar->addAction(ui->actionMoveDown);
    ui->toolBarLayout->addWidget(toolBar);

    int currentRow = -1;
    foreach (Room *room, rooms) {
        Room *copy = new Room();
        *copy = *room;
        mRooms += copy;
        mRoomsMap[copy] = room;
        if (room == initialRoom) {
            currentRow = rooms.indexOf(room);
        }
    }

    ui->name->completer()->setCompletionMode(QCompleter::PopupCompletion);
    ui->name->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    ui->name->completer()->setFilterMode(Qt::MatchContains);

    ui->internalName->completer()->setCompletionMode(QCompleter::PopupCompletion);
    ui->internalName->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    ui->internalName->completer()->setFilterMode(Qt::MatchContains);

    readRoomNamesDotTxt(mRoomNames);
    QStringList roomLabels, roomInternalNames;
    for (const RoomName& roomName : mRoomNames) {
        roomLabels << roomName.label;
        roomInternalNames << roomName.internalName;
    }

    ui->name->insertItems(0, roomLabels);
    ui->internalName->insertItems(0, roomInternalNames);

    setRoomsList();

    synchUI();

    connect(ui->listWidget, &QListWidget::itemSelectionChanged,
            this, &RoomsDialog::roomSelectionChanged);
    connect(ui->actionAdd, &QAction::triggered, this, &RoomsDialog::addRoom);
    connect(ui->actionDuplicate, &QAction::triggered, this, &RoomsDialog::duplicateRoom);
    connect(ui->actionRemove, &QAction::triggered, this, &RoomsDialog::removeRoom);
    connect(ui->actionMoveUp, &QAction::triggered, this, &RoomsDialog::moveRoomUp);
    connect(ui->actionMoveDown, &QAction::triggered, this, &RoomsDialog::moveRoomDown);

    connect(ui->name, &QComboBox::currentTextChanged, this, &RoomsDialog::nameEdited);
    connect(ui->internalName, &QComboBox::currentTextChanged, this, &RoomsDialog::internalNameEdited);
    connect(ui->color, &Tiled::Internal::ColorButton::colorChanged, this, &RoomsDialog::colorChanged);
    connect(ui->tilesList, &QListWidget::itemSelectionChanged,
            this, &RoomsDialog::tileSelectionChanged);
    connect(ui->tilesList, &QAbstractItemView::activated, this, &RoomsDialog::chooseTile);
    connect(ui->clearTile, &QAbstractButton::clicked, this, &RoomsDialog::clearTile);
    connect(ui->chooseTile, &QAbstractButton::clicked, this, &RoomsDialog::chooseTile);
    connect(ui->randomColor, &QAbstractButton::clicked, this, &RoomsDialog::randomiseColor);

    if (currentRow != -1) {
        ui->listWidget->setCurrentRow(currentRow);
        ui->tilesList->setCurrentRow(currentRow);
    }

    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("RoomsDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
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

bool BuildingEditor::compareQColors(const QColor& a, const QColor& b)
{
    return a.rgba() < b.rgba(); // Comparing RGBA values is a simple way
}

void RoomsDialog::readRoomNamesDotTxt(QList<RoomName> &rooms)
{
    mRoomColorSet.clear();

    QString filePath;

    // Read the application's RoomNames.txt
    filePath = Tiled::Internal::Preferences::instance()->appConfigPath(QStringLiteral("RoomNames.txt"));
    readRoomNamesDotTxt(filePath, rooms);

    // Read the user's optional RoomNames.txt
    filePath = Tiled::Internal::Preferences::instance()->configPath(QStringLiteral("RoomNames.txt"));
    readRoomNamesDotTxt(filePath, rooms);
}

void RoomsDialog::readRoomNamesDotTxt(const QString &fileName, QList<RoomName> &rooms)
{
    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        if (QFileInfo::exists(fileName)) {
            QMessageBox::warning(this, QStringLiteral("Error reading RoomNames.txt"),
                                 QStringLiteral("Failed to open %1").arg(fileName));
        }
        return;
    }

    QRandomGenerator *generator = QRandomGenerator::global();
    for (const SimpleFileBlock &block : simpleFile.blocks) {
        if (block.name == QStringLiteral("room")) {
            RoomName roomName;
            roomName.internalName = block.value("internal").trimmed();
            roomName.label = block.value("label").trimmed();
            if (block.hasValue("color") && !block.value("color").trimmed().isEmpty()) {
                QColor color = QColor(block.value("color").trimmed());
                if (color.isValid()) {
                    roomName.color = color;
                }
            }
            if (!roomName.color.isValid()) {
                QColor randomColor;
                do {
                    int red = generator->bounded(256); // 0 to 255
                    int green = generator->bounded(256); // 0 to 255
                    int blue = generator->bounded(256); // 0 to 255
                    randomColor = QColor(red, green, blue);
                } while (mRoomColorSet.find(randomColor) != mRoomColorSet.end());
                roomName.color = randomColor;
            }
            mRoomColorSet.insert(roomName.color);
            if (!roomName.label.isEmpty() && !roomName.internalName.isEmpty()) {
                rooms += roomName;
            }
        }
    }
}

int RoomsDialog::findRoomNameByLabel(const QString &label) const
{
    for (int i = 0; i < mRoomNames.size(); i++) {
        if (mRoomNames[i].label.contains(label, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

int RoomsDialog::findRoomNameByInternalName(const QString &internalName) const
{
    for (int i = 0; i < mRoomNames.size(); i++) {
        if (mRoomNames[i].internalName.contains(internalName, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
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
    ui->actionDuplicate->setEnabled(mRoom != 0);
    ui->actionRemove->setEnabled(mRoom != 0);
    ui->actionMoveUp->setEnabled(roomIndex > 0);
    ui->actionMoveDown->setEnabled(roomIndex >= 0 && roomIndex < mRooms.count() - 1);

    ui->name->setEnabled(mRoom != 0);
    ui->internalName->setEnabled(mRoom != 0);
    ui->color->setEnabled(mRoom != 0);
    ui->tilesList->setEnabled(mRoom != 0);

    bool enabled = false;
    if (mRoom != nullptr && mTileRow != -1) {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
        enabled = category->canAssignNone();
    }
    ui->clearTile->setEnabled(enabled);

    ui->chooseTile->setEnabled(mRoom != 0);

    if (mRoom) {
        int index = ui->name->findText(mRoom->Name);
        if (index != -1) {
            ui->name->setCurrentIndex(index);
        } else {
            ui->name->setCurrentText(mRoom->Name);
        }
        index = ui->internalName->findText(mRoom->internalName);
        if (index != -1) {
            ui->internalName->setCurrentIndex(index);
        } else {
            ui->internalName->setCurrentText(mRoom->internalName);
        }
        ui->color->setColor(mRoom->Color);
    } else {
        ui->name->lineEdit()->clear();
        ui->internalName->lineEdit()->clear();
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
    // Pick a default unused name for the new room.
    QStringList names;
    foreach (Room *room, mRooms)
        names += room->internalName;
    int n = 1;
    while (names.contains(tr("room%1").arg(n)))
        n++;

    Room *room = new Room;
    room->Name = tr("Room %1").arg(n);
    room->internalName = tr("room%1").arg(n);
    room->Color = pickColorForNewRoom();
    room->setTile(Room::InteriorWall, BuildingTilesMgr::instance()->defaultInteriorWall());
    room->setTile(Room::InteriorWallTrim, BuildingTilesMgr::instance()->defaultInteriorWallTrim());
    room->setTile(Room::Floor, BuildingTilesMgr::instance()->defaultFloorTile());
    room->setTile(Room::Ceiling, BuildingTilesMgr::instance()->defaultCeilingTile());

    mRooms += room;
    mRoomsMap[room] = 0;

    setRoomsList();
    ui->listWidget->setCurrentRow(mRooms.count() - 1);

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
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

void RoomsDialog::duplicateRoom()
{
    if (!mRoom)
        return;

    int index = mRooms.indexOf(mRoom);

    Room *room = new Room(mRoom);
    room->Color = pickColorForNewRoom();
    mRooms.insert(index + 1, room);
    mRoomsMap[room] = 0;

    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
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
    if (mRoom == nullptr) {
        return;
    }
    mRoom->Name = name;
    mRoomItem->setText(name);
}

void RoomsDialog::internalNameEdited(const QString &name)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->internalName == name) {
        return;
    }
    mRoom->internalName = name;
    int roomNameIndex = findRoomNameByInternalName(name);
    if (roomNameIndex != -1) {
        ui->color->setColor(mRoomNames[roomNameIndex].color);
    }
}

void RoomsDialog::colorChanged(const QColor &color)
{
    if (mRoom == nullptr) {
        return;
    }
    mRoom->Color = color.rgba();
    mRoomItem->setData(Qt::DecorationRole, color);
}

void RoomsDialog::randomiseColor()
{
    ui->color->setColor(pickColorForNewRoom());
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
    if (BuildingTileEntry *entry = selectedTile()) {
        Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile());
        ui->tileLabel->setPixmap(QPixmap::fromImage(tile->finalImage(64, 128)));
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTileEntry *RoomsDialog::selectedTile()
{
    if (mRoom == 0 || mTileRow == -1)
        return 0;

    BuildingTileEntry *entry = mRoom->tile(mTileRow);
    return entry ? entry : BuildingTilesMgr::instance()->noneTileEntry();
}

QRgb RoomsDialog::pickColorForNewRoom()
{
    std::set<QColor, decltype(&compareQColors)> colors(compareQColors);
    colors.insert(mRoomColorSet.cbegin(), mRoomColorSet.cend());
    for (Room *room : mRooms) {
        colors.insert(room->Color);
    }
    QColor randomColor;
    QRandomGenerator *generator = QRandomGenerator::global();
    do {
        int red = generator->bounded(256); // 0 to 255
        int green = generator->bounded(256); // 0 to 255
        int blue = generator->bounded(256); // 0 to 255
        randomColor = QColor(red, green, blue);
    } while (colors.find(randomColor) != colors.end());
    return randomColor.rgb();
}

void RoomsDialog::clearTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
    if (category->canAssignNone()) {
        mRoom->setTile(mTileRow, category->noneTileEntry());
        setTilePixmap();
    }
}

void RoomsDialog::chooseTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(
                mRoom->categoryEnum(mTileRow));
    ChooseBuildingTileDialog dialog(tr("Choose %1 tile for '%2'")
                                    .arg(category->label())
                                    .arg(mRoom->Name),
                                    category,
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTileEntry *entry = dialog.selectedTile()) {
            mRoom->setTile(mTileRow, entry);
            setTilePixmap();
        }
    }
}

void RoomsDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("RoomsDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void RoomsDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void RoomsDialog::reject()
{
    saveSettings();
    QDialog::reject();
}
