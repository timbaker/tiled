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

#include "buildingtilesdialog.h"
#include "ui_buildingtilesdialog.h"

#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtmx.h"
#include "furnituregroups.h"
#include "furnitureview.h"
#include "horizontallinedelegate.h"

#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////
namespace BuildingEditor {

class AddTileToCategory : public QUndoCommand
{
public:
    AddTileToCategory(BuildingTilesDialog *d, BuildingTileCategory *category,
                      int index, BuildingTileEntry *entry) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tile to Category")),
        mDialog(d),
        mCategory(category),
        mIndex(index),
        mEntry(entry)
    {
    }

    void undo()
    {
        mEntry = mDialog->removeTile(mCategory, mIndex);
    }

    void redo()
    {
        mDialog->addTile(mCategory, mIndex, mEntry);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    int mIndex;
    BuildingTileEntry *mEntry;
};

class RemoveTileFromCategory : public QUndoCommand
{
public:
    RemoveTileFromCategory(BuildingTilesDialog *d, BuildingTileCategory *category,
                           int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tile from Category")),
        mDialog(d),
        mCategory(category),
        mIndex(index),
        mEntry(0)
    {
    }

    void undo()
    {
        mDialog->addTile(mCategory, mIndex, mEntry);
    }

    void redo()
    {
        mEntry = mDialog->removeTile(mCategory, mIndex);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    int mIndex;
    BuildingTileEntry *mEntry;
};

class RenameTileCategory : public QUndoCommand
{
public:
    RenameTileCategory(BuildingTilesDialog *d, BuildingTileCategory *category,
                   const QString &name) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Rename Tile Category")),
        mDialog(d),
        mCategory(category),
        mName(name)
    {
    }

    void undo()
    {
        mName = mDialog->renameTileCategory(mCategory, mName);
    }

    void redo()
    {
        mName = mDialog->renameTileCategory(mCategory, mName);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    QString mName;
};

class AddCategory : public QUndoCommand
{
public:
    AddCategory(BuildingTilesDialog *d, int index, FurnitureGroup *category) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Category")),
        mDialog(d),
        mIndex(index),
        mCategory(category)
    {
    }

    void undo()
    {
        mCategory = mDialog->removeCategory(mIndex);
    }

    void redo()
    {
        mDialog->addCategory(mIndex, mCategory);
        mCategory = 0;
    }

    BuildingTilesDialog *mDialog;
    int mIndex;
    FurnitureGroup *mCategory;
};

class RemoveCategory : public QUndoCommand
{
public:
    RemoveCategory(BuildingTilesDialog *d, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Category")),
        mDialog(d),
        mIndex(index),
        mCategory(0)
    {
    }

    void undo()
    {
        mDialog->addCategory(mIndex, mCategory);
        mCategory = 0;
    }

    void redo()
    {
        mCategory = mDialog->removeCategory(mIndex);
    }

    BuildingTilesDialog *mDialog;
    int mIndex;
    FurnitureGroup *mCategory;
};

class ReorderCategory : public QUndoCommand
{
public:
    ReorderCategory(BuildingTilesDialog *d, int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Category")),
        mDialog(d),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        mDialog->reorderCategory(mNewIndex, mOldIndex);
    }

    void redo()
    {
        mDialog->reorderCategory(mOldIndex, mNewIndex);
    }

    BuildingTilesDialog *mDialog;
    int mOldIndex;
    int mNewIndex;
};

class ChangeEntryTile : public QUndoCommand
{
public:
    ChangeEntryTile(BuildingTilesDialog *d, BuildingTileEntry *entry,
                        int index, const QString &tileName = QString()) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Entry Tile")),
        mDialog(d),
        mEntry(entry),
        mIndex(index),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mTileName = mDialog->changeEntryTile(mEntry, mIndex, mTileName);
    }

    void redo()
    {
        mTileName = mDialog->changeEntryTile(mEntry, mIndex, mTileName);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileEntry *mEntry;
    int mIndex;
    QString mTileName;
};

class ChangeEntryOffset : public QUndoCommand
{
public:
    ChangeEntryOffset(BuildingTilesDialog *d, BuildingTileEntry *entry,
                        int e, const QPoint &offset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Entry Offset")),
        mDialog(d),
        mEntry(entry),
        mEnum(e),
        mOffset(offset)
    {
    }

    void undo()
    {
        mOffset = mDialog->changeEntryOffset(mEntry, mEnum, mOffset);
    }

    void redo()
    {
        mOffset = mDialog->changeEntryOffset(mEntry, mEnum, mOffset);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileEntry *mEntry;
    int mEnum;
    QPoint mOffset;
};

class ReorderEntry : public QUndoCommand
{
public:
    ReorderEntry(BuildingTilesDialog *d, BuildingTileCategory *category,
                 int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Entry")),
        mDialog(d),
        mCategory(category),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        mDialog->reorderEntry(mCategory, mNewIndex, mOldIndex);
    }

    void redo()
    {
        mDialog->reorderEntry(mCategory, mOldIndex, mNewIndex);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    int mOldIndex;
    int mNewIndex;
};

class ChangeFurnitureTile : public QUndoCommand
{
public:
    ChangeFurnitureTile(BuildingTilesDialog *d, FurnitureTile *ftile,
                        int x, int y, const QString &tileName = QString()) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Tile")),
        mDialog(d),
        mTile(ftile),
        mX(x),
        mY(y),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    void redo()
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    BuildingTilesDialog *mDialog;
    FurnitureTile *mTile;
    int mX;
    int mY;
    QString mTileName;
};

class AddFurnitureTiles : public QUndoCommand
{
public:
    AddFurnitureTiles(BuildingTilesDialog *d, FurnitureGroup *category,
                      int index, FurnitureTiles *ftiles) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Furniture Tiles")),
        mDialog(d),
        mCategory(category),
        mIndex(index),
        mTiles(ftiles)
    {
    }

    void undo()
    {
        mTiles = mDialog->removeFurnitureTiles(mCategory, mIndex);
    }

    void redo()
    {
        mDialog->insertFurnitureTiles(mCategory, mIndex, mTiles);
        mTiles = 0;
    }

    BuildingTilesDialog *mDialog;
    FurnitureGroup *mCategory;
    int mIndex;
    FurnitureTiles *mTiles;
};

class RemoveFurnitureTiles : public QUndoCommand
{
public:
    RemoveFurnitureTiles(BuildingTilesDialog *d, FurnitureGroup *category,
                      int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Furniture Tiles")),
        mDialog(d),
        mCategory(category),
        mIndex(index),
        mTiles(0)
    {
    }

    void undo()
    {
        mDialog->insertFurnitureTiles(mCategory, mIndex, mTiles);
        mTiles = 0;
    }

    void redo()
    {
        mTiles = mDialog->removeFurnitureTiles(mCategory, mIndex);
    }

    BuildingTilesDialog *mDialog;
    FurnitureGroup *mCategory;
    int mIndex;
    FurnitureTiles *mTiles;
};

class ReorderFurniture : public QUndoCommand
{
public:
    ReorderFurniture(BuildingTilesDialog *d, FurnitureGroup *category,
                     int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Furniture")),
        mDialog(d),
        mCategory(category),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        mDialog->reorderFurniture(mCategory, mNewIndex, mOldIndex);
    }

    void redo()
    {
        mDialog->reorderFurniture(mCategory, mOldIndex, mNewIndex);
    }

    BuildingTilesDialog *mDialog;
    FurnitureGroup *mCategory;
    int mOldIndex;
    int mNewIndex;
};

class ToggleFurnitureCorners : public QUndoCommand
{
public:
    ToggleFurnitureCorners(BuildingTilesDialog *d, FurnitureTiles *tiles) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Toggle Corners")),
        mDialog(d),
        mTiles(tiles)
    {
    }

    void undo()
    {
        mDialog->toggleCorners(mTiles);
    }

    void redo()
    {
        mDialog->toggleCorners(mTiles);
    }

    BuildingTilesDialog *mDialog;
    FurnitureTiles *mTiles;
};

class ChangeFurnitureLayer : public QUndoCommand
{
public:
    ChangeFurnitureLayer(BuildingTilesDialog *d, FurnitureTiles *tiles,
                         int layer) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Layer")),
        mDialog(d),
        mTiles(tiles),
        mLayer(layer)
    {
    }

    void undo()
    {
        redo();
    }

    void redo()
    {
        mLayer = mDialog->changeFurnitureLayer(mTiles, mLayer);
    }

    BuildingTilesDialog *mDialog;
    FurnitureTiles *mTiles;
    int /*FurnitureTiles::FurnitureLayer*/ mLayer;
};

class ChangeFurnitureGrime : public QUndoCommand
{
public:
    ChangeFurnitureGrime(BuildingTilesDialog *d, FurnitureTile *ftile, bool allow) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Grime")),
        mDialog(d),
        mTile(ftile),
        mGrime(allow)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mGrime = mDialog->changeFurnitureGrime(mTile, mGrime);
    }

    BuildingTilesDialog *mDialog;
    FurnitureTile *mTile;
    bool mGrime;
};

class RenameFurnitureCategory : public QUndoCommand
{
public:
    RenameFurnitureCategory(BuildingTilesDialog *d, FurnitureGroup *category,
                   const QString &name) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Rename Furniture Category")),
        mDialog(d),
        mCategory(category),
        mName(name)
    {
    }

    void undo()
    {
        mName = mDialog->renameFurnitureCategory(mCategory, mName);
    }

    void redo()
    {
        mName = mDialog->renameFurnitureCategory(mCategory, mName);
    }

    BuildingTilesDialog *mDialog;
    FurnitureGroup *mCategory;
    QString mName;
};

} // namespace BuildingEditor

/////

BuildingTilesDialog *BuildingTilesDialog::mInstance = 0;

BuildingTilesDialog *BuildingTilesDialog::instance()
{
    if (!mInstance)
        mInstance = new BuildingTilesDialog();
    return mInstance;
}

void BuildingTilesDialog::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTilesDialog::BuildingTilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTilesDialog),
    mZoomable(new Zoomable(this)),
    mCategory(0),
    mCurrentEntry(0),
    mFurnitureGroup(0),
    mCurrentFurniture(0),
    mCurrentTileset(0),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this)),
    mSynching(false),
    mExpertMode(false)
{
    ui->setupUi(this);

    QSettings settings;
    mExpertMode = settings.value(QLatin1String("BuildingEditor/BuildingTilesDialog/ExpertMode"), false).toBool();
    ui->actionExpertMode->setChecked(mExpertMode);

    qreal scale = settings.value(QLatin1String("BuildingEditor/BuildingTilesDialog/TileScale"),
                                 BuildingPreferences::instance()->tileScale()).toReal();
    mZoomable->setScale(scale);

    ui->categoryTilesView->setZoomable(mZoomable);
    ui->categoryTilesView->setAcceptDrops(true);
    ui->categoryTilesView->setDropIndicatorShown(false);
    ui->categoryTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->categoryTilesView->model(), SIGNAL(tileDropped(QString,int)),
            SLOT(tileDropped(QString,int)));
    connect(ui->categoryTilesView->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(tileSelectionChanged()));
    connect(ui->categoryTilesView, SIGNAL(activated(QModelIndex)),
            SLOT(tileActivated(QModelIndex)));

    ui->categoryView->setZoomable(mZoomable);
    ui->categoryView->setAcceptDrops(true);
    ui->categoryView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->categoryView->model(), SIGNAL(tileDropped(BuildingTileEntry*,int,QString)),
            SLOT(entryTileDropped(BuildingTileEntry*,int,QString)));
    connect(ui->categoryView->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(entrySelectionChanged()));
    connect(ui->categoryView, SIGNAL(activated(QModelIndex)),
            SLOT(entryActivated(QModelIndex)));

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->model()->setShowResolved(false);
    ui->furnitureView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->furnitureView->model(),
            SIGNAL(furnitureTileDropped(FurnitureTile*,int,int,QString)),
            SLOT(furnitureTileDropped(FurnitureTile*,int,int,QString)));
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(furnitureSelectionChanged()));
    connect(ui->furnitureView, SIGNAL(activated(QModelIndex)),
            SLOT(furnitureActivated(QModelIndex)));

