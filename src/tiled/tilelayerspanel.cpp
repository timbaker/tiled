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
#include "preferences.h"
#include "zoomable.h"

#include "BuildingEditor/mixedtilesetview.h"
#include "BuildingEditor/buildingtiles.h"

#include <QApplication>
#include <QDebug>
#include <QEvent>
#include <QScrollBar>
#include <QVBoxLayout>

using namespace Tiled;
using namespace Tiled::Internal;

TileLayersPanel::TileLayersPanel(QWidget *parent) :
    QWidget(parent),
    mDocument(0),
    mView(new MixedTilesetView(this)),
    mCurrentLayerIndex(-1)
{
    mView->zoomable()->setScale(0.25);
    mView->model()->setColumnCount(1);
    mView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(mView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentChanged()));
    connect(mView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
    connect(Preferences::instance(), SIGNAL(showTileLayersPanelChanged(bool)),
            SLOT(showTileLayersPanelChanged(bool)));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mView);
    setLayout(layout);

    setVisible(Preferences::instance()->showTileLayersPanel());
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
        connect(mDocument, SIGNAL(layerChanged(int)), SLOT(layerChanged(int)));
        connect(mDocument, SIGNAL(regionAltered(QRegion,Layer*)),
                SLOT(regionAltered(QRegion,Layer*)));
        setList();
    }
}

void TileLayersPanel::setScale(qreal scale)
{
    mView->zoomable()->setScale(scale);
}

qreal TileLayersPanel::scale() const
{
    return mView->zoomable()->scale();
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
}

void TileLayersPanel::activated(const QModelIndex &index)
{
    if (void *userData = mView->model()->userDataAt(index)) {
        int layerIndex = ((int)userData) - 1;
        Layer *layer = mDocument->map()->layerAt(layerIndex);
        if (!layer || !layer->asTileLayer())
            return;
        TileLayer *tl = layer->asTileLayer();
        if (tl->contains(mTilePos) && tl->cellAt(mTilePos).tile)
            emit tilePicked(tl->cellAt(mTilePos).tile);
    }
}

void TileLayersPanel::currentChanged()
{
    QModelIndex index = mView->currentIndex();
    if (void *userData = mView->model()->userDataAt(index)) {
        int layerIndex = ((int)userData) - 1;
        if (layerIndex == mCurrentLayerIndex)
            return;
        mCurrentLayerIndex = layerIndex;
        mDocument->setCurrentLayerIndex(layerIndex);
#if 0
        Layer *layer = mDocument->map()->layerAt(layerIndex);
        if (!layer)
            return;
        if (TileLayer *tl = layer->asTileLayer()) {
            if (tl->contains(mTilePos) && tl->cellAt(mTilePos).tile)
                emit tilePicked(tl->cellAt(mTilePos).tile);
        }
#endif
    }
}

void TileLayersPanel::layerIndexChanged(int index)
{
    if (mCurrentLayerIndex == index)
        return;
    mCurrentLayerIndex = index;
    setList();
}

void TileLayersPanel::layerChanged(int index)
{
    Layer *layer = mDocument->map()->layerAt(index);
    if (layer->asTileLayer() && (layer->level() == mDocument->currentLevel())) {
        setList(); // handle renaming
    }
}

void TileLayersPanel::regionAltered(const QRegion &region, Layer *layer)
{
    if (layer->asTileLayer() && (layer->level() == mDocument->currentLevel())
            && region.contains(mTilePos))
        setList();
}

void TileLayersPanel::showTileLayersPanelChanged(bool show)
{
    setVisible(show);
}
