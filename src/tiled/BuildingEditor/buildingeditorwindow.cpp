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

#include "buildingeditorwindow.h"
#include "ui_buildingeditorwindow.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingtools.h"
#include "FloorEditor.h"
#include "simplefile.h"

#include <QComboBox>
#include <QDir>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QMessageBox>
#include <QUndoGroup>

using namespace BuildingEditor;

/////

BuildingEditorWindow* BuildingEditorWindow::instance = 0;

BuildingEditorWindow::BuildingEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingEditorWindow),
    mCurrentDocument(0),
    roomEditor(new FloorEditor(this)),
    room(new QComboBox()),
    mUndoGroup(new QUndoGroup(this))
{
    ui->setupUi(this);

    instance = this;

    ui->toolBar->insertSeparator(ui->actionUpLevel);
    ui->toolBar->insertWidget(ui->actionUpLevel, room);
    ui->toolBar->insertSeparator(ui->actionUpLevel);

    QLabel *label = new QLabel;
    label->setText(tr("Ground Floor"));
    ui->toolBar->insertWidget(ui->actionUpLevel, label);

    QGraphicsView *view = new QGraphicsView(this);
    view->setScene(roomEditor);
    view->setMouseTracking(true);

    setCentralWidget(view);

    connect(ui->actionPecil, SIGNAL(triggered()),
            PencilTool::instance(), SLOT(activate()));
    PencilTool::instance()->setEditor(roomEditor);
    PencilTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionEraser, SIGNAL(triggered()),
            EraserTool::instance(), SLOT(activate()));
    EraserTool::instance()->setEditor(roomEditor);
    EraserTool::instance()->setAction(ui->actionEraser);

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setShortcuts(QKeySequence::Redo);
    ui->menuEdit->insertAction(0, redoAction);
    ui->menuEdit->insertAction(0, undoAction);
}

BuildingEditorWindow::~BuildingEditorWindow()
{
    delete ui;
}

bool BuildingEditorWindow::Startup()
{
#if 1
    if (!LoadBuildingTemplates())
        return false;

#else
    // NewBuildingDialog
    BuildingDefinition *def = new BuildingDefinition;
    def->Name = QLatin1String("Suburban 1");
    def->Wall = QLatin1String("walls_exterior_house_01_32");

    Room *room = new Room;
    room->Name = QLatin1String("Living Room");
    room->Color = qRgb(255, 0, 0);
    room->internalName = QLatin1String("livingroom");
    room->Wall = QLatin1String("walls_interior_house_01_20");
    room->Floor = QLatin1String("floors_interior_tilesandwood_01_42");
    def->RoomList += room;

    BuildingDefinition::Definitions += def;
    BuildingDefinition::DefinitionMap[def->Name] = def;
#endif

    RoomDefinitionManager::instance->Init(BuildingDefinition::Definitions.first());

    Building *building = new Building(20, 20);
    building->insertFloor(0, new BuildingFloor(building));

    mCurrentDocument = new BuildingDocument(building, QString());
    mUndoGroup->addStack(mCurrentDocument->undoStack());
    mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

    roomEditor->setDocument(mCurrentDocument);

    ///// NewBuildingDialog.exec()
    roomEditor->currentFloor = building->floors().first();
    roomEditor->mFloorItems += new GraphicsFloorItem(roomEditor->currentFloor);
    this->room->addItems(RoomDefinitionManager::instance->FillCombo());
    roomEditor->UpdateMetaBuilding();

    roomEditor->addItem(roomEditor->mFloorItems.first());
    roomEditor->addItem(new GraphicsGridItem(building->width(),
                                             building->height()));
    /////


    return true;
}

