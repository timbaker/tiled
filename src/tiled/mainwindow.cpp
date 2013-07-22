/*
 * mainwindow.cpp
 * Copyright 2008-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2009-2010, Jeff Bland <jksb@member.fsf.org>
 * Copyright 2009, Dennis Honeyman <arcticuno@gmail.com>
 * Copyright 2009, Christian Henz <chrhenz@gmx.de>
 * Copyright 2010, Andrew G. Crowell <overkill9999@gmail.com>
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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "aboutdialog.h"
#include "addremovemapobject.h"
#include "automappingmanager.h"
#include "addremovetileset.h"
#include "clipboardmanager.h"
#include "createobjecttool.h"
#include "documentmanager.h"
#include "editpolygontool.h"
#include "eraser.h"
#include "erasetiles.h"
#include "bucketfilltool.h"
#include "filltiles.h"
#include "languagemanager.h"
#include "layer.h"
#include "layerdock.h"
#include "layermodel.h"
#include "map.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "newmapdialog.h"
#include "newtilesetdialog.h"
#include "pluginmanager.h"
#include "propertiesdialog.h"
#include "resizedialog.h"
#include "objectselectiontool.h"
#include "objectgroup.h"
#include "offsetmapdialog.h"
#include "preferences.h"
#include "preferencesdialog.h"
#include "quickstampmanager.h"
#include "saveasimagedialog.h"
#include "stampbrush.h"
#include "tilelayer.h"
#include "tileselectiontool.h"
#include "tileset.h"
#include "tilesetdock.h"
#include "tilesetmanager.h"
#include "toolmanager.h"
#include "tmxmapreader.h"
#include "tmxmapwriter.h"
#include "undodock.h"
#include "utils.h"
#include "zoomable.h"
#include "commandbutton.h"
#include "objectsdock.h"
#ifdef ZOMBOID
#include "bmpblender.h"
#include "bmptool.h"
#include "changetileselection.h"
#include "converttolotdialog.h"
#include "convertorientationdialog.h"
#include "mapcomposite.h"
#include "mapimagemanager.h"
#include "mapmanager.h"
#include "mapsdock.h"
#include "picktiletool.h"
#include "roomdeftool.h"
#include "tiledefdialog.h"
#include "tiledeffile.h"
#include "tilelayerspanel.h"
#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "zlevelsdock.h"
#include "zprogress.h"
#include "worldeddock.h"
#include "worldlottool.h"

#include "worlded/worldedmgr.h"

#include <QDebug>
#include <QDesktopServices>
#include <QProcess>
#include <QSplitter>
#endif

#ifdef Q_WS_MAC
#include "macsupport.h"
#endif

#include <QCloseEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QScrollBar>
#include <QSessionManager>
#include <QTextStream>
#include <QUndoGroup>
#include <QUndoStack>
#include <QUndoView>
#include <QImageReader>
#include <QRegExp>
#include <QSignalMapper>
#include <QShortcut>
#include <QToolButton>

using namespace Tiled;
using namespace Tiled::Internal;
using namespace Tiled::Utils;

#ifdef ZOMBOID
#include "BuildingEditor/buildingeditorwindow.h"
#include "BuildingEditor/buildingpreferences.h"
#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/buildingtemplates.h"
#include "BuildingEditor/buildingtmx.h"
#include "BuildingEditor/furnituregroups.h"
using namespace BuildingEditor;
#endif

#ifdef ZOMBOID
MainWindow *MainWindow::mInstance = 0;
#endif

MainWindow::MainWindow(QWidget *parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags)
    , mUi(new Ui::MainWindow)
    , mMapDocument(0)
    , mActionHandler(new MapDocumentActionHandler(this))
    , mLayerDock(new LayerDock(this))
    , mObjectsDock(new ObjectsDock())
#ifdef ZOMBOID
    , mLevelsDock(new ZLevelsDock(this))
    , mMapsDock(new MapsDock(this))
    , mWorldEdDock(new WorldEdDock(this))
#endif
    , mTilesetDock(new TilesetDock(this))
#ifdef ZOMBOID
    , mTileLayersPanel(new TileLayersPanel())
    , mMainSplitter(new QSplitter(this))
    , mCurrentLevelMenu(new QMenu(this))
    , mCurrentLevelButton(new QToolButton(this))
    , mCurrentLayerMenu(new QMenu(this))
    , mCurrentLayerButton(new QToolButton(this))
#else
    , mCurrentLayerLabel(new QLabel)
#endif
    , mZoomable(0)
    , mZoomComboBox(new QComboBox)
    , mStatusInfoLabel(new QLabel)
    , mClipboardManager(new ClipboardManager(this))
    , mDocumentManager(DocumentManager::instance())
#ifdef ZOMBOID
    , mBuildingEditor(0)
    , mTileDefDialog(0)
#endif
{
#ifdef ZOMBOID
    mInstance = this;
#endif
    mUi->setupUi(this);
#ifdef ZOMBOID
    mMainSplitter->setObjectName(QLatin1String("mainSplitter"));
    mMainSplitter->setOrientation(Qt::Horizontal);
    mMainSplitter->setChildrenCollapsible(false);
    mMainSplitter->addWidget(mTileLayersPanel);
    mMainSplitter->addWidget(mDocumentManager->widget());
    mMainSplitter->setStretchFactor(0, 0);
    mMainSplitter->setStretchFactor(1, 1);
    mMainSplitter->setSizes(QList<int>() << 80 << 200);

    QVBoxLayout *centralLayout = static_cast<QVBoxLayout*>(centralWidget()->layout());
    centralLayout->insertWidget(0, mMainSplitter);
    centralLayout->setStretch(0, 1);
    centralLayout->setStretch(1, 0);
#else
    setCentralWidget(mDocumentManager->widget());
#endif

    PluginManager::instance()->loadPlugins();

#ifdef Q_WS_MAC
    MacSupport::addFullscreen(this);
#endif

    Preferences *preferences = Preferences::instance();

    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));

    QIcon tiledIcon(QLatin1String(":images/tiled-icon-16.png"));
    tiledIcon.addFile(QLatin1String(":images/tiled-icon-32.png"));
    setWindowIcon(tiledIcon);

    // Add larger icon versions for actions used in the tool bar
    QIcon newIcon = mUi->actionNew->icon();
    QIcon openIcon = mUi->actionOpen->icon();
    QIcon saveIcon = mUi->actionSave->icon();
    newIcon.addFile(QLatin1String(":images/24x24/document-new.png"));
    openIcon.addFile(QLatin1String(":images/24x24/document-open.png"));
    saveIcon.addFile(QLatin1String(":images/24x24/document-save.png"));
    redoIcon.addFile(QLatin1String(":images/24x24/edit-redo.png"));
    undoIcon.addFile(QLatin1String(":images/24x24/edit-undo.png"));
    mUi->actionNew->setIcon(newIcon);
    mUi->actionOpen->setIcon(openIcon);
    mUi->actionSave->setIcon(saveIcon);

    QUndoGroup *undoGroup = mDocumentManager->undoGroup();
    QAction *undoAction = undoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = undoGroup->createRedoAction(this, tr("Redo"));
    mUi->mainToolBar->setToolButtonStyle(Qt::ToolButtonFollowStyle);
    mUi->actionNew->setPriority(QAction::LowPriority);
    redoAction->setPriority(QAction::LowPriority);
    redoAction->setIcon(redoIcon);
    undoAction->setIcon(undoIcon);
    redoAction->setIconText(tr("Redo"));
    undoAction->setIconText(tr("Undo"));
    connect(undoGroup, SIGNAL(cleanChanged(bool)), SLOT(updateWindowTitle()));

    UndoDock *undoDock = new UndoDock(undoGroup, this);

#ifdef ZOMBOID
    addDockWidget(Qt::RightDockWidgetArea, mLayerDock);
    addDockWidget(Qt::RightDockWidgetArea, mLevelsDock);
    addDockWidget(Qt::RightDockWidgetArea, mObjectsDock);
    addDockWidget(Qt::RightDockWidgetArea, mWorldEdDock);
    addDockWidget(Qt::RightDockWidgetArea, mMapsDock);
    addDockWidget(Qt::RightDockWidgetArea, undoDock);
    addDockWidget(Qt::RightDockWidgetArea, mTilesetDock);
    tabifyDockWidget(mLayerDock, mLevelsDock);
    tabifyDockWidget(mLevelsDock, mObjectsDock);
    tabifyDockWidget(mObjectsDock, mWorldEdDock);
    tabifyDockWidget(mWorldEdDock, mMapsDock);
    tabifyDockWidget(undoDock, mTilesetDock);

    setStatusBar(0);

    QHBoxLayout *statusBarLayout = new QHBoxLayout(mUi->statusBarFrame);
    statusBarLayout->setObjectName(QLatin1String("statusBarLayout"));
    statusBarLayout->setContentsMargins(3, 3, 0, 3);
    mUi->statusBarFrame->setLayout(statusBarLayout);
#else
    addDockWidget(Qt::RightDockWidgetArea, mLayerDock);
    addDockWidget(Qt::RightDockWidgetArea, undoDock);
    tabifyDockWidget(undoDock, mLayerDock);
    addDockWidget(Qt::RightDockWidgetArea, mObjectsDock);
    tabifyDockWidget(mLayerDock, mObjectsDock);
    addDockWidget(Qt::RightDockWidgetArea, mTilesetDock);

    statusBar()->addPermanentWidget(mZoomComboBox);
#endif
    mUi->actionNew->setShortcuts(QKeySequence::New);
    mUi->actionOpen->setShortcuts(QKeySequence::Open);
    mUi->actionSave->setShortcuts(QKeySequence::Save);
    mUi->actionSaveAs->setShortcuts(QKeySequence::SaveAs);
    mUi->actionClose->setShortcuts(QKeySequence::Close);
    mUi->actionQuit->setShortcuts(QKeySequence::Quit);
    mUi->actionCut->setShortcuts(QKeySequence::Cut);
    mUi->actionCopy->setShortcuts(QKeySequence::Copy);
    mUi->actionPaste->setShortcuts(QKeySequence::Paste);
    mUi->actionDelete->setShortcuts(QKeySequence::Delete);
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setShortcuts(QKeySequence::Redo);

    mUi->actionShowGrid->setChecked(preferences->showGrid());
    mUi->actionSnapToGrid->setChecked(preferences->snapToGrid());
    mUi->actionHighlightCurrentLayer->setChecked(preferences->highlightCurrentLayer());
#ifdef ZOMBOID
    mUi->actionHighlightRoomUnderPointer->setChecked(preferences->highlightRoomUnderPointer());
    mUi->actionShowMiniMap->setChecked(preferences->showMiniMap());
    mUi->actionShowTileLayersPanel->setChecked(preferences->showTileLayersPanel());
#endif

    // Make sure Ctrl+= also works for zooming in
    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
#ifdef ZOMBOID
    keys += QKeySequence(tr("="));
#endif
    mUi->actionZoomIn->setShortcuts(keys);
    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    mUi->actionZoomOut->setShortcuts(keys);

    mUi->menuEdit->insertAction(mUi->actionCut, undoAction);
    mUi->menuEdit->insertAction(mUi->actionCut, redoAction);
    mUi->menuEdit->insertSeparator(mUi->actionCut);
    mUi->menuEdit->insertAction(mUi->actionPreferences,
                                mActionHandler->actionSelectAll());
    mUi->menuEdit->insertAction(mUi->actionPreferences,
                                mActionHandler->actionSelectNone());
    mUi->menuEdit->insertSeparator(mUi->actionPreferences);
    mUi->mainToolBar->addAction(undoAction);
    mUi->mainToolBar->addAction(redoAction);

    mUi->mainToolBar->addSeparator();

    mCommandButton = new CommandButton(this);
    mUi->mainToolBar->addWidget(mCommandButton);

    mUi->menuMap->insertAction(mUi->actionOffsetMap,
                               mActionHandler->actionCropToSelection());

    mRandomButton = new QToolButton(this);
    mRandomButton->setToolTip(tr("Random Mode"));
    mRandomButton->setIcon(QIcon(QLatin1String(":images/24x24/dice.png")));
    mRandomButton->setCheckable(true);
    mRandomButton->setShortcut(QKeySequence(tr("D")));
    mUi->mainToolBar->addWidget(mRandomButton);

    mLayerMenu = new QMenu(tr("&Layer"), this);
    mLayerMenu->addAction(mActionHandler->actionAddTileLayer());
    mLayerMenu->addAction(mActionHandler->actionAddObjectGroup());
    mLayerMenu->addAction(mActionHandler->actionAddImageLayer());
    mLayerMenu->addAction(mActionHandler->actionDuplicateLayer());
    mLayerMenu->addAction(mActionHandler->actionMergeLayerDown());
    mLayerMenu->addAction(mActionHandler->actionRemoveLayer());
    mLayerMenu->addAction(mActionHandler->actionRenameLayer());
    mLayerMenu->addSeparator();
    mLayerMenu->addAction(mActionHandler->actionSelectPreviousLayer());
    mLayerMenu->addAction(mActionHandler->actionSelectNextLayer());
    mLayerMenu->addAction(mActionHandler->actionMoveLayerUp());
    mLayerMenu->addAction(mActionHandler->actionMoveLayerDown());
    mLayerMenu->addSeparator();
    mLayerMenu->addAction(mActionHandler->actionToggleOtherLayers());
    mLayerMenu->addSeparator();
    mLayerMenu->addAction(mActionHandler->actionLayerProperties());

#ifdef ZOMBOID
    menuBar()->insertMenu(mUi->menuTools->menuAction(), mLayerMenu);
#else
    menuBar()->insertMenu(mUi->menuHelp->menuAction(), mLayerMenu);
#endif

    connect(mUi->actionNew, SIGNAL(triggered()), SLOT(newMap()));
    connect(mUi->actionOpen, SIGNAL(triggered()), SLOT(openFile()));
    connect(mUi->actionClearRecentFiles, SIGNAL(triggered()),
            SLOT(clearRecentFiles()));
    connect(mUi->actionSave, SIGNAL(triggered()), SLOT(saveFile()));
    connect(mUi->actionSaveAs, SIGNAL(triggered()), SLOT(saveFileAs()));
    connect(mUi->actionSaveAsImage, SIGNAL(triggered()), SLOT(saveAsImage()));
    connect(mUi->actionExport, SIGNAL(triggered()), SLOT(exportAs()));
    connect(mUi->actionClose, SIGNAL(triggered()), SLOT(closeFile()));
    connect(mUi->actionCloseAll, SIGNAL(triggered()), SLOT(closeAllFiles()));
    connect(mUi->actionQuit, SIGNAL(triggered()), SLOT(close()));

    connect(mUi->actionCut, SIGNAL(triggered()), SLOT(cut()));
    connect(mUi->actionCopy, SIGNAL(triggered()), SLOT(copy()));
    connect(mUi->actionPaste, SIGNAL(triggered()), SLOT(paste()));
    connect(mUi->actionDelete, SIGNAL(triggered()), SLOT(delete_()));
    connect(mUi->actionPreferences, SIGNAL(triggered()),
            SLOT(openPreferences()));

    connect(mUi->actionShowGrid, SIGNAL(toggled(bool)),
            preferences, SLOT(setShowGrid(bool)));
    connect(mUi->actionSnapToGrid, SIGNAL(toggled(bool)),
            preferences, SLOT(setSnapToGrid(bool)));
    connect(mUi->actionHighlightCurrentLayer, SIGNAL(toggled(bool)),
            preferences, SLOT(setHighlightCurrentLayer(bool)));
#ifdef ZOMBOID
    connect(mUi->actionHighlightRoomUnderPointer, SIGNAL(toggled(bool)),
            preferences, SLOT(setHighlightRoomUnderPointer(bool)));
    connect(mUi->actionShowMiniMap, SIGNAL(toggled(bool)),
            preferences, SLOT(setShowMiniMap(bool)));
    connect(mUi->actionShowTileLayersPanel, SIGNAL(toggled(bool)),
            preferences, SLOT(setShowTileLayersPanel(bool)));
#endif
    connect(mUi->actionZoomIn, SIGNAL(triggered()), SLOT(zoomIn()));
    connect(mUi->actionZoomOut, SIGNAL(triggered()), SLOT(zoomOut()));
    connect(mUi->actionZoomNormal, SIGNAL(triggered()), SLOT(zoomNormal()));

    connect(mUi->actionNewTileset, SIGNAL(triggered()), SLOT(newTileset()));
    connect(mUi->actionAddExternalTileset, SIGNAL(triggered()),
            SLOT(addExternalTileset()));
    connect(mUi->actionResizeMap, SIGNAL(triggered()), SLOT(resizeMap()));
    connect(mUi->actionOffsetMap, SIGNAL(triggered()), SLOT(offsetMap()));
    connect(mUi->actionMapProperties, SIGNAL(triggered()),
            SLOT(editMapProperties()));
    connect(mUi->actionAutoMap, SIGNAL(triggered()), SLOT(autoMap()));
#ifdef ZOMBOID
    connect(mUi->actionConvertToLot, SIGNAL(triggered()),
            SLOT(convertToLot()));
    connect(mUi->actionConvertOrientation, SIGNAL(triggered()),
            SLOT(convertOrientation()));
    connect(mUi->actionRoomDefGo, SIGNAL(triggered()),
            SLOT(RoomDefGo()));
    connect(mUi->actionRoomDefMerge, SIGNAL(triggered()),
            SLOT(RoomDefMerge()));
    connect(mUi->actionRoomDefRemove, SIGNAL(triggered()),
            SLOT(RoomDefRemove()));
    connect(mUi->actionRoomDefUnknownWalls, SIGNAL(triggered()),
            SLOT(RoomDefUnknownWalls()));
    connect(mUi->actionLuaScript, SIGNAL(triggered()), SLOT(LuaConsole()));
#endif

    connect(mActionHandler->actionLayerProperties(), SIGNAL(triggered()),
            SLOT(editLayerProperties()));

    connect(mUi->actionAbout, SIGNAL(triggered()), SLOT(aboutTiled()));
    connect(mUi->actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
#ifdef ZOMBOID
    connect(mUi->actionHelpContents, SIGNAL(triggered()), SLOT(helpContents()));
#endif

    connect(mTilesetDock, SIGNAL(tilesetsDropped(QStringList)),
            SLOT(newTilesets(QStringList)));

    // Add recent file actions to the recent files menu
    for (int i = 0; i < MaxRecentFiles; ++i)
    {
         mRecentFiles[i] = new QAction(this);
         mUi->menuRecentFiles->insertAction(mUi->actionClearRecentFiles,
                                            mRecentFiles[i]);
         mRecentFiles[i]->setVisible(false);
         connect(mRecentFiles[i], SIGNAL(triggered()),
                 this, SLOT(openRecentFile()));
    }
    mUi->menuRecentFiles->insertSeparator(mUi->actionClearRecentFiles);

    setThemeIcon(mUi->actionNew, "document-new");
    setThemeIcon(mUi->actionOpen, "document-open");
    setThemeIcon(mUi->menuRecentFiles, "document-open-recent");
    setThemeIcon(mUi->actionClearRecentFiles, "edit-clear");
    setThemeIcon(mUi->actionSave, "document-save");
    setThemeIcon(mUi->actionSaveAs, "document-save-as");
    setThemeIcon(mUi->actionClose, "window-close");
    setThemeIcon(mUi->actionQuit, "application-exit");
    setThemeIcon(mUi->actionCut, "edit-cut");
    setThemeIcon(mUi->actionCopy, "edit-copy");
    setThemeIcon(mUi->actionPaste, "edit-paste");
    setThemeIcon(mUi->actionDelete, "edit-delete");
    setThemeIcon(redoAction, "edit-redo");
    setThemeIcon(undoAction, "edit-undo");
    setThemeIcon(mUi->actionZoomIn, "zoom-in");
    setThemeIcon(mUi->actionZoomOut, "zoom-out");
    setThemeIcon(mUi->actionZoomNormal, "zoom-original");
    setThemeIcon(mUi->actionNewTileset, "document-new");
    setThemeIcon(mUi->actionResizeMap, "document-page-setup");
    setThemeIcon(mUi->actionMapProperties, "document-properties");
    setThemeIcon(mUi->actionAbout, "help-about");

    mStampBrush = new StampBrush(this);
    mBucketFillTool = new BucketFillTool(this);
    CreateObjectTool *tileObjectsTool = new CreateObjectTool(
            CreateObjectTool::CreateTile, this);
    CreateObjectTool *areaObjectsTool = new CreateObjectTool(
            CreateObjectTool::CreateArea, this);
    CreateObjectTool *polygonObjectsTool = new CreateObjectTool(
            CreateObjectTool::CreatePolygon, this);
    CreateObjectTool *polylineObjectsTool = new CreateObjectTool(
            CreateObjectTool::CreatePolyline, this);

    connect(mTilesetDock, SIGNAL(currentTilesChanged(const TileLayer*)),
            this, SLOT(setStampBrush(const TileLayer*)));
    connect(mStampBrush, SIGNAL(currentTilesChanged(const TileLayer*)),
            this, SLOT(setStampBrush(const TileLayer*)));
    connect(mTilesetDock, SIGNAL(currentTileChanged(Tile*)),
            tileObjectsTool, SLOT(setTile(Tile*)));
#ifdef ZOMBOID
    connect(mStampBrush, SIGNAL(tilePicked(Tile*)),
            SLOT(tilePicked(Tile*)));
    connect(mStampBrush, SIGNAL(altHover(QPoint)),
            SLOT(stampAltHovered(QPoint)));
    connect(mTileLayersPanel, SIGNAL(tilePicked(Tile*)),
            SLOT(tilePicked(Tile*)));
#endif

    connect(mRandomButton, SIGNAL(toggled(bool)),
            mStampBrush, SLOT(setRandom(bool)));
    connect(mRandomButton, SIGNAL(toggled(bool)),
            mBucketFillTool, SLOT(setRandom(bool)));

    ToolManager *toolManager = ToolManager::instance();
    toolManager->registerTool(mStampBrush);
    toolManager->registerTool(mBucketFillTool);
    toolManager->registerTool(new Eraser(this));
    toolManager->registerTool(new TileSelectionTool(this));
#ifdef ZOMBOID
    toolManager->registerTool(new PickTileTool(this));
#endif
    toolManager->addSeparator();
    toolManager->registerTool(new ObjectSelectionTool(this));
    toolManager->registerTool(new EditPolygonTool(this));
    toolManager->registerTool(areaObjectsTool);
    toolManager->registerTool(tileObjectsTool);
    toolManager->registerTool(polygonObjectsTool);
    toolManager->registerTool(polylineObjectsTool);
#ifdef ZOMBOID
    toolManager->registerTool(new RoomDefTool(this));
    toolManager->addSeparator();
    toolManager->registerTool(BmpBrushTool::instance());
    toolManager->registerTool(BmpRectTool::instance());
    toolManager->registerTool(BmpBucketTool::instance());
    toolManager->registerTool(BmpSelectionTool::instance());
    toolManager->registerTool(BmpWandTool::instance());
    toolManager->registerTool(BmpEraserTool::instance());
    toolManager->registerTool(BmpToLayersTool::instance());
    toolManager->addSeparator();
    toolManager->registerTool(WorldLotTool::instance());

    QAction *brushSizeMinus = new QAction(this);
    brushSizeMinus->setShortcut(QKeySequence(QLatin1String("[")));
    connect(brushSizeMinus, SIGNAL(triggered()), SLOT(brushSizeMinus()));
    addAction(brushSizeMinus);

    QAction *brushSizePlus = new QAction(this);
    brushSizePlus->setShortcut(QKeySequence(QLatin1String("]")));
    connect(brushSizePlus, SIGNAL(triggered()), SLOT(brushSizePlus()));
    addAction(brushSizePlus);

    connect(PickTileTool::instancePtr(), SIGNAL(tilePicked(Tile*)),
            SLOT(tilePicked(Tile*)));
#endif

    addToolBar(toolManager->toolBar());

#ifdef ZOMBOID
    mStatusInfoLabel->setObjectName(QLatin1String("statusInfoLabel"));
    mStatusInfoLabel->setAlignment(Qt::AlignCenter);
    resizeStatusInfoLabel();
    statusBarLayout->addWidget(mStatusInfoLabel);
#else
    statusBar()->addWidget(mStatusInfoLabel);
#endif
    connect(toolManager, SIGNAL(statusInfoChanged(QString)),
            this, SLOT(updateStatusInfoLabel(QString)));
#ifdef ZOMBOID
    mCurrentLevelButton->setObjectName(QLatin1String("currentLevelButton"));
    mCurrentLayerButton->setObjectName(QLatin1String("currentLayerButton"));
    connect(mCurrentLevelMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowLevelMenu()));
    connect(mCurrentLayerMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowLayerMenu()));
    connect(mCurrentLevelMenu, SIGNAL(triggered(QAction*)), SLOT(triggeredLevelMenu(QAction*)));
    connect(mCurrentLayerMenu, SIGNAL(triggered(QAction*)), SLOT(triggeredLayerMenu(QAction*)));
    mCurrentLevelButton->setMenu(mCurrentLevelMenu);
    mCurrentLayerButton->setMenu(mCurrentLayerMenu);
    mCurrentLevelButton->setPopupMode(QToolButton::InstantPopup);
    mCurrentLayerButton->setPopupMode(QToolButton::InstantPopup);
    statusBarLayout->addWidget(mCurrentLevelButton);
    statusBarLayout->addWidget(mCurrentLayerButton);
    statusBarLayout->addStretch();
    mZoomComboBox->setObjectName(QLatin1String("zoomComboBox"));
    statusBarLayout->addWidget(mZoomComboBox);
#else
    statusBar()->addWidget(mCurrentLayerLabel);
#endif
    mUi->menuView->addSeparator();
    mUi->menuView->addAction(mTilesetDock->toggleViewAction());
    mUi->menuView->addAction(mLayerDock->toggleViewAction());
    mUi->menuView->addAction(undoDock->toggleViewAction());
    mUi->menuView->addAction(mObjectsDock->toggleViewAction());
#ifdef ZOMBOID
    mUi->menuView->addAction(mLevelsDock->toggleViewAction());
    mUi->menuView->addAction(mWorldEdDock->toggleViewAction());
    mUi->menuView->addAction(mMapsDock->toggleViewAction());
#endif

    connect(mClipboardManager, SIGNAL(hasMapChanged()), SLOT(updateActions()));

    connect(mDocumentManager, SIGNAL(currentDocumentChanged(MapDocument*)),
            SLOT(mapDocumentChanged(MapDocument*)));
    connect(mDocumentManager, SIGNAL(documentCloseRequested(int)),
            this, SLOT(closeMapDocument(int)));

    QShortcut *switchToLeftDocument = new QShortcut(tr("Ctrl+PgUp"), this);
    connect(switchToLeftDocument, SIGNAL(activated()),
            mDocumentManager, SLOT(switchToLeftDocument()));
    QShortcut *switchToLeftDocument1 = new QShortcut(tr("Ctrl+Shift+Tab"), this);
    connect(switchToLeftDocument1, SIGNAL(activated()),
            mDocumentManager, SLOT(switchToLeftDocument()));

    QShortcut *switchToRightDocument = new QShortcut(tr("Ctrl+PgDown"), this);
    connect(switchToRightDocument, SIGNAL(activated()),
            mDocumentManager, SLOT(switchToRightDocument()));
    QShortcut *switchToRightDocument1 = new QShortcut(tr("Ctrl+Tab"), this);
    connect(switchToRightDocument1, SIGNAL(activated()),
            mDocumentManager, SLOT(switchToRightDocument()));

    new QShortcut(tr("X"), this, SLOT(flipStampHorizontally()));
    new QShortcut(tr("Y"), this, SLOT(flipStampVertically()));
    new QShortcut(tr("Z"), this, SLOT(rotateStampRight()));
    new QShortcut(tr("Shift+Z"), this, SLOT(rotateStampLeft()));

    QShortcut *copyPositionShortcut = new QShortcut(tr("Alt+C"), this);
    connect(copyPositionShortcut, SIGNAL(activated()),
            mActionHandler, SLOT(copyPosition()));

#ifdef ZOMBOID
    connect(mUi->actionBuildingEditor, SIGNAL(triggered()),
            SLOT(showBuildingEditor()));
    connect(mUi->actionTilesetMetaInfo, SIGNAL(triggered()),
            SLOT(tilesetMetaInfoDialog()));
    connect(mUi->actionTileProperties, SIGNAL(triggered()),
            SLOT(tilePropertiesEditor()));
    connect(mUi->actionWorldEd, SIGNAL(triggered()),
            SLOT(launchWorldEd()));
#endif

    updateActions();
#ifdef ZOMBOID
    // Something broke when I replaced the statusBar with a QFrame.
    // The dock widget geometry (specifically the width of the right-side dock
    // area) was no longer restored properly when the window was maximized.
    // So now I call readSettings() *after* the MainWindow is shown (see main.cpp).
    // Possibly related to https://bugreports.qt-project.org/browse/QTBUG-15080
#else
    readSettings();
#endif
    setupQuickStamps();

    connect(AutomappingManager::instance(), SIGNAL(warningsOccurred()),
            this, SLOT(autoMappingWarning()));
    connect(AutomappingManager::instance(), SIGNAL(errorsOccurred()),
            this, SLOT(autoMappingError()));
}

MainWindow::~MainWindow()
{
    mDocumentManager->closeAllDocuments();

    AutomappingManager::deleteInstance();
    QuickStampManager::deleteInstance();
    ToolManager::deleteInstance();
#ifdef ZOMBOID
#if 1
    BuildingTemplates::deleteInstance();
    BuildingTilesMgr::deleteInstance(); // Ensure all the tilesets are released
    BuildingTMX::deleteInstance();
    BuildingPreferences::deleteInstance();
#endif
    MapImageManager::deleteInstance();
    MapManager::deleteInstance();
    TileMetaInfoMgr::deleteInstance();
    TileDefDialog::deleteInstance();
    TilePropertyMgr::deleteInstance();
#endif
    TilesetManager::deleteInstance();
    DocumentManager::deleteInstance();
    Preferences::deleteInstance();
    LanguageManager::deleteInstance();
    PluginManager::deleteInstance();

    delete mUi;
}

void MainWindow::commitData(QSessionManager &manager)
{
    // Play nice with session management and cancel shutdown process when user
    // requests this
    if (manager.allowsInteraction())
        if (!confirmAllSave())
            manager.cancel();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#ifdef ZOMBOID
    writeSettings();
    if (confirmAllSave() &&
            TileDefDialog::closeYerself() &&
            (!mBuildingEditor || mBuildingEditor->closeYerself())) {
        event->accept();
    } else
        event->ignore();
#else
    writeSettings();

    if (confirmAllSave())
        event->accept();
    else
        event->ignore();
#endif
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        mUi->retranslateUi(this);
        retranslateUi();
        break;
    default:
        break;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
        if (MapView *mapView = mDocumentManager->currentMapView())
            mapView->setHandScrolling(true);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat())
        if (MapView *mapView = mDocumentManager->currentMapView())
            mapView->setHandScrolling(false);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    const QList<QUrl> urls = e->mimeData()->urls();
    if (!urls.isEmpty() && !urls.at(0).toLocalFile().isEmpty())
        e->accept();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    foreach (const QUrl &url, e->mimeData()->urls())
        openFile(url.toLocalFile());
}

void MainWindow::newMap()
{
    NewMapDialog newMapDialog(this);
    MapDocument *mapDocument = newMapDialog.createMap();

    if (!mapDocument)
        return;

    addMapDocument(mapDocument);
}

bool MainWindow::openFile(const QString &fileName,
                          MapReaderInterface *mapReader)
{
    if (fileName.isEmpty())
        return false;

#ifdef BUILDINGED
    if (fileName.endsWith(QLatin1String(".tbx"))) {
        showBuildingEditor();
        if (mBuildingEditor)
            return mBuildingEditor->openFile(fileName);
        return false;
    }
#endif

    // Select existing document if this file is already open
    int documentIndex = mDocumentManager->findDocument(fileName);
    if (documentIndex != -1) {
        mDocumentManager->switchToDocument(documentIndex);
        return true;
    }

    TmxMapReader tmxMapReader;

    if (!mapReader && !tmxMapReader.supportsFile(fileName)) {
        // Try to find a plugin that implements support for this format
        const PluginManager *pm = PluginManager::instance();
        QList<MapReaderInterface*> readers =
                pm->interfaces<MapReaderInterface>();

        foreach (MapReaderInterface *reader, readers) {
            if (reader->supportsFile(fileName)) {
                mapReader = reader;
                break;
            }
        }
    }

    if (!mapReader)
        mapReader = &tmxMapReader;

#ifdef ZOMBOID
    QFileInfo fileInfo(fileName);
    PROGRESS progress(tr("Reading %1").arg(fileInfo.fileName()));
#endif

    Map *map = mapReader->read(fileName);
    if (!map) {
        QMessageBox::critical(this, tr("Error Opening Map"),
                              mapReader->errorString());
        return false;
    }

    addMapDocument(new MapDocument(map, fileName));

    setRecentFile(fileName);
    return true;
}

bool MainWindow::openFile(const QString &fileName)
{
    return openFile(fileName, 0);
}

void MainWindow::openLastFiles()
{
#ifdef ZOMBOID
    foreach (QString f, Preferences::instance()->worldedFiles())
        if (!f.isEmpty() && QFileInfo(f).exists())
            WorldEd::WorldEdMgr::instance()->addProject(f);
#endif

    mSettings.beginGroup(QLatin1String("recentFiles"));

    QStringList lastOpenFiles = mSettings.value(
                QLatin1String("lastOpenFiles")).toStringList();
    QVariant openCountVariant = mSettings.value(
                QLatin1String("recentOpenedFiles"));

    // Backwards compatibility mode
    if (openCountVariant.isValid()) {
        const QStringList recentFiles = mSettings.value(
                    QLatin1String("fileNames")).toStringList();
        int openCount = qMin(openCountVariant.toInt(), recentFiles.size());
        for (; openCount; --openCount)
            lastOpenFiles.append(recentFiles.at(openCount - 1));
        mSettings.remove(QLatin1String("recentOpenedFiles"));
    }

    QStringList mapScales = mSettings.value(
                QLatin1String("mapScale")).toStringList();
    QStringList scrollX = mSettings.value(
                QLatin1String("scrollX")).toStringList();
    QStringList scrollY = mSettings.value(
                QLatin1String("scrollY")).toStringList();
    QStringList selectedLayer = mSettings.value(
                QLatin1String("selectedLayer")).toStringList();

#ifdef ZOMBOID
    PROGRESS *progress = lastOpenFiles.size() ? new PROGRESS(tr("Restoring session")) : 0;
#endif

    for (int i = 0; i < lastOpenFiles.size(); i++) {
        if (!(i < mapScales.size()))
            continue;
        if (!(i < scrollX.size()))
            continue;
        if (!(i < scrollY.size()))
            continue;
        if (!(i < selectedLayer.size()))
            continue;

        if (openFile(lastOpenFiles.at(i))) {
            MapView *mapView = mDocumentManager->currentMapView();

            // Restore camera to the previous position
            qreal scale = mapScales.at(i).toDouble();
            if (scale > 0)
                mapView->zoomable()->setScale(scale);

            const int hor = scrollX.at(i).toInt();
            const int ver = scrollY.at(i).toInt();
            mapView->horizontalScrollBar()->setSliderPosition(hor);
            mapView->verticalScrollBar()->setSliderPosition(ver);

            int layer = selectedLayer.at(i).toInt();
            if (layer > 0 && layer < mMapDocument->map()->layerCount())
                mMapDocument->setCurrentLayerIndex(layer);
        }
    }
    QString lastActiveDocument =
            mSettings.value(QLatin1String("lastActive")).toString();
    int documentIndex = mDocumentManager->findDocument(lastActiveDocument);
    if (documentIndex != -1)
        mDocumentManager->switchToDocument(documentIndex);

    mSettings.endGroup();

#ifdef ZOMBOID
    delete progress;
#endif
}

#ifdef ZOMBOID
bool MainWindow::InitConfigFiles()
{
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // Create ~/.TileZed if needed.
    QString configPath = Preferences::instance()->configPath();
    QDir dir(configPath);
    if (!dir.exists()) {
        if (!dir.mkpath(configPath)) {
            QMessageBox::critical(this, tr("It's no good, Jim!"),
                                  tr("Failed to create config directory:\n%1")
                                  .arg(QDir::toNativeSeparators(configPath)));
            return false;
        }
    }

    // Copy config files from the application directory to ~/.TileZed if they
    // don't exist there.
    QStringList configFiles;
    configFiles += TileMetaInfoMgr::instance()->txtName();
    configFiles += BuildingTemplates::instance()->txtName();
    configFiles += BuildingTilesMgr::instance()->txtName();
    configFiles += BuildingTMX::instance()->txtName();
    configFiles += FurnitureGroups::instance()->txtName();

    foreach (QString configFile, configFiles) {
        QString fileName = configPath + QLatin1Char('/') + configFile;
        if (!QFileInfo(fileName).exists()) {
            QString source = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                    + configFile;
            if (QFileInfo(source).exists()) {
                if (!QFile::copy(source, fileName)) {
                    QMessageBox::critical(this, tr("It's no good, Jim!"),
                                          tr("Failed to copy file:\nFrom: %1\nTo: %2")
                                          .arg(source).arg(fileName));
                    return false;
                }
            }
        }
    }

    // Read Tilesets.txt before TMXConfig.txt in case we are upgrading
    // TMXConfig.txt from VERSION0 to VERSION1.
    if (!TileMetaInfoMgr::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("%1\n(while reading %2)")
                              .arg(TileMetaInfoMgr::instance()->errorString())
                              .arg(TileMetaInfoMgr::instance()->txtName()));
        return false;
    }

    if (!BuildingTMX::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(BuildingTMX::instance()->txtName())
                              .arg(BuildingTMX::instance()->errorString()));
        return false;
    }

    if (!BuildingTilesMgr::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(BuildingTilesMgr::instance()->txtName())
                              .arg(BuildingTilesMgr::instance()->errorString()));
        return false;
    }

    if (!FurnitureGroups::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(FurnitureGroups::instance()->txtName())
                              .arg(FurnitureGroups::instance()->errorString()));
        return false;
    }

    if (!BuildingTemplates::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(BuildingTemplates::instance()->txtName())
                              .arg(BuildingTemplates::instance()->errorString()));
        return false;
    }

    return true;
}
#endif // ZOMBOID

void MainWindow::openFile()
{
    QString filter = tr("All Files (*)");
    filter += QLatin1String(";;");

    QString selectedFilter = tr("Tiled map files (*.tmx)");
    filter += selectedFilter;

    selectedFilter = mSettings.value(QLatin1String("lastUsedOpenFilter"),
                                     selectedFilter).toString();

    const PluginManager *pm = PluginManager::instance();
    QList<MapReaderInterface*> readers = pm->interfaces<MapReaderInterface>();
    foreach (const MapReaderInterface *reader, readers) {
        foreach (const QString &str, reader->nameFilters()) {
            if (!str.isEmpty()) {
                filter += QLatin1String(";;");
                filter += str;
            }
        }
    }

    QStringList fileNames = QFileDialog::getOpenFileNames(this, tr("Open Map"),
                                                    fileDialogStartLocation(),
                                                    filter, &selectedFilter);
    if (fileNames.isEmpty())
        return;

    // When a particular filter was selected, use the associated reader
    MapReaderInterface *mapReader = 0;
    foreach (MapReaderInterface *reader, readers) {
        if (reader->nameFilters().contains(selectedFilter))
            mapReader = reader;
    }

    mSettings.setValue(QLatin1String("lastUsedOpenFilter"), selectedFilter);
    foreach (const QString &fileName, fileNames)
        openFile(fileName, mapReader);
}

bool MainWindow::saveFile(const QString &fileName)
{
    if (!mMapDocument)
        return false;

    QString error;
    if (!mMapDocument->save(fileName, &error)) {
        QMessageBox::critical(this, tr("Error Saving Map"), error);
        return false;
    }

    setRecentFile(fileName);
    return true;
}

bool MainWindow::saveFile()
{
    if (!mMapDocument)
        return false;

    const QString currentFileName = mMapDocument->fileName();

    if (currentFileName.endsWith(QLatin1String(".tmx"), Qt::CaseInsensitive))
        return saveFile(currentFileName);
    else
        return saveFileAs();
}

bool MainWindow::saveFileAs()
{
    QString suggestedFileName;
    if (mMapDocument && !mMapDocument->fileName().isEmpty()) {
        const QFileInfo fileInfo(mMapDocument->fileName());
        suggestedFileName = fileInfo.path();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += fileInfo.completeBaseName();
        suggestedFileName += QLatin1String(".tmx");
    } else {
        suggestedFileName = fileDialogStartLocation();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += tr("untitled.tmx");
    }

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), suggestedFileName,
                                         tr("Tiled map files (*.tmx)"));
    if (!fileName.isEmpty())
        return saveFile(fileName);
    return false;
}

bool MainWindow::confirmSave()
{
    if (!mMapDocument || !mMapDocument->isModified())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return saveFile();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

bool MainWindow::confirmAllSave()
{
    for (int i = 0; i < mDocumentManager->documentCount(); i++) {
#ifdef ZOMBOID
        if (!mDocumentManager->documents().at(i)->isModified())
            continue;
#endif
        mDocumentManager->switchToDocument(i);
        if (!confirmSave())
            return false;
    }

    return true;
}

void MainWindow::saveAsImage()
{
    if (!mMapDocument)
        return;

    MapView *mapView = mDocumentManager->currentMapView();
    SaveAsImageDialog dialog(mMapDocument,
                             mMapDocument->fileName(),
                             mapView->zoomable()->scale(),
                             this);
    dialog.exec();
}

void MainWindow::exportAs()
{
    if (!mMapDocument)
        return;

    PluginManager *pm = PluginManager::instance();
    QList<MapWriterInterface*> writers = pm->interfaces<MapWriterInterface>();
    QString filter = tr("All Files (*)");
    foreach (const MapWriterInterface *writer, writers) {
        foreach (const QString &str, writer->nameFilters()) {
            if (!str.isEmpty()) {
                filter += QLatin1String(";;");
                filter += str;
            }
        }
    }

    QString selectedFilter =
            mSettings.value(QLatin1String("lastUsedExportFilter")).toString();

    QFileInfo baseNameInfo = QFileInfo(mMapDocument->fileName());
    QString baseName = baseNameInfo.baseName();

    QRegExp extensionFinder(QLatin1String("\\(\\*\\.([^\\)\\s]*)"));
    extensionFinder.indexIn(selectedFilter);
    const QString extension = extensionFinder.cap(1);

    Preferences *pref = Preferences::instance();
    QString lastExportedFilePath = pref->lastPath(Preferences::ExportedFile);

    QString suggestedFilename = lastExportedFilePath
                                + QLatin1String("/") + baseName
                                + QLatin1Char('.') + extension;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Export As..."),
                                                    suggestedFilename,
                                                    filter, &selectedFilter);
    if (fileName.isEmpty())
        return;

    pref->setLastPath(Preferences::ExportedFile, QFileInfo(fileName).path());

    MapWriterInterface *chosenWriter = 0;

    // If a specific filter was selected, use that writer
    foreach (MapWriterInterface *writer, writers)
        if (writer->nameFilters().contains(selectedFilter))
            chosenWriter = writer;

    // If not, try to find the file extension among the name filters
    QString suffix = QFileInfo(fileName).completeSuffix();
    if (!chosenWriter && !suffix.isEmpty()) {
        suffix.prepend(QLatin1String("*."));

        foreach (MapWriterInterface *writer, writers) {
            if (!writer->nameFilters().filter(suffix,
                                              Qt::CaseInsensitive).isEmpty()) {
                if (chosenWriter) {
                    QMessageBox::warning(this, tr("Non-unique file extension"),
                                         tr("Non-unique file extension.\n"
                                            "Please select specific format."));
                    exportAs();
                    return;
                } else {
                    chosenWriter = writer;
                }
            }
        }
    }

    // Also support exporting to the TMX map format when requested
    TmxMapWriter tmxMapWriter;
    if (!chosenWriter && fileName.endsWith(QLatin1String(".tmx"),
                                           Qt::CaseInsensitive))
        chosenWriter = &tmxMapWriter;

    if (!chosenWriter) {
        QMessageBox::critical(this, tr("Unknown File Format"),
                              tr("The given filename does not have any known "
                                 "file extension."));
        return;
    }

    mSettings.setValue(QLatin1String("lastUsedExportFilter"), selectedFilter);

    if (!chosenWriter->write(mMapDocument->map(), fileName)) {
        QMessageBox::critical(this, tr("Error Saving Map"),
                              chosenWriter->errorString());
    }
}

void MainWindow::closeFile()
{
    if (confirmSave())
        mDocumentManager->closeCurrentDocument();
}

void MainWindow::closeAllFiles()
{
    if (confirmAllSave())
        mDocumentManager->closeAllDocuments();
}

void MainWindow::cut()
{
    if (!mMapDocument)
        return;

    Layer *currentLayer = mMapDocument->currentLayer();
    if (!currentLayer)
        return;

    TileLayer *tileLayer = dynamic_cast<TileLayer*>(currentLayer);
    const QRegion &tileSelection = mMapDocument->tileSelection();
    const QList<MapObject*> &selectedObjects = mMapDocument->selectedObjects();

    copy();

    QUndoStack *stack = mMapDocument->undoStack();
    stack->beginMacro(tr("Cut"));

    if (tileLayer && !tileSelection.isEmpty()) {
        stack->push(new EraseTiles(mMapDocument, tileLayer, tileSelection));
    } else if (!selectedObjects.isEmpty()) {
        foreach (MapObject *mapObject, selectedObjects)
            stack->push(new RemoveMapObject(mMapDocument, mapObject));
    }

    mActionHandler->selectNone();

    stack->endMacro();
}

void MainWindow::copy()
{
    if (!mMapDocument)
        return;

    mClipboardManager->copySelection(mMapDocument);
}

void MainWindow::paste()
{
    if (!mMapDocument)
        return;

    Layer *currentLayer = mMapDocument->currentLayer();
    if (!currentLayer)
        return;

    Map *map = mClipboardManager->map();
    if (!map)
        return;

    // We can currently only handle maps with a single layer
    if (map->layerCount() != 1) {
        // Need to clean up the tilesets since they didn't get an owner
        qDeleteAll(map->tilesets());
        delete map;
        return;
    }

    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->addReferences(map->tilesets());

    mMapDocument->unifyTilesets(map);
    Layer *layer = map->layerAt(0);

    if (TileLayer *tileLayer = layer->asTileLayer()) {
        // Reset selection and paste into the stamp brush
        mActionHandler->selectNone();
        setStampBrush(tileLayer);
        ToolManager::instance()->selectTool(mStampBrush);
    } else if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
        if (ObjectGroup *currentObjectGroup = currentLayer->asObjectGroup()) {
            // Determine where to insert the objects
            const QPointF center = objectGroup->objectsBoundingRect().center();
            const MapView *view = mDocumentManager->currentMapView();

            // Take the mouse position if the mouse is on the view, otherwise
            // take the center of the view.
            QPoint viewPos;
            if (view->underMouse())
                viewPos = view->mapFromGlobal(QCursor::pos());
            else
                viewPos = QPoint(view->width() / 2, view->height() / 2);

            const MapRenderer *renderer = mMapDocument->renderer();
            const QPointF scenePos = view->mapToScene(viewPos);
            QPointF insertPos = renderer->pixelToTileCoords(scenePos);
            if (Preferences::instance()->snapToGrid())
                insertPos = insertPos.toPoint();
            const QPointF offset = insertPos - center;

            QUndoStack *undoStack = mMapDocument->undoStack();
            QList<MapObject*> pastedObjects;
#if QT_VERSION >= 0x040700
            pastedObjects.reserve(objectGroup->objectCount());
#endif
            undoStack->beginMacro(tr("Paste Objects"));
            foreach (const MapObject *mapObject, objectGroup->objects()) {
                MapObject *objectClone = mapObject->clone();
                objectClone->setPosition(objectClone->position() + offset);
                pastedObjects.append(objectClone);
                undoStack->push(new AddMapObject(mMapDocument,
                                                 currentObjectGroup,
                                                 objectClone));
            }
            undoStack->endMacro();

            mMapDocument->setSelectedObjects(pastedObjects);
        }
    }

    tilesetManager->removeReferences(map->tilesets());
    delete map;
}

void MainWindow::delete_()
{
    if (!mMapDocument)
        return;

    Layer *currentLayer = mMapDocument->currentLayer();
    if (!currentLayer)
        return;

    TileLayer *tileLayer = dynamic_cast<TileLayer*>(currentLayer);
    const QRegion &tileSelection = mMapDocument->tileSelection();
    const QList<MapObject*> &selectedObjects = mMapDocument->selectedObjects();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Delete"));

#ifdef ZOMBOID
    AbstractTool *tool = ToolManager::instance()->selectedTool();
    if (tool && dynamic_cast<AbstractBmpTool*>(tool)) {
        QRegion selection = mMapDocument->bmpSelection();
        if (!selection.isEmpty()) {
            QRect r = selection.boundingRect();
            int x = r.x(), y = r.y();
            QImage image(r.size(), QImage::Format_ARGB32);
            image.fill(qRgb(0, 0, 0));
            undoStack->push(new PaintBMP(mMapDocument,
                                         BmpBrushTool::instance()->bmpIndex(),
                                         x, y, image, selection));
        }
    } else
#endif // ZOMBOID
    if (tileLayer && !tileSelection.isEmpty()) {
        undoStack->push(new EraseTiles(mMapDocument, tileLayer, tileSelection));
    } else if (!selectedObjects.isEmpty()) {
        foreach (MapObject *mapObject, selectedObjects)
            undoStack->push(new RemoveMapObject(mMapDocument, mapObject));
    }
#ifndef ZOMBOID
    mActionHandler->selectNone();
#endif
    undoStack->endMacro();
}

void MainWindow::openPreferences()
{
    PreferencesDialog preferencesDialog(this);
    preferencesDialog.exec();
}

void MainWindow::zoomIn()
{
    if (MapView *mapView = mDocumentManager->currentMapView())
        mapView->zoomable()->zoomIn();
}

void MainWindow::zoomOut()
{
    if (MapView *mapView = mDocumentManager->currentMapView())
        mapView->zoomable()->zoomOut();
}

void MainWindow::zoomNormal()
{
    if (MapView *mapView = mDocumentManager->currentMapView())
        mapView->zoomable()->resetZoom();
}

bool MainWindow::newTileset(const QString &path)
{
    if (!mMapDocument)
        return false;

    Map *map = mMapDocument->map();
    Preferences *prefs = Preferences::instance();

    const QString startLocation = path.isEmpty()
            ? QFileInfo(prefs->lastPath(Preferences::ImageFile)).absolutePath()
            : path;

    NewTilesetDialog newTileset(startLocation, this);
    newTileset.setTileWidth(map->tileWidth());
    newTileset.setTileHeight(map->tileHeight());

    if (Tileset *tileset = newTileset.createTileset()) {
        mMapDocument->undoStack()->push(new AddTileset(mMapDocument, tileset));
        prefs->setLastPath(Preferences::ImageFile, tileset->imageSource());
        return true;
    }
    return false;
}

void MainWindow::newTilesets(const QStringList &paths)
{
    foreach (const QString &path, paths)
        if (!newTileset(path))
            return;
}

void MainWindow::addExternalTileset()
{
    if (!mMapDocument)
        return;

    const QString start = fileDialogStartLocation();
    const QString fileName =
            QFileDialog::getOpenFileName(this, tr("Add External Tileset"),
                                         start,
                                         tr("Tiled tileset files (*.tsx)"));
    if (fileName.isEmpty())
        return;

    TmxMapReader reader;
    if (Tileset *tileset = reader.readTileset(fileName)) {
        mMapDocument->undoStack()->push(new AddTileset(mMapDocument, tileset));
    } else {
        QMessageBox::critical(this, tr("Error Reading Tileset"),
                              reader.errorString());
    }
}

void MainWindow::resizeMap()
{
    if (!mMapDocument)
        return;

    Map *map = mMapDocument->map();

    ResizeDialog resizeDialog(this);
    resizeDialog.setOldSize(map->size());

    if (resizeDialog.exec()) {
        const QSize &newSize = resizeDialog.newSize();
        const QPoint &offset = resizeDialog.offset();
        if (newSize != map->size() || !offset.isNull())
            mMapDocument->resizeMap(newSize, offset);
    }
}

void MainWindow::offsetMap()
{
    if (!mMapDocument)
        return;

    OffsetMapDialog offsetDialog(mMapDocument, this);
    if (offsetDialog.exec()) {
        const QList<int> layerIndexes = offsetDialog.affectedLayerIndexes();
        if (layerIndexes.empty())
            return;

        mMapDocument->offsetMap(layerIndexes,
                                offsetDialog.offset(),
                                offsetDialog.affectedBoundingRect(),
                                offsetDialog.wrapX(),
                                offsetDialog.wrapY());
    }
}

void MainWindow::editMapProperties()
{
    if (!mMapDocument)
        return;
    PropertiesDialog propertiesDialog(tr("Map"),
                                      mMapDocument->map(),
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void MainWindow::autoMap()
{
    AutomappingManager::instance()->autoMap();
}

void MainWindow::autoMappingWarning()
{
    const QString title = tr("Automatic Mapping Warning");
    QString warnings = AutomappingManager::instance()->warningString();
    if (!warnings.isEmpty()) {
        QMessageBox::warning(this, title, warnings);
    }
}

#ifdef ZOMBOID
void MainWindow::stampAltHovered(const QPoint &tilePos)
{
    mTileLayersPanel->setTilePosition(tilePos);
}

void MainWindow::tilePicked(Tile *tile)
{
    mTilesetDock->tilePicked(tile);

    QString tileName = BuildingTilesMgr::nameForTile(tile);
    if (mTileDefDialog && TileDefDialog::instance()->isVisible())
        TileDefDialog::instance()->displayTile(tileName);
}

void MainWindow::showBuildingEditor()
{
    if (!mBuildingEditor) {
        mBuildingEditor = new BuildingEditor::BuildingEditorWindow();
        mBuildingEditor->show();
        if (!mBuildingEditor->Startup()) {
            delete mBuildingEditor;
            mBuildingEditor = 0;
            return;
        }

        connect(mBuildingEditor, SIGNAL(tilePicked(QString)),
                SLOT(buildingTilePicked(QString)));
    }

    mBuildingEditor->show();
    mBuildingEditor->raise();
    mBuildingEditor->activateWindow();
}

void MainWindow::buildingTilePicked(const QString &tileName)
{
    if (mTileDefDialog && TileDefDialog::instance()->isVisible())
        TileDefDialog::instance()->displayTile(tileName);
}

void MainWindow::tilesetMetaInfoDialog()
{
    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
#if 1
    foreach (Tileset *ts, mgr->tilesets()) {
        if (ts->isMissing()) {
            PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
            mgr->loadTilesets();
            break;
        }
    }
#else
    if (!mgr->hasReadTxt()) {
        if (!mgr->readTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 tr("%1\n(while reading %2)")
                                 .arg(mgr->errorString())
                                 .arg(mgr->txtName()));
            TileMetaInfoMgr::deleteInstance();
            return;
        }
        PROGRESS progress(tr("Loading Tilesets.txt tilesets"));
        mgr->loadTilesets();
    }
#endif

    TileMetaInfoDialog dialog(this);
    dialog.exec();

    if (!mgr->writeTxt()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
    }
}

void MainWindow::tilePropertiesEditor()
{
    TilePropertyMgr *mgr = TilePropertyMgr::instance();
    if (!mgr->hasReadTxt()) {
        if (!mgr->readTxt()) {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 tr("%1\n(while reading %2)")
                                 .arg(mgr->errorString()).arg(mgr->txtName()));
            TilePropertyMgr::deleteInstance();
            return;
        }
    }
    mTileDefDialog = TileDefDialog::instance();
    TileDefDialog::instance()->show();
    TileDefDialog::instance()->raise();
}

void MainWindow::launchWorldEd()
{
    QString path = QApplication::applicationDirPath();
#ifdef Q_OS_WIN
    path += QLatin1String("/WorldEd/PZWorldEd.exe");
#elif defined(Q_OS_MAC)
    path += QLatin1String("/WorldEd/PZWorldEd"); // FIXME: .app ?
#else
    path += QLatin1String("/WorldEd/PZWorldEd");
#endif
    path = QDir::cleanPath(path);
    path = QDir::toNativeSeparators(path);
    if (QFileInfo(path).exists()) {
        QProcess::startDetached(path, QStringList());
    } else {
        QMessageBox::warning(this, tr("Error launching WorldEd"),
                             tr("Couldn't find WorldEd!\n%1").arg(path));
    }
}

void MainWindow::brushSizeMinus()
{
    int brushSize = BmpBrushTool::instance()->brushSize();
    if (brushSize > 1)
        BmpBrushTool::instance()->setBrushSize(brushSize - 1);
}

void MainWindow::brushSizePlus()
{
    int brushSize = BmpBrushTool::instance()->brushSize();
    if (brushSize < 300)
        BmpBrushTool::instance()->setBrushSize(brushSize + 1);
}
#endif

void MainWindow::autoMappingError()
{
    const QString title = tr("Automatic Mapping Error");
    QString error = AutomappingManager::instance()->errorString();
    if (!error.isEmpty()) {
        QMessageBox::critical(this, title, error);
    }
}

#ifdef ZOMBOID
void MainWindow::convertToLot()
{
    if (!mMapDocument)
        return;

    QRect bounds = mMapDocument->tileSelection().boundingRect();
    if (bounds.isEmpty())
        return;

    Map *map = mMapDocument->map();

    ConvertToLotDialog dialog(mMapDocument, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // If the tile selection is not in level 0, adjust the bounds.
    if (map->orientation() == Map::LevelIsometric)
        bounds.translate(mMapDocument->currentLevel() * QPoint(-3, -3));

    MapComposite *mapComposite = mMapDocument->mapComposite();
    QPoint mapOffset;
    Map::Orientation mapOrient = dialog.levelIsometric()
            ? Map::LevelIsometric
            : Map::Isometric;
    int mapWidth = bounds.width(), mapHeight = bounds.height();
    int maxLevel = 0;
    if (dialog.emptyLevels()) {
        maxLevel = mapComposite->maxLevel();
    } else {
        int numLevels = mapComposite->layerGroupCount();
        for (int level = numLevels - 1; level >= 0; --level) {
            CompositeLayerGroup *lg = mapComposite->tileLayersForLevel(level);
            bool empty = true;
            foreach (TileLayer *tl, lg->layers()) {
                QRect bounds2 = bounds;
                if (map->orientation() == Map::Isometric)
                    bounds2.translate(-lg->level() * 3, -lg->level() * 3);
                for (int y = bounds2.top(); y <= bounds2.bottom(); y++) {
                    for (int x = bounds2.left(); x <= bounds2.right(); x++) {
                        if (!tl->cellAt(x, y).isEmpty()) {
                            empty = false;
                            break;
                        }
                    }
                    if (!empty)
                        break;
                }
                if (!empty)
                    break;
            }
            if (!empty) {
                maxLevel = lg->level();
                break;
            }
        }
    }
    if (mapOrient == Map::Isometric) {
        int offset = maxLevel * 3;
        mapOffset.setX(offset);
        mapOffset.setY(offset);
        mapWidth += offset;
        mapHeight += offset;
    }
    Map *clone = new Map(mapOrient, mapWidth, mapHeight,
                     map->tileWidth(), map->tileHeight());
    foreach (Tileset *ts, map->tilesets())
        clone->addTileset(ts);

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Convert Selection To Lot"));

    QRegion oldSelection = mMapDocument->tileSelection();

    foreach (Layer *layer, map->layers()) {
        if (TileLayer *tl = layer->asTileLayer()) {
            int level = tl->level();
            if (level > maxLevel)
                continue;
            TileLayer *cloneLayer = new TileLayer(tl->name(), 0, 0,
                                                  mapWidth, mapHeight);
            clone->addLayer(cloneLayer);

            int offset = 0, offsetSrc = 0;
            if (mapOrient == Map::Isometric)
                offset = mapOffset.x() - level * 3;
            if (map->orientation() == Map::Isometric)
                offsetSrc = -level * 3;

            for (int y = bounds.top(); y <= bounds.bottom(); y++) {
                for (int x = bounds.left(); x <= bounds.right(); x++) {
                    if (x + offsetSrc < 0 || y + offsetSrc < 0)
                        continue;
                    cloneLayer->setCell(offset + x - bounds.left(),
                                        offset + y - bounds.top(),
                                        tl->cellAt(x + offsetSrc, y + offsetSrc));
                }
            }

            if (dialog.eraseSource()) {
                QRect eraseRect = bounds.translated(offsetSrc, offsetSrc);
                eraseRect &= tl->bounds();
                QRegion eraseRegion(eraseRect);
                qDebug() << tl->name() << eraseRect;

                // Must set the tileSelection to the area to erase otherwise
                // TilePainter::paintableRegion won't erase outside the
                // selection.
                if (eraseRegion != mMapDocument->tileSelection())
                    undoStack->push(new ChangeTileSelection(mMapDocument, eraseRegion));
                undoStack->push(new EraseTiles(mMapDocument, tl, eraseRegion));
            }
        }
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (og->name().endsWith(QLatin1String("RoomDefs"))) {
                ObjectGroup *cloneLayer = new ObjectGroup(og->name(), 0, 0,
                                                          mapWidth, mapHeight);
                cloneLayer->setColor(og->color());
                clone->addLayer(cloneLayer);

                QPoint offset = mapOffset;
                if (map->orientation() == Map::LevelIsometric &&
                        mapOrient == Map::Isometric)
                    offset -= QPoint(3, 3) * og->level();
                if (map->orientation() == Map::Isometric &&
                        mapOrient == Map::LevelIsometric)
                    offset = QPoint(3, 3) * og->level();

                QList<MapObject*> remove;
                foreach (MapObject *object, og->objects()) {
                    QRectF objectBounds = object->bounds();
                    if (map->orientation() == Map::Isometric)
                        objectBounds.translate(og->level() * QPointF(3, 3));
                    if (objectBounds.intersects(bounds)) {
                        cloneLayer->addObject(new MapObject(object->name(), object->type(),
                                                            offset + object->position() - bounds.topLeft(),
                                                            object->size()));
                        remove += object;
                    }
                }

                if (dialog.eraseSource()) {
                    foreach (MapObject *object, remove)
                        undoStack->push(new RemoveMapObject(mMapDocument,
                                                            object));
                }
            }
        }
    }

    TmxMapWriter writer;
    if (!writer.write(clone, dialog.filePath())) {
        QMessageBox::critical(this, tr("Error Saving Map"),
                              writer.errorString());
        if (oldSelection != mMapDocument->tileSelection())
            undoStack->push(new ChangeTileSelection(mMapDocument, oldSelection));
        undoStack->endMacro();
        delete clone; // FIXME: release tilesets?
        return;
    }

    delete clone; // FIXME: release tilesets?

    if (ObjectGroup *og = dialog.objectGroup()) {
        QString lotName = dialog.filePath();
        MapObject *o = new MapObject(QLatin1String("lot"), lotName,
                                     bounds.topLeft() - mapOffset,
                                     clone->size());
        undoStack->push(new AddMapObject(mMapDocument, og, o));
    }

    if (oldSelection != mMapDocument->tileSelection())
        undoStack->push(new ChangeTileSelection(mMapDocument, oldSelection));
    undoStack->endMacro();

    if (dialog.openLot()) {
        QString fileName = dialog.filePath();
        DocumentManager *docmgr = DocumentManager::instance();
        int index = docmgr->findDocument(fileName);
        if (index >= 0) {
            docmgr->switchToDocument(index);
            closeFile();
        }
        openFile(fileName);
    }
}

void MainWindow::convertOrientation()
{
    ConvertOrientationDialog dialog(this);
    dialog.exec();
}

#include "roomdefecator.h"
#include <addremovelayer.h>

// Copied from BuildingFloor::roomRegion()
static QList<QRect> cleanupRegion(QRegion region)
{
    // Clean up the region by merging vertically-adjacent rectangles of the
    // same width.
    QVector<QRect> rects = region.rects();
    for (int i = 0; i < rects.size(); i++) {
        QRect r = rects[i];
        if (!r.isValid()) continue;
        for (int j = 0; j < rects.size(); j++) {
            if (i == j) continue;
            QRect r2 = rects.at(j);
            if (!r2.isValid()) continue;
            if (r2.left() == r.left() && r2.right() == r.right()) {
                if (r.bottom() + 1 == r2.top()) {
                    r.setBottom(r2.bottom());
                    rects[j] = QRect();
                } else if (r.top() == r2.bottom() + 1) {
                    r.setTop(r2.top());
                    rects[j] = QRect();
                }
            }
        }
        rects[i] = r;
    }

    QList<QRect> ret;
    foreach (QRect r, rects) {
        if (r.isValid())
            ret += r;
    }
    return ret;
}

void MainWindow::RoomDefGo()
{
    if (!mMapDocument)
        return;

    QRect bounds = mMapDocument->tileSelection().boundingRect();
    if (bounds.isEmpty())
        bounds = QRect(0, 0, mMapDocument->map()->width(), mMapDocument->map()->height());

    Map *map = mMapDocument->map();

    bool beginMacro = false;

    for (int level = 0; level <= mMapDocument->mapComposite()->maxLevel(); level++) {
        RoomDefecator rd(map, level, bounds);
        rd.defecate();
        if (rd.mRegions.isEmpty())
            continue;

        if (!beginMacro) {
            mMapDocument->undoStack()->beginMacro(tr("Auto RoomDefs"));
            beginMacro = true;
        }

        QString layerName = QString::fromLatin1("%1_RoomDefs").arg(level);
        int index = map->indexOfLayer(layerName, Layer::ObjectGroupType);
        ObjectGroup *og = 0;
        if (index < 0) {
            // Create the new layer in the same level as the current layer.
            // Stack it with other layers of the same type in level-order.
            index = map->layerCount();
            Layer *topLayerOfSameTypeInSameLevel = 0;
            Layer *bottomLayerOfSameTypeInGreaterLevel = 0;
            Layer *topLayerOfSameTypeInLesserLevel = 0;
            foreach (Layer *layer, map->layers(Layer::ObjectGroupType)) {
                if ((layer->level() > level) && !bottomLayerOfSameTypeInGreaterLevel)
                    bottomLayerOfSameTypeInGreaterLevel = layer;
                if (layer->level() < level)
                    topLayerOfSameTypeInLesserLevel = layer;
                if (layer->level() == level)
                    topLayerOfSameTypeInSameLevel = layer;
            }
            if (topLayerOfSameTypeInSameLevel)
                index = map->layers().indexOf(topLayerOfSameTypeInSameLevel) + 1;
            else if (bottomLayerOfSameTypeInGreaterLevel)
                index = map->layers().indexOf(bottomLayerOfSameTypeInGreaterLevel);
            else if (topLayerOfSameTypeInLesserLevel)
                index = map->layers().indexOf(topLayerOfSameTypeInLesserLevel) + 1;
            og = new ObjectGroup(layerName, 0, 0, map->width(), map->height());
            og->setColor(Qt::blue);
            mMapDocument->undoStack()->push(new AddLayer(mMapDocument, index, og));
        } else {
            og = map->layerAt(index)->asObjectGroup();
            mMapDocument->setLayerVisible(index, true);
        }

        int i = 1;
        foreach (QRegion rgn, rd.mRegions) {
            QList<QRect> rects = cleanupRegion(rgn);
            QString suffix;
            if (rects.size() > 1)
                suffix = QLatin1String("#");
            foreach (QRect r, rects) {
                MapObject *object = new MapObject(QString::fromLatin1("room%1%2").arg(i).arg(suffix),
                                                  QLatin1String("room"),
                                                  r.topLeft(), r.size());
                mMapDocument->undoStack()->push(new AddMapObject(mMapDocument,
                                                                 og, object));
            }
            ++i;
        }
    }

    if (beginMacro)
        mMapDocument->undoStack()->endMacro();
}

void MainWindow::RoomDefMerge()
{
    if (!mMapDocument ||
            !mMapDocument->currentLayer() ||
            mMapDocument->selectedObjects().isEmpty())
        return;

    int level = mMapDocument->currentLevel();
    QString layerName = QString::fromLatin1("%1_RoomDefs").arg(level);
    int index = mMapDocument->map()->indexOfLayer(layerName, Layer::ObjectGroupType);
    if (index < 0)
        return;
    ObjectGroup *og = mMapDocument->map()->layerAt(index)->asObjectGroup();

    QRegion merged;
    foreach (MapObject *object, mMapDocument->selectedObjects())
        merged += object->bounds().toRect();
    QList<QRect> rects = cleanupRegion(merged);

    mMapDocument->undoStack()->beginMacro(tr("Merge RoomDefs"));
    foreach (MapObject *object, mMapDocument->selectedObjects()) {
        mMapDocument->undoStack()->push(new RemoveMapObject(mMapDocument,
                                                            object));
    }

    QStringList taken;
    foreach (MapObject *o, og->objects())
        taken += o->name();
    QString suffix;
    if (rects.size() > 1)
        suffix = QLatin1String("#");
    QString name;
    int roomID = 1;
    while (true) {
        name = QString::fromLatin1("room%1%2").arg(roomID).arg(suffix);
        QString name2 = QString::fromLatin1("room%1%2").arg(roomID)
                .arg((rects.size() <= 1) ? QLatin1String("#") : QString());
        if (!taken.contains(name) && !taken.contains(name2))
            break;
        ++roomID;
    }

    QList<MapObject*> selected;
    foreach (QRect r, rects) {
        MapObject *object = new MapObject(name, QLatin1String("room"),
                                          r.topLeft(), r.size());
        mMapDocument->undoStack()->push(new AddMapObject(mMapDocument,
                                                         og, object));
        selected += object;
    }
    mMapDocument->setSelectedObjects(selected);

    mMapDocument->undoStack()->endMacro();
}

void MainWindow::RoomDefRemove()
{
    if (!mMapDocument)
        return;

    QRect bounds = mMapDocument->tileSelection().boundingRect();
    if (bounds.isEmpty())
        bounds = QRect(0, 0, mMapDocument->map()->width(), mMapDocument->map()->height());

    QList<MapObject*> remove;

    for (int level = 0; level <= mMapDocument->mapComposite()->maxLevel(); level++) {
        QString layerName = QString::fromLatin1("%1_RoomDefs").arg(level);
        int index = mMapDocument->map()->indexOfLayer(layerName, Layer::ObjectGroupType);
        if (index >= 0) {
            ObjectGroup *og = mMapDocument->map()->layerAt(index)->asObjectGroup();
            foreach (MapObject *o, og->objects()) {
                if (o->bounds().intersects(bounds))
                    remove += o;
            }
        }
    }

    if (!remove.size())
        return;

    mMapDocument->undoStack()->beginMacro(tr("Remove RoomDefs"));
    foreach (MapObject *o, remove)
        mMapDocument->undoStack()->push(
                    new RemoveMapObject(mMapDocument, o));
    mMapDocument->undoStack()->endMacro();
}

#include "tile.h"
void MainWindow::RoomDefUnknownWalls()
{
    if (!mMapDocument)
        return;

    QRect bounds = mMapDocument->tileSelection().boundingRect();
    QRect mapRect = QRect(0, 0, mMapDocument->map()->width(), mMapDocument->map()->height());
    if (bounds.isEmpty())
        bounds = mapRect;

    RoomDefecator rd(mMapDocument->map(), mMapDocument->currentLevel(), bounds);
    if (!rd.mLayerWalls)
        return;

    QSet<Tile*> tiles = rd.mWestWallTiles + rd.mNorthWallTiles;
    tiles += rd.mSouthEastWallTiles;

    QRegion unknown;
    bounds &= rd.mLayerWalls->bounds();
    for (int y = bounds.top(); y <= bounds.bottom(); y++) {
        for (int x = bounds.left(); x <= bounds.right(); x++) {
            Tile *tile = rd.mLayerWalls->cellAt(x, y).tile;
            if (tile && !tiles.contains(tile))
                unknown += QRect(x, y, 1, 1);
            else if (tile)
                ; // valid in Walls, don't check Walls2
            else if (rd.mLayerWalls2) {
                tile = rd.mLayerWalls2->cellAt(x, y).tile;
                if (tile && !tiles.contains(tile))
                    unknown += QRect(x, y, 1, 1);
            }
        }
    }

    if (!unknown.isEmpty()) {
        mMapDocument->undoStack()->push(
                    new ChangeTileSelection(mMapDocument, unknown));
    }
}

#include "luaconsole.h"
void MainWindow::LuaConsole()
{
    LuaConsole::instance()->show();
    LuaConsole::instance()->raise();
    LuaConsole::instance()->activateWindow();
}

namespace Tiled {
namespace Internal {

class ReorderLayer : public QUndoCommand
{
public:
    ReorderLayer(MapDocument *doc, int index, Layer *layer) :
        QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Layer")),
        mDocument(doc),
        mIndex(index),
        mLayer(layer)
    {}

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap()
    {
        Layer *current = mDocument->currentLayer();

        mIndex = reorderLayer(mIndex);

        mDocument->setCurrentLayerIndex(current
                                        ? mDocument->map()->layers().indexOf(current)
                                        : 0);
    }

    int reorderLayer(int index)
    {
        Q_UNUSED(index)
        int old = mDocument->map()->layers().indexOf(mLayer);
        LayerModel *layerModel = mDocument->layerModel();
        Layer *layer = layerModel->takeLayerAt(old);
        layerModel->insertLayer(mIndex, layer);
        return old;
    }

    MapDocument *mDocument;
    int mIndex;
    Layer *mLayer;
};

} // namespace Internal
} // namespace Tiled

#include "bmptooldialog.h"
#include "luatiled.h"
#include "painttilelayer.h"
bool MainWindow::LuaScript(MapDocument *doc, const QString &filePath)
{
    QString f = filePath;
    if (filePath.isEmpty()) {
        f = QDir(QApplication::applicationDirPath()).filePath(QLatin1String("lua/"));
        if (!LuaConsole::instance()->fileName().isEmpty())
            f = LuaConsole::instance()->fileName();
        f = QFileDialog::getOpenFileName(LuaConsole::instance(), tr("Open Lua Script"),
                                         f, tr("Lua files (*.lua)"));
    }
    if (f.isEmpty())
        return true;

    LuaConsole::instance()->setFile(f);

    Lua::LuaScript scripter(doc->map());
    scripter.mMap.mSelection = doc->tileSelection();
    QString output;
    bool ok = scripter.dofile(f, output);
    qDebug() << output;
    if (!ok) {
        return false;
    }

    QUndoStack *us = doc->undoStack();
    us->beginMacro(tr("Lua Script"));

    // Map resizing.

    // Handle deleted layers
    foreach (Lua::LuaLayer *ll, scripter.mMap.mRemovedLayers) {
        if (ll->mOrig) {
            int index = doc->map()->layers().indexOf(ll->mOrig);
            Q_ASSERT(index != -1);
            qDebug() << "remove layer" << ll->mOrig->name() << " at " << index;
            us->push(new RemoveLayer(doc, index));
        }
    }

    // Layers may have been added, moved, deleted, and/or edited.
    foreach (Lua::LuaLayer *ll, scripter.mMap.mLayers) {
        if (Layer *layer = ll->mOrig) {
            // This layer exists (somewhere) in the original map.
            int oldIndex = doc->map()->layers().indexOf(layer);
            int newIndex = scripter.mMap.mLayers.indexOf(ll);
            if (oldIndex != newIndex) {
                qDebug() << "move layer" << layer->name() << "from " << oldIndex << " to " << newIndex;
                us->push(new ReorderLayer(doc, newIndex, layer));
            }
        } else {
            // This is a new layer.
            Q_ASSERT(ll->mClone);
            Layer *newLayer = ll->mClone->clone();
            int index = scripter.mMap.mLayers.indexOf(ll);
            qDebug() << "add layer" << newLayer->name() << "at" << index;
            us->push(new AddLayer(doc, index, newLayer));
        }
    }

    // Clear the tile selection so it doesn't inhibit what the script changed.
    if (!doc->tileSelection().isEmpty())
        us->push(new ChangeTileSelection(doc, QRegion()));

    foreach (Lua::LuaLayer *ll, scripter.mMap.mLayers) {
        // Apply changes to tile layers.
        if (Lua::LuaTileLayer *tl = ll->asTileLayer()) {
            if (tl->mOrig == 0)
                continue; // Ignore new layers.
            if (!tl->mCloneTileLayer || tl->mAltered.isEmpty())
                continue; // No changes.
            TileLayer *source = tl->mCloneTileLayer->copy(tl->mAltered);
            QRect r = tl->mAltered.boundingRect();
            us->push(new PaintTileLayer(doc, tl->mOrig->asTileLayer(),
                                        r.x(), r.y(), source, tl->mAltered, true));
            delete source;
        }
        // Add/Remove/Delete objects
        if (Lua::LuaObjectGroup *og = ll->asObjectGroup()) {
            foreach (Lua::LuaMapObject *o, og->objects()) {
                if (og->mOrig) {

                } else {
                    us->push(new AddMapObject(doc,
                                              doc->map()->layerAt(scripter.mMap.mLayers.indexOf(ll))->asObjectGroup(),
                                              o->mClone->clone()));
                }
            }
        }
    }

    // Apply changes to rules
    if (scripter.mMap.mRulesChanged) {
        BmpToolDialog::changeBmpRules(doc,
                                      scripter.mMap.mClone->rbmpSettings()->rulesFile(),
                                      scripter.mMap.mClone->rbmpSettings()->aliasesCopy(),
                                      scripter.mMap.mClone->rbmpSettings()->rulesCopy());
    }

    // Apply changes to blends
    if (scripter.mMap.mBlendsChanged) {
        BmpToolDialog::changeBmpBlends(doc,
                                      scripter.mMap.mClone->rbmpSettings()->blendsFile(),
                                      scripter.mMap.mClone->rbmpSettings()->blendsCopy());
    }

    // Apply changes to BMP images
    Lua::LuaMapBmp &bmpMain = scripter.mMap.mBmpMain;
    if (!bmpMain.mAltered.isEmpty()) {
        QRect r = bmpMain.mAltered.boundingRect();
        us->push(new PaintBMP(doc, 0, r.x(), r.y(),
                              bmpMain.mBmp.image().copy(r),
                              bmpMain.mAltered));
    }
    Lua::LuaMapBmp &bmpVeg = scripter.mMap.mBmpVeg;
    if (!bmpVeg.mAltered.isEmpty()) {
        QRect r = bmpVeg.mAltered.boundingRect();
        us->push(new PaintBMP(doc, 1, r.x(), r.y(),
                              bmpVeg.mBmp.image().copy(r),
                              bmpVeg.mAltered));
    }

    // Handle the script changing the tile selection.
    if (doc->tileSelection() != scripter.mMap.mSelection)
        us->push(new ChangeTileSelection(doc, scripter.mMap.mSelection));

    us->endMacro();

    return true;
}

void MainWindow::LuaScript(const QString &filePath)
{
    LuaScript(mMapDocument, filePath);
}
#endif // ZOMBOID

void MainWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action)
        openFile(action->data().toString());
}

QStringList MainWindow::recentFiles() const
{
    QVariant v = mSettings.value(QLatin1String("recentFiles/fileNames"));
    return v.toStringList();
}

QString MainWindow::fileDialogStartLocation() const
{
    QStringList files = recentFiles();
    return (!files.isEmpty()) ? QFileInfo(files.first()).path() : QString();
}

void MainWindow::setRecentFile(const QString &fileName)
{
    // Remember the file by its canonical file path
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    if (canonicalFilePath.isEmpty())
        return;

    QStringList files = recentFiles();
    files.removeAll(canonicalFilePath);
    files.prepend(canonicalFilePath);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    mSettings.beginGroup(QLatin1String("recentFiles"));
    mSettings.setValue(QLatin1String("fileNames"), files);
    mSettings.endGroup();
    updateRecentFiles();
}

void MainWindow::clearRecentFiles()
{
    mSettings.beginGroup(QLatin1String("recentFiles"));
    mSettings.setValue(QLatin1String("fileNames"), QStringList());
    mSettings.endGroup();
    updateRecentFiles();
}

void MainWindow::updateRecentFiles()
{
    QStringList files = recentFiles();
    const int numRecentFiles = qMin(files.size(), (int) MaxRecentFiles);

    for (int i = 0; i < numRecentFiles; ++i)
    {
        mRecentFiles[i]->setText(QFileInfo(files[i]).fileName());
        mRecentFiles[i]->setData(files[i]);
        mRecentFiles[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
    {
        mRecentFiles[j]->setVisible(false);
    }
    mUi->menuRecentFiles->setEnabled(numRecentFiles > 0);
}

void MainWindow::updateActions()
{
    Map *map = 0;
    bool tileLayerSelected = false;
    bool objectsSelected = false;
    QRegion selection;

    if (mMapDocument) {
        Layer *currentLayer = mMapDocument->currentLayer();

        map = mMapDocument->map();
        tileLayerSelected = dynamic_cast<TileLayer*>(currentLayer) != 0;
        objectsSelected = !mMapDocument->selectedObjects().isEmpty();
        selection = mMapDocument->tileSelection();
    }

    const bool canCopy = (tileLayerSelected && !selection.isEmpty())
            || objectsSelected;
    mUi->actionSave->setEnabled(map);
    mUi->actionSaveAs->setEnabled(map);
    mUi->actionSaveAsImage->setEnabled(map);
    mUi->actionExport->setEnabled(map);
    mUi->actionClose->setEnabled(map);
    mUi->actionCloseAll->setEnabled(map);
    mUi->actionCut->setEnabled(canCopy);
    mUi->actionCopy->setEnabled(canCopy);
    mUi->actionPaste->setEnabled(mClipboardManager->hasMap());
#ifdef ZOMBOID
    mUi->actionDelete->setEnabled(canCopy || (mMapDocument && !mMapDocument->bmpSelection().isEmpty()));
#else
    mUi->actionDelete->setEnabled(canCopy);
#endif
    mUi->actionNewTileset->setEnabled(map);
    mUi->actionAddExternalTileset->setEnabled(map);
    mUi->actionResizeMap->setEnabled(map);
    mUi->actionOffsetMap->setEnabled(map);
    mUi->actionMapProperties->setEnabled(map);
    mUi->actionAutoMap->setEnabled(map);

    mCommandButton->setEnabled(map);

    updateZoomLabel(); // for the zoom actions

    Layer *layer = mMapDocument ? mMapDocument->currentLayer() : 0;
#ifdef ZOMBOID
    if (layer) {
        mCurrentLevelButton->setEnabled(true);
        mCurrentLayerButton->setEnabled(true);
        // The extra space at the end is deliberate so the toolbutton arrow
        // doesn't overlap the text.
        mCurrentLevelButton->setText(tr("Level: %1 ").arg(layer->level()));
        QString name = layer->name();
        if (name.isEmpty())
            name = tr("<no name>");
        else
            name = MapComposite::layerNameWithoutPrefix(name);
        mCurrentLayerButton->setText(tr("Layer: %1 ").arg(name));
    } else {
        mCurrentLevelButton->setText(tr("Level: <none> "));
        mCurrentLayerButton->setText(tr("Layer: <none> "));
        mCurrentLevelButton->setEnabled(false);
        mCurrentLayerButton->setEnabled(false);
    }
#else
    mCurrentLayerLabel->setText(tr("Current layer: %1").arg(
                                    layer ? layer->name() : tr("<none>")));
#endif

#ifdef ZOMBOID
    mUi->actionConvertToLot->setEnabled(false);
    if (mMapDocument) {
        const QRect bounds = mMapDocument->tileSelection().boundingRect();
        if (!bounds.isEmpty())
            mUi->actionConvertToLot->setEnabled(true);
    }
    mUi->actionRoomDefGo->setEnabled(mMapDocument);
    mUi->actionRoomDefMerge->setEnabled(mMapDocument &&
                                        !mMapDocument->selectedObjects().isEmpty());
    mUi->actionRoomDefRemove->setEnabled(mMapDocument);
    mUi->actionRoomDefUnknownWalls->setEnabled(mMapDocument);
#endif
}

void MainWindow::updateZoomLabel()
{
    MapView *mapView = mDocumentManager->currentMapView();

    Zoomable *zoomable = mapView ? mapView->zoomable() : 0;
    const qreal scale = zoomable ? zoomable->scale() : 1;

    mUi->actionZoomIn->setEnabled(zoomable && zoomable->canZoomIn());
    mUi->actionZoomOut->setEnabled(zoomable && zoomable->canZoomOut());
    mUi->actionZoomNormal->setEnabled(scale != 1);

    if (zoomable) {
        mZoomComboBox->setEnabled(true);
    } else {
        int index = mZoomComboBox->findData((qreal)1.0);
        mZoomComboBox->setCurrentIndex(index);
        mZoomComboBox->setEnabled(false);
    }
}

#ifdef ZOMBOID
void MainWindow::resizeStatusInfoLabel()
{
    int width = 999, height = 999;
    if (mMapDocument) {
        width = qMax(width, mMapDocument->map()->width());
        height = qMax(height, mMapDocument->map()->height());
    }
    QFontMetrics fm = mStatusInfoLabel->fontMetrics();
    QString coordString = QString(QLatin1String("%1,%2")).arg(width).arg(height);
    mStatusInfoLabel->setMinimumWidth(fm.width(coordString) + 8);
}

void MainWindow::aboutToShowLevelMenu()
{
    if (!mMapDocument) return;
    mCurrentLevelMenu->clear();
    QStringList items;
    items.prepend(QString::number(0));
    foreach (CompositeLayerGroup *layerGroup, mMapDocument->mapComposite()->sortedLayerGroups()) {
        if (!layerGroup->level()) continue;
        items.prepend(QString::number(layerGroup->level()));
    }
    foreach (QString item, items) {
        QAction *action = mCurrentLevelMenu->addAction(item);
        if (item.toInt() == mMapDocument->currentLevel()) {
            action->setCheckable(true);
            action->setChecked(true);
            action->setEnabled(false);
        }
    }
}

void MainWindow::aboutToShowLayerMenu()
{
    if (!mMapDocument) return;
    mCurrentLayerMenu->clear();
    int level = mMapDocument->currentLevel();
    QIcon tileIcon(QLatin1String(":/images/16x16/layer-tile.png"));

    QAction *before = 0;
    foreach (TileLayer *tl, mMapDocument->map()->tileLayers()) {
        if (tl->level() == level) {
            QAction *action = new QAction(tl->name(), mCurrentLayerMenu);
            action->setData(QVariant::fromValue(tl));
            mCurrentLayerMenu->insertAction(before, action);
            if (tl == mMapDocument->currentLayer()) {
                action->setCheckable(true);
                action->setChecked(true);
                action->setEnabled(false);
            } else
                action->setIcon(tileIcon);
            before = action;
        }
    }

    QIcon objectIcon(QLatin1String(":/images/16x16/layer-object.png"));
    foreach (ObjectGroup *og, mMapDocument->map()->objectGroups()) {
        if (og->level() == level) {
            QAction *action = new QAction(og->name(), mCurrentLayerMenu);
            action->setData(QVariant::fromValue(og));
            mCurrentLayerMenu->insertAction(before, action);
            if (og == mMapDocument->currentLayer()) {
                action->setCheckable(true);
                action->setChecked(true);
                action->setEnabled(false);
            }
            else
                action->setIcon(objectIcon);
            before = action;
        }
    }
}

void MainWindow::triggeredLevelMenu(QAction *action)
{
    if (!mMapDocument) return;
    int level = action->text().toInt();
    if (CompositeLayerGroup *layerGroup = mMapDocument->mapComposite()->tileLayersForLevel(level)) {
        if (Layer *layer = mMapDocument->currentLayer()) {
            // Try to switch to a layer with the same name in the new level
            QString name = MapComposite::layerNameWithoutPrefix(layer);
            if (layer->isTileLayer()) {
                foreach (TileLayer *tl, layerGroup->layers()) {
                    QString name2 = MapComposite::layerNameWithoutPrefix(tl);
                    if (name == name2) {
                        int index = mMapDocument->map()->layers().indexOf(tl);
                        mMapDocument->setCurrentLayerIndex(index);
                        return;
                    }
                }
            } else if (layer->isObjectGroup()) {
                foreach (ObjectGroup *og, mMapDocument->map()->objectGroups()) {
                    if (og->level() == level) {
                        QString name2 = MapComposite::layerNameWithoutPrefix(og);
                        if (name == name2) {
                            int index = mMapDocument->map()->layers().indexOf(og);
                            mMapDocument->setCurrentLayerIndex(index);
                            return;
                        }
                    }
                }
            }
        }
    }
    int index = 0;
    foreach (Layer *layer, mMapDocument->map()->layers()) {
        if (layer->level() == level) {
            mMapDocument->setCurrentLayerIndex(index);
            return;
        }
        ++index;
    }
}

void MainWindow::triggeredLayerMenu(QAction *action)
{
    if (!mMapDocument) return;
    ObjectGroup *og = action->data().value<ObjectGroup*>();
    TileLayer *tl = action->data().value<TileLayer*>();
    if (Layer *layer = og ? (Layer*)og : (Layer*)tl) {
        int index = mMapDocument->map()->layers().indexOf(layer);
        mMapDocument->setCurrentLayerIndex(index);
    }
}
#endif // ZOMBOID

void MainWindow::editLayerProperties()
{
    if (!mMapDocument)
        return;

    if (Layer *layer = mMapDocument->currentLayer())
        PropertiesDialog::showDialogFor(layer, mMapDocument, this);
}

void MainWindow::flipStampHorizontally()
{
    if (TileLayer *stamp = mStampBrush->stamp()) {
        stamp = static_cast<TileLayer*>(stamp->clone());
        stamp->flip(TileLayer::FlipHorizontally);
        setStampBrush(stamp);
    }
}

void MainWindow::flipStampVertically()
{
    if (TileLayer *stamp = mStampBrush->stamp()) {
        stamp = static_cast<TileLayer*>(stamp->clone());
        stamp->flip(TileLayer::FlipVertically);
        setStampBrush(stamp);
    }
}

void MainWindow::rotateStampLeft()
{
    if (TileLayer *stamp = mStampBrush->stamp()) {
        stamp = static_cast<TileLayer*>(stamp->clone());
        stamp->rotate(TileLayer::RotateLeft);
        setStampBrush(stamp);
    }
}

void MainWindow::rotateStampRight()
{
    if (TileLayer *stamp = mStampBrush->stamp()) {
        stamp = static_cast<TileLayer*>(stamp->clone());
        stamp->rotate(TileLayer::RotateRight);
        setStampBrush(stamp);
    }
}

/**
 * Sets the stamp brush, which is used by both the stamp brush and the bucket
 * fill tool.
 */
