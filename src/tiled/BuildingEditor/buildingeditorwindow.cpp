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
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreferencesdialog.h"
#include "buildingpropertiesdialog.h"
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
#include "categorydock.h"
#include "choosebuildingtiledialog.h"
#include "furnituregroups.h"
#include "furnitureview.h"
#include "horizontallinedelegate.h"
#include "listofstringsdialog.h"
#include "mixedtilesetview.h"
#include "newbuildingdialog.h"
#include "objecteditmode.h"
#include "resizebuildingdialog.h"
#include "roomsdialog.h"
#include "simplefile.h"
#include "templatefrombuildingdialog.h"
#include "tileeditmode.h"
#include "welcomemode.h"

#include "fancytabwidget.h"
#include "utils/stylehelper.h"

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
    mIsoView(0),
    mTileView(0)
{
    connect(document()->undoStack(), SIGNAL(cleanChanged(bool)), SLOT(autoSaveCheck()));
    connect(document()->undoStack(), SIGNAL(indexChanged(int)), SLOT(autoSaveCheck()));

    mAutoSaveTimer.setSingleShot(true);
    mAutoSaveTimer.setInterval(2.5 * 60 * 1000); // 2.5 minutes
    connect(&mAutoSaveTimer, SIGNAL(timeout()), SLOT(autoSaveTimeout()));
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
    if (mEditMode == TileMode) {
        mIsoView->centerOn(mTileView->mapToScene(mTileView->viewport()->rect().center()));
        mIsoView->zoomable()->setScale(mTileView->zoomable()->scale());
    }
    mEditMode = OrthoObjectMode;
    mPrevObjectMode = OrthoObjectMode;
}

void EditorWindowPerDocumentStuff::toIsoObject()
{
    if (mEditMode == TileMode) {
        mIsoView->centerOn(mTileView->mapToScene(mTileView->viewport()->rect().center()));
        mIsoView->zoomable()->setScale(mTileView->zoomable()->scale());
    }
    mEditMode = IsoObjectMode;
    mPrevObjectMode = IsoObjectMode;
}

void EditorWindowPerDocumentStuff::toObject()
{
    if (mPrevObjectMode == OrthoObjectMode)
        toOrthoObject();
    else
        toIsoObject();
}

void EditorWindowPerDocumentStuff::toTile()
{
    mTileView->centerOn(mIsoView->mapToScene(mIsoView->viewport()->rect().center()));
    mTileView->zoomable()->setScale(mIsoView->zoomable()->scale());
    mEditMode = TileMode;
}

void EditorWindowPerDocumentStuff::rememberTool()
{
    if (isTile())
        mPrevTileTool = ToolManager::instance()->currentTool();
    else
        mPrevObjectTool = ToolManager::instance()->currentTool();
}

void EditorWindowPerDocumentStuff::restoreTool()
{
    if (isTile() && mPrevTileTool && mPrevTileTool->action()->isEnabled())
        mPrevTileTool->makeCurrent();
    else if (isObject() && mPrevObjectTool && mPrevObjectTool->action()->isEnabled())
        mPrevObjectTool->makeCurrent();
}

