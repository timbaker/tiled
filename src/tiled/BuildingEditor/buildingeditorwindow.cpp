/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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
#include "buildingdocumentmgr.h"
#include "buildingfloor.h"
#include "buildingfloorsdialog.h"
#include "buildinglayersdock.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreferencesdialog.h"
#include "buildingpropertiesdialog.h"
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
#include "choosebuildingtiledialog.h"
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
    QAction *mActionPickTile;
};

}

TileModeToolBar::TileModeToolBar(QWidget *parent) :
    QToolBar(parent),
    mFloorLabel(new QLabel),
    mActionPencil(new QAction(this)),
    mActionSelectTiles(new QAction(this)),
    mActionPickTile(new QAction(this))
{
    setObjectName(QString::fromUtf8("TileToolBar"));
    setWindowTitle(tr("Tile ToolBar"));

    mActionPencil->setToolTip(tr("Draw Tiles"));
    mActionSelectTiles->setToolTip(tr("Select Tiles"));
    mActionPickTile->setToolTip(tr("Pick Tile"));

    mActionPencil->setCheckable(true);
    mActionSelectTiles->setCheckable(true);
    mActionPickTile->setCheckable(true);

    addAction(mActionPencil);
    addAction(mActionSelectTiles);
    addAction(mActionPickTile);
    addSeparator();
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    addWidget(mFloorLabel);
}

/////

BuildingDocumentGUI::BuildingDocumentGUI(BuildingDocument *doc) :
    QObject(doc),
    mMainWindow(BuildingEditorWindow::instance()),
    mDocument(doc),
    mStackedWidget(new QStackedWidget),
    mOrthoView(new BuildingOrthoView),
    mOrthoScene(new BuildingOrthoScene(mOrthoView)),
    mIsoView(new BuildingIsoView),
    mIsoScene(new BuildingIsoScene(mIsoView)),
    mEditMode(IsoObjectMode),
    mPrevObjectTool(PencilTool::instance()),
    mPrevTileTool(DrawTileTool::instance())
{
    mOrthoView->setScene(mOrthoScene);
    mIsoView->setScene(mIsoScene);

    orthoView()->setDocument(document());
    isoView()->setDocument(document());

    mStackedWidget->addWidget(mOrthoView);
    mStackedWidget->addWidget(mIsoView);
    mStackedWidget->setCurrentIndex(1);

    connect(document(), SIGNAL(fileNameChanged()), SLOT(updateDocumentTab()));
    connect(document(), SIGNAL(cleanChanged()), SLOT(updateDocumentTab()));
    connect(document()->undoStack(), SIGNAL(cleanChanged(bool)), SLOT(updateDocumentTab()));

    connect(document()->undoStack(), SIGNAL(cleanChanged(bool)), SLOT(autoSaveCheck()));
    connect(document()->undoStack(), SIGNAL(indexChanged(int)), SLOT(autoSaveCheck()));

    mAutoSaveTimer.setSingleShot(true);
    mAutoSaveTimer.setInterval(2.5 * 60 * 1000); // 2.5 minutes
    connect(&mAutoSaveTimer, SIGNAL(timeout()), SLOT(autoSaveTimeout()));
}

BuildingDocumentGUI::~BuildingDocumentGUI()
{
    removeAutoSaveFile();
    delete mStackedWidget;
}

QGraphicsView *BuildingDocumentGUI::currentView() const
{
    if (mEditMode == OrthoObjectMode) return orthoView();
    return isoView();
}

BuildingBaseScene *BuildingDocumentGUI::currentScene() const
{
    if (mEditMode == OrthoObjectMode) return orthoView()->scene();
    return isoView()->scene();
}

Zoomable *BuildingDocumentGUI::currentZoomable() const
{
    return (mEditMode == OrthoObjectMode) ? orthoView()->zoomable()
                                          : isoView()->zoomable();
}

