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

    mFloorLabel = new QToolButton;
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAutoRaise(true);
    mFloorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    mFloorLabel->setToolTip(tr("Click to edit floors"));
    connect(mFloorLabel, SIGNAL(clicked()),
            BuildingEditorWindow::instance(), SLOT(floorsDialog()));

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
        mFloorLabel->setText(QString());
    mFloorLabel->setEnabled(mCurrentDocument != 0);
}

/////

ObjectEditModePerDocumentStuff::ObjectEditModePerDocumentStuff(
        ObjectEditMode *mode, BuildingDocument *doc) :
    QObject(doc),
    mMode(mode),
    mDocument(doc)
{
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
}

void ObjectEditModePerDocumentStuff::activate()
{
    ToolManager::instance()->setEditor(scene());

    connect(view(), SIGNAL(mouseCoordinateChanged(QPoint)),
            mMode->mStatusBar, SLOT(mouseCoordinateChanged(QPoint)));
    connect(zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));

    zoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);

//    connect(document(), SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));

    Ui::BuildingEditorWindow *actions = BuildingEditorWindow::instance()->actionIface();
    connect(actions->actionZoomIn, SIGNAL(triggered()),
            SLOT(zoomIn()));
    connect(actions->actionZoomOut, SIGNAL(triggered()),
            SLOT(zoomOut()));
    connect(actions->actionNormalSize, SIGNAL(triggered()),
            SLOT(zoomNormal()));
}

void ObjectEditModePerDocumentStuff::deactivate()
{
//    document()->disconnect(this);
//    document()->disconnect(mMode);
//    view()->disconnect(this);
    view()->disconnect(mMode->mStatusBar);
    zoomable()->disconnect(this);
//    zoomable()->disconnect(mMode); /////

    Ui::BuildingEditorWindow *actions = BuildingEditorWindow::instance()->actionIface();
    actions->actionZoomIn->disconnect(this);
    actions->actionZoomOut->disconnect(this);
    actions->actionNormalSize->disconnect(this);

    actions->actionZoomIn->setEnabled(false);
    actions->actionZoomOut->setEnabled(false);
    actions->actionNormalSize->setEnabled(false);
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
    scene()->synchObjectItemVisibility();
}

void ObjectEditModePerDocumentStuff::zoomIn()
{
    zoomable()->zoomIn();
}

void ObjectEditModePerDocumentStuff::zoomOut()
{
    zoomable()->zoomOut();
}

void ObjectEditModePerDocumentStuff::zoomNormal()
{
    zoomable()->resetZoom();
}

void ObjectEditModePerDocumentStuff::updateActions()
{
    if (ToolManager::instance()->currentEditor() == scene()) {
        BuildingEditorWindow::instance()->actionIface()->actionZoomIn->setEnabled(zoomable()->canZoomIn());
        BuildingEditorWindow::instance()->actionIface()->actionZoomOut->setEnabled(zoomable()->canZoomOut());
        BuildingEditorWindow::instance()->actionIface()->actionNormalSize->setEnabled(zoomable()->scale() != 1.0);
    }
}

/////

OrthoObjectEditModePerDocumentStuff::OrthoObjectEditModePerDocumentStuff(
        OrthoObjectEditMode *mode, BuildingDocument *doc) :
    ObjectEditModePerDocumentStuff(mode, doc),
    mView(new BuildingOrthoView),
    mScene(new BuildingOrthoScene(mView))
{
    mView->setScene(mScene);
    mView->setDocument(doc);
}

OrthoObjectEditModePerDocumentStuff::~OrthoObjectEditModePerDocumentStuff()
{
    // This is added to a QTabWidget.
    // Removing a tab does not delete the page widget.
    // mScene is owned by the view.
    delete mView;
}

Tiled::Internal::Zoomable *OrthoObjectEditModePerDocumentStuff::zoomable() const
{
    return mView->zoomable();
}

/////

IsoObjectEditModePerDocumentStuff::IsoObjectEditModePerDocumentStuff(
        IsoObjectEditMode *mode, BuildingDocument *doc) :
    ObjectEditModePerDocumentStuff(mode, doc),
    mView(new BuildingIsoView),
    mScene(new BuildingIsoScene(mView))
{
    mView->setScene(mScene);
    mView->setDocument(doc);
}

IsoObjectEditModePerDocumentStuff::~IsoObjectEditModePerDocumentStuff()
{
    // This is added to a QTabWidget.
    // Removing a tab does not delete the page widget.
    // mScene is owned by the view.
    delete mView;
}

Tiled::Internal::Zoomable *IsoObjectEditModePerDocumentStuff::zoomable() const
{
    return mView->zoomable();
}

/////

