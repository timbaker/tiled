/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "categorydock.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"
#include "buildingtileentryview.h"
#include "buildingtools.h"
#include "buildingundoredo.h"
#include "furnituregroups.h"
#include "furnitureview.h"
#include "horizontallinedelegate.h"

#include "zoomable.h"

#include <QAction>
#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMenu>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace BuildingEditor;

//SINGLETON_IMPL(CategoryDock)

CategoryDock::CategoryDock(QWidget *parent) :
    QDockWidget(parent),
    mCurrentDocument(0),
    mCategory(0),
    mFurnitureGroup(0),
    mInitialCategoryViewSelectionEvent(false),
    mCategoryZoomable(new Tiled::Internal::Zoomable(this)),
    mUsedContextMenu(new QMenu(this)),
    mActionClearUsed(new QAction(this)),
    mSynching(false),
    ui(&_ui)
{
    setObjectName(QLatin1String("CategoryDock"));
    setWindowTitle(tr("Tiles and Furniture"));

    ui->categoryList = new QListWidget;
    ui->categoryList->setObjectName(QLatin1String("CategoryDock.categoryList"));

    ui->tilesetView = new BuildingTileEntryView;
    ui->tilesetView->setObjectName(QLatin1String("CategoryDock.tilesetView"));

    ui->furnitureView = new FurnitureView;
    ui->furnitureView->setObjectName(QLatin1String("CategoryDock.furnitureView"));

    ui->scaleComboBox = new QComboBox;
    ui->scaleComboBox->setObjectName(QLatin1String("CategoryDock.scaleComboBox"));

    ui->categoryStack = new QStackedWidget;
    ui->categoryStack->setObjectName(QLatin1String("CategoryDock.stack"));
    ui->categoryStack->addWidget(ui->tilesetView);
    ui->categoryStack->addWidget(ui->furnitureView);

    ui->categorySplitter = new QSplitter;
    ui->categorySplitter->setObjectName(QLatin1String("CategoryDock.splitter"));
    ui->categorySplitter->setOrientation(Qt::Vertical);
    ui->categorySplitter->addWidget(ui->categoryList);
    ui->categorySplitter->addWidget(ui->categoryStack);
    ui->categorySplitter->setSizes(QList<int>() << 128 << 256);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->setObjectName(QLatin1String("CategoryDock.comboLayout"));
    hbox->setMargin(0);
    hbox->addStretch(1);
    hbox->addWidget(ui->scaleComboBox);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setObjectName(QLatin1String("CategoryDock.contentsLayout"));
    vbox->setMargin(2);
    vbox->addWidget(ui->categorySplitter);
    vbox->addLayout(hbox);
    QWidget *w = new QWidget;
    w->setObjectName(QLatin1String("CategoryDock.contents"));
    w->setLayout(vbox);
    setWidget(w);

    mCategoryZoomable->setScale(BuildingPreferences::instance()->tileScale());

    BuildingPreferences *prefs = BuildingPreferences::instance();

    mCategoryZoomable->connectToComboBox(ui->scaleComboBox);
    connect(mCategoryZoomable, SIGNAL(scaleChanged(qreal)),
            prefs, SLOT(setTileScale(qreal)));
    connect(prefs, SIGNAL(tileScaleChanged(qreal)),
            SLOT(categoryScaleChanged(qreal)));

    connect(ui->categoryList, SIGNAL(itemSelectionChanged()),
            SLOT(categorySelectionChanged()));
    connect(ui->categoryList, SIGNAL(activated(QModelIndex)),
            SLOT(categoryActivated(QModelIndex)));

    ui->tilesetView->setZoomable(mCategoryZoomable);
    connect(ui->tilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(ui->tilesetView, SIGNAL(mousePressed()), SLOT(categoryViewMousePressed()));

    ui->furnitureView->setZoomable(mCategoryZoomable);
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(furnitureSelectionChanged()));

    QIcon clearIcon(QLatin1String(":/images/16x16/edit-clear.png"));
    mActionClearUsed->setIcon(clearIcon);
    mActionClearUsed->setText(tr("Remove unused entries"));
    connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedTiles()));
    mUsedContextMenu->addAction(mActionClearUsed);

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged()));

    /////

    // Add tile categories to the gui
    setCategoryList();

    /////

    QSettings mSettings;
    mSettings.beginGroup(QLatin1String("BuildingEditor/CategoryDock"));
    QString categoryName = mSettings.value(QLatin1String("SelectedCategory")).toString();
    if (!categoryName.isEmpty()) {
        int index = BuildingTilesMgr::instance()->indexOf(categoryName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstCategory + index);
    }
    QString fGroupName = mSettings.value(QLatin1String("SelectedFurnitureGroup")).toString();
    if (!fGroupName.isEmpty()) {
        int index = FurnitureGroups::instance()->indexOf(fGroupName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstFurnitureGroup + index);
    }
    mSettings.endGroup();

    // This will create the Tiles dialog.  It must come after reading all the
    // config files.
    connect(BuildingTilesDialog::instance(), SIGNAL(edited()),
            SLOT(tilesDialogEdited()));

    connect(BuildingDocumentMgr::instance(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));
}

