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
#include "buildingobjects.h"
#include "buildingpreferencesdialog.h"
#include "buildingpreviewwindow.h"
#include "buildingundoredo.h"
#include "buildingtemplates.h"
#include "buildingtemplatesdialog.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"
#include "buildingtools.h"
#include "FloorEditor.h"
#include "mixedtilesetview.h"
#include "newbuildingdialog.h"
#include "roomsdialog.h"
#include "simplefile.h"
#include "templatefrombuildingdialog.h"

#include "zprogress.h"

#include "tile.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"

#include <QBitmap>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QShowEvent>
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
    mRoomComboBox(new QComboBox()),
    mUndoGroup(new QUndoGroup(this)),
    mPreviewWin(0),
    mZoomable(new Zoomable(this)),
    mCategoryZoomable(new Zoomable(this))
{
    ui->setupUi(this);

    instance = this;

    ui->toolBar->insertSeparator(ui->actionUpLevel);
    ui->toolBar->insertWidget(ui->actionUpLevel, mRoomComboBox);
    ui->toolBar->insertSeparator(ui->actionUpLevel);

    mRoomComboBox->setIconSize(QSize(20, 20));
    mRoomComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    mFloorLabel = new QLabel;
    mFloorLabel->setText(tr("Ground Floor"));
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->toolBar->insertWidget(ui->actionUpLevel, mFloorLabel);

    mView = ui->floorView;
    mView->setScene(roomEditor);
    ui->coordLabel->clear();
    connect(mView, SIGNAL(mouseCoordinateChanged(QPoint)),
            SLOT(mouseCoordinateChanged(QPoint)));
    connect(mView->zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));
    mView->zoomable()->connectToComboBox(ui->editorScaleComboBox);

    connect(ui->actionPecil, SIGNAL(triggered()),
            PencilTool::instance(), SLOT(makeCurrent()));
    PencilTool::instance()->setEditor(roomEditor);
    PencilTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionEraser, SIGNAL(triggered()),
            EraserTool::instance(), SLOT(makeCurrent()));
    EraserTool::instance()->setEditor(roomEditor);
    EraserTool::instance()->setAction(ui->actionEraser);

    connect(ui->actionDoor, SIGNAL(triggered()),
            DoorTool::instance(), SLOT(makeCurrent()));
    DoorTool::instance()->setEditor(roomEditor);
    DoorTool::instance()->setAction(ui->actionDoor);

    connect(ui->actionWindow, SIGNAL(triggered()),
            WindowTool::instance(), SLOT(makeCurrent()));
    WindowTool::instance()->setEditor(roomEditor);
    WindowTool::instance()->setAction(ui->actionWindow);

    connect(ui->actionStairs, SIGNAL(triggered()),
            StairsTool::instance(), SLOT(makeCurrent()));
    StairsTool::instance()->setEditor(roomEditor);
    StairsTool::instance()->setAction(ui->actionStairs);

    connect(ui->actionSelectObject, SIGNAL(triggered()),
            SelectMoveObjectTool::instance(), SLOT(makeCurrent()));
    SelectMoveObjectTool::instance()->setEditor(roomEditor);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);

    connect(mRoomComboBox, SIGNAL(currentIndexChanged(int)),
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
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(0), undoAction);
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(1), redoAction);
    ui->menuEdit->insertSeparator(ui->menuEdit->actions().at(2));

    connect(ui->actionPreferences, SIGNAL(triggered()), SLOT(preferences()));

    connect(ui->actionNewBuilding, SIGNAL(triggered()), SLOT(newBuilding()));
    connect(ui->actionExportTMX, SIGNAL(triggered()), SLOT(exportTMX()));

    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));
    setWindowFlags(windowFlags() & ~Qt::WA_DeleteOnClose);

    connect(ui->actionZoomIn, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomOut()));
    connect(ui->actionNormalSize, SIGNAL(triggered()),
            mView->zoomable(), SLOT(resetZoom()));

    connect(ui->actionRooms, SIGNAL(triggered()), SLOT(roomsDialog()));
    connect(ui->actionTemplates, SIGNAL(triggered()), SLOT(templatesDialog()));
    connect(ui->actionTiles, SIGNAL(triggered()), SLOT(tilesDialog()));
    connect(ui->actionTemplateFromBuilding, SIGNAL(triggered()),
            SLOT(templateFromBuilding()));

    mCategoryZoomable->connectToComboBox(ui->scaleComboBox);
    connect(mCategoryZoomable, SIGNAL(scaleChanged(qreal)),
            SLOT(categoryScaleChanged(qreal)));

    readSettings();

    updateActions();
}

