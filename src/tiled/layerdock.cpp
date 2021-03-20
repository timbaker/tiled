/*
 * layerdock.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2010, Andrew G. Crowell <overkill9999@gmail.com>
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
 * Copyright 2011, Stefan Beller <stefanbeller@googlemail.com>
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

#include "layerdock.h"

#include "layer.h"
#include "layermodel.h"
#include "map.h"
#include "mapdocument.h"
#include "mapdocumentactionhandler.h"
#include "propertiesdialog.h"
#include "objectgrouppropertiesdialog.h"
#include "objectgroup.h"
#include "utils.h"

#include <QBoxLayout>
#include <QApplication>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QSlider>
#include <QUndoStack>
#include <QToolBar>

using namespace Tiled;
using namespace Tiled::Internal;

LayerDock::LayerDock(QWidget *parent):
    QDockWidget(parent),
    mOpacityLabel(new QLabel),
    mOpacitySlider(new QSlider(Qt::Horizontal)),
#ifdef ZOMBOID
    mZomboidLayerLabel(new QLabel),
    mZomboidLayerSlider(new QSlider(Qt::Horizontal)),
#endif
    mLayerView(new LayerView),
    mMapDocument(nullptr)
{
    setObjectName(QLatin1String("layerDock"));

    QWidget *widget = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setMargin(5);

    QHBoxLayout *opacityLayout = new QHBoxLayout;
    mOpacitySlider->setRange(0, 100);
    mOpacitySlider->setEnabled(false);
    opacityLayout->addWidget(mOpacityLabel);
    opacityLayout->addWidget(mOpacitySlider);
    mOpacityLabel->setBuddy(mOpacitySlider);

#ifdef ZOMBOID
    QHBoxLayout *zomboidLayerLayout = new QHBoxLayout;
    mZomboidLayerSlider->setRange(0, 9);
    mZomboidLayerSlider->setEnabled(false);
    zomboidLayerLayout->addWidget(mZomboidLayerLabel);
    zomboidLayerLayout->addWidget(mZomboidLayerSlider);
    mZomboidLayerLabel->setBuddy(mZomboidLayerSlider);
#endif

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
#ifdef ZOMBOID
    newLayerButton->setToolTip(tr("New Layer"));
#endif
    Utils::setThemeIcon(newLayerButton, "document-new");

    QToolBar *buttonContainer = new QToolBar;
    buttonContainer->setFloatable(false);
    buttonContainer->setMovable(false);
    buttonContainer->setIconSize(QSize(16, 16));

    buttonContainer->addWidget(newLayerButton);
    buttonContainer->addAction(handler->actionMoveLayerUp());
    buttonContainer->addAction(handler->actionMoveLayerDown());
    buttonContainer->addAction(handler->actionDuplicateLayer());
    buttonContainer->addAction(handler->actionRemoveLayer());
    buttonContainer->addSeparator();
    buttonContainer->addAction(handler->actionToggleOtherLayers());

#ifdef ZOMBOID
    QToolButton *button;
    button = dynamic_cast<QToolButton*>(buttonContainer->widgetForAction(handler->actionMoveLayerUp()));
    button->setAutoRepeat(true);
    button = dynamic_cast<QToolButton*>(buttonContainer->widgetForAction(handler->actionMoveLayerDown()));
    button->setAutoRepeat(true);
#endif


    layout->addLayout(opacityLayout);
#ifdef ZOMBOID
    layout->addLayout(zomboidLayerLayout);
#endif
    layout->addWidget(mLayerView);
    layout->addWidget(buttonContainer);

    setWidget(widget);
    retranslateUi();

    connect(mOpacitySlider, SIGNAL(valueChanged(int)),
            this, SLOT(setLayerOpacity(int)));
    updateOpacitySlider();

#ifdef ZOMBOID
    connect(mZomboidLayerSlider, SIGNAL(valueChanged(int)),
            this, SLOT(setZomboidLayer(int)));
    updateZomboidLayerSlider();
#endif

    // Workaround since a tabbed dockwidget that is not currently visible still
    // returns true for isVisible()
    connect(this, SIGNAL(visibilityChanged(bool)),
            mLayerView, SLOT(setVisible(bool)));
}

void LayerDock::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;

    if (mMapDocument) {
        connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(updateOpacitySlider()));
#ifdef ZOMBOID
        connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(updateZomboidLayerSlider()));
#endif
    }

    mLayerView->setMapDocument(mapDocument);
    updateOpacitySlider();
#ifdef ZOMBOID
    updateZomboidLayerSlider();
#endif
}

void LayerDock::changeEvent(QEvent *e)
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

void LayerDock::updateOpacitySlider()
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

void LayerDock::setLayerOpacity(int opacity)
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
        QModelIndex index = layerModel->toIndex(levelIndex, layerIndex);
        layerModel->setData(index,
                            qreal(opacity) / 100,
                            LayerModel::OpacityRole);
    }
}

#ifdef ZOMBOID
void LayerDock::updateZomboidLayerSlider()
{
    const bool enabled = mMapDocument;
    mZomboidLayerSlider->setEnabled(enabled);
    mZomboidLayerLabel->setEnabled(enabled);
    if (enabled) {
        mZomboidLayerSlider->blockSignals(true);
        mZomboidLayerSlider->setMaximum(mMapDocument->map()->layerCount());
        mZomboidLayerSlider->setValue(mMapDocument->maxVisibleLayer());
        mZomboidLayerSlider->blockSignals(false);
    } else {
        mZomboidLayerSlider->setValue(mZomboidLayerSlider->maximum());
    }
}

void LayerDock::setZomboidLayer(int number)
{
    if (!mMapDocument)
        return;

    int index = 0;
    for (Layer *layer : mMapDocument->map()->layers()) {
        if (layer->asTileLayer() ) {
            bool visible = (index + 1 <= number);
            if (visible != layer->isVisible())
                mMapDocument->setLayerVisible(layer->level(), index, visible);
        }
        index++;
    }

    mMapDocument->setMaxVisibleLayer(number);
}
#endif // ZOMBOID

void LayerDock::retranslateUi()
{
    setWindowTitle(tr("Layers"));
    mOpacityLabel->setText(tr("Opacity:"));
#ifdef ZOMBOID
    mZomboidLayerLabel->setText(tr("Visibility:"));
#endif
}


LayerView::LayerView(QWidget *parent):
    QTreeView(parent),
    mMapDocument(nullptr)
{
    setRootIsDecorated(false);
    setHeaderHidden(true);
    setItemsExpandable(false);
    setUniformRowHeights(true);
}

QSize LayerView::sizeHint() const
{
    return QSize(130, 100);
}

void LayerView::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument) {
        mMapDocument->disconnect(this);
        QItemSelectionModel *s = selectionModel();
        disconnect(s, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                   this, SLOT(currentRowChanged(QModelIndex)));
    }

    mMapDocument = mapDocument;

    if (mMapDocument) {
        setModel(mMapDocument->layerModel());

        connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(currentLayerIndexChanged(int)));
        connect(mMapDocument, SIGNAL(editLayerNameRequested()),
                this, SLOT(editLayerName()));

        QItemSelectionModel *s = selectionModel();
        connect(s, SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
                this, SLOT(currentRowChanged(QModelIndex)));

        currentLayerIndexChanged(mMapDocument->currentLayerIndex());
    } else {
        setModel(nullptr);
    }
}

void LayerView::currentRowChanged(const QModelIndex &index)
{
    const int layer = mMapDocument->layerModel()->toLayerIndex(index);
    mMapDocument->setCurrentLayerIndex(layer);
}

void LayerView::currentLayerIndexChanged(int layerIndex)
{
    if (layerIndex > -1) {
        const LayerModel *layerModel = mMapDocument->layerModel();
        int levelIndex = mMapDocument->currentLevelIndex();
        QModelIndex index = layerModel->toIndex(levelIndex, layerIndex);
        setCurrentIndex(index);
    } else {
        setCurrentIndex(QModelIndex());
    }
}

void LayerView::editLayerName()
{
    if (!isVisible())
        return;

    const LayerModel *layerModel = mMapDocument->layerModel();
    const int levelIndex = mMapDocument->currentLevelIndex();
    const int layerIndex = mMapDocument->currentLayerIndex();
    edit(layerModel->toIndex(levelIndex, layerIndex));
}

void LayerView::contextMenuEvent(QContextMenuEvent *event)
{
    if (!mMapDocument)
        return;

    const QModelIndex index = indexAt(event->pos());
    const LayerModel *m = mMapDocument->layerModel();
    const int layerIndex = m->toLayerIndex(index);

    MapDocumentActionHandler *handler = MapDocumentActionHandler::instance();

    QMenu menu;
    menu.addAction(handler->actionAddTileLayer());
    menu.addAction(handler->actionAddObjectGroup());
    menu.addAction(handler->actionAddImageLayer());

    if (layerIndex >= 0) {
        menu.addAction(handler->actionDuplicateLayer());
        menu.addAction(handler->actionMergeLayerDown());
        menu.addAction(handler->actionRemoveLayer());
        menu.addAction(handler->actionRenameLayer());
        menu.addSeparator();
        menu.addAction(handler->actionMoveLayerUp());
        menu.addAction(handler->actionMoveLayerDown());
        menu.addSeparator();
        menu.addAction(handler->actionToggleOtherLayers());
        menu.addSeparator();
        menu.addAction(handler->actionLayerProperties());
    }

    menu.exec(event->globalPos());
}

void LayerView::keyPressEvent(QKeyEvent *event)
{
    if (!mMapDocument)
        return;

    const QModelIndex index = currentIndex();
    if (!index.isValid())
        return;

    const LayerModel *m = mMapDocument->layerModel();
    const int levelIndex = m->toLevelIndex(index);
    const int layerIndex = m->toLayerIndex(index);

    if (event->key() == Qt::Key_Delete) {
        mMapDocument->removeLayer(levelIndex, layerIndex);
        return;
    }

    QTreeView::keyPressEvent(event);
}