void BuildingDocumentGUI::activate()
{
    connect(currentView(), SIGNAL(mouseCoordinateChanged(QPoint)),
            mMainWindow, SLOT(mouseCoordinateChanged(QPoint)));
    connect(currentZoomable(), SIGNAL(scaleChanged(qreal)),
            mMainWindow, SLOT(updateActions()));

    currentZoomable()->connectToComboBox(mMainWindow->ui->editorScaleComboBox);

    connect(document(), SIGNAL(usedFurnitureChanged()),
            mMainWindow, SLOT(usedFurnitureChanged()));
    connect(document(), SIGNAL(usedTilesChanged()),
            mMainWindow, SLOT(usedTilesChanged()));

    connect(document(), SIGNAL(roomAdded(Room*)), mMainWindow, SLOT(roomAdded(Room*)));
    connect(document(), SIGNAL(roomRemoved(Room*)), mMainWindow, SLOT(roomRemoved(Room*)));
    connect(document(), SIGNAL(roomsReordered()), mMainWindow, SLOT(roomsReordered()));
    connect(document(), SIGNAL(roomChanged(Room*)), mMainWindow, SLOT(roomChanged(Room*)));

    connect(document(), SIGNAL(floorAdded(BuildingFloor*)),
            mMainWindow, SLOT(updateActions()));
    connect(document(), SIGNAL(floorRemoved(BuildingFloor*)),
            mMainWindow, SLOT(updateActions()));
    connect(document(), SIGNAL(currentFloorChanged()),
            mMainWindow, SLOT(updateActions()));
    connect(document(), SIGNAL(currentLayerChanged()),
            mMainWindow, SLOT(updateActions()));

    connect(document(), SIGNAL(currentRoomChanged()),
            mMainWindow, SLOT(currentRoomChanged()));

    connect(document(), SIGNAL(selectedObjectsChanged()),
            mMainWindow, SLOT(updateActions()));

    connect(document(), SIGNAL(tileSelectionChanged(QRegion)),
            mMainWindow, SLOT(updateActions()));
    connect(document(), SIGNAL(clipboardTilesChanged()),
            mMainWindow, SLOT(updateActions()));

    connect(document(), SIGNAL(cleanChanged()), mMainWindow, SLOT(updateWindowTitle()));
}

void BuildingDocumentGUI::deactivate()
{
    BaseTool *tool = ToolManager::instance()->currentTool();
    if (isTile())
        mPrevTileTool = tool;
    else
        mPrevObjectTool = tool;

    document()->disconnect(mMainWindow);
    orthoView()->disconnect(mMainWindow);
    isoView()->disconnect(mMainWindow);
    currentZoomable()->disconnect(mMainWindow);
}

void BuildingDocumentGUI::toOrthoObject()
{
    mIsoView->zoomable()->connectToComboBox(0);
    mOrthoView->zoomable()->connectToComboBox(mMainWindow->ui->editorScaleComboBox);
    stackedWidget()->setCurrentIndex(0);
    mOrthoView->setFocus();
    mEditMode = OrthoObjectMode;
    mPrevObjectMode = OrthoObjectMode;
}

void BuildingDocumentGUI::toIsoObject()
{
    mOrthoView->zoomable()->connectToComboBox(0);
    mIsoView->zoomable()->connectToComboBox(mMainWindow->ui->editorScaleComboBox);
    mIsoView->scene()->setEditingTiles(false);
    stackedWidget()->setCurrentIndex(1);
    mIsoView->setFocus();
    mEditMode = IsoObjectMode;
    mPrevObjectMode = IsoObjectMode;
}

void BuildingDocumentGUI::toObject()
{
    if (mPrevObjectMode == OrthoObjectMode)
        toOrthoObject();
    else
        toIsoObject();
}

void BuildingDocumentGUI::toTile()
{
    mOrthoView->zoomable()->connectToComboBox(0);
    mIsoView->zoomable()->connectToComboBox(mMainWindow->ui->editorScaleComboBox);
    mIsoView->scene()->setEditingTiles(true);
    stackedWidget()->setCurrentIndex(1);
    mIsoView->setFocus();
    mEditMode = TileMode;
}

void BuildingDocumentGUI::restoreTool()
{
    if (isTile() && mPrevTileTool && mPrevTileTool->action()->isEnabled())
        mPrevTileTool->makeCurrent();
    else if (isObject() && mPrevObjectTool && mPrevObjectTool->action()->isEnabled())
        mPrevObjectTool->makeCurrent();
}