BuildingEditorWindow::~BuildingEditorWindow()
{
    BuildingTemplates::deleteInstance();
    BuildingTiles::deleteInstance(); // Ensure all the tilesets are released
    delete ui;
}

void BuildingEditorWindow::showEvent(QShowEvent *event)
{
    if (mPreviewWin)
        mPreviewWin->show();
    event->accept();
}

void BuildingEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmAllSave()) {
        writeSettings();
        if (mPreviewWin) {
            mPreviewWin->writeSettings();
            mPreviewWin->hide();
        }
        event->accept(); // doesn't destroy us
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
        return true;
    }
    return false;
}

bool BuildingEditorWindow::Startup()
{
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    if (!LoadMapBaseXMLLots()) {
        if (!mError.isEmpty())
            QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
        return false;
    }

    if (!LoadBuildingTiles()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading BuildingTiles.txt\n") + mError);
        return false;
    }

    if (!LoadBuildingTemplates()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading BuildingTemplates.txt\n") + mError);
        return false;
    }

    /////

    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        if (!validateTile(btemplate->Wall, "Wall") ||
                !validateTile(btemplate->DoorTile, "Door") ||
                !validateTile(btemplate->DoorFrameTile, "DoorFrame") ||
                !validateTile(btemplate->WindowTile, "Window") ||
                !validateTile(btemplate->StairsTile, "Stairs")) {
            mError += tr("\nIn template %1").arg(btemplate->Name);
            QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
            return false;
        }

        foreach (Room *room, btemplate->RoomList) {
            if (!validateTile(room->Floor, "Floor") ||
                    !validateTile(room->Wall, "Wall")) {
                mError += tr("\nIn template %1, room %2")
                        .arg(btemplate->Name)
                        .arg(room->Name);
                QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
                return false;
            }
        }
    }

    /////

    // Add tile categories to the gui
    setCategoryLists();

    /////

    mPreviewWin = new BuildingPreviewWindow(this);
    mPreviewWin->show();

    return true;
}

bool BuildingEditorWindow::LoadBuildingTemplates()
{
    QFileInfo info(QCoreApplication::applicationDirPath() + QLatin1Char('/')
                  + QLatin1String("BuildingTemplates.txt"));
    if (!info.exists()) {
        mError = tr("The BuildingTemplates.txt file doesn't exist.");
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }
    BuildingTiles *btiles = BuildingTiles::instance();
    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("Template")) {
            BuildingTemplate *def = new BuildingTemplate;
            def->Name = block.value("Name");
            def->Wall = btiles->getExteriorWall(block.value("Wall"));
            def->DoorTile = btiles->getDoorTile(block.value("Door"));
            def->DoorFrameTile = btiles->getDoorFrameTile(block.value("DoorFrame"));
            def->WindowTile = btiles->getWindowTile(block.value("Window"));
            def->StairsTile = btiles->getStairsTile(block.value("Stairs"));
            foreach (SimpleFileBlock roomBlock, block.blocks) {
                if (roomBlock.name == QLatin1String("Room")) {
                    Room *room = new Room;
                    room->Name = roomBlock.value("Name");
                    QStringList rgb = roomBlock.value("Color").split(QLatin1String(" "), QString::SkipEmptyParts);
                    room->Color = qRgb(rgb.at(0).toInt(),
                                       rgb.at(1).toInt(),
                                       rgb.at(2).toInt());
                    room->Wall = btiles->getInteriorWall(roomBlock.value("Wall"));
                    room->Floor = btiles->getFloorTile(roomBlock.value("Floor"));
                    room->internalName = roomBlock.value("InternalName");
                    def->RoomList += room;
                } else {
                    mError = tr("Unknown block name '%1': expected 'Room'.\n%2")
                            .arg(roomBlock.name)
                            .arg(path);
                    return false;
                }
            }
            BuildingTemplates::instance()->addTemplate(def);
        } else {
            mError = tr("Unknown block name '%1': expected 'Template'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

#if 0
    if (BuildingTemplates::instance()->templateCount() == 0) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("No buildings were defined in BuildingTemplates.txt."));
        return false;
    }
