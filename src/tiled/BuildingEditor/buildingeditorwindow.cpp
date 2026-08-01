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

#include "attributeeditmode.h"
#include "building.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingfloor.h"
#include "buildingfloorsdialog.h"
#include "buildingkeyvaluesdialog.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreferencesdialog.h"
#include "buildingpropertiesdialog.h"
#include "buildingreader.h"
#include "buildingundoredo.h"
#include "buildingisoview.h"
#include "buildingorthoview.h"
#include "buildingtemplates.h"
#include "buildingtemplatesdialog.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"
#include "buildingtiletools.h"
#include "buildingtmx.h"
#include "buildingtools.h"
#include "choosebuildingtiledialog.h"
#include "furnituregroups.h"
#include "listofstringsdialog.h"
#include "newbuildingdialog.h"
#include "objecteditmode.h"
#include "resizedialog.h"
#include "roomsdialog.h"
#include "templatefrombuildingdialog.h"
#include "tileeditmode.h"
#include "tiledeffile.h"
#include "welcomemode.h"

#include "fancytabwidget.h"
#include "utils/stylehelper.h"

#include "shortcut/actionmanager.h"
#include "shortcut/keyboardshortcutwindow.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"
#include "zprogress.h"

#include "maplevel.h"
#include "maprenderer.h"
#include "tile.h"
#include "tileset.h"

#include <QBitmap>
#include <QClipboard>
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
#include <QSplitter>
//#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUrl>
#include <QXmlStreamReader>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////

EditorWindowPerDocumentStuff::EditorWindowPerDocumentStuff(BuildingDocument *doc) :
    QObject(doc),
    mMainWindow(BuildingEditorWindow::instance()),
    mDocument(doc),
    mEditMode(IsoObjectMode),
    mPrevObjectTool(PencilTool::instance()),
    mPrevTileTool(DrawTileTool::instance()),
    mPrevAttributeTool(SelectTileTool::instance()),
    mMissingTilesetsReported(false),
    mIsoView(nullptr),
    mTileView(nullptr),
    mAttributeView(nullptr),
    mAutoSaveTimer(this)
{
    connect(document()->undoStack(), &QUndoStack::cleanChanged, this, &EditorWindowPerDocumentStuff::autoSaveCheck);
    connect(document()->undoStack(), &QUndoStack::indexChanged, this, &EditorWindowPerDocumentStuff::autoSaveCheck);

    mAutoSaveTimer.setSingleShot(true);
    mAutoSaveTimer.setInterval(2.5 * 60 * 1000); // 2.5 minutes
    connect(&mAutoSaveTimer, &QTimer::timeout, this, &EditorWindowPerDocumentStuff::autoSaveTimeout);
}

EditorWindowPerDocumentStuff::~EditorWindowPerDocumentStuff()
{
    removeAutoSaveFile();
}

void EditorWindowPerDocumentStuff::activate()
{
//    connect(currentZoomable(), SIGNAL(scaleChanged(qreal)),
//            mMainWindow, SLOT(updateActions()));

}

void EditorWindowPerDocumentStuff::deactivate()
{
    document()->disconnect(mMainWindow);
}

void EditorWindowPerDocumentStuff::toOrthoObject()
{
    if (mEditMode == EditMode::IsoObjectMode) {
        mIsoViewsCenter = mIsoView->mapToScene(mTileView->viewport()->rect().center());
        mIsoViewsZoom = mIsoView->zoomable()->scale();
    }
    if (mEditMode == EditMode::TileMode) {
        mIsoViewsCenter = mTileView->mapToScene(mTileView->viewport()->rect().center());
        mIsoViewsZoom = mTileView->zoomable()->scale();
    }
    if (mEditMode == EditMode::AttributeMode) {
        mIsoViewsCenter = mAttributeView->mapToScene(mAttributeView->viewport()->rect().center());
        mIsoViewsZoom = mAttributeView->zoomable()->scale();
    }
    mEditMode = EditMode::OrthoObjectMode;
    mPrevObjectMode = EditMode::OrthoObjectMode;
}

void EditorWindowPerDocumentStuff::toIsoObject()
{
    if (mEditMode == EditMode::TileMode) {
        mIsoViewsCenter = mTileView->mapToScene(mTileView->viewport()->rect().center());
        mIsoViewsZoom = mTileView->zoomable()->scale();
    }
    if (mEditMode == EditMode::AttributeMode) {
        mIsoViewsCenter = mAttributeView->mapToScene(mAttributeView->viewport()->rect().center());
        mIsoViewsZoom = mAttributeView->zoomable()->scale();
    }
    mIsoView->centerOn(mIsoViewsCenter);
    mIsoView->zoomable()->setScale(mIsoViewsZoom);
    mEditMode = EditMode::IsoObjectMode;
    mPrevObjectMode = EditMode::IsoObjectMode;
}

void EditorWindowPerDocumentStuff::toObject()
{
    if (mPrevObjectMode == EditMode::OrthoObjectMode)
        toOrthoObject();
    else
        toIsoObject();
}

void EditorWindowPerDocumentStuff::toTile()
{
    if (mEditMode == EditMode::IsoObjectMode) {
        mIsoViewsCenter = mIsoView->mapToScene(mIsoView->viewport()->rect().center());
        mIsoViewsZoom = mIsoView->zoomable()->scale();
    }
    if (mEditMode == EditMode::AttributeMode) {
        mIsoViewsCenter = mAttributeView->mapToScene(mAttributeView->viewport()->rect().center());
        mIsoViewsZoom = mAttributeView->zoomable()->scale();
    }
    mTileView->centerOn(mIsoViewsCenter);
    mTileView->zoomable()->setScale(mIsoViewsZoom);
    mEditMode = EditMode::TileMode;
}

void EditorWindowPerDocumentStuff::toAttribute()
{
    if (mEditMode == EditMode::IsoObjectMode) {
        mIsoViewsCenter = mIsoView->mapToScene(mIsoView->viewport()->rect().center());
        mIsoViewsZoom = mIsoView->zoomable()->scale();
    }
    if (mEditMode == EditMode::TileMode) {
        mIsoViewsCenter = mTileView->mapToScene(mTileView->viewport()->rect().center());
        mIsoViewsZoom = mTileView->zoomable()->scale();
    }
    mAttributeView->centerOn(mIsoViewsCenter);
    mAttributeView->zoomable()->setScale(mIsoViewsZoom);
    mEditMode = EditMode::AttributeMode;
}

void EditorWindowPerDocumentStuff::rememberTool()
{
    if (isAttribute())
        mPrevAttributeTool = ToolManager::instance()->currentTool();
    else if (isTile())
        mPrevTileTool = ToolManager::instance()->currentTool();
    else if (isObject())
        mPrevObjectTool = ToolManager::instance()->currentTool();
}

void EditorWindowPerDocumentStuff::restoreTool()
{
    if (isAttribute() && mPrevAttributeTool && mPrevAttributeTool->action()->isEnabled())
        mPrevAttributeTool->makeCurrent();
    else if (isTile() && mPrevTileTool && mPrevTileTool->action()->isEnabled())
        mPrevTileTool->makeCurrent();
    else if (isObject() && mPrevObjectTool && mPrevObjectTool->action()->isEnabled())
        mPrevObjectTool->makeCurrent();
}

void EditorWindowPerDocumentStuff::viewAddedForDocument(BuildingIsoView *view)
{
    if (view->scene()->editingAttributes())
        mAttributeView = view;
    else if (view->scene()->editingTiles())
        mTileView = view;
    else
        mIsoView = view;
}

void EditorWindowPerDocumentStuff::focusOn(int x, int y, int z, int objectIndex)
{
    toIsoObject();
    BuildingFloor *floor = document()->building()->floor(z);
    if (document()->currentFloor() != floor)
        document()->setCurrentFloor(floor);
    mIsoView->centerOn(mIsoView->scene()->tileToScene(QPoint(x, y), z));
    mIsoView->zoomable()->setScale(2);

    if (objectIndex >= 0 && objectIndex < floor->objectCount()) {
        BuildingObject *bo = floor->object(objectIndex);
        document()->setSelectedObjects(QSet<BuildingObject*>() << bo);
    }
}

void EditorWindowPerDocumentStuff::setInitialPosition()
{
    if (mInitialPositionSet) {
        return;
    }
    mInitialPositionSet = true;
    if (mIsoView != nullptr) {
        mIsoView->centerOn(mIsoView->scene()->mapRenderer()->tileToPixelCoords(mDocument->building()->bounds().center()));
    }
}

void EditorWindowPerDocumentStuff::autoSaveCheck()
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

void EditorWindowPerDocumentStuff::autoSaveTimeout()
{
    qDebug() << "BuildingEd auto-save timeout";
    if (mAutoSaveFileName.isEmpty()) {
        QString fileName = document()->fileName();
        QString suffix = QLatin1String(".autosave"); // BuildingDocument::write() looks for this
        if (fileName.isEmpty()) {
            int n = 1;
            QString dir = BuildingPreferences::instance()->configPath();
            do {
                fileName = QString::fromLatin1("%1/untitled%2.tbx").arg(dir).arg(n);
                ++n;
            } while (QFileInfo::exists(fileName + suffix));
        }
        fileName += suffix;
        mAutoSaveFileName = fileName;
    }
    mMainWindow->writeBuilding(document(), mAutoSaveFileName);
    qDebug() << "BuildingEd auto-saved:" << mAutoSaveFileName;
}

