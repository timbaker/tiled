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

#include "maprotation.h"

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

namespace Core {
namespace Internal {
class FancyTabWidget;
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
class BuildingOrthoScene;
class BuildingOrthoView;
class CategoryDock;
class Door;
class FurnitureGroup;
class FurnitureTile;
class IMode;
class IsoObjectEditMode;
class OrthoObjectEditMode;
class Room;
class TileEditMode;
class Window;
class Stairs;
class WelcomeMode;

class BuildingEditorWindow;
class EditorWindowPerDocumentStuff : public QObject
{
    Q_OBJECT
public:
    EditorWindowPerDocumentStuff(BuildingDocument *doc);
    ~EditorWindowPerDocumentStuff();

    BuildingDocument *document() const { return mDocument; }

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

    void viewAddedForDocument(BuildingIsoView *view);

    bool missingTilesetsReported() const
    { return mMissingTilesetsReported; }
    void setMissingTilesetsReported(bool reported)
    { mMissingTilesetsReported = reported; }

    void focusOn(int x, int y, int z, int objectIndex);

    void setRotation(Tiled::MapRotation rotation);

public slots:
    void autoSaveCheck();
    void autoSaveTimeout();

private:
    void removeAutoSaveFile();

private:
    BuildingEditorWindow *mMainWindow;
    BuildingDocument *mDocument;
    EditMode mEditMode;
    EditMode mPrevObjectMode;
    BaseTool *mPrevObjectTool;
    BaseTool *mPrevTileTool;
    bool mMissingTilesetsReported;

    // Hack to keep iso/tile view position and scale synched.
    BuildingIsoView *mIsoView;
    BuildingIsoView *mTileView;

    QTimer mAutoSaveTimer;
    QString mAutoSaveFileName;
};

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    static BuildingEditorWindow *instance()
    { return mInstance; }

    explicit BuildingEditorWindow(QWidget *parent = nullptr);
    ~BuildingEditorWindow();

    void closeEvent(QCloseEvent *event);

    bool openFile(const QString &fileName);
    bool openAutoSave(const QString &fileName);

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

    void focusOn(const QString &file, int x, int y, int z, int objectIndex);

private:
    BuildingDocumentMgr *docman() const;

    void readSettings();
    void writeSettings();

public:
    void saveSplitterSizes(QSplitter *splitter);
    void restoreSplitterSizes(QSplitter *splitter);

    void hackUpdateActions() { updateActions(); } // FIXME

    Ui::BuildingEditorWindow *actionIface() { return ui; }
    QToolBar *createCommonToolBar();

    void documentTabCloseRequested(int index);

    QStringList recentFiles() const;

private:
    bool writeBuilding(BuildingDocument *doc, const QString &fileName);

    bool confirmSave();

    void addRecentFile(const QString &fileName);

    void deleteObjects();

    void cropBuilding(const QRect &bounds);

    typedef Tiled::Tileset Tileset; // Hack for signals/slots

signals:
    void tilePicked(const QString &tileName);
    void recentFilesChanged();

private slots:
    void documentAdded(BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingDocument *doc);
    void currentDocumentChanged(BuildingDocument *doc);

    void currentEditorChanged();

    void updateWindowTitle();

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
    void exportNewBinary();

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

    void cropToMinimum();
    void cropToSelection();
    void resizeBuilding();
    void flipHorizontal();
    void flipVertical();
    void rotateRight();
    void rotateLeft();

    void templatesDialog();
public slots:
    void tilesDialog();
private slots:
    void templateFromBuilding();

    void showObjectsChanged(bool show);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void tilesetChanged(Tileset *tileset);

    void reportMissingTilesets();

    void updateActions();

    void help();

    void currentModeAboutToChange(IMode *mode);
    void currentModeChanged();

    void viewAddedForDocument(BuildingDocument *doc, BuildingIsoView *view);

    void setRotationNotRotated();
    void setRotationClockwise90();
    void setRotationClockwise180();
    void setRotationClockwise270();

private:
    static BuildingEditorWindow *mInstance;
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    EditorWindowPerDocumentStuff *mCurrentDocumentStuff;
    QUndoGroup *mUndoGroup;
    QSettings &mSettings;
    QString mError;
    bool mSynching;

    WelcomeMode *mWelcomeMode;
    OrthoObjectEditMode *mOrthoObjectEditMode;
    IsoObjectEditMode *mIsoObjectEditMode;
    TileEditMode *mTileEditMode;

    QMap<BuildingDocument*,EditorWindowPerDocumentStuff*> mDocumentStuff;
    friend class EditorWindowPerDocumentStuff;

    Core::Internal::FancyTabWidget *mTabWidget;

    bool mDocumentChanging;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
