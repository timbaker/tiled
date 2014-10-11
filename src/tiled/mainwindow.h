/*
 * mainwindow.h
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2009-2010, Jeff Bland <jksb@member.fsf.org>
 * Copyright 2010-2011, Stefan Beller <stefanbeller@googlemail.com>
 *
 * This file is part of Tiled.
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "mapdocument.h"

#include <QMainWindow>
#include <QSessionManager>
#include <QSettings>

class QComboBox;
class QLabel;
class QToolButton;
#ifdef ZOMBOID
class QSplitter;
#endif

namespace Ui {
class MainWindow;
}

#ifdef ZOMBOID
class ContainerOverlayDialog;

namespace BuildingEditor {
class BuildingEditorWindow;
}
#endif

namespace Tiled {

class TileLayer;
class MapReaderInterface;

#ifdef ZOMBOID
namespace Lua {
class LuaMap;
class LuaTileTool;
}
#endif

namespace Internal {

class ClipboardManager;
class DocumentManager;
class LayerDock;
class MapDocumentActionHandler;
class MapScene;
class StampBrush;
class BucketFillTool;
class TilesetDock;
class MapView;
class CommandButton;
class ObjectsDock;
class Zoomable;
#ifdef ZOMBOID
class EdgeTool;
class Eraser;
class ZLevelsDock;
class MapsDock;
class TileDefDialog;
class TileLayersPanel;
class WorldEdDock;
#endif

/**
 * The main editor window.
 *
 * Represents the main user interface, including the menu bar. It keeps track
 * of the current file and is also the entry point of all menu actions.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
#ifdef ZOMBOID
    static MainWindow *mInstance;
    static MainWindow *instance() { return mInstance; }
#endif

    MainWindow(QWidget *parent = 0, Qt::WindowFlags flags = 0);
    ~MainWindow();

    void commitData(QSessionManager &manager);

    /**
     * Opens the given file. When opened succesfully, the file is added to the
     * list of recent files.
     *
     * When a \a reader is given, it is used to open the file. Otherwise, a
     * reader is searched using MapReaderInterface::supportsFile.
     *
     * @return whether the file was succesfully opened
     */
    bool openFile(const QString &fileName, MapReaderInterface *reader);

    /**
     * Attempt to open the previously opened file.
     * TODO: Opening last file should be optional
     */
    void openLastFiles();

#ifdef ZOMBOID
    bool InitConfigFiles();

    void ApplyScriptChanges(MapDocument *doc, const QString &undoText, Lua::LuaMap *map);
    void LuaScript(const QString &filePath);
    bool LuaScript(MapDocument *doc, const QString &filePath);

    TileLayersPanel *tileLayersPanel() const
    { return mTileLayersPanel; }
#endif

public slots:
    bool openFile(const QString &fileName);

protected:
    void closeEvent(QCloseEvent *event);
    void changeEvent(QEvent *event);

    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);

    void dragEnterEvent(QDragEnterEvent *);
    void dropEvent(QDropEvent *);

public slots:
    void newMap();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void saveAsImage();
    void exportAs();
    void closeFile();
    void closeAllFiles();

    void cut();
    void copy();
    void paste();
    void delete_(); // delete is a reserved word
    void openPreferences();

    void zoomIn();
    void zoomOut();
    void zoomNormal();

    bool newTileset(const QString &path = QString());
    void newTilesets(const QStringList &paths);
    void addExternalTileset();
    void resizeMap();
    void offsetMap();
    void editMapProperties();
    void autoMap();
#ifdef ZOMBOID
    void convertToLot();
    void convertOrientation();

    void RoomDefGo();
    void RoomDefMerge();
    void RoomDefRemove();
    void RoomDefUnknownWalls();

    void LuaConsole();
#endif

    void updateWindowTitle();
    void updateActions();
    void updateZoomLabel();
#ifdef ZOMBOID
    void resizeStatusInfoLabel();
    void aboutToShowLevelMenu();
    void aboutToShowLayerMenu();
    void triggeredLevelMenu(QAction *action);
    void triggeredLayerMenu(QAction *action);
