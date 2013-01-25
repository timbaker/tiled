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

#include "buildingeditorwindow.h"
#include "ui_buildingeditorwindow.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingfloorsdialog.h"
#include "buildinglayersdock.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreferencesdialog.h"
#include "buildingundoredo.h"
#include "buildingfurnituredock.h"
#include "buildingisoview.h"
#include "buildingorthoview.h"
#include "buildingtemplates.h"
#include "buildingtemplatesdialog.h"
#include "buildingtiles.h"
#include "buildingtilesetdock.h"
#include "buildingtilesdialog.h"
#include "buildingtiletools.h"
#include "buildingtmx.h"
#include "buildingtools.h"
#include "furnituregroups.h"
#include "furnitureview.h"
#include "horizontallinedelegate.h"
#include "listofstringsdialog.h"
#include "mixedtilesetview.h"
#include "newbuildingdialog.h"
#include "resizebuildingdialog.h"
#include "roomsdialog.h"
#include "simplefile.h"
#include "templatefrombuildingdialog.h"

#include "preferences.h"
#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"
#include "zprogress.h"

#include "tile.h"
#include "tileset.h"

#include <QBitmap>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QShowEvent>
#include <QToolButton>
#include <QUndoGroup>
#include <QUrl>
#include <QXmlStreamReader>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace BuildingEditor {

class TileModeToolBar : public QToolBar
{
public:
    TileModeToolBar(QWidget *parent = 0);

public:
    QLabel *mFloorLabel;
    QAction *mActionPencil;
    QAction *mActionSelectTiles;
};

}

TileModeToolBar::TileModeToolBar(QWidget *parent) :
    QToolBar(parent),
    mFloorLabel(new QLabel),
    mActionPencil(new QAction(this)),
    mActionSelectTiles(new QAction(this))
{
    setObjectName(QString::fromUtf8("TileToolBar"));
    setWindowTitle(tr("Tile ToolBar"));

    mActionPencil->setCheckable(true);
    mActionSelectTiles->setCheckable(true);

    addAction(mActionPencil);
    addAction(mActionSelectTiles);
    addSeparator();
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    addWidget(mFloorLabel);
}

/////

BuildingEditorWindow* BuildingEditorWindow::mInstance = 0;

BuildingEditorWindow::BuildingEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingEditorWindow),
    mCurrentDocument(0),
    mOrthoScene(new BuildingOrthoScene(this)),
    mRoomComboBox(new QComboBox()),
    mUndoGroup(new QUndoGroup(this)),
    mCategoryZoomable(new Zoomable(this)),
    mCategory(0),
    mFurnitureGroup(0),
    mSynching(false),
    mInitialCategoryViewSelectionEvent(false),
    mAutoSaveTimer(new QTimer(this)),
    mUsedContextMenu(new QMenu(this)),
    mActionClearUsed(new QAction(this)),
    mOrient(OrientOrtho),
    mEditMode(ObjectMode),
    mTileModeToolBar(new TileModeToolBar(this)),
    mIsoScene(new BuildingIsoScene(this)),
    mFurnitureDock(new BuildingFurnitureDock(this)),
    mLayersDock(new BuildingLayersDock(this)),
    mTilesetDock(new BuildingTilesetDock(this)),
    mFirstTimeSeen(true)
{
    ui->setupUi(this);

    mInstance = this;

    BuildingPreferences *prefs = BuildingPreferences::instance();

    ui->toolBar->setWindowTitle(tr("Main ToolBar"));
    ui->toolBar->insertSeparator(ui->actionRooms);
    ui->toolBar->insertWidget(ui->actionRooms, mRoomComboBox);
    ui->toolBar->insertSeparator(ui->actionUpLevel);

    mRoomComboBox->setIconSize(QSize(20, 20));
    mRoomComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    mFloorLabel = new QLabel;
    mFloorLabel->setText(tr("Ground Floor"));
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->toolBar->insertWidget(ui->actionUpLevel, mFloorLabel);

    mOrthoView = ui->floorView;
    mOrthoView->setScene(mOrthoScene);
    ui->coordLabel->clear();
    connect(mOrthoView, SIGNAL(mouseCoordinateChanged(QPoint)),
            SLOT(mouseCoordinateChanged(QPoint)));
    connect(mOrthoView->zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));
    mOrthoView->zoomable()->connectToComboBox(ui->editorScaleComboBox);

    mIsoView = ui->tileView;
    mIsoView->setScene(mIsoScene);
    connect(ui->actionToggleOrthoIso, SIGNAL(triggered()), SLOT(toggleOrthoIso()));

    PencilTool::instance()->setEditor(mOrthoScene);
    PencilTool::instance()->setAction(ui->actionPecil);

    SelectMoveRoomsTool::instance()->setEditor(mOrthoScene);
    SelectMoveRoomsTool::instance()->setAction(ui->actionSelectRooms);

    DoorTool::instance()->setEditor(mOrthoScene);
    DoorTool::instance()->setAction(ui->actionDoor);

    WallTool::instance()->setEditor(mOrthoScene);
    WallTool::instance()->setAction(ui->actionWall);

    WindowTool::instance()->setEditor(mOrthoScene);
    WindowTool::instance()->setAction(ui->actionWindow);

    StairsTool::instance()->setEditor(mOrthoScene);
    StairsTool::instance()->setAction(ui->actionStairs);

    FurnitureTool::instance()->setEditor(mOrthoScene);
    FurnitureTool::instance()->setAction(ui->actionFurniture);

    /////
    RoofTool::instance()->setEditor(mOrthoScene);
    RoofTool::instance()->setAction(ui->actionRoof);

    QMenu *roofMenu = new QMenu(this);
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeW.png")),
                        tr("Slope (W)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeN.png")),
                        tr("Slope (N)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeE.png")),
                        tr("Slope (E)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeS.png")),
                        tr("Slope (S)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_peakWE.png")),
                        tr("Peak (Horizontal)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_peakNS.png")),
                        tr("Peak (Vertical)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_flat.png")),
                        tr("Flat Top"));
    connect(roofMenu, SIGNAL(triggered(QAction*)), SLOT(roofTypeChanged(QAction*)));

    QToolButton *button = static_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionRoof));
    button->setMenu(roofMenu);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    /////

    RoofCornerTool::instance()->setEditor(mOrthoScene);
    RoofCornerTool::instance()->setAction(ui->actionRoofCorner);

    roofMenu = new QMenu(this);
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerNW.png")),
                        tr("Inner (NW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerNE.png")),
                        tr("Inner (NE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerSE.png")),
                        tr("Inner (SE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerSW.png")),
                        tr("Inner (SW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerNW.png")),
                        tr("Outer (NW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerNE.png")),
                        tr("Outer (NE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerSE.png")),
                        tr("Outer (SE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerSW.png")),
                        tr("Outer (SW)"));
    connect(roofMenu, SIGNAL(triggered(QAction*)), SLOT(roofCornerTypeChanged(QAction*)));

    button = static_cast<QToolButton*>(ui->toolBar->widgetForAction(ui->actionRoofCorner));
    button->setMenu(roofMenu);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    /////

    SelectMoveObjectTool::instance()->setEditor(mOrthoScene);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));
    connect(ToolManager::instance(), SIGNAL(statusTextChanged(BaseTool*)),
            SLOT(updateToolStatusText()));

    connect(mRoomComboBox, SIGNAL(currentIndexChanged(int)),
            SLOT(roomIndexChanged(int)));

    connect(ui->actionUpLevel, SIGNAL(triggered()),
            SLOT(upLevel()));
    connect(ui->actionDownLevel, SIGNAL(triggered()),
            SLOT(downLevel()));

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    undoAction->setShortcuts(QKeySequence::Undo);
    redoAction->setShortcuts(QKeySequence::Redo);
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);
    Utils::setThemeIcon(undoAction, "edit-undo");
    Utils::setThemeIcon(redoAction, "edit-redo");
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(0), undoAction);
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(1), redoAction);
    ui->menuEdit->insertSeparator(ui->menuEdit->actions().at(2));

    ui->actionCut->setShortcuts(QKeySequence::Cut);
    ui->actionCopy->setShortcuts(QKeySequence::Copy);
    ui->actionPaste->setShortcuts(QKeySequence::Paste);
    ui->actionDelete->setShortcuts(QKeySequence::Delete);
    ui->actionSelectAll->setShortcuts(QKeySequence::SelectAll);
    ui->actionSelectNone->setShortcut(tr("Ctrl+Shift+A"));
    connect(ui->actionCut, SIGNAL(triggered()), SLOT(editCut()));
    connect(ui->actionCopy, SIGNAL(triggered()), SLOT(editCopy()));
    connect(ui->actionPaste, SIGNAL(triggered()), SLOT(editPaste()));
    connect(ui->actionDelete, SIGNAL(triggered()), SLOT(editDelete()));
    connect(ui->actionSelectAll, SIGNAL(triggered()), SLOT(selectAll()));
    connect(ui->actionSelectNone, SIGNAL(triggered()), SLOT(selectNone()));

    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(updateWindowTitle()));
    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(autoSaveCheck()));
    connect(mUndoGroup, SIGNAL(indexChanged(int)), SLOT(autoSaveCheck()));

    connect(ui->actionPreferences, SIGNAL(triggered()), SLOT(preferences()));

    connect(ui->actionNewBuilding, SIGNAL(triggered()), SLOT(newBuilding()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(openBuilding()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(saveBuilding()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(saveBuildingAs()));
    connect(ui->actionExportTMX, SIGNAL(triggered()), SLOT(exportTMX()));

    ui->actionNewBuilding->setShortcuts(QKeySequence::New);
    ui->actionOpen->setShortcuts(QKeySequence::Open);
    ui->actionSave->setShortcuts(QKeySequence::Save);
    ui->actionSaveAs->setShortcuts(QKeySequence::SaveAs);

    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));
    setWindowFlags(windowFlags() & ~Qt::WA_DeleteOnClose);

    ui->actionShowGrid->setChecked(prefs->showGrid());
    connect(ui->actionShowGrid, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowGrid(bool)));
    connect(prefs, SIGNAL(showGridChanged(bool)),
            ui->actionShowGrid, SLOT(setChecked(bool)));

    ui->actionHighlightFloor->setChecked(prefs->highlightFloor());
    connect(ui->actionHighlightFloor, SIGNAL(toggled(bool)),
            prefs, SLOT(setHighlightFloor(bool)));
    connect(prefs, SIGNAL(highlightFloorChanged(bool)),
            ui->actionHighlightFloor, SLOT(setChecked(bool)));

    ui->actionHighlightRoom->setChecked(prefs->highlightRoom());
    connect(ui->actionHighlightRoom, SIGNAL(toggled(bool)),
            prefs, SLOT(setHighlightRoom(bool)));
    connect(prefs, SIGNAL(highlightRoomChanged(bool)),
            ui->actionHighlightRoom, SLOT(setChecked(bool)));

    ui->actionShowObjects->setChecked(prefs->showObjects());
    connect(ui->actionShowObjects, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowObjects(bool)));
    connect(prefs, SIGNAL(showObjectsChanged(bool)),
            SLOT(showObjectsChanged(bool)));

    connect(ui->actionZoomIn, SIGNAL(triggered()), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()), SLOT(zoomOut()));
    connect(ui->actionNormalSize, SIGNAL(triggered()), SLOT(resetZoom()));

    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