#endif

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

    static const char *validCategoryNamesC[] = {
        "exterior_walls", "interior_walls", "floors", "doors", "door_frames",
        "windows", "stairs", 0
    };
    QStringList validCategoryNames;
    for (int i = 0; validCategoryNamesC[i]; i++)
        validCategoryNames << QLatin1String(validCategoryNamesC[i]);

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            if (!validCategoryNames.contains(categoryName)) {
                mError = tr("Unknown category '%1' in BuildingTiles.txt.").arg(categoryName);
                return false;
            }

            QString label = block.value("label");
            BuildingTiles::instance()->addCategory(categoryName, label);
            SimpleFileBlock tilesBlock = block.block("tiles");
            foreach (SimpleFileKeyValue kv, tilesBlock.values) {
                if (kv.name == QLatin1String("tile")) {
                    QStringList values = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                    for (int i = 0; i < values.count(); i++)
                        values[i] = BuildingTiles::normalizeTileName(values[i]);
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
                    int count = BuildingTiles::instance()->tilesetFor(tilesetName)->tileCount(); // FIXME: tileset might not exist
                    if (countStr != QLatin1String("all"))
                        count = countStr.toInt();
                    for (int i = start; i < start + count; i += offset)
                        BuildingTiles::instance()->add(categoryName,
                                                       BuildingTiles::nameForTile(tilesetName, i));
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
            if (!BuildingTiles::instance()->tileFor(tile)) {
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
    // Make sure the user has chosen the Tiles directory.
    static const char *KEY_TILES_DIR = "BuildingEditor/TilesDirectory";
    QString tilesDirectory = mSettings.value(QLatin1String(KEY_TILES_DIR),
                                             QLatin1String("../Tiles")).toString();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        QMessageBox::information(this, tr("Choose Tiles Directory"),
                                 tr("Please enter the Tiles directory in the Preferences."));
        BuildingPreferencesDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            mError.clear();
            return false;
        }
        tilesDirectory = mSettings.value(QLatin1String(KEY_TILES_DIR)).toString();
    }

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

                QString source = tilesDirectory + QLatin1Char('/') + name
                        + QLatin1String(".png");
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

                BuildingTiles::instance()->addTileset(ts);
            }
            xml.skipCurrentElement();
        }
    } else {
        mError = tr("Not a map file.\n%1").arg(path);
        return false;
    }

    return true;
}

bool BuildingEditorWindow::validateTile(BuildingTile *btile, const char *key)
{
    if (!BuildingTiles::instance()->tileFor(btile)) {
        mError = tr("%1 tile '%2' doesn't exist.")
                .arg(QLatin1String(key))
                .arg(btile->name());
        return false;
    }
    return true;
}

void BuildingEditorWindow::setCurrentRoom(Room *room) const
{
    int roomIndex = mCurrentDocument->building()->indexOf(room);
    this->mRoomComboBox->setCurrentIndex(roomIndex);
}

Room *BuildingEditorWindow::currentRoom() const
{
    if (!currentBuilding()->roomCount())
        return 0;
    int roomIndex = mRoomComboBox->currentIndex();
    return mCurrentDocument->building()->room(roomIndex);
}

Building *BuildingEditorWindow::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
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
    mView->zoomable()->setScale(mSettings.value(QLatin1String("EditorScale"),
                                                1.0).toReal());
    mCategoryZoomable->setScale(mSettings.value(QLatin1String("CategoryScale"),
                                                0.5).toReal());
    mSettings.endGroup();
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.setValue(QLatin1String("EditorScale"), mView->zoomable()->scale());
    mSettings.setValue(QLatin1String("CategoryScale"), mCategoryZoomable->scale());
    mSettings.endGroup();
}

void BuildingEditorWindow::updateRoomComboBox()
{
    mRoomComboBox->clear();
    if (!mCurrentDocument)
        return;

    int index = 0;
    foreach (Room *room, currentBuilding()->rooms()) {
        QImage image(20, 20, QImage::Format_ARGB32);
        image.fill(qRgba(0,0,0,0));
        QPainter painter(&image);
        painter.fillRect(1, 1, 18, 18, room->Color);
        mRoomComboBox->addItem(room->Name);
        mRoomComboBox->setItemIcon(index, QPixmap::fromImage(image));
        index++;
    }
}

void BuildingEditorWindow::roomIndexChanged(int index)
{
}

