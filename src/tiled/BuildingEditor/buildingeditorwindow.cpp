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
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreferencesdialog.h"
#include "buildingpreviewwindow.h"
#include "buildingundoredo.h"
#include "buildingtemplates.h"
#include "buildingtemplatesdialog.h"
#include "buildingtiles.h"
#include "buildingtilesdialog.h"
#include "buildingtmx.h"
#include "buildingtools.h"
#include "FloorEditor.h"
#include "furnituregroups.h"
#include "furnitureview.h"
#include "mixedtilesetview.h"
#include "newbuildingdialog.h"
#include "resizebuildingdialog.h"
#include "roomsdialog.h"
#include "simplefile.h"
#include "templatefrombuildingdialog.h"

#include "zprogress.h"

#include "tile.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"

#include <QBitmap>
#include <QCloseEvent>
#include <QComboBox>
#include <QDebug>
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
#include <QXmlStreamReader>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////

BuildingEditorWindow* BuildingEditorWindow::mInstance = 0;

BuildingEditorWindow::BuildingEditorWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingEditorWindow),
    mCurrentDocument(0),
    roomEditor(new FloorEditor(this)),
    mRoomComboBox(new QComboBox()),
    mUndoGroup(new QUndoGroup(this)),
    mPreviewWin(0),
    mZoomable(new Zoomable(this)),
    mCategoryZoomable(new Zoomable(this)),
    mCategory(0),
    mFurnitureGroup(0),
    mSynching(false),
    mAutoSaveTimer(new QTimer(this))
{
    ui->setupUi(this);

    mInstance = this;

    BuildingPreferences *prefs = BuildingPreferences::instance();

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

    mView = ui->floorView;
    mView->setScene(roomEditor);
    ui->coordLabel->clear();
    connect(mView, SIGNAL(mouseCoordinateChanged(QPoint)),
            SLOT(mouseCoordinateChanged(QPoint)));
    connect(mView->zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));
    mView->zoomable()->connectToComboBox(ui->editorScaleComboBox);

    connect(ui->actionPecil, SIGNAL(triggered()),
            PencilTool::instance(), SLOT(makeCurrent()));
    PencilTool::instance()->setEditor(roomEditor);
    PencilTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionEraser, SIGNAL(triggered()),
            EraserTool::instance(), SLOT(makeCurrent()));
    EraserTool::instance()->setEditor(roomEditor);
    EraserTool::instance()->setAction(ui->actionEraser);

    connect(ui->actionSelectRooms, SIGNAL(triggered()),
            SelectMoveRoomsTool::instance(), SLOT(makeCurrent()));
    SelectMoveRoomsTool::instance()->setEditor(roomEditor);
    SelectMoveRoomsTool::instance()->setAction(ui->actionSelectRooms);

    connect(ui->actionDoor, SIGNAL(triggered()),
            DoorTool::instance(), SLOT(makeCurrent()));
    DoorTool::instance()->setEditor(roomEditor);
    DoorTool::instance()->setAction(ui->actionDoor);

    connect(ui->actionWindow, SIGNAL(triggered()),
            WindowTool::instance(), SLOT(makeCurrent()));
    WindowTool::instance()->setEditor(roomEditor);
    WindowTool::instance()->setAction(ui->actionWindow);

    connect(ui->actionStairs, SIGNAL(triggered()),
            StairsTool::instance(), SLOT(makeCurrent()));
    StairsTool::instance()->setEditor(roomEditor);
    StairsTool::instance()->setAction(ui->actionStairs);

    connect(ui->actionFurniture, SIGNAL(triggered()),
            FurnitureTool::instance(), SLOT(makeCurrent()));
    FurnitureTool::instance()->setEditor(roomEditor);
    FurnitureTool::instance()->setAction(ui->actionFurniture);

    /////
    connect(ui->actionRoof, SIGNAL(triggered()),
            RoofTool::instance(), SLOT(makeCurrent()));
    RoofTool::instance()->setEditor(roomEditor);
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

    connect(ui->actionRoofCorner, SIGNAL(triggered()),
            RoofCornerTool::instance(), SLOT(makeCurrent()));
    RoofCornerTool::instance()->setEditor(roomEditor);
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

    connect(ui->actionSelectObject, SIGNAL(triggered()),
            SelectMoveObjectTool::instance(), SLOT(makeCurrent()));
    SelectMoveObjectTool::instance()->setEditor(roomEditor);
    SelectMoveObjectTool::instance()->setAction(ui->actionSelectObject);

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));
    connect(ToolManager::instance(), SIGNAL(statusTextChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));

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

    ui->actionShowObjects->setChecked(prefs->showObjects());
    connect(ui->actionShowObjects, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowObjects(bool)));
    connect(prefs, SIGNAL(showObjectsChanged(bool)), SLOT(updateActions()));

    connect(ui->actionZoomIn, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomOut()));
    connect(ui->actionNormalSize, SIGNAL(triggered()),
            mView->zoomable(), SLOT(resetZoom()));

    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
