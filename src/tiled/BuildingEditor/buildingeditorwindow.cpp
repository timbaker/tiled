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

#include "simplefile.h"

#include <QComboBox>
#include <QDir>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QMessageBox>

using namespace BuildingEditor;

GraphicsFloorItem::GraphicsFloorItem(EditorFloor *floor) :
    QGraphicsItem(),
    mFloor(floor)
{
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return QRectF(0, 0, mFloor->width * 30, mFloor->height * 30);
}

void GraphicsFloorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                              QWidget *)
{
    for (int x = 0; x < mFloor->width; x++) {
        for (int y = 0; y < mFloor->height; y++) {
            QRgb c = mFloor->bmp->pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue;
            painter->fillRect(x * 30, y * 30, 30, 30, c);
        }
    }
}

/////

GraphicsGridItem::GraphicsGridItem(int width, int height) :
    QGraphicsItem(),
    mWidth(width),
    mHeight(height)
{
}

QRectF GraphicsGridItem::boundingRect() const
{
    return QRectF(0, 0, mWidth * 30, mHeight * 30);
}

void GraphicsGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                             QWidget *)
{
    QPen pen(QColor(128, 128, 220, 80));
    pen.setWidth(2);
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);

    for (int x = 0; x < mWidth; x++)
        painter->drawLine(x * 30, 0, x * 30, mHeight * 30);

    for (int y = 0; y < mHeight; y++)
        painter->drawLine(0, y * 30, mWidth * 30, y * 30);
}

/////

BuildingEditorWindow* BuildingEditorWindow::instance = 0;

BuildingEditorWindow::BuildingEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingEditorWindow),
    roomEditor(new FloorEditor(this)),
    room(new QComboBox())
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

    setCentralWidget(view);

    connect(ui->actionPecil, SIGNAL(triggered()),
            PencilTool::instance(), SLOT(activate()));
    PencilTool::instance()->setEditor(roomEditor);
    PencilTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionEraser, SIGNAL(triggered()),
            EraserTool::instance(), SLOT(activate()));
    EraserTool::instance()->setEditor(roomEditor);
    EraserTool::instance()->setAction(ui->actionEraser);
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

    BuildingDefinition::Room *room = new BuildingDefinition::Room;
    room->Name = QLatin1String("Living Room");
    room->Color = qRgb(255, 0, 0);
    room->internalName = QLatin1String("livingroom");
    room->Wall = QLatin1String("walls_interior_house_01_20");
    room->Floor = QLatin1String("floors_interior_tilesandwood_01_42");
    def->RoomList += room;

    BuildingDefinition::Definitions += def;
    BuildingDefinition::DefinitionMap[def->Name] = def;
#endif

    ///// NewBuildingDialog.exec()
    RoomDefinitionManager::instance->Init(BuildingDefinition::Definitions.first());
    roomEditor->currentFloor = new EditorFloor(20, 20);
    roomEditor->mCurrentFloorItem = new GraphicsFloorItem(roomEditor->currentFloor);
    this->room->addItems(RoomDefinitionManager::instance->FillCombo());
    roomEditor->floors += roomEditor->currentFloor;
    roomEditor->UpdateMetaBuilding();

    roomEditor->addItem(roomEditor->mCurrentFloorItem);
    roomEditor->addItem(new GraphicsGridItem(roomEditor->currentFloor->width,
                                             roomEditor->currentFloor->height));
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
                BuildingDefinition::Room *room = new BuildingDefinition::Room;
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

QString BuildingEditorWindow::currentRoom() const
{
    return room->currentText();
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

WallTypes::WallType *WallTypes::getEWallFromName(QString exteriorWall)
{
    foreach (WallType *wallType, ETypes) {
        if (wallType->ToString() == exteriorWall)
            return wallType;
    }
    return 0;
}

WallTypes::WallType *WallTypes::getOrAdd(QString exteriorWall)
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
    ColorToRoomName.clear();
    RoomNameToColor.clear();
    WallForRoom.clear();
    FloorForRoom.clear();
    Rooms.clear();
    ExteriorWall = definition->Wall;

    foreach (BuildingDefinition::Room *room, definition->RoomList) {
        ColorToRoomName[room->Color] = room->Name;
        RoomNameToColor[room->Name] = room->Color;
        Rooms += room->Name;
        WallForRoom[room->Name] = room->Wall;
        FloorForRoom[room->Name] = room->Floor;
    }
}

