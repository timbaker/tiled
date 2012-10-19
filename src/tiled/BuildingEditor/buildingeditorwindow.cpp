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

#include "zprogress.h"

#include "tile.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "utils.h"

#include <QBitmap>
#include <QCloseEvent>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
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
    mUndoGroup(new QUndoGroup(this)),
    mPreviewWin(0)
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

    connect(ui->actionWindow, SIGNAL(triggered()),
            WindowTool::instance(), SLOT(activate()));
    WindowTool::instance()->setEditor(roomEditor);
    WindowTool::instance()->setAction(ui->actionWindow);

    connect(ui->actionStairs, SIGNAL(triggered()),
            StairsTool::instance(), SLOT(activate()));
    StairsTool::instance()->setEditor(roomEditor);
    StairsTool::instance()->setAction(ui->actionStairs);

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

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setShortcuts(QKeySequence::Redo);
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);
    Utils::setThemeIcon(undoAction, "edit-undo");
    Utils::setThemeIcon(redoAction, "edit-redo");
    ui->menuEdit->insertAction(0, redoAction);
    ui->menuEdit->insertAction(redoAction, undoAction);

    connect(ui->actionExportTMX, SIGNAL(triggered()), SLOT(exportTMX()));

    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));
    setWindowFlags(windowFlags() & ~Qt::WA_DeleteOnClose);

    readSettings();
}

BuildingEditorWindow::~BuildingEditorWindow()
{
    delete ui;
}

void BuildingEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmAllSave()) {
        writeSettings();
        event->accept(); // doesn't destroy us, hides PreviewWindow for us
    } else
        event->ignore();

}

bool BuildingEditorWindow::confirmAllSave()
{
    return true;
}

// Called by Tiled::Internal::MainWindow::closeEvent
bool BuildingEditorWindow::closeYerself()
{
    if (confirmAllSave()) {
        writeSettings();
        if (mPreviewWin)
            mPreviewWin->writeSettings();
        delete this; // Delete ourself and PreviewWindow.
                     // Gotta delete PreviewWindow before Tiled's MainWindow
                     // so all the tilesets are released.
        return true;
    }
    return false;
}