void BuildingDocumentGUI::updateDocumentTab()
{
    int tabIndex = mMainWindow->docman()->indexOf(document());
    if (tabIndex == -1)
        return;

    QString tabText = document()->displayName();
    if (document()->isModified())
        tabText.prepend(QLatin1Char('*'));
    mMainWindow->ui->documentTabWidget->setTabText(tabIndex, tabText);

    QString tooltipText = QDir::toNativeSeparators(document()->fileName());
    mMainWindow->ui->documentTabWidget->setTabToolTip(tabIndex, tooltipText);
}

void BuildingDocumentGUI::autoSaveCheck()
{
    if (!document()->isModified()) {
        if (mAutoSaveTimer.isActive()) {
            mAutoSaveTimer.stop();
            qDebug() << "BuildingEd auto-save timer stopped (document is clean)";
        }
        removeAutoSaveFile();
        return;
    }
    if (mAutoSaveTimer.isActive())
        return;
    mAutoSaveTimer.start();
    qDebug() << "BuildingEd auto-save timer started";
}

void BuildingDocumentGUI::autoSaveTimeout()
{
    qDebug() << "BuildingEd auto-save timeout";
    QString fileName = document()->fileName();
    QString suffix = QLatin1String(".autosave"); // BuildingDocument::write() looks for this
    if (fileName.isEmpty()) {
        int n = 1;
        QString dir = BuildingPreferences::instance()->configPath();
        do {
            fileName = QString::fromLatin1("%1/untitled%2.tbx").arg(dir).arg(n);
            ++n;
        } while (QFileInfo(fileName + suffix).exists());
    }
    fileName += suffix;
    mMainWindow->writeBuilding(document(), fileName);
    mAutoSaveFileName = fileName;
    qDebug() << "BuildingEd auto-saved:" << fileName;
}

void BuildingDocumentGUI::removeAutoSaveFile()
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

/////

BuildingEditorWindow* BuildingEditorWindow::mInstance = 0;

BuildingEditorWindow::BuildingEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingEditorWindow),
    mCurrentDocument(0),
    mRoomComboBox(new QComboBox()),
    mUndoGroup(new QUndoGroup(this)),
    mCategoryZoomable(new Zoomable(this)),
    mCategory(0),
    mFurnitureGroup(0),
    mSynching(false),
    mInitialCategoryViewSelectionEvent(false),
    mUsedContextMenu(new QMenu(this)),
    mActionClearUsed(new QAction(this)),
    mTileModeToolBar(new TileModeToolBar(this)),
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

    ui->documentTabWidget->clear();
    ui->documentTabWidget->setDocumentMode(true);
    ui->documentTabWidget->setTabsClosable(true);

    connect(ui->documentTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentDocumentTabChanged(int)));
    connect(ui->documentTabWidget, SIGNAL(tabCloseRequested(int)),
            SLOT(documentTabCloseRequested(int)));

    connect(docman(), SIGNAL(documentAdded(BuildingDocument*)),
            SLOT(documentAdded(BuildingDocument*)));
    connect(docman(), SIGNAL(documentAboutToClose(int,BuildingDocument*)),
            SLOT(documentAboutToClose(int,BuildingDocument*)));
    connect(docman(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));

    ui->coordLabel->clear();

    connect(ui->actionToggleOrthoIso, SIGNAL(triggered()), SLOT(toggleOrthoIso()));

    PencilTool::instance()->setAction(ui->actionPecil);
    SelectMoveRoomsTool::instance()->setAction(ui->actionSelectRooms);
    DoorTool::instance()->setAction(ui->actionDoor);
    WallTool::instance()->setAction(ui->actionWall);
    WindowTool::instance()->setAction(ui->actionWindow);
    StairsTool::instance()->setAction(ui->actionStairs);
    FurnitureTool::instance()->setAction(ui->actionFurniture);
    RoofTool::instance()->setAction(ui->actionRoof);
    RoofCornerTool::instance()->setAction(ui->actionRoofCorner);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);

    /////
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
    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    keys += QKeySequence(tr("="));
    ui->actionZoomIn->setShortcuts(keys);