    connect(ui->categoryList, SIGNAL(currentRowChanged(int)),
            SLOT(categoryChanged(int)));
    connect(ui->categoryList, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(categoryNameEdited(QListWidgetItem*)));
//    mCategory = BuildingTiles::instance()->categories().at(ui->categoryList->currentRow());

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(ui->tilesetList, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    ui->tilesetTilesView->setZoomable(mZoomable);
    ui->tilesetTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tilesetTilesView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    ui->tilesetTilesView->setDragEnabled(true);

    /////
    QToolBar *toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionNewCategory);
    toolBar->addAction(ui->actionRemoveCategory);
    toolBar->addAction(ui->actionMoveCategoryUp);
    toolBar->addAction(ui->actionMoveCategoryDown);
    connect(ui->actionRemoveCategory, SIGNAL(triggered()), SLOT(removeCategory()));
    connect(ui->actionNewCategory, SIGNAL(triggered()), SLOT(newCategory()));
    connect(ui->actionMoveCategoryUp, SIGNAL(triggered()), SLOT(moveCategoryUp()));
    connect(ui->actionMoveCategoryDown, SIGNAL(triggered()), SLOT(moveCategoryDown()));
    ui->categoryListToolbarLayout->addWidget(toolBar);
    /////

    // Create UI for adjusting BuildingTileEntry offset
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->setMargin(0);