void EditorWindowPerDocumentStuff::viewAddedForDocument(BuildingIsoView *view)
{
    if (view->scene()->editingTiles())
        mTileView = view;
    else
        mIsoView = view;
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
    mSynching(false),
    mOrthoObjectEditMode(0),
    mIsoObjectEditMode(0),
    mTileEditMode(0)
{
    ui->setupUi(this);

    mInstance = this;

    BuildingPreferences *prefs = BuildingPreferences::instance();

    connect(docman(), SIGNAL(documentAdded(BuildingDocument*)),
            SLOT(documentAdded(BuildingDocument*)));
    connect(docman(), SIGNAL(documentAboutToClose(int,BuildingDocument*)),
            SLOT(documentAboutToClose(int,BuildingDocument*)));
    connect(docman(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));

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

    DrawTileTool::instance()->setAction(ui->actionDrawTiles);
    SelectTileTool::instance()->setAction(ui->actionSelectTiles);
    PickTileTool::instance()->setAction(ui->actionPickTiles);

    connect(PickTileTool::instance(), SIGNAL(tilePicked(QString)),
            SIGNAL(tilePicked(QString)));

    connect(ToolManager::instance(), SIGNAL(currentEditorChanged()),
            SLOT(currentEditorChanged()));

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
    Tiled::Utils::setThemeIcon(undoAction, "edit-undo");
    Tiled::Utils::setThemeIcon(redoAction, "edit-redo");
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

    // Do this after connect() calls above -> esp. documentAdded()
    mWelcomeMode = new WelcomeMode(this);
    mOrthoObjectEditMode = new OrthoObjectEditMode(this);
    mIsoObjectEditMode = new IsoObjectEditMode(this);
    mTileEditMode = new TileEditMode(this);

    connect(mIsoObjectEditMode, SIGNAL(viewAddedForDocument(BuildingDocument*,BuildingIsoView*)),
            SLOT(viewAddedForDocument(BuildingDocument*,BuildingIsoView*)));
    connect(mTileEditMode, SIGNAL(viewAddedForDocument(BuildingDocument*,BuildingIsoView*)),
            SLOT(viewAddedForDocument(BuildingDocument*,BuildingIsoView*)));

    ::Utils::StyleHelper::setBaseColor(::Utils::StyleHelper::DEFAULT_BASE_COLOR);
    mTabWidget = new Core::Internal::FancyTabWidget;
    mTabWidget->setObjectName(QLatin1String("FancyTabWidget"));
    mTabWidget->statusBar()->setVisible(false);
    new ModeManager(mTabWidget, this);
    ModeManager::instance().addMode(mWelcomeMode);
    ModeManager::instance().addMode(mOrthoObjectEditMode);
    ModeManager::instance().addMode(mIsoObjectEditMode);
    ModeManager::instance().addMode(mTileEditMode);
    setCentralWidget(mTabWidget);

    mWelcomeMode->setEnabled(true);
    ModeManager::instance().setCurrentMode(mWelcomeMode);

    connect(ModeManager::instancePtr(), SIGNAL(currentModeAboutToChange(IMode*)), SLOT(currentModeAboutToChange(IMode*)));
    connect(ModeManager::instancePtr(), SIGNAL(currentModeChanged()), SLOT(currentModeChanged()));

    readSettings();

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

    mOrthoObjectEditMode->readSettings(mSettings);
    mIsoObjectEditMode->readSettings(mSettings);
    mTileEditMode->readSettings(mSettings);
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
    mSettings.endGroup();

    mOrthoObjectEditMode->writeSettings(mSettings);
    mIsoObjectEditMode->writeSettings(mSettings);
    mTileEditMode->writeSettings(mSettings);
}

void BuildingEditorWindow::saveSplitterSizes(QSplitter *splitter)
{
//    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    mSettings.setValue(tr("%1.sizes").arg(splitter->objectName()), v);
//    mSettings.endGroup();
}

void BuildingEditorWindow::restoreSplitterSizes(QSplitter *splitter)
{
//    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
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

    if (!fileName.endsWith(QLatin1String(".autosave")))
        addRecentFile(fileName);
    return true;
}

bool BuildingEditorWindow::confirmSave()
{
    if (!mCurrentDocument || !mCurrentDocument->isModified())
        return true;

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
    return mSettings.value(QLatin1String("BuildingEditor/RecentFiles"))
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

    mSettings.setValue(QLatin1String("BuildingEditor/RecentFiles"), files);
    emit recentFilesChanged();
}

void BuildingEditorWindow::documentAdded(BuildingDocument *doc)
{
    mUndoGroup->addStack(doc->undoStack());

    mDocumentStuff[doc] = new EditorWindowPerDocumentStuff(doc);

    mOrthoObjectEditMode->setEnabled(true);
    mIsoObjectEditMode->setEnabled(true);
    mTileEditMode->setEnabled(true);

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
    Q_UNUSED(doc)

    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
}

void BuildingEditorWindow::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        mCurrentDocumentStuff->rememberTool();
        mCurrentDocument->disconnect(this);
    }

    mCurrentDocument = doc;
    mCurrentDocumentStuff = doc ? mDocumentStuff[doc] : 0; // FIXME: unset when deleted

    if (mCurrentDocument) {
        switch (mCurrentDocumentStuff->editMode())
        {
        case EditorWindowPerDocumentStuff::OrthoObjectMode: {
            ModeManager::instance().setCurrentMode(mOrthoObjectEditMode);
            break;
        }
        case EditorWindowPerDocumentStuff::IsoObjectMode: {
            ModeManager::instance().setCurrentMode(mIsoObjectEditMode);
            break;
        }
        case EditorWindowPerDocumentStuff::TileMode: {
            ModeManager::instance().setCurrentMode(mTileEditMode);
        }
        }

        mUndoGroup->setActiveStack(mCurrentDocument->undoStack());

        connect(mCurrentDocument, SIGNAL(floorAdded(BuildingFloor*)),
                SLOT(updateActions()));
        connect(mCurrentDocument, SIGNAL(floorRemoved(BuildingFloor*)),
                SLOT(updateActions()));
        connect(mCurrentDocument, SIGNAL(currentFloorChanged()),
                SLOT(updateActions()));

        connect(mCurrentDocument, SIGNAL(currentLayerChanged()),
                SLOT(updateActions()));

        connect(mCurrentDocument, SIGNAL(currentRoomChanged()),
                SLOT(updateActions()));

        connect(mCurrentDocument, SIGNAL(selectedObjectsChanged()),
                SLOT(updateActions()));

        connect(mCurrentDocument, SIGNAL(tileSelectionChanged(QRegion)),
                SLOT(updateActions()));
        connect(mCurrentDocument, SIGNAL(clipboardTilesChanged()),
                SLOT(updateActions()));

        connect(mCurrentDocument, SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));
    } else {
        ToolManager::instance()->clearDocument();

        mOrthoObjectEditMode->setEnabled(false);
        mIsoObjectEditMode->setEnabled(false);
        mTileEditMode->setEnabled(false);
    }

    updateActions();
    updateWindowTitle();
}