#if 0
    mOrthoView->addAction(ui->actionZoomIn);
    mIsoView->addAction(ui->actionZoomIn);
    ui->actionZoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#endif
    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);
#if 0
    mOrthoView->addAction(ui->actionZoomOut);
    mIsoView->addAction(ui->actionZoomOut);
    ui->actionZoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#endif
    keys.clear();
    keys += QKeySequence(tr("Ctrl+0"));
    keys += QKeySequence(tr("0"));
    ui->actionNormalSize->setShortcuts(keys);
#if 0
    mOrthoView->addAction(ui->actionNormalSize);
    mIsoView->addAction(ui->actionNormalSize);
    ui->actionNormalSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#endif
    /////
    mTileModeToolBar->mActionPencil->setIcon(
                QIcon(QLatin1String(":images/22x22/stock-tool-clone.png")));
    mTileModeToolBar->mActionSelectTiles->setIcon(ui->actionSelectRooms->icon());
    mTileModeToolBar->mActionPickTile->setIcon(
                QIcon(QLatin1String(":BuildingEditor/icons/icon_eyedrop.png")));
    mTileModeToolBar->addAction(ui->actionUpLevel);
    mTileModeToolBar->addAction(ui->actionDownLevel);
    addToolBar(mTileModeToolBar);
    connect(ui->actionSwitchEditMode, SIGNAL(triggered()), SLOT(toggleEditMode()));
    DrawTileTool::instance()->setAction(mTileModeToolBar->mActionPencil);
    SelectTileTool::instance()->setAction(mTileModeToolBar->mActionSelectTiles);
    PickTileTool::instance()->setAction(mTileModeToolBar->mActionPickTile);
    connect(PickTileTool::instance(), SIGNAL(tilePicked(QString)),
            SIGNAL(tilePicked(QString)));
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

    connect(ui->actionBuildingProperties, SIGNAL(triggered()),
            SLOT(buildingPropertiesDialog()));
    connect(ui->actionGrime, SIGNAL(triggered()),
            SLOT(buildingGrime()));
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

    readSettings();

    // Restore visibility due to mode-switching.
    // readSettings() calls QWidget::restoreGeometry() which might change
    // widget visibility.
    ui->toolBar->show();
    ui->dockWidget->show();
    mTileModeToolBar->hide();
    mFurnitureDock->hide();
    mLayersDock->hide();
    mTilesetDock->hide();

    updateActions();
}

BuildingEditorWindow::~BuildingEditorWindow()
{
#if 1
    BuildingTilesDialog::deleteInstance();
    ToolManager::deleteInstance();
#else
    BuildingTemplates::deleteInstance();
    BuildingTilesDialog::deleteInstance();
    BuildingTilesMgr::deleteInstance(); // Ensure all the tilesets are released
    BuildingTMX::deleteInstance();
    BuildingPreferences::deleteInstance();
    ToolManager::deleteInstance();
#endif
    delete ui;
}

void BuildingEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmAllSave()) {
        writeSettings();
        docman()->closeAllDocuments();
        event->accept(); // doesn't destroy us
    } else
        event->ignore();

}

bool BuildingEditorWindow::openFile(const QString &fileName)
{
    // Select existing document if this file is already open
    int documentIndex = docman()->findDocument(fileName);
    if (documentIndex != -1) {
        ui->documentTabWidget->setCurrentIndex(documentIndex);
        return true;
    }

    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        BuildingDocumentMgr::instance()->addDocument(doc);
        return true;
    }

    QMessageBox::warning(this, tr("Error reading building"), error);
    return false;
}

bool BuildingEditorWindow::confirmAllSave()
{
    foreach (BuildingDocument *doc, docman()->documents()) {
        if (!doc->isModified())
            continue;
        docman()->setCurrentDocument(doc);
        if (!confirmSave())
            return false;
    }
    return true;
}

