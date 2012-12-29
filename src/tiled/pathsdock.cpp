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

#include "pathsdock.h"

#include "addremovepath.h"
#include "documentmanager.h"
#include "map.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "movepathtolayer.h"
#include "pathlayer.h"
#include "pathmodel.h"
#include "pathpropertiesdialog.h"
#include "reorderpath.h"
#include "utils.h"

#include <QBoxLayout>
#include <QApplication>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QMenu>
#include <QSlider>
#include <QToolBar>
#include <QUrl>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

PathsDock::PathsDock(QWidget *parent)
    : QDockWidget(parent)
    , mPathsView(new PathsView())
    , mMapDocument(0)
{
    setObjectName(QLatin1String("PathsDock"));

    mActionMoveUp = new QAction(this);
    mActionMoveUp->setIcon(QIcon(QLatin1String(":/images/16x16/go-up.png")));

    mActionMoveDown = new QAction(this);
    mActionMoveDown->setIcon(QIcon(QLatin1String(":/images/16x16/go-down.png")));

    mActionDuplicatePaths = new QAction(this);
    mActionDuplicatePaths->setIcon(QIcon(QLatin1String(":/images/16x16/stock-duplicate-16.png")));

    mActionRemovePaths = new QAction(this);
    mActionRemovePaths->setIcon(QIcon(QLatin1String(":/images/16x16/edit-delete.png")));

    mActionPathProperties = new QAction(this);
    mActionPathProperties->setIcon(QIcon(QLatin1String(":/images/16x16/document-properties.png")));
    mActionPathProperties->setToolTip(tr("Object Properties"));

    Utils::setThemeIcon(mActionMoveUp, "go-up");
    Utils::setThemeIcon(mActionMoveDown, "go-down");
    Utils::setThemeIcon(mActionRemovePaths, "edit-delete");
    Utils::setThemeIcon(mActionPathProperties, "document-properties");

    connect(mActionMoveUp, SIGNAL(triggered()), SLOT(moveUp()));
    connect(mActionMoveDown, SIGNAL(triggered()), SLOT(moveDown()));
    connect(mActionDuplicatePaths, SIGNAL(triggered()), SLOT(duplicatePaths()));
    connect(mActionRemovePaths, SIGNAL(triggered()), SLOT(removePaths()));
    connect(mActionPathProperties, SIGNAL(triggered()), SLOT(pathProperties()));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);
    layout->addWidget(mPathsView);

    MapDocumentActionHandler *handler = MapDocumentActionHandler::instance();

    QAction *newLayerAction = new QAction(this);
    newLayerAction->setIcon(QIcon(QLatin1String(":/images/16x16/document-new.png")));
    newLayerAction->setToolTip(tr("Add Path Layer"));
    connect(newLayerAction, SIGNAL(triggered()),
            handler->actionAddPathLayer(), SIGNAL(triggered()));

    mActionMoveToLayer = new QAction(this);
    mActionMoveToLayer->setIcon(QIcon(QLatin1String(":/images/16x16/layer-object.png")));
    mActionMoveToLayer->setToolTip(tr("Move Path To Layer"));

    QToolBar *toolbar = new QToolBar;
    toolbar->setFloatable(false);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));

    toolbar->addAction(newLayerAction);
    toolbar->addAction(mActionMoveUp);
    toolbar->addAction(mActionMoveDown);
    toolbar->addAction(mActionDuplicatePaths);
    toolbar->addAction(mActionRemovePaths);

    toolbar->addAction(mActionMoveToLayer);
    QToolButton *button;
    button = dynamic_cast<QToolButton*>(toolbar->widgetForAction(mActionMoveToLayer));
    mMoveToMenu = new QMenu(this);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setMenu(mMoveToMenu);
    connect(mMoveToMenu, SIGNAL(aboutToShow()), SLOT(aboutToShowMoveToMenu()));
    connect(mMoveToMenu, SIGNAL(triggered(QAction*)),
            SLOT(triggeredMoveToMenu(QAction*)));

    toolbar->addAction(mActionPathProperties);

    layout->addWidget(toolbar);
    setWidget(widget);
    retranslateUi();

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mPathsView, SLOT(setVisible(bool)));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,MapDocument*)),
            SLOT(documentAboutToClose(int,MapDocument*)));

    updateActions();
}

void PathsDock::setMapDocument(MapDocument *mapDoc)
{
    if (mMapDocument) {
        saveExpandedGroups(mMapDocument);
        mMapDocument->disconnect(this);
    }

    mMapDocument = mapDoc;

    mPathsView->setMapDocument(mapDoc);

    if (mMapDocument) {
        restoreExpandedGroups(mMapDocument);
        connect(mMapDocument, SIGNAL(selectedPathsChanged()),
                this, SLOT(updateActions()));
    }

    updateActions();
}

void PathsDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void PathsDock::retranslateUi()
{
    setWindowTitle(tr("Paths"));
}

void PathsDock::updateActions()
{
    int count = mMapDocument ? mMapDocument->selectedPaths().count() : 0;
    bool enabled = count > 0;
    mActionMoveUp->setEnabled(enabled);
    mActionMoveDown->setEnabled(enabled);
    mActionDuplicatePaths->setEnabled(enabled);
    mActionRemovePaths->setEnabled(enabled);
    mActionPathProperties->setEnabled(enabled && (count == 1));

    mActionDuplicatePaths->setToolTip((enabled && count > 1)
        ? tr("Duplicate %n Paths", "", count) : tr("Duplicate Path"));
    mActionRemovePaths->setToolTip((enabled && count > 1)
        ? tr("Remove %n Paths", "", count) : tr("Remove Path"));

    if (mMapDocument && (mMapDocument->map()->objectGroupCount() < 2))
        enabled = false;
    mActionMoveToLayer->setEnabled(enabled);
    mActionMoveToLayer->setToolTip((enabled && count > 1)
        ? tr("Move %n Paths To Layer", "", count) : tr("Move Path To Layer"));
}

void PathsDock::aboutToShowMoveToMenu()
{
    mMoveToMenu->clear();

    foreach (PathLayer *pathLayer, mMapDocument->map()->pathLayers()) {
        QAction *action = mMoveToMenu->addAction(pathLayer->name());
        action->setData(QVariant::fromValue(pathLayer));
    }
}

void PathsDock::triggeredMoveToMenu(QAction *action)
{
    PathLayer *pathLayer = action->data().value<PathLayer*>();

    const QList<Path *> &paths = mMapDocument->selectedPaths();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Move %n Path(s) to Layer", "",
                             paths.size()));
    foreach (Path *path, paths) {
        if (path->pathLayer() == pathLayer)
            continue;
        undoStack->push(new MovePathToLayer(mMapDocument,
                                            path,
                                            pathLayer));
    }
    undoStack->endMacro();
}

void PathsDock::moveUp()
{
    // Unnecessary check is unnecessary
    if (!mMapDocument || !mMapDocument->selectedPaths().count())
        return;

    // TODO: multiple selection is allowed so move multiple paths.
    Path *path = mMapDocument->selectedPaths().first();
    if (path != path->pathLayer()->paths().last()) {
        mMapDocument->undoStack()->push(new ReorderPath(mMapDocument, path,
                                                        path->index() + 1));
    }
}

void PathsDock::moveDown()
{
    // Unnecessary check is unnecessary
    if (!mMapDocument || !mMapDocument->selectedPaths().count())
        return;

    // TODO: multiple selection is allowed so move multiple paths.
    Path *path = mMapDocument->selectedPaths().first();
    if (path != path->pathLayer()->paths().first()) {
        mMapDocument->undoStack()->push(new ReorderPath(mMapDocument, path,
                                                        path->index() - 1));
    }
}

void PathsDock::duplicatePaths()
{
    // Unnecessary check is unnecessary
    if (!mMapDocument || !mMapDocument->selectedPaths().count())
        return;

    const QList<Path *> &paths = mMapDocument->selectedPaths();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Duplicate %n Path(s)", "", paths.size()));

    QList<Path*> clones;
    foreach (Path *path, paths) {
        Path *clone = path->clone();
        undoStack->push(new AddPath(mMapDocument,
                                    path->pathLayer(),
                                    clone));
        clones << clone;
    }

    undoStack->endMacro();
    mMapDocument->setSelectedPaths(clones);
}

void PathsDock::removePaths()
{
    // Unnecessary check is unnecessary
    if (!mMapDocument || !mMapDocument->selectedPaths().count())
        return;

    const QList<Path *> &paths = mMapDocument->selectedPaths();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Remove %n Path(s)", "", paths.size()));
    foreach (Path *path, paths)
        undoStack->push(new RemovePath(mMapDocument, path));
    undoStack->endMacro();
}

void PathsDock::pathProperties()
{
    // Unnecessary check is unnecessary
    if (!mMapDocument || !mMapDocument->selectedPaths().count())
        return;

    const QList<Path *> &selectedPaths = mMapDocument->selectedPaths();

    Path *path = selectedPaths.first();

    PathPropertiesDialog dialog(window());
    dialog.setPath(mMapDocument, path);
    dialog.exec();
//    mMapDocument->pathModel()->emitPathsChanged(QList<Path*>());
}

void PathsDock::saveExpandedGroups(MapDocument *mapDoc)
{
    mExpandedGroups[mapDoc].clear();
    foreach (PathLayer *og, mapDoc->map()->pathLayers()) {
        if (mPathsView->isExpanded(mPathsView->model()->index(og)))
            mExpandedGroups[mapDoc].append(og);
    }
}