void CategoryDock::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument)
        mCurrentDocument->disconnect(this);

    mCurrentDocument = doc;

    if (mCurrentDocument) {
        connect(mCurrentDocument, SIGNAL(currentRoomChanged()),
                SLOT(currentRoomChanged()));
        connect(mCurrentDocument, SIGNAL(usedFurnitureChanged()),
                SLOT(usedFurnitureChanged()));
        connect(mCurrentDocument, SIGNAL(usedTilesChanged()),
                SLOT(usedTilesChanged()));
    }

    if (ui->categoryList->currentRow() < 2)
        categorySelectionChanged();
}

Building *CategoryDock::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
}

Room *CategoryDock::currentRoom() const
{
    return mCurrentDocument ? mCurrentDocument->currentRoom() : 0;
}

void CategoryDock::setCategoryList()
{
    mSynching = true;

    ui->categoryList->clear();

    ui->categoryList->addItem(tr("Used Tiles"));
    ui->categoryList->addItem(tr("Used Furniture"));

    HorizontalLineDelegate::instance()->addToList(ui->categoryList);

    mRowOfFirstCategory = ui->categoryList->count();
    foreach (BuildingTileCategory *category, BuildingTilesMgr::instance()->categories()) {
        ui->categoryList->addItem(category->label());
    }

    HorizontalLineDelegate::instance()->addToList(ui->categoryList);

    mRowOfFirstFurnitureGroup = ui->categoryList->count();
    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        ui->categoryList->addItem(group->mLabel);
    }

    mSynching = false;
}

void CategoryDock::categoryScaleChanged(qreal scale)
{
    mCategoryZoomable->setScale(scale);
}

void CategoryDock::categoryViewMousePressed()
{
    mInitialCategoryViewSelectionEvent = false;
}

void CategoryDock::categoryActivated(const QModelIndex &index)
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();

    int row = index.row();
    if (row >= 0 && row < 2)
        ;
    else if (BuildingTileCategory *category = categoryAt(row)) {
        dialog->selectCategory(category);
    } else if (FurnitureGroup *group = furnitureGroupAt(row)) {
        dialog->selectCategory(group);
    }
    BuildingEditorWindow::instance()->tilesDialog();
}

static QString paddedNumber(int number)
{
    return QString(QLatin1String("%1")).arg(number, 3, 10, QLatin1Char('0'));
}