//    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    ui->actionZoomIn->setShortcuts(keys);
    mOrthoView->addAction(ui->actionZoomIn);
    mIsoView->addAction(ui->actionZoomIn);
    ui->actionZoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);
    mOrthoView->addAction(ui->actionZoomOut);
    mIsoView->addAction(ui->actionZoomOut);
    ui->actionZoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys.clear();
    keys += QKeySequence(tr("Ctrl+0"));
    keys += QKeySequence(tr("0"));
    ui->actionNormalSize->setShortcuts(keys);
    mOrthoView->addAction(ui->actionNormalSize);
    mIsoView->addAction(ui->actionNormalSize);
    ui->actionNormalSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    /////
    mTileModeToolBar->mActionPencil->setIcon(
                QIcon(QLatin1String(":images/22x22/stock-tool-clone.png")));
    mTileModeToolBar->mActionSelectTiles->setIcon(ui->actionSelectRooms->icon());
    mTileModeToolBar->addAction(ui->actionUpLevel);
    mTileModeToolBar->addAction(ui->actionDownLevel);
    addToolBar(mTileModeToolBar);
    // These three actions are only active when the view widget (or a child)
    // has the focus.
    mIsoView->addAction(ui->actionZoomIn);
    mIsoView->addAction(ui->actionZoomOut);
    mIsoView->addAction(ui->actionNormalSize);
    connect(mIsoView->zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));
    connect(ui->actionSwitchEditMode, SIGNAL(triggered()), SLOT(toggleEditMode()));
    connect(mIsoView, SIGNAL(mouseCoordinateChanged(QPoint)),
            SLOT(mouseCoordinateChanged(QPoint)));
    DrawTileTool::instance()->setAction(mTileModeToolBar->mActionPencil);
    DrawTileTool::instance()->setEditor(mIsoScene);
    SelectTileTool::instance()->setAction(mTileModeToolBar->mActionSelectTiles);
    SelectTileTool::instance()->setEditor(mIsoScene);
    /////

    addDockWidget(Qt::RightDockWidgetArea, mLayersDock);
    addDockWidget(Qt::RightDockWidgetArea, mTilesetDock);
    tabifyDockWidget(mTilesetDock, mFurnitureDock);
    mLayersDock->hide();
    mTilesetDock->hide();
    mFurnitureDock->hide();

    connect(ui->actionResize, SIGNAL(triggered()), SLOT(resizeBuilding()));
    connect(ui->actionFlipHorizontal, SIGNAL(triggered()), SLOT(flipHorizontal()));
    connect(ui->actionFlipVertical, SIGNAL(triggered()), SLOT(flipVertical()));
    connect(ui->actionRotateRight, SIGNAL(triggered()), SLOT(rotateRight()));
    connect(ui->actionRotateLeft, SIGNAL(triggered()), SLOT(rotateLeft()));

    connect(ui->actionInsertFloorAbove, SIGNAL(triggered()), SLOT(insertFloorAbove()));
    connect(ui->actionInsertFloorBelow, SIGNAL(triggered()), SLOT(insertFloorBelow()));
    connect(ui->actionRemoveFloor, SIGNAL(triggered()), SLOT(removeFloor()));
    connect(ui->actionFloors, SIGNAL(triggered()), SLOT(floorsDialog()));

    connect(ui->actionRooms, SIGNAL(triggered()), SLOT(roomsDialog()));
    connect(ui->actionTemplates, SIGNAL(triggered()), SLOT(templatesDialog()));
    connect(ui->actionTiles, SIGNAL(triggered()), SLOT(tilesDialog()));
    connect(ui->actionTemplateFromBuilding, SIGNAL(triggered()),
            SLOT(templateFromBuilding()));

    connect(ui->actionHelp, SIGNAL(triggered()), SLOT(help()));
    connect(ui->actionAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

    mCategoryZoomable->connectToComboBox(ui->scaleComboBox);
    connect(mCategoryZoomable, SIGNAL(scaleChanged(qreal)),
            prefs, SLOT(setTileScale(qreal)));
    connect(prefs, SIGNAL(tileScaleChanged(qreal)),
            SLOT(categoryScaleChanged(qreal)));

    ui->categorySplitter->setSizes(QList<int>() << 128 << 256);

    connect(ui->categoryList, SIGNAL(itemSelectionChanged()),
            SLOT(categorySelectionChanged()));
    connect(ui->categoryList, SIGNAL(activated(QModelIndex)),
            SLOT(categoryActivated(QModelIndex)));

    ui->tilesetView->setZoomable(mCategoryZoomable);
    connect(ui->tilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(ui->tilesetView, SIGNAL(mousePressed()), SLOT(categoryViewMousePressed()));

    ui->furnitureView->setZoomable(mCategoryZoomable);
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(furnitureSelectionChanged()));

    QIcon clearIcon(QLatin1String(":/images/16x16/edit-clear.png"));
    mActionClearUsed->setIcon(clearIcon);
    mActionClearUsed->setText(tr("Clean-up"));
    connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedTiles()));
    mUsedContextMenu->addAction(mActionClearUsed);

    ui->statusLabel->clear();

    mAutoSaveTimer->setSingleShot(true);
    mAutoSaveTimer->setInterval(2.5 * 60 * 1000);
    connect(mAutoSaveTimer, SIGNAL(timeout()), SLOT(autoSaveTimeout()));

    readSettings();

    // Restore visibility due to mode-switching.
    // readSettings() calls QWidget::restoreGeometry() which might change
    // widget visibility.
    ui->toolBar->show();
    mOrthoView->show();
    ui->dockWidget->show();
    mTileModeToolBar->hide();
    mFurnitureDock->hide();
    mLayersDock->hide();
    mTilesetDock->hide();

    updateActions();
}

BuildingEditorWindow::~BuildingEditorWindow()
{
    BuildingTemplates::deleteInstance();
    BuildingTilesDialog::deleteInstance();
    BuildingTilesMgr::deleteInstance(); // Ensure all the tilesets are released
    BuildingTMX::deleteInstance();
    BuildingPreferences::deleteInstance();
    ToolManager::deleteInstance();
    delete ui;
}

void BuildingEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmAllSave()) {
        clearDocument();
        writeSettings();
        event->accept(); // doesn't destroy us
    } else
        event->ignore();

}

bool BuildingEditorWindow::openFile(const QString &fileName)
{
    if (!confirmSave())
        return false;

    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        addDocument(doc);
        return true;
    }

    QMessageBox::warning(this, tr("Error reading building"), error);
    return false;
}

bool BuildingEditorWindow::confirmAllSave()
{
    return confirmSave();
}

// Called by Tiled::Internal::MainWindow::closeEvent
bool BuildingEditorWindow::closeYerself()
{
    if (confirmAllSave()) {
        clearDocument();
        writeSettings();
        delete this;
        return true;
    }
    return false;
}

bool BuildingEditorWindow::Startup()
{
#if 1
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        if (ts->isMissing()) {
            PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
            TileMetaInfoMgr::instance()->loadTilesets();
            break;
        }
    }
#else
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // Create ~/.TileZed if needed.
    QString configPath = BuildingPreferences::instance()->configPath();
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

#if 1
    // Read Tilesets.txt before LoadTMXConfig() in case we are upgrading
    // TMXConfig.txt from VERSION0 to VERSION1.
    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->hasReadTxt()) {
        if (!mgr->readTxt()) {
            QMessageBox::critical(this, tr("It's no good, Jim!"),
                                  tr("%1\n(while reading %2)")
                                  .arg(mgr->errorString())
                                  .arg(mgr->txtName()));
            TileMetaInfoMgr::deleteInstance();
            return false;
        }
        PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
        mgr->loadTilesets();
    }
#endif

    if (!LoadTMXConfig()) {
        if (!mError.isEmpty()) // Empty when user cancelled choosing Tiles directory.
            QMessageBox::critical(this, tr("It's no good, Jim!"),
                                  tr("Error while reading %1\n%2")
                                  .arg(BuildingTMX::instance()->txtName())
                                  .arg(mError));
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
#endif

    /////
#if 0
    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        if (!validateTile(btemplate->Wall, "Wall") ||
                !validateTile(btemplate->DoorTile, "Door") ||
                !validateTile(btemplate->DoorFrameTile, "DoorFrame") ||
                !validateTile(btemplate->WindowTile, "Window") ||
                !validateTile(btemplate->CurtainsTile, "Curtains") ||
                !validateTile(btemplate->StairsTile, "Stairs")) {
            mError += tr("\nIn template %1").arg(btemplate->Name);
            QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
            return false;
        }

        foreach (Room *room, btemplate->RoomList) {
            if (!validateTile(room->Floor, "Floor") ||
                    !validateTile(room->Wall, "Wall")) {
                mError += tr("\nIn template %1, room %2")
                        .arg(btemplate->Name)
                        .arg(room->Name);
                QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
                return false;
            }
        }
    }