    QLabel *label = new QLabel(tr("Tile Offset"));
    hbox->addWidget(label);

    label = new QLabel(tr("x:"));
    mEntryOffsetSpinX = new QSpinBox();
    mEntryOffsetSpinX->setRange(-3, 3);
    hbox->addWidget(label);
    hbox->addWidget(mEntryOffsetSpinX);

    label = new QLabel(tr("y:"));
    mEntryOffsetSpinY = new QSpinBox();
    mEntryOffsetSpinY->setRange(-3, 3);
    hbox->addWidget(label);
    hbox->addWidget(mEntryOffsetSpinY);

    hbox->addStretch(1);

    QWidget *layoutWidget = new QWidget();
    layoutWidget->setLayout(hbox);
    ui->categoryLayout->insertWidget(1, layoutWidget);
    mEntryOffsetUI = layoutWidget;
    connect(mEntryOffsetSpinX, SIGNAL(valueChanged(int)),
            SLOT(entryOffsetChanged()));
    connect(mEntryOffsetSpinY, SIGNAL(valueChanged(int)),
            SLOT(entryOffsetChanged()));

    // Create UI for choosing furniture layer
    {
    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->setMargin(0);

    QLabel *label = new QLabel(tr("Layer:"));
    hbox->addWidget(label);

    QComboBox *cb = new QComboBox;
    cb->addItems(FurnitureTiles::layerNames());
    hbox->addWidget(cb);

    mFurnitureGrimeCheckBox = new QCheckBox(tr("Grime"));
    hbox->addWidget(mFurnitureGrimeCheckBox);

    hbox->addStretch(1);

    QWidget *layoutWidget = new QWidget();
    layoutWidget->setLayout(hbox);
    ui->categoryLayout->insertWidget(2, layoutWidget);
    mFurnitureLayerUI = layoutWidget;
    mFurnitureLayerComboBox = cb;
    connect(mFurnitureLayerComboBox, SIGNAL(currentIndexChanged(int)),
            SLOT(furnitureLayerChanged(int)));
    }
    connect(mFurnitureGrimeCheckBox, SIGNAL(toggled(bool)),
            SLOT(furnitureGrimeChanged(bool)));

    /////
    toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionToggleCorners);
    toolBar->addAction(ui->actionClearTiles);
    toolBar->addAction(ui->actionMoveTileUp);
    toolBar->addAction(ui->actionMoveTileDown);
    toolBar->addSeparator();
    toolBar->addAction(ui->actionAddTiles);
    toolBar->addAction(ui->actionRemoveTiles);
    toolBar->addAction(ui->actionExpertMode);
    connect(ui->actionToggleCorners, SIGNAL(triggered()), SLOT(toggleCorners()));
    connect(ui->actionClearTiles, SIGNAL(triggered()), SLOT(clearTiles()));
    connect(ui->actionMoveTileUp, SIGNAL(triggered()), SLOT(moveTileUp()));
    connect(ui->actionMoveTileDown, SIGNAL(triggered()), SLOT(moveTileDown()));
    connect(ui->actionAddTiles, SIGNAL(triggered()), SLOT(addTiles()));
    connect(ui->actionRemoveTiles, SIGNAL(triggered()), SLOT(removeTiles()));
    connect(ui->actionExpertMode, SIGNAL(toggled(bool)), SLOT(setExpertMode(bool)));
    ui->categoryToolbarLayout->addWidget(toolBar, 1);

    QComboBox *scaleComboBox = new QComboBox;
    mZoomable->connectToComboBox(scaleComboBox);
//    scaleComboBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    ui->categoryToolbarLayout->addWidget(scaleComboBox);
    /////

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QToolButton *button = new QToolButton(this);
    button->setIcon(undoIcon);
    Utils::setThemeIcon(button, "edit-undo");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(undoAction->text());
    button->setEnabled(mUndoGroup->canUndo());
    button->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->buttonsLayout->insertWidget(0, button);
    connect(mUndoGroup, SIGNAL(canUndoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(mUndoGroup, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));
    connect(button, SIGNAL(clicked()), undoAction, SIGNAL(triggered()));

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(redoAction->text());
    button->setEnabled(mUndoGroup->canRedo());
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->buttonsLayout->insertWidget(1, button);
    connect(mUndoGroup, SIGNAL(canRedoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), redoAction, SIGNAL(triggered()));

    connect(ui->tilesetMgr, SIGNAL(clicked()), SLOT(manageTilesets()));

    ui->overallSplitter->setStretchFactor(1, 1);

    setCategoryList();
    setTilesetList();

    synchUI();

    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);

    QString categoryName = settings.value(QLatin1String("SelectedCategory")).toString();
    if (!categoryName.isEmpty()) {
        int index = BuildingTilesMgr::instance()->indexOf(categoryName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstCategory + index);
    }

    QString furnitureGroupName = settings.value(QLatin1String("SelectedFurnitureGroup")).toString();
    if (!furnitureGroupName.isEmpty()) {
        int index = FurnitureGroups::instance()->indexOf(furnitureGroupName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstFurnitureGroup + index);
    }

    QString tilesetName = settings.value(QLatin1String("SelectedTileset")).toString();
    if (!tilesetName.isEmpty()) {
        if (Tiled::Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            int index = TileMetaInfoMgr::instance()->indexOf(tileset);
            ui->tilesetList->setCurrentRow(index);
        }
    }
    settings.endGroup();

    restoreSplitterSizes(ui->overallSplitter);
    restoreSplitterSizes(ui->categorySplitter);
}

BuildingTilesDialog::~BuildingTilesDialog()
{
    delete ui;
}

bool BuildingTilesDialog::changes()
{
    bool changes = mChanges;
    mChanges = false;
    return changes;
}