void MainWindow::setStampBrush(const TileLayer *tiles)
{
    if (!tiles)
        return;

    mStampBrush->setStamp(static_cast<TileLayer*>(tiles->clone()));
    mBucketFillTool->setStamp(static_cast<TileLayer*>(tiles->clone()));

    // When selecting a new stamp, it makes sense to switch to a stamp tool
    ToolManager *m = ToolManager::instance();
    AbstractTool *selectedTool = m->selectedTool();
    if (selectedTool != mStampBrush && selectedTool != mBucketFillTool)
        m->selectTool(mStampBrush);
}

void MainWindow::updateStatusInfoLabel(const QString &statusInfo)
{
    mStatusInfoLabel->setText(statusInfo);
}

void MainWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("mainwindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());

    QVariantList v;
    foreach (int size, mMainSplitter->sizes())
        v += size;
    mSettings.setValue(tr("%1/sizes").arg(mMainSplitter->objectName()), v);
    mSettings.setValue(QLatin1String("TileLayersPanel/scale"),
                       mTileLayersPanel->scale());

    mSettings.endGroup();

    mSettings.beginGroup(QLatin1String("recentFiles"));
    if (MapDocument *document = mDocumentManager->currentDocument())
        mSettings.setValue(QLatin1String("lastActive"), document->fileName());

    QStringList fileList;
    QStringList mapScales;
    QStringList scrollX;
    QStringList scrollY;
    QStringList selectedLayer;
    for (int i = 0; i < mDocumentManager->documentCount(); i++) {
        mDocumentManager->switchToDocument(i);
        fileList.append(mDocumentManager->currentDocument()->fileName());
        MapView *mapView = mDocumentManager->currentMapView();
        const int currentLayerIndex = mMapDocument->currentLayerIndex();

        mapScales.append(QString::number(mapView->zoomable()->scale()));
        scrollX.append(QString::number(
                       mapView->horizontalScrollBar()->sliderPosition()));
        scrollY.append(QString::number(
                       mapView->verticalScrollBar()->sliderPosition()));
        selectedLayer.append(QString::number(currentLayerIndex));
    }
    mSettings.setValue(QLatin1String("lastOpenFiles"), fileList);
    mSettings.setValue(QLatin1String("mapScale"), mapScales);
    mSettings.setValue(QLatin1String("scrollX"), scrollX);
    mSettings.setValue(QLatin1String("scrollY"), scrollY);
    mSettings.setValue(QLatin1String("selectedLayer"), selectedLayer);
    mSettings.endGroup();
}

void MainWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("mainwindow"));
    QByteArray geom = mSettings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(800, 600);
    restoreState(mSettings.value(QLatin1String("state"),
                                 QByteArray()).toByteArray());

    QVariant v = mSettings.value(tr("%1/sizes").arg(mMainSplitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        mMainSplitter->setSizes(sizes);
    }

    qreal scale = mSettings.value(QLatin1String("TileLayersPanel/scale"), 0.25).toReal();
    mTileLayersPanel->setScale(scale);
    mSettings.endGroup();
    updateRecentFiles();
}

void MainWindow::updateWindowTitle()
{
    if (mMapDocument) {
        setWindowTitle(tr("[*]%1 - Tiled").arg(mMapDocument->displayName()));
        setWindowFilePath(mMapDocument->fileName());
        setWindowModified(mMapDocument->isModified());
    } else {
        setWindowTitle(QApplication::applicationName());
        setWindowFilePath(QString());
        setWindowModified(false);
    }
}

void MainWindow::addMapDocument(MapDocument *mapDocument)
{
    mDocumentManager->addDocument(mapDocument);

    MapView *mapView = mDocumentManager->currentMapView();
    connect(mapView->zoomable(), SIGNAL(scaleChanged(qreal)),
            this, SLOT(updateZoomLabel()));

    Preferences *prefs = Preferences::instance();

    MapScene *mapScene = mapView->mapScene();
    mapScene->setGridVisible(prefs->showGrid());
    connect(prefs, SIGNAL(showGridChanged(bool)),
            mapScene, SLOT(setGridVisible(bool)));
}

