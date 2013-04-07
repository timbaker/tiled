/*
 * documentmanager.cpp
 * Copyright 2010, Stefan Beller <stefanbeller@googlemail.com>
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "documentmanager.h"

#ifdef ZOMBOID
#include "tilelayerspanel.h"
#include "ZomboidScene.h"
#include <QHBoxLayout>
#include <QDir>
#endif

#include "abstracttool.h"
#include "maprenderer.h"
#include "toolmanager.h"

#include <QTabWidget>
#include <QUndoGroup>
#include <QFileInfo>

using namespace Tiled;
using namespace Tiled::Internal;

DocumentManager *DocumentManager::mInstance = 0;

DocumentManager *DocumentManager::instance()
{
    if (!mInstance)
        mInstance = new DocumentManager;
    return mInstance;
}

void DocumentManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent)
    , mTabWidget(new QTabWidget)
    , mUndoGroup(new QUndoGroup(this))
    , mSelectedTool(0)
    , mSceneWithTool(0)
{
#ifdef ZOMBOID
    mTabWidget->setObjectName(QLatin1String("documentTabs"));
#endif
    mTabWidget->setDocumentMode(true);
    mTabWidget->setTabsClosable(true);

    connect(mTabWidget, SIGNAL(currentChanged(int)),
            SLOT(currentIndexChanged()));
    connect(mTabWidget, SIGNAL(tabCloseRequested(int)),
            SIGNAL(documentCloseRequested(int)));

    ToolManager *toolManager = ToolManager::instance();
    setSelectedTool(toolManager->selectedTool());
    connect(toolManager, SIGNAL(selectedToolChanged(AbstractTool*)),
            SLOT(setSelectedTool(AbstractTool*)));
}

DocumentManager::~DocumentManager()
{
    // All documents should be closed gracefully beforehand
    Q_ASSERT(mDocuments.isEmpty());
    delete mTabWidget;
}

QWidget *DocumentManager::widget() const
{
    return mTabWidget;
}

MapDocument *DocumentManager::currentDocument() const
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return 0;

    return mDocuments.at(index);
}

MapView *DocumentManager::currentMapView() const
{
#ifdef ZOMBOID
    return mDocuments.size() ? mMapViews[currentDocument()] : 0;
#else
    return static_cast<MapView*>(mTabWidget->currentWidget());
#endif
}

MapScene *DocumentManager::currentMapScene() const
{
    if (MapView *mapView = currentMapView())
        return mapView->mapScene();

    return 0;
}

int DocumentManager::findDocument(const QString &fileName) const
{
    const QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();
    if (canonicalFilePath.isEmpty()) // file doesn't exist
        return -1;

    for (int i = 0; i < mDocuments.size(); ++i) {
        QFileInfo fileInfo(mDocuments.at(i)->fileName());
        if (fileInfo.canonicalFilePath() == canonicalFilePath)
            return i;
    }

    return -1;
}

void DocumentManager::switchToDocument(int index)
{
    mTabWidget->setCurrentIndex(index);
}

void DocumentManager::switchToLeftDocument()
{
    const int tabCount = mTabWidget->count();
    if (tabCount < 2)
        return;

    const int currentIndex = mTabWidget->currentIndex();
    switchToDocument((currentIndex > 0 ? currentIndex : tabCount) - 1);
}

void DocumentManager::switchToRightDocument()
{
    const int tabCount = mTabWidget->count();
    if (tabCount < 2)
        return;

    const int currentIndex = mTabWidget->currentIndex();
    switchToDocument((currentIndex + 1) % tabCount);
}

#ifndef ZOMBOID
void DocumentManager::addDocument(MapDocument *mapDocument)
{
    Q_ASSERT(mapDocument);
    Q_ASSERT(!mDocuments.contains(mapDocument));

    mDocuments.append(mapDocument);
    mUndoGroup->addStack(mapDocument->undoStack());

    MapView *view = new MapView(mTabWidget);
#ifdef ZOMBOID
    MapScene *scene = new ZomboidScene(view); // scene is owned by the view
#else
    MapScene *scene = new MapScene(view); // scene is owned by the view
#endif

    scene->setMapDocument(mapDocument);
#ifdef ZOMBOID
    view->setMapScene(scene);
#else
    view->setScene(scene);
#endif

    const int documentIndex = mDocuments.size() - 1;

    mTabWidget->addTab(view, mapDocument->displayName());
#ifdef ZOMBOID
    mTabWidget->setTabToolTip(documentIndex, QDir::toNativeSeparators(
                                  mapDocument->fileName()));
#else
    mTabWidget->setTabToolTip(documentIndex, mapDocument->fileName());
#endif
    connect(mapDocument, SIGNAL(fileNameChanged()), SLOT(updateDocumentTab()));
    connect(mapDocument, SIGNAL(modifiedChanged()), SLOT(updateDocumentTab()));

    switchToDocument(documentIndex);
    centerViewOn(0, 0);
}
#else
void DocumentManager::addDocument(MapDocument *mapDocument)
{
    Q_ASSERT(mapDocument);
    Q_ASSERT(!mDocuments.contains(mapDocument));

    mDocuments.append(mapDocument);
    mUndoGroup->addStack(mapDocument->undoStack());

    QWidget *widget = new QWidget(mTabWidget);
    TileLayersPanel *tlPanel = new TileLayersPanel(widget);
    MapView *view = new MapView(widget);
    MapScene *scene = new ZomboidScene(view); // scene is owned by the view

    QHBoxLayout *layout = new QHBoxLayout();
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(tlPanel);
    layout->addWidget(view);
    widget->setLayout(layout);

    scene->setMapDocument(mapDocument);
    view->setMapScene(scene);
    tlPanel->setDocument(mapDocument);

    connect(tlPanel, SIGNAL(tilePicked(Tile*)), SIGNAL(tilePicked(Tile*)));

    const int documentIndex = mDocuments.size() - 1;

    mTabWidget->addTab(widget, mapDocument->displayName());
    mTabWidget->setTabToolTip(documentIndex, QDir::toNativeSeparators(
                                  mapDocument->fileName()));

    mMapViews[mapDocument] = view;
    mTileLayerPanels[mapDocument] = tlPanel;

    connect(mapDocument, SIGNAL(fileNameChanged()), SLOT(updateDocumentTab()));
    connect(mapDocument, SIGNAL(modifiedChanged()), SLOT(updateDocumentTab()));

    switchToDocument(documentIndex);
    centerViewOn(0, 0);
}
#endif // ZOMBOID

void DocumentManager::closeCurrentDocument()
{
    const int index = mTabWidget->currentIndex();
    if (index == -1)
        return;

    MapDocument *mapDocument = mDocuments.takeAt(index);
#ifdef ZOMBOID
    MapView *mapView = mMapViews[mapDocument];
#else
    MapView *mapView = currentMapView();
#endif

    mTabWidget->removeTab(index);
#ifdef ZOMBOID
    emit documentAboutToClose(index, mapDocument);
    mMapViews.remove(mapDocument);
    mTileLayerPanels.remove(mapDocument);
#endif
    delete mapView;
    delete mapDocument;
}

void DocumentManager::closeAllDocuments()
{
    while (!mDocuments.isEmpty())
        closeCurrentDocument();
}

void DocumentManager::currentIndexChanged()
{
    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();
        mSceneWithTool = 0;
    }

    MapDocument *mapDocument = currentDocument();

    if (mapDocument)
        mUndoGroup->setActiveStack(mapDocument->undoStack());

    emit currentDocumentChanged(mapDocument);

    if (MapScene *mapScene = currentMapScene()) {
        mapScene->setSelectedTool(mSelectedTool);
        mapScene->enableSelectedTool();
        mSceneWithTool = mapScene;
    }
}

void DocumentManager::setSelectedTool(AbstractTool *tool)
{
    if (mSelectedTool == tool)
        return;

    mSelectedTool = tool;

    if (mSceneWithTool) {
        mSceneWithTool->disableSelectedTool();

        if (mSelectedTool) {
            mSceneWithTool->setSelectedTool(mSelectedTool);
            mSceneWithTool->enableSelectedTool();
        }
    }
}

void DocumentManager::updateDocumentTab()
{
    MapDocument *mapDocument = static_cast<MapDocument*>(sender());
    const int index = mDocuments.indexOf(mapDocument);

    QString tabText = mapDocument->displayName();
    if (mapDocument->isModified())
        tabText.prepend(QLatin1Char('*'));

    mTabWidget->setTabText(index, tabText);
#ifdef ZOMBOID
    mTabWidget->setTabToolTip(index, QDir::toNativeSeparators(
                                  mapDocument->fileName()));
#else
    mTabWidget->setTabToolTip(index, mapDocument->fileName());
#endif
}

void DocumentManager::centerViewOn(int x, int y)
{
    MapView *view = currentMapView();
    if (!view)
        return;

    view->centerOn(currentDocument()->renderer()->tileToPixelCoords(x, (qreal)y));
}

#ifdef ZOMBOID
TileLayersPanel *DocumentManager::currentTileLayerPanel() const
{
    return mDocuments.size() ? mTileLayerPanels[currentDocument()] : 0;
}

void DocumentManager::stampAltHovered(const QPoint &tilePos)
{
    if (TileLayersPanel *panel = currentTileLayerPanel())
        panel->setTilePosition(tilePos);
}
#endif // ZOMBOID

// Wrapper around QGraphicsView::ensureVisible.  In ensureVisible, when the rectangle to
// view is larger than the viewport, the final position is often undesirable with multiple
// scrolls taking place.  In this implementation, when the rectangle to view (plus margins)
// does not fit in the current view or is not partially visible already, the view is centered
// on the rectangle's center.
void ensureRectVisible(QGraphicsView *view, const QRectF &rect, int xmargin, int ymargin)
{
    QRect rectToView = view->mapFromScene(rect).boundingRect();
    rectToView.adjust(-xmargin, -ymargin, xmargin, ymargin);
    QRect viewportRect = view->viewport()->rect(); // includes scrollbars?
    if (viewportRect.contains(rectToView, true))
        return;
    if (rectToView.width() > viewportRect.width() ||
            rectToView.height() > viewportRect.height() ||
            !viewportRect.intersects(rectToView)) {
        view->centerOn(rect.center());
    } else {
        view->ensureVisible(rect, xmargin, ymargin);
    }
}

void DocumentManager::ensureRectVisible(QRectF &rect, int xmargin, int ymargin)
{
    MapView *view = currentMapView();
    if (!view)
        return;

    ::ensureRectVisible(view, rect, xmargin, ymargin);
}