// Called by Tiled::Internal::MainWindow::closeEvent
bool BuildingEditorWindow::closeYerself()
{
    if (confirmAllSave()) {
        writeSettings();
        docman()->closeAllDocuments();
        delete this;
        return true;
    }
    return false;
}

bool BuildingEditorWindow::Startup()
{
    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        if (ts->isMissing()) {
            PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
            TileMetaInfoMgr::instance()->loadTilesets();
            break;
        }
    }

    /////

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
    // config files.
    connect(BuildingTilesDialog::instance(), SIGNAL(edited()),
            SLOT(tilesDialogEdited()));

    return true;
}

void BuildingEditorWindow::setCurrentRoom(Room *room) const
{
    mCurrentDocument->setCurrentRoom(room);
}

Room *BuildingEditorWindow::currentRoom() const
{
    return mCurrentDocument ? mCurrentDocument->currentRoom() : 0;
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

void BuildingEditorWindow::selectAndDisplay(BuildingTileEntry *entry)
{
    if (!entry)
        return;
    ui->categoryList->setCurrentRow(0); // Used Tiles
    QModelIndex index = ui->tilesetView->model()->index((void*)entry);
    ui->tilesetView->setCurrentIndex(index);
    QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                              Q_ARG(int, 0), Q_ARG(QModelIndex, index));
//    ui->tilesetView->scrollTo(index);
}

void BuildingEditorWindow::selectAndDisplay(FurnitureTile *ftile)
{
    if (!ftile)
        return;
    ui->categoryList->setCurrentRow(1); // Used Furniture
    QModelIndex index = ui->furnitureView->model()->index(ftile);
    ui->furnitureView->setCurrentIndex(index);
    QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                              Q_ARG(int, 1), Q_ARG(QModelIndex, index));
    //    ui->furnitureView->scrollTo(index);
}

BuildingDocumentMgr *BuildingEditorWindow::docman() const
{
    return BuildingDocumentMgr::instance();
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
#if 0
    mOrthoView->zoomable()->setScale(mSettings.value(QLatin1String("EditorScale"),
                                                1.0).toReal());
    mIsoView->zoomable()->setScale(mSettings.value(QLatin1String("IsoEditorScale"),
                                                1.0).toReal());
    QString orient = mSettings.value(QLatin1String("Orientation"),
                                     QLatin1String("isometric")).toString();
    if (orient == QLatin1String("isometric"))
        toggleOrthoIso();
#endif
    mSettings.endGroup();

    mCategoryZoomable->setScale(BuildingPreferences::instance()->tileScale());

    restoreSplitterSizes(ui->categorySplitter);
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
#if 0
    mSettings.setValue(QLatin1String("EditorScale"), mOrthoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("IsoEditorScale"), mIsoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("Orientation"),
                       (mOrient == OrientOrtho) ? QLatin1String("orthogonal")
                                                : QLatin1String("isometric"));
#endif
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

    index = currentBuilding()->indexOf(currentRoom);
    if (index != -1)
        mRoomComboBox->setCurrentIndex(index);
}