//    keys += QKeySequence(tr("Ctrl+="));
    keys += QKeySequence(tr("+"));
    ui->actionZoomIn->setShortcuts(keys);
    mView->addAction(ui->actionZoomIn);
    ui->actionZoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);
    mView->addAction(ui->actionZoomOut);
    ui->actionZoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys.clear();
    keys += QKeySequence(tr("Ctrl+0"));
    keys += QKeySequence(tr("0"));
    ui->actionNormalSize->setShortcuts(keys);
    mView->addAction(ui->actionNormalSize);
    ui->actionNormalSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    connect(ui->actionResize, SIGNAL(triggered()), SLOT(resizeBuilding()));
    connect(ui->actionFlipHorizontal, SIGNAL(triggered()), SLOT(flipHorizontal()));
    connect(ui->actionFlipVertical, SIGNAL(triggered()), SLOT(flipVertical()));
    connect(ui->actionRotateRight, SIGNAL(triggered()), SLOT(rotateRight()));
    connect(ui->actionRotateLeft, SIGNAL(triggered()), SLOT(rotateLeft()));

    connect(ui->actionInsertFloorAbove, SIGNAL(triggered()), SLOT(insertFloorAbove()));
    connect(ui->actionInsertFloorBelow, SIGNAL(triggered()), SLOT(insertFloorBelow()));
    connect(ui->actionRemoveFloor, SIGNAL(triggered()), SLOT(removeFloor()));

    connect(ui->actionRooms, SIGNAL(triggered()), SLOT(roomsDialog()));
    connect(ui->actionTemplates, SIGNAL(triggered()), SLOT(templatesDialog()));
    connect(ui->actionTiles, SIGNAL(triggered()), SLOT(tilesDialog()));
    connect(ui->actionTemplateFromBuilding, SIGNAL(triggered()),
            SLOT(templateFromBuilding()));

    mCategoryZoomable->connectToComboBox(ui->scaleComboBox);
    connect(mCategoryZoomable, SIGNAL(scaleChanged(qreal)),
            prefs, SLOT(setTileScale(qreal)));
    connect(prefs, SIGNAL(tileScaleChanged(qreal)),
            SLOT(categoryScaleChanged(qreal)));

    ui->categorySplitter->setSizes(QList<int>() << 128 << 256);

    connect(ui->categoryList, SIGNAL(itemSelectionChanged()),
            SLOT(categorySelectionChanged()));

    ui->tilesetView->setZoomable(mCategoryZoomable);
    connect(ui->tilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));

    ui->furnitureView->setZoomable(mCategoryZoomable);
    connect(ui->furnitureView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(furnitureSelectionChanged()));

    ui->statusLabel->clear();

    mAutoSaveTimer->setSingleShot(true);
    mAutoSaveTimer->setInterval(2.5 * 60 * 1000);
    connect(mAutoSaveTimer, SIGNAL(timeout()), SLOT(autoSaveTimeout()));

    readSettings();

    updateActions();
}

BuildingEditorWindow::~BuildingEditorWindow()
{
    BuildingTemplates::deleteInstance();
    BuildingTilesMgr::deleteInstance(); // Ensure all the tilesets are released
    BuildingTMX::deleteInstance();
    BuildingPreferences::deleteInstance();
    delete ui;
}

void BuildingEditorWindow::showEvent(QShowEvent *event)
{
    if (mPreviewWin)
        mPreviewWin->show();
    event->accept();
}

void BuildingEditorWindow::closeEvent(QCloseEvent *event)
{
    if (confirmAllSave()) {
        clearDocument();
        writeSettings();
        if (mPreviewWin) {
            mPreviewWin->writeSettings();
            mPreviewWin->hide();
        }
        event->accept(); // doesn't destroy us
    } else
        event->ignore();

}