bool BuildingEditorWindow::Startup()
{
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!LoadBuildingTemplates())
        return false;

    if (!LoadMapBaseXMLLots()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
        return false;
    }

    if (!LoadBuildingTiles()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading BuildingTiles.txt\n") + mError);
        return false;
    }

    QList<Tiled::Tile*> tiles;

    /////

    QToolBox *toolBox = ui->toolBox;
    while (toolBox->count())
        toolBox->removeItem(0);

    foreach (BuildingTiles::Category *category, BuildingTiles::instance()->categories()) {
        QString categoryName = category->name();
        const char *slot = 0;
        if (categoryName == QLatin1String("exterior_walls"))
            slot = SLOT(currentEWallChanged(QItemSelection));
        else if (categoryName == QLatin1String("interior_walls"))
            slot = SLOT(currentIWallChanged(QItemSelection));
        else if (categoryName == QLatin1String("floors"))
            slot = SLOT(currentFloorChanged(QItemSelection));
        else if (categoryName == QLatin1String("doors"))
            slot = SLOT(currentDoorChanged(QItemSelection));
        else if (categoryName == QLatin1String("door_frames"))
            slot = SLOT(currentDoorFrameChanged(QItemSelection));
        else if (categoryName == QLatin1String("windows"))
            slot = SLOT(currentWindowChanged(QItemSelection));
        else if (categoryName == QLatin1String("stairs"))
            slot = SLOT(currentStairsChanged(QItemSelection));
        else {
            mError = tr("Unknown category '%1' in BuildingTiles.txt.").arg(categoryName);
            QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
            return false;
        }

        Tiled::Internal::MixedTilesetView *w = new Tiled::Internal::MixedTilesetView(toolBox);
        toolBox->addItem(w, category->label());
        connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                slot);

        tiles.clear();
        foreach (BuildingTile *tile, category->tiles()) {
            if (!tile->mAlternates.count() || (tile == tile->mAlternates.first()))
                tiles += mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex);
        }
        w->model()->setTiles(tiles);
    }

    /////

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

    /////

    mPreviewWin = new BuildingPreviewWindow(this);
    mPreviewWin->scene()->setTilesets(mTilesetByName);
    mPreviewWin->setDocument(currentDocument());
    mPreviewWin->show();

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
        mError = tr("The BuildingTiles.txt file doesn't exist.");
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            QString label = block.value("label");
            BuildingTiles::instance()->addCategory(categoryName, label);
            SimpleFileBlock tilesBlock = block.block("tiles");
            foreach (SimpleFileKeyValue kv, tilesBlock.values) {
                if (kv.name == QLatin1String("tile")) {
                    QStringList values = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                    BuildingTiles::instance()->add(categoryName, values);
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
            foreach (SimpleFileBlock rangeBlock, block.blocks) {
                if (rangeBlock.name == QLatin1String("range")) {
                    QString tilesetName = rangeBlock.value("tileset");
                    int start = rangeBlock.value("start").toInt();
                    int offset = rangeBlock.value("offset").toInt();
                    QString countStr = rangeBlock.value("count");
                    int count = mTilesetByName[tilesetName]->tileCount();
                    if (countStr != QLatin1String("all"))
                        count = countStr.toInt();
                    for (int i = start; i < start + count; i += offset)
                        BuildingTiles::instance()->add(categoryName,
                                                       tilesetName + QLatin1Char('_') + QString::number(i));
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    // Check that all the tiles exist
    foreach (BuildingTiles::Category *category, BuildingTiles::instance()->categories()) {
        foreach (BuildingTile *tile, category->tiles()) {
            if (!tileFor(tile)) {
                mError = tr("Tile %1 #%2 doesn't exist.")
                        .arg(tile->mTilesetName)
                        .arg(tile->mIndex);
                return false;
            }
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

    PROGRESS progress(tr("Reading MapBaseXMLLots.txt tilesets"), this);

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
    if (!mTilesetByName.contains(tilesetName))
        return 0;
    if (index >= mTilesetByName[tilesetName]->tileCount())
        return 0;
    return mTilesetByName[tilesetName]->tileAt(index);
}

Tile *BuildingEditorWindow::tileFor(BuildingTile *tile)
{
    if (!mTilesetByName.contains(tile->mTilesetName))
        return 0;
    if (tile->mIndex >= mTilesetByName[tile->mTilesetName]->tileCount())
        return 0;
    return mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex);
}

QString BuildingEditorWindow::nameForTile(Tile *tile)
{
    return tile->tileset()->name() + QLatin1String("_") + QString::number(tile->id());
}

void BuildingEditorWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QByteArray geom = mSettings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(800, 600);
    restoreState(mSettings.value(QLatin1String("state"),
                                 QByteArray()).toByteArray());
    mSettings.endGroup();
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.endGroup();
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

void BuildingEditorWindow::currentDoorChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        RoomDefinitionManager::instance->mDoorTile = nameForTile(tile);
        // Assign the new tile to selected doors
        QList<Door*> doors;
        QString tileName = nameForTile(tile);
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Door *door = dynamic_cast<Door*>(object)) {
                if (door->mTile != BuildingTiles::instance()->tileForDoor(door, tileName))
                    doors += door;
            }
        }
        if (doors.count()) {
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Tile"));
            foreach (Door *door, doors)
                mCurrentDocument->undoStack()->push(new ChangeDoorTile(mCurrentDocument,
                                                                       door,
                                                                       BuildingTiles::instance()->tileForDoor(door, tileName)));
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentDoorFrameChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        RoomDefinitionManager::instance->mDoorFrameTile = nameForTile(tile);
        // Assign the new tile to selected doors
        QList<Door*> doors;
        QString tileName = nameForTile(tile);
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Door *door = dynamic_cast<Door*>(object)) {
                if (door->mFrameTile != BuildingTiles::instance()->tileForDoor(door, tileName, true))
                    doors += door;
            }
        }
        if (doors.count()) {
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
            foreach (Door *door, doors)
                mCurrentDocument->undoStack()->push(new ChangeDoorTile(mCurrentDocument,
                                                                       door,
                                                                       BuildingTiles::instance()->tileForDoor(door, tileName, true),
                                                                       true));
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentWindowChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        // New windows will be created with this tile
        RoomDefinitionManager::instance->mWindowTile = nameForTile(tile);
        // Assign the new tile to selected windows
        QList<Window*> windows;
        QString tileName = nameForTile(tile);
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Window *window = dynamic_cast<Window*>(object)) {
                if (window->mTile != BuildingTiles::instance()->tileForWindow(window, tileName))
                    windows += window;
            }
        }
        if (windows.count()) {
            if (windows.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
            foreach (Window *window, windows)
                mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                       window,
                                                                       BuildingTiles::instance()->tileForWindow(window, tileName)));
            if (windows.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentStairsChanged(const QItemSelection &selected)
{
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        // New stairs will be created with this tile
        RoomDefinitionManager::instance->mStairsTile = nameForTile(tile);
        // Assign the new tile to selected stairs
        QList<Stairs*> stairsList;
        QString tileName = nameForTile(tile);
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Stairs *stairs = dynamic_cast<Stairs*>(object)) {
                if (stairs->mTile != BuildingTiles::instance()->tileForStairs(stairs, tileName))
                    stairsList += stairs;
            }
        }
        if (stairsList.count()) {
            if (stairsList.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Stairs Tile"));
            foreach (Stairs *stairs, stairsList)
                mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                       stairs,
                                                                       BuildingTiles::instance()->tileForStairs(stairs, tileName)));
            if (stairsList.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
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

void BuildingEditorWindow::exportTMX()
{
    QString initialDir = mSettings.value(
                QLatin1String("BuildingEditor/ExportDirectory")).toString();

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), initialDir,
                                         tr("Tiled map files (*.tmx)"));
    if (fileName.isEmpty())
        return;

    mPreviewWin->exportTMX(fileName);

    mSettings.setValue(QLatin1String("BuildingEditor/ExportDirectory"),
                       QFileInfo(fileName).absolutePath());
}

