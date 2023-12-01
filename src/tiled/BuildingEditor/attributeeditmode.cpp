/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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


#include "attributeeditmode.h"
#include "attributeeditmode_p.h"

#include "building.h"
#include "buildingattributesdock.h"
#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingisoview.h"
#include "buildinglayersdock.h"
#include "editmodestatusbar.h"
#include "embeddedmainwindow.h"
#include "ui_buildingeditorwindow.h"

#include <QDockWidget>
#include <QMenu>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>

#define docman() BuildingDocumentMgr::instance()

using namespace BuildingEditor;

AttributeEditModeToolBar::AttributeEditModeToolBar(QWidget *parent) :
    QToolBar(parent),
    mCurrentDocument(nullptr)
{
    setObjectName(QString::fromUtf8("AttributeEditModeToolBar"));
    setWindowTitle(tr("Attribute ToolBar"));

    mFloorLabel = new QToolButton;
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAutoRaise(true);
    mFloorLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    mFloorLabel->setToolTip(tr("Click to edit floors"));
    connect(mFloorLabel, &QAbstractButton::clicked,
            BuildingEditorWindow::instance(), &BuildingEditorWindow::floorsDialog);

//    addAction(BuildingEditorWindow::instance()->actionIface()->actionDrawTiles);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionSelectTiles);
//    addAction(BuildingEditorWindow::instance()->actionIface()->actionPickTiles);
    addSeparator();
    addWidget(mFloorLabel);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionUpLevel);
    addAction(BuildingEditorWindow::instance()->actionIface()->actionDownLevel);

    connect(docman(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &AttributeEditModeToolBar::currentDocumentChanged);
}

void AttributeEditModeToolBar::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument)
        mCurrentDocument->disconnect(this);

    mCurrentDocument = doc;

    if (mCurrentDocument) {
        connect(mCurrentDocument, &BuildingDocument::floorAdded,
                this, &AttributeEditModeToolBar::updateActions);
        connect(mCurrentDocument, &BuildingDocument::floorRemoved,
                this, &AttributeEditModeToolBar::updateActions);
        connect(mCurrentDocument, &BuildingDocument::currentFloorChanged,
                this, &AttributeEditModeToolBar::updateActions);
    }

    updateActions();
}

void AttributeEditModeToolBar::updateActions()
{
    if (mCurrentDocument) {
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mCurrentDocument->currentLevel() + 1)
                             .arg(mCurrentDocument->building()->floorCount()));
    } else {
        mFloorLabel->setText(QString());
    }
    mFloorLabel->setEnabled(mCurrentDocument != 0);
}

/////

