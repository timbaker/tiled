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

#include "zoomable.h"

#include "tile.h"
#include "tileset.h"
#include "utils.h"

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


class ChangeFurnitureTile : public QUndoCommand
{
public:
    ChangeFurnitureTile(BuildingTilesDialog *d, FurnitureTile *ftile,
                        int index, const QString &tileName = QString()) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Tile")),
        mDialog(d),
        mTile(ftile),
        mIndex(index),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mIndex, mTileName);
    }

    void redo()
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mIndex, mTileName);
    }

    BuildingTilesDialog *mDialog;
    FurnitureTile *mTile;
    int mIndex;
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

class AddBuildingTileset : public QUndoCommand
{
public:
    AddBuildingTileset(BuildingTilesDialog *d, Tiled::Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tileset")),
        mDialog(d),
        mTileset(tileset)
    {
    }

    ~AddBuildingTileset()
    {
    }

    void undo()
    {
        mDialog->removeTileset(mTileset);
    }

    void redo()
    {
        mDialog->addTileset(mTileset);
    }

    BuildingTilesDialog *mDialog;
    Tiled::Tileset *mTileset;
};

class RemoveBuildingTileset : public QUndoCommand
{
public:
    RemoveBuildingTileset(BuildingTilesDialog *d, Tiled::Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tileset")),
        mDialog(d),
        mTileset(tileset)
    {
    }

    ~RemoveBuildingTileset()
    {
    }

    void undo()
    {
        mDialog->addTileset(mTileset);
    }

    void redo()
    {
        mDialog->removeTileset(mTileset);
    }

    BuildingTilesDialog *mDialog;
    Tiled::Tileset *mTileset;
};

} // namespace BuildingEditor

/////

BuildingTilesDialog::BuildingTilesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTilesDialog),
    mZoomable(new Zoomable(this)),
    mCategory(0),
    mFurnitureGroup(0),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this)),
    mSynching(false),
    mExpertMode(false)
{
    ui->setupUi(this);

    QSettings settings;
    mExpertMode = settings.value(QLatin1String("BuildingEditor/BuildingTilesDialog/ExpertMode"), false).toBool();
    ui->actionExpertMode->setChecked(mExpertMode);

    mZoomable->setScale(BuildingPreferences::instance()->tileScale());

    ui->categoryTilesView->setZoomable(mZoomable);
    ui->categoryTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->categoryTilesView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));
    connect(ui->categoryTilesView, SIGNAL(activated(QModelIndex)),
            SLOT(tileActivated(QModelIndex)));

    ui->categoryView->setZoomable(mZoomable);
    ui->categoryView->setAcceptDrops(true);
    ui->categoryView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->categoryView->model(), SIGNAL(tileDropped(BuildingTileEntry*,int,QString)),
            SLOT(entryTileDropped(BuildingTileEntry*,int,QString)));
    connect(ui->categoryView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));
    connect(ui->categoryView, SIGNAL(activated(QModelIndex)),
            SLOT(entryActivated(QModelIndex)));

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->furnitureView->model(),
            SIGNAL(furnitureTileDropped(FurnitureTile*,int,QString)),
            SLOT(furnitureTileDropped(FurnitureTile*,int,QString)));
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    connect(ui->categoryList, SIGNAL(currentRowChanged(int)),
            SLOT(categoryChanged(int)));
    connect(ui->categoryList, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(categoryNameEdited(QListWidgetItem*)));
