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

#include "zlevelsdock.h"

#include "documentmanager.h"
#include "layermodel.h"
#include "map.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "maplevel.h"
#include "tilelayer.h"
#include "utils.h"
#include "zlevelsmodel.h"

#include <QBoxLayout>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSlider>
#include <QToolButton>
#include <QToolBar>

using namespace Tiled;
using namespace Tiled::Internal;

ZLevelsDock::ZLevelsDock(QWidget *parent) :
    QDockWidget(parent),
    mMapDocument(nullptr),
    mView(new ZLevelsView()),
    mOpacityLabel(new QLabel),
    mOpacitySlider(new QSlider(Qt::Horizontal)),
    mVisibilityLabel(new QLabel),
    mVisibilitySlider(new QSlider(Qt::Horizontal))
{
    setObjectName(QLatin1String("ZLevelsDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(5, 5, 5, 5);

    QHBoxLayout *opacityLayout = new QHBoxLayout;
    mOpacitySlider->setRange(0, 100);
    mOpacitySlider->setEnabled(false);
    opacityLayout->addWidget(mOpacityLabel);
    opacityLayout->addWidget(mOpacitySlider);
    mOpacityLabel->setBuddy(mOpacitySlider);

    QHBoxLayout *visibilityLayout = new QHBoxLayout;
    mVisibilitySlider->setRange(0, 9);
    mVisibilitySlider->setEnabled(false);
    visibilityLayout->addWidget(mVisibilityLabel);
    visibilityLayout->addWidget(mVisibilitySlider);
    mVisibilityLabel->setBuddy(mVisibilitySlider);

    /////
    MapDocumentActionHandler *handler = MapDocumentActionHandler::instance();

    QMenu *newLayerMenu = new QMenu(this);
    newLayerMenu->addAction(handler->actionAddTileLayer());
    newLayerMenu->addAction(handler->actionAddObjectGroup());
    newLayerMenu->addAction(handler->actionAddImageLayer());

    const QIcon newIcon(QLatin1String(":/images/16x16/document-new.png"));
    QToolButton *newLayerButton = new QToolButton;
    newLayerButton->setPopupMode(QToolButton::InstantPopup);
    newLayerButton->setMenu(newLayerMenu);
    newLayerButton->setIcon(newIcon);
    newLayerButton->setToolTip(tr("New Layer"));
    Utils::setThemeIcon(newLayerButton, "document-new");

    QToolBar *toolBar = new QToolBar;
    toolBar->setFloatable(false);
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(16, 16));

    toolBar->addWidget(newLayerButton);
    toolBar->addAction(handler->actionMoveLayerUp());
    toolBar->addAction(handler->actionMoveLayerDown());
    toolBar->addAction(handler->actionDuplicateLayer());
    toolBar->addAction(handler->actionRemoveLayer());
    toolBar->addSeparator();
    toolBar->addAction(handler->actionToggleOtherLayers());

    QToolButton *button;
    button = dynamic_cast<QToolButton*>(toolBar->widgetForAction(handler->actionMoveLayerUp()));
    button->setAutoRepeat(true);
    button = dynamic_cast<QToolButton*>(toolBar->widgetForAction(handler->actionMoveLayerDown()));
    button->setAutoRepeat(true);
    /////

    layout->addLayout(opacityLayout);
    layout->addLayout(visibilityLayout);
    layout->addWidget(mView);
    layout->addWidget(toolBar);

    setWidget(widget);
    retranslateUi();

    connect(mOpacitySlider, &QAbstractSlider::valueChanged,
            this, &ZLevelsDock::setLayerOpacity);
    updateOpacitySlider();

    connect(mVisibilitySlider, &QAbstractSlider::valueChanged,
            this, &ZLevelsDock::setTopmostVisibleLayer);
    updateVisibilitySlider();

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, &QDockWidget::visibilityChanged,
            mView, &QWidget::setVisible);

    connect(DocumentManager::instance(), &DocumentManager::documentAboutToClose,
            this, &ZLevelsDock::documentAboutToClose);

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
        connect(mMapDocument, &MapDocument::currentLayerIndexChanged,
                this, &ZLevelsDock::updateOpacitySlider);
        connect(mMapDocument, &MapDocument::currentLayerIndexChanged,
                this, &ZLevelsDock::updateVisibilitySlider);
    }

    updateOpacitySlider();
    updateVisibilitySlider();
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
    mOpacityLabel->setText(tr("Opacity:"));
    mVisibilityLabel->setText(tr("Visibility:"));
}

void ZLevelsDock::updateOpacitySlider()
{
    const bool enabled = mMapDocument &&
                         mMapDocument->currentLayerIndex() != -1;

    mOpacitySlider->setEnabled(enabled);
    mOpacityLabel->setEnabled(enabled);

    if (enabled) {
        qreal opacity = mMapDocument->currentLayer()->opacity();
        mOpacitySlider->setValue((int) (opacity * 100));
    } else {
        mOpacitySlider->setValue(100);
    }
}