bool BuildingEditorWindow::confirmAllSave()
{
    return confirmSave();
}

// Called by Tiled::Internal::MainWindow::closeEvent
bool BuildingEditorWindow::closeYerself()
{
    if (confirmAllSave()) {
        writeSettings();
        if (mPreviewWin)
            mPreviewWin->writeSettings();
        delete this; // Delete ourself and PreviewWindow.
        return true;
    }
    return false;
}

bool BuildingEditorWindow::Startup()
{
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

    if (!LoadTMXConfig()) {
        if (!mError.isEmpty()) // Empty when user cancelled choosing Tiles directory.
            QMessageBox::critical(this, tr("It's no good, Jim!"), mError);
        return false;
    }

    if (!BuildingTilesMgr::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(BuildingTilesMgr::instance()->txtName())
                              .arg(BuildingTilesMgr::instance()->errorString()));
        return false;
    }

    if (!BuildingTemplates::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(BuildingTemplates::instance()->txtName())
                              .arg(BuildingTemplates::instance()->errorString()));
        return false;
    }

    if (!FurnitureGroups::instance()->readTxt()) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(FurnitureGroups::instance()->txtName())
                              .arg(FurnitureGroups::instance()->errorString()));
        return false;
    }

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

    /////

    // Add tile categories to the gui
    setCategoryList();

    /////

    mPreviewWin = new BuildingPreviewWindow(this);
    mPreviewWin->show();

    return true;
}

bool BuildingEditorWindow::LoadTMXConfig()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = BuildingPreferences::instance()->tilesDirectory();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        QMessageBox::information(this, tr("Choose Tiles Directory"),
                                 tr("Please enter the Tiles directory in the Preferences."));
        BuildingPreferencesDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) {
            mError.clear();
            return false;
        }
    }

    PROGRESS progress(tr("Reading TMXConfig.txt tilesets"), this);

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
    if (!mCurrentDocument)
        return 0;
    if (!currentBuilding()->roomCount())
        return 0;
    int roomIndex = mRoomComboBox->currentIndex();
    if (roomIndex < 0)
        return 0;
    return mCurrentDocument->building()->room(roomIndex);
}

Building *BuildingEditorWindow::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
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
    mView->zoomable()->setScale(mSettings.value(QLatin1String("EditorScale"),
                                                1.0).toReal());
    mSettings.endGroup();

    mCategoryZoomable->setScale(BuildingPreferences::instance()->tileScale());

    restoreSplitterSizes(ui->categorySplitter);
}

void BuildingEditorWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/MainWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.setValue(QLatin1String("EditorScale"), mView->zoomable()->scale());
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
    Q_UNUSED(scale)
}

void BuildingEditorWindow::categorySelectionChanged()
{
    mCategory = 0;
    mFurnitureGroup = 0;

    QList<QListWidgetItem*> selected = ui->categoryList->selectedItems();
    if (selected.count() == 1) {
        int row = ui->categoryList->row(selected.first());
        if (row < BuildingTilesMgr::instance()->categoryCount()) {
            mCategory = BuildingTilesMgr::instance()->category(row);
            QList<Tiled::Tile*> tiles;
            QList<void*> userData;
            if (mCategory->canAssignNone()) {
                tiles += BuildingTilesMgr::instance()->noneTiledTile();
                userData += BuildingTilesMgr::instance()->noneTileEntry();
            }
            QMap<QString,BuildingTileEntry*> entryMap;
            foreach (BuildingTileEntry *entry, mCategory->entries()) {
                QString key = entry->displayTile()->name() + QString::number((qulonglong)entry);
                entryMap[key] = entry;
            }
            foreach (BuildingTileEntry *entry, entryMap.values()) {
                if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
                    tiles += tile;
                    userData += entry;
                }
            }
            ui->tilesetView->model()->setTiles(tiles, userData);
            ui->tilesetView->scrollToTop();
            ui->categoryStack->setCurrentIndex(0);

            selectCurrentCategoryTile();
        } else {
            row -= BuildingTilesMgr::instance()->categoryCount();
            mFurnitureGroup = FurnitureGroups::instance()->group(row);
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
            if (mCategory->asExteriorWalls())
                currentEWallChanged(entry);
            else if (mCategory->asInteriorWalls())
                currentIWallChanged(entry);
            else if (mCategory->asFloors())
                currentFloorChanged(entry);
            else if (mCategory->asDoors())
                currentDoorChanged(entry);
            else if (mCategory->asDoorFrames())
                currentDoorFrameChanged(entry);
            else if (mCategory->asWindows())
                currentWindowChanged(entry);
            else if (mCategory->asCurtains())
                currentCurtainsChanged(entry);
            else if (mCategory->asStairs())
                currentStairsChanged(entry);
            else if (mCategory->asRoofCaps())
                currentRoofTileChanged(entry, RoofObject::TileCap);
            else if (mCategory->asRoofSlopes())
                currentRoofTileChanged(entry, RoofObject::TileSlope);
            else if (mCategory->asRoofTops())
                currentRoofTileChanged(entry, RoofObject::TileTop);
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

void BuildingEditorWindow::currentEWallChanged(BuildingTileEntry *entry)
{
    mCurrentDocument->undoStack()->push(new ChangeEWall(mCurrentDocument, entry));
}

void BuildingEditorWindow::currentIWallChanged(BuildingTileEntry *entry)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeWallForRoom(mCurrentDocument,
                                                              currentRoom(),
                                                              entry));
}

