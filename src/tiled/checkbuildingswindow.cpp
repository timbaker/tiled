#include "checkbuildingswindow.h"
#include "ui_checkbuildingswindow.h"

#include "mainwindow.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "rearrangetiles.h"
#include "tilesetmanager.h"
#include "tilemetainfomgr.h"
#include "zprogress.h"

#include "BuildingEditor/building.h"
#include "BuildingEditor/buildingeditorwindow.h"
#include "BuildingEditor/buildingfloor.h"
#include "BuildingEditor/buildingmap.h"
#include "BuildingEditor/buildingobjects.h"
#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/buildingpreferences.h"
#include "BuildingEditor/buildingreader.h"
#include "BuildingEditor/buildingtemplates.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QDir>
#include <QFileDialog>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

CheckBuildingsWindow::CheckBuildingsWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::CheckBuildingsWindow),
    mFileSystemWatcher(new FileSystemWatcher(this))
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    connect(ui->dirBrowse, SIGNAL(clicked()), SLOT(browse()));
    connect(ui->checkNow, SIGNAL(clicked()), SLOT(check()));
    connect(ui->treeWidget, SIGNAL(itemActivated(QTreeWidgetItem*,int)), SLOT(itemActivated(QTreeWidgetItem*,int)));

    connect(ui->checkInteriorOutside, SIGNAL(clicked()), SLOT(syncList()));
    connect(ui->checkSwitches, SIGNAL(clicked()), SLOT(syncList()));
    connect(ui->checkRoomLight, SIGNAL(clicked()), SLOT(syncList()));
    connect(ui->checkGrime, SIGNAL(clicked()), SLOT(syncList()));
    connect(ui->checkSink, SIGNAL(clicked()), SLOT(syncList()));
    connect(ui->check2x, SIGNAL(clicked()), SLOT(syncList()));

    ui->dirEdit->setText(BuildingPreferences::instance()->mapsDirectory());
//    ui->dirEdit->setText(QLatin1Literal("C:/Users/Tim/Desktop/ProjectZomboid/Buildings"));

    ui->checkSwitches->setChecked(true);
    ui->checkInteriorOutside->setChecked(false);
    ui->checkRoomLight->setChecked(false);

    ui->treeWidget->setColumnCount(1);

    connect(mFileSystemWatcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, SIGNAL(timeout()), SLOT(fileChangedTimeout()));
}

CheckBuildingsWindow::~CheckBuildingsWindow()
{
    delete ui;
}

void CheckBuildingsWindow::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, QString(),
                                                  ui->dirEdit->text());
    if (!f.isEmpty()) {
        ui->dirEdit->setText(QDir::toNativeSeparators(f));
    }
}

void CheckBuildingsWindow::check()
{
    QDir dir(ui->dirEdit->text());

    PROGRESS progress(tr("Checking"), this);
    ui->treeWidget->clear();
    qDeleteAll(mFiles);
    mFiles.clear();

    foreach (QString path, mWatchedFiles)
        mFileSystemWatcher->removePath(path);
    mWatchedFiles.clear();

    QStringList filters;
    filters << QLatin1String("*.tbx");
    dir.setNameFilters(filters);
    dir.setFilter(QDir::Files | QDir::Readable | QDir::Writable);

    foreach (QString fileName, dir.entryList()) {
        progress.update(tr("Checking %1").arg(fileName));
        QString filePath = dir.filePath(fileName);
        check(filePath);
        mFileSystemWatcher->addPath(filePath);
        mWatchedFiles += filePath;
    }
}

void CheckBuildingsWindow::itemActivated(QTreeWidgetItem *item, int column)
{
    if (item->parent() == 0)
        return;
    Issue &issue = mFiles[ui->treeWidget->indexOfTopLevelItem(item->parent())]->issues[item->parent()->indexOfChild(item)];
    MainWindow::instance()->openFile(issue.file->path);
    BuildingEditorWindow::instance()->focusOn(issue.file->path, issue.x, issue.y, issue.z, issue.objectIndex);
}

void CheckBuildingsWindow::syncList()
{
    foreach (IssueFile *file, mFiles)
        syncList(file);
}