void BuildingTilesDialog::selectCategory(BuildingTileCategory *category)
{
    int index = BuildingTilesMgr::instance()->indexOf(category);
    ui->categoryList->setCurrentRow(mRowOfFirstCategory + index);
}

void BuildingTilesDialog::selectCategory(FurnitureGroup *furnitureGroup)
{
    int index = FurnitureGroups::instance()->indexOf(furnitureGroup);
    ui->categoryList->setCurrentRow(mRowOfFirstFurnitureGroup + index);
}

void BuildingTilesDialog::reparent(QWidget *parent)
{
    QPoint savePosition = pos();
    setParent(parent, windowFlags());
    move(savePosition);
}

void BuildingTilesDialog::addTile(BuildingTileCategory *category,
                                  int index, BuildingTileEntry *entry)
{
    // Create a new BuildingTileEntry based on assumptions about the order of
    // tiles in the tileset.
    category->insertEntry(index, entry);

    tilesetSelectionChanged();
    setCategoryTiles();

    if (mExpertMode) {
        ui->categoryView->scrollTo(
                    ui->categoryView->model()->index(
                        entry, category->shadowToEnum(0)));
    } else {
#if 1
        ui->categoryTilesView->scrollTo(ui->categoryTilesView->index(entry));
#else
        ui->categoryTilesView->scrollTo(
                    ui->categoryTilesView->model()->index((void*)entry));
#endif
    }
}

BuildingTileEntry *BuildingTilesDialog::removeTile(BuildingTileCategory *category, int index)
{
    BuildingTileEntry *entry = category->removeEntry(index);

    tilesetSelectionChanged();
    setCategoryTiles();

    return entry;
}

QString BuildingTilesDialog::renameTileCategory(BuildingTileCategory *category,
                                                const QString &name)
{
    QString old = category->label();
    category->setLabel(name);
    int row = BuildingTilesMgr::instance()->indexOf(category);
    ui->categoryList->item(row)->setText(name);
    return old;
}

void BuildingTilesDialog::addCategory(int index, FurnitureGroup *category)
{
    FurnitureGroups::instance()->insertGroup(index, category);

    int row = ui->categoryList->currentRow();
    setCategoryList();
    if (index <= row)
        ++row;
    ui->categoryList->setCurrentRow(row);
    synchUI();
}

FurnitureGroup *BuildingTilesDialog::removeCategory(int index)
{
    int row = mRowOfFirstFurnitureGroup + index;
    delete ui->categoryList->item(row);
    FurnitureGroup *group = FurnitureGroups::instance()->removeGroup(index);
    if (group == mFurnitureGroup) {
        mFurnitureGroup = 0;
        mCurrentFurniture = 0;
    }
    synchUI();

    return group;
}

QString BuildingTilesDialog::changeEntryTile(BuildingTileEntry *entry, int e,
                                             const QString &tileName)
{
    QString old = entry->tile(e)->isNone() ? QString() : entry->tile(e)->name();
    entry->setTile(e, BuildingTilesMgr::instance()->get(tileName));

    BuildingTilesMgr::instance()->entryTileChanged(entry, e);

    ui->categoryView->update(ui->categoryView->model()->index(entry, e));
    return old;
}

QPoint BuildingTilesDialog::changeEntryOffset(BuildingTileEntry *entry, int e,
                                              const QPoint &offset)
{
    QPoint old = entry->offset(e);
    entry->setOffset(e, offset);
    BuildingTilesMgr::instance()->entryTileChanged(entry, e);
    ui->categoryView->update(ui->categoryView->model()->index(entry, e));
    return old;
}

void BuildingTilesDialog::reorderEntry(BuildingTileCategory *category,
                                       int oldIndex, int newIndex)
{
    BuildingTileEntry *entry = category->removeEntry(oldIndex);
    category->insertEntry(newIndex, entry);

    setCategoryTiles();
#if 1
    ui->categoryTilesView->setCurrentIndex(
                ui->categoryTilesView->index(category->entry(newIndex)));
#else
    ui->categoryTilesView->setCurrentIndex(
                ui->categoryTilesView->model()->index(
                    (void*)category->entry(newIndex)));
#endif
}

void BuildingTilesDialog::insertFurnitureTiles(FurnitureGroup *category,
                                               int index,
                                               FurnitureTiles *ftiles)
{
    category->mTiles.insert(index, ftiles);
    ftiles->setGroup(category);
    mCurrentFurniture = 0;
    ui->furnitureView->setTiles(category->mTiles);
}

FurnitureTiles *BuildingTilesDialog::removeFurnitureTiles(FurnitureGroup *category,
                                                          int index)
{
    FurnitureTiles *ftiles = category->mTiles.takeAt(index);
    ftiles->setGroup(0);
    ui->furnitureView->model()->removeTiles(ftiles);
    return ftiles;
}

QString BuildingTilesDialog::changeFurnitureTile(FurnitureTile *ftile,
                                                 int x, int y,
                                                 const QString &tileName)
{
    QString old = ftile->tile(x, y) ? ftile->tile(x, y)->name() : QString();
    QSize oldSize = ftile->size();
    ftile->setTile(x, y, tileName.isEmpty()
                   ? 0 : BuildingTilesMgr::instance()->get(tileName));

    FurnitureGroups::instance()->tileChanged(ftile);

    if (ftile->size() != oldSize)
        ui->furnitureView->furnitureTileResized(ftile);

    ui->furnitureView->update(ui->furnitureView->model()->index(ftile));
    return old;
}

void BuildingTilesDialog::reorderFurniture(FurnitureGroup *category,
                                           int oldIndex, int newIndex)
{
    FurnitureTiles *ftiles = category->mTiles.takeAt(oldIndex);
    category->mTiles.insert(newIndex, ftiles);

    setFurnitureTiles();
    ui->furnitureView->setCurrentIndex(
                ui->furnitureView->model()->index(ftiles->tile(FurnitureTile::FurnitureW)));
}

void BuildingTilesDialog::toggleCorners(FurnitureTiles *ftiles)
{
    ftiles->toggleCorners();

    FurnitureView *v = ui->furnitureView;
    v->model()->toggleCorners(ftiles);
}

int BuildingTilesDialog::changeFurnitureLayer(FurnitureTiles *ftiles, int layer)
{
    int old = ftiles->layer();
    ftiles->setLayer(static_cast<FurnitureTiles::FurnitureLayer>(layer));
    FurnitureGroups::instance()->layerChanged(ftiles);
    synchUI(); // update the layer combobox
    return old;
}