#endif
    void aboutTiled();
    void openRecentFile();
    void clearRecentFiles();

    void editLayerProperties();

    void flipStampHorizontally();
    void flipStampVertically();
    void rotateStampLeft();
    void rotateStampRight();

    void setStampBrush(const TileLayer *tiles);
    void updateStatusInfoLabel(const QString &statusInfo);

    void mapDocumentChanged(MapDocument *mapDocument);
    void closeMapDocument(int index);

    void autoMappingError();
    void autoMappingWarning();

#ifdef ZOMBOID
    void tilePicked(Tile *tile);

    void showBuildingEditor();
    void buildingTilePicked(const QString &tileName);
    void tilesetMetaInfoDialog();
    void tilePropertiesEditor();
    void compareTileDef();
    void createPackFile();
    void showPackViewer();
    void comparePackFiles();
    void containerOverlayDialog();
    void enflatulator();
    void virtualTilesetDialog();
    void launchWorldEd();

    void brushSizeMinus();
    void brushSizePlus();

    void helpContents();
#endif

private:
    /**
      * Asks the user whether the current map should be saved when necessary.
      *
      * @return <code>true</code> when any unsaved data is either discarded or
      *         saved, <code>false</code> when the user cancelled or saving
      *         failed.
      */
    bool confirmSave();

    /**
      * Checks all maps for changes, if so, ask if to save these changes.
      *
      * @return <code>true</code> when any unsaved data is either discarded or
      *         saved, <code>false</code> when the user cancelled or saving
      *         failed.
      */
    bool confirmAllSave();

    /**
     * Save the current map to the given file name. When saved succesfully, the
     * file is added to the list of recent files.
     * @return <code>true</code> on success, <code>false</code> on failure
     */
    bool saveFile(const QString &fileName);

    void writeSettings();
#ifdef ZOMBOID
public:
    void readSettings();
private:
#else
    void readSettings();
#endif

    void addMapDocument(MapDocument *mapDocument);
    QStringList recentFiles() const;
    QString fileDialogStartLocation() const;

    /**
     * Add the given file to the recent files list.
     */
    void setRecentFile(const QString &fileName);

    /**
     * Update the recent files menu.
     */
    void updateRecentFiles();

    void retranslateUi();

    Ui::MainWindow *mUi;
    MapDocument *mMapDocument;
    MapDocumentActionHandler *mActionHandler;
    LayerDock *mLayerDock;
    ObjectsDock *mObjectsDock;
#ifdef ZOMBOID
    ZLevelsDock *mLevelsDock;
    MapsDock *mMapsDock;
    WorldEdDock *mWorldEdDock;
#endif
    TilesetDock *mTilesetDock;
#ifdef ZOMBOID
    TileLayersPanel *mTileLayersPanel;
    QSplitter *mMainSplitter;
    QMenu *mCurrentLevelMenu;
    QToolButton *mCurrentLevelButton;
    QMenu *mCurrentLayerMenu;
    QToolButton *mCurrentLayerButton;
#else
    QLabel *mCurrentLayerLabel;
#endif
    Zoomable *mZoomable;
    QComboBox *mZoomComboBox;
    QLabel *mStatusInfoLabel;
    QSettings mSettings;
    QToolButton *mRandomButton;
    CommandButton *mCommandButton;

    StampBrush *mStampBrush;
    BucketFillTool *mBucketFillTool;
#ifdef ZOMBOID
    Eraser *mEraserTool;

    void initLuaTileTools();
    QList<Lua::LuaTileTool*> mLuaTileTools;
#endif

    ClipboardManager *mClipboardManager;

    enum { MaxRecentFiles = 8 };
    QAction *mRecentFiles[MaxRecentFiles];

    QMenu *mLayerMenu;

    void setupQuickStamps();

    DocumentManager *mDocumentManager;

#ifdef ZOMBOID
    BuildingEditor::BuildingEditorWindow *mBuildingEditor;
    TileDefDialog *mTileDefDialog;
    ContainerOverlayDialog *mContainerOverlayDialog;
#endif
};

} // namespace Internal
} // namespace Tiled

#endif // MAINWINDOW_H