QStringList RoomDefinitionManager::FillCombo()
{
    return Rooms;
}

QRgb RoomDefinitionManager::Get(QString roomName)
{
    return RoomNameToColor[roomName];
}

int RoomDefinitionManager::GetIndex(QRgb col)
{
    return Rooms.indexOf(ColorToRoomName[col]);
}

int RoomDefinitionManager::getRoomCount()
{
    return Rooms.count();
}

WallTypes::WallType *RoomDefinitionManager::getWallForRoom(int i)
{
    if (!WallTypes::instance->ITypesByName.contains(WallForRoom[Rooms[i]])) {
        QString name = WallForRoom[Rooms[i]];
        QString id = WallForRoom[Rooms[i]];
        name = name.mid(0, name.lastIndexOf(QLatin1Char('_')));
        id = id.mid(name.lastIndexOf(QLatin1Char('_')) + 1);
        WallTypes::WallType *wall = new WallTypes::WallType(name, id.toInt());
        WallTypes::instance->ITypes += wall;
        WallTypes::instance->ITypesByName[WallForRoom[Rooms[i]]] = wall;
    }
    return WallTypes::instance->ITypesByName[WallForRoom[Rooms[i]]];
}

FloorTypes::FloorType *RoomDefinitionManager::getFloorForRoom(int i)
{
    if (!FloorTypes::instance->TypesByName.contains(FloorForRoom[Rooms[i]])) {
        QString name = FloorForRoom[Rooms[i]];
        QString id = FloorForRoom[Rooms[i]];
        name = name.mid(0, name.lastIndexOf(QLatin1Char('_')));
        id = id.mid(name.lastIndexOf(QLatin1Char('_')) + 1);
        FloorTypes::FloorType *wall = new FloorTypes::FloorType(name, id.toInt());
        FloorTypes::instance->Types += wall;
        FloorTypes::instance->TypesByName[FloorForRoom[Rooms[i]]] = wall;
    }
    return FloorTypes::instance->TypesByName[FloorForRoom[Rooms[i]]];
}

/////

FloorEditor::FloorEditor(QWidget *parent) :
    QGraphicsScene(parent),
    mCurrentTool(0)
{
    setBackgroundBrush(Qt::black);
}

void FloorEditor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void FloorEditor::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void FloorEditor::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void FloorEditor::UpdateMetaBuilding()
{
    currentFloor->UpdateLayout();
    currentFloor->layout->exteriorWall = WallTypes::instance->getEWallFromName(
                RoomDefinitionManager::instance->ExteriorWall);
    QVector<Layout*> l;

    foreach (EditorFloor *fl, floors) {
        fl->layout->exteriorWall = WallTypes::instance->getEWallFromName(
                    RoomDefinitionManager::instance->ExteriorWall);
        l += fl->layout;
        fl->UpdateLayout();
    }

    building = new MetaBuilding(l, currentFloor->layout->exteriorWall);

#if 0
    if (Form1.instance.preview == null)
        return;

    Form1.instance.preview.panel1.building = building;
    Form1.instance.preview.panel1.Invalidate();
    Invalidate();
#endif
}

void FloorEditor::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
    }

    mCurrentTool = tool;

    if (mCurrentTool)
        mCurrentTool->action()->setChecked(true);
}

QPoint FloorEditor::sceneToTile(const QPointF &scenePos)
{
    return QPoint(scenePos.x() / 30, scenePos.y() / 30);
}

bool FloorEditor::currentFloorContains(const QPoint &tilePos)
{
    int x = tilePos.x(), y = tilePos.y();
    if (x < 0 || y < 0 || x >= currentFloor->width || y >= currentFloor->height)
        return false;
    return true;
}

/////

EditorFloor::EditorFloor(int w, int h) :
    width(w),
    height(h),
    layout(0)
{
    bmp = new QImage(w, h, QImage::Format_RGB32);
    bmp->fill(Qt::black);
}