void BuildingEditorWindow::categoryScaleChanged(qreal scale)
{
    mSettings.setValue(QLatin1String("BuildingEditor/MainWindow/CategoryScale"),
                       scale);
}

void BuildingEditorWindow::currentEWallChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("exterior_walls"), tile);
        mCurrentDocument->undoStack()->push(new ChangeEWall(mCurrentDocument,
                                                            btile));
    }
}

void BuildingEditorWindow::currentIWallChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("interior_walls"), tile);
        mCurrentDocument->undoStack()->push(new ChangeWallForRoom(mCurrentDocument,
                                                                  currentRoom(),
                                                                  btile));
    }
}

void BuildingEditorWindow::currentFloorChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("floors"), tile);
        mCurrentDocument->undoStack()->push(new ChangeFloorForRoom(mCurrentDocument,
                                                                   currentRoom(),
                                                                   btile));
    }
}

void BuildingEditorWindow::currentDoorChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("doors"), tile);
        currentBuilding()->setDoorTile(btile);
        // Assign the new tile to selected doors
        QList<Door*> doors;
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Door *door = dynamic_cast<Door*>(object)) {
                if (door->mTile != btile)
                    doors += door;
            }
        }
        if (doors.count()) {
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Tile"));
            foreach (Door *door, doors)
                mCurrentDocument->undoStack()->push(new ChangeDoorTile(mCurrentDocument,
                                                                       door,
                                                                       btile));
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentDoorFrameChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("door_frames"), tile);
        currentBuilding()->setDoorFrameTile(btile);
        // Assign the new tile to selected doors
        QList<Door*> doors;
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Door *door = dynamic_cast<Door*>(object)) {
                if (door->mFrameTile != btile)
                    doors += door;
            }
        }
        if (doors.count()) {
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
            foreach (Door *door, doors)
                mCurrentDocument->undoStack()->push(new ChangeDoorTile(mCurrentDocument,
                                                                       door,
                                                                       btile,
                                                                       true));
            if (doors.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentWindowChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        // New windows will be created with this tile
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("windows"), tile);
        currentBuilding()->setWindowTile(btile);
        // Assign the new tile to selected windows
        QList<Window*> windows;
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Window *window = dynamic_cast<Window*>(object)) {
                if (window->mTile != btile)
                    windows += window;
            }
        }
        if (windows.count()) {
            if (windows.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
            foreach (Window *window, windows)
                mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                       window,
                                                                       btile));
            if (windows.count() > 1)
                mCurrentDocument->undoStack()->endMacro();
        }
    }
}

void BuildingEditorWindow::currentStairsChanged(const QItemSelection &selected)
{
    if (!mCurrentDocument)
        return;
    QModelIndexList indexes = selected.indexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
        Tile *tile = m->tileAt(index);
        if (!tile)
            return;
        // New stairs will be created with this tile
        BuildingTile *btile = BuildingTiles::instance()->fromTiledTile(
                    QLatin1String("stairs"), tile);
        currentBuilding()->setStairsTile(btile);
        // Assign the new tile to selected stairs
        QList<Stairs*> stairsList;
        foreach (BaseMapObject *object, mCurrentDocument->selectedObjects()) {
            if (Stairs *stairs = dynamic_cast<Stairs*>(object)) {
                if (stairs->mTile != btile)
                    stairsList += stairs;
            }
        }
        if (stairsList.count()) {
            if (stairsList.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Stairs Tile"));
            foreach (Stairs *stairs, stairsList)
                mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                       stairs,
                                                                       btile));
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
    mCurrentDocument->setSelectedObjects(QSet<BaseMapObject*>());
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
    updateActions();
}

void BuildingEditorWindow::downLevel()
{
    int level = mCurrentDocument->currentFloor()->level() - 1;
    if (level < 0)
        return;
    mCurrentDocument->setSelectedObjects(QSet<BaseMapObject*>());
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
    updateActions();
}