#endif

    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    /////

    // Add tile categories to the gui
    setCategoryList();

    /////

    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QString categoryName = mSettings.value(QLatin1String("SelectedCategory")).toString();
    if (!categoryName.isEmpty()) {
        int index = BuildingTilesMgr::instance()->indexOf(categoryName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstCategory + index);
    }
    QString fGroupName = mSettings.value(QLatin1String("SelectedFurnitureGroup")).toString();
    if (!fGroupName.isEmpty()) {
        int index = FurnitureGroups::instance()->indexOf(fGroupName);
        if (index >= 0)
            ui->categoryList->setCurrentRow(mRowOfFirstFurnitureGroup + index);
    }
    mSettings.endGroup();

    // This will create the Tiles dialog.  It must come after reading all the
    // config files above.
    connect(BuildingTilesDialog::instance(), SIGNAL(edited()),
            SLOT(tilesDialogEdited()));

    return true;
}

bool BuildingEditorWindow::LoadTMXConfig()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = Preferences::instance()->tilesDirectory();
    QDir dir(tilesDirectory);
    if (tilesDirectory.isEmpty() || !dir.exists()) {
        QMessageBox::information(this, tr("Choose Tiles Directory"),
                                 tr("Please choose the Tiles directory in the following dialog."));
        TileMetaInfoDialog dialog(this);
        dialog.exec();
#if 0
        tilesDirectory = Preferences::instance()->tilesDirectory();
        if (tilesDirectory.isEmpty() || !QDir(tilesDirectory).exists()) {
            mError.clear();
            return false;
        }
#endif
    }

    bool ok = BuildingTMX::instance()->readTxt();
    mError = BuildingTMX::instance()->errorString();
    return ok;
}

bool BuildingEditorWindow::validateTile(BuildingTile *btile, const char *key)
{
    if (btile->isNone())
        return true;
    if (!BuildingTilesMgr::instance()->tileFor(btile)) {
        mError = tr("%1 tile '%2' doesn't exist.")
                .arg(QLatin1String(key))
                .arg(btile->name());
        return false;
    }
    return true;
}

void BuildingEditorWindow::setCurrentRoom(Room *room) const
{
    int roomIndex = mCurrentDocument->building()->indexOf(room);
    this->mRoomComboBox->setCurrentIndex(roomIndex);
}

Room *BuildingEditorWindow::currentRoom() const
{
    Building *building = currentBuilding();
    if (!building || !building->roomCount())
        return 0;
    int roomIndex = mRoomComboBox->currentIndex();
    if (roomIndex < 0 || roomIndex >= building->roomCount())
        return 0;
    return building->room(roomIndex);
}

Building *BuildingEditorWindow::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
}

BuildingFloor *BuildingEditorWindow::currentFloor() const
{
    return mCurrentDocument ? mCurrentDocument->currentFloor() : 0;
}

QString BuildingEditorWindow::currentLayer() const
{
    return mCurrentDocument ? mCurrentDocument->currentLayer() : QString();
}

void BuildingEditorWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QByteArray geom = mSettings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(800, 600);
    restoreState(mSettings.value(QLatin1String("state"),
                                 QByteArray()).toByteArray());
    mOrthoView->zoomable()->setScale(mSettings.value(QLatin1String("EditorScale"),
                                                1.0).toReal());
    mIsoView->zoomable()->setScale(mSettings.value(QLatin1String("IsoEditorScale"),
                                                1.0).toReal());
    QString orient = mSettings.value(QLatin1String("Orientation"),
                                     QLatin1String("isometric")).toString();
    if (orient == QLatin1String("isometric"))
        toggleOrthoIso();
    mSettings.endGroup();

    mCategoryZoomable->setScale(BuildingPreferences::instance()->tileScale());

    restoreSplitterSizes(ui->categorySplitter);
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.setValue(QLatin1String("EditorScale"), mOrthoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("IsoEditorScale"), mIsoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("Orientation"),
                       (mOrient == OrientOrtho) ? QLatin1String("orthogonal")
                                                : QLatin1String("isometric"));
    mSettings.setValue(QLatin1String("SelectedCategory"),
                       mCategory ? mCategory->name() : QString());
    mSettings.setValue(QLatin1String("SelectedFurnitureGroup"),
                       mFurnitureGroup ? mFurnitureGroup->mLabel : QString());
    mSettings.endGroup();

    saveSplitterSizes(ui->categorySplitter);
}

void BuildingEditorWindow::saveSplitterSizes(QSplitter *splitter)
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    mSettings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    mSettings.endGroup();
}

void BuildingEditorWindow::restoreSplitterSizes(QSplitter *splitter)
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QVariant v = mSettings.value(tr("%1/sizes").arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
    mSettings.endGroup();
}

void BuildingEditorWindow::updateRoomComboBox()
{
    Room *currentRoom = this->currentRoom();
    mRoomComboBox->clear();
    if (!mCurrentDocument)
        return;

    int index = 0;
    foreach (Room *room, currentBuilding()->rooms()) {
        QImage image(20, 20, QImage::Format_ARGB32);
        image.fill(qRgba(0,0,0,0));
        QPainter painter(&image);
        painter.fillRect(1, 1, 18, 18, room->Color);
        mRoomComboBox->addItem(room->Name);
        mRoomComboBox->setItemIcon(index, QPixmap::fromImage(image));
        index++;
    }

    if (currentBuilding()->rooms().contains(currentRoom))
        setCurrentRoom(currentRoom);
}

void BuildingEditorWindow::roomIndexChanged(int index)
{
    Q_UNUSED(index)
    selectCurrentCategoryTile();
}

void BuildingEditorWindow::categoryScaleChanged(qreal scale)
{
    mCategoryZoomable->setScale(scale);
}

void BuildingEditorWindow::categoryViewMousePressed()
{
    mInitialCategoryViewSelectionEvent = false;
}

void BuildingEditorWindow::categoryActivated(const QModelIndex &index)
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();

    int row = index.row();
    if (row >= 0 && row < 2)
        ;
    else if (BuildingTileCategory *category = categoryAt(row)) {
        dialog->selectCategory(category);
    } else if (FurnitureGroup *group = furnitureGroupAt(row)) {
        dialog->selectCategory(group);
    }
    tilesDialog();
}

static QString paddedNumber(int number)
{
    return QString(QLatin1String("%1")).arg(number, 3, 10, QLatin1Char('0'));
}

void BuildingEditorWindow::categorySelectionChanged()
{
    mCategory = 0;
    mFurnitureGroup = 0;

    ui->tilesetView->model()->setTiles(QList<Tile*>());
    ui->furnitureView->model()->setTiles(QList<FurnitureTiles*>());

    ui->tilesetView->setContextMenu(0);
    ui->furnitureView->setContextMenu(0);
    mActionClearUsed->disconnect(this);

    QList<QListWidgetItem*> selected = ui->categoryList->selectedItems();
    if (selected.count() == 1) {
        int row = ui->categoryList->row(selected.first());
        if (row == 0) { // Used Tiles
            if (!mCurrentDocument) return;

            // Sort by category + index
            QList<BuildingTileCategory*> categories;
            QMap<QString,BuildingTileEntry*> entryMap;
            foreach (BuildingTileEntry *entry, currentBuilding()->usedTiles()) {
                BuildingTileCategory *category = entry->category();
                int categoryIndex = BuildingTilesMgr::instance()->indexOf(category);
                int index = category->indexOf(entry) + 1;
                QString key = paddedNumber(categoryIndex) + QLatin1String("_") + paddedNumber(index);
                entryMap[key] = entry;
                if (!categories.contains(category) && category->canAssignNone())
                    categories += entry->category();
            }

            // Add "none" tile first in each category where it is allowed.
            foreach (BuildingTileCategory *category, categories) {
                int categoryIndex = BuildingTilesMgr::instance()->indexOf(category);
                QString key = paddedNumber(categoryIndex) + QLatin1String("_") + paddedNumber(0);
                entryMap[key] = category->noneTileEntry();
            }

            QList<Tiled::Tile*> tiles;
            QList<void*> userData;
            QStringList headers;
            foreach (BuildingTileEntry *entry, entryMap.values()) {
                if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                    tiles += tile;
                    userData += entry;
                    headers += entry->category()->label();
                }
            }
            ui->tilesetView->model()->setTiles(tiles, userData, headers);
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedTiles()));
            ui->tilesetView->setContextMenu(mUsedContextMenu);
        } else if (row == 1) { // Used Furniture
            if (!mCurrentDocument) return;
            QMap<QString,FurnitureTiles*> furnitureMap;
            int index = 0;
            foreach (FurnitureTiles *ftiles, currentBuilding()->usedFurniture()) {
                // Sort by category name + index
                QString key = tr("<No Group>") + QString::number(index++);
                if (FurnitureGroup *g = ftiles->group()) {
                    key = g->mLabel + QString::number(g->mTiles.indexOf(ftiles));
                }
                if (!furnitureMap.contains(key))
                    furnitureMap[key] = ftiles;
            }
            ui->furnitureView->model()->setTiles(furnitureMap.values());
            ui->furnitureView->scrollToTop();
            ui->categoryStack->setCurrentIndex(1);

            connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedFurniture()));
            ui->furnitureView->setContextMenu(mUsedContextMenu);
        } else if (mCategory = categoryAt(row)) {
            QList<Tiled::Tile*> tiles;
            QList<void*> userData;
            QStringList headers;
            if (mCategory->canAssignNone()) {
                tiles += BuildingTilesMgr::instance()->noneTiledTile();
                userData += BuildingTilesMgr::instance()->noneTileEntry();
                headers += BuildingTilesMgr::instance()->noneTiledTile()->tileset()->name();
            }
            QMap<QString,BuildingTileEntry*> entryMap;
            int i = 0;
            foreach (BuildingTileEntry *entry, mCategory->entries()) {
                QString key = entry->displayTile()->name() + QString::number(i++);
                entryMap[key] = entry;
            }
            foreach (BuildingTileEntry *entry, entryMap.values()) {
                if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                    tiles += tile;
                    userData += entry;
                    if (tile == TilesetManager::instance()->missingTile())
                        headers += entry->displayTile()->mTilesetName;
                    else
                        headers += tile->tileset()->name();
                }
            }
            ui->tilesetView->model()->setTiles(tiles, userData, headers);
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            selectCurrentCategoryTile();
        } else if (mFurnitureGroup = furnitureGroupAt(row)) {
            ui->furnitureView->model()->setTiles(mFurnitureGroup->mTiles);
            ui->furnitureView->scrollToTop();
            ui->categoryStack->setCurrentIndex(1);
        }
    }
}