void EditorFloor::UpdateLayout()
{
//    delete layout;
    layout = new Layout(bmp, Objects);
}

/////

MetaBuilding::MetaBuilding(QVector<Layout *> floors, WallTypes::WallType *ext)
{
    this->floors = floors;

    foreach (Layout *layout, floors)
        buildingFloors += new BuildingFloor(layout);
}

/////

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

Door *Layout::GetDoorAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = dynamic_cast<Door*>(o))
            return door;
    }
    return 0;
}

Window *Layout::GetWindowAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = dynamic_cast<Window*>(o))
            return window;
    }
    return 0;
}

Stairs *Layout::GetStairsAt(int x, int y)
{
    foreach (BaseMapObject *o, Objects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = dynamic_cast<Stairs*>(o))
            return stairs;
    }
    return 0;
}

/////

BuildingFloor::BuildingFloor(Layout *l)
{
    if (!l)
        return;

    layout = l;
    w = l->w + 1;
    h = l->h + 1;
    // +1 for the outside walls;
    squares.resize(w);
    for (int x = 0; x < w; x++)
        squares[x].resize(h);

    WallTypes::WallType *wtype = 0;

    exteriorWall = l->exteriorWall;
    interiorWalls = l->interiorWalls;
    floors = l->floors;

    // first put back walls in...

    // N first...

    for (int x = 0; x < l->w; x++) {
        for (int y = 0; y < l->h + 1; y++) {
            if (y == l->h && l->grid[x][y - 1] > 0)
                // put N wall here...
                squares[x][y].walls += new WallTile(WallTile::N, exteriorWall);
            else if (y < l->h && l->grid[x][y] < 0 && y > 0 && l->grid[x][y-1] != l->grid[x][y])
                squares[x][y].walls += new WallTile(WallTile::N, exteriorWall);
            else if (y < l->h && (y == 0 || l->grid[x][y-1] != l->grid[x][y]) && l->grid[x][y] >= 0)
                squares[x][y].walls += new WallTile(WallTile::N, interiorWalls[l->grid[x][y]]);
        }
    }

    for (int x = 0; x < l->w+1; x++)
    {
        for (int y = 0; y < l->h; y++)
        {
            if (x == l->w && l->grid[x - 1][y] >= 0)
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                    // put W wall here...
                    squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }
            else if (x < l->w && l->grid[x][y] < 0 && x > 0 && l->grid[x - 1][y] != l->grid[x][y])
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                    // put W wall here...
                    squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }
            else if (x < l->w && l->grid[x][y] >= 0 && (x == 0 || l->grid[x - 1][y] != l->grid[x][y]))
            {
                wtype = interiorWalls[l->grid[x][y]];
                // If already contains a north, put in a west...
                if(squares[x][y].Contains(WallTile::N))
                    squares[x][y].Replace(new WallTile(WallTile::NW, wtype), WallTile::N);
                else
                // put W wall here...
                squares[x][y].walls += new WallTile(WallTile::W, wtype);
            }

        }
    }

    for (int x = 0; x < l->w + 1; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            if (x > 0 && y > 0)
            {
                if (squares[x][y].walls.count() > 0)
                    continue;
                if (x < l->w && l->grid[x][y - 1] >= 0)
                    wtype = interiorWalls[l->grid[x][y - 1]];
                else if (y < l->h &&  l->grid[x-1][y] >= 0)
                    wtype = interiorWalls[l->grid[x - 1][y]];
                else
                    wtype = exteriorWall;
                // Put in the SE piece...
                if ((squares[x][y - 1].Contains(WallTile::W) || squares[x][y - 1].Contains(WallTile::NW)) && (squares[x - 1][y].Contains(WallTile::N) || squares[x - 1][y].Contains(WallTile::NW)))
                    squares[x][y].walls += new WallTile(WallTile::SE, wtype);
            }

        }
    }

    for (int x = 0; x < l->w; x++)
    {
        for (int y = 0; y < l->h + 1; y++)
        {
            Door *door = l->GetDoorAt(x, y);
            if (door != 0)
            {
                if (door->dir == BaseMapObject::N)
                    squares[x][y].ReplaceWithDoor(WallTile::N);
                if (door->dir == BaseMapObject::W)
                    squares[x][y].ReplaceWithDoor(WallTile::W);
            }

            Window *window = l->GetWindowAt(x, y);
            if (window != 0)
            {
                if (window->dir == BaseMapObject::N)
                    squares[x][y].ReplaceWithWindow(WallTile::N);
                if (window->dir == BaseMapObject::W)
                    squares[x][y].ReplaceWithWindow(WallTile::W);
            }

            Stairs *stairs = l->GetStairsAt(x, y);
            if (stairs != 0)
            {
                squares[x][y].stairsTexture = stairs->getStairsTexture(x, y);
            }
        }
    }

    for (int x = 0; x < l->w; x++) {
        for (int y = 0; y < l->h; y++) {
            if (l->grid[x][y] >= 0)
                squares[x][y].floorTile = new FloorTile(floors[l->grid[x][y]]);
        }
    }
}

