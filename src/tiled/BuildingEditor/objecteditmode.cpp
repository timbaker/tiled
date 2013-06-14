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

#include "objecteditmode.h"
#include "objecteditmode_p.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingeditorwindow.h"
#include "ui_buildingeditorwindow.h"
#include "buildingisoview.h"
#include "buildingorthoview.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtools.h"
#include "categorydock.h"
#include "editmodestatusbar.h"
#include "embeddedmainwindow.h"

#include "zoomable.h"

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QLabel>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>

using namespace BuildingEditor;

#define docman() BuildingDocumentMgr::instance()

/////

ObjectEditModeToolBar::ObjectEditModeToolBar(ObjectEditMode *mode, QWidget *parent) :
    QToolBar(parent),
    mCurrentDocument(0)
{
    setObjectName(QString::fromUtf8("ObjectEditModeToolBar"));
    setWindowTitle(tr("Object ToolBar"));

    Ui::BuildingEditorWindow *actions = BuildingEditorWindow::instance()->actionIface();

    addAction(actions->actionPecil);
    addAction(actions->actionWall);
    addAction(actions->actionSelectRooms);
    addSeparator();
    addAction(actions->actionDoor);
    addAction(actions->actionWindow);
    addAction(actions->actionStairs);
    addAction(actions->actionRoof);
    addAction(actions->actionRoofCorner);
    addAction(actions->actionFurniture);
    addAction(actions->actionSelectObject);
    addAction(actions->actionRooms);
    addAction(actions->actionUpLevel);
    addAction(actions->actionDownLevel);
    addSeparator();

    mRoomComboBox = new QComboBox;
    mRoomComboBox->setIconSize(QSize(20, 20));
    mRoomComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    addWidget(mRoomComboBox);
    addAction(actions->actionRooms);
    connect(mRoomComboBox, SIGNAL(currentIndexChanged(int)),
            SLOT(roomIndexChanged(int)));

    mFloorLabel = new QLabel;
    mFloorLabel->setText(tr("Ground Floor"));
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    addSeparator();
    addWidget(mFloorLabel);
    addAction(actions->actionUpLevel);
    addAction(actions->actionDownLevel);

    /////
    QMenu *roofMenu = new QMenu(this);
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeW.png")),
                        mode->tr("Slope (W)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeN.png")),
                         mode->tr("Slope (N)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeE.png")),
                         mode->tr("Slope (E)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_slopeS.png")),
                         mode->tr("Slope (S)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_peakWE.png")),
                         mode->tr("Peak (Horizontal)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_peakNS.png")),
                         mode->tr("Peak (Vertical)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_roof_flat.png")),
                         mode->tr("Flat Top"));
    connect(roofMenu, SIGNAL(triggered(QAction*)), SLOT(roofTypeChanged(QAction*)));

    QToolButton *button = static_cast<QToolButton*>(widgetForAction(actions->actionRoof));
    button->setMenu(roofMenu);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    /////

    roofMenu = new QMenu(this);
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerNW.png")),
                        mode->tr("Inner (NW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerNE.png")),
                        mode->tr("Inner (NE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerSE.png")),
                        mode->tr("Inner (SE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_innerSW.png")),
                        mode->tr("Inner (SW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerNW.png")),
                        mode->tr("Outer (NW)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerNE.png")),
                        mode->tr("Outer (NE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerSE.png")),
                        mode->tr("Outer (SE)"));
    roofMenu->addAction(QPixmap(QLatin1String(":/BuildingEditor/icons/icon_corner_outerSW.png")),
                        mode->tr("Outer (SW)"));
    connect(roofMenu, SIGNAL(triggered(QAction*)), SLOT(roofCornerTypeChanged(QAction*)));

    button = static_cast<QToolButton*>(widgetForAction(actions->actionRoofCorner));
    button->setMenu(roofMenu);
    button->setPopupMode(QToolButton::MenuButtonPopup);
    /////

    connect(docman(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));
}

Building *ObjectEditModeToolBar::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
}

Room *ObjectEditModeToolBar::currentRoom() const
{
    return mCurrentDocument ? mCurrentDocument->currentRoom() : 0;
}

void ObjectEditModeToolBar::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument)
        mCurrentDocument->disconnect(this);

    mCurrentDocument = doc;

    if (mCurrentDocument) {
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
    }

    updateRoomComboBox();
    updateActions();
}