void BuildingEditorWindow::newBuilding()
{
    NewBuildingDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (mCurrentDocument) {
        roomEditor->clearDocument();
        mPreviewWin->clearDocument();
        mUndoGroup->removeStack(mCurrentDocument->undoStack());
        delete mCurrentDocument->building();
        delete mCurrentDocument;
    }

#if 0
    RoomDefinitionManager::instance->Init(dialog.buildingTemplate());
#endif

    Building *building = new Building(dialog.buildingWidth(),
                                      dialog.buildingHeight(),
                                      dialog.buildingTemplate());
    building->insertFloor(0, new BuildingFloor(building, 0));

    mCurrentDocument = new BuildingDocument(building, QString());
    mCurrentDocument->setCurrentFloor(building->floor(0));
    mUndoGroup->addStack(mCurrentDocument->undoStack());
    mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

    roomEditor->setDocument(mCurrentDocument);

    updateRoomComboBox();

    mFloorLabel->setText(tr("Ground Floor"));

    resizeCoordsLabel();

    connect(mCurrentDocument, SIGNAL(roomAdded(Room*)), SLOT(roomAdded(Room*)));
    connect(mCurrentDocument, SIGNAL(roomRemoved(Room*)), SLOT(roomRemoved(Room*)));
    connect(mCurrentDocument, SIGNAL(roomsReordered()), SLOT(roomsReordered()));
    connect(mCurrentDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));

    /////

    mPreviewWin->setDocument(currentDocument());

    updateActions();

    if (building->roomCount())
        PencilTool::instance()->makeCurrent();
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

void BuildingEditorWindow::preferences()
{
    BuildingPreferencesDialog dialog(this);
    dialog.exec();
}

void BuildingEditorWindow::roomsDialog()
{
    if (!mCurrentDocument)
        return;
    QList<Room*> originalRoomList = mCurrentDocument->building()->rooms();
    RoomsDialog dialog(originalRoomList, this);
    dialog.setWindowTitle(tr("Rooms in building"));

    if (dialog.exec() != QDialog::Accepted)
        return;

    mCurrentDocument->undoStack()->beginMacro(tr("Edit Rooms"));

    QList<Room*> deletedRooms = originalRoomList;
    // Rooms may have been added, moved, deleted, and/or edited.
    foreach (Room *dialogRoom, dialog.rooms()) {
        if (Room *room = dialog.originalRoom(dialogRoom)) {
            int oldIndex = mCurrentDocument->building()->rooms().indexOf(room);
            int newIndex = dialog.rooms().indexOf(dialogRoom);
            if (oldIndex != newIndex) {
                qDebug() << "move room" << room->Name << "from " << oldIndex << " to " << newIndex;
                mCurrentDocument->undoStack()->push(new ReorderRoom(mCurrentDocument,
                                                                    newIndex,
                                                                    room));
            }
            if (*room != *dialogRoom) {
                qDebug() << "change room" << room->Name;
                mCurrentDocument->undoStack()->push(new ChangeRoom(mCurrentDocument,
                                                                   room,
                                                                   dialogRoom));
            }
            deletedRooms.removeOne(room);
        } else {
            // This is a new room.
            qDebug() << "add room" << dialogRoom->Name << "at" << dialog.rooms().indexOf(dialogRoom);
            Room *newRoom = new Room(dialogRoom);
            int index = dialog.rooms().indexOf(dialogRoom);
            mCurrentDocument->undoStack()->push(new AddRoom(mCurrentDocument,
                                                            index,
                                                            newRoom));
        }
    }
    // now handle deleted rooms
    foreach (Room *room, deletedRooms) {
        qDebug() << "delete room" << room->Name;
        int index = mCurrentDocument->building()->rooms().indexOf(room);
        foreach (BuildingFloor *floor, currentBuilding()->floors()) {
            bool changed = false;
            QVector<QVector<Room*> > grid = floor->grid();
            for (int x = 0; x < grid.size(); x++) {
                for (int y = 0; y < grid[x].size(); y++) {
                    if (grid[x][y] == room) {
                        grid[x][y] = 0;
                        changed = true;
                    }
                }
            }
            if (changed) {
                mCurrentDocument->undoStack()->push(new SwapFloorGrid(mCurrentDocument,
                                                                      floor,
                                                                      grid));
            }
        }
        mCurrentDocument->undoStack()->push(new RemoveRoom(mCurrentDocument,
                                                           index));
    }

    mCurrentDocument->undoStack()->endMacro();
}

void BuildingEditorWindow::roomAdded(Room *room)
{
    updateRoomComboBox();
    updateActions();
}

void BuildingEditorWindow::roomRemoved(Room *room)
{
    updateRoomComboBox();
    updateActions();
}

void BuildingEditorWindow::roomsReordered()
{
    updateRoomComboBox();
}

void BuildingEditorWindow::roomChanged(Room *room)
{
    updateRoomComboBox();
}