bool BuildingEditorWindow::LoadBuildingTemplates()
{
    QDir d = QDir(QCoreApplication::applicationDirPath() + QLatin1Char('/')
                  + QLatin1String("BuildingTemplates"));
    if (!d.exists()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("The BuildingTemplates directory doesn't exist."));
        return false;
    }

    foreach (QFileInfo file, d.entryInfoList(QStringList() << QLatin1String("*.txt"), QDir::Files)) {
        QString path = file.absoluteFilePath();
        SimpleFile simple;
        if (!simple.read(path)) {
            QMessageBox::critical(this, tr("It's no good, Jim!"),
                                  tr("Error reading %1.").arg(path));
            return false;
        }
        BuildingDefinition *def = new BuildingDefinition;
        def->Name = simple.value("Name");
        def->Wall = simple.value("Wall");
        foreach (SimpleFileBlock block, simple.blocks) {
            if (block.name == QLatin1String("Room")) {
                Room *room = new Room;
                room->Name = block.value("Name");
                QStringList rgb = block.value("Color").split(QLatin1String(" "), QString::SkipEmptyParts);
                room->Color = qRgb(rgb.at(0).toInt(),
                                   rgb.at(1).toInt(),
                                   rgb.at(2).toInt());
                room->Wall = block.value("Wall");
                room->Floor = block.value("Floor");
                room->internalName = block.value("InternalName");
                def->RoomList += room;
            } else {
                QMessageBox::critical(this, tr("It's no good, Jim!"),
                                      tr("Unknown block name '%1'.\n%2")
                                      .arg(block.name)
                                      .arg(path));
                return false;
            }
        }

        BuildingDefinition::Definitions += def;
        BuildingDefinition::DefinitionMap[def->Name] = def;
    }

    if (BuildingDefinition::Definitions.isEmpty()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("No buildings were defined in BuildingTemplates/ directory."));
        return false;
    }

    return true;
}

Room *BuildingEditorWindow::currentRoom() const
{
    int roomIndex = room->currentIndex();
    return RoomDefinitionManager::instance->getRoom(roomIndex);
}

/////

WallTypes *WallTypes::instance = new WallTypes;

void WallTypes::Add(QString name, int first)
{
    WallType *t = new WallType(name, first);
    ITypes += t;
    ITypesByName[t->ToString()] = t;
}

void WallTypes::AddExt(QString name, int first)
{
    ETypes += new WallType(name, first);
}

WallType *WallTypes::getEWallFromName(QString exteriorWall)
{
    foreach (WallType *wallType, ETypes) {
        if (wallType->ToString() == exteriorWall)
            return wallType;
    }
    return 0;
}

WallType *WallTypes::getOrAdd(QString exteriorWall)
{
    foreach (WallType *type, ETypes) {
        if (type->ToString() == exteriorWall)
            return type;
    }

    AddExt(exteriorWall.mid(0, exteriorWall.lastIndexOf(QLatin1Char('_'))),
           exteriorWall.mid(exteriorWall.lastIndexOf(QLatin1Char('_')) + 1).toInt());
    return ETypes[ETypes.count() - 1];
}

/////

FloorTypes *FloorTypes::instance = new FloorTypes;

/////

QList<BuildingDefinition*> BuildingDefinition::Definitions;
QMap<QString,BuildingDefinition*> BuildingDefinition::DefinitionMap;

/////

RoomDefinitionManager *RoomDefinitionManager::instance = new RoomDefinitionManager;


void RoomDefinitionManager::Init(BuildingDefinition *definition)
{
#if 1
    mBuildingDefinition = definition;
    ColorToRoom.clear();
    RoomToColor.clear();

    ExteriorWall = definition->Wall;

    foreach (Room *room, definition->RoomList) {
        ColorToRoom[room->Color] = room;
        RoomToColor[room] = room->Color;
    }
#else
    ColorToRoomName.clear();
    RoomNameToColor.clear();
    WallForRoom.clear();
    FloorForRoom.clear();
    Rooms.clear();

    ExteriorWall = definition->Wall;

    foreach (Room *room, definition->RoomList) {
        ColorToRoomName[room->Color] = room->Name;
        RoomNameToColor[room->Name] = room->Color;
        Rooms += room->Name;
        WallForRoom[room->Name] = room->Wall;
        FloorForRoom[room->Name] = room->Floor;
    }
#endif
}

QStringList RoomDefinitionManager::FillCombo()
{
    QStringList roomNames;
    foreach (Room *room, mBuildingDefinition->RoomList)
        roomNames += room->Name;
    return roomNames;
}

#if 0
QRgb RoomDefinitionManager::Get(QString roomName)
{
    return RoomNameToColor[roomName];
}
#endif

int RoomDefinitionManager::GetIndex(QRgb col)
{
    return mBuildingDefinition->RoomList.indexOf(ColorToRoom[col]);
}

int RoomDefinitionManager::GetIndex(Room *room)
{
    return mBuildingDefinition->RoomList.indexOf(room);
}

Room *RoomDefinitionManager::getRoom(int index)
{
    if (index < 0)
        return 0;
    return mBuildingDefinition->RoomList.at(index);
}