void EditorWindowPerDocumentStuff::removeAutoSaveFile()
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
    mCurrentDocumentStuff(0),
    mUndoGroup(new QUndoGroup(this)),
    mSettings(BuildingPreferences::instance()->settings()),
    mSynching(false),
    mOrthoObjectEditMode(0),
    mIsoObjectEditMode(0),
    mTileEditMode(0),
    mAttributeEditMode(nullptr),
    mDocumentChanging(false)
{
    ui->setupUi(this);

    mInstance = this;

    BuildingPreferences *prefs = BuildingPreferences::instance();

    connect(docman(), &BuildingDocumentMgr::documentAdded,
            this, &BuildingEditorWindow::documentAdded);
    connect(docman(), &BuildingDocumentMgr::documentAboutToClose,
            this, &BuildingEditorWindow::documentAboutToClose);
    connect(docman(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &BuildingEditorWindow::currentDocumentChanged);

    PencilTool::instance()->setAction(ui->actionPencil);
    SelectMoveRoomsTool::instance()->setAction(ui->actionSelectRooms);
    DoorTool::instance()->setAction(ui->actionDoor);
    WallTool::instance()->setAction(ui->actionWall);
    WindowTool::instance()->setAction(ui->actionWindow);
    StairsTool::instance()->setAction(ui->actionStairs);
    FurnitureTool::instance()->setAction(ui->actionFurniture);
    RoofTool::instance()->setAction(ui->actionRoof);
    RoofShallowTool::instance()->setAction(ui->actionRoofShallow);
    RoofSlope30Tool::instance()->setAction(ui->actionRoof30Degree);
    RoofCornerTool::instance()->setAction(ui->actionRoofCorner);
    RoofCornerSlope30Tool::instance()->setAction(ui->actionRoofCorner30Degree);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);
    BasementAccessTool::instance()->setAction(ui->actionBasementAccessTool);

    DrawTileTool::instance()->setAction(ui->actionDrawTiles);
    SelectTileTool::instance()->setAction(ui->actionSelectTiles);
    PickTileTool::instance()->setAction(ui->actionPickTiles);
    FloorGrimeTileTool::instance()->setAction(ui->actionFloorGrime);

    connect(PickTileTool::instance(), &PickTileTool::tilePicked,
            this, &BuildingEditorWindow::tilePicked);

    connect(ToolManager::instance(), &ToolManager::currentEditorChanged,
            this, &BuildingEditorWindow::currentEditorChanged);

    connect(ui->actionUpLevel, &QAction::triggered,
            this, &BuildingEditorWindow::upLevel);
    connect(ui->actionDownLevel, &QAction::triggered,
            this, &BuildingEditorWindow::downLevel);

    mUndoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    mRedoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    mUndoAction->setShortcuts(QKeySequence::Undo);
    mRedoAction->setShortcuts(QKeySequence::Redo);
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    undoIcon.addFile(QLatin1String(":images/24x24/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    redoIcon.addFile(QLatin1String(":images/24x24/edit-redo.png"));
    mUndoAction->setIcon(undoIcon);
    mRedoAction->setIcon(redoIcon);
    Tiled::Utils::setThemeIcon(mUndoAction, "edit-undo");
    Tiled::Utils::setThemeIcon(mRedoAction, "edit-redo");
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(0), mUndoAction);
    ui->menuEdit->insertAction(ui->menuEdit->actions().at(1), mRedoAction);
    ui->menuEdit->insertSeparator(ui->menuEdit->actions().at(2));

    QIcon newIcon = ui->actionNewBuilding->icon();
    QIcon openIcon = ui->actionOpen->icon();
    QIcon saveIcon = ui->actionSave->icon();
    newIcon.addFile(QLatin1String(":images/24x24/document-new.png"));
    openIcon.addFile(QLatin1String(":images/24x24/document-open.png"));
    saveIcon.addFile(QLatin1String(":images/24x24/document-save.png"));
    ui->actionNewBuilding->setIcon(newIcon);
    ui->actionOpen->setIcon(openIcon);
    ui->actionSave->setIcon(saveIcon);

    ui->actionExportNewBinary->setVisible(true);

    ui->actionCut->setShortcuts(QKeySequence::Cut);
    ui->actionCopy->setShortcuts(QKeySequence::Copy);
    ui->actionPaste->setShortcuts(QKeySequence::Paste);
    ui->actionDelete->setShortcuts(QKeySequence::Delete);
    QList<QKeySequence> keys1;
    keys1 += QKeySequence(Qt::CTRL | Qt::Key_Delete);
    ui->actionDeleteInAllLayers->setShortcuts(keys1);
    ui->actionSelectAll->setShortcuts(QKeySequence::SelectAll);

    ui->actionSelectNone->setShortcut(tr("Ctrl+Shift+A"));
    connect(ui->actionCut, &QAction::triggered, this, &BuildingEditorWindow::editCut);
    connect(ui->actionCopy, &QAction::triggered, this, &BuildingEditorWindow::editCopy);
    connect(ui->actionPaste, &QAction::triggered, this, &BuildingEditorWindow::editPaste);
    connect(ui->actionDelete, &QAction::triggered, this, &BuildingEditorWindow::editDelete);
    connect(ui->actionDeleteInAllLayers, &QAction::triggered, this, &BuildingEditorWindow::editDeleteInAllLayers);
    connect(ui->actionSelectAll, &QAction::triggered, this, &BuildingEditorWindow::selectAll);
    connect(ui->actionSelectNone, &QAction::triggered, this, &BuildingEditorWindow::selectNone);

    connect(mUndoGroup, &QUndoGroup::cleanChanged, this, &BuildingEditorWindow::updateWindowTitle);

    connect(ui->actionPreferences, &QAction::triggered, this, &BuildingEditorWindow::preferences);
    connect(ui->actionKeyboardShortcuts, &QAction::triggered, this, &BuildingEditorWindow::keyboardShortcuts);

    connect(ui->actionNewBuilding, &QAction::triggered, this, &BuildingEditorWindow::newBuilding);
    connect(ui->actionOpen, &QAction::triggered, this, &BuildingEditorWindow::openBuilding);
    connect(ui->actionSave, &QAction::triggered, this, &BuildingEditorWindow::saveBuilding);
    connect(ui->actionSaveAs, &QAction::triggered, this, &BuildingEditorWindow::saveBuildingAs);

    connect(ui->actionExportTMX, &QAction::triggered, this, &BuildingEditorWindow::exportTMX);
    ui->actionExportTMX->setVisible(false);
    connect(ui->actionExportNewBinary, &QAction::triggered, this, &BuildingEditorWindow::exportNewBinary);

    ui->actionNewBuilding->setShortcuts(QKeySequence::New);
    ui->actionOpen->setShortcuts(QKeySequence::Open);
    ui->actionSave->setShortcuts(QKeySequence::Save);
    ui->actionSaveAs->setShortcuts(QKeySequence::SaveAs);

    connect(ui->actionClose, &QAction::triggered, this, &QWidget::close);
    setAttribute(Qt::WA_DeleteOnClose, false);

    ui->actionShowGrid->setChecked(prefs->showGrid());
    connect(ui->actionShowGrid, &QAction::toggled,
            prefs, &BuildingPreferences::setShowGrid);
    connect(prefs, &BuildingPreferences::showGridChanged,
            ui->actionShowGrid, &QAction::setChecked);

    ui->actionHighlightFloor->setChecked(prefs->highlightFloor());
    connect(ui->actionHighlightFloor, &QAction::toggled,
            prefs, &BuildingPreferences::setHighlightFloor);
    connect(prefs, &BuildingPreferences::highlightFloorChanged,
            ui->actionHighlightFloor, &QAction::setChecked);

    ui->actionHighlightRoom->setChecked(prefs->highlightRoom());
    connect(ui->actionHighlightRoom, &QAction::toggled,
            prefs, &BuildingPreferences::setHighlightRoom);
    connect(prefs, &BuildingPreferences::highlightRoomChanged,
            ui->actionHighlightRoom, &QAction::setChecked);

    ui->actionShowLowerFloors->setChecked(prefs->showLowerFloors());
    connect(ui->actionShowLowerFloors, &QAction::toggled,
            prefs, &BuildingPreferences::setShowLowerFloors);
    connect(prefs, &BuildingPreferences::showLowerFloorsChanged,
            ui->actionShowLowerFloors, &QAction::setChecked);

    ui->actionShowOnlyFloors->setChecked(prefs->showOnlyFloors());
    connect(ui->actionShowOnlyFloors, &QAction::toggled,
            prefs, &BuildingPreferences::setShowOnlyFloors);
    connect(prefs, &BuildingPreferences::showOnlyFloorsChanged,
            ui->actionShowOnlyFloors, &QAction::setChecked);

    ui->actionShowObjects->setChecked(prefs->showObjects());
    connect(ui->actionShowObjects, &QAction::toggled,
            prefs, &BuildingPreferences::setShowObjects);
    connect(prefs, &BuildingPreferences::showObjectsChanged,
            this, &BuildingEditorWindow::showObjectsChanged);

    ui->actionHighlightUnlitRooms->setChecked(prefs->highlightUnlitRooms());
    connect(ui->actionHighlightUnlitRooms, &QAction::toggled,
            prefs, &BuildingPreferences::setHighlightUnlitRooms);
    connect(prefs, &BuildingPreferences::highlightUnlitRoomsChanged,
            this, &BuildingEditorWindow::highlightUnlitRoomsChanged);

    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    keys += QKeySequence(tr("="));
    ui->actionZoomIn->setShortcuts(keys);

    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);

    keys.clear();
    keys += QKeySequence(tr("Ctrl+0"));
    keys += QKeySequence(tr("0"));
    ui->actionNormalSize->setShortcuts(keys);

    connect(ui->actionCropToMinimum, &QAction::triggered, this, &BuildingEditorWindow::cropToMinimum);
    connect(ui->actionCropToSelection, &QAction::triggered, this, &BuildingEditorWindow::cropToSelection);
    connect(ui->actionResize, &QAction::triggered, this, &BuildingEditorWindow::resizeBuilding);
    connect(ui->actionFlipHorizontal, &QAction::triggered, this, &BuildingEditorWindow::flipHorizontal);
    connect(ui->actionFlipVertical, &QAction::triggered, this, &BuildingEditorWindow::flipVertical);
    connect(ui->actionRotateRight, &QAction::triggered, this, &BuildingEditorWindow::rotateRight);
    connect(ui->actionRotateLeft, &QAction::triggered, this, &BuildingEditorWindow::rotateLeft);

    connect(ui->actionInsertFloorAbove, &QAction::triggered, this, &BuildingEditorWindow::insertFloorAbove);
    connect(ui->actionInsertFloorBelow, &QAction::triggered, this, &BuildingEditorWindow::insertFloorBelow);
    connect(ui->actionRemoveFloor, &QAction::triggered, this, &BuildingEditorWindow::removeFloor);
    connect(ui->actionFloors, &QAction::triggered, this, &BuildingEditorWindow::floorsDialog);

    connect(ui->actionBuildingProperties, &QAction::triggered,
            this, &BuildingEditorWindow::buildingPropertiesDialog);
    connect(ui->actionKeyValues, &QAction::triggered, this, &BuildingEditorWindow::keyValuesDialog);
    connect(ui->actionGrime, &QAction::triggered,
            this, &BuildingEditorWindow::buildingGrime);
    connect(ui->actionRooms, &QAction::triggered, this, &BuildingEditorWindow::roomsDialog);
    connect(ui->actionTemplates, &QAction::triggered, this, &BuildingEditorWindow::templatesDialog);
    connect(ui->actionTiles, &QAction::triggered, this, &BuildingEditorWindow::tilesDialog);
    connect(ui->actionTemplateFromBuilding, &QAction::triggered,
            this, &BuildingEditorWindow::templateFromBuilding);
    connect(ui->actionBasementAccessNone, &QAction::triggered, this, &BuildingEditorWindow::setBasementAccessNone);
    connect(ui->actionBasementAccessNorth, &QAction::triggered, this, &BuildingEditorWindow::setBasementAccessNorth);
    connect(ui->actionBasementAccessWest, &QAction::triggered, this, &BuildingEditorWindow::setBasementAccessWest);

    connect(ui->actionHelp, &QAction::triggered, this, &BuildingEditorWindow::help);
    connect(ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);

    // Do this after connect() calls above -> esp. documentAdded()
    mWelcomeMode = new WelcomeMode(this);
    mOrthoObjectEditMode = new OrthoObjectEditMode(this);
    mIsoObjectEditMode = new IsoObjectEditMode(this);
    mTileEditMode = new TileEditMode(this);
    mAttributeEditMode = new AttributeEditMode(this);

    connect(mIsoObjectEditMode, &IsoObjectEditMode::viewAddedForDocument,
            this, &BuildingEditorWindow::viewAddedForDocument);
    connect(mTileEditMode, &TileEditMode::viewAddedForDocument,
            this, &BuildingEditorWindow::viewAddedForDocument);
    connect(mAttributeEditMode, &AttributeEditMode::viewAddedForDocument,
            this, &BuildingEditorWindow::viewAddedForDocument);

    ::Utils::StyleHelper::setBaseColor(::Utils::StyleHelper::DEFAULT_BASE_COLOR);
    mTabWidget = new Core::Internal::FancyTabWidget;
    mTabWidget->setObjectName(QLatin1String("FancyTabWidget"));
    mTabWidget->statusBar()->setVisible(false);
    new ModeManager(mTabWidget, this);
    ModeManager::instance().addMode(mWelcomeMode);
    ModeManager::instance().addMode(mOrthoObjectEditMode);
    ModeManager::instance().addMode(mIsoObjectEditMode);
    ModeManager::instance().addMode(mTileEditMode);
    ModeManager::instance().addMode(mAttributeEditMode);
    setCentralWidget(mTabWidget);

    mWelcomeMode->setEnabled(true);
    ModeManager::instance().setCurrentMode(mWelcomeMode);

    connect(ModeManager::instancePtr(), &ModeManager::currentModeAboutToChange,
            this, &BuildingEditorWindow::currentModeAboutToChange);
    connect(ModeManager::instancePtr(), &ModeManager::currentModeChanged,
            this, &BuildingEditorWindow::currentModeChanged);

    // Do this *after* all the different modes handle the document changing.
    connect(docman(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &BuildingEditorWindow::reportMissingTilesets);

    initActionManager();

#ifndef BUILDINGED_SA
    readSettings();
#endif

    updateActions();
    updateWindowTitle();
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
        if (mKeyboardShortcutWindow != nullptr) {
            mKeyboardShortcutWindow->close();
        }
        event->accept(); // doesn't destroy us
    } else
        event->ignore();

}

