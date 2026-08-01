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

#include "BuildingEditor/exportbasementsdialog.h"
#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QTimer>
#include <QVector>

class ActionManager;
class KeyboardShortcutWindow;

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
class Map;
class Tile;
class Tileset;
namespace Internal {
class TileDefFile;
class Zoomable;
}
}

namespace Core {
namespace Internal {
class FancyTabWidget;
}
}

namespace BuildingEditor {

class AttributeEditMode;
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
class ExportBasementsDialog;
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
        TileMode,
        AttributeMode,
    };
    void setEditMode(EditMode mode);
    EditMode editMode() const { return mEditMode; }
    bool isOrthoObject() { return mEditMode == EditMode::OrthoObjectMode; }
    bool isIsoObject() { return mEditMode == EditMode::IsoObjectMode; }
    bool isObject() { return mEditMode != EditMode::TileMode && mEditMode != EditMode::AttributeMode; }
    bool isTile() { return mEditMode == EditMode::TileMode; }
    bool isOrtho() { return mEditMode == EditMode::OrthoObjectMode; }
    bool isIso() { return mEditMode != EditMode::OrthoObjectMode; }
    bool isAttribute() { return mEditMode == EditMode::AttributeMode; }

    void toOrthoObject();
    void toIsoObject();
    void toObject();
    void toTile();
    void toAttribute();

    void rememberTool();
    void restoreTool();

    void viewAddedForDocument(BuildingIsoView *view);

    bool missingTilesetsReported() const
    { return mMissingTilesetsReported; }
    void setMissingTilesetsReported(bool reported)
    { mMissingTilesetsReported = reported; }

    void focusOn(int x, int y, int z, int objectIndex);

    BuildingIsoView *isoView() const { return mIsoView; }
    BuildingIsoView *tileView() const { return mTileView; }

    void setInitialPosition();

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
    BaseTool *mPrevAttributeTool;
    bool mMissingTilesetsReported;
    bool mInitialPositionSet = false;

    // Hack to keep iso/tile view position and scale synched.
    QPointF mIsoViewsCenter;
    qreal mIsoViewsZoom = qreal(1.0);
    BuildingIsoView *mIsoView;
    BuildingIsoView *mTileView;
    BuildingIsoView *mAttributeView;

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

#ifndef BUILDINGED_SA
    bool openFile(const QString &fileName);
#endif
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

    QUndoGroup *undoGroup() const
    { return mUndoGroup; }

    QAction *undoAction() const
    { return mUndoAction; }

    QAction *redoAction() const
    { return mRedoAction; }

#ifdef BUILDINGED_SA
    void readSettings();
#endif

private:
    BuildingDocumentMgr *docman() const;

#ifndef BUILDINGED_SA
    void readSettings();
#endif
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

    void exportNewBinaryFile(ExportBasementsDialog *dialog, const QString& tbxFilePath, QSet<QString> &northStairTiles, QSet<QString> &westStairTiles, QString &luaCode);
    void getTopStaircaseTiles(QSet<QString>& northStairTiles, QSet<QString> &westStairTiles);
    bool getBasementStaircase(Tiled::Map* map, QSet<QString>& northStairTiles, QSet<QString>& westStairTiles, int &stairx, int &stairy, QString& stairDir, bool isBasementAccess);

    void initActionManager();

    typedef Tiled::Tileset Tileset; // Hack for signals/slots

signals:
    void tilePicked(const QString &tileName);
    void recentFilesChanged();

private slots:
    void documentAdded(BuildingEditor::BuildingDocument *doc);
    void documentAboutToClose(int index, BuildingEditor::BuildingDocument *doc);
    void currentDocumentChanged(BuildingEditor::BuildingDocument *doc);

    void currentEditorChanged();

    void updateWindowTitle();

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
    void exportNewBinary();

    void editCut();
    void editCopy();
    void editPaste();
    void editDelete();
    void editDeleteInAllLayers();

    void selectAll();
    void selectNone();

    void preferences();

    void buildingPropertiesDialog();
    void keyValuesDialog();
    void buildingGrime();

    void roomAdded(BuildingEditor::Room *room);
    void roomRemoved(BuildingEditor::Room *room);
    void roomsReordered();
    void roomChanged(BuildingEditor::Room *room);

    void cropToMinimum();
    void cropToSelection();
    void resizeBuilding();
    void flipHorizontal();
    void flipVertical();
    void rotateRight();
    void rotateLeft();

    void setBasementAccessNone();
    void setBasementAccessNorth();
    void setBasementAccessWest();

    void templatesDialog();

    void keyboardShortcuts();

public slots:
#ifdef BUILDINGED_SA
    bool openFile(const QString &fileName);
#endif
    void floorsDialog();
    void roomsDialog();
    void tilesDialog();
    void templateFromBuilding();

private slots:
    void showObjectsChanged(bool show);
    void highlightUnlitRoomsChanged(bool show);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void tilesetChanged(Tiled::Tileset *tileset);

    void reportMissingTilesets();

    void updateActions();

    void help();

    void currentModeAboutToChange(BuildingEditor::IMode *mode);
    void currentModeChanged();

    void viewAddedForDocument(BuildingEditor::BuildingDocument *doc, BuildingEditor::BuildingIsoView *view);

private:
    static BuildingEditorWindow *mInstance;
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    EditorWindowPerDocumentStuff *mCurrentDocumentStuff;
    ActionManager *mActionManager = nullptr;
    KeyboardShortcutWindow *mKeyboardShortcutWindow = nullptr;
    QUndoGroup *mUndoGroup;
    QAction *mUndoAction;
    QAction *mRedoAction;
    QSettings &mSettings;
    QString mError;
    bool mSynching;

    WelcomeMode *mWelcomeMode;
    OrthoObjectEditMode *mOrthoObjectEditMode;
    IsoObjectEditMode *mIsoObjectEditMode;
    TileEditMode *mTileEditMode;
    AttributeEditMode *mAttributeEditMode;

    QMap<BuildingDocument*,EditorWindowPerDocumentStuff*> mDocumentStuff;
    friend class EditorWindowPerDocumentStuff;

    Core::Internal::FancyTabWidget *mTabWidget;

    bool mDocumentChanging;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