void CheckBuildingsWindow::syncList(IssueFile *file)
{
    int rowMin = 0, rowMax = mFiles.size() - 1;
    if (file != 0)
        rowMin = rowMax = mFiles.indexOf(file);
    for (int row = rowMin; row <= rowMax; row++) {
        QTreeWidgetItem *fileItem = ui->treeWidget->topLevelItem(row);
        bool anyVisible = false;
        for (int i = 0; i < fileItem->childCount(); i++) {
            Issue &issue = mFiles[row]->issues[i];
            bool visible = true;
            if (issue.type == Issue::LightSwitch && !ui->checkSwitches->isChecked())
                visible = false;
            if (issue.type == Issue::InteriorOutside && !ui->checkInteriorOutside->isChecked())
                visible = false;
            if (issue.type == Issue::RoomLight && !ui->checkRoomLight->isChecked())
                visible = false;
            if (issue.type == Issue::Grime && !ui->checkGrime->isChecked())
                visible = false;
            if (issue.type == Issue::Sinks && !ui->checkSink->isChecked())
                visible = false;
            if (issue.type == Issue::Rearranged && !ui->check2x->isChecked())
                visible = false;
            QTreeWidgetItem *issueItem = fileItem->child(i);
            issueItem->setHidden(!visible);
            if (visible) anyVisible = true;
        }
        fileItem->setHidden(!anyVisible);
    }
}

void CheckBuildingsWindow::check(const QString &filePath)
{
    RearrangeTiles::instance()->readTxtIfNeeded();

    BuildingReader reader;
    if (Building *building = reader.read(filePath)) {
        reader.fix(building);
        BuildingMap::loadNeededTilesets(building);
        BuildingMap bmap(building);
        Map *map = bmap.mergedMap();
        bmap.addRoomDefObjects(map);
        QSet<Tileset*> usedTilesets = map->usedTilesets();
        usedTilesets.remove(TilesetManager::instance()->missingTileset());
        TileMetaInfoMgr::instance()->loadTilesets(usedTilesets.toList());
//            TilesetManager::instance()->removeReferences(map->tilesets());
        check(&bmap, building, map, filePath);
        TilesetManager::instance()->removeReferences(map->tilesets());
        delete map;
        delete building;
    }
}

