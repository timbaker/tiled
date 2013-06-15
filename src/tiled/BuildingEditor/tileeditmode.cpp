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

#include "tileeditmode.h"
#include "tileeditmode_p.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingeditorwindow.h"
#include "ui_buildingeditorwindow.h"
#include "buildingfurnituredock.h"
#include "buildingisoview.h"
#include "buildinglayersdock.h"
#include "buildingtemplates.h"
#include "buildingtilesetdock.h"
#include "buildingtiletools.h"
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
#include <QUndoStack>
#include <QVBoxLayout>

#define docman() BuildingDocumentMgr::instance()

using namespace BuildingEditor;

/////

TileEditModeToolBar::TileEditModeToolBar(QWidget *parent) :
    QToolBar(parent),
    mCurrentDocument(0),
    mFloorLabel(new QLabel)
{
    setObjectName(QString::fromUtf8("TileEditModeToolBar"));
    setWindowTitle(tr("Tile ToolBar"));

    mFloorLabel->setText(tr("Ground Floor"));
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    addAction(BuildingEditorWindow::instance()->actionIface()->actionDrawTiles);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionSelectTiles);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionPickTiles);
    addSeparator();
    addWidget(mFloorLabel);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionUpLevel);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionDownLevel);

    connect(docman(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));
}

void TileEditModeToolBar::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument)
        mCurrentDocument->disconnect(this);

    mCurrentDocument = doc;

    if (mCurrentDocument) {
        connect(mCurrentDocument, SIGNAL(floorAdded(BuildingFloor*)),
                SLOT(updateActions()));
        connect(mCurrentDocument, SIGNAL(floorRemoved(BuildingFloor*)),
                SLOT(updateActions()));
        connect(mCurrentDocument, SIGNAL(currentFloorChanged()),
                SLOT(updateActions()));
    }

    updateActions();
}

void TileEditModeToolBar::updateActions()
{
//    bool hasDoc = mCurrentDocument != 0;

    if (mCurrentDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentLevel() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    else
        mFloorLabel->clear();
}

/////

TileEditModePerDocumentStuff::TileEditModePerDocumentStuff(
        TileEditMode *mode, BuildingDocument *doc) :
    QObject(doc),
    mMode(mode),
    mDocument(doc),
    mIsoView(new BuildingIsoView),
    mIsoScene(new BuildingIsoScene(mIsoView))
{
    mIsoScene->setEditingTiles(true);
    mIsoView->setScene(mIsoScene);
    mIsoView->setDocument(document());

    connect(document(), SIGNAL(fileNameChanged()), SLOT(updateDocumentTab()));
    connect(document(), SIGNAL(cleanChanged()), SLOT(updateDocumentTab()));
    connect(document()->undoStack(), SIGNAL(cleanChanged(bool)), SLOT(updateDocumentTab()));

    connect(ToolManager::instance(), SIGNAL(currentEditorChanged()),
            SLOT(updateActions()));
}

TileEditModePerDocumentStuff::~TileEditModePerDocumentStuff()
{
    // This is added to a QTabWidget.
    // Removing a tab does not delete the page widget.
    // mIsoScene is a child of the view.
    delete mIsoView;
}

Tiled::Internal::Zoomable *TileEditModePerDocumentStuff::zoomable() const
{
    return mIsoView->zoomable();
}

void TileEditModePerDocumentStuff::activate()
{
    ToolManager::instance()->setEditor(scene());

    connect(view(), SIGNAL(mouseCoordinateChanged(QPoint)),
            mMode->mStatusBar, SLOT(mouseCoordinateChanged(QPoint)));
    connect(zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));

    zoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);

    connect(document(), SIGNAL(tileSelectionChanged(QRegion)),
            SLOT(updateActions()));
    connect(document(), SIGNAL(clipboardTilesChanged()),
            SLOT(updateActions()));

//    connect(document(), SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));

    connect(BuildingEditorWindow::instance()->actionIface()->actionZoomIn, SIGNAL(triggered()),
            SLOT(zoomIn()));
    connect(BuildingEditorWindow::instance()->actionIface()->actionZoomOut, SIGNAL(triggered()),
            SLOT(zoomOut()));
    connect(BuildingEditorWindow::instance()->actionIface()->actionNormalSize, SIGNAL(triggered()),
            SLOT(zoomNormal()));
}

void TileEditModePerDocumentStuff::deactivate()
{
//    document()->disconnect(this);
//    document()->disconnect(mMode); /////
    view()->disconnect(mMode->mStatusBar);
    view()->disconnect(this);
    zoomable()->disconnect(this);
    BuildingEditorWindow::instance()->actionIface()->actionZoomIn->disconnect(this);
    BuildingEditorWindow::instance()->actionIface()->actionZoomOut->disconnect(this);
    BuildingEditorWindow::instance()->actionIface()->actionNormalSize->disconnect(this);
}

void TileEditModePerDocumentStuff::updateDocumentTab()
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


void TileEditModePerDocumentStuff::zoomIn()
{
    zoomable()->zoomIn();
}

void TileEditModePerDocumentStuff::zoomOut()
{
    zoomable()->zoomOut();
}

void TileEditModePerDocumentStuff::zoomNormal()
{
    zoomable()->resetZoom();
}