void ZLevelsDock::setLayerOpacity(int opacity)
{
    if (!mMapDocument)
        return;

    const int levelIndex = mMapDocument->currentLevelIndex();
    if (levelIndex == -1)
        return;

    const int layerIndex = mMapDocument->currentLayerIndex();
    if (layerIndex == -1)
        return;

    const Layer *layer = mMapDocument->map()->layerAt(levelIndex, layerIndex);

    if ((int) (layer->opacity() * 100) != opacity) {
        LayerModel *layerModel = mMapDocument->layerModel();
        layerModel->setData(layerModel->toIndex(levelIndex, layerIndex),
                            qreal(opacity) / 100,
                            LayerModel::OpacityRole);
    }
}

void ZLevelsDock::updateVisibilitySlider()
{
    const bool enabled = mMapDocument;
    mVisibilitySlider->setEnabled(enabled);
    mVisibilityLabel->setEnabled(enabled);
    if (enabled) {
        mVisibilitySlider->blockSignals(true);
        mVisibilitySlider->setMaximum(mMapDocument->map()->layerCount());
        mVisibilitySlider->setValue(mMapDocument->maxVisibleLayer());
        mVisibilitySlider->blockSignals(false);
    } else {
        mVisibilitySlider->setValue(mVisibilitySlider->maximum());
    }
}

void ZLevelsDock::setTopmostVisibleLayer(int layerIndex)
{
    if (!mMapDocument)
        return;

    int totalLayers = 0;
    for (int z = 0; z < mMapDocument->map()->levelCount(); z++) {
        MapLevel *mapLevel = mMapDocument->map()->levelAt(z);
        for (int index = 0; index < mapLevel->layerCount(); index++) {
            Layer *layer  = mapLevel->layerAt(index);
            if (layer->asTileLayer()) {
                bool visible = (totalLayers + 1 <= layerIndex);
                if (visible != layer->isVisible()) {
                    mMapDocument->setLayerVisible(layer->level(), index, visible);
                }
            }
            totalLayers++;
        }
    }

    mMapDocument->setMaxVisibleLayer(layerIndex);
}

void ZLevelsDock::updateActions()
{
}

void ZLevelsDock::saveExpandedLevels(MapDocument *mapDoc)
{
    mExpandedLevels[mapDoc].clear();
    for (int level : mView->model()->levels()) {
        if (mView->isExpanded(mView->model()->index(level))) {
            mExpandedLevels[mapDoc].append(level);
        }
    }
}

void ZLevelsDock::restoreExpandedLevels(MapDocument *mapDoc)
{
    if (mExpandedLevels.contains(mapDoc)) {
        for (int level : mExpandedLevels[mapDoc]) {
            mView->setExpanded(mView->model()->index(level), true);
        }
        mExpandedLevels[mapDoc].clear();
    } else {
        mView->expandAll();
    }

    // Also restore the selection
    if (Layer *layer = mapDoc->currentLayer()) {
        mView->setCurrentIndex(mView->model()->index(layer));
    }
}

void ZLevelsDock::documentAboutToClose(int index, MapDocument *mapDocument)
{
    Q_UNUSED(index)
    mExpandedLevels.remove(mapDocument);
}

///// ///// ///// ///// /////

ZLevelsView::ZLevelsView(QWidget *parent)
    : QTreeView(parent)
    , mMapDocument(nullptr)
    , mSynching(false)
{
    setRootIsDecorated(true);
    setHeaderHidden(true);
    setItemsExpandable(true);
    setUniformRowHeights(true);

    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setSelectionMode(QAbstractItemView::ExtendedSelection);

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
        // 2 equal-sized columns, user can't adjust
#if QT_VERSION >= 0x050000
        header()->setSectionResizeMode(0, QHeaderView::Stretch);
#else
        header()->setResizeMode(0, QHeaderView::Stretch);
#endif

        connect(mMapDocument, &MapDocument::currentLayerIndexChanged,
                this, &ZLevelsView::currentLayerIndexChanged);
        connect(mMapDocument, &MapDocument::editLayerNameRequested,
                this, &ZLevelsView::editLayerName);
    } else {
        if (model())
            model()->setMapDocument(nullptr);
        setModel(mModel = nullptr);
    }

}

void ZLevelsView::onActivated(const QModelIndex &index)
{
    Q_UNUSED(index)
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
        if (Layer *layer = model()->toLayer(index)) {
            MapLevel *mapLevel = mMapDocument->map()->levelAt(layer->level());
            int layerIndex = mapLevel->layers().indexOf(layer);
            if ((layer->level() != mMapDocument->currentLevelIndex()) || (layerIndex != mMapDocument->currentLayerIndex())) {
                mSynching = true;
                mMapDocument->setCurrentLayerIndex(layer->level(), layerIndex);
                mSynching = false;
            }
        }
    }
}

void ZLevelsView::currentLayerIndexChanged(int levelIndex, int layerIndex)
{
    if (mSynching)
        return;
    
    if (layerIndex > -1) {
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

    // Selected no layer, or a layer not in a CompositeLayerGroup
    mSynching = true;
    setCurrentIndex(QModelIndex());
    mSynching = false;
}

void ZLevelsView::editLayerName()
{
    if (!isVisible())
        return;

    const ZLevelsModel *levelsModel = mMapDocument->levelsModel();
    edit(levelsModel->index(mMapDocument->currentLayer()));
}