//    mCategory = BuildingTiles::instance()->categories().at(ui->categoryList->currentRow());

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(ui->tilesetList, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));

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

    /////
    toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionToggleCorners);
    toolBar->addAction(ui->actionClearTiles);
    toolBar->addAction(ui->actionMoveFurnitureUp);
    toolBar->addAction(ui->actionMoveFurnitureDown);
    toolBar->addSeparator();
    toolBar->addAction(ui->actionAddTiles);
    toolBar->addAction(ui->actionRemoveTiles);
    toolBar->addAction(ui->actionExpertMode);
    connect(ui->actionToggleCorners, SIGNAL(triggered()), SLOT(toggleCorners()));
    connect(ui->actionClearTiles, SIGNAL(triggered()), SLOT(clearTiles()));
    connect(ui->actionMoveFurnitureUp, SIGNAL(triggered()), SLOT(moveFurnitureUp()));
    connect(ui->actionMoveFurnitureDown, SIGNAL(triggered()), SLOT(moveFurnitureDown()));
    connect(ui->actionAddTiles, SIGNAL(triggered()), SLOT(addTiles()));
    connect(ui->actionRemoveTiles, SIGNAL(triggered()), SLOT(removeTiles()));
    connect(ui->actionExpertMode, SIGNAL(toggled(bool)), SLOT(setExpertMode(bool)));
    ui->categoryToolbarLayout->addWidget(toolBar);
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
    ui->buttonsLayout->addWidget(button);
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
    ui->buttonsLayout->addWidget(button);
    connect(mUndoGroup, SIGNAL(canRedoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), redoAction, SIGNAL(triggered()));

    /////
    toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddTilesets);
    toolBar->addAction(ui->actionRemoveTileset);
    ui->tilesetToolbarLayout->addWidget(toolBar);
    connect(ui->actionAddTilesets, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionRemoveTileset, SIGNAL(triggered()), SLOT(removeTileset()));
    /////

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
            ui->categoryList->setCurrentRow(index);
    }
    QString furnitureGroupName = settings.value(QLatin1String("SelectedFurnitureGroup")).toString();
    if (!furnitureGroupName.isEmpty()) {
        int index = FurnitureGroups::instance()->indexOf(furnitureGroupName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(numTileCategories() + index);
    }
    settings.endGroup();

    restoreSplitterSizes(ui->overallSplitter);
    restoreSplitterSizes(ui->categorySplitter);
    restoreSplitterSizes(ui->tilesetSplitter);
}

BuildingTilesDialog::~BuildingTilesDialog()
{
    delete ui;
}

bool BuildingTilesDialog::changes() const
{
    return !mUndoStack->isClean();
}

void BuildingTilesDialog::addTile(BuildingTileCategory *category,
                                  int index, BuildingTileEntry *entry)
{
    // Create a new BuildingTileEntry based on assumptions about the order of
    // tiles in the tileset.
    category->insertEntry(index, entry);

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}