void BuildingEditorWindow::currentFloorChanged(BuildingTileEntry *entry)
{
    if (!currentRoom())
        return;

    mCurrentDocument->undoStack()->push(new ChangeFloorForRoom(mCurrentDocument,
                                                               currentRoom(),
                                                               entry));
}

void BuildingEditorWindow::currentDoorChanged(BuildingTileEntry *entry)
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
                                                                   entry));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentDoorFrameChanged(BuildingTileEntry *entry)
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
                                                                   1));
        if (doors.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentWindowChanged(BuildingTileEntry *entry)
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
                                                                     entry));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentCurtainsChanged(BuildingTileEntry *entry)
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
                                                                     1));
        if (windows.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentStairsChanged(BuildingTileEntry *entry)
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
                                                                     entry));
        if (stairsList.count() > 1)
            mCurrentDocument->undoStack()->endMacro();
    }
}

void BuildingEditorWindow::currentRoofTileChanged(BuildingTileEntry *entry, int which)
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
    if (mCategory->asExteriorWalls())
        currentTile = mCurrentDocument->building()->exteriorWall();
    if (currentRoom() && mCategory->asInteriorWalls())
        currentTile = currentRoom()->Wall;
    if (currentRoom() && mCategory->asFloors())
        currentTile = currentRoom()->Floor;
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
    if (mCategory->asRoofCaps())
        currentTile = mCurrentDocument->building()->roofCapTile();
    if (mCategory->asRoofSlopes())
        currentTile = mCurrentDocument->building()->roofSlopeTile();
    if (mCategory->asRoofTops())
        currentTile = mCurrentDocument->building()->roofTopTile();
    if (currentTile) {
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
        roomEditor->clearDocument();
        mPreviewWin->clearDocument();
        mUndoGroup->removeStack(mCurrentDocument->undoStack());
        delete mCurrentDocument->building();
        delete mCurrentDocument;
        removeAutoSaveFile();
        if (mAutoSaveTimer->isActive())
            mAutoSaveTimer->stop();
        mRoomComboBox->clear();
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
    if (building->roofTopTile()->isNone())
        building->setRoofTopTile(btiles->defaultRoofTopTiles());

    roomEditor->setDocument(mCurrentDocument);

    updateRoomComboBox();

    resizeCoordsLabel();

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

    mPreviewWin->setDocument(currentDocument());

    updateActions();

    if (building->roomCount())
        PencilTool::instance()->makeCurrent();

    updateWindowTitle();
}

void BuildingEditorWindow::clearDocument()
{
    if (mCurrentDocument) {
        roomEditor->clearDocument();
        mPreviewWin->clearDocument();
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

    mPreviewWin->exportTMX(fileName);

    mSettings.setValue(QLatin1String("BuildingEditor/ExportDirectory"),
                       QFileInfo(fileName).absolutePath());
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
                                                                      grid));
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
    undoStack->push(new ResizeBuildingBefore(mCurrentDocument));
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
    undoStack->push(new ResizeBuildingAfter(mCurrentDocument));
    undoStack->endMacro();
}