void BuildingEditorWindow::currentEditorChanged()
{
    updateActions();
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

void BuildingEditorWindow::selectAll()
{
    if (!mCurrentDocument)
        return;
    if (mCurrentDocumentStuff->isTile()) {
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
    if (mCurrentDocumentStuff->isTile()) {
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

void BuildingEditorWindow::updateActions()
{
    bool hasDoc = (ModeManager::instance().currentMode() != mWelcomeMode) &&
            mCurrentDocument != 0;
    bool showObjects = BuildingPreferences::instance()->showObjects();
    bool objectMode = hasDoc && mCurrentDocumentStuff->isObject();

    bool hasEditor = ToolManager::instance()->currentEditor() != 0;
    PencilTool::instance()->setEnabled(hasEditor && objectMode && currentRoom() != 0);
    WallTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    SelectMoveRoomsTool::instance()->setEnabled(hasEditor && objectMode);
    DoorTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    WindowTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    StairsTool::instance()->setEnabled(hasEditor && objectMode && showObjects);
    FurnitureTool::instance()->setEnabled(hasEditor && objectMode && showObjects &&
            FurnitureTool::instance()->currentTile() != 0);
    bool roofTilesOK = hasDoc && currentBuilding()->roofCapTile()->asRoofCap() &&
            currentBuilding()->roofSlopeTile()->asRoofSlope() /*&&
            currentBuilding()->roofTopTile()->asRoofTop()*/;
    RoofTool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    RoofCornerTool::instance()->setEnabled(hasEditor && objectMode && showObjects && roofTilesOK);
    SelectMoveObjectTool::instance()->setEnabled(hasEditor && objectMode && showObjects);

    DrawTileTool::instance()->setEnabled(hasEditor && !objectMode && !currentLayer().isEmpty());
    SelectTileTool::instance()->setEnabled(hasEditor && !objectMode && !currentLayer().isEmpty());
    PickTileTool::instance()->setEnabled(hasEditor && !objectMode);

    ui->actionUpLevel->setEnabled(hasDoc &&
                                  !mCurrentDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(hasDoc &&
                                    !mCurrentDocument->currentFloorIsBottom());

//    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionExportTMX->setEnabled(hasDoc);

    ui->actionShowObjects->setEnabled(hasDoc);

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

    ui->menuViews->setEnabled(ModeManager::instance().currentMode() != mWelcomeMode);
}

void BuildingEditorWindow::help()
{
    QUrl url = QUrl::fromLocalFile(
            QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("docs/BuildingEd/index.html"));
    QDesktopServices::openUrl(url);
}

void BuildingEditorWindow::currentModeAboutToChange(IMode *mode)
{
    if (!mCurrentDocument)
        return;

    mCurrentDocumentStuff->rememberTool();
}

void BuildingEditorWindow::currentModeChanged()
{
    if (!mCurrentDocument)
        return;

    if (mCurrentDocumentStuff->isTile())
        mCurrentDocument->setTileSelection(QRegion());

    if (ModeManager::instance().currentMode() == mOrthoObjectEditMode)
        mCurrentDocumentStuff->toOrthoObject();
    else if (ModeManager::instance().currentMode() == mIsoObjectEditMode)
        mCurrentDocumentStuff->toIsoObject();
    else if (ModeManager::instance().currentMode() == mTileEditMode)
        mCurrentDocumentStuff->toTile();

    updateActions();
    updateWindowTitle();

    if (mCurrentDocumentStuff)
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
