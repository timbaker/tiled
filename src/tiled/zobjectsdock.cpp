/*
 * zobjectsdock.cpp
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

#include "zobjectsdock.hpp"

#include "addremovemapobject.h"
#include "map.h"
#include "mapobject.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "objectgroup.h"
#include "objectpropertiesdialog.h"
#include "utils.h"
#include "zmapobjectmodel.hpp"

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

ZObjectsDock::ZObjectsDock(QWidget *parent)
    : QDockWidget(parent)
    , mObjectsView(new ZObjectsView())
	, mMapDocument(0)
{
    setObjectName(QLatin1String("ZObjectsDock"));

    mActionDuplicateObjects = new QAction(this);
    mActionDuplicateObjects->setIcon(QIcon(QLatin1String(":/images/16x16/stock-duplicate-16.png")));

	mActionRemoveObjects = new QAction(this);
    mActionRemoveObjects->setIcon(QIcon(QLatin1String(":/images/16x16/edit-delete.png")));

	mActionObjectProperties = new QAction(this);
    mActionObjectProperties->setIcon(QIcon(QLatin1String(":/images/16x16/document-properties.png")));
	mActionObjectProperties->setToolTip(tr("Object Propertes"));

    connect(mActionDuplicateObjects, SIGNAL(triggered()), SLOT(duplicateObjects()));
    connect(mActionRemoveObjects, SIGNAL(triggered()), SLOT(removeObjects()));
    connect(mActionObjectProperties, SIGNAL(triggered()), SLOT(objectProperties()));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);
	layout->addWidget(mObjectsView);

    MapDocumentActionHandler *handler = MapDocumentActionHandler::instance();

    QMenu *newLayerMenu = new QMenu(this);
    newLayerMenu->addAction(handler->actionAddObjectGroup());

    const QIcon newIcon(QLatin1String(":/images/16x16/document-new.png"));
    QToolButton *newLayerButton = new QToolButton;
    newLayerButton->setPopupMode(QToolButton::InstantPopup);
    newLayerButton->setMenu(newLayerMenu);
    newLayerButton->setIcon(newIcon);
	newLayerButton->setToolTip(tr("New Layer"));
	Utils::setThemeIcon(newLayerButton, "document-new");

    QToolBar *toolbar = new QToolBar;
    toolbar->setFloatable(false);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(16, 16));

    toolbar->addWidget(newLayerButton);
	toolbar->addAction(mActionDuplicateObjects);
    toolbar->addAction(mActionRemoveObjects);
//    toolbar->addSeparator();
    toolbar->addAction(mActionObjectProperties);

    layout->addWidget(toolbar);
    setWidget(widget);
    retranslateUi();

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mObjectsView, SLOT(setVisible(bool)));

	updateActions();
}

void ZObjectsDock::setMapDocument(MapDocument *mapDoc)
{
	if (mMapDocument)
		mMapDocument->disconnect(this);
	mMapDocument = mapDoc;
	if (mMapDocument)
        connect(mMapDocument, SIGNAL(selectedObjectsChanged()), this, SLOT(updateActions()));
	updateActions();

    mObjectsView->setMapDocument(mapDoc);
}

void ZObjectsDock::changeEvent(QEvent *e)
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

void ZObjectsDock::retranslateUi()
{
    setWindowTitle(tr("Objects"));
}

void ZObjectsDock::updateActions()
{
	int count = mMapDocument ? mMapDocument->selectedObjects().count() : 0;
	bool enabled = count > 0;
	mActionDuplicateObjects->setEnabled(enabled);
	mActionRemoveObjects->setEnabled(enabled);
	mActionObjectProperties->setEnabled(enabled && (count == 1));

	mActionDuplicateObjects->setToolTip((enabled && count > 1)
		? tr("Duplicate %n Objects", "", count) : tr("Duplicate Object"));
	mActionRemoveObjects->setToolTip((enabled && count > 1)
		? tr("Remove %n Objects", "", count) : tr("Remove Object"));

}

void ZObjectsDock::duplicateObjects()
{
	// Unnecessary check is unnecessary
	if (!mMapDocument || !mMapDocument->selectedObjects().count())
		return;

	const QList<MapObject *> &objects = mMapDocument->selectedObjects();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Duplicate %n Object(s)", "", objects.size()));

	QList<MapObject*> clones;
	foreach (MapObject *mapObject, objects) {
		MapObject *clone = mapObject->clone();
		undoStack->push(new AddMapObject(mMapDocument,
										 mapObject->objectGroup(),
										 clone));
		clones << clone;
	}

    undoStack->endMacro();
    mMapDocument->setSelectedObjects(clones);
}

void ZObjectsDock::removeObjects()
{
	// Unnecessary check is unnecessary
	if (!mMapDocument || !mMapDocument->selectedObjects().count())
		return;

	const QList<MapObject *> &objects = mMapDocument->selectedObjects();

    QUndoStack *undoStack = mMapDocument->undoStack();
    undoStack->beginMacro(tr("Remove %n Object(s)", "", objects.size()));
    foreach (MapObject *mapObject, objects)
		undoStack->push(new RemoveMapObject(mMapDocument, mapObject));
    undoStack->endMacro();
}

void ZObjectsDock::objectProperties()
{
	// Unnecessary check is unnecessary
	if (!mMapDocument || !mMapDocument->selectedObjects().count())
		return;

	const QList<MapObject *> &selectedObjects = mMapDocument->selectedObjects();

	MapObject *mapObject = selectedObjects.first();
    ObjectPropertiesDialog propertiesDialog(mMapDocument, mapObject, 0);
    propertiesDialog.exec();
}

///// ///// ///// ///// /////

ZObjectsView::ZObjectsView(QWidget *parent)
    : QTreeView(parent)
	, mMapDocument(0)
	, mSynching(false)
{
    setRootIsDecorated(true);
    setHeaderHidden(false);
    setItemsExpandable(true);
    setUniformRowHeights(true);

//	ZMapObjectModel *model = new ZMapObjectModel;
//	setModel(model);

	connect(this, SIGNAL(activated(QModelIndex)), SLOT(onActivated(QModelIndex)));
}

QSize ZObjectsView::sizeHint() const
{
    return QSize(130, 100);
}

void ZObjectsView::setMapDocument(MapDocument *mapDoc)
{
	if (mapDoc == mMapDocument)
		return;

	if (mMapDocument) {
		mMapDocument->disconnect(this);
        disconnect(selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                this, SLOT(currentRowChanged(QModelIndex)));
	}

	mMapDocument = mapDoc;

	if (mMapDocument) {
		mMapObjectModel = mMapDocument->mapObjectModel();
		setModel(mMapObjectModel);
		model()->setMapDocument(mapDoc);
		header()->setResizeMode(0, QHeaderView::Stretch); // 2 equal-sized columns, user can't adjust

		connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(currentLayerIndexChanged(int)));
 		connect(mMapDocument, SIGNAL(selectedObjectsChanged()), this, SLOT(selectedObjectsChanged()));
        connect(selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                this, SLOT(currentRowChanged(QModelIndex)));
	} else {
		if (model())
			model()->setMapDocument(0);
		setModel(mMapObjectModel = 0);
	}

}

void ZObjectsView::onActivated(const QModelIndex &index)
{
    // show object properties, center in view
}

void ZObjectsView::currentRowChanged(const QModelIndex &index)
{
	if (ObjectGroup *og = model()->toLayer(index)) {
		int index = mMapDocument->map()->layers().indexOf(og);
		mMapDocument->setCurrentLayerIndex(index);
	}
	if (MapObject *o = model()->toMapObject(index)) {
		mSynching = true;
		mMapDocument->setSelectedObjects(QList<MapObject*>() << o);
		mSynching = false;
	}
}

void ZObjectsView::currentLayerIndexChanged(int index)
{
    if (index > -1) {
		Layer *layer = mMapDocument->currentLayer();
		if (ObjectGroup *og = layer->asObjectGroup()) {
			if (model()->toLayer(currentIndex()) != og) {
				setCurrentIndex(model()->index(og));
			}
			return;
		}
    }
    setCurrentIndex(QModelIndex());
}

void ZObjectsView::selectedObjectsChanged()
{
	if (mSynching)
		return;

	clearSelection();

	if (!mMapDocument)
		return;

	const QList<MapObject *> &selectedObjects = mMapDocument->selectedObjects();

	foreach (MapObject *o, selectedObjects) {
		QModelIndex index = model()->index(o);
		selectionModel()->select(index, QItemSelectionModel::Select |  QItemSelectionModel::Rows);
	}
	if (selectedObjects.count())
		scrollTo(model()->index(selectedObjects.first()));
}