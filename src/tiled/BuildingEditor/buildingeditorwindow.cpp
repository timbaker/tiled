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
#include "buildingpreviewwindow.h"
#include "buildingundoredo.h"
#include "buildingtools.h"
#include "FloorEditor.h"
#include "mixedtilesetview.h"
#include "simplefile.h"

#include "tile.h"
#include "tileset.h"
#include "tilesetmanager.h"

#include <QComboBox>
#include <QDir>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QUndoGroup>
#include <QXmlStreamReader>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

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

    mFloorLabel = new QLabel;
    mFloorLabel->setText(tr("Ground Floor"));
    mFloorLabel->setMinimumWidth(90);
    ui->toolBar->insertWidget(ui->actionUpLevel, mFloorLabel);

    QGraphicsView *view = new QGraphicsView(this);
    view->setScene(roomEditor);
    view->setMouseTracking(true);
    view->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    setCentralWidget(view);

    connect(ui->actionPecil, SIGNAL(triggered()),
            PencilTool::instance(), SLOT(activate()));
    PencilTool::instance()->setEditor(roomEditor);
    PencilTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionEraser, SIGNAL(triggered()),
            EraserTool::instance(), SLOT(activate()));
    EraserTool::instance()->setEditor(roomEditor);
    EraserTool::instance()->setAction(ui->actionEraser);

    connect(ui->actionDoor, SIGNAL(triggered()),
            DoorTool::instance(), SLOT(activate()));
    DoorTool::instance()->setEditor(roomEditor);
    DoorTool::instance()->setAction(ui->actionDoor);

    connect(ui->actionSelectObject, SIGNAL(triggered()),
            SelectMoveObjectTool::instance(), SLOT(activate()));
    SelectMoveObjectTool::instance()->setEditor(roomEditor);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);

    connect(room, SIGNAL(currentIndexChanged(int)),
            SLOT(roomIndexChanged(int)));

    connect(ui->actionUpLevel, SIGNAL(triggered()),
            SLOT(upLevel()));
    connect(ui->actionDownLevel, SIGNAL(triggered()),
            SLOT(downLevel()));

    /////

    QToolBox *toolBox = ui->toolBox;
    while (toolBox->count())
        toolBox->removeItem(0);

    Tiled::Internal::MixedTilesetView *w = new Tiled::Internal::MixedTilesetView(toolBox);
    toolBox->addItem(w, tr("External Walls"));
    connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(currentEWallChanged(QItemSelection)));

    w = new Tiled::Internal::MixedTilesetView(toolBox);
    toolBox->addItem(w, tr("Internal Walls"));
    connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(currentIWallChanged(QItemSelection)));

    w = new Tiled::Internal::MixedTilesetView(toolBox);
    toolBox->addItem(w, tr("Floors"));
    connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(currentFloorChanged(QItemSelection)));

    /////

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setShortcuts(QKeySequence::Redo);
    ui->menuEdit->insertAction(0, redoAction);
    ui->menuEdit->insertAction(0, undoAction);

    /////

}

BuildingEditorWindow::~BuildingEditorWindow()
{
    delete ui;
}