int RoomDefinitionManager::getRoomCount()
{
    return mBuildingDefinition->RoomList.count();
}

WallType *RoomDefinitionManager::getWallForRoom(int i)
{
    Room *room = getRoom(i);
    if (!WallTypes::instance->ITypesByName.contains(room->Wall)) {
        QString name = room->Wall;
        QString id = room->Wall;
        name = name.mid(0, name.lastIndexOf(QLatin1Char('_')));
        id = id.mid(name.lastIndexOf(QLatin1Char('_')) + 1);
        WallType *wall = new WallType(name, id.toInt());
        WallTypes::instance->ITypes += wall;
        WallTypes::instance->ITypesByName[room->Wall] = wall;
    }
    return WallTypes::instance->ITypesByName[room->Wall];
}

FloorType *RoomDefinitionManager::getFloorForRoom(int i)
{
    Room *room = getRoom(i);
    if (!FloorTypes::instance->TypesByName.contains(room->Floor)) {
        QString name = room->Floor;
        QString id = room->Floor;
        name = name.mid(0, name.lastIndexOf(QLatin1Char('_')));
        id = id.mid(name.lastIndexOf(QLatin1Char('_')) + 1);
        FloorType *wall = new FloorType(name, id.toInt());
        FloorTypes::instance->Types += wall;
        FloorTypes::instance->TypesByName[room->Floor] = wall;
    }
    return FloorTypes::instance->TypesByName[room->Floor];
}

/////

#if 0
Layout::Layout(QImage *bmp, QList<BaseMapObject *> objects) :
    w(bmp->width()),
    h(bmp->height()),
    Objects(objects)
{
    grid.resize(w);
    for (int x = 0; x < w; x++) {
        grid[x].resize(h);
        for (int y = 0; y < h; y++) {
            QRgb col = bmp->pixel(x, y);
            if (col == qRgb(0, 0, 0))
                grid[x][y] = -1;
            else
                grid[x][y] = RoomDefinitionManager::instance->GetIndex(col);
        }
    }

    int nRooms = RoomDefinitionManager::instance->getRoomCount();

    exteriorWall = WallTypes::instance->getOrAdd(RoomDefinitionManager::instance->ExteriorWall);
    for (int i = 0; i < nRooms; i++) {
        interiorWalls += RoomDefinitionManager::instance->getWallForRoom(i);
        floors += RoomDefinitionManager::instance->getFloorForRoom(i);
    }
    exteriorWall = WallTypes::instance->getEWallFromName(RoomDefinitionManager::instance->ExteriorWall);
}
#endif

Layout::Layout(int w, int h) :
    w(w),
    h(h)
{
    grid.resize(w);
    for (int x = 0; x < w; x++) {
        grid[x].resize(h);
        for (int y = 0; y < h; y++) {
            grid[x][y] = -1;
        }
    }

    exteriorWall = WallTypes::instance->getOrAdd(RoomDefinitionManager::instance->ExteriorWall);
    int nRooms = RoomDefinitionManager::instance->getRoomCount();
    for (int i = 0; i < nRooms; i++) {
        interiorWalls += RoomDefinitionManager::instance->getWallForRoom(i);
        floors += RoomDefinitionManager::instance->getFloorForRoom(i);
    }
}

Room *Layout::roomAt(int x, int y)
{
    int roomIndex = grid[x][y];
    return RoomDefinitionManager::instance->getRoom(roomIndex);
}

/////


/////

QRect Stairs::bounds() const
{
    if (dir == N)
        return QRect(X, Y, 1, 5);
    if (dir == W)
        return QRect(X, Y, 5, 1);
    return QRect();
}

QString Stairs::getStairsTexture(int x, int y)
{
    if (dir == N) {
        if (x == X / 30) {
            int index = y - (Y / 30);
            if (index == 1)
                return RoomDefinitionManager::instance->TopStairNorth;
            if (index == 2)
                return RoomDefinitionManager::instance->MidStairNorth;
            if (index == 3)
                return RoomDefinitionManager::instance->BotStairNorth;
        }
    }
    if (dir == W) {
        if (y == Y / 30) {
            int index = x - (X / 30);
            if (index == 1)
                return RoomDefinitionManager::instance->TopStairWest;
            if (index == 2)
                return RoomDefinitionManager::instance->MidStairWest;
            if (index == 3)
                return RoomDefinitionManager::instance->BotStairWest;
        }
    }
    return QString();
}

/////