void BuildingEditorWindow::templatesDialog()
{
    BuildingTemplatesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    BuildingTemplates::instance()->replaceTemplates(dialog.templates());
    BuildingTemplates::instance()->writeBuildingTemplatesTxt();
}

void BuildingEditorWindow::tilesDialog()
{
    BuildingTilesDialog dialog(this);
    dialog.exec();

    BuildingTiles::instance()->writeBuildingTilesTxt();

    setCategoryLists();
}

void BuildingEditorWindow::templateFromBuilding()
{
    if (!mCurrentDocument)
        return;

    TemplateFromBuildingDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Building *building = mCurrentDocument->building();
    BuildingTemplate *btemplate = new BuildingTemplate;
    btemplate->Name = dialog.name();
    btemplate->Wall = building->exteriorWall();
    btemplate->DoorTile = building->doorTile();
    btemplate->DoorFrameTile = building->doorFrameTile();
    btemplate->WindowTile = building->windowTile();
    btemplate->StairsTile = building->stairsTile();

    foreach (Room *room, building->rooms())
        btemplate->RoomList += new Room(room);

    BuildingTemplates::instance()->addTemplate(btemplate);

    BuildingTemplates::instance()->writeBuildingTemplatesTxt();

    templatesDialog();
}

void BuildingEditorWindow::mouseCoordinateChanged(const QPoint &tilePos)
{
    ui->coordLabel->setText(tr("%1,%2").arg(tilePos.x()).arg(tilePos.y()));
}

void BuildingEditorWindow::resizeCoordsLabel()
{
    int width = 999, height = 999;
    if (mCurrentDocument) {
        width = qMax(width, mCurrentDocument->building()->width());
        height = qMax(height, mCurrentDocument->building()->height());
    }
    QFontMetrics fm = ui->coordLabel->fontMetrics();
    QString coordString = QString(QLatin1String("%1,%2")).arg(width).arg(height);
    ui->coordLabel->setMinimumWidth(fm.width(coordString) + 8);
}

void BuildingEditorWindow::setCategoryLists()
{
    QToolBox *toolBox = ui->toolBox;
    while (toolBox->count()) {
        QWidget *w = toolBox->widget(0);
        toolBox->removeItem(0);
        delete w;
    }

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
            qFatal("bogus category name"); // the names were validated elsewhere
        }

        Tiled::Internal::MixedTilesetView *w
                = new Tiled::Internal::MixedTilesetView(mCategoryZoomable, toolBox);
        toolBox->addItem(w, category->label());
        connect(w->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                slot);

        QList<Tiled::Tile*> tiles;
        foreach (BuildingTile *tile, category->tiles()) {
            if (!tile->mAlternates.count() || (tile == tile->mAlternates.first()))
                tiles += BuildingTiles::instance()->tileFor(tile);
        }
        w->model()->setTiles(tiles);
    }
}

void BuildingEditorWindow::updateActions()
{
    PencilTool::instance()->setEnabled(mCurrentDocument != 0 &&
            currentRoom() != 0);
    EraserTool::instance()->setEnabled(mCurrentDocument != 0);
    DoorTool::instance()->setEnabled(mCurrentDocument != 0);
    WindowTool::instance()->setEnabled(mCurrentDocument != 0);
    StairsTool::instance()->setEnabled(mCurrentDocument != 0);
    SelectMoveObjectTool::instance()->setEnabled(mCurrentDocument != 0);

    ui->actionUpLevel->setEnabled(mCurrentDocument != 0);
    ui->actionDownLevel->setEnabled(mCurrentDocument != 0 &&
            mCurrentDocument->currentFloor()->level() > 0);

    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(false);
    ui->actionSaveAs->setEnabled(false);
    ui->actionExportTMX->setEnabled(mCurrentDocument != 0);

    ui->actionZoomIn->setEnabled(mView->zoomable()->canZoomIn());
    ui->actionZoomOut->setEnabled(mView->zoomable()->canZoomOut());
    ui->actionNormalSize->setEnabled(mView->zoomable()->scale() != 1.0);

    ui->actionRooms->setEnabled(mCurrentDocument != 0);
    ui->actionTemplateFromBuilding->setEnabled(mCurrentDocument != 0);

    mRoomComboBox->setEnabled(mCurrentDocument != 0 &&
            currentRoom() != 0);

    if (mCurrentDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentFloor()->level() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    else
        mFloorLabel->clear();
}
