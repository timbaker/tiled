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

class BuildingTilesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingTilesDialog(QWidget *parent = 0);
    ~BuildingTilesDialog();

    bool changes() const
    { return mChanges; }
    
private:
    void setCategoryList();
    void setCategoryTiles();
    void setFurnitureTiles();
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

private slots:
    void categoryChanged(int index);
    void tilesetSelectionChanged();
    void synchUI();

    void addTiles();
    void removeTiles();

    void furnitureEdited();

    void categoryNameEdited(QListWidgetItem *item);

    void newCategory();
    void removeCategory();
    void toggleCorners();

    void accept();

private:
    Ui::BuildingTilesDialog *ui;
    Tiled::Internal::Zoomable *mZoomable;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    bool mChanges;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESDIALOG_H