bool BuildingEditorWindow::Startup()
{
    if (!LoadBuildingTemplates())
        return false;

    if (!LoadBuildingTiles())
        return false;

    if (!LoadMapBaseXMLLots()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
        return false;
    }

    Tiled::Internal::MixedTilesetView *v;
    v = static_cast<Tiled::Internal::MixedTilesetView*>(ui->toolBox->widget(0));
    QList<Tiled::Tile*> tiles;
    foreach (WallType *t, WallTypes::instance->ETypes)
        tiles += tileFor(t->ToString());
    v->model()->setTiles(tiles);

    v = static_cast<Tiled::Internal::MixedTilesetView*>(ui->toolBox->widget(1));
    tiles.clear();
    foreach (WallType *t, WallTypes::instance->ITypes)
        tiles += tileFor(t->ToString());
    v->model()->setTiles(tiles);

    v = static_cast<Tiled::Internal::MixedTilesetView*>(ui->toolBox->widget(2));
    tiles.clear();
    foreach (FloorType *t, FloorTypes::instance->Types)
        tiles += tileFor(t->ToString());
    v->model()->setTiles(tiles);

    RoomDefinitionManager::instance->Init(BuildingDefinition::Definitions.first());

    Building *building = new Building(20, 20);
    building->insertFloor(0, new BuildingFloor(building, 0));

    mCurrentDocument = new BuildingDocument(building, QString());
    mCurrentDocument->setCurrentFloor(building->floor(0));
    mUndoGroup->addStack(mCurrentDocument->undoStack());
    mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

    roomEditor->setDocument(mCurrentDocument);

    ///// NewBuildingDialog.exec()
    this->room->addItems(RoomDefinitionManager::instance->FillCombo());

    this->room->setIconSize(QSize(20, 20));
    for (int i = 0; i < RoomDefinitionManager::instance->getRoomCount(); i++) {
        QImage image(20, 20, QImage::Format_ARGB32);
        image.fill(qRgba(0,0,0,0));
        QPainter painter(&image);
        painter.fillRect(1, 1, 18, 18, RoomDefinitionManager::instance->getRoom(i)->Color);
        this->room->setItemIcon(i, QPixmap::fromImage(image));
    }

    roomEditor->UpdateMetaBuilding();

    /////

    BuildingPreviewWindow *previewWin = new BuildingPreviewWindow(this);
    previewWin->scene()->setTilesets(mTilesetByName);
    previewWin->setDocument(currentDocument());
    previewWin->show();

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

bool BuildingEditorWindow::LoadBuildingTiles()
{
    QFileInfo info(QCoreApplication::applicationDirPath() + QLatin1Char('/')
                  + QLatin1String("BuildingTiles.txt"));
    if (!info.exists()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("The BuildingTiles.txt file doesn't exist."));
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error reading %1.").arg(path));
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            SimpleFileBlock tilesBlock = block.block("tiles");
            foreach (SimpleFileKeyValue kv, tilesBlock.values) {
                if (kv.name == QLatin1String("tile")) {
                    QString tileName = kv.value;
                    QString tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
                    int index = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1).toInt();
                    if (categoryName == QLatin1String("exterior_walls")) {
                        WallTypes::instance->AddExt(tilesetName, index);
                    }
                    if (categoryName == QLatin1String("interior_walls")) {
                        WallTypes::instance->Add(tilesetName, index);
                    }
                    if (categoryName == QLatin1String("floors")) {
                        FloorTypes::instance->Add(tilesetName, index);
                    }
                    if (categoryName == QLatin1String("doors")) {
                        DoorTypes::instance->Add(tilesetName, index);
                    }
                } else {
                    QMessageBox::critical(this, tr("It's no good, Jim!"),
                                          tr("Unknown value name '%1'.\n%2")
                                          .arg(kv.name)
                                          .arg(path));
                    return false;
                }
            }
        } else {
            QMessageBox::critical(this, tr("It's no good, Jim!"),
                                  tr("Unknown block name '%1'.\n%2")
                                  .arg(block.name)
                                  .arg(path));
            return false;
        }
    }

    return true;
}

bool BuildingEditorWindow::LoadMapBaseXMLLots()
{
    QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("MapBaseXMLLots.txt");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Couldn't open %1").arg(path);
        return false;
    }

    QXmlStreamReader xml;
    xml.setDevice(&file);

    if (xml.readNextStartElement() && xml.name() == "map") {
        while (xml.readNextStartElement()) {
            if (xml.name() == "tileset") {
                const QXmlStreamAttributes atts = xml.attributes();
//                const int firstGID = atts.value(QLatin1String("firstgid")).toString().toInt();
                const QString name = atts.value(QLatin1String("name")).toString();

                QString source = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                        + QLatin1String("../../ProjectZomboid/BMPToMap/BuildingEditor/Tiles/") + name + QLatin1String(".png");
                if (!QFileInfo(source).exists()) {
                    mError = tr("Tileset in MapBaseXMLLots.txt doesn't exist.\n%1").arg(source);
                    return false;
                }
                source = QFileInfo(source).canonicalFilePath();

                Tileset *ts = new Tileset(name, 64, 128);

                TilesetImageCache *cache = TilesetManager::instance()->imageCache();
                Tileset *cached = cache->findMatch(ts, source);
                if (!cached || !ts->loadFromCache(cached)) {
                    const QImage tilesetImage = QImage(source);
                    if (ts->loadFromImage(tilesetImage, source))
                        cache->addTileset(ts);
                    else {
                        mError = tr("Error loading tileset image:\n'%1'").arg(source);
                        return false;
                    }
                }

                mTilesetByName[name] = ts;
            }
            xml.skipCurrentElement();
        }
    } else {
        mError = tr("Not a map file.\n%1").arg(path);
        return false;
    }

    return true;
}

