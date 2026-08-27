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

#include "building.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"

#include "preferences.h"
#include "tile.h"
#include "utils.h"

#include <QCompleter>
#include <QDebug>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>

using namespace BuildingEditor;

RoomsDialog::RoomsDialog(BuildingDocument *doc, Room *initialRoom, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RoomsDialog),
    mDocument(doc),
    mRoom(nullptr),
    mRoomItem(nullptr),
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

    ui->name->completer()->setCompletionMode(QCompleter::PopupCompletion);
    ui->name->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    ui->name->completer()->setFilterMode(Qt::MatchContains);

    ui->internalName->completer()->setCompletionMode(QCompleter::PopupCompletion);
    ui->internalName->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    ui->internalName->completer()->setFilterMode(Qt::MatchContains);

    readRoomNamesDotTxt(mRoomNames);
    QStringList roomLabels, roomInternalNames;
    for (const RoomName& roomName : std::as_const(mRoomNames)) {
        roomLabels << roomName.label;
        roomInternalNames << roomName.internalName;
    }

    ui->name->insertItems(0, roomLabels);
    ui->internalName->insertItems(0, roomInternalNames);

    {
        QUndoGroup *mUndoGroup = BuildingEditorWindow::instance()->undoGroup();
        QAction *undoAction = BuildingEditorWindow::instance()->undoAction();
        QAction *redoAction = BuildingEditorWindow::instance()->redoAction();
        connect(mUndoGroup, &QUndoGroup::undoTextChanged, this, &RoomsDialog::undoTextChanged);
        connect(mUndoGroup, &QUndoGroup::redoTextChanged, this, &RoomsDialog::redoTextChanged);

        mUndoButton = new QToolButton(this);
        mUndoButton->setText(QStringLiteral("Undo"));
        QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
        undoIcon.addFile(QLatin1String(":images/24x24/edit-undo.png"));
        mUndoButton->setIcon(undoIcon);
        Tiled::Utils::setThemeIcon(mUndoButton, "edit-undo");
        mUndoButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        mUndoButton->setEnabled(mUndoGroup->canUndo());
        mUndoButton->setShortcut(QKeySequence::Undo);
        ui->undoRedoLayout->insertWidget(0, mUndoButton);
        connect(mUndoGroup, &QUndoGroup::canUndoChanged, mUndoButton, &QWidget::setEnabled);
        connect(mUndoButton, &QAbstractButton::clicked, undoAction, &QAction::triggered);

        mRedoButton = new QToolButton(this);
        mRedoButton->setText(QStringLiteral("Redo"));
        QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
        redoIcon.addFile(QLatin1String(":images/24x24/edit-redo.png"));
        mRedoButton->setIcon(redoIcon);
        mRedoButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        Tiled::Utils::setThemeIcon(mRedoButton, "edit-redo");
        mRedoButton->setEnabled(mUndoGroup->canRedo());
        mRedoButton->setShortcut(QKeySequence::Redo);
        ui->undoRedoLayout->insertWidget(1, mRedoButton);
        connect(mUndoGroup, &QUndoGroup::canRedoChanged, mRedoButton, &QWidget::setEnabled);
        connect(mRedoButton, &QAbstractButton::clicked, redoAction, &QAction::triggered);

        connect(mDocument, &BuildingDocument::roomAdded, this, &RoomsDialog::roomAdded);
        connect(mDocument, &BuildingDocument::roomRemoved, this, &RoomsDialog::roomRemoved);
        connect(mDocument, &BuildingDocument::roomChanged, this, &RoomsDialog::roomChanged);
        connect(mDocument, &BuildingDocument::roomsReordered, this, &RoomsDialog::roomsReordered);
    }

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
    connect(ui->randomTile, &QAbstractButton::clicked, this, &RoomsDialog::randomTile);
    connect(ui->chooseTile, &QAbstractButton::clicked, this, &RoomsDialog::chooseTile);
    connect(ui->randomColor, &QAbstractButton::clicked, this, &RoomsDialog::randomiseColor);

    int currentRow = mDocument->building()->indexOf(initialRoom);
    if (currentRow != -1) {
        ui->listWidget->setCurrentRow(currentRow);
        ui->tilesList->setCurrentRow(currentRow);
    }

    readSettings();
}

