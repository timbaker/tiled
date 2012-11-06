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
#include "furnituregroups.h"
#include "furnitureview.h"

#include "zoomable.h"

#include "tileset.h"
#include "utils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QScrollBar>
#include <QSettings>
#include <QToolBar>
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
                      const QString &tileName) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tile to Category")),
        mDialog(d),
        mCategory(category),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mDialog->removeTile(mCategory, mTileName);
    }

    void redo()
    {
        mDialog->addTile(mCategory, mTileName);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    QString mTileName;
};

class RemoveTileFromCategory : public QUndoCommand
{
public:
    RemoveTileFromCategory(BuildingTilesDialog *d, BuildingTileCategory *category,
                           const QString &tileName) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tile from Category")),
        mDialog(d),
        mCategory(category),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mDialog->addTile(mCategory, mTileName);
    }

    void redo()
    {
        mDialog->removeTile(mCategory, mTileName);
    }

    BuildingTilesDialog *mDialog;
    BuildingTileCategory *mCategory;
    QString mTileName;
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

class RenameCategory : public QUndoCommand
{
public:
    RenameCategory(BuildingTilesDialog *d, FurnitureGroup *category,
                   const QString &name) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Rename Category")),
        mDialog(d),
        mCategory(category),
        mName(name)
    {
    }

    void undo()
    {
        mName = mDialog->renameCategory(mCategory, mName);
    }

    void redo()
    {
        mName = mDialog->renameCategory(mCategory, mName);
    }

    BuildingTilesDialog *mDialog;
    FurnitureGroup *mCategory;
    QString mName;
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
    mChanges(false)
{
    ui->setupUi(this);

    QSettings settings;

    mZoomable->setScale(BuildingPreferences::instance()->tileScale());

    connect(ui->addTiles, SIGNAL(clicked()), SLOT(addTiles()));
    connect(ui->removeTiles, SIGNAL(clicked()), SLOT(removeTiles()));
    connect(ui->clearTiles, SIGNAL(clicked()), SLOT(clearTiles()));

    ui->tilesetView->setZoomable(mZoomable);
    ui->tilesetView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->furnitureView->model(),
            SIGNAL(furnitureTileDropped(FurnitureTile*,int,QString)),
            SLOT(furnitureTileDropped(FurnitureTile*,int,QString)));
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    setCategoryList();
    connect(ui->categoryList, SIGNAL(currentRowChanged(int)),
            SLOT(categoryChanged(int)));
    connect(ui->categoryList, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(categoryNameEdited(QListWidgetItem*)));
//    mCategory = BuildingTiles::instance()->categories().at(ui->categoryList->currentRow());

    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->listWidget->fontMetrics();
    foreach (Tileset *tileset, BuildingTiles::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        ui->listWidget->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    int sbw = ui->listWidget->verticalScrollBar()->sizeHint().width();
    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    ui->listWidget->setFixedWidth(width + 16 + sbw);

    connect(ui->listWidget, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));

    ui->tableWidget->setZoomable(mZoomable);
    ui->tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tableWidget->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(synchUI()));

    ui->tableWidget->setDragEnabled(true);

    /////
    QToolBar *toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionNewCategory);
    toolBar->addAction(ui->actionRemoveCategory);
    toolBar->addAction(ui->actionMoveCategoryUp);
    toolBar->addAction(ui->actionMoveCategoryDown);
    toolBar->addAction(ui->actionToggleCorners);
    connect(ui->actionRemoveCategory, SIGNAL(triggered()), SLOT(removeCategory()));
    connect(ui->actionNewCategory, SIGNAL(triggered()), SLOT(newCategory()));
    connect(ui->actionMoveCategoryUp, SIGNAL(triggered()), SLOT(moveCategoryUp()));
    connect(ui->actionMoveCategoryDown, SIGNAL(triggered()), SLOT(moveCategoryDown()));
    connect(ui->actionToggleCorners, SIGNAL(triggered()), SLOT(toggleCorners()));
    ui->categoryToolbarLayout->addWidget(toolBar);
    /////

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
    toolBar->addSeparator();
    toolBar->addAction(undoAction);
    toolBar->addAction(redoAction);
    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    synchUI();

    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
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
    return mChanges || !mUndoStack->isClean();
}

void BuildingTilesDialog::addTile(BuildingTileCategory *category, const QString &tileName)
{
    category->add(tileName);

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
}