void TileEditModePerDocumentStuff::updateActions()
{
    if (ToolManager::instance()->currentEditor() == scene()) {
        BuildingEditorWindow::instance()->actionIface()->actionZoomIn->setEnabled(zoomable()->canZoomIn());
        BuildingEditorWindow::instance()->actionIface()->actionZoomOut->setEnabled(zoomable()->canZoomOut());
        BuildingEditorWindow::instance()->actionIface()->actionNormalSize->setEnabled(zoomable()->scale() != 1.0);
    }
}

/////

TileEditMode::TileEditMode(QObject *parent) :
    IMode(parent),
    mCurrentDocument(0),
    mCurrentDocumentStuff(0),
    mTabWidget(new QTabWidget),
    mStatusBar(new EditModeStatusBar(QLatin1String("TileEditModeStatusBar."))),
    mToolBar(new TileEditModeToolBar),
    mFurnitureDock(new BuildingFurnitureDock),
    mLayersDock(new BuildingLayersDock),
    mTilesetDock(new BuildingTilesetDock),
    mFirstTimeSeen(true)
{
    setDisplayName(tr("Tile"));

    mMainWindow = new EmbeddedMainWindow;
    mMainWindow->setObjectName(QString::fromUtf8("TileEditModeWidget"));

    mTabWidget->setObjectName(QString::fromUtf8("TileEditModeTabWidget"));
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    mStatusBar->setObjectName(QLatin1String("TileEditModeStatusBar"));

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setObjectName(QLatin1String("TileEditModeVBox"));
    vbox->setMargin(0);
//    vbox->addWidget(mToolBar);
    vbox->addWidget(mTabWidget);
    vbox->setStretchFactor(mTabWidget, 1);
    vbox->addLayout(mStatusBar->statusBarLayout);
    QWidget *w = new QWidget;
    w->setObjectName(QString::fromUtf8("TileEditModeVBoxWidget"));
    w->setLayout(vbox);

    mMainWindow->setCentralWidget(w);
    mMainWindow->addToolBar(mToolBar);
    mMainWindow->registerDockWidget(mLayersDock);
    mMainWindow->registerDockWidget(mTilesetDock);
    mMainWindow->registerDockWidget(mFurnitureDock);
    mMainWindow->addDockWidget(Qt::RightDockWidgetArea, mLayersDock);
    mMainWindow->addDockWidget(Qt::RightDockWidgetArea, mTilesetDock);
    mMainWindow->tabifyDockWidget(mTilesetDock, mFurnitureDock);

    connect(mTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentDocumentTabChanged(int)));
    connect(mTabWidget, SIGNAL(tabCloseRequested(int)),
            SLOT(documentTabCloseRequested(int)));

    if (mFirstTimeSeen) {
        mFirstTimeSeen = false;
        mFurnitureDock->switchTo();
        mTilesetDock->firstTimeSetup();
    }

    setWidget(mMainWindow);

    connect(BuildingDocumentMgr::instance(), SIGNAL(documentAdded(BuildingDocument*)),
            SLOT(documentAdded(BuildingDocument*)));
    connect(BuildingDocumentMgr::instance(), SIGNAL(currentDocumentChanged(BuildingDocument*)),
            SLOT(currentDocumentChanged(BuildingDocument*)));
    connect(BuildingDocumentMgr::instance(), SIGNAL(documentAboutToClose(int,BuildingDocument*)),
            SLOT(documentAboutToClose(int,BuildingDocument*)));

    connect(this, SIGNAL(activeStateChanged(bool)), SLOT(onActiveStateChanged(bool)));
}

#define WIDGET_STATE_VERSION 0
void TileEditMode::readSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("BuildingEditor/TileEditMode"));
    mMainWindow->readSettings(settings);
    settings.endGroup();
}

void TileEditMode::writeSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("BuildingEditor/TileEditMode"));
    mMainWindow->writeSettings(settings);
    settings.endGroup();
}

void TileEditMode::onActiveStateChanged(bool active)
{
    if (active) {
        if (mCurrentDocumentStuff)
            mCurrentDocumentStuff->activate();
    } else {
        if (mCurrentDocumentStuff)
            mCurrentDocumentStuff->deactivate();
    }
}

void TileEditMode::documentAdded(BuildingDocument *doc)
{
    mDocumentStuff[doc] = new TileEditModePerDocumentStuff(this, doc);

    int docIndex = BuildingDocumentMgr::instance()->indexOf(doc);
    mTabWidget->blockSignals(true);
    mTabWidget->insertTab(docIndex, mDocumentStuff[doc]->view(), doc->displayName());
    mTabWidget->blockSignals(false);
    mDocumentStuff[doc]->updateDocumentTab();
}

void TileEditMode::currentDocumentChanged(BuildingDocument *doc)
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

void TileEditMode::documentAboutToClose(int index, BuildingDocument *doc)
{
    Q_UNUSED(doc)
    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
    mTabWidget->removeTab(index);
}

void TileEditMode::currentDocumentTabChanged(int index)
{
    docman()->setCurrentDocument(index);
}

void TileEditMode::documentTabCloseRequested(int index)
{
    BuildingEditorWindow::instance()->documentTabCloseRequested(index);
}

void TileEditMode::updateActions()
{
}