RoomsDialog::~RoomsDialog()
{
    delete ui;
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
    RoomNamesFile file;
    if (!file.read(fileName)) {
        if (QFileInfo::exists(fileName)) {
            QMessageBox::warning(this, QStringLiteral("Error reading RoomNames.txt"),
                                 QStringLiteral("Failed to read %1.\n%2").arg(fileName).arg(file.errorString()));
        }
        return;
    }
    for (const RoomName &roomName : file.roomNames()) {
        if (!roomName.label.isEmpty() && !roomName.internalName.isEmpty()) {
            rooms += roomName;
        }
    }
    mRoomColorSet.insert(file.colorSet().cbegin(), file.colorSet().cend());
}

QListWidgetItem *RoomsDialog::itemFor(Room *room)
{
    int index = mDocument->building()->indexOf(room);
    if (index >= 0 && index < mDocument->building()->roomCount()) {
        return ui->listWidget->item(index);
    }
    return nullptr;
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
        if (mRoomNames[i].internalName.toLower() == internalName.toLower()) {
            return i;
        }
    }
    return -1;
}

void RoomsDialog::setRoomsList()
{
    int index = mDocument->building()->indexOf(mRoom);
    QListWidget *w = ui->listWidget;
    w->clear();
    for (Room *room : mDocument->building()->rooms()) {
        QListWidgetItem *item = new QListWidgetItem(room->Name);
        item->setData(Qt::DecorationRole, QColor(room->Color));
        w->addItem(item);
    }
    if (index != -1) {
        ui->listWidget->setCurrentRow(index);
    }
}

void RoomsDialog::synchUI()
{
    const bool hasRoom = mRoom != nullptr;
    int roomIndex = hasRoom ? mDocument->building()->indexOf(mRoom) : -1;
    ui->actionDuplicate->setEnabled(hasRoom);
    ui->actionRemove->setEnabled(hasRoom);
    ui->actionMoveUp->setEnabled(roomIndex > 0);
    ui->actionMoveDown->setEnabled(roomIndex >= 0 && roomIndex < mDocument->building()->roomCount() - 1);

    ui->name->setEnabled(hasRoom);
    ui->internalName->setEnabled(hasRoom);
    ui->color->setEnabled(hasRoom);
    ui->randomColor->setEnabled(hasRoom);
    ui->tilesList->setEnabled(hasRoom);

    bool enabled = false;
    if ((selectedTile() != nullptr) && !selectedTile()->isNone()) {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
        enabled = category->canAssignNone();
    }
    ui->clearTile->setEnabled(enabled);

    ui->randomTile->setEnabled(hasRoom);
    ui->chooseTile->setEnabled(hasRoom);

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
    if (item != nullptr) {
        mRoomItem = item;
        mRoom = mDocument->building()->room(ui->listWidget->row(mRoomItem));
    } else {
        mRoomItem = nullptr;
        mRoom = nullptr;
    }
    synchUI();
}

void RoomsDialog::addRoom()
{
    // Pick a default unused name for the new room.
    QStringList names;
    for (Room *room : mDocument->building()->rooms()) {
        names += room->internalName;
    }
    int n = 1;
    while (names.contains(tr("room%1").arg(n))) {
        n++;
    }
    Room *room = new Room;
    room->Name = tr("Room %1").arg(n);
    room->internalName = tr("room%1").arg(n);
    room->Color = pickColorForNewRoom();
    room->setTile(Room::InteriorWall, BuildingTilesMgr::instance()->defaultInteriorWall());
    room->setTile(Room::InteriorWallTrim, BuildingTilesMgr::instance()->defaultInteriorWallTrim());
    room->setTile(Room::Floor, BuildingTilesMgr::instance()->defaultFloorTile());
    room->setTile(Room::Ceiling, BuildingTilesMgr::instance()->defaultCeilingTile());

    mDocument->undoStack()->push(new AddRoom(mDocument, mDocument->building()->roomCount(), room));

//    setRoomsList();
//    ui->listWidget->setCurrentRow(mDocument->building()->roomCount() - 1);

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
}