bool BuildingTilesDialog::changeFurnitureGrime(FurnitureTile *ftile, bool allow)
{
    bool old = ftile->allowGrime();
    ftile->setAllowGrime(allow);
    FurnitureGroups::instance()->grimeChanged(ftile);
    synchUI(); // update the grime checkbox
    return old;
}

QString BuildingTilesDialog::renameFurnitureCategory(FurnitureGroup *category,
                                            const QString &name)
{
    QString old = category->mLabel;
    category->mLabel = name;
    int row = mRowOfFirstFurnitureGroup + FurnitureGroups::instance()->indexOf(category);
    ui->categoryList->item(row)->setText(name);
    return old;
}

void BuildingTilesDialog::reorderCategory(int oldIndex, int newIndex)
{
    FurnitureGroup *category = FurnitureGroups::instance()->removeGroup(oldIndex);
    FurnitureGroups::instance()->insertGroup(newIndex, category);

    setCategoryList();
    ui->categoryList->setCurrentRow(mRowOfFirstFurnitureGroup + newIndex);
}

void BuildingTilesDialog::setCategoryList()
{
    ui->categoryList->clear();

    mRowOfFirstCategory = ui->categoryList->count();
    foreach (BuildingTileCategory *category, BuildingTilesMgr::instance()->categories()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(category->label());
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->categoryList->addItem(item);
    }

    HorizontalLineDelegate::instance()->addToList(ui->categoryList);

    mRowOfFirstFurnitureGroup = ui->categoryList->count();
    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(group->mLabel);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->categoryList->addItem(item);
    }
}

void BuildingTilesDialog::setCategoryTiles()
{ 
    bool expertMode = mExpertMode && mCategory && !mCategory->shadowImage().isNull();
#if 1
    QList<BuildingTileEntry*> entries;
    if (mCategory && !expertMode) {
        QMap<QString,BuildingTileEntry*> entryMap;
        int i = 0;
        foreach (BuildingTileEntry *entry, mCategory->entries()) {
            QString key = entry->displayTile()->name() + QString::number(i++);
            entryMap[key] = entry;
        }
        entries = entryMap.values();
    }
    mCurrentEntry = 0;
    ui->categoryTilesView->setEntries(entries);
#else
    QList<Tiled::Tile*> tiles;
    QList<void*> userData;
    QStringList headers;
    if (mCategory && !expertMode) {
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
    }
    mCurrentEntry = 0;
    ui->categoryTilesView->setTiles(tiles, userData, headers);
#endif
    ui->categoryView->setCategory(expertMode ? mCategory : 0);
}

void BuildingTilesDialog::setFurnitureTiles()
{
    QList<FurnitureTiles*> ftiles;
    if (mFurnitureGroup)
        ftiles = mFurnitureGroup->mTiles;
    mCurrentFurniture = 0;
    ui->furnitureView->setTiles(ftiles);
}

void BuildingTilesDialog::setTilesetList()
{
    ui->tilesetList->clear();
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing())
            item->setForeground(Qt::red);
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setFixedWidth(width + 16 + sbw);
}

void BuildingTilesDialog::saveSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    settings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    settings.endGroup();
}

void BuildingTilesDialog::restoreSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QVariant v = settings.value(tr("%1/sizes").arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
    settings.endGroup();
}

void BuildingTilesDialog::displayTileInTileset(Tiled::Tile *tile)
{
    if (!tile)
        return;
    int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset());
    if (row >= 0) {
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void BuildingTilesDialog::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
}

BuildingTileCategory *BuildingTilesDialog::categoryAt(int row)
{
    if (row >= mRowOfFirstCategory &&
            row < mRowOfFirstCategory + BuildingTilesMgr::instance()->categoryCount())
        return BuildingTilesMgr::instance()->category(row - mRowOfFirstCategory);
    return 0;
}

FurnitureGroup *BuildingTilesDialog::furnitureGroupAt(int row)
{
    if (row >= mRowOfFirstFurnitureGroup &&
            row < mRowOfFirstFurnitureGroup + FurnitureGroups::instance()->groupCount())
        return FurnitureGroups::instance()->group(row - mRowOfFirstFurnitureGroup);
    return 0;
}

void BuildingTilesDialog::synchUI()
{
    bool add = false;
    bool remove = false;
    bool clear = false;
    bool moveUp = false;
    bool moveDown = false;

    if (mFurnitureGroup) {
        add = true;
        remove = mCurrentFurniture != 0;
        clear = mCurrentFurniture != 0;
        if (mCurrentFurniture) {
            moveUp = mCurrentFurniture->owner() != mFurnitureGroup->mTiles.first();
            moveDown = mCurrentFurniture->owner() !=  mFurnitureGroup->mTiles.last();
        }
    } else if (mCategory) {
        if (mExpertMode) {
            add = true;
            remove = mCurrentEntry != 0;
            clear = mCurrentEntry != 0;
        } else {
            add = ui->tilesetTilesView->selectionModel()->selectedIndexes().count();;
            remove = mCurrentEntry != 0;
        }
    }
    ui->actionAddTiles->setEnabled(add);
    ui->actionRemoveTiles->setEnabled(remove);

    ui->actionRemoveCategory->setEnabled(mFurnitureGroup != 0);
    ui->actionMoveCategoryUp->setEnabled(mFurnitureGroup != 0 &&
            mFurnitureGroup != FurnitureGroups::instance()->groups().first());
    ui->actionMoveCategoryDown->setEnabled(mFurnitureGroup != 0 &&
            mFurnitureGroup != FurnitureGroups::instance()->groups().last());

    ui->actionToggleCorners->setEnabled(mFurnitureGroup && remove);
    ui->actionClearTiles->setEnabled(clear);
    ui->actionExpertMode->setEnabled(mFurnitureGroup == 0);

    mEntryOffsetUI->setVisible(mExpertMode && !mFurnitureGroup);
    mEntryOffsetUI->setEnabled(clear); // single item selected

    mSynching = true;
    if (mExpertMode && mCurrentEntry) {
        mEntryOffsetSpinX->setValue(mCurrentEntry->offset(mCurrentEntryEnum).x());
        mEntryOffsetSpinY->setValue(mCurrentEntry->offset(mCurrentEntryEnum).y());
    } else {
        mEntryOffsetSpinX->setValue(0);
        mEntryOffsetSpinY->setValue(0);
    }
    mSynching = false;

    mFurnitureLayerUI->setVisible(mFurnitureGroup);
    mFurnitureLayerUI->setEnabled(mCurrentFurniture);

    mSynching = true;
    if (mCurrentFurniture) {
        mFurnitureLayerComboBox->setCurrentIndex(mCurrentFurniture->owner()->layer());
        mFurnitureGrimeCheckBox->setChecked(mCurrentFurniture->allowGrime());
    }
    mSynching = false;

    ui->actionMoveTileUp->setEnabled(moveUp);
    ui->actionMoveTileDown->setEnabled(moveDown);

    ui->actionRemoveTileset->setEnabled(ui->tilesetList->currentRow() >= 0);
}