void ObjectEditModeToolBar::currentRoomChanged()
{
    if (Room *room = currentRoom()) {
        int roomIndex = mCurrentDocument->building()->indexOf(room);
        mRoomComboBox->setCurrentIndex(roomIndex);
    } else
        mRoomComboBox->setCurrentIndex(-1);
}

void ObjectEditModeToolBar::updateRoomComboBox()
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

void ObjectEditModeToolBar::roomIndexChanged(int index)
{
    if (mCurrentDocument)
        mCurrentDocument->setCurrentRoom((index == -1) ? 0 : currentBuilding()->room(index));
}

void ObjectEditModeToolBar::roomAdded(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
    updateActions();
}

void ObjectEditModeToolBar::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
    updateActions();
}

void ObjectEditModeToolBar::roomsReordered()
{
    updateRoomComboBox();
}

void ObjectEditModeToolBar::roomChanged(Room *room)
{
    Q_UNUSED(room)
    updateRoomComboBox();
}

void ObjectEditModeToolBar::roofTypeChanged(QAction *action)
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

    RoofTool::instance()->action()->setIcon(action->icon());

    if (!RoofTool::instance()->isCurrent())
        RoofTool::instance()->makeCurrent();

}

void ObjectEditModeToolBar::roofCornerTypeChanged(QAction *action)
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

    RoofCornerTool::instance()->action()->setIcon(action->icon());

    if (!RoofCornerTool::instance()->isCurrent())
        RoofCornerTool::instance()->makeCurrent();

}

void ObjectEditModeToolBar::updateActions()
{
    bool hasDoc = mCurrentDocument != 0;

    mRoomComboBox->setEnabled(hasDoc && currentRoom() != 0);

    if (mCurrentDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentLevel() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    else
        mFloorLabel->clear();
}

/////

ObjectEditModePerDocumentStuff::ObjectEditModePerDocumentStuff(
        ObjectEditMode *mode, BuildingDocument *doc) :
    QObject(doc),
    mMode(mode),
    mDocument(doc),
    mStackedWidget(new QStackedWidget),
    mOrthoView(new BuildingOrthoView),
    mOrthoScene(new BuildingOrthoScene(mOrthoView)),
    mIsoView(new BuildingIsoView),
    mIsoScene(new BuildingIsoScene(mIsoView)),
    mOrthoMode(false)
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

    connect(BuildingPreferences::instance(), SIGNAL(showObjectsChanged(bool)),
            SLOT(showObjectsChanged()));

    connect(ToolManager::instance(), SIGNAL(currentEditorChanged()),
            SLOT(updateActions()));
}

ObjectEditModePerDocumentStuff::~ObjectEditModePerDocumentStuff()
{
    // This is added to a QTabWidget.
    // Removing a tab does not delete the page widget.
    delete mStackedWidget;
}

QGraphicsView *ObjectEditModePerDocumentStuff::currentView() const
{
    if (isOrtho()) return orthoView();
    return isoView();
}

BuildingBaseScene *ObjectEditModePerDocumentStuff::currentScene() const
{
    if (isOrtho()) return orthoView()->scene();
    return isoView()->scene();
}

Tiled::Internal::Zoomable *ObjectEditModePerDocumentStuff::currentZoomable() const
{
    if (isOrtho()) return orthoView()->zoomable();
    return isoView()->zoomable();
}

void ObjectEditModePerDocumentStuff::activate()
{
    ToolManager::instance()->setEditor(currentScene());

    connect(currentView(), SIGNAL(mouseCoordinateChanged(QPoint)),
            mMode->mStatusBar, SLOT(mouseCoordinateChanged(QPoint)));
    connect(currentZoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));

    currentZoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);

    connect(document(), SIGNAL(roomAdded(Room*)), mMode, SLOT(roomAdded(Room*)));
    connect(document(), SIGNAL(roomRemoved(Room*)), mMode, SLOT(roomRemoved(Room*)));
    connect(document(), SIGNAL(roomsReordered()), mMode, SLOT(roomsReordered()));
    connect(document(), SIGNAL(roomChanged(Room*)), mMode, SLOT(roomChanged(Room*)));

    connect(document(), SIGNAL(floorAdded(BuildingFloor*)),
            mMode, SLOT(updateActions()));
    connect(document(), SIGNAL(floorRemoved(BuildingFloor*)),
            mMode, SLOT(updateActions()));
    connect(document(), SIGNAL(currentFloorChanged()),
            mMode, SLOT(updateActions()));
    connect(document(), SIGNAL(currentLayerChanged()),
            mMode, SLOT(updateActions()));

    connect(document(), SIGNAL(selectedObjectsChanged()),
            mMode, SLOT(updateActions()));
