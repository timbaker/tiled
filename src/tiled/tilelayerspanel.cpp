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

#include "tilelayerspanel.h"

#include "mapcomposite.h"
#include "mapdocument.h"
#include "zoomable.h"

#include "BuildingEditor/mixedtilesetview.h"
#include "BuildingEditor/buildingtiles.h"

#include <QDebug>
#include <QEvent>
#include <QScrollBar>
#include <QVBoxLayout>

using namespace Tiled;
using namespace Tiled::Internal;

Zoomable *TileLayersPanel::mZoomable = new Zoomable();

TileLayersPanel::TileLayersPanel(QWidget *parent) :
    QWidget(parent),
    mDocument(0),
    mView(new MixedTilesetView(mZoomable)),
    mCurrentLayerIndex(-1),
    mResizeLater(false)
{
    setFixedWidth(80); // FIXME: resize to contents

//    mView->zoomable()->setScale(0.5); // TODO: save/restore
    mView->model()->setColumnCount(1);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(mView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentChanged()));
    connect(mView->zoomable(), SIGNAL(scaleChanged(qreal)), SLOT(resizeToContents()));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mView);
    setLayout(layout);
}

void TileLayersPanel::setDocument(MapDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    mView->clear();

    if (mDocument) {
        connect(mDocument, SIGNAL(currentLayerIndexChanged(int)),
                SLOT(layerIndexChanged(int)));
        connect(mDocument, SIGNAL(layerAdded(int)), SLOT(setList()));
        connect(mDocument, SIGNAL(layerRemoved(int)), SLOT(setList()));
        connect(mDocument, SIGNAL(regionAltered(QRegion,Layer*)),
                SLOT(regionAltered(QRegion,Layer*)));
        setList();
    }
}

void TileLayersPanel::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    resizeToContentsLater();
}

void TileLayersPanel::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    resizeToContentsLater();
}

void TileLayersPanel::setTilePosition(const QPoint &tilePos)
{
    mTilePos = tilePos;
    setList();
}

void TileLayersPanel::setList()
{
    int level = mDocument->currentLevel();

    CompositeLayerGroup *lg = mDocument->mapComposite()->layerGroupForLevel(level);
    if (!lg) return;

    QList<Tile*> tiles;
    QStringList headers;
    QList<void*> userData;
    foreach (TileLayer *tl, lg->layers()) {
        headers.prepend(MapComposite::layerNameWithoutPrefix(tl));
        Tile *tile = tl->contains(mTilePos) ? tl->cellAt(mTilePos).tile : 0;
        if (!tile)
            tile = BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
        tiles.prepend(tile);
        userData.prepend((void*)(mDocument->map()->layers().indexOf(tl) + 1));
    }

    mView->setTiles(tiles, userData, headers);
    mView->setCurrentIndex(mView->model()->index((void*)(mCurrentLayerIndex+1)));

    if (isVisible() && !mResizeLater) {
        QMetaObject::invokeMethod(this, "resizeToContents", Qt::QueuedConnection);
        mResizeLater = true;
    }
}

void TileLayersPanel::currentChanged()
{
    QModelIndex index = mView->currentIndex();
    if (void *userData = mView->model()->userDataAt(index)) {
        int layerIndex = ((int)userData) - 1;
//        qDebug() << "TileLayersPanel::currentChanged() layerIndex=" << layerIndex;
        if (layerIndex == mCurrentLayerIndex)
            return;
        mCurrentLayerIndex = layerIndex;
        mDocument->setCurrentLayerIndex(layerIndex);

        Layer *layer = mDocument->map()->layerAt(layerIndex);
        if (!layer) return;
        if (TileLayer *tl = layer->asTileLayer()) {
            if (tl->contains(mTilePos) && tl->cellAt(mTilePos).tile)
                emit tilePicked(tl->cellAt(mTilePos).tile);
        }
    }
}

void TileLayersPanel::layerIndexChanged(int index)
{
    if (mCurrentLayerIndex == index)
        return;
    mCurrentLayerIndex = index;
    setList();
}

void TileLayersPanel::regionAltered(const QRegion &region, Layer *layer)
{
    if (layer->asTileLayer() && (layer->level() == mDocument->currentLevel())
            && region.contains(mTilePos))
        setList();
}

void TileLayersPanel::resizeToContents()
{
    mResizeLater = false;
    int height = 0;
    for (int row = 0; row < mView->model()->rowCount(); row++)
        height += mView->rowHeight(row);

    int width = mView->columnWidth(0);
    width = qMax(width, 40);
    if (height > mView->viewport()->rect().height())
        width += mView->verticalScrollBar()->sizeHint().width();
    setFixedWidth(width);
}

void TileLayersPanel::resizeToContentsLater()
{
    if (!mResizeLater) {
        QMetaObject::invokeMethod(this, "resizeToContents", Qt::QueuedConnection);
        mResizeLater = true;
    }
}
