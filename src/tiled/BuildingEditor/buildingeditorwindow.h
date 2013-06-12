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
#include <QTimer>
#include <QVector>

class QComboBox;
class QLabel;
class QSplitter;
class QStackedWidget;
class QUndoGroup;
class QGraphicsView;

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
class BuildingBaseScene;
class BuildingDocument;
class BuildingDocumentMgr;
class BuildingFloor;
class BuildingTile;
class BuildingTileEntry;
class BuildingTileCategory;
class BuildingIsoScene;
class BuildingIsoView;
class BuildingFurnitureDock;
class BuildingLayersDock;
class BuildingTilesetDock;
class Door;
class BuildingOrthoScene;
class BuildingOrthoView;
class FurnitureGroup;
class FurnitureTile;
class Room;
class Window;
class Stairs;
class TileModeToolBar;

class BuildingEditorWindow;
class BuildingDocumentGUI : public QObject
{
    Q_OBJECT
public:
    BuildingDocumentGUI(BuildingDocument *doc);
    ~BuildingDocumentGUI();

    BuildingDocument *document() const { return mDocument; }
    BuildingOrthoView *orthoView() const { return mOrthoView; }
    BuildingIsoView *isoView() const { return mIsoView; }

    QGraphicsView *currentView() const;
    BuildingBaseScene *currentScene() const;
    Tiled::Internal::Zoomable *currentZoomable() const;

    QStackedWidget *stackedWidget() const { return mStackedWidget; }

    void activate();
    void deactivate();

    enum EditMode {
        OrthoObjectMode,
        IsoObjectMode,
        TileMode
    };
    void setEditMode(EditMode mode);
    EditMode editMode() const { return mEditMode; }
    bool isOrthoObject() { return mEditMode == OrthoObjectMode; }
    bool isIsoObject() { return mEditMode == IsoObjectMode; }
    bool isObject() { return mEditMode != TileMode; }
    bool isTile() { return mEditMode == TileMode; }
    bool isOrtho() { return mEditMode == OrthoObjectMode; }
    bool isIso() { return mEditMode != OrthoObjectMode; }

    void toOrthoObject();
    void toIsoObject();
    void toObject();
    void toTile();

    void rememberTool();
    void restoreTool();

public slots:
    void updateDocumentTab();
    void autoSaveCheck();
    void autoSaveTimeout();

private:
    void removeAutoSaveFile();

private:
    BuildingEditorWindow *mMainWindow;
    BuildingDocument *mDocument;
    QStackedWidget *mStackedWidget;
    BuildingOrthoView *mOrthoView;
    BuildingOrthoScene *mOrthoScene;
    BuildingIsoView *mIsoView;
    BuildingIsoScene *mIsoScene;
    EditMode mEditMode;
    EditMode mPrevObjectMode;
    BaseTool *mPrevObjectTool;
    BaseTool *mPrevTileTool;

    QTimer mAutoSaveTimer;
    QString mAutoSaveFileName;
};

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static BuildingEditorWindow *instance()
    { return mInstance; }

    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    void closeEvent(QCloseEvent *event);

    bool openFile(const QString &fileName);

    bool confirmAllSave();
    bool closeYerself();

    bool Startup();

    void setCurrentRoom(Room *room) const;
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Building *currentBuilding() const;

    BuildingFloor *currentFloor() const;

    QString currentLayer() const;

    void selectAndDisplay(BuildingTileEntry *entry);
    void selectAndDisplay(FurnitureTile *ftile);