void BuildingEditorWindow::tileSelectionChanged()
{
    if (!mCurrentDocument || mSynching)
        return;

    QModelIndexList indexes = ui->tilesetView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        if (ui->tilesetView->model()->tileAt(index)) {
            BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(
                        ui->tilesetView->model()->userDataAt(index));
            bool mergeable = ui->tilesetView->mouseDown() &&
                    mInitialCategoryViewSelectionEvent;
            qDebug() << "mergeable=" << mergeable;
            mInitialCategoryViewSelectionEvent = true;
            BuildingTileCategory *category = mCategory ? mCategory : entry->category();
            if (category->isNone())
                ; // never happens
            else if (category->asExteriorWalls())
                currentEWallChanged(entry, mergeable);
            else if (category->asInteriorWalls())
                currentIWallChanged(entry, mergeable);
            else if (category->asFloors())
                currentFloorChanged(entry, mergeable);
            else if (category->asDoors())
                currentDoorChanged(entry, mergeable);
            else if (category->asDoorFrames())
                currentDoorFrameChanged(entry, mergeable);
            else if (category->asWindows())
                currentWindowChanged(entry, mergeable);
            else if (category->asCurtains())
                currentCurtainsChanged(entry, mergeable);
            else if (category->asStairs())
                currentStairsChanged(entry, mergeable);
            else if (category->asGrimeFloor())
                currentRoomTileChanged(Room::GrimeFloor, entry, mergeable);
            else if (category->asGrimeWall())
                currentRoomTileChanged(Room::GrimeWall, entry, mergeable);
            else if (category->asRoofCaps())
                currentRoofTileChanged(entry, RoofObject::TileCap, mergeable);
            else if (category->asRoofSlopes())
                currentRoofTileChanged(entry, RoofObject::TileSlope, mergeable);
            else if (category->asRoofTops())
                currentRoofTileChanged(entry, RoofObject::TileTop, mergeable);
            else
                qFatal("unhandled category in BuildingEditorWindow::tileSelectionChanged()");
        }
    }
}

void BuildingEditorWindow::furnitureSelectionChanged()
{
    if (!mCurrentDocument)
        return;

    QModelIndexList indexes = ui->furnitureView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        if (FurnitureTile *ftile = ui->furnitureView->model()->tileAt(index)) {
            FurnitureTool::instance()->setCurrentTile(ftile);

            // Assign the new tile to selected objects
            QList<FurnitureObject*> objects;
            foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
                if (FurnitureObject *furniture = object->asFurniture()) {
                    if (furniture->furnitureTile() != ftile)
                        objects += furniture;
                }
            }

            if (objects.count() > 1)
                mCurrentDocument->undoStack()->beginMacro(tr("Change Furniture Tile"));
            foreach (FurnitureObject *furniture, objects)
                mCurrentDocument->undoStack()->push(new ChangeFurnitureObjectTile(mCurrentDocument,
                                                                            furniture,
                                                                            ftile));
            if (objects.count() > 1)
                mCurrentDocument->undoStack()->endMacro();

        }
    }
    updateActions();
}

void BuildingEditorWindow::usedTilesChanged()
{
    if (ui->categoryList->currentRow() == 0)
        categorySelectionChanged();
}

void BuildingEditorWindow::usedFurnitureChanged()
{
    if (ui->categoryList->currentRow() == 1)
        categorySelectionChanged();
}


void BuildingEditorWindow::resetUsedTiles()
{
    Building *building = currentBuilding();
    if (!building)
        return;

    QList<BuildingTileEntry*> entries;
    foreach (BuildingFloor *floor, building->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (object->asFurniture())
                continue;
            for (int i = 0; i < 3; i++) {
                if (object->tile(i) && !object->tile(i)->isNone()
                        && !entries.contains(object->tile(i)))
                    entries += object->tile(i);
            }
        }
    }
    BuildingTileEntry *entry = building->exteriorWall();
    if (entry && !entry->isNone() && !entries.contains(entry))
        entries += entry;
    foreach (Room *room, building->rooms()) {
        foreach (BuildingTileEntry *entry, room->tiles()) {
            if (entry && !entry->isNone() && !entries.contains(entry))
                entries += entry;
        }
    }
    mCurrentDocument->undoStack()->push(new ChangeUsedTiles(mCurrentDocument,
                                                            entries));
}

void BuildingEditorWindow::resetUsedFurniture()
{
    Building *building = currentBuilding();
    if (!building)
        return;

    QList<FurnitureTiles*> furniture;
    foreach (BuildingFloor *floor, building->floors()) {
        foreach (BuildingObject *object, floor->objects()) {
            if (FurnitureObject *fo = object->asFurniture()) {
                if (FurnitureTile *ftile = fo->furnitureTile()) {
                    if (!furniture.contains(ftile->owner()))
                        furniture += ftile->owner();
                }
            }
        }
    }

    mCurrentDocument->undoStack()->push(new ChangeUsedFurniture(mCurrentDocument,
                                                                furniture));
}

void BuildingEditorWindow::currentEWallChanged(BuildingTileEntry *entry, bool mergeable)
{
    // Assign the new tile to selected wall objects.
    QList<WallObject*> objects;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (WallObject *wall = object->asWall()) {
            if (wall->tile() != entry)
                objects += wall;
        }
    }
    if (objects.size()) {
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Wall Object Exterior Tile"));
        foreach (WallObject *wall, objects)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     wall,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
    if (objects.size() || WallTool::instance()->isCurrent()) {
        WallTool::instance()->setCurrentExteriorTile(entry);
        return;
    }

    mCurrentDocument->undoStack()->push(new ChangeEWall(mCurrentDocument, entry,
                                                        mergeable));
}

void BuildingEditorWindow::currentIWallChanged(BuildingTileEntry *entry, bool mergeable)
{
    // Assign the new tile to selected wall objects.
    QList<WallObject*> objects;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (WallObject *wall = object->asWall()) {
            if (wall->tile() != entry)
                objects += wall;
        }
    }
    if (objects.size()) {
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Wall Object Interior Tile"));
        foreach (WallObject *wall, objects)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     wall,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (objects.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
    if (objects.size() || WallTool::instance()->isCurrent()) {
        WallTool::instance()->setCurrentInteriorTile(entry);
        return;
    }

    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           Room::InteriorWall,
                                                           entry, mergeable));
}

void BuildingEditorWindow::currentFloorChanged(BuildingTileEntry *entry, bool mergeable)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           Room::Floor,
                                                           entry, mergeable));
}

void BuildingEditorWindow::currentDoorChanged(BuildingTileEntry *entry, bool mergeable)
{
    currentBuilding()->setDoorTile(entry);

    // Assign the new tile to selected doors
    QList<Door*> doors;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Door *door = object->asDoor()) {
            if (door->tile() != entry)
                doors += door;
        }
    }
    if (doors.count()) {
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Tile"));
        foreach (Door *door, doors)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     door,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentDoorFrameChanged(BuildingTileEntry *entry, bool mergeable)
{
    currentBuilding()->setDoorFrameTile(entry);

    // Assign the new tile to selected doors
    QList<Door*> doors;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Door *door = object->asDoor()) {
            if (door->frameTile() != entry)
                doors += door;
        }
    }
    if (doors.count()) {
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
        foreach (Door *door, doors)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     door,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentWindowChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New windows will be created with this tile
    currentBuilding()->setWindowTile(entry);

    // Assign the new tile to selected windows
    QList<Window*> windows;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Window *window = object->asWindow()) {
            if (window->tile() != entry)
                windows += window;
        }
    }
    if (windows.count()) {
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Door Frame Tile"));
        foreach (Window *window, windows)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     window,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentCurtainsChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New windows will be created with this tile
    currentBuilding()->setCurtainsTile(entry);

    // Assign the new tile to selected windows
    QList<Window*> windows;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Window *window = object->asWindow()) {
            if (window->curtainsTile() != entry)
                windows += window;
        }
    }
    if (windows.count()) {
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Window Curtains"));
        foreach (Window *window, windows)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     window,
                                                                     entry,
                                                                     mergeable,
                                                                     1));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentStairsChanged(BuildingTileEntry *entry, bool mergeable)
{
    // New stairs will be created with this tile
    currentBuilding()->setStairsTile(entry);

    // Assign the new tile to selected stairs
    QList<Stairs*> stairsList;
    foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
        if (Stairs *stairs = object->asStairs()) {
            if (stairs->tile() != entry)
                stairsList += stairs;
        }
    }
    if (stairsList.count()) {
        if (stairsList.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Stairs Tile"));
        foreach (Stairs *stairs, stairsList)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     stairs,
                                                                     entry,
                                                                     mergeable,
                                                                     0));
        if (stairsList.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentRoomTileChanged(int entryEnum,
                                                  BuildingTileEntry *entry,
                                                  bool mergeable)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeRoomTile(mCurrentDocument,
                                                           currentRoom(),
                                                           entryEnum,
                                                           entry, mergeable));
}

