/*
 * movelayer.cpp
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "movelayer.h"

#include "layer.h"
#include "layermodel.h"
#include "mapdocument.h"
#include "maplevel.h"

#include <QCoreApplication>

using namespace Tiled::Internal;

MoveLayer::MoveLayer(MapDocument *mapDocument, int levelIndex, int layerIndex, Direction direction):
    mMapDocument(mapDocument),
    mLevelIndex(levelIndex),
    mLayerIndex(layerIndex),
    mDirection(direction)
{
    setText((direction == Down) ?
            QCoreApplication::translate("Undo Commands", "Lower Layer") :
            QCoreApplication::translate("Undo Commands", "Raise Layer"));
}

void MoveLayer::redo()
{
    moveLayer();
}

void MoveLayer::undo()
{
    moveLayer();
}

void MoveLayer::moveLayer()
{
    MapLevel *mapLevel = mMapDocument->map()->levelAt(mLevelIndex);
    const int currentIndex = mMapDocument->currentLayerIndex();
    const bool wasSelected = (mLayerIndex == currentIndex);
    const int prevIndex = mLayerIndex;

    LayerModel *layerModel = mMapDocument->layerModel();
    Layer *layer = layerModel->takeLayerAt(mLevelIndex, mLayerIndex);

    // Change the direction and index to swap undo/redo
    switch (mDirection) {
    case Up:
        if (mLayerIndex == mapLevel->layerCount()) {
            mLevelIndex++;
            mLayerIndex = 0;
            layer->setLevel(mLevelIndex);
        } else {
            mLayerIndex++;
        }
        mDirection = Down;
        break;
    case Down:
        if (mLayerIndex == 0) {
            mLevelIndex--;
            MapLevel *mapLevelBelow = mMapDocument->map()->levelAt(mLevelIndex);
            mLayerIndex = mapLevelBelow->layerCount();
            layer->setLevel(mLevelIndex);
        } else {
            mLayerIndex--;
        }
        mDirection = Up;
        break;
    }

    const bool isSelected = (mLayerIndex == currentIndex);

    layerModel->insertLayer(mLayerIndex, layer);

    // Set the layer that is now supposed to be selected
    mMapDocument->setCurrentLayerIndex(mLevelIndex,
                wasSelected ? mLayerIndex :
                                 (isSelected ? prevIndex : currentIndex));
}