void RoomsDialog::removeRoom()
{
    if (mRoom == nullptr) {
        return;
    }
    int index = mDocument->building()->indexOf(mRoom);
    mDocument->undoStack()->beginMacro(QStringLiteral("Remove Room"));
    for (BuildingFloor *floor : mDocument->building()->floors()) {
        bool changed = false;
        QVector<QVector<Room*> > grid = floor->grid();
        for (int x = 0; x < grid.size(); x++) {
            for (int y = 0; y < grid[x].size(); y++) {
                if (grid[x][y] == mRoom) {
                    grid[x][y] = nullptr;
                    changed = true;
                }
            }
        }
        if (changed) {
            mDocument->undoStack()->push(new SwapFloorGrid(mDocument, floor, grid, "Remove Room From Floor"));
        }
    }
    mDocument->undoStack()->push(new RemoveRoom(mDocument, index));
    mDocument->undoStack()->endMacro();
    mRoom = nullptr;
    mRoomItem = nullptr;
//    setRoomsList();
    if (index == mDocument->building()->roomCount()) {
        index = mDocument->building()->roomCount() - 1;
    }
    ui->listWidget->setCurrentRow(index);
}

void RoomsDialog::duplicateRoom()
{
    if (mRoom == nullptr)
        return;

    int index = mDocument->building()->indexOf(mRoom);

    Room *room = new Room(mRoom);
    room->Color = pickColorForNewRoom();

    mDocument->undoStack()->push(new AddRoom(mDocument, index + 1, room));

    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
}

void RoomsDialog::moveRoomUp()
{
    if (mRoom == nullptr) {
         return;
    }
    int index = mDocument->building()->indexOf(mRoom);
    if (index <= 0) {
        return;
    }
    mDocument->undoStack()->push(new ReorderRoom(mDocument, index - 1, mRoom));
    setRoomsList();
    ui->listWidget->setCurrentRow(index - 1);
}

void RoomsDialog::moveRoomDown()
{
    if (mRoom == nullptr) {
         return;
    }
    int index = mDocument->building()->indexOf(mRoom);
    if (index == mDocument->building()->roomCount() - 1) {
        return;
    }
    mDocument->undoStack()->push(new ReorderRoom(mDocument, index + 1, mRoom));
    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);
}

void RoomsDialog::nameEdited(const QString &name)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->Name == name) {
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.Name = name;
    mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::Name, -1));
}

void RoomsDialog::internalNameEdited(const QString &name)
{
    if (mRoom == nullptr) {
        ui->internalNameIcon->setVisible(false);
        return;
    }
    int roomNameIndex = findRoomNameByInternalName(name);
    if (mRoom->internalName == name) {
        ui->internalNameIcon->setVisible(BuildingPreferences::instance()->unknownRoomWarning() && (roomNameIndex == -1));
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.internalName = name;
    if (roomNameIndex != -1) {
        editedRoom.Color = mRoomNames[roomNameIndex].color.rgba();
//        ui->color->setColor();
    }
    mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::InternalName, -1));
    ui->internalNameIcon->setVisible(roomNameIndex == -1);
}

void RoomsDialog::colorChanged(const QColor &color)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->Color == color.rgba()) {
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.Color = color.rgba();
    mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::Color, -1));
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
    for (Room *room : mDocument->building()->rooms()) {
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
        Room editedRoom(mRoom);
        editedRoom.setTile(mTileRow, category->noneTileEntry());
        mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::Tile, mTileRow));
    }
}

void RoomsDialog::randomTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
    QList<BuildingTileEntry*> entries = category->entries();
    if (category->canAssignNone()) {
        entries += category->noneTileEntry();
    }
    QRandomGenerator *rand = QRandomGenerator::global();
    Room editedRoom(mRoom);
    editedRoom.setTile(mTileRow, entries.at(rand->bounded(entries.size())));
    mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::Tile, mTileRow));
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
            Room editedRoom(mRoom);
            editedRoom.setTile(mTileRow, entry);
            mDocument->undoStack()->push(new ChangeRoom(mDocument, mRoom, &editedRoom, ChangeRoom::Change::Tile, mTileRow));
        }
    }
}

void RoomsDialog::roomAdded(Room *room)
{
    setRoomsList();
    ui->listWidget->setCurrentRow(mDocument->building()->indexOf(room));
    synchUI();
}

void RoomsDialog::roomRemoved(Room *room)
{
    if (room == mRoom) {
        mRoom = nullptr;
    }
    setRoomsList();
    synchUI();
}

void RoomsDialog::roomChanged(Room *room)
{
    if (QListWidgetItem *item = itemFor(room)) {
        item->setText(room->Name);
        item->setData(Qt::DecorationRole, QColor(room->Color));
    }
    synchUI();
}

void RoomsDialog::roomsReordered()
{
    setRoomsList();
    synchUI();
}

void RoomsDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void RoomsDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void RoomsDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("RoomsDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void RoomsDialog::readSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("RoomsDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
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