private:
    BuildingDocumentMgr *docman() const;

    void readSettings();
    void writeSettings();
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

    void updateRoomComboBox();
    void resizeCoordsLabel();

    void setCategoryList();

    bool writeBuilding(BuildingDocument *doc, const QString &fileName);

    bool confirmSave();

    void clearDocument();

    void currentEWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentIWallChanged(BuildingTileEntry *entry, bool mergeable);
    void currentFloorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorChanged(BuildingTileEntry *entry, bool mergeable);
    void currentDoorFrameChanged(BuildingTileEntry *entry, bool mergeable);
    void currentWindowChanged(BuildingTileEntry *entry, bool mergeable);
    void currentCurtainsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentStairsChanged(BuildingTileEntry *entry, bool mergeable);
    void currentRoomTileChanged(int entryEnum, BuildingTileEntry *entry, bool mergeable);
    void currentRoofTileChanged(BuildingTileEntry *entry, int which, bool mergeable);

    void selectCurrentCategoryTile();

    BuildingTileCategory *categoryAt(int row);
    FurnitureGroup *furnitureGroupAt(int row);

    void deleteObjects();

    typedef Tiled::Tileset Tileset; // Hack for signals/slots

signals:
    void tilePicked(const QString &tileName);

private slots:
    void documentAdded(BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingDocument *doc);
    void currentDocumentChanged(BuildingDocument *doc);

    void currentDocumentTabChanged(int index);
    void documentTabCloseRequested(int index);

    void updateWindowTitle();

    void roomIndexChanged(int index);
    void categoryScaleChanged(qreal scale);

    void categoryViewMousePressed();

    void categoryActivated(const QModelIndex &index);

    void categorySelectionChanged();
    void tileSelectionChanged();
    void furnitureSelectionChanged();

    void scrollToNow(int which, const QModelIndex &index);

    void usedTilesChanged();
    void usedFurnitureChanged();
    void resetUsedTiles();
    void resetUsedFurniture();

    void upLevel();
    void downLevel();

    void insertFloorAbove();
    void insertFloorBelow();
    void removeFloor();
    void floorsDialog();

    void newBuilding();
    void openBuilding();
    bool saveBuilding();
    bool saveBuildingAs();
    void exportTMX();

    void editCut();
    void editCopy();
    void editPaste();
    void editDelete();

    void selectAll();
    void selectNone();

    void preferences();

    void buildingPropertiesDialog();
    void buildingGrime();

    void roomsDialog();
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);
    void currentRoomChanged();

    void resizeBuilding();
    void flipHorizontal();
    void flipVertical();
    void rotateRight();
    void rotateLeft();

    void templatesDialog();
    void tilesDialog();
    void tilesDialogEdited();
    void templateFromBuilding();

    void zoomIn();
    void zoomOut();
    void resetZoom();

    void showObjectsChanged(bool show);

    void mouseCoordinateChanged(const QPoint &tilePos);

    void currentToolChanged(BaseTool *tool);
    void updateToolStatusText();

    void roofTypeChanged(QAction *action);
    void roofCornerTypeChanged(QAction *action);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void tilesetChanged(Tileset *tileset);

    void reportMissingTilesets();

    void updateActions();

    void help();

    void setEditMode();
    void toggleOrthoIso();
    void toggleEditMode();

private:
    static BuildingEditorWindow *mInstance;
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    BuildingDocumentGUI *mCurrentDocumentGUI;
    QComboBox *mRoomComboBox;
    QLabel *mFloorLabel;
    QUndoGroup *mUndoGroup;
    QSettings mSettings;
    QString mError;
    Tiled::Internal::Zoomable *mCategoryZoomable;
    BuildingTileCategory *mCategory;
    FurnitureGroup *mFurnitureGroup;
    int mRowOfFirstCategory;
    int mRowOfFirstFurnitureGroup;
    bool mSynching;
    bool mInitialCategoryViewSelectionEvent;
    QMenu *mUsedContextMenu;
    QAction *mActionClearUsed;

    TileModeToolBar *mTileModeToolBar;
    BuildingFurnitureDock *mFurnitureDock;
    BuildingLayersDock *mLayersDock;
    BuildingTilesetDock *mTilesetDock;
    bool mFirstTimeSeen;

    friend class BuildingDocumentGUI;
    QMap<BuildingDocument*,BuildingDocumentGUI*> mDocumentGUI;
    BuildingDocumentGUI::EditMode mEditMode;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