AttributeEditMode::AttributeEditMode(QObject *parent) :
    IMode(parent),
    mTabWidget(new QTabWidget),
    mStatusBar(new EditModeStatusBar(QLatin1String("AttributeEditModeStatusBar."))),
    mToolBar(new AttributeEditModeToolBar),
    mAttributesDock(new BuildingAttributesDock),
    mLayersDock(new BuildingLayersDock),
    mCurrentDocument(nullptr),
    mCurrentDocumentStuff(nullptr)
{
    setDisplayName(tr("Properties"));
    setIcon(QIcon(QLatin1String(":/BuildingEditor/icons/mode_attributes.png")));

    mMainWindow = new EmbeddedMainWindow;
    mMainWindow->setObjectName(QString::fromUtf8("AttributeEditMode.Widget"));

    mTabWidget->setObjectName(QString::fromUtf8("AttributeEditMode.TabWidget"));
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    mStatusBar->setObjectName(QLatin1String("AttributeEditMode.StatusBar"));

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->setObjectName(QLatin1String("AttributeEditMode.VBox"));
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(mTabWidget);
    vbox->setStretchFactor(mTabWidget, 1);
    vbox->addLayout(mStatusBar->statusBarLayout);
    QWidget *w = new QWidget;
    w->setObjectName(QString::fromUtf8("AttributeEditMode.VBoxWidget"));
    w->setLayout(vbox);

    QToolBar *commonToolBar = BuildingEditorWindow::instance()->createCommonToolBar();
    commonToolBar->setObjectName(QLatin1String("AttributeEditMode.CommonToolBar"));

    mMainWindow->setCentralWidget(w);
    mMainWindow->addToolBar(Qt::LeftToolBarArea, commonToolBar);
    mMainWindow->addToolBar(mToolBar);
    mMainWindow->registerDockWidget(mAttributesDock),
    mMainWindow->registerDockWidget(mLayersDock);
    mMainWindow->addDockWidget(Qt::RightDockWidgetArea, mLayersDock);
    mMainWindow->addDockWidget(Qt::RightDockWidgetArea, mAttributesDock);

    connect(mTabWidget, &QTabWidget::currentChanged,
            this, &AttributeEditMode::currentDocumentTabChanged);
    connect(mTabWidget, &QTabWidget::tabCloseRequested,
            this, &AttributeEditMode::documentTabCloseRequested);

    setWidget(mMainWindow);

    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::documentAdded,
            this, &AttributeEditMode::documentAdded);
    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::currentDocumentChanged,
            this, &AttributeEditMode::currentDocumentChanged);
    connect(BuildingDocumentMgr::instance(), &BuildingDocumentMgr::documentAboutToClose,
            this, &AttributeEditMode::documentAboutToClose);

    connect(this, &IMode::activeStateChanged, this, &AttributeEditMode::onActiveStateChanged);
}

void AttributeEditMode::readSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("AttributeEditMode"));
    mMainWindow->readSettings(settings);
    settings.endGroup();
}

void AttributeEditMode::writeSettings(QSettings &settings)
{
    settings.beginGroup(QLatin1String("AttributeEditMode"));
    mMainWindow->writeSettings(settings);
    settings.endGroup();
}

void AttributeEditMode::onActiveStateChanged(bool active)
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
        foreach (QToolBar *toolBar, mMainWindow->toolBars())
            menu->addAction(toolBar->toggleViewAction());
    } else {
        if (mCurrentDocumentStuff)
            mCurrentDocumentStuff->deactivate();
    }
}

void AttributeEditMode::documentAdded(BuildingDocument *doc)
{
    mDocumentStuff[doc] = new AttributeEditModePerDocumentStuff(this, doc);

    int docIndex = BuildingDocumentMgr::instance()->indexOf(doc);
    mTabWidget->blockSignals(true);
    mTabWidget->insertTab(docIndex, mDocumentStuff[doc]->view(), doc->displayName());
    mTabWidget->blockSignals(false);
    mDocumentStuff[doc]->updateDocumentTab();

    // Hack to keep iso/tile view position + scale synched.
    emit viewAddedForDocument(doc, mDocumentStuff[doc]->view());
}

void AttributeEditMode::currentDocumentChanged(BuildingDocument *doc)
{
    if (mCurrentDocument) {
        if (isActive())
            mCurrentDocumentStuff->deactivate();
    }

    mCurrentDocument = doc;
    mCurrentDocumentStuff = doc ? mDocumentStuff[doc] : nullptr;

    if (mCurrentDocument) {
        mTabWidget->setCurrentIndex(docman()->indexOf(doc));
        if (isActive())
            mCurrentDocumentStuff->activate();
    }
}

void AttributeEditMode::documentAboutToClose(int index, BuildingDocument *doc)
{
    Q_UNUSED(doc)
    // At this point, the document is not in the DocumentManager's list of documents.
    // Removing the current tab will cause another tab to be selected and
    // the current document to change.
    mTabWidget->removeTab(index);
}

void AttributeEditMode::currentDocumentTabChanged(int index)
{
    docman()->setCurrentDocument(index);
}

void AttributeEditMode::documentTabCloseRequested(int index)
{
    BuildingEditorWindow::instance()->documentTabCloseRequested(index);
}

void AttributeEditMode::updateActions()
{

}