bool BuildingEditorWindow::openFile(const QString &fileName)
{
    // Select existing document if this file is already open
    int documentIndex = docman()->findDocument(fileName);
    if (documentIndex != -1) {
        docman()->setCurrentDocument(documentIndex);
        if (mWelcomeMode->isActive()) {
            IMode *mode = 0;
            switch (mCurrentDocumentStuff->editMode()) {
            case EditorWindowPerDocumentStuff::OrthoObjectMode:
                mode = mOrthoObjectEditMode;
                break;
            case EditorWindowPerDocumentStuff::IsoObjectMode:
                mode = mIsoObjectEditMode;
                break;
            case EditorWindowPerDocumentStuff::TileMode:
                mode = mTileEditMode;
                break;
            case EditorWindowPerDocumentStuff::AttributeMode:
                mode = mAttributeEditMode;
                break;
            }
            ModeManager::instance().setCurrentMode(mode);
        }
        return true;
    }

    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        docman()->addDocument(doc);
        addRecentFile(fileName);
        return true;
    }

    QMessageBox::warning(this, tr("Error reading building"), error);
    return false;
}

bool BuildingEditorWindow::openAutoSave(const QString &fileName)
{
    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        docman()->addDocument(doc);
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
#ifdef BUILDINGED_SA
    mIsoObjectEditMode->afterInitConfigFiles();
    mOrthoObjectEditMode->afterInitConfigFiles();
    mTileEditMode->afterInitConfigFiles();
#endif

    // Refresh the ui before blocking while loading tilesets etc
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        if (ts->isMissing()) {
            PROGRESS progress(tr("Loading Tilesets.txt tilesets"), this);
            TileMetaInfoMgr::instance()->loadTilesets(true);
            TilesetManager::instance()->waitForTilesets();
            break;
        }
    }

    /////

    connect(BuildingTilesMgr::instance(), &BuildingTilesMgr::tilesetAdded,
            this, &BuildingEditorWindow::tilesetAdded);
    connect(BuildingTilesMgr::instance(), &BuildingTilesMgr::tilesetAboutToBeRemoved,
            this, &BuildingEditorWindow::tilesetAboutToBeRemoved);
    connect(BuildingTilesMgr::instance(), &BuildingTilesMgr::tilesetRemoved,
            this, &BuildingEditorWindow::tilesetRemoved);

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged,
            this, &BuildingEditorWindow::tilesetChanged);

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

void BuildingEditorWindow::focusOn(const QString &file, int x, int y, int z, int objectIndex)
{
    int documentIndex = docman()->findDocument(file);
    if (documentIndex != -1) {
        docman()->setCurrentDocument(documentIndex);
        mCurrentDocumentStuff->focusOn(x, y, z, objectIndex);
    }
}

BuildingDocumentMgr *BuildingEditorWindow::docman() const
{
    return BuildingDocumentMgr::instance();
}

void BuildingEditorWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("MainWindow"));
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

    mOrthoObjectEditMode->readSettings(mSettings);
    mIsoObjectEditMode->readSettings(mSettings);
    mTileEditMode->readSettings(mSettings);
    mAttributeEditMode->readSettings(mSettings);
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
#if 0
    mSettings.setValue(QLatin1String("EditorScale"), mOrthoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("IsoEditorScale"), mIsoView->zoomable()->scale());
    mSettings.setValue(QLatin1String("Orientation"),
                       (mOrient == OrientOrtho) ? QLatin1String("orthogonal")
                                                : QLatin1String("isometric"));
#endif
    mSettings.endGroup();

    mOrthoObjectEditMode->writeSettings(mSettings);
    mIsoObjectEditMode->writeSettings(mSettings);
    mTileEditMode->writeSettings(mSettings);
    mAttributeEditMode->writeSettings(mSettings);
}

void BuildingEditorWindow::saveSplitterSizes(QSplitter *splitter)
{
//    mSettings.beginGroup(QLatin1String("MainWindow"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    mSettings.setValue(tr("%1.sizes").arg(splitter->objectName()), v);
//    mSettings.endGroup();
}

