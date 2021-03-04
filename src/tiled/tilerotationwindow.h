/*
 * tilerotationwindow.h
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

#ifndef TILEROTATIONWINDOW_H
#define TILEROTATIONWINDOW_H

#include <QMainWindow>

namespace Ui {
class TileRotationWindow;
}

namespace BuildingEditor {
class BuildingTile;
class FurnitureGroup;
class FurnitureTile;
class FurnitureTiles;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

class QUndoGroup;
class QUndoStack;

class TileRotationWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TileRotationWindow(QWidget *parent = nullptr);
    ~TileRotationWindow();

protected:
    void closeEvent(QCloseEvent *event);
    bool confirmSave();
    QString getSaveLocation();
    void fileOpen(const QString &fileName);
    bool fileSave(const QString &fileName);
    bool fileOpen(const QString &fileName, QList<BuildingEditor::FurnitureTiles*> &tiles, QStringList& noRotateTileNames);
    bool fileSave(const QString &fileName, const QList<BuildingEditor::FurnitureTiles*> &tiles, const QStringList& noRotateTileNames);
    void setTilesetList();
    void updateUsedTiles();
    bool isTileUsed(const QString &_tileName);
    void displayTileInTileset(Tiled::Tile *tile);
    void displayTileInTileset(BuildingEditor::BuildingTile *tile);
    void insertFurnitureTiles(BuildingEditor::FurnitureGroup *group, int index, BuildingEditor::FurnitureTiles *ftiles);
    BuildingEditor::FurnitureTiles* removeFurnitureTiles(BuildingEditor::FurnitureGroup *group, int index);
    QString changeFurnitureTile(BuildingEditor::FurnitureTile *ftile, int x, int y, const QString &tileName);

protected slots:
    void fileNew();
    void fileOpen();
    bool fileSave();
    bool fileSaveAs();
    void addTiles();
    void clearTiles();
    void removeTiles();
    void updateWindowTitle();
    void syncUI();
    void filterEdited(const QString &text);
    void tileActivated(const QModelIndex &index);
    void tilesetSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tiled::Tileset *tileset);
    void furnitureTileDropped(BuildingEditor::FurnitureTile *ftile, int x, int y, const QString &tileName);
    void furnitureActivated(const QModelIndex &index);

private:
    Ui::TileRotationWindow *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    Tiled::Tileset *mCurrentTileset;
    QString mHoverTileName;
    QString mError;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    BuildingEditor::FurnitureGroup* mFurnitureGroup;
    QStringList mNoRotateTileNames;

    friend class AddFurnitureTiles;
    friend class ChangeFurnitureTile;
    friend class RemoveFurnitureTiles;
};

#endif // TILEROTATIONWINDOW_H