void BuildingEditorWindow::roomIndexChanged(int index)
{
    if (currentDocument())
        currentDocument()->setCurrentRoom((index == -1) ? 0 : currentBuilding()->room(index));
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

    ui->tilesetView->clear();
    ui->furnitureView->clear();

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
#if 1
            ui->tilesetView->setEntries(entryMap.values(), true);
#else
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
            ui->tilesetView->setTiles(tiles, userData, headers);
#endif
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
            ui->furnitureView->setTiles(furnitureMap.values());
            ui->furnitureView->scrollToTop();
            ui->categoryStack->setCurrentIndex(1);

            connect(mActionClearUsed, SIGNAL(triggered()), SLOT(resetUsedFurniture()));
            ui->furnitureView->setContextMenu(mUsedContextMenu);
        } else if ((mCategory = categoryAt(row))) {
#if 1
            QList<BuildingTileEntry*> entries;
            if (mCategory->canAssignNone()) {
                entries += BuildingTilesMgr::instance()->noneTileEntry();
            }
            QMap<QString,BuildingTileEntry*> entryMap;
            int i = 0;
            foreach (BuildingTileEntry *entry, mCategory->entries()) {
                QString key = entry->displayTile()->name() + QString::number(i++);
                entryMap[key] = entry;
            }
            entries += entryMap.values();
            ui->tilesetView->setEntries(entries);
#else
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
            ui->tilesetView->setTiles(tiles, userData, headers);
#endif
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            selectCurrentCategoryTile();
        } else if ((mFurnitureGroup = furnitureGroupAt(row))) {
            ui->furnitureView->setTiles(mFurnitureGroup->mTiles);
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
#if 1
        if (BuildingTileEntry *entry = ui->tilesetView->entry(index)) {
#else
        if (ui->tilesetView->model()->tileAt(index)) {
            BuildingTileEntry *entry = static_cast<BuildingTileEntry*>(
                        ui->tilesetView->model()->userDataAt(index));
#endif
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

void BuildingEditorWindow::scrollToNow(int which, const QModelIndex &index)
{
    if (which == 0)
        ui->tilesetView->scrollTo(index);
    else
        ui->furnitureView->scrollTo(index);
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

    mCurrentDocument->undoStack()->push(
                new ChangeBuildingTile(mCurrentDocument,
                                       Building::ExteriorWall, entry,
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
#if 1
        mSynching = true;
        QModelIndex index = ui->tilesetView->index(currentTile);
        ui->tilesetView->setCurrentIndex(index);
        mSynching = false;
#else
        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(currentTile->displayTile())) {
            QModelIndex index = ui->tilesetView->model()->index(tile);
            mSynching = true;
            ui->tilesetView->setCurrentIndex(index);
            mSynching = false;
        }
#endif
    }
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
    NewBuildingDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Building *building = new Building(dialog.buildingWidth(),
                                      dialog.buildingHeight(),
                                      dialog.buildingTemplate());
    building->insertFloor(0, new BuildingFloor(building, 0));

    BuildingDocument *doc = new BuildingDocument(building, QString());

    docman()->addDocument(doc);
}

void BuildingEditorWindow::openBuilding()
{
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
        docman()->addDocument(doc);
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

void BuildingEditorWindow::documentAdded(BuildingDocument *doc)
{
    Building *building = doc->building();

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

    doc->setCurrentFloor(building->floor(0));
    doc->setCurrentRoom(building->roomCount() ? building->room(0) : 0);
    mUndoGroup->addStack(doc->undoStack());

    BuildingDocumentGUI *gui = new BuildingDocumentGUI(doc);
    mDocumentGUI[doc] = gui;

    int docIndex = docman()->indexOf(doc);
    ui->documentTabWidget->insertTab(docIndex, gui->stackedWidget(),
                                     doc->displayName());
    ui->documentTabWidget->setTabToolTip(docIndex, QDir::toNativeSeparators(doc->fileName()));

    reportMissingTilesets();
#if 0
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

    updateWindowTitle();

    reportMissingTilesets();
#endif
}

void BuildingEditorWindow::documentAboutToClose(int index, BuildingDocument *doc)
{
    Q_UNUSED(doc)

    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
    ui->documentTabWidget->removeTab(index);
}

void BuildingEditorWindow::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        mCurrentDocumentGUI->deactivate();
    }

    mCurrentDocument = doc;
    mCurrentDocumentGUI = doc ? mDocumentGUI[doc] : 0;

    if (mCurrentDocument) {
        mUndoGroup->setActiveStack(mCurrentDocument->undoStack());
        mCurrentDocumentGUI->activate();
        mLayersDock->setDocument(mCurrentDocument);
        mTilesetDock->setDocument(mCurrentDocument);
        setEditMode();
        ui->documentTabWidget->setCurrentIndex(docman()->indexOf(doc));
    } else {
        ToolManager::instance()->clearDocument();
        mRoomComboBox->clear();
        mLayersDock->clearDocument();
        mTilesetDock->clearDocument();
    }

    if (ui->categoryList->currentRow() < 2)
        categorySelectionChanged();

    updateRoomComboBox();
    resizeCoordsLabel();
    updateActions();
    updateWindowTitle();
}

void BuildingEditorWindow::currentDocumentTabChanged(int index)
{
    docman()->setCurrentDocument(index);
}

void BuildingEditorWindow::documentTabCloseRequested(int index)
{
    BuildingDocument *doc = docman()->documentAt(index);
    if (doc->isModified()) {
        docman()->setCurrentDocument(index);
        if (!confirmSave())
            return;
    }
    docman()->closeDocument(index);
}

#if 0
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
#endif

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

    if (mCurrentDocumentGUI->isTile()) {
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

    if (mCurrentDocumentGUI->isTile()) {
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

    if (mCurrentDocumentGUI->isTile()) {
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
    if (mCurrentDocumentGUI->isTile()) {
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
        return;
    }
    deleteObjects();
}

void BuildingEditorWindow::selectAll()
{
    if (!mCurrentDocument)
        return;
    if (mCurrentDocumentGUI->isTile()) {
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
    if (mCurrentDocumentGUI->isTile()) {
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

void BuildingEditorWindow::buildingPropertiesDialog()
{
    if (!mCurrentDocument)
        return;

    BuildingPropertiesDialog dialog(mCurrentDocument, this);
    dialog.exec();
}

void BuildingEditorWindow::buildingGrime()
{
    if (!mCurrentDocument)
        return;

    ChooseBuildingTileDialog dialog(tr("Choose Building Grime Tile"),
                                    BuildingTilesMgr::instance()->catGrimeWall(),
                                    mCurrentDocument->building()->tile(Building::GrimeWall),
                                    this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    mCurrentDocument->undoStack()->push(
                new ChangeBuildingTile(mCurrentDocument,
                                       Building::GrimeWall,
                                       dialog.selectedTile(),
                                       false));
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

void BuildingEditorWindow::currentRoomChanged()
{
    if (Room *room = currentRoom()) {
        int roomIndex = mCurrentDocument->building()->indexOf(room);
        this->mRoomComboBox->setCurrentIndex(roomIndex);
    } else
        this->mRoomComboBox->setCurrentIndex(-1);
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
    mCurrentDocumentGUI->currentZoomable()->zoomIn();
}

void BuildingEditorWindow::zoomOut()
{
    mCurrentDocumentGUI->currentZoomable()->zoomOut();
}

void BuildingEditorWindow::resetZoom()
{
    mCurrentDocumentGUI->currentZoomable()->resetZoom();
}

void BuildingEditorWindow::showObjectsChanged(bool show)
{
    Q_UNUSED(show)
    mCurrentDocumentGUI->orthoView()->scene()->synchObjectItemVisibility();
    mCurrentDocumentGUI->isoView()->scene()->synchObjectItemVisibility();
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
#if 0
    categorySelectionChanged();
#endif
}

void BuildingEditorWindow::tilesetAboutToBeRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    ui->tilesetView->clear();
    // FurnitureView doesn't cache Tiled::Tiles
#endif
}

void BuildingEditorWindow::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    categorySelectionChanged();
#endif
}

void BuildingEditorWindow::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    categorySelectionChanged();
#endif
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
    bool objectMode = hasDoc && mCurrentDocumentGUI->isObject();

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
    PickTileTool::instance()->setEnabled(!objectMode && mCurrentDocument);

    ui->actionUpLevel->setEnabled(hasDoc &&
                                  !mCurrentDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(hasDoc &&
                                    !mCurrentDocument->currentFloorIsBottom());

//    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionExportTMX->setEnabled(hasDoc);

    ui->actionShowObjects->setEnabled(hasDoc);

    Zoomable *zoomable = hasDoc ? mCurrentDocumentGUI->currentZoomable() : 0;
    ui->actionZoomIn->setEnabled(hasDoc && zoomable->canZoomIn());
    ui->actionZoomOut->setEnabled(hasDoc && zoomable->canZoomOut());
    ui->actionNormalSize->setEnabled(hasDoc && zoomable->scale() != 1.0);

    ui->actionBuildingProperties->setEnabled(hasDoc);
    ui->actionGrime->setEnabled(hasDoc);
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
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/BuildingEd/index.html"));
    QDesktopServices::openUrl(url);
}

// Switches between tile and object modes, hiding/showing the toolbar & docks
// as needed.
// FIXME: a lot of flashing
void BuildingEditorWindow::setEditMode()
{
    if (!mCurrentDocument)
        return;

//    if (mCurrentDocumentGUI->editMode() == mEditMode)
//        return;

//    ToolManager::instance()->clearDocument();
    BuildingBaseScene *editor = mCurrentDocumentGUI->currentScene();
    ToolManager::instance()->setEditor(editor);

    switch (mCurrentDocumentGUI->editMode())
    {
    case BuildingDocumentGUI::OrthoObjectMode: {
        if (mEditMode == BuildingDocumentGUI::TileMode) {
            // Switch from Tile to OrthoObject
            mCurrentDocument->setTileSelection(QRegion());
            ui->toolBar->show();
            ui->dockWidget->show();
            mTileModeToolBar->hide();
            mLayersDock->hide();
            mTilesetDock->hide();
            mFurnitureDock->hide();
        }
        break;
    }
    case BuildingDocumentGUI::IsoObjectMode: {
        if (mEditMode == BuildingDocumentGUI::TileMode) {
            // Switch from Tile to IsoObject
            mCurrentDocument->setTileSelection(QRegion());
            ui->toolBar->show();
            ui->dockWidget->show();
            mTileModeToolBar->hide();
            mLayersDock->hide();
            mTilesetDock->hide();
            mFurnitureDock->hide();
        }
        break;
    }
    case BuildingDocumentGUI::TileMode: {
        // Switch from OrthoObject/IsoObject to Tile
        mTileModeToolBar->show();
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
        break;
    }
    }

    mEditMode = mCurrentDocumentGUI->editMode();

    updateActions();

    mCurrentDocumentGUI->restoreTool();
}

void BuildingEditorWindow::toggleOrthoIso()
{
    if (!mCurrentDocument)
        return;

    if (mCurrentDocumentGUI->isTile()) {
        toggleEditMode();
        return;
    }

#if 1
    if (mCurrentDocumentGUI->isOrthoObject())
        mCurrentDocumentGUI->toIsoObject();
    else
        mCurrentDocumentGUI->toOrthoObject();

    setEditMode();
#else
    BuildingBaseScene *editor;
    if (mCurrentDocumentGUI->isOrthoObject()) {
        mCurrentDocumentGUI->toIsoObject();
        editor = mCurrentDocumentGUI->isoView()->scene();
    } else {
        mCurrentDocumentGUI->toOrthoObject();
        editor = mCurrentDocumentGUI->orthoView()->scene();
    }

    // Deactivate the current tool so it removes any cursor item from the
    // scene.
    if (ToolManager::instance()->currentTool())
        ToolManager::instance()->activateTool(0);

    ToolManager::instance()->setEditor(editor);

    // Restore the current tool so it adds any cursor item back to the scene.
    mCurrentDocumentGUI->restoreTool();
#endif
}

void BuildingEditorWindow::toggleEditMode()
{
    if (!mCurrentDocument)
        return;
#if 1
    if (mCurrentDocumentGUI->isTile())
        mCurrentDocumentGUI->toObject();
    else
        mCurrentDocumentGUI->toTile();

    setEditMode();
#else
    ToolManager::instance()->clearDocument();

    if (mCurrentDocumentGUI->isObject()) {
        // Switch to TileMode
        mTileModeToolBar->show();
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

        mCurrentDocumentGUI->toTile();
    } else {
        // Switch to ObjectMode
        if (mCurrentDocument)
            mCurrentDocument->setTileSelection(QRegion());
        ui->toolBar->show();
        ui->dockWidget->show();
        mTileModeToolBar->hide();
        mLayersDock->hide();
        mTilesetDock->hide();
        mFurnitureDock->hide();

        mCurrentDocumentGUI->toObject();
    }

    BuildingBaseScene *editor = mCurrentDocumentGUI->currentScene();
    ToolManager::instance()->setEditor(editor);

    updateActions();

    mCurrentDocumentGUI->restoreTool();
#endif
}