void BuildingEditorWindow::restoreSplitterSizes(QSplitter *splitter)
{
//    mSettings.beginGroup(QLatin1String("MainWindow"));
    QVariant v = mSettings.value(tr("%1.sizes").arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
//    mSettings.endGroup();
}

QToolBar *BuildingEditorWindow::createCommonToolBar()
{
    QToolBar *toolBar = new QToolBar;
    toolBar->setWindowTitle(tr("Main ToolBar"));
    toolBar->addAction(ui->actionNewBuilding);
    toolBar->addAction(ui->actionOpen);
    toolBar->addAction(ui->actionSave);
    toolBar->addAction(ui->actionSaveAs);
    toolBar->addSeparator();
    toolBar->addAction(ui->menuEdit->actions().at(0)); // undo
    toolBar->addAction(ui->menuEdit->actions().at(1)); // redo
    return toolBar;
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
                QLatin1String("OpenSaveDirectory")).toString();
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Building"),
                                                    initialDir, filter);
    if (fileName.isEmpty())
        return;

    mSettings.setValue(QLatin1String("OpenSaveDirectory"),
                       QFileInfo(fileName).absolutePath());

    QString error;
    if (BuildingDocument *doc = BuildingDocument::read(fileName, error)) {
        docman()->addDocument(doc);
        addRecentFile(fileName);
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
                    QLatin1String("OpenSaveDirectory")).toString();
        suggestedFileName += QLatin1Char('/');
        suggestedFileName += tr("untitled.tbx");
    }

    const QString fileName =
            QFileDialog::getSaveFileName(this, QString(), suggestedFileName,
                                         tr("TileZed building files (*.tbx)"));
    if (!fileName.isEmpty()) {
        mSettings.setValue(QLatin1String("OpenSaveDirectory"),
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

    if (!fileName.endsWith(QLatin1String(".autosave")))
        addRecentFile(fileName);
    return true;
}

bool BuildingEditorWindow::confirmSave()
{
    if (!mCurrentDocument || !mCurrentDocument->isModified())
        return true;

    if (ModeManager::instance().currentMode() == mWelcomeMode)
        ModeManager::instance().setCurrentMode(mIsoObjectEditMode);

    if (isMinimized())
        setWindowState(windowState() & (~Qt::WindowMinimized | Qt::WindowActive));

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

QStringList BuildingEditorWindow::recentFiles() const
{
    return mSettings.value(QLatin1String("RecentFiles"))
            .toStringList();
}

void BuildingEditorWindow::addRecentFile(const QString &fileName)
{
    // Remember the file by its canonical file path
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

    if (canonicalFilePath.isEmpty())
        return;

    const int MaxRecentFiles = 10;

    QStringList files = recentFiles();
    files.removeAll(canonicalFilePath);
    files.prepend(canonicalFilePath);
    while (files.size() > MaxRecentFiles)
        files.removeLast();

    mSettings.setValue(QLatin1String("RecentFiles"), files);
    emit recentFilesChanged();
}

void BuildingEditorWindow::documentAdded(BuildingDocument *doc)
{
    mUndoGroup->addStack(doc->undoStack());

    mDocumentStuff[doc] = new EditorWindowPerDocumentStuff(doc);

    mOrthoObjectEditMode->setEnabled(true);
    mIsoObjectEditMode->setEnabled(true);
    mTileEditMode->setEnabled(true);
    mAttributeEditMode->setEnabled(true);

//    reportMissingTilesets();
#if 1
#else
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
    Q_UNUSED(index)

    mDocumentStuff.remove(doc);

    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
}

void BuildingEditorWindow::currentDocumentChanged(BuildingDocument *doc)
{
    mDocumentChanging = true;

    if (mCurrentDocument) {
        if (!mWelcomeMode->isActive())
            mCurrentDocumentStuff->rememberTool();
        mCurrentDocument->disconnect(this);
    }

    mCurrentDocument = doc;
    mCurrentDocumentStuff = doc ? mDocumentStuff[doc] : nullptr; // FIXME: unset when deleted

    if (mCurrentDocument) {
        IMode *mode = 0;
        switch (mCurrentDocumentStuff->editMode()) {
        case EditorWindowPerDocumentStuff::OrthoObjectMode:
            mode = mOrthoObjectEditMode;
            break;
        case EditorWindowPerDocumentStuff::IsoObjectMode:
            mode = mIsoObjectEditMode;
            break;
        case EditorWindowPerDocumentStuff::TileMode:
            mode = mTileEditMode;
            break;
        case EditorWindowPerDocumentStuff::AttributeMode:
            mode = mAttributeEditMode;
            break;
        }
        ModeManager::instance().setCurrentMode(mode);

        mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

        connect(mCurrentDocument, &BuildingDocument::floorAdded,
                this, &BuildingEditorWindow::updateActions);
        connect(mCurrentDocument, &BuildingDocument::floorRemoved,
                this, &BuildingEditorWindow::updateActions);
        connect(mCurrentDocument, &BuildingDocument::currentFloorChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::currentLayerChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::currentRoomChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::selectedObjectsChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::roomSelectionChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::tileSelectionChanged,
                this, &BuildingEditorWindow::updateActions);
        connect(mCurrentDocument, &BuildingDocument::clipboardTilesChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::basementAccessChanged,
                this, &BuildingEditorWindow::updateActions);

        connect(mCurrentDocument, &BuildingDocument::cleanChanged, this, &BuildingEditorWindow::updateWindowTitle);
    } else {
        ToolManager::instance()->clearDocument();

        mOrthoObjectEditMode->setEnabled(false);
        mIsoObjectEditMode->setEnabled(false);
        mTileEditMode->setEnabled(false);
        mAttributeEditMode->setEnabled(false);
    }

    updateActions();
    updateWindowTitle();

    if (mCurrentDocumentStuff && !mWelcomeMode->isActive()) {
        mCurrentDocumentStuff->setInitialPosition();
        mCurrentDocumentStuff->restoreTool();
    }

    mDocumentChanging = false;
}

void BuildingEditorWindow::currentEditorChanged()
{
    updateActions();

    // This is needed only when the mode didn't change.
    if (mCurrentDocumentStuff && !mDocumentChanging && !mWelcomeMode->isActive())
        mCurrentDocumentStuff->restoreTool();
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
    if (ModeManager::instance().currentMode() == mWelcomeMode) {
        setWindowTitle(tr("BuildingEd"));
        return;
    }

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
                QLatin1String("ExportDirectory")).toString();

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

    mSettings.setValue(QLatin1String("ExportDirectory"),
                       QFileInfo(fileName).absolutePath());
}

#include "buildingmap.h"
#include "exportbasementsdialog.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "newmapbinaryfile.h"

void BuildingEditorWindow::exportNewBinary()
{
    ExportBasementsDialog dialog(this);
    int result = dialog.exec();
    if (result != QDialog::Accepted) {
        return;
    }

    QStringList fileNames = dialog.fileNames();
    if (fileNames.isEmpty()) {
        QMessageBox::information(this, tr("Export Basements"), tr("No TBX files were selected for export."));
        return;
    }

    QSet<QString> northStairTiles;
    QSet<QString> westStairTiles;
    getTopStaircaseTiles(northStairTiles, westStairTiles);

    QString luaCode;
    for (const QString& fileName : qAsConst(fileNames)) {
        exportNewBinaryFile(&dialog, fileName, northStairTiles, westStairTiles, luaCode);
    }

    QApplication::clipboard()->setText(luaCode);
    QMessageBox::information(this, tr("Export Basements"), tr("basements.lua code was copied to the system clipboard."));
}

void BuildingEditorWindow::editCut()
{
    if (!mCurrentDocument || currentLayer().isEmpty())
        return;

    if (mCurrentDocumentStuff->isTile()) {
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

    if (mCurrentDocumentStuff->isTile()) {
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

    if (mCurrentDocumentStuff->isTile()) {
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
    if (mCurrentDocumentStuff->isTile()) {
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

void BuildingEditorWindow::editDeleteInAllLayers()
{
    if (!mCurrentDocument)
        return;
    if (!mCurrentDocumentStuff->isTile())
        return;
    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Delete In All Layers"));
    QRegion selection = mCurrentDocument->tileSelection();
    QRect r = selection.boundingRect();
    BuildingFloor *floor = currentFloor();
    for (const QString &layerName : floor->grimeLayers()) {
        FloorTileGrid *tiles = floor->grimeAt(layerName, r);
        bool changed = tiles->replace(selection.translated(-r.topLeft()), QString());
        if (changed) {
            mCurrentDocument->undoStack()->push(
                        new PaintFloorTiles(mCurrentDocument, floor,
                                            layerName, selection,
                                            r.topLeft(), tiles,
                                            "Delete In All Layers"));
        } else {
            delete tiles;
        }
    }
    undoStack->endMacro();
}

void BuildingEditorWindow::selectAll()
{
    if (!mCurrentDocument)
        return;
    if (mCurrentDocumentStuff->isTile() || mCurrentDocumentStuff->isAttribute()) {
        mCurrentDocument->undoStack()->push(
                    new ChangeTileSelection(mCurrentDocument, currentFloor()->bounds(1, 1)));
        return;
    }
    if (PencilTool::instance()->isCurrent() || SelectMoveRoomsTool::instance()->isCurrent()) {
        mCurrentDocument->undoStack()->push(
                    new ChangeRoomSelection(mCurrentDocument, currentFloor()->bounds()));
        return;
    }
    QSet<BuildingObject*> objects(currentFloor()->objects().begin(), currentFloor()->objects().end());
    mCurrentDocument->setSelectedObjects(objects);
}

void BuildingEditorWindow::selectNone()
{
    if (!mCurrentDocument)
        return;
    if (mCurrentDocumentStuff->isTile() || mCurrentDocumentStuff->isAttribute()) {
        mCurrentDocument->undoStack()->push(
                    new ChangeTileSelection(mCurrentDocument, QRegion()));
        return;
    }
    if (PencilTool::instance()->isCurrent() || SelectMoveRoomsTool::instance()->isCurrent()) {
        mCurrentDocument->undoStack()->push(
                    new ChangeRoomSelection(mCurrentDocument, QRegion()));
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

void BuildingEditorWindow::keyValuesDialog()
{
    if (mCurrentDocument == nullptr)
        return;

    BuildingKeyValuesDialog dialog(mCurrentDocument, this);
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
    if (mCurrentDocument == nullptr) {
        return;
    }
    QList<Room*> originalRoomList = mCurrentDocument->building()->rooms();
    RoomsDialog dialog(mCurrentDocument, mCurrentDocument->currentRoom(), this);
    dialog.setWindowTitle(tr("Rooms in building"));

    if (dialog.exec() != QDialog::Accepted)
        return;
#if 0
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
#endif
}

void BuildingEditorWindow::roomAdded(Room *room)
{
    Q_UNUSED(room)
    updateActions();
}

void BuildingEditorWindow::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    updateActions();
}

void BuildingEditorWindow::roomsReordered()
{
}

void BuildingEditorWindow::roomChanged(Room *room)
{
    Q_UNUSED(room)
}

void BuildingEditorWindow::cropToMinimum()
{
    if (!mCurrentDocument)
        return;

    QRect bounds;
    QRect tileBounds;
    QRect vertObjects, horzObjects;
    Building *building = currentBuilding();
    foreach (BuildingFloor *floor, building->floors()) {
        for (int y = 0; y < floor->height(); y++) {
            for (int x = 0; x < floor->width(); x++) {
                if (floor->GetRoomAt(x, y))
                    bounds |= QRect(x, y, 1, 1);
            }
        }

        // Door, Window, and Wall objects are allowed to lie on the
        // outer edge of a building without adding an extra row/column
        // to the building.
        foreach (BuildingObject *object, floor->objects()) {
            if (object->asDoor() || object->asWindow()) {
                if (object->isN())
                    horzObjects |= object->bounds();
                else
                    vertObjects |= object->bounds();
            } else if (object->asWall()) {
                if (object->isN()) // bass-ackwards
                    vertObjects |= object->bounds();
                else
                    horzObjects |= object->bounds();
            } else
                bounds |= object->bounds() & floor->bounds();
        }

        for (int y = 0; y < floor->height() + 1; y++) {
            for (int x = 0; x < floor->width() + 1; x++) {
                foreach (QString layerName, floor->grimeLayers())
                    if (!floor->grimeAt(layerName, x, y).isEmpty())
                        tileBounds |= QRect(x, y, 1, 1);
            }
        }
    }
    if (tileBounds.width() > 1) tileBounds.adjust(0, 0, -1, 0);
    if (tileBounds.height() > 1) tileBounds.adjust(0, 0, 0, -1);
    bounds |= tileBounds;

    if (horzObjects.bottom() > (bounds | vertObjects).bottom())
        horzObjects.adjust(0, 0, 0, -1);
    if (vertObjects.right() > (bounds | horzObjects).right())
        vertObjects.adjust(0, 0, -1, 0);
    bounds |= horzObjects | vertObjects;

    bounds &= building->bounds();
    if (!bounds.isEmpty())
        cropBuilding(bounds);
}

void BuildingEditorWindow::cropToSelection()
{
    if (!mCurrentDocument)
        return;

    QRegion selection = mCurrentDocument->roomSelection();
    if (selection.isEmpty())
        return;

    QRect bounds = selection.boundingRect() & mCurrentDocument->building()->bounds();
    cropBuilding(bounds);
}

void BuildingEditorWindow::cropBuilding(const QRect &bounds)
{
    QPoint offset = -bounds.topLeft();
    QSize newSize = bounds.size();

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Crop Building"));

    // Offset
    foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {

        // Move the rooms
        QVector<QVector<Room*> > grid;
        grid.resize(floor->width());
        for (int x = 0; x < floor->width(); x++)
            grid[x].resize(floor->height());
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            for (int x = bounds.left(); x <= bounds.right(); x++) {
                grid[x-bounds.left()][y-bounds.top()] = floor->GetRoomAt(x, y);
            }
        }
        undoStack->push(new SwapFloorGrid(mCurrentDocument, floor, grid,
                                          "Offset Rooms"));

        // Move the user-placed tiles
        QMap<QString,FloorTileGrid*> grime;
        foreach (QString layerName, floor->grimeLayers()) {
            FloorTileGrid *src = floor->grime()[layerName];
            FloorTileGrid *dest = new FloorTileGrid(floor->width() + 1, floor->height() + 1);
            for (int y = bounds.top(); y <= bounds.bottom() + 1; y++) {
                for (int x = bounds.left(); x <= bounds.right() + 1; x++) {
                    dest->replace(x-bounds.left(), y-bounds.top(), src->at(x, y));
                }
            }
            grime[layerName] = dest;
        }
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, grime,
                                           "Offset Tiles", true));
    }

    // Resize
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, true));
    undoStack->push(new ResizeBuilding(mCurrentDocument, offset, newSize));
    bool objectsDeleted = false;
    foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {
        undoStack->push(new ResizeFloor(mCurrentDocument, floor, newSize));
        // Offset objects. Remove objects that aren't in bounds.
        for (int i = floor->objectCount() - 1; i >= 0; --i) {
            BuildingObject *object = floor->object(i);
            if (object->isValidPos(offset))
                undoStack->push(new MoveObject(mCurrentDocument, object, object->pos() + offset));
            else {
                undoStack->push(new RemoveObject(mCurrentDocument, floor, i));
                objectsDeleted = true;
            }
        }
    }
    if (mCurrentDocument->building()->hasBasementAccess()) {
        BasementAccess ba = mCurrentDocument->building()->basementAccess();
        ba.mX += offset.x();
        ba.mY += offset.y();
        undoStack->push(new SetBasementAccess(mCurrentDocument, ba));
    }
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, false));

    undoStack->push(new ChangeRoomSelection(mCurrentDocument,
                                            mCurrentDocument->roomSelection().translated(offset)));
    undoStack->endMacro();

    if (objectsDeleted) {
        QMessageBox::information(this, tr("Crop Building"),
                                 tr("Some objects were deleted during cropping."));
    }
}