void BuildingTilesDialog::removeTile(BuildingTileCategory *category, const QString &tileName)
{
    category->remove(tileName);

    tilesetSelectionChanged();
    setCategoryTiles();
    synchUI(); // model calling reset() doesn't generate selectionChanged signal
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
    QString old = ftile->mTiles[index] ? ftile->mTiles[index]->name() : QString();
    ftile->mTiles[index] = tileName.isEmpty() ? 0
                                              : BuildingTiles::instance()->getFurnitureTile(tileName);
    ui->furnitureView->update(ui->furnitureView->model()->index(ftile));
    return old;
}

QString BuildingTilesDialog::renameCategory(FurnitureGroup *category,
                                            const QString &name)
{
    QString old = category->mLabel;
    category->mLabel = name;
    int row = numTileCategories() + FurnitureGroups::instance()->indexOf(category);
    ui->categoryList->item(row)->setText(name);
    return old;
}

void BuildingTilesDialog::setCategoryList()
{
    ui->categoryList->clear();

    foreach (BuildingTileCategory *category, BuildingTiles::instance()->categories())
        ui->categoryList->addItem(category->label());

    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(group->mLabel);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        ui->categoryList->addItem(item);
    }
}

void BuildingTilesDialog::setCategoryTiles()
{
    QList<Tiled::Tile*> tiles;
    foreach (BuildingTile *tile, mCategory->tiles()) {
        if (!tile->mAlternates.count() || (tile == tile->mAlternates.first()))
            tiles += BuildingTiles::instance()->tileFor(tile);
    }
    ui->tilesetView->model()->setTiles(tiles);
}

void BuildingTilesDialog::setFurnitureTiles()
{
    ui->furnitureView->model()->setTiles(mFurnitureGroup->mTiles);
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
    return BuildingTiles::instance()->categoryCount();
}

void BuildingTilesDialog::synchUI()
{
    bool add = false;
    bool remove = false;
    if (mFurnitureGroup) {
        add = true;
        remove = ui->furnitureView->selectionModel()->selectedIndexes().count();
    } else if (mCategory) {
        add = ui->tableWidget->selectionModel()->selectedIndexes().count();
        remove = ui->tilesetView->selectionModel()->selectedIndexes().count();
    }
    ui->addTiles->setEnabled(add);
    ui->removeTiles->setEnabled(remove);
    ui->clearTiles->setEnabled(mFurnitureGroup && remove);

    ui->actionRemoveCategory->setEnabled(mFurnitureGroup != 0);
    ui->actionMoveCategoryUp->setEnabled(mFurnitureGroup != 0 &&
            mFurnitureGroup != FurnitureGroups::instance()->groups().first());
    ui->actionMoveCategoryDown->setEnabled(mFurnitureGroup != 0 &&
            mFurnitureGroup != FurnitureGroups::instance()->groups().last());
    ui->actionToggleCorners->setEnabled(mFurnitureGroup && remove);
}

void BuildingTilesDialog::categoryChanged(int index)
{
    mCategory = 0;
    mFurnitureGroup = 0;
    if (index < 0) {
        // only happens when setting the list again
    } else if (index < numTileCategories()) {
        mCategory = BuildingTiles::instance()->categories().at(index);
        ui->categoryStack->setCurrentIndex(0);
        setCategoryTiles();
        tilesetSelectionChanged();
    } else {
        index -= numTileCategories();
        mFurnitureGroup = FurnitureGroups::instance()->groups().at(index);
        setFurnitureTiles();
        ui->categoryStack->setCurrentIndex(1);
    }
    synchUI();
}

void BuildingTilesDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->listWidget->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        QRect bounds;
        if (mCategory)
            bounds = mCategory->categoryBounds();

        int row = ui->listWidget->row(item);
        MixedTilesetModel *model = ui->tableWidget->model();
        Tileset *tileset = BuildingTiles::instance()->tilesets().at(row);
        model->setTileset(tileset);
        for (int i = 0; i < tileset->tileCount(); i++) {
            Tile *tile = tileset->tileAt(i);
            if (mCategory && mCategory->usesTile(tile))
                model->setCategoryBounds(tile, bounds);
        }
    }
    synchUI();
}