void CategoryDock::categorySelectionChanged()
{
    mCategory = 0;
    mFurnitureGroup = 0;

    ui->tilesetView->clear();
    ui->furnitureView->clear();

    ui->tilesetView->setContextMenu(0);
    ui->furnitureView->setContextMenu(0);
    mActionClearUsed->disconnect(this);

    QList<QListWidgetItem*> selected = ui->categoryList->selectedItems();
    if (selected.count() == 1) {
        int row = ui->categoryList->row(selected.first());
        if (row == 0) { // Used Tiles
            if (!mCurrentDocument) return;

            // Sort by category + index
            QList<BuildingTileCategory*> categories;
            QMap<QString,BuildingTileEntry*> entryMap;
            foreach (BuildingTileEntry *entry, currentBuilding()->usedTiles()) {
                BuildingTileCategory *category = entry->category();
                int categoryIndex = BuildingTilesMgr::instance()->indexOf(category);
                int index = category->indexOf(entry) + 1;
                QString key = paddedNumber(categoryIndex) + QLatin1String("_") + paddedNumber(index);
                entryMap[key] = entry;
                if (!categories.contains(category) && category->canAssignNone())
                    categories += entry->category();
            }

            // Add "none" tile first in each category where it is allowed.
            foreach (BuildingTileCategory *category, categories) {
                int categoryIndex = BuildingTilesMgr::instance()->indexOf(category);
                QString key = paddedNumber(categoryIndex) + QLatin1String("_") + paddedNumber(0);
                entryMap[key] = category->noneTileEntry();
            }
#if 1
            ui->tilesetView->setEntries(entryMap.values(), true);
#else
            QList<Tiled::Tile*> tiles;
            QList<void*> userData;
            QStringList headers;
            foreach (BuildingTileEntry *entry, entryMap.values()) {
                if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                    tiles += tile;
                    userData += entry;
                    headers += entry->category()->label();
                }
            }
            ui->tilesetView->setTiles(tiles, userData, headers);
#endif
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedTiles()));
            ui->tilesetView->setContextMenu(mUsedContextMenu);
        } else if (row == 1) { // Used Furniture
            if (!mCurrentDocument) return;
            QMap<QString,FurnitureTiles*> furnitureMap;
            int index = 0;
            foreach (FurnitureTiles *ftiles, currentBuilding()->usedFurniture()) {
                // Sort by category name + index
                QString key = tr("<No Group>") + QString::number(index++);
                if (FurnitureGroup *g = ftiles->group()) {
                    key = g->mLabel + QString::number(g->mTiles.indexOf(ftiles));
                }
                if (!furnitureMap.contains(key))
                    furnitureMap[key] = ftiles;
            }
            ui->furnitureView->setTiles(furnitureMap.values());
            ui->furnitureView->scrollToTop();
            ui->categoryStack->setCurrentIndex(1);

            connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedFurniture()));
            ui->furnitureView->setContextMenu(mUsedContextMenu);
        } else if ((mCategory = categoryAt(row))) {
#if 1
            QList<BuildingTileEntry*> entries;
            if (mCategory->canAssignNone()) {
                entries += BuildingTilesMgr::instance()->noneTileEntry();
            }
            QMap<QString,BuildingTileEntry*> entryMap;
            int i = 0;
            foreach (BuildingTileEntry *entry, mCategory->entries()) {
                QString key = entry->displayTile()->name() + QString::number(i++);
                entryMap[key] = entry;
            }
            entries += entryMap.values();
            ui->tilesetView->setEntries(entries);
#else
            QList<Tiled::Tile*> tiles;
            QList<void*> userData;
            QStringList headers;
            if (mCategory->canAssignNone()) {
                tiles += BuildingTilesMgr::instance()->noneTiledTile();
                userData += BuildingTilesMgr::instance()->noneTileEntry();
                headers += BuildingTilesMgr::instance()->noneTiledTile()->tileset()->name();
            }
            QMap<QString,BuildingTileEntry*> entryMap;
            int i = 0;
            foreach (BuildingTileEntry *entry, mCategory->entries()) {
                QString key = entry->displayTile()->name() + QString::number(i++);
                entryMap[key] = entry;
            }
            foreach (BuildingTileEntry *entry, entryMap.values()) {
                if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                    tiles += tile;
                    userData += entry;
                    if (tile == TilesetManager::instance()->missingTile())
                        headers += entry->displayTile()->mTilesetName;
                    else
                        headers += tile->tileset()->name();
                }
            }
            ui->tilesetView->setTiles(tiles, userData, headers);
