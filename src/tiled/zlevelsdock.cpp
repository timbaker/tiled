/*
 * ZLevelsDock.cpp
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

#include "zlevelsdock.hpp"

#include "documentmanager.h"
#include "map.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "utils.h"
#include "tilelayer.h"
#include "zlevelsmodel.hpp"

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

ZLevelsDock::ZLevelsDock(QWidget *parent)
    : QDockWidget(parent)
    , mView(new ZLevelsView())
	, mMapDocument(0)
{
    setObjectName(QLatin1String("ZLevelsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);
	layout->addWidget(mView);

    MapDocumentActionHandler *handler = MapDocumentActionHandler::instance();

    QToolBar *toolbar = new QToolBar;
    toolbar->setFloatable(false);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));


    layout->addWidget(toolbar);
    setWidget(widget);
    retranslateUi();

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mView, SLOT(setVisible(bool)));

    connect(DocumentManager::instance(), SIGNAL(documentCloseRequested(int)),
            SLOT(documentCloseRequested(int)));

	updateActions();
}

void ZLevelsDock::setMapDocument(MapDocument *mapDoc)
{
	if (mMapDocument) {
		saveExpandedLevels(mMapDocument);
		mMapDocument->disconnect(this);
	}

	mMapDocument = mapDoc;

    mView->setMapDocument(mapDoc);

	if (mMapDocument) {
		restoreExpandedLevels(mMapDocument);
        connect(mMapDocument, SIGNAL(selectedObjectsChanged()), this, SLOT(updateActions()));
	}

	updateActions();
}

void ZLevelsDock::changeEvent(QEvent *e)
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

void ZLevelsDock::retranslateUi()
{
    setWindowTitle(tr("Levels"));
}

void ZLevelsDock::updateActions()
{
}

void ZLevelsDock::saveExpandedLevels(MapDocument *mapDoc)
{
	mExpandedLevels[mapDoc].clear();
	foreach (ZTileLayerGroup *g, mapDoc->map()->tileLayerGroups()) {
		if (mView->isExpanded(mView->model()->index(g)))
			mExpandedLevels[mapDoc].append(g);
	}
}

void ZLevelsDock::restoreExpandedLevels(MapDocument *mapDoc)
{
	foreach (ZTileLayerGroup *g, mExpandedLevels[mapDoc])
		mView->setExpanded(mView->model()->index(g), true);
	mExpandedLevels[mapDoc].clear();
#if 0
	// Also restore the selection
	foreach (MapObject *o, mapDoc->selectedObjects()) {
		QModelIndex index = mView->model()->index(o);
		mView->selectionModel()->select(index, QItemSelectionModel::Select |  QItemSelectionModel::Rows);
	}
#endif
}

void ZLevelsDock::documentCloseRequested(int index)
{
    DocumentManager *documentManager = DocumentManager::instance();
    MapDocument *mapDoc = documentManager->documents().at(index);
	mExpandedLevels.remove(mapDoc);
}

///// ///// ///// ///// /////

ZLevelsView::ZLevelsView(QWidget *parent)
    : QTreeView(parent)
	, mMapDocument(0)
	, mSynching(false)
{
    setRootIsDecorated(true);
    setHeaderHidden(true);
    setItemsExpandable(true);
    setUniformRowHeights(true);

	setSelectionBehavior(QAbstractItemView::SelectRows);
//	setSelectionMode(QAbstractItemView::ExtendedSelection);

	connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize ZLevelsView::sizeHint() const
{
    return QSize(130, 100);
}

void ZLevelsView::setMapDocument(MapDocument *mapDoc)
{
	if (mapDoc == mMapDocument)
		return;

	if (mMapDocument) {
		mMapDocument->disconnect(this);
	}

	mMapDocument = mapDoc;

	if (mMapDocument) {
		setModel(mModel = mMapDocument->levelsModel());
		model()->setMapDocument(mapDoc);
		header()->setResizeMode(0, QHeaderView::Stretch); // 2 equal-sized columns, user can't adjust

		connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(currentLayerIndexChanged(int)));
	} else {
		if (model())
			model()->setMapDocument(0);
		setModel(mModel = 0);
	}

}

void ZLevelsView::onActivated(const QModelIndex &index)
{
    // show object properties, center in view
}

void ZLevelsView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
	QTreeView::selectionChanged(selected, deselected);

	if (!mMapDocument || mSynching)
		return;

	QModelIndexList selectedRows = selectionModel()->selectedRows();
	int count = selectedRows.count();

	if (count == 1) {
		QModelIndex index = selectedRows.first();
		if (TileLayer *tl = model()->toTileLayer(index)) {
			int layerIndex = mMapDocument->map()->layers().indexOf(tl);
			if (layerIndex != mMapDocument->currentLayerIndex()) {
				mSynching = true;
				mMapDocument->setCurrentLayerIndex(layerIndex);
				mSynching = false;
			}
		}
	}
}

void ZLevelsView::currentLayerIndexChanged(int index)
{
	if (mSynching)
		return;
	
    if (index > -1) {
		Layer *layer = mMapDocument->currentLayer();
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->group()) {
				mSynching = true;
				setCurrentIndex(model()->index(tl));
				mSynching = false;
				return;
			}
		}
    }

	// Selected no layer, or a layer not in a ZTileLayerGroup
	mSynching = true;
    setCurrentIndex(QModelIndex());
	mSynching = false;
}

