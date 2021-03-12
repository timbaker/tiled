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
class TileRotateDelegate;
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
    bool fileOpen(const QString &fileName, QList<Tiled::TilesetRotated*> &tiles, QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals);
    bool fileSave(const QString &fileName, const QList<Tiled::TilesetRotated*> &tilesets, const QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals);
    void setVisualList();
    void setTilesetList();
    void updateUsedTiles();
    bool isTileUsed(const QString &_tileName);
    void displayTileInTileset(Tiled::Tile *tile);
    void displayTileInTileset(BuildingEditor::BuildingTile *tile);
    void addVisual(QSharedPointer<Tiled::TileRotatedVisual> visual, int index);
    QSharedPointer<Tiled::TileRotatedVisual> removeVisual(int index);
    struct AssignedVisual
    {
        AssignedVisual(Tiled::TileRotated *tileR)
        {
            mVisual = tileR->mVisual;
            mMapRotation = tileR->mRotation;
        }
        AssignedVisual(QSharedPointer<Tiled::TileRotatedVisual> visual, Tiled::MapRotation mapRotation)
        {
            mVisual = visual;
            mMapRotation = mapRotation;
        }
        QSharedPointer<Tiled::TileRotatedVisual> mVisual;
        Tiled::MapRotation mMapRotation;
    };

    AssignedVisual assignVisual(Tiled::TileRotated *tileR, QSharedPointer<Tiled::TileRotatedVisual> visual, Tiled::MapRotation mapRotation);
    Tiled::TileRotatedVisualData changeVisualData(QSharedPointer<Tiled::TileRotatedVisual> visual, Tiled::MapRotation mapRotation, const Tiled::TileRotatedVisualData& data);
    Tiled::Tileset* getRotatedTileset(const QString tilesetName);
    Tiled::TileRotated *rotatedTileFor(Tiled::Tile *tileR);
    Tiled::TileRotated *getOrCreateRotatedTileFor(Tiled::Tile *tile, Tiled::MapRotation mapRotation);
    QString getTilesetNameR(const QString& tilesetName, Tiled::MapRotation mapRotation);
    Tiled::TilesetRotated* getOrCreateTilesetRotated(const QString &tilesetName, Tiled::MapRotation mapRotation);
    Tiled::TileRotated* getOrCreateTileRotated(Tiled::TilesetRotated *tileset, int tileID);

protected slots:
    void fileNew();
    void fileOpen();
    bool fileSave();
    bool fileSaveAs();
    void createVisual();
    void clearVisual();
    void deleteVisual();
    void updateWindowTitle();
    void syncUI();
    void filterEdited(const QString &text);
    void tileActivated(const QModelIndex &index);
    void tilesetSelectionChanged();
    void tileRotatedActivated(const QModelIndex &index);
    void visualListSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tiled::Tileset *tileset);
    void tileDropped(QSharedPointer<Tiled::TileRotatedVisual> visual, Tiled::MapRotation mapRotation, const QString &tileName);
    void typeComboActivated(int index);

private:
    Ui::TileRotationWindow *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    QSharedPointer<Tiled::TileRotatedVisual> mCurrentVisual;
    Tiled::Tileset *mCurrentTileset;
    QString mHoverTileName;
    QString mError;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QList<Tiled::TilesetRotated*> mTilesets;
    QList<QSharedPointer<Tiled::TileRotatedVisual>> mVisuals;
    QList<QSharedPointer<Tiled::TileRotatedVisual>> mUnassignedVisuals;
    QMap<QString, Tiled::Tileset*> mTilesetRotated;
    QMap<QString, Tiled::TilesetRotated*> mTilesetByNameRotated;

    friend class AssignVisual;
    friend class CreateVisual;
    friend class DeleteVisual;
    friend class ChangeTiles;
    friend class Tiled::TileRotateDelegate;
};

#endif // TILEROTATIONWINDOW_H