#endif
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            selectCurrentCategoryTile();
        } else if ((mFurnitureGroup = furnitureGroupAt(row))) {
            ui->furnitureView->setTiles(mFurnitureGroup->mTiles);
            ui->furnitureView->scrollToTop();
            ui->categoryStack->setCurrentIndex(1);
        }
    }
}

void CategoryDock::currentEWallChanged(BuildingTileEntry *entry, bool mergeable)
{
    // Assign the new tile to selected wall objects.
    QList<WallObject*> objects;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (WallObject *wall = object->asWall()) {
            if (wall->tile() != entry)
                objects += wall;
        }
    }
    if (objects.size()) {
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Wall Object Exterior Tile"));
        foreach (WallObject *wall, objects)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     wall,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
    if (objects.size() || WallTool::instance()->isCurrent()) {
        WallTool::instance()->setCurrentExteriorTile(entry);
        return;
    }

    mCurrentDocument->undoStack()->push(
                new ChangeBuildingTile(mCurrentDocument,
                                       Building::ExteriorWall, entry,
                                       mergeable));
}

void CategoryDock::currentIWallChanged(BuildingTileEntry *entry, bool mergeable)
{
    // Assign the new tile to selected wall objects.
    QList<WallObject*> objects;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (WallObject *wall = object->asWall()) {
            if (wall->tile() != entry)
                objects += wall;
        }
    }
    if (objects.size()) {
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Wall Object Interior Tile"));
        foreach (WallObject *wall, objects)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     wall,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
    if (objects.size() || WallTool::instance()->isCurrent()) {
        WallTool::instance()->setCurrentInteriorTile(entry);
        return;
    }

    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           Room::InteriorWall,
                                                           entry, mergeable));
}

void CategoryDock::currentFloorChanged(BuildingTileEntry *entry, bool mergeable)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           Room::Floor,
                                                           entry, mergeable));
}

void CategoryDock::currentDoorChanged(BuildingTileEntry *entry, bool mergeable)
{
    currentBuilding()->setDoorTile(entry);

    // Assign the new tile to selected doors
    QList<Door*> doors;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Door *door = object->asDoor()) {
            if (door->tile() != entry)
                doors += door;
        }
    }
    if (doors.count()) {
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Tile"));
        foreach (Door *door, doors)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     door,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::currentDoorFrameChanged(BuildingTileEntry *entry, bool mergeable)
{
    currentBuilding()->setDoorFrameTile(entry);

    // Assign the new tile to selected doors
    QList<Door*> doors;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Door *door = object->asDoor()) {
            if (door->frameTile() != entry)
                doors += door;
        }
    }
    if (doors.count()) {
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
        foreach (Door *door, doors)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     door,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::currentWindowChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New windows will be created with this tile
    currentBuilding()->setWindowTile(entry);

    // Assign the new tile to selected windows
    QList<Window*> windows;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Window *window = object->asWindow()) {
            if (window->tile() != entry)
                windows += window;
        }
    }
    if (windows.count()) {
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
        foreach (Window *window, windows)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     window,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::currentCurtainsChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New windows will be created with this tile
    currentBuilding()->setCurtainsTile(entry);

    // Assign the new tile to selected windows
    QList<Window*> windows;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Window *window = object->asWindow()) {
            if (window->curtainsTile() != entry)
                windows += window;
        }
    }
    if (windows.count()) {
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Window Curtains"));
        foreach (Window *window, windows)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     window,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::currentStairsChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New stairs will be created with this tile
    currentBuilding()->setStairsTile(entry);

    // Assign the new tile to selected stairs
    QList<Stairs*> stairsList;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Stairs *stairs = object->asStairs()) {
            if (stairs->tile() != entry)
                stairsList += stairs;
        }
    }
    if (stairsList.count()) {
        if (stairsList.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Stairs Tile"));
        foreach (Stairs *stairs, stairsList)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     stairs,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (stairsList.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::currentRoomTileChanged(int entryEnum,
                                                  BuildingTileEntry *entry,
                                                  bool mergeable)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           entryEnum,
                                                           entry, mergeable));
}