BuildingTileEntry *BuildingTilesDialog::removeTile(BuildingTileCategory *category, int index)
{
    BuildingTileEntry *entry = category->removeEntry(index);

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal

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
    int row = numTileCategories() + index;
    delete ui->categoryList->item(row);
    FurnitureGroup *group = FurnitureGroups::instance()->removeGroup(index);
    if (group == mFurnitureGroup)
        mFurnitureGroup = 0;
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

void BuildingTilesDialog::insertFurnitureTiles(FurnitureGroup *category,
                                               int index,
                                               FurnitureTiles *ftiles)
{
    category->mTiles.insert(index, ftiles);
    ui->furnitureView->model()->setTiles(category->mTiles);
}

FurnitureTiles *BuildingTilesDialog::removeFurnitureTiles(FurnitureGroup *category,
                                                          int index)
{
    FurnitureTiles *ftiles = category->mTiles.takeAt(index);
    ui->furnitureView->model()->removeTiles(ftiles);
    return ftiles;
}

QString BuildingTilesDialog::changeFurnitureTile(FurnitureTile *ftile,
                                                 int index,
                                                 const QString &tileName)
{
    QString old = ftile->tile(index) ? ftile->tile(index)->name() : QString();
    ftile->setTile(index, tileName.isEmpty()
                   ? 0 : BuildingTilesMgr::instance()->get(tileName));

    FurnitureGroups::instance()->tileChanged(ftile);

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

QString BuildingTilesDialog::renameFurnitureCategory(FurnitureGroup *category,
                                            const QString &name)
{
    QString old = category->mLabel;
    category->mLabel = name;
    int row = numTileCategories() + FurnitureGroups::instance()->indexOf(category);
    ui->categoryList->item(row)->setText(name);
    return old;
}

void BuildingTilesDialog::reorderCategory(int oldIndex, int newIndex)
{
    FurnitureGroup *category = FurnitureGroups::instance()->removeGroup(oldIndex);
    FurnitureGroups::instance()->insertGroup(newIndex, category);

    setCategoryList();
    ui->categoryList->setCurrentRow(numTileCategories() + newIndex);
}

void BuildingTilesDialog::addTileset(Tileset *tileset)
{
    BuildingTilesMgr::instance()->addTileset(tileset);

    categoryChanged(ui->categoryList->currentRow());
    setTilesetList();
    synchUI();
}

void BuildingTilesDialog::removeTileset(Tileset *tileset)
{
    ui->categoryTilesView->model()->setTiles(QList<Tiled::Tile*>());

    BuildingTilesMgr::instance()->removeTileset(tileset);

    categoryChanged(ui->categoryList->currentRow());
    setTilesetList();
    synchUI();
}

void BuildingTilesDialog::setCategoryList()
{
    ui->categoryList->clear();

    foreach (BuildingTileCategory *category, BuildingTilesMgr::instance()->categories()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(category->label());
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->categoryList->addItem(item);
    }

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
    QList<Tiled::Tile*> tiles;
    QList<void*> userData;
    if (mCategory && !expertMode) {
        QMap<QString,BuildingTileEntry*> entryMap;
        foreach (BuildingTileEntry *entry, mCategory->entries()) {
            QString key = entry->displayTile()->name() + QString::number((qulonglong)entry);
            entryMap[key] = entry;
        }
        foreach (BuildingTileEntry *entry, entryMap.values()) {
            if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                tiles += tile;
                userData += entry;
            }
        }
    }
    ui->categoryTilesView->model()->setTiles(tiles, userData);
    ui->categoryView->model()->setCategory(expertMode ? mCategory : 0);
}

void BuildingTilesDialog::setFurnitureTiles()
{
    QList<FurnitureTiles*> ftiles;
    if (mFurnitureGroup)
        ftiles = mFurnitureGroup->mTiles;
    ui->furnitureView->model()->setTiles(ftiles);
}

void BuildingTilesDialog::setTilesetList()
{
    ui->tilesetList->clear();
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    foreach (Tileset *tileset, BuildingTilesMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setMinimumWidth(width + 16 + sbw);
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

int BuildingTilesDialog::numTileCategories() const
{
    return BuildingTilesMgr::instance()->categoryCount();
}

void BuildingTilesDialog::displayTileInTileset(Tiled::Tile *tile)
{
    if (!tile)
        return;
    int row = BuildingTilesMgr::instance()->tilesets().indexOf(tile->tileset());
    if (row >= 0) {
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void BuildingTilesDialog::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
}

void BuildingTilesDialog::synchUI()
{
    bool add = false;
    bool remove = false;
    bool clear = false;
    if (mFurnitureGroup) {
        add = true;
        remove = ui->furnitureView->selectionModel()->selectedIndexes().count();
        clear = remove;
    } else if (mCategory) {
        if (mExpertMode && !mCategory->shadowImage().isNull()) {
            add = true;
            remove = ui->categoryView->selectionModel()->selectedIndexes().count();
            clear = remove;
            TileCategoryModel *m = ui->categoryView->model();
            QModelIndex current = ui->categoryView->currentIndex();
            if (BuildingTileEntry *entry = m->entryAt(current)) {
                int e = m->enumAt(current);
                mSynching = true;
                mEntryOffsetSpinX->setValue(entry->offset(e).x());
                mEntryOffsetSpinY->setValue(entry->offset(e).y());
                mSynching = false;
            }
        } else {
            add = ui->tilesetTilesView->selectionModel()->selectedIndexes().count();
            remove = ui->categoryTilesView->selectionModel()->selectedIndexes().count();
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
    mEntryOffsetUI->setEnabled(clear); // valid selection

    QModelIndex current = ui->furnitureView->currentIndex();
    int index = current.isValid() ? current.row() : -1;
    ui->actionMoveFurnitureUp->setEnabled(mFurnitureGroup && (index > 0));
    ui->actionMoveFurnitureDown->setEnabled(mFurnitureGroup && (index >= 0) &&
                                            (index < mFurnitureGroup->mTiles.count()));

    ui->actionRemoveTileset->setEnabled(ui->tilesetList->currentRow() >= 0);
}

void BuildingTilesDialog::categoryChanged(int index)
{
    mCategory = 0;
    mFurnitureGroup = 0;
    if (index < 0) {
        // only happens when setting the list again
        setCategoryTiles();
        setFurnitureTiles();
        tilesetSelectionChanged();
    } else if (index < numTileCategories()) {
        mCategory = BuildingTilesMgr::instance()->category(index);
        if (mExpertMode && !mCategory->shadowImage().isNull()) {
            ui->categoryStack->setCurrentIndex(2);
        } else {
            ui->categoryStack->setCurrentIndex(0);
        }
        setCategoryTiles();
        tilesetSelectionChanged();
    } else {
        index -= numTileCategories();
        mFurnitureGroup = FurnitureGroups::instance()->group(index);
        setFurnitureTiles();
        ui->categoryStack->setCurrentIndex(1);
    }
    synchUI();
}

void BuildingTilesDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
#if 0
        QRect bounds;
        if (mCategory)
            bounds = mCategory->categoryBounds();
#endif
        int row = ui->tilesetList->row(item);
        MixedTilesetModel *model = ui->tilesetTilesView->model();
        Tileset *tileset = BuildingTilesMgr::instance()->tilesets().at(row);
        model->setTileset(tileset);
#if 0
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (mCategory && mCategory->usesTile(tile))
                model->setCategoryBounds(tile, bounds);
        }
#endif
    } else {
        ui->tilesetTilesView->model()->setTiles(QList<Tile*>());
    }
    synchUI();
}

void BuildingTilesDialog::addTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureTiles *tiles = new FurnitureTiles(false);
        mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup,
                                               mFurnitureGroup->mTiles.count(),
                                               tiles));
        return;
    }

    if (!mCategory)
        return;

    if (mExpertMode && !mCategory->shadowImage().isNull()) {
        // Create a new blank entry in the category.
        BuildingTileEntry *entry = new BuildingTileEntry(mCategory);
        mUndoStack->push(new AddTileToCategory(this, mCategory,
                                               mCategory->entryCount(), entry));
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

    MixedTilesetView *v = ui->categoryTilesView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    if (selection.count() > 1)
        mUndoStack->beginMacro(tr("Remove Tiles from %1").arg(mCategory->label()));
    foreach (QModelIndex index, selection) {
        BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(
                    v->model()->userDataAt(index));
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
            for (int i = 0; i < 4; i++) {
                if (ftile->tile(i))
                    mUndoStack->push(new ChangeFurnitureTile(this, ftile, i));
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
        categoryChanged(ui->categoryList->currentRow());
    }
}

void BuildingTilesDialog::entryTileDropped(BuildingTileEntry *entry, int e, const QString &tileName)
{
    mUndoStack->push(new ChangeEntryTile(this, entry, e, tileName));
}

void BuildingTilesDialog::furnitureTileDropped(FurnitureTile *ftile, int index,
                                               const QString &tileName)
{
    mUndoStack->push(new ChangeFurnitureTile(this, ftile, index, tileName));
}

void BuildingTilesDialog::categoryNameEdited(QListWidgetItem *item)
{
    int row = ui->categoryList->row(item);
    if (row < numTileCategories()) {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(row);
        if (item->text() != category->label())
            mUndoStack->push(new RenameTileCategory(this, category, item->text()));
        return;
    }
    row -= numTileCategories();
    FurnitureGroup *category = FurnitureGroups::instance()->group(row);
    if (item->text() != category->mLabel)
        mUndoStack->push(new RenameFurnitureCategory(this, category, item->text()));
}

void BuildingTilesDialog::newCategory()
{
    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("New Category");
    int index = mFurnitureGroup
            ? FurnitureGroups::instance()->indexOf(mFurnitureGroup) + 1
            : FurnitureGroups::instance()->groupCount();
    mUndoStack->push(new AddCategory(this, index, group));

    QListWidgetItem *item = ui->categoryList->item(numTileCategories() + index);
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

void BuildingTilesDialog::moveFurnitureUp()
{
    if (!mFurnitureGroup)
         return;
    FurnitureView *v = ui->furnitureView;
    QModelIndex current = v->currentIndex();
    if (!current.isValid())
        return;
    FurnitureTiles *ftiles = v->model()->tileAt(current)->owner();
    int index = mFurnitureGroup->mTiles.indexOf(ftiles);
    if (index == 0)
        return;
    mUndoStack->push(new ReorderFurniture(this, mFurnitureGroup, index, index - 1));
}

void BuildingTilesDialog::moveFurnitureDown()
{
    if (!mFurnitureGroup)
         return;
    FurnitureView *v = ui->furnitureView;
    QModelIndex current = v->currentIndex();
    if (!current.isValid())
        return;
    FurnitureTiles *ftiles = v->model()->tileAt(current)->owner();
    int index = mFurnitureGroup->mTiles.indexOf(ftiles);
    if (index == mFurnitureGroup->mTiles.count() - 1)
        return;
    mUndoStack->push(new ReorderFurniture(this, mFurnitureGroup, index, index + 1));
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

void BuildingTilesDialog::addTileset()
{
    const QString tilesDir = BuildingPreferences::instance()->tilesDirectory();
    const QString filter = Utils::readableImageFormatsFilter();
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Tileset Image"),
                                                          tilesDir,
                                                          filter);
    QFileInfoList add;
    foreach (QString f, fileNames) {
        QFileInfo info(f);
        QString name = info.completeBaseName();
        if (BuildingTilesMgr::instance()->tilesetFor(name))
            continue; // FIXME: duplicate tileset names not allowed, even in different directories
        add += info;
    }
    if (add.count() > 1)
        mUndoStack->beginMacro(tr("Add Tilesets"));
    foreach (QFileInfo info, add) {
        if (Tiled::Tileset *ts = BuildingTMX::instance()->loadTileset(info.canonicalFilePath()))
            mUndoStack->push(new AddBuildingTileset(this, ts));
        else {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 BuildingTMX::instance()->errorString());
        }
    }
    if (add.count() > 1)
        mUndoStack->endMacro();
}

void BuildingTilesDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->tilesetList->row(item);
        Tileset *tileset = BuildingTilesMgr::instance()->tilesets().at(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?").arg(tileset->name()),
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        mUndoStack->push(new RemoveBuildingTileset(this, tileset));
    }
}

void BuildingTilesDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void BuildingTilesDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
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

void BuildingTilesDialog::entryOffsetChanged()
{
    if (!mCategory || mSynching)
        return;
    QModelIndex index = ui->categoryView->currentIndex();
    TileCategoryModel *m = ui->categoryView->model();
    if (BuildingTileEntry *entry = m->entryAt(index)) {
        int e = m->enumAt(index);
        QPoint offset(mEntryOffsetSpinX->value(), mEntryOffsetSpinY->value());
        if (offset != entry->offset(e)) {
            mUndoStack->push(new ChangeEntryOffset(this, entry, e, offset));
        }
    }
}

void BuildingTilesDialog::accept()
{
    if (changes()) {
        BuildingTilesMgr::instance()->writeTxt(this);
        if (!FurnitureGroups::instance()->writeTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 FurnitureGroups::instance()->errorString());
        }
        if (!BuildingTMX::instance()->writeTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 BuildingTMX::instance()->errorString());
        }
    }

    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("SelectedCategory"),
                      mCategory ? mCategory->name() : QString());
    settings.setValue(QLatin1String("SelectedFurnitureGroup"),
                      mFurnitureGroup ? mFurnitureGroup->mLabel : QString());
    settings.setValue(QLatin1String("ExpertMode"), mExpertMode);
    settings.endGroup();

    saveSplitterSizes(ui->overallSplitter);
    saveSplitterSizes(ui->categorySplitter);
    saveSplitterSizes(ui->tilesetSplitter);

    QDialog::accept();
}