void CheckBuildingsWindow::check(BuildingMap *bmap, Building *building, Map *map, const QString &fileName)
{
    const int NORTH_SWITCH = 0;
    const int WEST_SWITCH = 1;
    const int EAST_SWITCH = 2;
    const int SOUTH_SWITCH = 3;

//    qDebug() << "checking " << fileName;
    mCurrentIssueFile = 0;
    foreach (IssueFile *file, mFiles) {
        if (file->path == fileName) {
            mCurrentIssueFile = file;
            file->issues.clear();
            break;
        }
    }
    if (mCurrentIssueFile == 0) {
        mCurrentIssueFile = new IssueFile(fileName);
        mFiles += mCurrentIssueFile;
    }

    bool interiorFloor = false;
    MapInfo *mapInfo = MapManager::instance()->newFromMap(map);
    MapComposite mc(mapInfo);
    foreach (BuildingFloor *floor, building->floors()) {
        int z = floor->level();
        QSet<Room*> roomWithSwitch;
        QSet<Room*> roomWithSink;
        foreach (BuildingObject *bo, floor->objects()) {
            int x = bo->x(), y = bo->y();
            if (FurnitureObject *fo = bo->asFurniture()) {
                foreach (BuildingTile *btile, fo->buildingTiles()) {
                    if (btile->mTilesetName == QLatin1Literal("fixtures_sinks_01")) {
                        if (Room *room = floor->GetRoomAt(x, y))
                            roomWithSink |= room;
                    }
                    if (btile->mTilesetName == QLatin1Literal("lighting_indoor_01")) {
                        BuildingFloor::Square &square = floor->squares[x][y];
                        if (btile->mIndex == NORTH_SWITCH || btile->mIndex == NORTH_SWITCH + 4) {
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientN) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue(Issue::LightSwitch, "North Switch not on a Wall", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::North)
                                issue(Issue::LightSwitch, "North Switch on a Door", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::North)
                                issue(Issue::LightSwitch, "North Switch on a Window", bo);
                        }
                        if (btile->mIndex == WEST_SWITCH || btile->mIndex == WEST_SWITCH + 4) {
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientW) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue(Issue::LightSwitch, "West Switch not on a Wall", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::West)
                                issue(Issue::LightSwitch, "West Switch on a Door", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::West)
                                issue(Issue::LightSwitch, "West Switch on a Window", bo);
                        }
                        if (btile->mIndex == EAST_SWITCH || btile->mIndex == EAST_SWITCH + 4) {
                            BuildingFloor::Square &square = floor->squares[x+1][y];
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientW) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue(Issue::LightSwitch, "East Switch not on a Wall", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::West)
                                issue(Issue::LightSwitch, "East Switch on a Door", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::West)
                                issue(Issue::LightSwitch, "East Switch on a Window", bo);
                        }
                        if (btile->mIndex == SOUTH_SWITCH || btile->mIndex == SOUTH_SWITCH + 4) {
                            BuildingFloor::Square &square = floor->squares[x][y+1];
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientN) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue(Issue::LightSwitch, "South Switch not on a Wall", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::North)
                                issue(Issue::LightSwitch, "South Switch on a Door", bo);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::North)
                                issue(Issue::LightSwitch, "South Switch on a Window", bo);
                        }
                        if (btile->mIndex == NORTH_SWITCH || btile->mIndex == WEST_SWITCH || btile->mIndex == EAST_SWITCH || btile->mIndex == SOUTH_SWITCH ||
                                btile->mIndex == NORTH_SWITCH + 4 || btile->mIndex == WEST_SWITCH + 4 || btile->mIndex == EAST_SWITCH + 4 || btile->mIndex == SOUTH_SWITCH + 4) {
                            if (Room *room = floor->GetRoomAt(x, y))
                                roomWithSwitch |= room;
                        }
                        break;
                    }
                }
            }
        }

        QMap<Room*,QPoint> roomPos;
        QMap<Room*,int> roomSize;

        CompositeLayerGroup *layers = mc.tileLayersForLevel(floor->level());
        for (int y = 0; y < floor->height(); y++) {
            for (int x = 0; x < floor->width(); x++) {
                BuildingFloor::Square &square = floor->squares[x][y];
                foreach (TileLayer *layer, layers->layers()) {
                    Tile *tile = layer->cellAt(x, y).tile;
                    /*
                    if (tile != 0 && tile->tileset()->name() == QLatin1Literal("lighting_indoor_01")) {
                        if (tile->id() == NORTH_SWITCH) {
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientN) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue("NORTH SWITCH NOT ON A WALL", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::North)
                                issue("NORTH SWITCH ON A DOOR", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::North)
                                issue("NORTH SWITCH ON A WINDOW", x, y, z);
                        }
                        if (tile->id() == WEST_SWITCH) {
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientW) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue("WEST SWITCH NOT ON A WALL", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::West)
                                issue("WEST SWITCH ON A DOOR", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::West)
                                issue("WEST SWITCH ON A WINDOW", x, y, z);
                        }
                        if (tile->id() == EAST_SWITCH) {
                            BuildingFloor::Square &square = floor->squares[x+1][y];
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientW) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue("EAST SWITCH NOT ON A WALL", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::West)
                                issue("EAST SWITCH ON A DOOR", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::West)
                                issue("EAST SWITCH ON A WINDOW", x, y, z);
                        }
                        if (tile->id() == SOUTH_SWITCH) {
                            BuildingFloor::Square &square = floor->squares[x][y+1];
                            if (!square.IsWallOrient(BuildingFloor::Square::WallOrientN) && !square.IsWallOrient(BuildingFloor::Square::WallOrientNW))
                                issue("SOUTH SWITCH NOT ON A WALL", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionDoor] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionDoor] == BTC_Doors::North)
                                issue("SOUTH SWITCH ON A DOOR", x, y, z);
                            if (square.mEntries[BuildingFloor::Square::SectionWindow] != 0 && square.mEntryEnum[BuildingFloor::Square::SectionWindow] == BTC_Windows::North)
                                issue("SOUTH SWITCH ON A WINDOW", x, y, z);
                        }
                    }
                */
                    if (tile != 0 && tile->tileset()->name().startsWith(QLatin1Literal("floors_interior_"))) {
                        if (!interiorFloor && floor->GetRoomAt(x, y) == 0) {
                            issue(Issue::InteriorOutside, "Interior floor tile outside building", x, y, z);
                            interiorFloor = true;
                        }
                    }
                    if (layer->name().endsWith(QLatin1Literal("_Floor"))) {
                        if (tile != 0 && tile->tileset()->name().startsWith(QLatin1Literal("overlay_grime_"))) {
                            issue(Issue::Grime, "Grime in the floor layer", x, y, z);
                        }
                    }
                    if (tile != 0 && tile->tileset()->name() == QLatin1Literal("vegetation_foliage_01")) {
                        bool foundBlendsNatural = false;
                        foreach (TileLayer *tl2, layers->layers()) {
                            if (tl2 == layer)
                                break;
                            if (!tl2->cellAt(x, y).isEmpty() && tl2->cellAt(x, y).tile->tileset()->name().startsWith(QLatin1Literal(""))) {
                                foundBlendsNatural = true;
                                break;
                            }
                        }
                        if (!foundBlendsNatural) {
                            issue(Issue::Rearranged, tr("vegetation_foliage tile must be on blends_natural for erosion to work"), x, y, z);
                        }
                    }
                    if (tile != 0 && RearrangeTiles::instance()->isRearranged(tile)) {
                        issue(Issue::Rearranged, tr("Rearranged tile (%1)").arg(BuildingTilesMgr::instance()->nameForTile(tile)), x, y, z);
                    }
                }
                if (Room *room = floor->GetRoomAt(x, y)) {
                    if (!roomPos.contains(room))
                        roomPos[room] = QPoint(x, y);
                    roomSize[room]++;
                }
            }
        }

        foreach (Room *room, roomSize.keys()) {
            if (room->Name != QLatin1Literal("empty") && roomSize[room] > 4 && !roomWithSwitch.contains(room))
                issue(Issue::RoomLight, QString::fromLatin1("Room without Light Switch (%1)").arg(room->Name), roomPos[room].x(), roomPos[room].y(), z);
            if (!roomWithSink.contains(room) && (room->Name.toLower() == QLatin1Literal("kitchen") || room->Name.toLower() == QLatin1Literal("bathroom")))
                issue(Issue::Sinks, QString::fromLatin1("Room without Sink (%1)").arg(room->Name), roomPos[room].x(), roomPos[room].y(), z);
        }

    }
    delete mapInfo;

    updateList(mCurrentIssueFile);
    syncList(mCurrentIssueFile);
}