void BuildingTilesDialog::categoryChanged(int index)
{
    mCategory = 0;
    mCurrentEntry = 0;
    mFurnitureGroup = 0;
    mCurrentFurniture = 0;
    if (index < 0) {
        // only happens when setting the list again
        setCategoryTiles();
        setFurnitureTiles();
        tilesetSelectionChanged();
    } else if ((mCategory = categoryAt(index))) {
        if (mExpertMode && !mCategory->shadowImage().isNull()) {
            ui->categoryStack->setCurrentIndex(2);
        } else {
            ui->categoryStack->setCurrentIndex(0);
        }
        setCategoryTiles();
        tilesetSelectionChanged();
    } else if ((mFurnitureGroup = furnitureGroupAt(index))) {
        setFurnitureTiles();
        ui->categoryStack->setCurrentIndex(1);
    }
    synchUI();
}

void BuildingTilesDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mCurrentTileset = 0;
    if (item) {
#if 0
        QRect bounds;
        if (mCategory)
            bounds = mCategory->categoryBounds();
#endif
        int row = ui->tilesetList->row(item);
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
        if (mCurrentTileset->isMissing())
            ui->tilesetTilesView->clear();
        else
            ui->tilesetTilesView->setTileset(mCurrentTileset);
#if 0
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (mCategory && mCategory->usesTile(tile))
                model->setCategoryBounds(tile, bounds);
        }
#endif
    } else {
        ui->tilesetTilesView->clear();
    }
    synchUI();
}

void BuildingTilesDialog::addTiles()
{
    if (mFurnitureGroup != 0) {
        int index = mFurnitureGroup->mTiles.count();
        if (mCurrentFurniture)
            index = mFurnitureGroup->mTiles.indexOf(mCurrentFurniture->owner()) + 1;
        FurnitureTiles *tiles = new FurnitureTiles(false);
        mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup,
                                               index, tiles));
        return;
    }

    if (!mCategory)
        return;

    if (mExpertMode && !mCategory->shadowImage().isNull()) {
        int index = mCategory->entryCount();
        if (mCurrentEntry)
            index = mCategory->indexOf(mCurrentEntry) + 1;
        // Create a new blank entry in the category.
        BuildingTileEntry *entry = new BuildingTileEntry(mCategory);
        mUndoStack->push(new AddTileToCategory(this, mCategory,
                                               index, entry));
        return;
    }

    QModelIndexList selection = ui->tilesetTilesView->selectionModel()->selectedIndexes();
    QList<Tile*> tiles;
    foreach (QModelIndex index, selection) {
        Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
        if (!mCategory->usesTile(tile))
            tiles += tile;
    }
    if (tiles.isEmpty())
        return;
    if (tiles.count() > 1)
        mUndoStack->beginMacro(tr("Add Tiles To %1").arg(mCategory->label()));
    foreach (Tile *tile, tiles) {
        QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
        BuildingTileEntry *entry = mCategory->createEntryFromSingleTile(tileName);
        mUndoStack->push(new AddTileToCategory(this, mCategory,
                                               mCategory->entryCount(), entry));
    }
    if (tiles.count() > 1)
        mUndoStack->endMacro();
}

void BuildingTilesDialog::removeTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTiles*> remove;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *tile = v->model()->tileAt(index);
            if (!remove.contains(tile->owner()))
                remove += tile->owner();
        }
        if (remove.count() > 1)
            mUndoStack->beginMacro(tr("Remove Furniture Tiles"));
        foreach (FurnitureTiles *tiles, remove) {
            mUndoStack->push(new RemoveFurnitureTiles(this, mFurnitureGroup,
                                                      mFurnitureGroup->mTiles.indexOf(tiles)));
        }
        if (remove.count() > 1)
            mUndoStack->endMacro();
        return;
    }

    if (!mCategory)
        return;

    if (mExpertMode && !mCategory->shadowImage().isNull()) {
        TileCategoryView *v = ui->categoryView;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        QList<BuildingTileEntry*> entries;
        foreach (QModelIndex index, selection) {
            if (BuildingTileEntry *entry = v->model()->entryAt(index)) {
                if (!entries.contains(entry))
                    entries += entry;
            }
        }
        if (entries.count() > 1)
            mUndoStack->beginMacro(tr("Remove Tiles from %1").arg(mCategory->label()));
        foreach (BuildingTileEntry *entry, entries)
            mUndoStack->push(new RemoveTileFromCategory(this, mCategory,
                                                        mCategory->indexOf(entry)));
        if (entries.count() > 1)
            mUndoStack->endMacro();
        return;
    }

#if 1
    BuildingTileEntryView *v = ui->categoryTilesView;
#else
    MixedTilesetView *v = ui->categoryTilesView;
#endif
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    if (selection.count() > 1)
        mUndoStack->beginMacro(tr("Remove Tiles from %1").arg(mCategory->label()));
    foreach (QModelIndex index, selection) {
#if 1
        BuildingTileEntry *entry = v->entry(index);
#else
        BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(
                    v->model()->userDataAt(index));
#endif
        mUndoStack->push(new RemoveTileFromCategory(this, mCategory,
                                                    mCategory->indexOf(entry)));
    }
    if (selection.count() > 1)
        mUndoStack->endMacro();
}