ObjectEditMode::ObjectEditMode(QObject *parent) :
    IMode(parent),
    mCategoryDock(new CategoryDock),
    mCurrentDocument(0),
    mCurrentDocumentStuff(0)
{
    mMainWindow = new EmbeddedMainWindow;
    mMainWindow->setObjectName(QLatin1String("ObjectEditMode.Widget"));

    mToolBar = new ObjectEditModeToolBar(this);
    mStatusBar = new EditModeStatusBar(QLatin1String("ObjectEditMode.StatusBar."));

    mTabWidget = new QTabWidget;
    mTabWidget->setObjectName(QLatin1String("ObjectEditMode.TabWidget"));
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setObjectName(QLatin1String("ObjectEditMode.VBox"));
    vbox->setMargin(0);
//    vbox->addWidget(mToolBar);
    vbox->addWidget(mTabWidget);
    vbox->addLayout(mStatusBar->statusBarLayout);
    vbox->setStretchFactor(mTabWidget, 1);
    QWidget *w = new QWidget;
    w->setObjectName(QLatin1String("ObjectEditMode.VBoxWidget"));
    w->setLayout(vbox);

    QToolBar *commonToolBar = BuildingEditorWindow::instance()->createCommonToolBar();
    commonToolBar->setObjectName(QLatin1String("ObjectEditMode.CommonToolBar"));

    mMainWindow->setCentralWidget(w);
    mMainWindow->addToolBar(Qt::LeftToolBarArea, commonToolBar);
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

    connect(this, SIGNAL(activeStateChanged(bool)), SLOT(onActiveStateChanged(bool)));
}

Building *ObjectEditMode::currentBuilding() const
{
    return mCurrentDocument ? mCurrentDocument->building() : 0;
}

Room *ObjectEditMode::currentRoom() const
{
    return mCurrentDocument ? mCurrentDocument->currentRoom() : 0;
}

#define WIDGET_STATE_VERSION 0
void ObjectEditMode::readSettings(QSettings &settings)
{
    settings.beginGroup(QString::fromLatin1("BuildingEditor/%1ObjectEditMode").arg(mSettingsPrefix));
    mMainWindow->readSettings(settings);
    mCategoryDock->readSettings(settings);
    settings.endGroup();
}

void ObjectEditMode::writeSettings(QSettings &settings)
{
    settings.beginGroup(QString::fromLatin1("BuildingEditor/%1ObjectEditMode").arg(mSettingsPrefix));
    mMainWindow->writeSettings(settings);
    mCategoryDock->writeSettings(settings);
    settings.endGroup();
}

void ObjectEditMode::onActiveStateChanged(bool active)
{
    QMenu *menu = BuildingEditorWindow::instance()->actionIface()->menuViews;
    menu->clear();

    if (active) {
        if (mCurrentDocumentStuff)
            mCurrentDocumentStuff->activate();

        QMap<QString,QAction*> map;
        foreach (QDockWidget *dockWidget, mMainWindow->dockWidgets()) {
            QAction *action = dockWidget->toggleViewAction();
            map[action->text()] = action;
        }
        foreach (QAction *action, map.values())
            menu->addAction(action);
        menu->addSeparator();
        foreach (QToolBar *toolBar, mMainWindow->toolBars()) {
            menu->addAction(toolBar->toggleViewAction());
        }
    } else {
        if (mCurrentDocumentStuff)
            mCurrentDocumentStuff->deactivate();
    }
}

void ObjectEditMode::documentAdded(BuildingDocument *doc)
{
    mDocumentStuff[doc] = createPerDocumentStuff(doc);

    int docIndex = BuildingDocumentMgr::instance()->indexOf(doc);
    mTabWidget->blockSignals(true);
    mTabWidget->insertTab(docIndex, mDocumentStuff[doc]->view(), doc->displayName());
    mTabWidget->blockSignals(false);
    mDocumentStuff[doc]->updateDocumentTab();
}

void ObjectEditMode::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        if (isActive())
            mCurrentDocumentStuff->deactivate();
    }

    mCurrentDocument = doc;
    mCurrentDocumentStuff = doc ? mDocumentStuff[doc] : 0;

    if (mCurrentDocument) {
        mTabWidget->setCurrentIndex(docman()->indexOf(doc));
        if (isActive())
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

void ObjectEditMode::updateActions()
{
}

/////

OrthoObjectEditMode::OrthoObjectEditMode(QObject *parent) :
    ObjectEditMode(parent)
{
    setDisplayName(tr("Ortho"));
    setIcon(QIcon(QLatin1String(":/BuildingEditor/icons/mode_ortho.png")));
    mSettingsPrefix = QLatin1String("Ortho");
}

ObjectEditModePerDocumentStuff *OrthoObjectEditMode::createPerDocumentStuff(BuildingDocument *doc)
{
    return new OrthoObjectEditModePerDocumentStuff(this, doc);
}

/////

IsoObjectEditMode::IsoObjectEditMode(QObject *parent) :
    ObjectEditMode(parent)
{
    setDisplayName(tr("Iso"));
    setIcon(QIcon(QLatin1String(":/BuildingEditor/icons/mode_iso.png")));
    mSettingsPrefix = QLatin1String("Iso");
}

void IsoObjectEditMode::documentAdded(BuildingDocument *doc)
{
    ObjectEditMode::documentAdded(doc);

    // Hack to keep iso/tile view position + scale synched.
    emit viewAddedForDocument(doc, (BuildingIsoView*)mDocumentStuff[doc]->view());
}

ObjectEditModePerDocumentStuff *IsoObjectEditMode::createPerDocumentStuff(BuildingDocument *doc)
{
    return new IsoObjectEditModePerDocumentStuff(this, doc);
}