void BuildingEditorWindow::flipHorizontal()
{
    if (!mCurrentDocument)
        return;

    mCurrentDocument->undoStack()->push(new FlipBuilding(mCurrentDocument, true));
}

void BuildingEditorWindow::flipVertical()
{
    if (!mCurrentDocument)
        return;

    mCurrentDocument->undoStack()->push(new FlipBuilding(mCurrentDocument, false));
}

void BuildingEditorWindow::rotateRight()
{
    if (!mCurrentDocument)
        return;

    mCurrentDocument->undoStack()->push(new RotateBuilding(mCurrentDocument,
                                                           true));
}

void BuildingEditorWindow::rotateLeft()
{
    if (!mCurrentDocument)
        return;

    mCurrentDocument->undoStack()->push(new RotateBuilding(mCurrentDocument,
                                                           false));
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
    BuildingTilesDialog dialog(this);
    dialog.exec();

    if (dialog.changes()) {
        int row = ui->categoryList->currentRow();
        setCategoryList();
        row = qMin(row, ui->categoryList->count() - 1);
        ui->categoryList->setCurrentRow(row);
    }
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

    BuildingTemplates::instance()->addTemplate(btemplate);

    BuildingTemplates::instance()->writeTxt(this);

    templatesDialog();
}

void BuildingEditorWindow::mouseCoordinateChanged(const QPoint &tilePos)
{
    ui->coordLabel->setText(tr("%1,%2").arg(tilePos.x()).arg(tilePos.y()));
}

void BuildingEditorWindow::currentToolChanged(BaseTool *tool)
{
    if (tool)
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
    if (fileName.isEmpty())
        return; // where to save it?
    fileName += QLatin1String(".autosave");
    writeBuilding(mCurrentDocument, fileName);
    mAutoSaveFileName = fileName;
    qDebug() << "BuildingEd auto-saved:" << fileName;
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

    foreach (BuildingTileCategory *category, BuildingTilesMgr::instance()->categories()) {
        ui->categoryList->addItem(category->label());
    }

    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups()) {
        ui->categoryList->addItem(group->mLabel);
    }

    mSynching = false;
}

void BuildingEditorWindow::updateActions()
{
    bool hasDoc = mCurrentDocument != 0;
    bool showObjects = BuildingPreferences::instance()->showObjects();

    PencilTool::instance()->setEnabled(hasDoc &&
            currentRoom() != 0);
    EraserTool::instance()->setEnabled(hasDoc);
    SelectMoveRoomsTool::instance()->setEnabled(hasDoc);
    DoorTool::instance()->setEnabled(hasDoc && showObjects);
    WindowTool::instance()->setEnabled(hasDoc && showObjects);
    StairsTool::instance()->setEnabled(hasDoc && showObjects);
    FurnitureTool::instance()->setEnabled(hasDoc && showObjects &&
            FurnitureTool::instance()->currentTile() != 0);
    bool roofTilesOK = hasDoc && currentBuilding()->roofCapTile()->asRoofCap() &&
            currentBuilding()->roofSlopeTile()->asRoofSlope() /*&&
            currentBuilding()->roofTopTile()->asRoofTop()*/;
    RoofTool::instance()->setEnabled(hasDoc && showObjects && roofTilesOK);
    RoofCornerTool::instance()->setEnabled(hasDoc && showObjects && roofTilesOK);
    SelectMoveObjectTool::instance()->setEnabled(hasDoc && showObjects);

    ui->actionUpLevel->setEnabled(hasDoc &&
                                  !mCurrentDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(hasDoc &&
                                    !mCurrentDocument->currentFloorIsBottom());

//    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(hasDoc);
    ui->actionSaveAs->setEnabled(hasDoc);
    ui->actionExportTMX->setEnabled(hasDoc);

    ui->actionShowObjects->setEnabled(hasDoc);

    ui->actionZoomIn->setEnabled(hasDoc && mView->zoomable()->canZoomIn());
    ui->actionZoomOut->setEnabled(hasDoc && mView->zoomable()->canZoomOut());
    ui->actionNormalSize->setEnabled(hasDoc && mView->zoomable()->scale() != 1.0);

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

    mRoomComboBox->setEnabled(hasDoc && currentRoom() != 0);

    if (mCurrentDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentLevel() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    else
        mFloorLabel->clear();
}