void BuildingTilesDialog::clearTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTile*> clear;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *tile = v->model()->tileAt(index);
            if (!tile->isEmpty())
                clear += tile;
        }
        if (clear.isEmpty())
            return;
        mUndoStack->beginMacro(tr("Clear Furniture Tiles"));
        foreach (FurnitureTile* ftile, clear) {
            for (int x = 0; x < ftile->width(); x++) {
                for (int y = 0; y < ftile->height(); y++) {
                    if (ftile->tile(x, y))
                        mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y));
                }
            }
        }
        mUndoStack->endMacro();
        return;
    }

    if (mExpertMode && !mCategory->shadowImage().isNull()) {
        TileCategoryView *v = ui->categoryView;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        QList<BuildingTileEntry*> entries;
        QList<int> enums;
        foreach (QModelIndex index, selection) {
            if (BuildingTileEntry *entry = v->model()->entryAt(index)) {
                entries += entry;
                enums += v->model()->enumAt(index);
            }
        }
        if (entries.count() > 1)
            mUndoStack->beginMacro(tr("Clear Tiles from %1").arg(mCategory->label()));
        for (int i = 0; i < entries.size(); i++)
            mUndoStack->push(new ChangeEntryTile(this, entries[i], enums[i], QString()));
        if (entries.count() > 1)
            mUndoStack->endMacro();
        return;
    }
}

void BuildingTilesDialog::setExpertMode(bool expert)
{
    if (expert != mExpertMode) {
        mExpertMode = expert;
        BuildingTileEntry *entry = mCurrentEntry;
        categoryChanged(ui->categoryList->currentRow());
        if (entry) {
            if (mExpertMode) {
                TileCategoryView *v = ui->categoryView;
                QModelIndex index = v->model()->index(
                            entry, entry->category()->shadowToEnum(0));
                v->setCurrentIndex(index);
                v->scrollTo(index);
            } else {
#if 1
                BuildingTileEntryView *v = ui->categoryTilesView;
                v->setCurrentIndex(v->index(entry));
#else
                MixedTilesetView *v = ui->categoryTilesView;
                v->setCurrentIndex(v->model()->index((void*)entry));
#endif
                v->scrollTo(v->currentIndex());
            }
        }
    }
}

void BuildingTilesDialog::tileDropped(const QString &tilesetName, int tileId)
{
    if (!mCategory)
        return;

    QString tileName = BuildingTilesMgr::instance()->nameForTile(tilesetName, tileId);

    BuildingTileEntry *entry = mCategory->createEntryFromSingleTile(tileName);
    if (mCategory->findMatch(entry))
        return; // Don't allow the same entry twice (can add duplicate in expert mode)

    // Keep tiles from same tileset together.
    int index = 0;
    foreach (BuildingTileEntry *entry2, mCategory->entries()) {
        if (entry2->displayTile()->mTilesetName == entry->displayTile()->mTilesetName) {
            if (entry2->displayTile()->mIndex > entry->displayTile()->mIndex)
                break;
        }
        if (entry2->displayTile()->mTilesetName > entry->displayTile()->mTilesetName)
            break;
        ++index;
    }

    mUndoStack->push(new AddTileToCategory(this, mCategory, index, entry));
}

void BuildingTilesDialog::entryTileDropped(BuildingTileEntry *entry, int e, const QString &tileName)
{
    mUndoStack->push(new ChangeEntryTile(this, entry, e, tileName));
}

void BuildingTilesDialog::furnitureTileDropped(FurnitureTile *ftile, int x, int y,
                                               const QString &tileName)
{
    mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y, tileName));
}

void BuildingTilesDialog::categoryNameEdited(QListWidgetItem *item)
{
    int row = ui->categoryList->row(item);
    if (BuildingTileCategory *category = categoryAt(row)) {
        if (item->text() != category->label())
            mUndoStack->push(new RenameTileCategory(this, category, item->text()));
    }
    if (FurnitureGroup *group = furnitureGroupAt(row)) {
        if (item->text() != group->mLabel)
            mUndoStack->push(new RenameFurnitureCategory(this, group, item->text()));
    }
}

void BuildingTilesDialog::newCategory()
{
    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("New Category");
    int index = mFurnitureGroup
            ? FurnitureGroups::instance()->indexOf(mFurnitureGroup) + 1
            : FurnitureGroups::instance()->groupCount();
    mUndoStack->push(new AddCategory(this, index, group));

    QListWidgetItem *item = ui->categoryList->item(mRowOfFirstFurnitureGroup + index);
    ui->categoryList->setCurrentItem(item);
    ui->categoryList->editItem(item);
}

void BuildingTilesDialog::removeCategory()
{
    if (!mFurnitureGroup)
        return;

    mUndoStack->push(new RemoveCategory(this,
                                        FurnitureGroups::instance()->indexOf(mFurnitureGroup)));
}

void BuildingTilesDialog::moveCategoryUp()
{
    if (!mFurnitureGroup)
         return;
    int index = FurnitureGroups::instance()->indexOf(mFurnitureGroup);
    if (index == 0)
        return;
    mUndoStack->push(new ReorderCategory(this, index, index - 1));
}

void BuildingTilesDialog::moveCategoryDown()
{
    if (!mFurnitureGroup)
         return;
    int index = FurnitureGroups::instance()->indexOf(mFurnitureGroup);
    if (index == FurnitureGroups::instance()->groupCount() - 1)
        return;
    mUndoStack->push(new ReorderCategory(this, index, index + 1));
}

void BuildingTilesDialog::moveTileUp()
{
    if (mCurrentFurniture) {
        FurnitureTiles *ftiles = mCurrentFurniture->owner();
        int index = mFurnitureGroup->mTiles.indexOf(ftiles);
        if (index == 0)
            return;
        mUndoStack->push(new ReorderFurniture(this, mFurnitureGroup, index, index - 1));
    }

    if (mCurrentEntry) {
        int index = mCategory->indexOf(mCurrentEntry);
        if (index == 0)
            return;
        mUndoStack->push(new ReorderEntry(this, mCategory, index, index - 1));
    }
}

void BuildingTilesDialog::moveTileDown()
{
    if (mCurrentFurniture) {
        FurnitureTiles *ftiles = mCurrentFurniture->owner();
        int index = mFurnitureGroup->mTiles.indexOf(ftiles);
        if (index == mFurnitureGroup->mTiles.count() - 1)
            return;
        mUndoStack->push(new ReorderFurniture(this, mFurnitureGroup, index, index + 1));
    }

    if (mCurrentEntry) {
        int index = mCategory->indexOf(mCurrentEntry);
        if (index == mCategory->entryCount() - 1)
            return;
        mUndoStack->push(new ReorderEntry(this, mCategory, index, index + 1));
    }
}

