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

#ifndef BUILDINGTILESDIALOG_H
#define BUILDINGTILESDIALOG_H

#include <QDialog>
#include <QModelIndex>

class QListWidgetItem;
class QSplitter;
class QToolButton;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class BuildingTilesDialog;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingTile;
class BuildingTileEntry;
class BuildingTileCategory;
class FurnitureGroup;
class FurnitureTile;
class FurnitureTiles;

class BuildingTilesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingTilesDialog(QWidget *parent = 0);
    ~BuildingTilesDialog();

    bool changes() const;

    //+ UNDO/REDO
    void addTile(BuildingTileCategory *category, int index, BuildingTileEntry *entry);
    BuildingTileEntry *removeTile(BuildingTileCategory *category, int index);
    QString renameTileCategory(BuildingTileCategory *category, const QString &name);

    void addCategory(int index, FurnitureGroup *category);
    FurnitureGroup *removeCategory(int index);
    QString changeEntryTile(BuildingTileEntry *entry, int e, const QString &tileName);

    void insertFurnitureTiles(FurnitureGroup *category, int index,
                              FurnitureTiles *ftiles);
    FurnitureTiles* removeFurnitureTiles(FurnitureGroup *category, int index);
    QString changeFurnitureTile(FurnitureTile *ftile, int index,
                                const QString &tileName);
    void reorderFurniture(FurnitureGroup *category, int oldIndex, int newIndex);
    void toggleCorners(FurnitureTiles *ftiles);

    QString renameFurnitureCategory(FurnitureGroup *category, const QString &name);
    void reorderCategory(int oldIndex, int newIndex);

    void addTileset(Tiled::Tileset *tileset);
    void removeTileset(Tiled::Tileset *tileset);
    //- UNDO/REDO

private:
    void setCategoryList();
    void setCategoryTiles();
    void setFurnitureTiles();
    void setTilesetList();
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);
    int numTileCategories() const;
    void displayTileInTileset(Tiled::Tile *tile);
    void displayTileInTileset(BuildingTile *tile);

private slots:
    void categoryChanged(int index);
    void tilesetSelectionChanged();
    void synchUI();

    void addTiles();
    void removeTiles();
    void clearTiles();

    void setExpertMode(bool expert);

    void entryTileDropped(BuildingTileEntry *entry, int e, const QString &tileName);

    void furnitureTileDropped(FurnitureTile *ftile, int index,
                              const QString &tileName);

    void categoryNameEdited(QListWidgetItem *item);

    void newCategory();
    void removeCategory();

    void moveCategoryUp();
    void moveCategoryDown();

    void moveFurnitureUp();
    void moveFurnitureDown();

    void toggleCorners();

    void addTileset();
    void removeTileset();

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void tileActivated(const QModelIndex &index);
    void entryActivated(const QModelIndex &index);

    void accept();

private:
    Ui::BuildingTilesDialog *ui;
    Tiled::Internal::Zoomable *mZoomable;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
    bool mExpertMode;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESDIALOG_H
