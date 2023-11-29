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

#include "attributeeditmode_p.h"

#include "attributeeditmode.h"

#include "buildingdocument.h"
#include "buildingdocumentmgr.h"
#include "buildingeditorwindow.h"
#include "buildingisoview.h"
#include "buildingtiletools.h"
#include "editmodestatusbar.h"
#include "ui_buildingeditorwindow.h"
#include "zoomable.h"

#include <QDir>
#include <QUndoStack>

using namespace BuildingEditor;

AttributeEditModePerDocumentStuff::AttributeEditModePerDocumentStuff(AttributeEditMode *mode, BuildingDocument *doc) :
    QObject(doc),
    mMode(mode),
    mDocument(doc),
    mIsoView(new BuildingIsoView),
    mIsoScene(new BuildingIsoScene(mIsoView))
{
    mIsoScene->setEditingTiles(true);
    mIsoScene->setEditingAttributes(true);
    mIsoView->setScene(mIsoScene);
    mIsoView->setDocument(document());

    connect(document(), &BuildingDocument::fileNameChanged, this, &AttributeEditModePerDocumentStuff::updateDocumentTab);
    connect(document(), &BuildingDocument::cleanChanged, this, &AttributeEditModePerDocumentStuff::updateDocumentTab);
    connect(document()->undoStack(), &QUndoStack::cleanChanged, this, &AttributeEditModePerDocumentStuff::updateDocumentTab);

    connect(ToolManager::instance(), &ToolManager::currentEditorChanged,
            this, &AttributeEditModePerDocumentStuff::updateActions);
}

AttributeEditModePerDocumentStuff::~AttributeEditModePerDocumentStuff()
{
    // This is added to a QTabWidget.
    // Removing a tab does not delete the page widget.
    // mIsoScene is a child of the view.
    delete mIsoView;
}

Tiled::Internal::Zoomable *AttributeEditModePerDocumentStuff::zoomable() const
{
    return mIsoView->zoomable();
}

void AttributeEditModePerDocumentStuff::activate()
{
    ToolManager::instance()->setEditor(scene());

    connect(view(), &BuildingIsoView::mouseCoordinateChanged,
            mMode->mStatusBar, &EditModeStatusBar::mouseCoordinateChanged);
    connect(zoomable(), &Tiled::Internal::Zoomable::scaleChanged,
            this, &AttributeEditModePerDocumentStuff::updateActions);

    zoomable()->connectToComboBox(mMode->mStatusBar->editorScaleComboBox);

    connect(document(), &BuildingDocument::tileSelectionChanged,
            this, &AttributeEditModePerDocumentStuff::updateActions);
    connect(document(), &BuildingDocument::clipboardTilesChanged,
            this, &AttributeEditModePerDocumentStuff::updateActions);

//    connect(document(), SIGNAL(cleanChanged()), SLOT(updateWindowTitle()));

    Ui::BuildingEditorWindow *actions = BuildingEditorWindow::instance()->actionIface();
    connect(actions->actionZoomIn, &QAction::triggered,
            this, &AttributeEditModePerDocumentStuff::zoomIn);
    connect(actions->actionZoomOut, &QAction::triggered,
            this, &AttributeEditModePerDocumentStuff::zoomOut);
    connect(actions->actionNormalSize, &QAction::triggered,
            this, &AttributeEditModePerDocumentStuff::zoomNormal);
}

void AttributeEditModePerDocumentStuff::deactivate()
{
//    document()->disconnect(this);
//    document()->disconnect(mMode); /////
    view()->disconnect(mMode->mStatusBar);
    view()->disconnect(this);
    zoomable()->disconnect(this);

    Ui::BuildingEditorWindow *actions = BuildingEditorWindow::instance()->actionIface();
    actions->actionZoomIn->disconnect(this);
    actions->actionZoomOut->disconnect(this);
    actions->actionNormalSize->disconnect(this);

    actions->actionZoomIn->setEnabled(false);
    actions->actionZoomOut->setEnabled(false);
    actions->actionNormalSize->setEnabled(false);
}

void AttributeEditModePerDocumentStuff::updateDocumentTab()
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


void AttributeEditModePerDocumentStuff::zoomIn()
{
    zoomable()->zoomIn();
}

void AttributeEditModePerDocumentStuff::zoomOut()
{
    zoomable()->zoomOut();
}

void AttributeEditModePerDocumentStuff::zoomNormal()
{
    zoomable()->resetZoom();
}

void AttributeEditModePerDocumentStuff::updateActions()
{
    if (ToolManager::instance()->currentEditor() == scene()) {
        auto actions = BuildingEditorWindow::instance()->actionIface();
        actions->actionZoomIn->setEnabled(zoomable()->canZoomIn());
        actions->actionZoomOut->setEnabled(zoomable()->canZoomOut());
        actions->actionNormalSize->setEnabled(zoomable()->scale() != 1.0);
    }
}