void MainWindow::aboutTiled()
{
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void MainWindow::retranslateUi()
{
    updateWindowTitle();

    mRandomButton->setToolTip(tr("Random Mode"));
    mLayerMenu->setTitle(tr("&Layer"));
    mActionHandler->retranslateUi();
}

void MainWindow::mapDocumentChanged(MapDocument *mapDocument)
{
    if (mMapDocument)
        mMapDocument->disconnect(this);

    if (mZoomable)
        mZoomable->connectToComboBox(0);
    mZoomable = 0;

    mMapDocument = mapDocument;

    mActionHandler->setMapDocument(mMapDocument);
    mLayerDock->setMapDocument(mMapDocument);
    mObjectsDock->setMapDocument(mMapDocument);
#ifdef ZOMBOID
    mLevelsDock->setMapDocument(mMapDocument);
    mWorldEdDock->setMapDocument(mMapDocument);
    mTileLayersPanel->setDocument(mMapDocument);
#endif
    mTilesetDock->setMapDocument(mMapDocument);
    AutomappingManager::instance()->setMapDocument(mMapDocument);
    QuickStampManager::instance()->setMapDocument(mMapDocument);

    if (mMapDocument) {
        connect(mMapDocument, SIGNAL(fileNameChanged()),
                SLOT(updateWindowTitle()));
        connect(mapDocument, SIGNAL(currentLayerIndexChanged(int)),
                SLOT(updateActions()));
        connect(mapDocument, SIGNAL(tileSelectionChanged(QRegion,QRegion)),
                SLOT(updateActions()));
        connect(mapDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(updateActions()));
#ifdef ZOMBOID
        connect(mapDocument, SIGNAL(mapChanged()),
                SLOT(resizeStatusInfoLabel()));
        connect(mapDocument, SIGNAL(layerRenamed(int)),
                SLOT(updateActions()));
#endif

        if (MapView *mapView = mDocumentManager->currentMapView()) {
            mZoomable = mapView->zoomable();
            mZoomable->connectToComboBox(mZoomComboBox);
        }
    }
    updateWindowTitle();
    updateActions();
#ifdef ZOMBOID
    resizeStatusInfoLabel();
#endif
}

void MainWindow::setupQuickStamps()
{
    QuickStampManager *quickStampManager = QuickStampManager::instance();
    QList<int> keys = QuickStampManager::keys();

    QSignalMapper *selectMapper = new QSignalMapper(this);
    QSignalMapper *saveMapper = new QSignalMapper(this);

    for (int i = 0; i < keys.length(); i++) {
        // Set up shortcut for selecting this quick stamp
        QShortcut *selectStamp = new QShortcut(this);
        selectStamp->setKey(keys.value(i));
        connect(selectStamp, SIGNAL(activated()), selectMapper, SLOT(map()));
        selectMapper->setMapping(selectStamp, i);

        // Set up shortcut for saving this quick stamp
        QShortcut *saveStamp = new QShortcut(this);
        saveStamp->setKey(QKeySequence(Qt::CTRL + keys.value(i)));
        connect(saveStamp, SIGNAL(activated()), saveMapper, SLOT(map()));
        saveMapper->setMapping(saveStamp, i);
    }

    connect(selectMapper, SIGNAL(mapped(int)),
            quickStampManager, SLOT(selectQuickStamp(int)));
    connect(saveMapper, SIGNAL(mapped(int)),
            quickStampManager, SLOT(saveQuickStamp(int)));

    connect(quickStampManager, SIGNAL(setStampBrush(const TileLayer*)),
            this, SLOT(setStampBrush(const TileLayer*)));
}

void MainWindow::closeMapDocument(int index)
{
    mDocumentManager->switchToDocument(index);
    if (confirmSave())
        mDocumentManager->closeCurrentDocument();
}

#ifdef ZOMBOID
void MainWindow::helpContents()
{
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/TileZed/index.html"));
    QDesktopServices::openUrl(url);
}
#endif