/////

bool BuildingFloor::Square::Contains(BuildingFloor::WallTile::WallSection sec)
{
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == sec)
            return true;
    }
    return false;
}

void BuildingFloor::Square::Replace(BuildingFloor::WallTile *tile,
                                    BuildingFloor::WallTile::WallSection secToReplace)
{
    int i = 0;
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == secToReplace) {
            walls.replace(i, tile);
            return;
        }
        i++;
    }
}

void BuildingFloor::Square::ReplaceWithDoor(BuildingFloor::WallTile::WallSection direction)
{
    Q_UNUSED(direction)
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == WallTile::N) {
            wallTile->Section = WallTile::NDoor;
            return;
        }
        if (wallTile->Section == WallTile::W) {
            wallTile->Section = WallTile::WDoor;
            return;
        }
    }
}

void BuildingFloor::Square::ReplaceWithWindow(BuildingFloor::WallTile::WallSection direction)
{
    Q_UNUSED(direction)
    foreach (WallTile *wallTile, walls) {
        if (wallTile->Section == WallTile::N) {
            wallTile->Section = WallTile::NWindow;
            return;
        }
        if (wallTile->Section == WallTile::W) {
            wallTile->Section = WallTile::WWindow;
            return;
        }
    }
}

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

PencilTool *PencilTool::mInstance = 0;

PencilTool *PencilTool::instance()
{
    if (!mInstance)
        mInstance = new PencilTool();
    return mInstance;
}

PencilTool::PencilTool()
    : BaseTool()
{
}

void PencilTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QImage *bmp = mEditor->currentFloor->getBMP();

    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos)) {
        QString roomName = BuildingEditorWindow::instance->currentRoom();
        bmp->setPixel(tilePos, RoomDefinitionManager::instance->Get(roomName));
        mEditor->currentFloor->UpdateLayout();
        mEditor->mCurrentFloorItem->update();
    }
    mMouseDown = true;
}

void PencilTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mousePressEvent(event);
    }
}

void PencilTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mEditor->UpdateMetaBuilding();
        mMouseDown = false;
    }
}

void PencilTool::activate()
{
    mEditor->activateTool(this);
}

void PencilTool::deactivate()
{
}

/////

EraserTool *EraserTool::mInstance = 0;

EraserTool *EraserTool::instance()
{
    if (!mInstance)
        mInstance = new EraserTool();
    return mInstance;
}

EraserTool::EraserTool()
    : BaseTool()
{
}

void EraserTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QImage *bmp = mEditor->currentFloor->getBMP();

    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos)) {
        bmp->setPixel(tilePos, qRgb(0, 0, 0));
        mEditor->currentFloor->UpdateLayout();
        mEditor->mCurrentFloorItem->update();
    }
    mMouseDown = true;
}

void EraserTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mousePressEvent(event);
    }
}

void EraserTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mMouseDown) {
        mEditor->UpdateMetaBuilding();
        mMouseDown = false;
    }
}

void EraserTool::activate()
{
    mEditor->activateTool(this);
}

void EraserTool::deactivate()
{
}

/////