void PathsDock::restoreExpandedGroups(MapDocument *mapDoc)
{
    if (mExpandedGroups.contains(mapDoc)) {
        foreach (PathLayer *og, mExpandedGroups[mapDoc])
            mPathsView->setExpanded(mPathsView->model()->index(og), true);
        mExpandedGroups[mapDoc].clear();
    } else
        mPathsView->expandAll();

    // Also restore the selection
    foreach (Path *o, mapDoc->selectedPaths()) {
        QModelIndex index = mPathsView->model()->index(o);
        mPathsView->selectionModel()->select(index, QItemSelectionModel::Select |  QItemSelectionModel::Rows);
    }
}

void PathsDock::documentAboutToClose(int index, MapDocument *mapDocument)
{
    Q_UNUSED(index)
    mExpandedGroups.remove(mapDocument);
}

///// ///// ///// ///// /////

PathsView::PathsView(QWidget *parent)
    : QTreeView(parent)
    , mMapDocument(0)
    , mSynching(false)
{
    setRootIsDecorated(true);
    setHeaderHidden(false);
    setItemsExpandable(true);
    setUniformRowHeights(true);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize PathsView::sizeHint() const
{
    return QSize(130, 100);
}

void PathsView::setMapDocument(MapDocument *mapDoc)
{
    if (mapDoc == mMapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDoc;

    if (mMapDocument) {
        mPathModel = mMapDocument->pathModel();
        setModel(mPathModel);
        model()->setMapDocument(mapDoc);
        header()->setResizeMode(0, QHeaderView::Stretch); // 2 equal-sized columns, user can't adjust

        connect(mMapDocument, SIGNAL(selectedPathsChanged()),
                this, SLOT(selectedPathsChanged()));
        connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                SLOT(currentLayerIndexChanged(int)));
    } else {
        if (model())
            model()->setMapDocument(0);
        setModel(mPathModel = 0);
    }

}

void PathsView::onActivated(const QModelIndex &index)
{
    if (Path *path = model()->toPath(index)) {
        PathPropertiesDialog dialog(window());
        dialog.setPath(mMapDocument, path);
        dialog.exec();
    }
}

void PathsView::selectionChanged(const QItemSelection &selected,
                                   const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);

    if (!mMapDocument || mSynching)
        return;

    QModelIndexList selectedRows = selectionModel()->selectedRows();
    int currentLayerIndex = -1;

    QList<Path*> selectedPaths;
    foreach (const QModelIndex &index, selectedRows) {
        PathLayer *pathLayer = model()->toLayer(index);
        if (Path *path = model()->toPath(index)) {
            selectedPaths.append(path);
            pathLayer = path->pathLayer();
        }
        if (pathLayer) {
            int index = mMapDocument->map()->layers().indexOf(pathLayer);
            if (currentLayerIndex == -1)
                currentLayerIndex = index;
            else if (currentLayerIndex != index)
                currentLayerIndex = -2;
        }
    }

    // Switch the current path layer if only one path layer (and/or its paths)
    // are included in the current selection.
    if (currentLayerIndex >= 0 && currentLayerIndex != mMapDocument->currentLayerIndex())
        mMapDocument->setCurrentLayerIndex(currentLayerIndex);

    if (selectedPaths != mMapDocument->selectedPaths()) {
        mSynching = true;
        if (selectedPaths.count() == 1) {
            Path *path = selectedPaths.first();
            QRect bounds = path->polygon().boundingRect();
            DocumentManager::instance()->centerViewOn(bounds.center().x(),
                                                      bounds.center().y());
        }
        mMapDocument->setSelectedPaths(selectedPaths);
        mSynching = false;
    }
}

void PathsView::selectedPathsChanged()
{
    if (mSynching)
        return;

    if (!mMapDocument)
        return;

    const QList<Path *> &selectedPaths = mMapDocument->selectedPaths();

    mSynching = true;
    clearSelection();
    foreach (Path *o, selectedPaths) {
        QModelIndex index = model()->index(o);
        selectionModel()->select(index, QItemSelectionModel::Select |  QItemSelectionModel::Rows);
    }
    mSynching = false;

    if (selectedPaths.count() == 1) {
        Path *o = selectedPaths.first();
        scrollTo(model()->index(o));
    }
}

void PathsView::currentLayerIndexChanged(int index)
{
    if (mSynching)
        return;

    if (index > -1) {
        Layer *layer = mMapDocument->currentLayer();
        if (PathLayer *pathLayer = layer->asPathLayer()) {
            mSynching = true;
            setCurrentIndex(model()->index(pathLayer));
            mSynching = false;
            return;
        }
    }

    // Selected no layer, or not a PathLayer
    mSynching = true;
    setCurrentIndex(QModelIndex());
    mSynching = false;
}

