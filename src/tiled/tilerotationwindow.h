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

#include "tilerotation.h"

#include <QMainWindow>

namespace Ui {
class TileRotationWindow;
}

namespace BuildingEditor {
class BuildingTile;
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
    bool fileOpen(const QString &fileName, QList<Tiled::TilesetRotated*> &tiles, QMap<QString, QString> &mapping);
    bool fileSave(const QString &fileName, const QList<Tiled::TilesetRotated*> &tilesets, const QMap<QString, QString>& mapping);
    void setTilesetRotatedList();
    void setTilesetList();
    void updateUsedTiles();
    bool isTileUsed(const QString &_tileName);
    void displayTileInTileset(Tiled::Tile *tile);
    void displayTileInTileset(BuildingEditor::BuildingTile *tile);
    void addTile(Tiled::TileRotated *tile, int index, const QString& tileName);
    QString removeTile(Tiled::TileRotated *tile, int index);
    QStringList changeTiles(Tiled::TileRotated *tile, int index, const QStringList& tileNames);

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
    void tileRotatedActivated(const QModelIndex &index);
    void tilesetRotatedSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tiled::Tileset *tileset);
    void tileDropped(Tiled::TileRotated *tile, int x, const QString &tileName);
    void typeComboActivated(int index);

private:
    Ui::TileRotationWindow *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    Tiled::TilesetRotated *mCurrentTilesetRotated;
    Tiled::Tileset *mCurrentTileset;
    QString mHoverTileName;
    QString mError;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QList<Tiled::TilesetRotated*> mTilesets;
    QMap<QString, QString> mMapping;

    friend class AddTile;
    friend class ChangeTiles;
};

#endif // TILEROTATIONWINDOW_H
