/*
 * Copyright 2025, Tim Baker <treectrl@users.sf.net>
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

#include "templateroomsdialog.h"
#include "ui_roomsdialog.h"

#include "building.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtemplatesdialog.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"
#include "templatedocument.h"
#include "templateundoredo.h"

#include "preferences.h"
#include "simplefile.h"
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

TemplateRoomsDialog::TemplateRoomsDialog(TemplateDocument *doc, Room *initialRoom, BuildingTemplatesDialog *parent) :
    QDialog(parent),
    ui(new Ui::RoomsDialog),
    mDocument(doc),
    mTemplate(doc->templ8()),
    mUndoGroup(parent->undoGroup()),
    mUndoStack(parent->undoStack()),
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
    for (const RoomName& roomName : mRoomNames) {
        roomLabels << roomName.label;
        roomInternalNames << roomName.internalName;
    }

    ui->name->insertItems(0, roomLabels);
    ui->internalName->insertItems(0, roomInternalNames);

    {
        QAction *undoAction = parent->undoAction();
        QAction *redoAction = parent->redoAction();
        connect(mUndoGroup, &QUndoGroup::undoTextChanged, this, &TemplateRoomsDialog::undoTextChanged);
        connect(mUndoGroup, &QUndoGroup::redoTextChanged, this, &TemplateRoomsDialog::redoTextChanged);

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

        connect(mDocument, &TemplateDocument::roomAdded, this, &TemplateRoomsDialog::roomAdded);
        connect(mDocument, &TemplateDocument::roomRemoved, this, &TemplateRoomsDialog::roomRemoved);
        connect(mDocument, &TemplateDocument::roomChanged, this, &TemplateRoomsDialog::roomChanged);
        connect(mDocument, &TemplateDocument::roomsReordered, this, &TemplateRoomsDialog::roomsReordered);
    }

    setRoomsList();

    synchUI();

    connect(ui->listWidget, &QListWidget::itemSelectionChanged,
            this, &TemplateRoomsDialog::roomSelectionChanged);
    connect(ui->actionAdd, &QAction::triggered, this, &TemplateRoomsDialog::addRoom);
    connect(ui->actionDuplicate, &QAction::triggered, this, &TemplateRoomsDialog::duplicateRoom);
    connect(ui->actionRemove, &QAction::triggered, this, &TemplateRoomsDialog::removeRoom);
    connect(ui->actionMoveUp, &QAction::triggered, this, &TemplateRoomsDialog::moveRoomUp);
    connect(ui->actionMoveDown, &QAction::triggered, this, &TemplateRoomsDialog::moveRoomDown);

    connect(ui->name, &QComboBox::currentTextChanged, this, &TemplateRoomsDialog::nameEdited);
    connect(ui->internalName, &QComboBox::currentTextChanged, this, &TemplateRoomsDialog::internalNameEdited);
    connect(ui->color, &Tiled::Internal::ColorButton::colorChanged, this, &TemplateRoomsDialog::colorChanged);
    connect(ui->tilesList, &QListWidget::itemSelectionChanged,
            this, &TemplateRoomsDialog::tileSelectionChanged);
    connect(ui->tilesList, &QAbstractItemView::activated, this, &TemplateRoomsDialog::chooseTile);
    connect(ui->clearTile, &QAbstractButton::clicked, this, &TemplateRoomsDialog::clearTile);
    connect(ui->randomTile, &QAbstractButton::clicked, this, &TemplateRoomsDialog::randomTile);
    connect(ui->chooseTile, &QAbstractButton::clicked, this, &TemplateRoomsDialog::chooseTile);
    connect(ui->randomColor, &QAbstractButton::clicked, this, &TemplateRoomsDialog::randomiseColor);

    int currentRow = mTemplate->indexOf(initialRoom);
    if (currentRow != -1) {
        ui->listWidget->setCurrentRow(currentRow);
        ui->tilesList->setCurrentRow(currentRow);
    }

    readSettings();
}

TemplateRoomsDialog::~TemplateRoomsDialog()
{
    delete ui;
}

void TemplateRoomsDialog::readRoomNamesDotTxt(QList<RoomName> &rooms)
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

void TemplateRoomsDialog::readRoomNamesDotTxt(const QString &fileName, QList<RoomName> &rooms)
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

QListWidgetItem *TemplateRoomsDialog::itemFor(Room *room)
{
    int index = mTemplate->indexOf(room);
    if (index >= 0 && index < mTemplate->roomCount()) {
        return ui->listWidget->item(index);
    }
    return nullptr;
}

int TemplateRoomsDialog::findRoomNameByLabel(const QString &label) const
{
    for (int i = 0; i < mRoomNames.size(); i++) {
        if (mRoomNames[i].label.contains(label, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

int TemplateRoomsDialog::findRoomNameByInternalName(const QString &internalName) const
{
    for (int i = 0; i < mRoomNames.size(); i++) {
        if (mRoomNames[i].internalName.contains(internalName, Qt::CaseInsensitive)) {
            return i;
        }
    }
    return -1;
}

void TemplateRoomsDialog::setRoomsList()
{
    int index = mTemplate->indexOf(mRoom);
    QListWidget *w = ui->listWidget;
    w->clear();
    for (Room *room : mTemplate->rooms()) {
        QListWidgetItem *item = new QListWidgetItem(room->Name);
        item->setData(Qt::DecorationRole, QColor(room->Color));
        w->addItem(item);
    }
    if (index != -1) {
        ui->listWidget->setCurrentRow(index);
    }
}

void TemplateRoomsDialog::synchUI()
{
    const bool hasRoom = mRoom != nullptr;
    int roomIndex = hasRoom ? mTemplate->indexOf(mRoom) : -1;
    ui->actionDuplicate->setEnabled(hasRoom);
    ui->actionRemove->setEnabled(hasRoom);
    ui->actionMoveUp->setEnabled(roomIndex > 0);
    ui->actionMoveDown->setEnabled(roomIndex >= 0 && roomIndex < mTemplate->roomCount() - 1);

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

    ui->randomTile->setEnabled(mRoom != nullptr);
    ui->chooseTile->setEnabled(mRoom != nullptr);

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

void TemplateRoomsDialog::roomSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->listWidget->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item != nullptr) {
        mRoomItem = item;
        mRoom = mTemplate->room(ui->listWidget->row(mRoomItem));
    } else {
        mRoomItem = nullptr;
        mRoom = nullptr;
    }
    synchUI();
}

void TemplateRoomsDialog::addRoom()
{
    // Pick a default unused name for the new room.
    QStringList names;
    for (Room *room : mTemplate->rooms()) {
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

    mUndoStack->push(new TemplateAddRoom(mDocument, mTemplate->roomCount(), room));

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
}

void TemplateRoomsDialog::removeRoom()
{
    if (mRoom == nullptr) {
        return;
    }
    int index = mTemplate->indexOf(mRoom);
    mUndoStack->push(new TemplateRemoveRoom(mDocument, index));
    mRoom = nullptr;
    mRoomItem = nullptr;
//    setRoomsList();
    if (index == mTemplate->roomCount()) {
        index = mTemplate->roomCount() - 1;
    }
    ui->listWidget->setCurrentRow(index);
}

void TemplateRoomsDialog::duplicateRoom()
{
    if (mRoom == nullptr)
        return;

    int index = mTemplate->indexOf(mRoom);

    Room *room = new Room(mRoom);
    room->Color = pickColorForNewRoom();

    mUndoStack->push(new TemplateAddRoom(mDocument, index + 1, room));

    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);

    ui->name->setFocus();
    ui->name->lineEdit()->selectAll();
}

void TemplateRoomsDialog::moveRoomUp()
{
    if (mRoom == nullptr) {
         return;
    }
    int index = mTemplate->indexOf(mRoom);
    if (index <= 0) {
        return;
    }
    mUndoStack->push(new TemplateReorderRoom(mDocument, index - 1, mRoom));
    setRoomsList();
    ui->listWidget->setCurrentRow(index - 1);
}

void TemplateRoomsDialog::moveRoomDown()
{
    if (mRoom == nullptr) {
         return;
    }
    int index = mTemplate->indexOf(mRoom);
    if (index == mTemplate->roomCount() - 1) {
        return;
    }
    mUndoStack->push(new TemplateReorderRoom(mDocument, index + 1, mRoom));
    setRoomsList();
    ui->listWidget->setCurrentRow(index + 1);
}

void TemplateRoomsDialog::nameEdited(const QString &name)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->Name == name) {
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.Name = name;
    mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::Name, -1));
}

void TemplateRoomsDialog::internalNameEdited(const QString &name)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->internalName == name) {
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.internalName = name;
    int roomNameIndex = findRoomNameByInternalName(name);
    if (roomNameIndex != -1) {
        editedRoom.Color = mRoomNames[roomNameIndex].color.rgba();
//        ui->color->setColor();
    }
    mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::InternalName, -1));
}

void TemplateRoomsDialog::colorChanged(const QColor &color)
{
    if (mRoom == nullptr) {
        return;
    }
    if (mRoom->Color == color.rgba()) {
        return;
    }
    Room editedRoom(mRoom);
    editedRoom.Color = color.rgba();
    mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::Color, -1));
}

void TemplateRoomsDialog::randomiseColor()
{
    ui->color->setColor(pickColorForNewRoom());
}

void TemplateRoomsDialog::tileSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mTileRow = item ? ui->tilesList->row(item) : -1;
    synchUI();
}

void TemplateRoomsDialog::setTilePixmap()
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

BuildingTileEntry *TemplateRoomsDialog::selectedTile()
{
    if (mRoom == 0 || mTileRow == -1)
        return 0;

    BuildingTileEntry *entry = mRoom->tile(mTileRow);
    return entry ? entry : BuildingTilesMgr::instance()->noneTileEntry();
}

QRgb TemplateRoomsDialog::pickColorForNewRoom()
{
    std::set<QColor, decltype(&compareQColors)> colors(compareQColors);
    colors.insert(mRoomColorSet.cbegin(), mRoomColorSet.cend());
    for (Room *room : mTemplate->rooms()) {
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

void TemplateRoomsDialog::clearTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
    if (category->canAssignNone()) {
        Room editedRoom(mRoom);
        editedRoom.setTile(mTileRow, category->noneTileEntry());
        mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::Tile, mTileRow));
    }
}

void TemplateRoomsDialog::randomTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mRoom->categoryEnum(mTileRow));
    QList<BuildingTileEntry*> entries = category->entries();
    if (category->canAssignNone()) {
        entries += category->noneTileEntry();
    }
    QRandomGenerator *rand = QRandomGenerator::global();
    Room editedRoom(mRoom);
    editedRoom.setTile(mTileRow, entries.at(rand->bounded(entries.size())));
    mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::Tile, mTileRow));
}

void TemplateRoomsDialog::chooseTile()
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
            mUndoStack->push(new TemplateChangeRoom(mDocument, mRoom, &editedRoom, TemplateChangeRoom::Change::Tile, mTileRow));
        }
    }
}

void TemplateRoomsDialog::roomAdded(Room *room)
{
    setRoomsList();
    ui->listWidget->setCurrentRow(mTemplate->indexOf(room));
    synchUI();
}

void TemplateRoomsDialog::roomRemoved(Room *room)
{
    if (room == mRoom) {
        mRoom = nullptr;
    }
    setRoomsList();
    synchUI();
}

void TemplateRoomsDialog::roomChanged(Room *room)
{
    if (QListWidgetItem *item = itemFor(room)) {
        item->setText(room->Name);
        item->setData(Qt::DecorationRole, QColor(room->Color));
    }
    synchUI();
}

void TemplateRoomsDialog::roomsReordered()
{
    setRoomsList();
    synchUI();
}

void TemplateRoomsDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void TemplateRoomsDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void TemplateRoomsDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("TemplateRoomsDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void TemplateRoomsDialog::readSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("TemplateRoomsDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
}

void TemplateRoomsDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void TemplateRoomsDialog::reject()
{
    saveSettings();
    QDialog::reject();
}