void CategoryDock::currentRoofTileChanged(BuildingTileEntry *entry, int which, bool mergeable)
{
    // New roofs will be created with these tiles
    switch (which) {
    case RoofObject::TileCap: currentBuilding()->setRoofCapTile(entry); break;
    case RoofObject::TileSlope: currentBuilding()->setRoofSlopeTile(entry); break;
    case RoofObject::TileTop: currentBuilding()->setRoofTopTile(entry); break;
    default:
        qFatal("bogus 'which' passed to CategoryDock::currentRoofTileChanged");
        break;
    }

    BuildingEditorWindow::instance()->hackUpdateActions(); // in case the roof tools should be enabled

    QList<RoofObject*> objectList;
    bool selectedOnly = (which == RoofObject::TileTop);
    if (selectedOnly) {
        foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
            if (RoofObject *roof = object->asRoof())
                if (roof->tile(which) != entry)
                    objectList += roof;
        }
    } else {
        // Change the tiles for each roof object.
        foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (RoofObject *roof = object->asRoof())
                    if (roof->tile(which) != entry)
                        objectList += roof;
            }
        }
    }

    if (objectList.count()) {
        if (objectList.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Roof Tiles"));
        foreach (RoofObject *roof, objectList)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     roof,
                                                                     entry,
                                                                     mergeable,
                                                                     which));
        if (objectList.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void CategoryDock::selectCurrentCategoryTile()
{
    if (!mCurrentDocument || !mCategory)
        return;
    BuildingTileEntry *currentTile = 0;
    if (mCategory->asExteriorWalls()) {
        if (WallTool::instance()->isCurrent())
            currentTile = WallTool::instance()->currentExteriorTile();
        else
            currentTile = mCurrentDocument->building()->exteriorWall();
    }
    if (mCategory->asInteriorWalls()) {
        if (WallTool::instance()->isCurrent())
            currentTile = WallTool::instance()->currentInteriorTile();
        else if (currentRoom())
            currentTile = currentRoom()->tile(Room::InteriorWall);
    }
    if (currentRoom() && mCategory->asFloors())
        currentTile = currentRoom()->tile(Room::Floor);
    if (mCategory->asDoors())
        currentTile = mCurrentDocument->building()->doorTile();
    if (mCategory->asDoorFrames())
        currentTile = mCurrentDocument->building()->doorFrameTile();
    if (mCategory->asWindows())
        currentTile = mCurrentDocument->building()->windowTile();
    if (mCategory->asCurtains())
        currentTile = mCurrentDocument->building()->curtainsTile();
    if (mCategory->asStairs())
        currentTile = mCurrentDocument->building()->stairsTile();
    if (mCategory->asGrimeFloor() && currentRoom())
        currentTile = currentRoom()->tile(Room::GrimeFloor);
    if (mCategory->asGrimeWall() && currentRoom())
        currentTile = currentRoom()->tile(Room::GrimeWall);
    if (mCategory->asRoofCaps())
        currentTile = mCurrentDocument->building()->roofCapTile();
    if (mCategory->asRoofSlopes())
        currentTile = mCurrentDocument->building()->roofSlopeTile();
    if (mCategory->asRoofTops())
        currentTile = mCurrentDocument->building()->roofTopTile();
    if (currentTile && (currentTile->isNone() || (currentTile->category() == mCategory))) {
#if 1
        mSynching = true;
        QModelIndex index = ui->tilesetView->index(currentTile);
        ui->tilesetView->setCurrentIndex(index);
        mSynching = false;
#else
        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(currentTile->displayTile())) {
            QModelIndex index = ui->tilesetView->model()->index(tile);
            mSynching = true;
            ui->tilesetView->setCurrentIndex(index);
            mSynching = false;
        }
#endif
    }
}

BuildingTileCategory *CategoryDock::categoryAt(int row)
{
    if (row >= mRowOfFirstCategory &&
            row < mRowOfFirstCategory + BuildingTilesMgr::instance()->categoryCount())
        return BuildingTilesMgr::instance()->category(row - mRowOfFirstCategory);
    return 0;
}