void BuildingEditorWindow::setCurrentRoom(Room *room) const
{
    int roomIndex = RoomDefinitionManager::instance->GetIndex(room);
    this->room->setCurrentIndex(roomIndex);
}

Room *BuildingEditorWindow::currentRoom() const
{
    int roomIndex = room->currentIndex();
    return RoomDefinitionManager::instance->getRoom(roomIndex);
}

Tiled::Tile *BuildingEditorWindow::tileFor(const QString &tileName)
{
    QString tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
    int index = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1).toInt();
    return mTilesetByName[tilesetName]->tileAt(index);
}

QString BuildingEditorWindow::nameForTile(Tile *tile)
{
    return tile->tileset()->name() + QLatin1String("_") + QString::number(tile->id());
}

void BuildingEditorWindow::roomIndexChanged(int index)
{
}

void BuildingEditorWindow::currentEWallChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        mCurrentDocument->undoStack()->push(new ChangeEWall(mCurrentDocument,
                                                            nameForTile(tile)));
    }
}

void BuildingEditorWindow::currentIWallChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        mCurrentDocument->undoStack()->push(new ChangeWallForRoom(mCurrentDocument,
                                                                  currentRoom(),
                                                                  nameForTile(tile)));
    }
}

void BuildingEditorWindow::currentFloorChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        mCurrentDocument->undoStack()->push(new ChangeFloorForRoom(mCurrentDocument,
                                                                   currentRoom(),
                                                                   nameForTile(tile)));
    }
}

void BuildingEditorWindow::upLevel()
{
    int level = mCurrentDocument->currentFloor()->level() + 1;
    if (level == mCurrentDocument->building()->floorCount()) {
        BuildingFloor *floor = new BuildingFloor(mCurrentDocument->building(), level);
        mCurrentDocument->insertFloor(level, floor); // TODO: make undoable
    }
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
    mFloorLabel->setText(tr("Floor %1").arg(level));
}

void BuildingEditorWindow::downLevel()
{
    int level = mCurrentDocument->currentFloor()->level() - 1;
    if (level < 0)
        return;
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
    if (level > 0)
        mFloorLabel->setText(tr("Floor %1").arg(level));
    else
        mFloorLabel->setText(tr("Ground Floor"));
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


void FloorTypes::Add(QString name, int first)
{
    FloorType *t = new FloorType(name, first);
    Types += t;
    TypesByName[name] = t;
}

/////

DoorTypes *DoorTypes::instance = new DoorTypes;


void DoorTypes::Add(QString name, int first)
{
    DoorType *t = new DoorType(name, first);
    Types += t;
    TypesByName[name] = t;
}

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
        id = id.mid(id.lastIndexOf(QLatin1Char('_')) + 1);
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
        id = id.mid(id.lastIndexOf(QLatin1Char('_')) + 1);
        FloorType *floor = new FloorType(name, id.toInt());
        FloorTypes::instance->Types += floor;
        FloorTypes::instance->TypesByName[room->Floor] = floor;
    }
    return FloorTypes::instance->TypesByName[room->Floor];
}

void RoomDefinitionManager::setWallForRoom(Room *room, QString tile)
{
    room->Wall = tile;
}

void RoomDefinitionManager::setFloorForRoom(Room *room, QString tile)
{
    room->Floor = tile;
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

BaseMapObject::BaseMapObject(BuildingFloor *floor, int x, int y, Direction dir) :
    mFloor(floor),
    mX(x),
    mY(y),
    mDir(dir)
{
}

int BaseMapObject::index()
{
    return mFloor->indexOf(this);
}

/////

QRect Stairs::bounds() const
{
    if (mDir == N)
        return QRect(mX, mY, 1, 5);
    if (mDir == W)
        return QRect(mX, mY, 5, 1);
    return QRect();
}

QString Stairs::getStairsTexture(int x, int y)
{
    if (mDir == N) {
        if (x == mX / 30) {
            int index = y - (mY / 30);
            if (index == 1)
                return RoomDefinitionManager::instance->TopStairNorth;
            if (index == 2)
                return RoomDefinitionManager::instance->MidStairNorth;
            if (index == 3)
                return RoomDefinitionManager::instance->BotStairNorth;
        }
    }
    if (mDir == W) {
        if (y == mY / 30) {
            int index = x - (mX / 30);
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