void BuildingEditorWindow::currentRoofTileChanged(BuildingTileEntry *entry, int which, bool mergeable)
{
    // New roofs will be created with these tiles
    switch (which) {
    case RoofObject::TileCap: currentBuilding()->setRoofCapTile(entry); break;
    case RoofObject::TileSlope: currentBuilding()->setRoofSlopeTile(entry); break;
    case RoofObject::TileTop: currentBuilding()->setRoofTopTile(entry); break;
    default:
        qFatal("bogus 'which' passed to BuildingEditorWindow::currentRoofTileChanged");
        break;
    }

    updateActions(); // in case the roof tools should be enabled

    QList<RoofObject*> objectList;
    bool selectedOnly = (which == RoofObject::TileTop);
    if (selectedOnly) {
        foreach (BuildingObject *object, mCurrentDocument->selectedObjects()) {
            if (RoofObject *roof = object->asRoof())
                if (roof->tile(which) != entry)
                    objectList += roof;
        }
    } else {
        // Change the tiles for each roof object.
        foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (RoofObject *roof = object->asRoof())
                    if (roof->tile(which) != entry)
                        objectList += roof;
            }
        }
    }

    if (objectList.count()) {
        if (objectList.count() > 1)
            mCurrentDocument->undoStack()->beginMacro(tr("Change Roof Tiles"));
        foreach (RoofObject *roof, objectList)
            mCurrentDocument->undoStack()->push(new ChangeObjectTile(mCurrentDocument,
                                                                     roof,
                                                                     entry,
                                                                     mergeable,
                                                                     which));
        if (objectList.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::selectCurrentCategoryTile()
{
    if (!mCurrentDocument || !mCategory)
        return;
    BuildingTileEntry *currentTile = 0;
    if (mCategory->asExteriorWalls()) {
        if (WallTool::instance()->isCurrent())
            currentTile = WallTool::instance()->currentExteriorTile();
        else
            currentTile = mCurrentDocument->building()->exteriorWall();
    }
    if (mCategory->asInteriorWalls()) {
        if (WallTool::instance()->isCurrent())
            currentTile = WallTool::instance()->currentInteriorTile();
        else if (currentRoom())
            currentTile = currentRoom()->tile(Room::InteriorWall);
    }
    if (currentRoom() && mCategory->asFloors())
        currentTile = currentRoom()->tile(Room::Floor);
    if (mCategory->asDoors())
        currentTile = mCurrentDocument->building()->doorTile();
    if (mCategory->asDoorFrames())
        currentTile = mCurrentDocument->building()->doorFrameTile();
    if (mCategory->asWindows())
        currentTile = mCurrentDocument->building()->windowTile();
    if (mCategory->asCurtains())
        currentTile = mCurrentDocument->building()->curtainsTile();
    if (mCategory->asStairs())
        currentTile = mCurrentDocument->building()->stairsTile();
    if (mCategory->asGrimeFloor() && currentRoom())
        currentTile = currentRoom()->tile(Room::GrimeFloor);
    if (mCategory->asGrimeWall() && currentRoom())
        currentTile = currentRoom()->tile(Room::GrimeWall);
    if (mCategory->asRoofCaps())
        currentTile = mCurrentDocument->building()->roofCapTile();
    if (mCategory->asRoofSlopes())
        currentTile = mCurrentDocument->building()->roofSlopeTile();
    if (mCategory->asRoofTops())
        currentTile = mCurrentDocument->building()->roofTopTile();
    if (currentTile && (currentTile->isNone() || (currentTile->category() == mCategory))) {
        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(currentTile->displayTile())) {
            QModelIndex index = ui->tilesetView->model()->index(tile);
            mSynching = true;
            ui->tilesetView->setCurrentIndex(index);
            mSynching = false;
        }
    }
}

void BuildingEditorWindow::removeAutoSaveFile()
{
    if (mAutoSaveFileName.isEmpty())
        return;
    QFile file(mAutoSaveFileName);
    if (file.exists()) {
        file.remove();
        qDebug() << "BuildingEd autosave deleted:" << mAutoSaveFileName;
    }
    mAutoSaveFileName.clear();
}

BuildingTileCategory *BuildingEditorWindow::categoryAt(int row)
{
    if (row >= mRowOfFirstCategory &&
            row < mRowOfFirstCategory + BuildingTilesMgr::instance()->categoryCount())
        return BuildingTilesMgr::instance()->category(row - mRowOfFirstCategory);
    return 0;
}

FurnitureGroup *BuildingEditorWindow::furnitureGroupAt(int row)
{
    if (row >= mRowOfFirstFurnitureGroup &&
            row < mRowOfFirstFurnitureGroup + FurnitureGroups::instance()->groupCount())
        return FurnitureGroups::instance()->group(row - mRowOfFirstFurnitureGroup);
    return 0;
}

void BuildingEditorWindow::upLevel()
{
    if ( mCurrentDocument->currentFloorIsTop())
        return;
    int level = mCurrentDocument->currentLevel() + 1;
    mCurrentDocument->setSelectedObjects(QSet<BuildingObject*>());
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
//    updateActions();
}

void BuildingEditorWindow::downLevel()
{
    if (mCurrentDocument->currentFloorIsBottom())
        return;
    int level = mCurrentDocument->currentLevel() - 1;
    mCurrentDocument->setSelectedObjects(QSet<BuildingObject*>());
    mCurrentDocument->setCurrentFloor(mCurrentDocument->building()->floor(level));
//    updateActions();
}

void BuildingEditorWindow::insertFloorAbove()
{
    if (!mCurrentDocument)
        return;
    BuildingFloor *newFloor = new BuildingFloor(mCurrentDocument->building(),
                                                mCurrentDocument->currentLevel() + 1);
    mCurrentDocument->undoStack()->push(new InsertFloor(mCurrentDocument,
                                                        newFloor->level(),
                                                        newFloor));
    mCurrentDocument->setSelectedObjects(QSet<BuildingObject*>());
    mCurrentDocument->setCurrentFloor(newFloor);
}

void BuildingEditorWindow::insertFloorBelow()
{
    if (!mCurrentDocument)
        return;
    int level = mCurrentDocument->currentLevel();
    BuildingFloor *newFloor = new BuildingFloor(mCurrentDocument->building(),
                                                level);
    mCurrentDocument->undoStack()->push(new InsertFloor(mCurrentDocument,
                                                        newFloor->level(),
                                                        newFloor));
    mCurrentDocument->setSelectedObjects(QSet<BuildingObject*>());
    mCurrentDocument->setCurrentFloor(newFloor);
}

void BuildingEditorWindow::removeFloor()
{
    if (!mCurrentDocument || mCurrentDocument->building()->floorCount() == 1)
        return;

    int index = mCurrentDocument->currentLevel();
    mCurrentDocument->undoStack()->push(new RemoveFloor(mCurrentDocument,
                                                        index));
}

void BuildingEditorWindow::floorsDialog()
{
    if (!mCurrentDocument)
        return;

    BuildingFloorsDialog dialog(mCurrentDocument, this);
    dialog.exec();
}

void BuildingEditorWindow::newBuilding()
{
    if (!confirmSave())
        return;

    NewBuildingDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Building *building = new Building(dialog.buildingWidth(),
                                      dialog.buildingHeight(),
                                      dialog.buildingTemplate());
    building->insertFloor(0, new BuildingFloor(building, 0));

    BuildingDocument *doc = new BuildingDocument(building, QString());

    addDocument(doc);
}

void BuildingEditorWindow::openBuilding()
{
    if (!confirmSave())
        return;

    QString filter = tr("TileZed building files (*.tbx)");
    filter += QLatin1String(";;");
    filter += tr("All Files (*)");

    QString initialDir = mSettings.value(
                QLatin1String("BuildingEditor/OpenSaveDirectory")).toString();
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Building"),
                                                    initialDir, filter);
    if (fileName.isEmpty())
        return;

    mSettings.setValue(QLatin1String("BuildingEditor/OpenSaveDirectory"),
                       QFileInfo(fileName).absolutePath());

    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        addDocument(doc);
        return;
    }

    QMessageBox::warning(this, tr("Error reading building"), error);
}

bool BuildingEditorWindow::saveBuilding()
{
    if (!mCurrentDocument)
        return false;

    const QString currentFileName = mCurrentDocument->fileName();

    if (currentFileName.endsWith(QLatin1String(".tbx"), Qt::CaseInsensitive))
        return writeBuilding(mCurrentDocument, currentFileName);
    else
        return saveBuildingAs();
}

bool BuildingEditorWindow::saveBuildingAs()
{
    if (!mCurrentDocument)
        return false;

    QString suggestedFileName;
    if (!mCurrentDocument->fileName().isEmpty()) {
        const QFileInfo fileInfo(mCurrentDocument->fileName());
        suggestedFileName = fileInfo.path();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += fileInfo.completeBaseName();
        suggestedFileName += QLatin1String(".tbx");
    } else {
        suggestedFileName = mSettings.value(
                    QLatin1String("BuildingEditor/OpenSaveDirectory")).toString();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += tr("untitled.tbx");
    }

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), suggestedFileName,
                                         tr("TileZed building files (*.tbx)"));
    if (!fileName.isEmpty()) {
        mSettings.setValue(QLatin1String("BuildingEditor/OpenSaveDirectory"),
                           QFileInfo(fileName).absolutePath());
        bool ok = writeBuilding(mCurrentDocument, fileName);
        if (ok)
            updateWindowTitle();
        return ok;
    }
    return false;
}

bool BuildingEditorWindow::writeBuilding(BuildingDocument *doc, const QString &fileName)
{
    if (!doc)
        return false;

    QString error;
    if (!doc->write(fileName, error)) {
        QMessageBox::critical(this, tr("Error Saving Building"), error);
        return false;
    }

//    setRecentFile(fileName);
    return true;
}