void CheckBuildingsWindow::issue(Issue::Type type, const QString &detail, int x, int y, int z)
{
    mCurrentIssueFile->issues += Issue(mCurrentIssueFile, type, detail, x, y, z);
}

void CheckBuildingsWindow::issue(Issue::Type type, const char *detail, int x, int y, int z)
{
//    qDebug() << detail << x << "," << y << "," << z;
    mCurrentIssueFile->issues += Issue(mCurrentIssueFile, type, QString::fromLatin1(detail), x, y, z);
}

void CheckBuildingsWindow::issue(Issue::Type type, const char *detail, BuildingObject *object)
{
    mCurrentIssueFile->issues += Issue(mCurrentIssueFile, type, QString::fromLatin1(detail), object);
}

void CheckBuildingsWindow::updateList(CheckBuildingsWindow::IssueFile *file)
{
    QTreeWidgetItem *fileItem = ui->treeWidget->topLevelItem(mFiles.indexOf(file));
    if (fileItem == 0) {
        fileItem = new QTreeWidgetItem(QStringList() << QFileInfo(file->path).fileName());
        ui->treeWidget->addTopLevelItem(fileItem);
        fileItem->setExpanded(true);
    }
    while (fileItem->childCount() > 0)
        delete fileItem->takeChild(0);
    for (int j = 0; j < file->issues.size(); j++) {
        QTreeWidgetItem *issueItem = new QTreeWidgetItem(QStringList() << file->issues[j].toString());
        fileItem->addChild(issueItem);
    }
}

void CheckBuildingsWindow::fileChanged(const QString &path)
{
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void CheckBuildingsWindow::fileChangedTimeout()
{
    foreach (const QString &path, mChangedFiles) {
//        qDebug() << "CHANGED " << path;
        mFileSystemWatcher->removePath(path);
        mWatchedFiles.removeOne(path);
        QFileInfo info(path);
        if (info.exists()) {
            mFileSystemWatcher->addPath(path);
            mWatchedFiles += path;
            foreach (IssueFile *file, mFiles) {
                if (file->path == path) {
                    check(path);
                    updateList(file);
                    syncList(file);
                    break;
                }
            }
        }
    }

    mChangedFiles.clear();
}

CheckBuildingsWindow::Issue::Issue(IssueFile *file, Type type, const QString &detail, BuildingObject *object) :
    file(file),
    type(type),
    detail(detail),
    x(object->x()),
    y(object->y()),
    z(object->floor()->level()),
    objectIndex(object->index())
{
}