void BuildingEditorWindow::exportNewBinaryFile(ExportBasementsDialog *dialog, const QString &tbxFilePath, QSet<QString> &northStairTiles, QSet<QString> &westStairTiles, QString& luaCode)
{
    BuildingReader reader;
    Building *building = reader.read(tbxFilePath);
    if (building == nullptr) {
        return;
    }
    reader.fix(building);
    BuildingMap bmap(building);
    Map *map = bmap.mergedMap();

#if 0
    if (map->orientation() == Map::LevelIsometric) {
        if (!BuildingPreferences::instance()->levelIsometric()) {
            Map *isoMap = MapManager::instance()->convertOrientation(map, Map::Isometric);
            TilesetManager::instance()->removeReferences(map->tilesets());
            delete map;
            map = isoMap;
        }
    }
#endif

    for (BuildingFloor *floor : building->floors()) {
#if 0
        // The given map has layers required by the editor, i.e., Floors, Walls,
        // Doors, etc.  The TMXConfig.txt file may specify extra layer names.
        // So we need to insert any extra layers in the order specified in
        // TMXConfig.txt.  If the layer name has a N_ prefix, it is only added
        // to level N, otherwise it is added to every level.  Object layers are
        // added above *all* the tile layers in the map.
        int previousExistingLayer = -1;
        foreach (LayerInfo layerInfo, mLayers) {
            QString layerName = layerInfo.mName;
            int level;
            if (MapComposite::levelForLayer(layerName, &level)) {
                if (level != floor->level())
                    continue;
            } else {
                layerName = tr("%1_%2").arg(floor->level()).arg(layerName);
            }
            int n;
            if ((n = map->indexOfLayer(layerName)) >= 0) {
                previousExistingLayer = n;
                continue;
            }
            if (layerInfo.mType == LayerInfo::Tile) {
                TileLayer *tl = new TileLayer(layerName, 0, 0,
                                              map->width(), map->height());
                if (previousExistingLayer < 0)
                    previousExistingLayer = 0;
                map->insertLayer(previousExistingLayer + 1, tl);
                previousExistingLayer++;
            } else {
                ObjectGroup *og = new ObjectGroup(layerName,
                                                  0, 0, map->width(), map->height());
                map->addLayer(og);
            }
        }
#endif

        bmap.addRoomDefObjects(map, floor);
    }

    MapInfo* mapInfo = MapManager::instance()->newFromMap(map);
    MapComposite mapComposite(mapInfo);

    QPoint offset;
    MapComposite* mapCompositeCropped = mapComposite.cropToMinimum(offset);
    MapComposite* mapCompositeToWrite = mapCompositeCropped ? mapCompositeCropped : &mapComposite;

    QVector<Tiled::PropertiesGrid*> attributesGrids;
    for (BuildingFloor *floor : building->floors()) {
        attributesGrids += floor->squarePropertiesGrid()->clone(QRect(offset, mapCompositeToWrite->map()->size()));
    }

    BasementAccess ba = building->basementAccess();

    delete building;

    int SquaresPerChunk = 8;
    NewMapBinaryFile file(SquaresPerChunk);
    QFileInfo fileInfo(tbxFilePath);
    QString fileName = QDir(dialog->exportDirectory()).filePath(fileInfo.completeBaseName() + QStringLiteral(".pzby"));
    bool isBasementAccess = fileInfo.fileName().startsWith(QStringLiteral("ba_"));
    MapLevel *mapLevel = isBasementAccess ? map->minMapLevel() : map->maxMapLevel();
    if (file.write(mapCompositeToWrite, attributesGrids, fileName) && (mapLevel != nullptr)) {
        Map* mapToWrite = mapCompositeToWrite->map();
        MapInfo *mapInfo1 = mapCompositeToWrite->mapInfo();
        if (ba.isValid()) {
            luaCode += QStringLiteral("%1 = { width=%2, height=%3, stairx=%4, stairy=%5, stairDir=\"%6\" },")
                    .arg(fileInfo.completeBaseName())
                    .arg(mapInfo1->width())
                    .arg(mapInfo1->height())
                    .arg(ba.mX - offset.x())
                    .arg(ba.mY - offset.y())
                    .arg(ba.dirString());
            luaCode += QStringLiteral("\n");
        } else {
            int stairx = 0;
            int stairy = 0;
            QString stairDir = QStringLiteral("N");
            if (getBasementStaircase(mapToWrite, northStairTiles, westStairTiles, stairx, stairy, stairDir, isBasementAccess)) {
                luaCode += QStringLiteral("%1 = { width=%2, height=%3, stairx=%4, stairy=%5, stairDir=\"%6\" },")
                        .arg(fileInfo.completeBaseName())
                        .arg(mapInfo1->width())
                        .arg(mapInfo1->height())
                        .arg(stairx)
                        .arg(stairy)
                        .arg(stairDir);
                luaCode += QStringLiteral("\n");
            }
        }
    }

    TilesetManager::instance()->removeReferences(map->tilesets());

    if (mapCompositeCropped) {
        delete mapCompositeCropped->mapInfo();
        delete mapCompositeCropped->map();
        delete mapCompositeCropped;
    }

    delete mapInfo;
    delete map;
    qDeleteAll(attributesGrids);
}

