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
    void setVisualDataList();
    void setTilesetList();
    void updateUsedTiles();
    bool isTileUsed(const QString &_tileName);
    void displayTileInTileset(Tiled::Tile *tile);
    void displayTileInTileset(BuildingEditor::BuildingTile *tile);
    void addVisual(QSharedPointer<Tiled::TileRotatedVisual> visual, int index);
    QSharedPointer<Tiled::TileRotatedVisual> removeVisual(int index);
    void changeDataEdge(Tiled::TileRotatedVisualEdge edge);
    void changeDataOffset(int x, int y);

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
    Tiled::TileRotated *rotatedTileFor(Tiled::Tile *tileR);
    Tiled::TileRotated *getOrCreateTileRotatedForTileReal(Tiled::Tile *tileReal);
    Tiled::TileRotated *getTileRotatedForTileReal(Tiled::Tile *tileReal);
    Tiled::TilesetRotated* getOrCreateTilesetRotated(const QString &tilesetName);
    Tiled::TilesetRotated* getTilesetRotated(const QString &tilesetName);
    Tiled::TileRotated* getOrCreateTileRotated(Tiled::TilesetRotated *tilesetR, int tileID);

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
    void visualActivated(const QModelIndex &index);
    void visualDataSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tiled::Tileset *tileset);
    void tileDroppedOntoVisualView(QSharedPointer<Tiled::TileRotatedVisual> visual, Tiled::MapRotation mapRotation, const QString &tileName);
    void tileDroppedOntoVisualDataView(const QString &tilesetName, int tileID);
    void edgeComboActivated(int index);
    void edgeRadioClicked(int id);
    void changeDataOffsetDX(bool dx);
    void changeDataOffsetDY(bool dy);
    void pinVisualsToggled(bool checked);

    void tileContextMenu_AddFloor();
    void tileContextMenu_AddNorthAndWestTiles();
    void tileContextMenu_AddSouthAndEastTiles();
    void tileContextMenu_AddNorthWestCorner();

private:
    Ui::TileRotationWindow *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    QSharedPointer<Tiled::TileRotatedVisual> mCurrentVisual;
    Tiled::MapRotation mCurrentVisualRotation;
    Tiled::Tileset *mCurrentTileset;
    Tiled::Tileset *mPinnedTileset;
    QString mHoverTileName;
    QString mError;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QList<Tiled::TilesetRotated*> mTilesetRotatedList;
    QList<QSharedPointer<Tiled::TileRotatedVisual>> mVisuals;
    QList<QSharedPointer<Tiled::TileRotatedVisual>> mUnassignedVisuals;
    QMap<QString, Tiled::Tileset*> mFakeTilesetLookup;
    QMap<QString, Tiled::TilesetRotated*> mTilesetByName;
    QMenu *mTileContextMenu;
    bool mSynchingUI = false;

    friend class AssignVisual;
    friend class CreateVisual;
    friend class DeleteVisual;
    friend class ChangeTiles;
    friend class InitFromBuildingTiles;
    friend class Tiled::TileRotateDelegate;
};

#endif // TILEROTATIONWINDOW_H