FurnitureGroup *CategoryDock::furnitureGroupAt(int row)
{
    if (row >= mRowOfFirstFurnitureGroup &&
            row < mRowOfFirstFurnitureGroup + FurnitureGroups::instance()->groupCount())
        return FurnitureGroups::instance()->group(row - mRowOfFirstFurnitureGroup);
    return 0;
}

void CategoryDock::tileSelectionChanged()
{
    if (!mCurrentDocument || mSynching)
        return;

    QModelIndexList indexes = ui->tilesetView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
#if 1
        if (BuildingTileEntry *entry = ui->tilesetView->entry(index)) {
#else
        if (ui->tilesetView->model()->tileAt(index)) {
            BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(
                        ui->tilesetView->model()->userDataAt(index));
#endif
            bool mergeable = ui->tilesetView->mouseDown() &&
                    mInitialCategoryViewSelectionEvent;
            qDebug() << "mergeable=" << mergeable;
            mInitialCategoryViewSelectionEvent = true;
            BuildingTileCategory *category = mCategory ? mCategory : entry->category();
            if (category->isNone())
                ; // never happens
            else if (category->asExteriorWalls())
                currentEWallChanged(entry, mergeable);
            else if (category->asInteriorWalls())
                currentIWallChanged(entry, mergeable);
            else if (category->asFloors())
                currentFloorChanged(entry, mergeable);
            else if (category->asDoors())
                currentDoorChanged(entry, mergeable);
            else if (category->asDoorFrames())
                currentDoorFrameChanged(entry, mergeable);
            else if (category->asWindows())
                currentWindowChanged(entry, mergeable);
            else if (category->asCurtains())
                currentCurtainsChanged(entry, mergeable);
            else if (category->asStairs())
                currentStairsChanged(entry, mergeable);
            else if (category->asGrimeFloor())
                currentRoomTileChanged(Room::GrimeFloor, entry, mergeable);
            else if (category->asGrimeWall())
                currentRoomTileChanged(Room::GrimeWall, entry, mergeable);
            else if (category->asRoofCaps())
                currentRoofTileChanged(entry, RoofObject::TileCap, mergeable);
            else if (category->asRoofSlopes())
                currentRoofTileChanged(entry, RoofObject::TileSlope, mergeable);
            else if (category->asRoofTops())
                currentRoofTileChanged(entry, RoofObject::TileTop, mergeable);
            else
                qFatal("unhandled category in CategoryDock::tileSelectionChanged()");
        }
    }
}

void CategoryDock::furnitureSelectionChanged()
{
    if (!mCurrentDocument)
        return;

    QModelIndexList indexes = ui->furnitureView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        if (FurnitureTile *ftile = ui->furnitureView->model()->tileAt(index)) {
            FurnitureTool::instance()->setCurrentTile(ftile);

            // Assign the new tile to selected objects
            QList<FurnitureObject*> objects;
            foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
                if (FurnitureObject *furniture = object->asFurniture()) {
                    if (furniture->furnitureTile() != ftile)
                        objects += furniture;
                }
            }

            if (objects.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Furniture Tile"));
            foreach (FurnitureObject *furniture, objects)
                mCurrentDocument->undoStack()->push(new ChangeFurnitureObjectTile(mCurrentDocument,
                                                                                  furniture,
                                                                                  ftile));
            if (objects.count() > 1)
                mCurrentDocument->undoStack()->endMacro();

        }
    }

    // Possibly enable the FurnitureTool
    BuildingEditorWindow::instance()->hackUpdateActions();
}

void CategoryDock::objectPicked(BuildingObject *object)
{
    if (FurnitureObject *fobj = object->asFurniture())
        selectAndDisplay(fobj->furnitureTile());
    else
        selectAndDisplay(object->tile());
}