void BuildingTilesDialog::toggleCorners()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTiles*> toggle;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *ftile = v->model()->tileAt(index);
            if (!toggle.contains(ftile->owner()))
                toggle += ftile->owner();
        }
        if (toggle.count() == 0)
            return;
        if (toggle.count() > 1)
            mUndoStack->beginMacro(tr("Toggle Corners"));
        foreach (FurnitureTiles *ftiles, toggle)
            mUndoStack->push(new ToggleFurnitureCorners(this, ftiles));
        if (toggle.count() > 1)
            mUndoStack->endMacro();
    }
}

void BuildingTilesDialog::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt())
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
}

void BuildingTilesDialog::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

void BuildingTilesDialog::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void BuildingTilesDialog::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

// Called when a tileset image changes or a missing tileset was found.
void BuildingTilesDialog::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        if (tileset->isMissing())
            ui->tilesetTilesView->clear();
        else
            ui->tilesetTilesView->setTileset(tileset);
    }

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesetList->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
}

void BuildingTilesDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void BuildingTilesDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void BuildingTilesDialog::tileSelectionChanged()
{
    if (mExpertMode)
        return;
    mCurrentEntry = 0;
    QModelIndex current = ui->categoryTilesView->currentIndex();
#if 1
    if (BuildingTileEntry *entry = ui->categoryTilesView->entry(current)) {
        mCurrentEntry = entry;
    }
#else
    MixedTilesetModel *m = ui->categoryTilesView->model();
    if (BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(m->userDataAt(current))) {
        mCurrentEntry = entry;
    }
#endif
    synchUI();
}

void BuildingTilesDialog::entrySelectionChanged()
{
    if (!mExpertMode)
        return;
    mCurrentEntry = 0;
    QModelIndex current = ui->categoryView->currentIndex();
    TileCategoryModel *m = ui->categoryView->model();
    if (BuildingTileEntry *entry = m->entryAt(current)) {
        mCurrentEntry = entry;
        mCurrentEntryEnum = m->enumAt(current);
    }
    synchUI();
}

void BuildingTilesDialog::furnitureSelectionChanged()
{
    mCurrentFurniture = 0;
    QModelIndex current = ui->furnitureView->currentIndex();
    if (FurnitureTile *ftile = ui->furnitureView->model()->tileAt(current)) {
        mCurrentFurniture = ftile;
    }
    synchUI();
}

void BuildingTilesDialog::tileActivated(const QModelIndex &index)
{
    MixedTilesetModel *m = ui->categoryTilesView->model();
    if (Tiled::Tile *tile = m->tileAt(index))
        displayTileInTileset(tile);
}

void BuildingTilesDialog::entryActivated(const QModelIndex &index)
{
    TileCategoryModel *m = ui->categoryView->model();
    if (BuildingTileEntry *entry = m->entryAt(index)) {
        int e = m->enumAt(index);
        displayTileInTileset(entry->tile(e));
    }
}

void BuildingTilesDialog::furnitureActivated(const QModelIndex &index)
{
    FurnitureModel *m = ui->furnitureView->model();
    if (FurnitureTile *ftile = m->tileAt(index)) {
        foreach (BuildingTile *btile, ftile->tiles()) {
            if (btile) {
                displayTileInTileset(btile);
                break;
            }
        }
    }
}

void BuildingTilesDialog::entryOffsetChanged()
{
    if (!mExpertMode || !mCurrentEntry || mSynching)
        return;
    QPoint offset(mEntryOffsetSpinX->value(), mEntryOffsetSpinY->value());
    if (offset != mCurrentEntry->offset(mCurrentEntryEnum)) {
        mUndoStack->push(new ChangeEntryOffset(this, mCurrentEntry,
                                               mCurrentEntryEnum, offset));
    }
}

void BuildingTilesDialog::furnitureLayerChanged(int index)
{
    if (!mCurrentFurniture || mSynching || index < 0)
        return;

    FurnitureTiles::FurnitureLayer layer =
            static_cast<FurnitureTiles::FurnitureLayer>(index);

    QList<FurnitureTiles*> ftilesList;
    FurnitureView *v = ui->furnitureView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        FurnitureTile *ftile = v->model()->tileAt(index);
        if (!ftilesList.contains(ftile->owner()) && (ftile->owner()->layer() != layer))
            ftilesList += ftile->owner();
    }
    if (ftilesList.count() == 0)
        return;
    if (ftilesList.count() > 1)
        mUndoStack->beginMacro(tr("Change Furniture Layer"));
    foreach (FurnitureTiles *ftiles, ftilesList)
        mUndoStack->push(new ChangeFurnitureLayer(this, ftiles, index));
    if (ftilesList.count() > 1)
        mUndoStack->endMacro();
}

void BuildingTilesDialog::furnitureGrimeChanged(bool allow)
{
    if (!mCurrentFurniture || mSynching)
        return;

    QList<FurnitureTile*> ftiles;
    FurnitureView *v = ui->furnitureView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        FurnitureTile *ftile = v->model()->tileAt(index);
        if (ftile->allowGrime() != allow)
            ftiles += ftile;
    }
    if (ftiles.count() == 0)
        return;
    if (ftiles.count() > 1)
        mUndoStack->beginMacro(tr("Change Furniture Layer"));
    foreach (FurnitureTile *ftile, ftiles)
        mUndoStack->push(new ChangeFurnitureGrime(this, ftile, allow));
    if (ftiles.count() > 1)
        mUndoStack->endMacro();
}

void BuildingTilesDialog::accept()
{
    if ((mChanges = !mUndoStack->isClean())) {
        BuildingTilesMgr::instance()->writeTxt(this);
        if (!FurnitureGroups::instance()->writeTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 FurnitureGroups::instance()->errorString());
        }
        if (!BuildingTMX::instance()->writeTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 BuildingTMX::instance()->errorString());
        }
        mUndoStack->setClean();

        emit edited();
    }

    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("SelectedCategory"),
                      mCategory ? mCategory->name() : QString());
    settings.setValue(QLatin1String("SelectedFurnitureGroup"),
                      mFurnitureGroup ? mFurnitureGroup->mLabel : QString());
    settings.setValue(QLatin1String("ExpertMode"), mExpertMode);
    settings.setValue(QLatin1String("SelectedTileset"),
                      mCurrentTileset ? mCurrentTileset->name() : QString());
    settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
    settings.endGroup();

    saveSplitterSizes(ui->overallSplitter);
    saveSplitterSizes(ui->categorySplitter);

    QDialog::accept();
}

void BuildingTilesDialog::reject()
{
    // There's no 'Cancel' button, so changes are always accepted.
    // But closing the dialog by the titlebar close-box or pressing the Escape
    // key calls reject().
    accept();
}