bool BuildingEditorWindow::confirmSave()
{
    if (!mCurrentDocument || !mCurrentDocument->isModified())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return saveBuilding();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

void BuildingEditorWindow::addDocument(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        // Disable all the tools before losing the document/views/etc.
        ToolManager::instance()->clearDocument();
        mRoomComboBox->clear();
        mOrthoScene->clearDocument();
        mIsoView->clearDocument();
        mLayersDock->clearDocument();
        mTilesetDock->clearDocument();
        mUndoGroup->removeStack(mCurrentDocument->undoStack());
        delete mCurrentDocument->building();
        delete mCurrentDocument;
        removeAutoSaveFile();
        if (mAutoSaveTimer->isActive())
            mAutoSaveTimer->stop();
    }

    mCurrentDocument = doc;

    Building *building = mCurrentDocument->building();
    mCurrentDocument->setCurrentFloor(building->floor(0));
    mUndoGroup->addStack(mCurrentDocument->undoStack());
    mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

    // Roof tiles need to be non-none to enable the roof tools.
    // Old templates will have 'none' for these tiles.
    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
    if (building->roofCapTile()->isNone())
        building->setRoofCapTile(btiles->defaultRoofCapTiles());
    if (building->roofSlopeTile()->isNone())
        building->setRoofSlopeTile(btiles->defaultRoofSlopeTiles());
#if 0
    if (building->roofTopTile()->isNone())
        building->setRoofTopTile(btiles->defaultRoofTopTiles());
#endif

    // Handle reading old buildings
    if (building->usedTiles().isEmpty()) {
        QList<BuildingTileEntry*> entries;
        QList<FurnitureTiles*> furniture;
        foreach (BuildingFloor *floor, building->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (FurnitureObject *fo = object->asFurniture()) {
                    if (FurnitureTile *ftile = fo->furnitureTile()) {
                        if (!furniture.contains(ftile->owner()))
                            furniture += ftile->owner();
                    }
                    continue;
                }
                for (int i = 0; i < 3; i++) {
                    if (object->tile(i) && !object->tile(i)->isNone()
                            && !entries.contains(object->tile(i)))
                        entries += object->tile(i);
                }
            }
        }
        BuildingTileEntry *entry = building->exteriorWall();
        if (entry && !entry->isNone() && !entries.contains(entry))
            entries += entry;
        foreach (Room *room, building->rooms()) {
            foreach (BuildingTileEntry *entry, room->tiles()) {
                if (entry && !entry->isNone() && !entries.contains(entry))
                    entries += entry;
            }
        }
        building->setUsedTiles(entries);
        building->setUsedFurniture(furniture);
    }

    if (ui->categoryList->currentRow() < 2)
        categorySelectionChanged();

    mOrthoScene->setDocument(mCurrentDocument);
    mIsoView->setDocument(mCurrentDocument);
    mLayersDock->setDocument(mCurrentDocument);
    mTilesetDock->setDocument(mCurrentDocument);

    updateRoomComboBox();

    resizeCoordsLabel();

    // Redisplay "Used Tiles" and "Used Furniture"
    connect(mCurrentDocument, SIGNAL(usedFurnitureChanged()),
            SLOT(usedFurnitureChanged()));
    connect(mCurrentDocument, SIGNAL(usedTilesChanged()),
            SLOT(usedTilesChanged()));

    connect(mCurrentDocument, SIGNAL(roomAdded(Room*)), SLOT(roomAdded(Room*)));
    connect(mCurrentDocument, SIGNAL(roomRemoved(Room*)), SLOT(roomRemoved(Room*)));
    connect(mCurrentDocument, SIGNAL(roomsReordered()), SLOT(roomsReordered()));
    connect(mCurrentDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));

    connect(mCurrentDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(updateActions()));
    connect(mCurrentDocument, SIGNAL(floorRemoved(BuildingFloor*)),
            SLOT(updateActions()));
    connect(mCurrentDocument, SIGNAL(currentFloorChanged()),
            SLOT(updateActions()));
    connect(mCurrentDocument, SIGNAL(currentLayerChanged()),
            SLOT(updateActions()));

    connect(mCurrentDocument, SIGNAL(selectedObjectsChanged()),
            SLOT(updateActions()));

    connect(mCurrentDocument, SIGNAL(tileSelectionChanged(QRegion)),
            SLOT(updateActions()));
    connect(mCurrentDocument, SIGNAL(clipboardTilesChanged()),
            SLOT(updateActions()));

    connect(mCurrentDocument, SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));

    updateActions();

    if (building->roomCount())
        PencilTool::instance()->makeCurrent();

    updateWindowTitle();

    reportMissingTilesets();
}

void BuildingEditorWindow::clearDocument()
{
    if (mCurrentDocument) {
        // Disable all the tools before losing the document/views/etc.
        ToolManager::instance()->clearDocument();
        mOrthoScene->clearDocument();
        mIsoView->clearDocument();
        mLayersDock->clearDocument();
        mTilesetDock->clearDocument();
        mUndoGroup->removeStack(mCurrentDocument->undoStack());
        delete mCurrentDocument->building();
        delete mCurrentDocument;
        mCurrentDocument = 0;
        updateRoomComboBox();
        resizeCoordsLabel();
        updateActions();
        updateWindowTitle();
        removeAutoSaveFile();
    }
}

void BuildingEditorWindow::updateWindowTitle()
{
    QString fileName = mCurrentDocument ? mCurrentDocument->fileName() : QString();
    if (fileName.isEmpty())
        fileName = tr("Untitled");
    else {
        fileName = QDir::toNativeSeparators(fileName);
    }
    setWindowTitle(tr("[*]%1 - Building Editor").arg(fileName));
    setWindowFilePath(fileName);
    setWindowModified(mCurrentDocument ? mCurrentDocument->isModified() : false);
}

void BuildingEditorWindow::exportTMX()
{
    QString initialDir = mSettings.value(
                QLatin1String("BuildingEditor/ExportDirectory")).toString();

    if (!mCurrentDocument->fileName().isEmpty()) {
        QFileInfo info(mCurrentDocument->fileName());
        initialDir += tr("/%1").arg(info.completeBaseName());
    }

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), initialDir,
                                         tr("Tiled map files (*.tmx)"));
    if (fileName.isEmpty())
        return;

    if (!BuildingTMX::instance()->exportTMX(currentBuilding(), fileName)) {
        QMessageBox::critical(this, tr("Error Saving Map"),
                              BuildingTMX::instance()->errorString());

    }

    mSettings.setValue(QLatin1String("BuildingEditor/ExportDirectory"),
                       QFileInfo(fileName).absolutePath());
}

void BuildingEditorWindow::editCut()
{
    if (!mCurrentDocument || currentLayer().isEmpty())
        return;

    if (mEditMode == TileMode) {
        QRegion selection = mCurrentDocument->tileSelection();
        if (!selection.isEmpty()) {
            QRect r = selection.boundingRect();
            FloorTileGrid *tiles = currentFloor()->grimeAt(currentLayer(), r, selection);
            mCurrentDocument->setClipboardTiles(tiles, selection.translated(-r.topLeft()));

            tiles = tiles->clone();
            if (tiles->replace(selection.translated(-r.topLeft()), QString()))
                mCurrentDocument->undoStack()->push(
                            new PaintFloorTiles(mCurrentDocument, currentFloor(),
                                                currentLayer(), selection,
                                                r.topLeft(), tiles,
                                                "Cut Tiles"));
            else
                delete tiles;
        }
    }
}

void BuildingEditorWindow::editCopy()
{
    if (!mCurrentDocument || currentLayer().isEmpty())
        return;

    if (mEditMode == TileMode) {
        QRegion selection = mCurrentDocument->tileSelection();
        if (!selection.isEmpty()) {
            QRect r = selection.boundingRect();
            FloorTileGrid *tiles = currentFloor()->grimeAt(currentLayer(), r, selection);
            mCurrentDocument->setClipboardTiles(tiles, selection.translated(-r.topLeft()));
        }
    }
}

void BuildingEditorWindow::editPaste()
{
    if (!mCurrentDocument || currentLayer().isEmpty())
        return;

    if (mEditMode == TileMode) {
        if (FloorTileGrid *tiles = mCurrentDocument->clipboardTiles()) {
            DrawTileTool::instance()->makeCurrent();
            DrawTileTool::instance()->setCaptureTiles(tiles->clone(),
                                                      mCurrentDocument->clipboardTilesRgn());
        }
    }
}

void BuildingEditorWindow::editDelete()
{
    if (!mCurrentDocument)
        return;
    if (mEditMode == TileMode) {
        if (currentLayer().isEmpty())
            return;
        QRegion selection = currentDocument()->tileSelection();
        QRect r = selection.boundingRect();
        FloorTileGrid *tiles = currentFloor()->grimeAt(currentLayer(), r);
        bool changed = tiles->replace(selection.translated(-r.topLeft()), QString());
        if (changed)
            mCurrentDocument->undoStack()->push(
                        new PaintFloorTiles(mCurrentDocument, currentFloor(),
                                            currentLayer(), selection,
                                            r.topLeft(), tiles,
                                            "Delete Tiles"));
        else
            delete tiles;
    }
    if (mEditMode == ObjectMode) {
        deleteObjects();
    }
}

void BuildingEditorWindow::selectAll()
{
    if (!mCurrentDocument)
        return;
    if (mEditMode == TileMode) {
        mCurrentDocument->undoStack()->push(
                    new ChangeTileSelection(mCurrentDocument, currentFloor()->bounds(1, 1)));
        return;
    }
    QSet<BuildingObject*> objects = currentFloor()->objects().toSet();
    mCurrentDocument->setSelectedObjects(objects);
}

void BuildingEditorWindow::selectNone()
{
    if (!mCurrentDocument)
        return;
    if (mEditMode == TileMode) {
        mCurrentDocument->undoStack()->push(
                    new ChangeTileSelection(mCurrentDocument, QRegion()));
        return;
    }
    mCurrentDocument->setSelectedObjects(QSet<BuildingObject*>());
}

void BuildingEditorWindow::deleteObjects()
{
    if (!mCurrentDocument)
        return;
    QSet<BuildingObject*> selected = mCurrentDocument->selectedObjects();
    if (!selected.size())
        return;
    if (selected.size() > 1)
        mCurrentDocument->undoStack()->beginMacro(tr("Remove %1 Objects")
                                                  .arg(selected.size()));
    foreach (BuildingObject *object, selected) {
        mCurrentDocument->undoStack()->push(new RemoveObject(mCurrentDocument,
                                                             object->floor(),
                                                             object->index()));
    }

    if (selected.size() > 1)
        mCurrentDocument->undoStack()->endMacro();
}

void BuildingEditorWindow::preferences()
{
    BuildingPreferencesDialog dialog(this);
    dialog.exec();
}