void CategoryDock::selectAndDisplay(BuildingTileEntry *entry)
{
    if (!entry)
        return;
    ui->categoryList->setCurrentRow(0); // Used Tiles
    QModelIndex index = ui->tilesetView->model()->index((void*)entry);
    ui->tilesetView->setCurrentIndex(index);
    QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                              Q_ARG(int, 0), Q_ARG(QModelIndex, index));
//    ui->tilesetView->scrollTo(index);
}

void CategoryDock::selectAndDisplay(FurnitureTile *ftile)
{
    if (!ftile)
        return;
    ui->categoryList->setCurrentRow(1); // Used Furniture
    QModelIndex index = ui->furnitureView->model()->index(ftile);
    ui->furnitureView->setCurrentIndex(index);
    QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                              Q_ARG(int, 1), Q_ARG(QModelIndex, index));
    //    ui->furnitureView->scrollTo(index);
}

void CategoryDock::readSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("CategoryDock"));
    BuildingEditorWindow::instance()->restoreSplitterSizes(ui->categorySplitter);
    settings.endGroup();
}

void CategoryDock::writeSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("CategoryDock"));
    settings.setValue(QLatin1String("SelectedCategory"),
                      mCategory ? mCategory->name() : QString());
    settings.setValue(QLatin1String("SelectedFurnitureGroup"),
                      mFurnitureGroup ? mFurnitureGroup->mLabel : QString());
    BuildingEditorWindow::instance()->saveSplitterSizes(ui->categorySplitter);
    settings.endGroup();

}

void CategoryDock::scrollToNow(int which, const QModelIndex &index)
{
    if (which == 0)
        ui->tilesetView->scrollTo(index);
    else
        ui->furnitureView->scrollTo(index);
}

void CategoryDock::usedTilesChanged()
{
    if (ui->categoryList->currentRow() == 0)
        categorySelectionChanged();
}

void CategoryDock::usedFurnitureChanged()
{
    if (ui->categoryList->currentRow() == 1)
        categorySelectionChanged();
}


void CategoryDock::resetUsedTiles()
{
    Building *building = currentBuilding();
    if (!building)
        return;

    QList<BuildingTileEntry*> entries;
    foreach (BuildingFloor *floor, building->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (object->asFurniture())
                continue;
            for (int i = 0; i < 3; i++) {
                if (object->tile(i) && !object->tile(i)->isNone()
                        && !entries.contains(object->tile(i)))
                    entries += object->tile(i);
            }
        }
    }

    if (BuildingTileEntry *entry = building->exteriorWall()) {
        if (!entry->isNone() && !entries.contains(entry))
            entries += entry;
    }

    if (BuildingTileEntry *entry = building->tile(Building::GrimeWall)) {
        if (!entry->isNone() && !entries.contains(entry))
            entries += entry;
    }

    foreach (Room *room, building->rooms()) {
        foreach (BuildingTileEntry *entry, room->tiles()) {
            if (entry && !entry->isNone() && !entries.contains(entry))
                entries += entry;
        }
    }
    mCurrentDocument->undoStack()->push(new ChangeUsedTiles(mCurrentDocument,
                                                            entries));
}

void CategoryDock::resetUsedFurniture()
{
    Building *building = currentBuilding();
    if (!building)
        return;

    QList<FurnitureTiles*> furniture;
    foreach (BuildingFloor *floor, building->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (FurnitureObject *fo = object->asFurniture()) {
                if (FurnitureTile *ftile = fo->furnitureTile()) {
                    if (!furniture.contains(ftile->owner()))
                        furniture += ftile->owner();
                }
            }
        }
    }

    mCurrentDocument->undoStack()->push(new ChangeUsedFurniture(mCurrentDocument,
                                                                furniture));
}

void CategoryDock::currentRoomChanged()
{
    selectCurrentCategoryTile();
}

void CategoryDock::tilesDialogEdited()
{
    int row = ui->categoryList->currentRow();
    setCategoryList();
    row = qMin(row, ui->categoryList->count() - 1);
    ui->categoryList->setCurrentRow(row);
}

void CategoryDock::currentToolChanged()
{
    selectCurrentCategoryTile();
}