void BuildingEditorWindow::getTopStaircaseTiles(QSet<QString> &northStairTiles, QSet<QString> &westStairTiles)
{
    TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    tileDefWatcher->check();
    for (TileDefWatcherFile *watcherFile : qAsConst(tileDefWatcher->mFiles)) {
        for (TileDefTileset* tdts : watcherFile->mTileDefFile->tilesets()) {
            for (TileDefTile* tdt : qAsConst(tdts->mTiles)) {
                if (tdt->mProperties.contains(QStringLiteral("stairsTN"))) {
                    northStairTiles += BuildingTilesMgr::nameForTile(tdt->tileset()->mName, tdt->id());
                }
                else if (tdt->mProperties.contains(QStringLiteral("stairsTW"))) {
                    westStairTiles += BuildingTilesMgr::nameForTile(tdt->tileset()->mName, tdt->id());
                }
            }
        }
    }
}

bool BuildingEditorWindow::getBasementStaircase(Tiled::Map *map, QSet<QString> &northStairTiles, QSet<QString> &westStairTiles, int &stairx, int &stairy, QString &stairDir, bool isBasementAccess)
{
    MapLevel* mapLevel = isBasementAccess ? map->minMapLevel() : map->maxMapLevel();
    for (TileLayer* tileLayer : mapLevel->tileLayers()) {
        for (int y = 0; y < tileLayer->height(); y++) {
            for (int x = 0; x < tileLayer->width(); x++) {
                const Cell& cell = tileLayer->cellAt(x, y);
                if (cell.isEmpty()) {
                    continue;
                }
                QString tileName = BuildingEditor::BuildingTilesMgr::instance()->nameForTile(cell.tile);
                if (northStairTiles.contains(tileName)) {
                    stairx = x;
                    stairy = y;
                    stairDir = QStringLiteral("N");
                    return true;
                }
                if (westStairTiles.contains(tileName)) {
                    stairx = x;
                    stairy = y;
                    stairDir = QStringLiteral("W");
                    return true;
                }
            }
        }
    }
    return false;
}

void BuildingEditorWindow::resizeBuilding()
{
    if (!mCurrentDocument)
        return;

#if 1
    ResizeDialog dialog(this);
    dialog.setOldSize(currentBuilding()->size());
    if (dialog.exec() != QDialog::Accepted)
        return;

    QPoint offset = dialog.offset();
    QSize newSize = dialog.newSize();
    QRect newBounds(QPoint(), newSize);
//    QRect overlap = newBounds & currentBuilding()->bounds().translated(offset);

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Resize Building"));

    // Calculate offset+resized room+tile grids.
    QMap<BuildingFloor*, QVector<QVector<Room*> > > grids;
    QMap<BuildingFloor*, QMap<QString,FloorTileGrid*> > grimes;
    foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {

        // Rooms
        QVector<QVector<Room*> > grid;
        grid.resize(newSize.width());
        for (int x = 0; x < newSize.width(); x++)
            grid[x].resize(newSize.height());
        for (int y = 0; y < floor->height(); y++) {
            for (int x = 0; x < floor->width(); x++) {
                if (newBounds.contains(x + offset.x(), y + offset.y()))
                    grid[x + offset.x()][y + offset.y()] = floor->GetRoomAt(x, y);
            }
        }
        grids[floor] = grid;

        // User-placed tiles
        QMap<QString,FloorTileGrid*> grime;
        foreach (QString layerName, floor->grimeLayers()) {
            FloorTileGrid *src = floor->grime()[layerName];
            FloorTileGrid *dest = new FloorTileGrid(newSize.width() + 1, newSize.height() + 1);
            for (int y = 0; y < floor->height() + 1; y++) {
                for (int x = 0; x < floor->width() + 1; x++) {
                    if (newBounds.adjusted(0, 0, 1, 1).contains(x + offset.x(), y + offset.y()))
                        dest->replace(x + offset.x(), y + offset.y(), src->at(x, y));
                }
            }
            grime[layerName] = dest;
        }
        grimes[floor] = grime;
    }

    // Resize
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, true));
    undoStack->push(new ResizeBuilding(mCurrentDocument, offset, newSize));
    foreach (BuildingFloor *floor, mCurrentDocument->building()->floors()) {
        undoStack->push(new ResizeFloor(mCurrentDocument, floor, newSize));
        undoStack->push(new SwapFloorGrid(mCurrentDocument, floor, grids[floor],
                                          "Offset Rooms"));
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, grimes[floor],
                                           "Offset Tiles", true));
        // Offset objects. Remove objects that aren't in bounds.
        for (int i = floor->objectCount() - 1; i >= 0; --i) {
            BuildingObject *object = floor->object(i);
            if (object->isValidPos(offset))
                undoStack->push(new MoveObject(mCurrentDocument, object, object->pos() + offset));
            else {
                undoStack->push(new RemoveObject(mCurrentDocument, floor, i));
            }
        }
    }
    if (mCurrentDocument->building()->hasBasementAccess()) {
        BasementAccess ba = mCurrentDocument->building()->basementAccess();
        ba.mX += offset.x();
        ba.mY += offset.y();
        undoStack->push(new SetBasementAccess(mCurrentDocument, ba));
    }
    undoStack->push(new EmitResizeBuilding(mCurrentDocument, false));
    undoStack->endMacro();
#else
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
#endif
}

void BuildingEditorWindow::flipHorizontal()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Flip Horizontal"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    bool lostTiles = false;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        if (!floor->hasUserTiles()) continue;
        lostTiles = true;
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new FlipBuilding(mCurrentDocument, true));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();

    if (lostTiles)
        QMessageBox::information(this, tr("Flip Building"),
                                 tr("User-drawn tiles were removed during flipping."));
}

void BuildingEditorWindow::flipVertical()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    mCurrentDocument->undoStack()->beginMacro(tr("Flip Vertical"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    bool lostTiles = false;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        if (!floor->hasUserTiles()) continue;
        lostTiles = true;
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new FlipBuilding(mCurrentDocument, false));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();

    if (lostTiles)
        QMessageBox::information(this, tr("Flip Building"),
                                 tr("User-drawn tiles were removed during flipping."));
}

void BuildingEditorWindow::rotateRight()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Rotate Right"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    bool lostTiles = false;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        if (!floor->hasUserTiles()) continue;
        lostTiles = true;
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new RotateBuilding(mCurrentDocument, true));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();

    if (lostTiles)
        QMessageBox::information(this, tr("Rotate Building"),
                                 tr("User-drawn tiles were removed during rotating."));
}

void BuildingEditorWindow::rotateLeft()
{
    if (!mCurrentDocument)
        return;

    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->beginMacro(tr("Rotate Left"));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, true));
    QMap<QString,FloorTileGrid*> emptyGrime;
    bool lostTiles = false;
    foreach (BuildingFloor *floor, currentBuilding()->floors()) {
        if (!floor->hasUserTiles()) continue;
        lostTiles = true;
        undoStack->push(new SwapFloorGrime(mCurrentDocument, floor, emptyGrime,
                                           "Remove All Tiles", false));
    }
    undoStack->push(new RotateBuilding(mCurrentDocument, false));
    undoStack->push(new EmitRotateBuilding(mCurrentDocument, false));
    undoStack->endMacro();

    if (lostTiles)
        QMessageBox::information(this, tr("Rotate Building"),
                                 tr("User-drawn tiles were removed during rotating."));
}

void BuildingEditorWindow::setBasementAccessNone()
{
    if (mCurrentDocument == nullptr)
        return;
    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->push(new SetBasementAccess(mCurrentDocument, BasementAccess()));
}

void BuildingEditorWindow::setBasementAccessNorth()
{
    if (mCurrentDocument == nullptr)
        return;
    BasementAccess ba{ 0, 0, BuildingObject::Direction::N };
    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->push(new SetBasementAccess(mCurrentDocument, ba));
}