void BuildingEditorWindow::roomsDialog()
{
    if (!mCurrentDocument)
        return;
    QList<Room*> originalRoomList = mCurrentDocument->building()->rooms();
    RoomsDialog dialog(originalRoomList, this);
    dialog.setWindowTitle(tr("Rooms in building"));

    if (dialog.exec() != QDialog::Accepted)
        return;

    mCurrentDocument->undoStack()->beginMacro(tr("Edit Rooms"));

    QList<Room*> deletedRooms = originalRoomList;
    // Rooms may have been added, moved, deleted, and/or edited.
    foreach (Room *dialogRoom, dialog.rooms()) {
        if (Room *room = dialog.originalRoom(dialogRoom)) {
            int oldIndex = mCurrentDocument->building()->rooms().indexOf(room);
            int newIndex = dialog.rooms().indexOf(dialogRoom);
            if (oldIndex != newIndex) {
                qDebug() << "move room" << room->Name << "from " << oldIndex << " to " << newIndex;
                mCurrentDocument->undoStack()->push(new ReorderRoom(mCurrentDocument,
                                                                    newIndex,
                                                                    room));
            }
            if (*room != *dialogRoom) {
                qDebug() << "change room" << room->Name;
                mCurrentDocument->undoStack()->push(new ChangeRoom(mCurrentDocument,
                                                                   room,
                                                                   dialogRoom));
            }
            deletedRooms.removeOne(room);
        } else {
            // This is a new room.
            qDebug() << "add room" << dialogRoom->Name << "at" << dialog.rooms().indexOf(dialogRoom);
            Room *newRoom = new Room(dialogRoom);
            int index = dialog.rooms().indexOf(dialogRoom);
            mCurrentDocument->undoStack()->push(new AddRoom(mCurrentDocument,
                                                            index,
                                                            newRoom));
        }
    }
    // now handle deleted rooms
    foreach (Room *room, deletedRooms) {
        qDebug() << "delete room" << room->Name;
        int index = mCurrentDocument->building()->rooms().indexOf(room);
        foreach (BuildingFloor *floor, currentBuilding()->floors()) {
            bool changed = false;
            QVector<QVector<Room*> > grid = floor->grid();
            for (int x = 0; x < grid.size(); x++) {
                for (int y = 0; y < grid[x].size(); y++) {
                    if (grid[x][y] == room) {
                        grid[x][y] = 0;
                        changed = true;
                    }
                }
            }
            if (changed) {
                mCurrentDocument->undoStack()->push(new SwapFloorGrid(mCurrentDocument,
                                                                      floor,
                                                                      grid,
                                                                      "Remove Room From Floor"));
            }
        }
        mCurrentDocument->undoStack()->push(new RemoveRoom(mCurrentDocument,
                                                           index));
    }

    mCurrentDocument->undoStack()->endMacro();
}

void BuildingEditorWindow::roomAdded(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
    updateActions();
}

void BuildingEditorWindow::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
    updateActions();
}

void BuildingEditorWindow::roomsReordered()
{
    updateRoomComboBox();
}

void BuildingEditorWindow::roomChanged(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
}

void BuildingEditorWindow::resizeBuilding()
{
    if (!mCurrentDocument)
        return;

    ResizeBuildingDialog dialog(mCurrentDocument->building(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    QSize newSize = dialog.buildingSize();

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Resize Building"));
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, true));
    undoStack->push(new ResizeBuilding(mCurrentDocument, newSize));
    foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {
        undoStack->push(new ResizeFloor(mCurrentDocument, floor, newSize));
        // Remove objects that aren't in bounds.
        for (int i = floor->objectCount() - 1; i >= 0; --i) {
            BuildingObject *object = floor->object(i);
            if (!object->isValidPos())
                undoStack->push(new RemoveObject(mCurrentDocument,
                                                 floor, i));
        }
    }
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, false));
    undoStack->endMacro();
}

void BuildingEditorWindow::flipHorizontal()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Flip Horizontal"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new FlipBuilding(mCurrentDocument, true));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();
}

void BuildingEditorWindow::flipVertical()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    mCurrentDocument->undoStack()->beginMacro(tr("Flip Vertical"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new FlipBuilding(mCurrentDocument, false));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();
}

void BuildingEditorWindow::rotateRight()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Rotate Right"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new RotateBuilding(mCurrentDocument, true));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();
}

void BuildingEditorWindow::rotateLeft()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Rotate Left"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new RotateBuilding(mCurrentDocument, false));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();
}

void BuildingEditorWindow::templatesDialog()
{
    BuildingTemplatesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    BuildingTemplates::instance()->replaceTemplates(dialog.templates());
    BuildingTemplates::instance()->writeTxt(this);
}

void BuildingEditorWindow::tilesDialog()
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();
    dialog->reparent(this);
    dialog->exec();
}

void BuildingEditorWindow::tilesDialogEdited()
{
    int row = ui->categoryList->currentRow();
    setCategoryList();
    row = qMin(row, ui->categoryList->count() - 1);
    ui->categoryList->setCurrentRow(row);
}

void BuildingEditorWindow::templateFromBuilding()
{
    if (!mCurrentDocument)
        return;

    TemplateFromBuildingDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Building *building = mCurrentDocument->building();
    BuildingTemplate *btemplate = new BuildingTemplate;
    btemplate->setName(dialog.name());
    btemplate->setTiles(building->tiles());

    foreach (Room *room, building->rooms())
        btemplate->addRoom(new Room(room));

    btemplate->setUsedTiles(building->usedTiles());
    btemplate->setUsedFurniture(building->usedFurniture());

    BuildingTemplates::instance()->addTemplate(btemplate);

    BuildingTemplates::instance()->writeTxt(this);

    templatesDialog();
}

void BuildingEditorWindow::zoomIn()
{
    if (mEditMode == ObjectMode && mOrient == OrientOrtho)
        mOrthoView->zoomable()->zoomIn();
    else
        mIsoView->zoomable()->zoomIn();
}

void BuildingEditorWindow::zoomOut()
{
    if (mEditMode == ObjectMode && mOrient == OrientOrtho)
        mOrthoView->zoomable()->zoomOut();
    else
        mIsoView->zoomable()->zoomOut();
}

void BuildingEditorWindow::resetZoom()
{
    if (mEditMode == ObjectMode && mOrient == OrientOrtho)
        mOrthoView->zoomable()->resetZoom();
    else
        mIsoView->zoomable()->resetZoom();
}

void BuildingEditorWindow::showObjectsChanged(bool show)
{
    Q_UNUSED(show)
    mOrthoView->scene()->synchObjectItemVisibility();
    mIsoView->scene()->synchObjectItemVisibility();
    updateActions();
}

void BuildingEditorWindow::mouseCoordinateChanged(const QPoint &tilePos)
{
    ui->coordLabel->setText(tr("%1,%2").arg(tilePos.x()).arg(tilePos.y()));
}

void BuildingEditorWindow::currentToolChanged(BaseTool *tool)
{
    Q_UNUSED(tool)
    selectCurrentCategoryTile();
    updateToolStatusText();
}

void BuildingEditorWindow::updateToolStatusText()
{
    if (BaseTool *tool = ToolManager::instance()->currentTool())
        ui->statusLabel->setText(tool->statusText());
    else
        ui->statusLabel->clear();
}

void BuildingEditorWindow::roofTypeChanged(QAction *action)
{
    int index = action->parentWidget()->actions().indexOf(action);

    static RoofObject::RoofType roofTypes[] = {
        RoofObject::SlopeW,
        RoofObject::SlopeN,
        RoofObject::SlopeE,
        RoofObject::SlopeS,
        RoofObject::PeakWE,
        RoofObject::PeakNS,
        RoofObject::FlatTop,
    };

    RoofTool::instance()->setRoofType(roofTypes[index]);

    ui->actionRoof->setIcon(action->icon());

    if (!RoofTool::instance()->isCurrent())
        RoofTool::instance()->makeCurrent();

}

void BuildingEditorWindow::roofCornerTypeChanged(QAction *action)
{
    int index = action->parentWidget()->actions().indexOf(action);

    static RoofObject::RoofType roofTypes[] = {
        RoofObject::CornerInnerNW,
        RoofObject::CornerInnerNE,
        RoofObject::CornerInnerSE,
        RoofObject::CornerInnerSW,

        RoofObject::CornerOuterNW,
        RoofObject::CornerOuterNE,
        RoofObject::CornerOuterSE,
        RoofObject::CornerOuterSW
    };

    RoofCornerTool::instance()->setRoofType(roofTypes[index]);

    ui->actionRoofCorner->setIcon(action->icon());

    if (!RoofCornerTool::instance()->isCurrent())
        RoofCornerTool::instance()->makeCurrent();

}

void BuildingEditorWindow::tilesetAdded(Tileset *tileset)
{
    Q_UNUSED(tileset)
    categorySelectionChanged();
}

void BuildingEditorWindow::tilesetAboutToBeRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    ui->tilesetView->model()->setTiles(QList<Tile*>());
    // FurnitureView doesn't cache Tiled::Tiles
}

void BuildingEditorWindow::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    categorySelectionChanged();
}

void BuildingEditorWindow::tilesetChanged(BuildingEditorWindow::Tileset *tileset)
{
    Q_UNUSED(tileset)
    categorySelectionChanged();
}

void BuildingEditorWindow::autoSaveCheck()
{
    if (!mCurrentDocument || !mCurrentDocument->isModified()) {
        if (mAutoSaveTimer->isActive()) {
            mAutoSaveTimer->stop();
            qDebug() << "BuildingEd auto-save timer stopped (document is clean)";
        }
        removeAutoSaveFile();
        return;
    }
    if (mAutoSaveTimer->isActive())
        return;
    mAutoSaveTimer->start();
    qDebug() << "BuildingEd auto-save timer started";
}

void BuildingEditorWindow::autoSaveTimeout()
{
    qDebug() << "BuildingEd auto-save timeout";
    if (!mCurrentDocument)
        return;
    QString fileName = mCurrentDocument->fileName();
    if (fileName.isEmpty()) {
        fileName = BuildingPreferences::instance()->configPath(QLatin1String("untitled.tbx"));
    }
    fileName += QLatin1String(".autosave");
    writeBuilding(mCurrentDocument, fileName);
    mAutoSaveFileName = fileName;
    qDebug() << "BuildingEd auto-saved:" << fileName;
}