void BuildingTilesDialog::addTiles()
{
    if (mFurnitureGroup != 0) {
        FurnitureTiles *tiles = new FurnitureTiles;
        tiles->mTiles[0] = new FurnitureTile(tiles, FurnitureTile::FurnitureW);
        tiles->mTiles[1] = new FurnitureTile(tiles, FurnitureTile::FurnitureN);
        tiles->mTiles[2] = new FurnitureTile(tiles, FurnitureTile::FurnitureE);
        tiles->mTiles[3] = new FurnitureTile(tiles, FurnitureTile::FurnitureS);
        mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup,
                                               mFurnitureGroup->mTiles.count(),
                                               tiles));
        return;
    }

    if (!mCategory)
        return;

    QModelIndexList selection = ui->tableWidget->selectionModel()->selectedIndexes();
    QList<Tile*> tiles;
    foreach (QModelIndex index, selection) {
        Tile *tile = ui->tableWidget->model()->tileAt(index);
        if (!mCategory->usesTile(tile))
            tiles += tile;
    }
    if (tiles.isEmpty())
        return;
    if (tiles.count() > 1)
        mUndoStack->beginMacro(tr("Add Tiles To %1").arg(mCategory->label()));
    foreach (Tile *tile, tiles) {
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        mUndoStack->push(new AddTileToCategory(this, mCategory, tileName));
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

    MixedTilesetView *v = ui->tilesetView;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    if (selection.count() > 1)
        mUndoStack->beginMacro(tr("Remove Tiles from %1").arg(mCategory->label()));
    foreach (QModelIndex index, selection) {
        Tile *tile = v->model()->tileAt(index);
        QString tileName = BuildingTiles::instance()->nameForTile(tile);
        mUndoStack->push(new RemoveTileFromCategory(this, mCategory, tileName));
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
                if (ftile->mTiles[i])
                    mUndoStack->push(new ChangeFurnitureTile(this, ftile, i));
            }
        }
        mUndoStack->endMacro();
        return;
    }
}

void BuildingTilesDialog::furnitureTileDropped(FurnitureTile *ftile, int index,
                                               const QString &tileName)
{
    mUndoStack->push(new ChangeFurnitureTile(this, ftile, index, tileName));
}

void BuildingTilesDialog::categoryNameEdited(QListWidgetItem *item)
{
    int row = ui->categoryList->row(item);
    row -= numTileCategories();
    FurnitureGroup *category = FurnitureGroups::instance()->groups().at(row);
    if (item->text() != category->mLabel)
        mUndoStack->push(new RenameCategory(this, category, item->text()));
}

void BuildingTilesDialog::newCategory()
{
    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("New Category");
    mUndoStack->push(new AddCategory(this, FurnitureGroups::instance()->groups().count(),
                                     group));
    ui->categoryList->setCurrentRow(ui->categoryList->count() - 1);
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
    FurnitureGroups::instance()->removeGroup(index);
    FurnitureGroups::instance()->insertGroup(index - 1, mFurnitureGroup);

    setCategoryList();
    ui->categoryList->setCurrentRow(numTileCategories() + index - 1);

    mChanges = true;
}

void BuildingTilesDialog::moveCategoryDown()
{
    if (!mFurnitureGroup)
         return;
    int index = FurnitureGroups::instance()->indexOf(mFurnitureGroup);
    if (index == FurnitureGroups::instance()->groupCount() - 1)
        return;
    FurnitureGroups::instance()->removeGroup(index);
    FurnitureGroups::instance()->insertGroup(index + 1, mFurnitureGroup);

    setCategoryList();
    ui->categoryList->setCurrentRow(numTileCategories() + index + 1);

    mChanges = true;
}

void BuildingTilesDialog::toggleCorners()
{
    if (mFurnitureGroup != 0) {
        FurnitureView *v = ui->furnitureView;
        QList<FurnitureTiles*> toggle;
        QModelIndexList selection = v->selectionModel()->selectedIndexes();
        foreach (QModelIndex index, selection) {
            FurnitureTile *tile = v->model()->tileAt(index);
            if (!toggle.contains(tile->owner()))
                toggle += tile->owner();
        }
        foreach (FurnitureTiles *tiles, toggle) {
            tiles->toggleCorners();
            v->update(v->model()->index(tiles->mTiles[0]));
            v->update(v->model()->index(tiles->mTiles[1]));
            v->update(v->model()->index(tiles->mTiles[2]));
            v->update(v->model()->index(tiles->mTiles[3]));
        }
        mChanges = true;
        return;
    }
}

void BuildingTilesDialog::accept()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("BuildingEditor/BuildingTilesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();

    saveSplitterSizes(ui->overallSplitter);
    saveSplitterSizes(ui->categorySplitter);
    saveSplitterSizes(ui->tilesetSplitter);

    QDialog::accept();
}