#if 0
    connect(document(), SIGNAL(tileSelectionChanged(QRegion)),
            mMode, SLOT(updateActions()));
    connect(document(), SIGNAL(clipboardTilesChanged()),
            mMode, SLOT(updateActions()));
#endif
//    connect(document(), SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));

    connect(BuildingEditorWindow::instance()->actionIface()->actionZoomIn, SIGNAL(triggered()),
            SLOT(zoomIn()));
    connect(BuildingEditorWindow::instance()->actionIface()->actionZoomOut, SIGNAL(triggered()),
            SLOT(zoomOut()));
    connect(BuildingEditorWindow::instance()->actionIface()->actionNormalSize, SIGNAL(triggered()),
            SLOT(zoomNormal()));
}

void ObjectEditModePerDocumentStuff::deactivate()
{
    document()->disconnect(this);
    document()->disconnect(mMode);
    orthoView()->disconnect(this);
    orthoView()->disconnect(mMode->mStatusBar);
    isoView()->disconnect(this);
    isoView()->disconnect(mMode->mStatusBar);
    currentZoomable()->disconnect(this);
    currentZoomable()->disconnect(mMode); /////
    BuildingEditorWindow::instance()->actionIface()->actionZoomIn->disconnect(this);
    BuildingEditorWindow::instance()->actionIface()->actionZoomOut->disconnect(this);
    BuildingEditorWindow::instance()->actionIface()->actionNormalSize->disconnect(this);
}

void ObjectEditModePerDocumentStuff::toOrtho()
{
    mIsoView->zoomable()->connectToComboBox(0);
    mOrthoView->zoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);
    stackedWidget()->setCurrentIndex(0);
    mOrthoView->setFocus();
    mOrthoMode = true;
    ToolManager::instance()->setEditor(currentScene());
}

void ObjectEditModePerDocumentStuff::toIso()
{
    mOrthoView->zoomable()->connectToComboBox(0);
    mIsoView->zoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);
    stackedWidget()->setCurrentIndex(1);
    mIsoView->setFocus();
    mOrthoMode = false;
    ToolManager::instance()->setEditor(currentScene());
}

void ObjectEditModePerDocumentStuff::updateDocumentTab()
{
    int tabIndex = BuildingDocumentMgr::instance()->indexOf(document());
    if (tabIndex == -1)
        return;

    QString tabText = document()->displayName();
    if (document()->isModified())
        tabText.prepend(QLatin1Char('*'));
    mMode->mTabWidget->setTabText(tabIndex, tabText);

    QString tooltipText = QDir::toNativeSeparators(document()->fileName());
    mMode->mTabWidget->setTabToolTip(tabIndex, tooltipText);
}

void ObjectEditModePerDocumentStuff::showObjectsChanged()
{
    orthoView()->scene()->synchObjectItemVisibility();
    isoView()->scene()->synchObjectItemVisibility();
}

void ObjectEditModePerDocumentStuff::zoomIn()
{
    currentZoomable()->zoomIn();
}

void ObjectEditModePerDocumentStuff::zoomOut()
{
    currentZoomable()->zoomOut();
}

void ObjectEditModePerDocumentStuff::zoomNormal()
{
    currentZoomable()->resetZoom();
}

void ObjectEditModePerDocumentStuff::updateActions()
{
    if (ToolManager::instance()->currentEditor() == currentScene()) {
        Tiled::Internal::Zoomable *zoomable = currentZoomable();
        BuildingEditorWindow::instance()->actionIface()->actionZoomIn->setEnabled(zoomable->canZoomIn());
        BuildingEditorWindow::instance()->actionIface()->actionZoomOut->setEnabled(zoomable->canZoomOut());
        BuildingEditorWindow::instance()->actionIface()->actionNormalSize->setEnabled(zoomable->scale() != 1.0);
    }
}

/////