void BuildingEditorWindow::reportMissingTilesets()
{
    Building *building = currentBuilding();

    QSet<QString> missingTilesets;
    foreach (BuildingTileEntry *entry, building->usedTiles()) {
        if (!entry || entry->isNone())
            continue;
        for (int i = 0; i < entry->category()->enumCount(); i++) {
            BuildingTile *btile = entry->tile(i);
            if (btile->mTilesetName.isEmpty())
                continue;
            if (!TileMetaInfoMgr::instance()->tileset(btile->mTilesetName))
                missingTilesets.insert(btile->mTilesetName);
        }
    }
    foreach (FurnitureTiles *ftiles, building->usedFurniture()) {
        foreach (FurnitureTile *ftile, ftiles->tiles()) {
            if (!ftile || ftile->isEmpty())
                continue;
            foreach (BuildingTile *btile, ftile->tiles()) {
                if (!btile || btile->mTilesetName.isEmpty())
                    continue;
                if (!TileMetaInfoMgr::instance()->tileset(btile->mTilesetName))
                    missingTilesets.insert(btile->mTilesetName);
            }
        }
    }
    if (missingTilesets.size()) {
        QStringList tilesets(missingTilesets.values());
        tilesets.sort();
        ListOfStringsDialog dialog(tr("The following tileset files were not found."),
                                   tilesets, this);
        dialog.setWindowTitle(tr("Missing Tilesets"));
        dialog.exec();
    }
}

void BuildingEditorWindow::resizeCoordsLabel()
{
    int width = 999, height = 999;
    if (mCurrentDocument) {
        width = qMax(width, mCurrentDocument->building()->width());
        height = qMax(height, mCurrentDocument->building()->height());
    }
    QFontMetrics fm = ui->coordLabel->fontMetrics();
    QString coordString = QString(QLatin1String("%1,%2")).arg(width).arg(height);
    ui->coordLabel->setMinimumWidth(fm.width(coordString) + 8);
}

void BuildingEditorWindow::setCategoryList()
{
    mSynching = true;

    ui->categoryList->clear();

    ui->categoryList->addItem(tr("Used Tiles"));
    ui->categoryList->addItem(tr("Used Furniture"));

    HorizontalLineDelegate::instance()->addToList(ui->categoryList);

    mRowOfFirstCategory = ui->categoryList->count();
    foreach (BuildingTileCategory *category, BuildingTilesMgr::instance()->categories()) {
        ui->categoryList->addItem(category->label());
    }

    HorizontalLineDelegate::instance()->addToList(ui->categoryList);

    mRowOfFirstFurnitureGroup = ui->categoryList->count();
    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        ui->categoryList->addItem(group->mLabel);
    }

    mSynching = false;
}

void BuildingEditorWindow::updateActions()
{
    bool hasDoc = mCurrentDocument != 0;
    bool showObjects = BuildingPreferences::instance()->showObjects();
    bool objectMode = mEditMode == ObjectMode;

    PencilTool::instance()->setEnabled(hasDoc && objectMode && currentRoom() != 0);
    WallTool::instance()->setEnabled(hasDoc && objectMode && showObjects);
    SelectMoveRoomsTool::instance()->setEnabled(hasDoc && objectMode);
    DoorTool::instance()->setEnabled(hasDoc && objectMode && showObjects);
    WindowTool::instance()->setEnabled(hasDoc && objectMode && showObjects);
    StairsTool::instance()->setEnabled(hasDoc && objectMode && showObjects);
    FurnitureTool::instance()->setEnabled(hasDoc && objectMode && showObjects &&
            FurnitureTool::instance()->currentTile() != 0);
    bool roofTilesOK = hasDoc && currentBuilding()->roofCapTile()->asRoofCap() &&
            currentBuilding()->roofSlopeTile()->asRoofSlope() /*&&
            currentBuilding()->roofTopTile()->asRoofTop()*/;
    RoofTool::instance()->setEnabled(hasDoc && objectMode && showObjects && roofTilesOK);
    RoofCornerTool::instance()->setEnabled(hasDoc && objectMode && showObjects && roofTilesOK);
    SelectMoveObjectTool::instance()->setEnabled(hasDoc && objectMode && showObjects);

    DrawTileTool::instance()->setEnabled(!objectMode && !currentLayer().isEmpty());
    SelectTileTool::instance()->setEnabled(!objectMode && !currentLayer().isEmpty());

    ui->actionUpLevel->setEnabled(hasDoc &&
                                  !mCurrentDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(hasDoc &&
                                    !mCurrentDocument->currentFloorIsBottom());

//    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionExportTMX->setEnabled(hasDoc);

    ui->actionShowObjects->setEnabled(hasDoc);

    Zoomable *zoomable = (mEditMode == ObjectMode && mOrient == OrientOrtho)
            ? mOrthoView->zoomable()
            : mIsoView->zoomable();
    ui->actionZoomIn->setEnabled(hasDoc && zoomable->canZoomIn());
    ui->actionZoomOut->setEnabled(hasDoc && zoomable->canZoomOut());
    ui->actionNormalSize->setEnabled(hasDoc && zoomable->scale() != 1.0);

    ui->actionRooms->setEnabled(hasDoc);
    ui->actionTemplateFromBuilding->setEnabled(hasDoc);

    ui->actionResize->setEnabled(hasDoc);
    ui->actionFlipHorizontal->setEnabled(hasDoc);
    ui->actionFlipVertical->setEnabled(hasDoc);
    ui->actionRotateRight->setEnabled(hasDoc);
    ui->actionRotateLeft->setEnabled(hasDoc);

    ui->actionInsertFloorAbove->setEnabled(hasDoc);
    ui->actionInsertFloorBelow->setEnabled(hasDoc);
    ui->actionRemoveFloor->setEnabled(hasDoc &&
                                      mCurrentDocument->building()->floorCount() > 1);
    ui->actionFloors->setEnabled(hasDoc);

    mRoomComboBox->setEnabled(hasDoc && currentRoom() != 0);

    bool hasTileSel = hasDoc && !objectMode && !mCurrentDocument->tileSelection().isEmpty();
    ui->actionCut->setEnabled(hasTileSel);
    ui->actionCopy->setEnabled(hasTileSel);
    ui->actionPaste->setEnabled(hasDoc && !objectMode && mCurrentDocument->clipboardTiles());
    if (!objectMode) {
        ui->actionSelectAll->setEnabled(!currentLayer().isEmpty());
        ui->actionSelectNone->setEnabled(hasTileSel);
        ui->actionDelete->setEnabled(hasTileSel);
    } else {
        ui->actionSelectAll->setEnabled(hasDoc);
        ui->actionSelectNone->setEnabled(hasDoc && mCurrentDocument->selectedObjects().size());
        ui->actionDelete->setEnabled(hasDoc && mCurrentDocument->selectedObjects().size());
    }

    if (mCurrentDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentLevel() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    else
        mFloorLabel->clear();

    mTileModeToolBar->mFloorLabel->setText(mFloorLabel->text());
}

void BuildingEditorWindow::help()
{
    QString path = QLatin1String("file:///") +
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/BuildingEd/index.html");
    QDesktopServices::openUrl(QUrl(path, QUrl::TolerantMode));
}

void BuildingEditorWindow::toggleOrthoIso()
{
    if (mEditMode == TileMode) {
        toggleEditMode();
        return;
    }

    BuildingBaseScene *editor;
    if (mOrient == OrientOrtho) {
        mOrthoView->zoomable()->connectToComboBox(0);
        mIsoView->zoomable()->connectToComboBox(ui->editorScaleComboBox);
        editor = mIsoScene;
        mOrient = OrientIso;
    } else {
        mIsoView->zoomable()->connectToComboBox(0);
        mOrthoView->zoomable()->connectToComboBox(ui->editorScaleComboBox);
        editor = mOrthoScene;
        mOrient = OrientOrtho;
    }

    ui->stackedWidget->setCurrentIndex(!ui->stackedWidget->currentIndex());

    BaseTool *tool = ToolManager::instance()->currentTool();

    // Deactivate the current tool so it removes any cursor item from the
    // scene.
    if (tool)
        ToolManager::instance()->activateTool(0);

    PencilTool::instance()->setEditor(editor);
    SelectMoveRoomsTool::instance()->setEditor(editor);
    DoorTool::instance()->setEditor(editor);
    WallTool::instance()->setEditor(editor);
    WindowTool::instance()->setEditor(editor);
    StairsTool::instance()->setEditor(editor);
    FurnitureTool::instance()->setEditor(editor);
    RoofTool::instance()->setEditor(editor);
    RoofCornerTool::instance()->setEditor(editor);
    SelectMoveObjectTool::instance()->setEditor(editor);


    // Restore the current tool so it adds any cursor item back to the scene.
    if (tool)
        ToolManager::instance()->activateTool(tool);
}

void BuildingEditorWindow::toggleEditMode()
{
    ToolManager::instance()->clearDocument();

    if (mEditMode == ObjectMode) {
        // Switch to TileMode
        mTileModeToolBar->show();
        mEditMode = TileMode;
        mLayersDock->show(); // FIXME: unless the user hid it
        mTilesetDock->show(); // FIXME: unless the user hid it
        mFurnitureDock->show(); // FIXME: unless the user hid it
        ui->toolBar->hide();
        ui->dockWidget->hide();
        if (mFirstTimeSeen) {
            mFirstTimeSeen = false;
            mFurnitureDock->switchTo();
            mTilesetDock->firstTimeSetup();
        }
        mIsoView->scene()->setEditingTiles(true);
        if (mOrient == OrientOrtho) {
            ui->stackedWidget->setCurrentIndex(1);
            mOrthoView->zoomable()->connectToComboBox(0);
            mIsoView->zoomable()->connectToComboBox(ui->editorScaleComboBox);
            mIsoView->setFocus();
        }
    } else if (mEditMode == TileMode) {
        // Switch to ObjectMode
        if (mCurrentDocument)
            mCurrentDocument->setTileSelection(QRegion());
        ui->toolBar->show();
        ui->dockWidget->show();
        mTileModeToolBar->hide();
        mLayersDock->hide();
        mTilesetDock->hide();
        mFurnitureDock->hide();
        mEditMode = ObjectMode;
        mIsoView->scene()->setEditingTiles(false);
        if (mOrient == OrientOrtho) {
            ui->stackedWidget->setCurrentIndex(0);
            mIsoView->zoomable()->connectToComboBox(0);
            mOrthoView->zoomable()->connectToComboBox(ui->editorScaleComboBox);
            mOrthoView->setFocus();
        }
    }

    updateActions();
}