void BuildingEditorWindow::setBasementAccessWest()
{
    if (mCurrentDocument == nullptr)
        return;
    BasementAccess ba{ 0, 0, BuildingObject::Direction::W };
    QUndoStack *undoStack = mCurrentDocument->undoStack();
    undoStack->push(new SetBasementAccess(mCurrentDocument, ba));
}

void BuildingEditorWindow::templatesDialog()
{
    BuildingTemplatesDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    BuildingTemplates::instance()->replaceTemplates(dialog.templates());
    BuildingTemplates::instance()->writeTxt(this);
}

void BuildingEditorWindow::initActionManager()
{
    const QString fileName = Preferences::instance()->userPath(QStringLiteral("shortcuts/BuildingEd.txt"));
    mActionManager = new ActionManager(fileName, this);

    const QString CONTEXT_MENU = QStringLiteral("Menu");
    const QString CATEGORY_MENU_FILE = QStringLiteral("File");
    const QString CATEGORY_MENU_EDIT = QStringLiteral("Edit");
    const QString CATEGORY_MENU_VIEW = QStringLiteral("View");
    const QString CATEGORY_MENU_BUILDING = QStringLiteral("Building");
    const QString CATEGORY_MENU_FLOOR = QStringLiteral("Floor");

    ActionManager *actionManager = mActionManager;
    actionManager->registerAction(ui->actionNewBuilding, CONTEXT_MENU, CATEGORY_MENU_FILE, QStringLiteral("Menu.File.New"));
    actionManager->registerAction(ui->actionOpen, CONTEXT_MENU, CATEGORY_MENU_FILE, QStringLiteral("Menu.File.Open"));
    actionManager->registerAction(ui->actionSave, CONTEXT_MENU, CATEGORY_MENU_FILE, QStringLiteral("Menu.File.Save"));
    actionManager->registerAction(ui->actionSaveAs, CONTEXT_MENU, CATEGORY_MENU_FILE, QStringLiteral("Menu.File.SaveAs"));
    actionManager->registerAction(ui->actionClose, CONTEXT_MENU, CATEGORY_MENU_FILE, QStringLiteral("Menu.File.Close"));

    actionManager->registerAction(mUndoAction, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Undo"));
    actionManager->registerAction(mRedoAction, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Redo"));
    actionManager->registerAction(ui->actionCut, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Cut"));
    actionManager->registerAction(ui->actionCopy, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Copy"));
    actionManager->registerAction(ui->actionPaste, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Paste"));
    actionManager->registerAction(ui->actionDelete, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.Delete"));
    actionManager->registerAction(ui->actionDeleteInAllLayers, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.DeleteInAllLayers"));
    actionManager->registerAction(ui->actionSelectAll, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.SelectAll"));
    actionManager->registerAction(ui->actionSelectNone, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.SelectNone"));
    actionManager->registerAction(ui->actionPreferences, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.KeyboardShortcuts"));
    actionManager->registerAction(ui->actionKeyboardShortcuts, CONTEXT_MENU, CATEGORY_MENU_EDIT, QStringLiteral("Menu.Edit.KeyboardShortcuts"));

    actionManager->registerAction(ui->actionShowGrid, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ShowGrid"));
    actionManager->registerAction(ui->actionHighlightFloor, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.HighlightCurrentFloor"));
    actionManager->registerAction(ui->actionHighlightRoom, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.HighlightRoom"));
    actionManager->registerAction(ui->actionShowLowerFloors, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ShowLowerFloors"));
    actionManager->registerAction(ui->actionShowOnlyFloors, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ShowOnlyFloors"));
    actionManager->registerAction(ui->actionShowObjects, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ShowObjectShapes"));
    actionManager->registerAction(ui->actionHighlightUnlitRooms, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.HighlightUnlitRooms"));
    actionManager->registerAction(ui->actionZoomIn, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ZoomIn"));
    actionManager->registerAction(ui->actionZoomOut, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ZoomOut"));
    actionManager->registerAction(ui->actionNormalSize, CONTEXT_MENU, CATEGORY_MENU_VIEW, QStringLiteral("Menu.View.ZoomNormal"));

    actionManager->registerAction(ui->actionCropToMinimum, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.CropToMinimum"));
    actionManager->registerAction(ui->actionCropToSelection, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.CropToSelection"));
    actionManager->registerAction(ui->actionResize, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Resize"));
    actionManager->registerAction(ui->actionFlipHorizontal, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.FlipHorizontal"));
    actionManager->registerAction(ui->actionFlipVertical, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.FlipVertical"));
    actionManager->registerAction(ui->actionRotateLeft, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.RotateLeft"));
    actionManager->registerAction(ui->actionRotateRight, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.RotateRight"));
    actionManager->registerAction(ui->actionBuildingProperties, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Properties"));
    actionManager->registerAction(ui->actionKeyValues, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.KeyValues"));
    actionManager->registerAction(ui->actionGrime, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Grime"));
    actionManager->registerAction(ui->actionRooms, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Rooms"));
    actionManager->registerAction(ui->actionTemplates, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Templates"));
    actionManager->registerAction(ui->actionTiles, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.Tiles"));
    actionManager->registerAction(ui->actionTemplateFromBuilding, CONTEXT_MENU, CATEGORY_MENU_BUILDING, QStringLiteral("Menu.Building.TemplateFrom"));

    actionManager->registerAction(ui->actionInsertFloorAbove, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.AddFloorAbove"));
    actionManager->registerAction(ui->actionInsertFloorBelow, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.AddFloorBelow"));
    actionManager->registerAction(ui->actionRemoveFloor, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.RemoveFloor"));
    actionManager->registerAction(ui->actionFloors, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.Floors"));
    actionManager->registerAction(ui->actionUpLevel, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.UpLevel"));
    actionManager->registerAction(ui->actionDownLevel, CONTEXT_MENU, CATEGORY_MENU_FLOOR, QStringLiteral("Menu.Floor.DownLevel"));

    const QString CONTEXT_TOOLS = QStringLiteral("Tools");
    const QString CATEGORY_TOOL_OBJECT = QStringLiteral("Object");
    const QString CATEGORY_TOOL_TILE = QStringLiteral("Tile");

    actionManager->registerAction(ui->actionPencil, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object.Pencil"));
    actionManager->registerAction(ui->actionWall, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionSelectRooms, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object.SelectRoom"));
    actionManager->registerAction(ui->actionDoor, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionWindow, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionStairs, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionRoof, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionRoofShallow, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionRoof30Degree, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionRoofCorner, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionRoofCorner30Degree, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionFurniture, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionSelectObject, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object."));
    actionManager->registerAction(ui->actionBasementAccessTool, CONTEXT_TOOLS, CATEGORY_TOOL_OBJECT, QStringLiteral("Tools.Object.BasementAccess"));

    actionManager->registerAction(ui->actionDrawTiles, CONTEXT_TOOLS, CATEGORY_TOOL_TILE, QStringLiteral("Tools.Tile.Draw"));
    actionManager->registerAction(ui->actionSelectTiles, CONTEXT_TOOLS, CATEGORY_TOOL_TILE, QStringLiteral("Tools.Tile.Select"));
    actionManager->registerAction(ui->actionPickTiles, CONTEXT_TOOLS, CATEGORY_TOOL_TILE, QStringLiteral("Tools.Tile.Pick"));
    actionManager->registerAction(ui->actionFloorGrime, CONTEXT_TOOLS, CATEGORY_TOOL_TILE, QStringLiteral("Tools.Tile.FloorGrime"));

    connect(actionManager, &ActionManager::shortcutEdited, ToolManager::instance(), &ToolManager::shortcutEdited);

    // Do this after all ToolManager::register() calls.
    QString error;
    mActionManager->load(error);
    mActionManager->emitShortcutEditedForAllActions();
}

void BuildingEditorWindow::keyboardShortcuts()
{
    QString error;
    mActionManager->load(error);
    mActionManager->emitShortcutEditedForAllActions();
    if (mKeyboardShortcutWindow == nullptr) {
        mKeyboardShortcutWindow = new KeyboardShortcutWindow(mActionManager, &mSettings, QStringLiteral("KeyboardShortcutsWindow"), this);
        mKeyboardShortcutWindow->setAttribute(Qt::WA_DeleteOnClose, false);
    }
    mKeyboardShortcutWindow->show();
    mKeyboardShortcutWindow->raise();
}

void BuildingEditorWindow::tilesDialog()
{
    BuildingTilesDialog *dialog = BuildingTilesDialog::instance();
    dialog->reparent(this);
    dialog->exec();
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

void BuildingEditorWindow::showObjectsChanged(bool show)
{
    Q_UNUSED(show)
    updateActions();
}

void BuildingEditorWindow::highlightUnlitRoomsChanged(bool show)
{
    Q_UNUSED(show)
    updateActions();

    bool hasDoc = (ModeManager::instance().currentMode() != mWelcomeMode) && (mCurrentDocumentStuff != nullptr);
    if (hasDoc == false) {
        return;
    }
    if (mCurrentDocumentStuff->isoView() != nullptr) {
        mCurrentDocumentStuff->isoView()->scene()->calculateUnlitRoomMask();
    }
    if (mCurrentDocumentStuff->tileView() != nullptr) {
        mCurrentDocumentStuff->tileView()->scene()->calculateUnlitRoomMask();
    }
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
    if (!building || mCurrentDocumentStuff->missingTilesetsReported())
        return;

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
    foreach (BuildingFloor *floor, building->floors()) {
        foreach (QString layerName, floor->grimeLayers()) {
            for (int y = 0; y < floor->height(); y++) {
                for (int x = 0; x < floor->width(); x++) {
                    QString tileName = floor->grimeAt(layerName, x, y);
                    QString tilesetName;
                    int tileIndex;
                    if (!tileName.isEmpty() && BuildingTilesMgr::parseTileName(tileName, tilesetName, tileIndex)) {
                        if (!TileMetaInfoMgr::instance()->tileset(tilesetName))
                            missingTilesets.insert(tilesetName);
                    }
                }
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

    mCurrentDocumentStuff->setMissingTilesetsReported(true);
}

void BuildingEditorWindow::updateActions()
{
    bool hasDoc = (ModeManager::instance().currentMode() != mWelcomeMode) &&
            mCurrentDocument != 0;
    bool showObjects = BuildingPreferences::instance()->showObjects();
    bool objectMode = hasDoc && mCurrentDocumentStuff->isObject();
    bool attributeMode = hasDoc && mCurrentDocumentStuff->isAttribute();

    bool hasEditor = ToolManager::instance()->currentEditor() != 0;
    PencilTool::instance()->setEnabled(hasEditor && objectMode && currentRoom() != 0);
    WallTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    SelectMoveRoomsTool::instance()->setEnabled(hasEditor && objectMode);
    DoorTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    WindowTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    StairsTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    FurnitureTool::instance()->setEnabled(hasEditor && objectMode && showObjects &&
            FurnitureTool::instance()->currentTile() != nullptr);
    bool roofTilesOK = hasDoc && currentBuilding()->roofCapTile()->asRoofCap() &&
            currentBuilding()->roofSlopeTile()->asRoofSlope() /*&&
            currentBuilding()->roofTopTile()->asRoofTop()*/;
    RoofTool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    RoofShallowTool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    RoofSlope30Tool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    RoofCornerTool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    RoofCornerSlope30Tool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    SelectMoveObjectTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    BasementAccessTool::instance()->setEnabled(hasEditor && objectMode && mCurrentDocument->building()->hasBasementAccess());

    DrawTileTool::instance()->setEnabled(hasEditor && !objectMode && !currentLayer().isEmpty());
    SelectTileTool::instance()->setEnabled(hasEditor && !objectMode && !currentLayer().isEmpty());
    PickTileTool::instance()->setEnabled(hasEditor && !objectMode);
    FloorGrimeTileTool::instance()->setEnabled(hasEditor && !objectMode && !currentLayer().isEmpty());

    ui->actionUpLevel->setEnabled(hasDoc &&
                                  !mCurrentDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(hasDoc &&
                                    !mCurrentDocument->currentFloorIsBottom());

//    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionExportTMX->setEnabled(hasDoc);
    ui->actionExportNewBinary->setEnabled(hasDoc);

    ui->actionShowObjects->setEnabled(hasDoc);
    ui->actionHighlightUnlitRooms->setEnabled(hasDoc);

    ui->actionBuildingProperties->setEnabled(hasDoc);
    ui->actionKeyValues->setEnabled(hasDoc);
    ui->actionGrime->setEnabled(hasDoc);
    ui->actionRooms->setEnabled(hasDoc);
    ui->actionTemplateFromBuilding->setEnabled(hasDoc);

    ui->actionCropToMinimum->setEnabled(hasDoc);
    ui->actionCropToSelection->setEnabled(hasDoc &&
                                          !mCurrentDocument->roomSelection().isEmpty());
    ui->actionResize->setEnabled(hasDoc);
    ui->actionFlipHorizontal->setEnabled(hasDoc);
    ui->actionFlipVertical->setEnabled(hasDoc);
    ui->actionRotateRight->setEnabled(hasDoc);
    ui->actionRotateLeft->setEnabled(hasDoc);

    ui->actionInsertFloorAbove->setEnabled(hasDoc && currentBuilding()->floorCount() < MAX_BUILDING_FLOORS);
    ui->actionInsertFloorBelow->setEnabled(hasDoc && currentBuilding()->floorCount() < MAX_BUILDING_FLOORS);
    ui->actionRemoveFloor->setEnabled(hasDoc && currentBuilding()->floorCount() > 1);
    ui->actionFloors->setEnabled(hasDoc);

    bool hasTileSel = hasDoc && !objectMode && !mCurrentDocument->tileSelection().isEmpty();
    ui->actionCut->setEnabled(hasTileSel && !attributeMode);
    ui->actionCopy->setEnabled(hasTileSel && !attributeMode);
    ui->actionPaste->setEnabled(hasDoc && !objectMode && !attributeMode && mCurrentDocument->clipboardTiles());
    if (!objectMode) {
        ui->actionSelectAll->setEnabled(!currentLayer().isEmpty());
        ui->actionSelectNone->setEnabled(hasTileSel);
        ui->actionDelete->setEnabled(hasTileSel && !attributeMode);
        ui->actionDeleteInAllLayers->setEnabled(hasTileSel && !attributeMode);
    } else {
        ui->actionSelectAll->setEnabled(hasDoc);
        bool selectNone = false;
        if (PencilTool::instance()->isCurrent() || SelectMoveRoomsTool::instance()->isCurrent())
            selectNone = hasDoc && !mCurrentDocument->roomSelection().isEmpty();
        else
            selectNone = hasDoc && mCurrentDocument->selectedObjects().size();
        ui->actionSelectNone->setEnabled(selectNone);
        ui->actionDelete->setEnabled(hasDoc && mCurrentDocument->selectedObjects().size());
    }

    ui->menuViews->setEnabled(ModeManager::instance().currentMode() != mWelcomeMode);
}

void BuildingEditorWindow::help()
{
    QString fileName = Preferences::instance()->docsPath(QLatin1String("BuildingEd/index.html"));
    QUrl url = QUrl::fromLocalFile(fileName);
    QDesktopServices::openUrl(url);
}

void BuildingEditorWindow::currentModeAboutToChange(IMode *mode)
{
    Q_UNUSED(mode)

    if (!mCurrentDocument)
        return;

    if (!mDocumentChanging && !mWelcomeMode->isActive())
        mCurrentDocumentStuff->rememberTool();
}

void BuildingEditorWindow::currentModeChanged()
{
    if (!mCurrentDocument)
        return;

    IMode *mode = ModeManager::instance().currentMode();

    if (mCurrentDocumentStuff->isTile())
        mCurrentDocument->setTileSelection(QRegion());
    else if (mode == mTileEditMode)
        mCurrentDocument->setRoomSelection(QRegion());

    if (mode == mOrthoObjectEditMode)
        mCurrentDocumentStuff->toOrthoObject();
    else if (mode == mIsoObjectEditMode)
        mCurrentDocumentStuff->toIsoObject();
    else if (mode == mTileEditMode)
        mCurrentDocumentStuff->toTile();
    else if (mode == mAttributeEditMode)
        mCurrentDocumentStuff->toAttribute();

    updateActions();
    updateWindowTitle();

    if (mCurrentDocumentStuff && !mDocumentChanging && (mode != mWelcomeMode))
        mCurrentDocumentStuff->restoreTool();
}

void BuildingEditorWindow::viewAddedForDocument(BuildingDocument *doc, BuildingIsoView *view)
{
    mDocumentStuff[doc]->viewAddedForDocument(view);
}

#if 0
void BuildingEditorWindow::setEditMode()
{
    if (!mCurrentDocument)
        return;

    switch (mCurrentDocumentStuff->editMode())
    {
    case EditorWindowPerDocumentStuff::OrthoObjectMode: {
        ModeManager::instance().setCurrentMode(mOrthoObjectEditMode);
#if 0
        if (mEditMode == EditorWindowPerDocumentStuff::TileMode) {
            // Switch from Tile to OrthoObject
            mCurrentDocument->setTileSelection(QRegion());
            mModeStack->setCurrentWidget(mObjectEditMode->widget());
        }
#endif
        break;
    }
    case EditorWindowPerDocumentStuff::IsoObjectMode: {
        ModeManager::instance().setCurrentMode(mIsoObjectEditMode);
#if 0
        if (mEditMode == EditorWindowPerDocumentStuff::TileMode) {
            // Switch from Tile to IsoObject
            mCurrentDocument->setTileSelection(QRegion());
            mModeStack->setCurrentWidget(mObjectEditMode->widget());
        }
#endif
        break;
    }
    case EditorWindowPerDocumentStuff::TileMode: {
        ModeManager::instance().setCurrentMode(mTileEditMode);
#if 0
        // Switch from OrthoObject/IsoObject to Tile
        mModeStack->setCurrentWidget(mTileEditMode->widget());
        ModeManager::instance().setCurrentMode(mTileEditMode);
        break;
#endif
    }
    }
//    mTabWidget->setCurrentIndex(mCurrentDocumentStuff->editMode());
//    mModeStack->setCurrentWidget(ModeManager::instance().currentMode()->widget());

    mEditMode = mCurrentDocumentStuff->editMode();

    updateActions();

//    mCurrentDocumentStuff->restoreTool();
}

void BuildingEditorWindow::toggleOrthoIso()
{
    if (!mCurrentDocument)
        return;

    if (mCurrentDocumentStuff->isTile()) {
        toggleEditMode();
        return;
    }

    mCurrentDocumentStuff->rememberTool();
    if (mCurrentDocumentStuff->isOrthoObject())
        mCurrentDocumentStuff->toIsoObject();
    else
        mCurrentDocumentStuff->toOrthoObject();

    setEditMode();
}

void BuildingEditorWindow::toggleEditMode()
{
    if (!mCurrentDocument)
        return;

    mCurrentDocumentStuff->rememberTool();
    if (mCurrentDocumentStuff->isTile())
        mCurrentDocumentStuff->toObject();
    else
        mCurrentDocumentStuff->toTile();

    setEditMode();
}
#endif