/////

QList<BuildingDefinition*> BuildingDefinition::Definitions;
QMap<QString,BuildingDefinition*> BuildingDefinition::DefinitionMap;

/////

RoomDefinitionManager *RoomDefinitionManager::instance = new RoomDefinitionManager;


void RoomDefinitionManager::Init(BuildingDefinition *definition)
{
    mBuildingDefinition = definition;
    ColorToRoom.clear();
    RoomToColor.clear();

    ExteriorWall = definition->Wall;
    mDoorTile = QLatin1String("fixtures_doors_01_0");
    mDoorFrameTile = QLatin1String("fixtures_doors_frames_01_0");
    mWindowTile = QLatin1String("fixtures_windows_01_0");
    mStairsTile = QLatin1String("fixtures_stairs_01_0");

    foreach (Room *room, definition->RoomList) {
        ColorToRoom[room->Color] = room;
        RoomToColor[room] = room->Color;
    }
}

QStringList RoomDefinitionManager::FillCombo()
{
    QStringList roomNames;
    foreach (Room *room, mBuildingDefinition->RoomList)
        roomNames += room->Name;
    return roomNames;
}

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

BuildingTile *RoomDefinitionManager::getWallForRoom(int i)
{
    Room *room = getRoom(i);
    return BuildingTiles::instance()->get(QLatin1String("interior_walls"), room->Wall);
}

BuildingTile *RoomDefinitionManager::getFloorForRoom(int i)
{
    Room *room = getRoom(i);
    return BuildingTiles::instance()->get(QLatin1String("floors"), room->Floor);
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
    mDir(dir),
    mTile(0)
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

int Stairs::getStairsOffset(int x, int y)
{
    if (mDir == N) {
        if (x == mX) {
            int index = y - mY;
            if (index == 1)
                return 2; //RoomDefinitionManager::instance->TopStairNorth;
            if (index == 2)
                return 1; //RoomDefinitionManager::instance->MidStairNorth;
            if (index == 3)
                return 0; //RoomDefinitionManager::instance->BotStairNorth;
        }
    }
    if (mDir == W) {
        if (y == mY) {
            int index = x - mX;
            if (index == 1)
                return 2; //RoomDefinitionManager::instance->TopStairWest;
            if (index == 2)
                return 1; //RoomDefinitionManager::instance->MidStairWest;
            if (index == 3)
                return 0; //RoomDefinitionManager::instance->BotStairWest;
        }
    }
    return -1;
}

/////

BuildingTiles *BuildingTiles::mInstance = 0;

BuildingTiles *BuildingTiles::instance()
{
    if (!mInstance)
        mInstance = new BuildingTiles;
    return mInstance;
}

BuildingTile *BuildingTiles::get(const QString &categoryName, const QString &tileName)
{
    Category *category = this->category(categoryName);
    return category->get(tileName);
}

bool BuildingTiles::parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
    index = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1).toInt();
    return true;
}

QString BuildingTiles::adjustTileNameIndex(const QString &tileName, int offset)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    index += offset;
    return tilesetName + QLatin1Char('_') + QString::number(index);
}

BuildingTile *BuildingTiles::tileForDoor(Door *door, const QString &tileName,
                                         bool isFrame)
{
    QString adjustedName = tileName;
    if (door->dir() == BaseMapObject::N)
        adjustedName = adjustTileNameIndex(tileName, 1);
    return get(QLatin1String(isFrame ? "door_frames" : "doors"), adjustedName);
}

BuildingTile *BuildingTiles::tileForWindow(Window *window, const QString &tileName)
{
    QString adjustedName = tileName;
    if (window->dir() == BaseMapObject::N)
        adjustedName = adjustTileNameIndex(tileName, 1);
    return get(QLatin1String("windows"), adjustedName);
}

BuildingTile *BuildingTiles::tileForStairs(Stairs *stairs, const QString &tileName)
{
    return get(QLatin1String("stairs"), tileName);
}

/////

