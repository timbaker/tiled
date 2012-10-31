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

class QListWidgetItem;
class QSplitter;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class BuildingTilesDialog;
}

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

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

    void addTile(BuildingTileCategory *category, const QString &tileName);
    void removeTile(BuildingTileCategory *category, const QString &tileName);

    void addCategory(int index, FurnitureGroup *category);
    FurnitureGroup *removeCategory(int index);

    void insertFurnitureTiles(FurnitureGroup *category, int index,
                              FurnitureTiles *ftiles);
    FurnitureTiles* removeFurnitureTiles(FurnitureGroup *category, int index);

    QString changeFurnitureTile(FurnitureTile *ftile, int index,
                                const QString &tileName);

    QString renameCategory(FurnitureGroup *category, const QString &name);

private:
    void setCategoryList();
    void setCategoryTiles();
    void setFurnitureTiles();
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);
    int numTileCategories() const;

private slots:
    void categoryChanged(int index);
    void tilesetSelectionChanged();
    void synchUI();

    void addTiles();
    void removeTiles();
    void clearTiles();

    void furnitureTileDropped(FurnitureTile *ftile, int index,
                              const QString &tileName);

    void categoryNameEdited(QListWidgetItem *item);

    void newCategory();
    void removeCategory();

    void moveCategoryUp();
    void moveCategoryDown();

    void toggleCorners();

    void accept();

private:
    Ui::BuildingTilesDialog *ui;
    Tiled::Internal::Zoomable *mZoomable;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    bool mChanges;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESDIALOG_H
