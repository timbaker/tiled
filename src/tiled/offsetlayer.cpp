/*
 * offsetlayer.cpp
 * Copyright 2009, Jeff Bland <jeff@teamphobic.com>
 * Copyright 2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "offsetlayer.h"

#include "layer.h"
#include "layermodel.h"
#include "map.h"
#include "mapdocument.h"
#include "maplevel.h"

#include <QCoreApplication>

using namespace Tiled;
using namespace Tiled::Internal;

OffsetLayer::OffsetLayer(MapDocument *mapDocument,
                         int levelIndex,
                         int layerIndex,
                         const QPoint &offset,
                         const QRect &bounds,
                         bool wrapX,
                         bool wrapY)
    : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                               "Offset Layer"))
    , mMapDocument(mapDocument)
    , mLevelIndex(levelIndex)
    , mLayerIndex(layerIndex)
    , mOriginalLayer(nullptr)
{
    // Create the offset layer (once)
    MapLevel *mapLevel = mMapDocument->map()->levelAt(mLevelIndex);
    Layer *layer = mapLevel->layerAt(mLayerIndex);
    mOffsetLayer = layer->clone();
    mOffsetLayer->offset(offset, bounds, wrapX, wrapY);
}

OffsetLayer::~OffsetLayer()
{
    delete mOriginalLayer;
    delete mOffsetLayer;
}

void OffsetLayer::undo()
{
    Q_ASSERT(!mOffsetLayer);
    mOffsetLayer = swapLayer(mOriginalLayer);
    mOriginalLayer = nullptr;
}

void OffsetLayer::redo()
{
    Q_ASSERT(!mOriginalLayer);
    mOriginalLayer = swapLayer(mOffsetLayer);
    mOffsetLayer = nullptr;
}

Layer *OffsetLayer::swapLayer(Layer *layer)
{
    const int currentLevelIndex = mMapDocument->currentLevelIndex();
    const int currentIndex = mMapDocument->currentLayerIndex();

    LayerModel *layerModel = mMapDocument->layerModel();
    Layer *replaced = layerModel->takeLayerAt(mLevelIndex, mLayerIndex);
    layerModel->insertLayer(mLayerIndex, layer);

    if (mLevelIndex == currentLevelIndex && mLayerIndex == currentIndex)
        mMapDocument->setCurrentLayerIndex(mLevelIndex, mLayerIndex);

    return replaced;
}
