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

#ifndef BUILDINGEDITORWINDOW_H
#define BUILDINGEDITORWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QVector>

class QComboBox;
class QLabel;
class QSplitter;
class QUndoGroup;

namespace Ui {
class BuildingEditorWindow;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BaseTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingPreviewWindow;
class BuildingTile;
class BuildingTileEntry;
class BuildingTileCategory;
class Door;
class FloorEditor;
class FloorView;
class FurnitureGroup;
class Room;
class Window;
class Stairs;

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static BuildingEditorWindow *instance()
    { return mInstance; }

    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);

    bool confirmAllSave();
    bool closeYerself();

    bool Startup();

    bool LoadTMXConfig();
    bool validateTile(BuildingTile *btile, const char *key);

    void setCurrentRoom(Room *room) const; // TODO: move to BuildingDocument
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Building *currentBuilding() const;

private:
    void readSettings();
    void writeSettings();
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

    void updateRoomComboBox();
    void resizeCoordsLabel();

    void setCategoryList();

    bool writeBuilding(BuildingDocument *doc, const QString &fileName);

    bool confirmSave();

    void addDocument(BuildingDocument *doc);
    void clearDocument();

    void currentEWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentIWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentFloorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorFrameChanged(BuildingTileEntry *entry, bool mergeable);
    void currentWindowChanged(BuildingTileEntry *entry, bool mergeable);
    void currentCurtainsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentStairsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentRoofTileChanged(BuildingTileEntry *entry, int which, bool mergeable);

    void selectCurrentCategoryTile();

    void removeAutoSaveFile();

private slots:
    void updateWindowTitle();

    void roomIndexChanged(int index);
    void categoryScaleChanged(qreal scale);

    void categoryViewMousePressed();

    void categoryActivated(const QModelIndex &index);

    void categorySelectionChanged();
    void tileSelectionChanged();
    void furnitureSelectionChanged();

    void upLevel();
    void downLevel();

    void insertFloorAbove();
    void insertFloorBelow();
    void removeFloor();

    void newBuilding();
    void openBuilding();
    bool saveBuilding();
    bool saveBuildingAs();
    void exportTMX();

    void preferences();

    void roomsDialog();
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void resizeBuilding();
    void flipHorizontal();
    void flipVertical();
    void rotateRight();
    void rotateLeft();

    void templatesDialog();
    void tilesDialog();
    void tilesDialogEdited();
    void templateFromBuilding();

    void mouseCoordinateChanged(const QPoint &tilePos);

    void currentToolChanged(BaseTool *tool);
    void updateToolStatusText();

    void roofTypeChanged(QAction *action);
    void roofCornerTypeChanged(QAction *action);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void autoSaveCheck();
    void autoSaveTimeout();

    void updateActions();

private:
    static BuildingEditorWindow *mInstance;
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    FloorEditor *roomEditor;
    FloorView *mView;
    QComboBox *mRoomComboBox;
    QLabel *mFloorLabel;
    QUndoGroup *mUndoGroup;
    QSettings mSettings;
    QString mError;
    BuildingPreviewWindow *mPreviewWin;
    Tiled::Internal::Zoomable *mZoomable;
    Tiled::Internal::Zoomable *mCategoryZoomable;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    bool mSynching;
    bool mInitialCategoryViewSelectionEvent;
    QTimer *mAutoSaveTimer;
    QString mAutoSaveFileName;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