ObjectEditMode::ObjectEditMode(QObject *parent) :
    IMode(parent),
    mCurrentDocument(0),
    mCurrentDocumentStuff(0),
    mCategoryDock(new CategoryDock)
{
    mMainWindow = new EmbeddedMainWindow;
    mMainWindow->setObjectName(QLatin1String("ObjectEditModeWidget"));

    mToolBar = new ObjectEditModeToolBar(this);
    mStatusBar = new EditModeStatusBar(QLatin1String("ObjectEditModeStatusBar."));

    mTabWidget = new QTabWidget;
    mTabWidget->setObjectName(QLatin1String("ObjectEditModeTabWidget"));
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setObjectName(QLatin1String("ObjectEditModeVBox"));
    vbox->setMargin(0);
//    vbox->addWidget(mToolBar);
    vbox->addWidget(mTabWidget);
    vbox->addLayout(mStatusBar->statusBarLayout);
    vbox->setStretchFactor(mTabWidget, 1);
    QWidget *w = new QWidget;
    w->setObjectName(QLatin1String("ObjectEditModeVBoxWidget"));
    w->setLayout(vbox);

    mMainWindow->setCentralWidget(w);
    mMainWindow->addToolBar(mToolBar);
    mMainWindow->registerDockWidget(mCategoryDock);
    mMainWindow->addDockWidget(Qt::RightDockWidgetArea, mCategoryDock);

    setWidget(mMainWindow);

    connect(mTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentDocumentTabChanged(int)));
    connect(mTabWidget, SIGNAL(tabCloseRequested(int)),
            SLOT(documentTabCloseRequested(int)));

    connect(BuildingDocumentMgr::instance(), SIGNAL(documentAdded(BuildingDocument*)),
            SLOT(documentAdded(BuildingDocument*)));
    connect(BuildingDocumentMgr::instance(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));
    connect(BuildingDocumentMgr::instance(), SIGNAL(documentAboutToClose(int,BuildingDocument*)),
            SLOT(documentAboutToClose(int,BuildingDocument*)));
}

Building *ObjectEditMode::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
}

Room *ObjectEditMode::currentRoom() const
{
    return mCurrentDocument ? mCurrentDocument->currentRoom() : 0;
}

void ObjectEditMode::toOrtho()
{
    if (mCurrentDocumentStuff)
        mCurrentDocumentStuff->toOrtho();
}

void ObjectEditMode::toIso()
{
    if (mCurrentDocumentStuff)
        mCurrentDocumentStuff->toIso();
}

#define WIDGET_STATE_VERSION 0
void ObjectEditMode::readSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("BuildingEditor/ObjectEditMode"));
    QByteArray state = settings.value(QLatin1String("state")).toByteArray();
    mMainWindow->restoreState(state, WIDGET_STATE_VERSION);
    settings.endGroup();

    mCategoryDock->readSettings(settings);
}

void ObjectEditMode::writeSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("BuildingEditor/ObjectEditMode"));
    settings.setValue(QLatin1String("state"), mMainWindow->saveState(WIDGET_STATE_VERSION));
    settings.endGroup();

    mCategoryDock->writeSettings(settings);
}

void ObjectEditMode::documentAdded(BuildingDocument *doc)
{
    mDocumentStuff[doc] = new ObjectEditModePerDocumentStuff(this, doc);

    int docIndex = BuildingDocumentMgr::instance()->indexOf(doc);
    mTabWidget->blockSignals(true);
    mTabWidget->insertTab(docIndex, mDocumentStuff[doc]->stackedWidget(), doc->displayName());
    mTabWidget->blockSignals(false);
    mDocumentStuff[doc]->updateDocumentTab();
}

void ObjectEditMode::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        mCurrentDocumentStuff->deactivate();
    }

    mCurrentDocument = doc;
    mCurrentDocumentStuff = doc ? mDocumentStuff[doc] : 0;

    if (mCurrentDocument) {
        mTabWidget->setCurrentIndex(docman()->indexOf(doc));
        mCurrentDocumentStuff->activate();
    }
}

void ObjectEditMode::documentAboutToClose(int index, BuildingDocument *doc)
{
    Q_UNUSED(doc)
    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
    mTabWidget->removeTab(index);
}

void ObjectEditMode::currentDocumentTabChanged(int index)
{
    docman()->setCurrentDocument(index);
}

void ObjectEditMode::documentTabCloseRequested(int index)
{
    BuildingEditorWindow::instance()->documentTabCloseRequested(index);
}


void ObjectEditMode::roomAdded(Room *room)
{
    Q_UNUSED(room)
    updateActions();
}

void ObjectEditMode::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    updateActions();
}

void ObjectEditMode::roomsReordered()
{
}

void ObjectEditMode::roomChanged(Room *room)
{
    Q_UNUSED(room)
}

void ObjectEditMode::updateActions()
{
}

