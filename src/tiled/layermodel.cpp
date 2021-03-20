/*
 * layermodel.cpp
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

#include "layermodel.h"

#include "map.h"
#include "mapdocument.h"
#include "maplevel.h"
#include "layer.h"
#include "renamelayer.h"
#include "tilelayer.h"

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace Tiled
{
namespace Internal
{

class LayerModelPrivate
{
public:
    class Item
    {
    public:
        int mZ;
        int mIndex;
    };

    LayerModelPrivate(LayerModel &model)
        : mModel(model)
    {

    }

    Item *toItem(const QModelIndex &index) const
    {
        if (index.isValid()) {
            return static_cast<Item*>(index.internalPointer());
        }
        return nullptr;
    }

    LayerModel &mModel;
    QList<Item*> mItems;
};

}
}

/////

LayerModel::LayerModel(QObject *parent):
    QAbstractListModel(parent),
    mMapDocument(nullptr),
    mMap(nullptr),
    mTileLayerIcon(QLatin1String(":/images/16x16/layer-tile.png")),
    mObjectGroupIcon(QLatin1String(":/images/16x16/layer-object.png")),
    mImageLayerIcon(QLatin1String(":/images/16x16/layer-image.png")),
    mPrivate(new LayerModelPrivate(*this))
{
}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    if (mMap == nullptr) {
        return 0;
    }
    if (LayerModelPrivate::Item *item = mPrivate->toItem(parent)) {
        if (item->mIndex == -1) { // A MapLevel
            return mMap->levelCount();
        } else {
            return mMap->levelAt(item->mZ)->layerCount();
        }
    }
    return 0;
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    const int levelIndex = toLevelIndex(index);
    if (levelIndex < 0)
        return QVariant();

    const int layerIndex = toLayerIndex(index);
    if (layerIndex < 0)
        return QVariant();

    const MapLevel *mapLevel = mMap->levelAt(levelIndex);
    const Layer *layer = mapLevel->layerAt(layerIndex);

    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
        return layer->name();
    case Qt::DecorationRole:
        if (layer->isTileLayer())
            return mTileLayerIcon;
        else if (layer->isObjectGroup())
            return mObjectGroupIcon;
        else if (layer->isImageLayer())
            return mImageLayerIcon;
        else
            Q_ASSERT(false);
    case Qt::CheckStateRole:
        return layer->isVisible() ? Qt::Checked : Qt::Unchecked;
    case OpacityRole:
        return layer->opacity();
    default:
        return QVariant();
    }
}

bool LayerModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    const int levelIndex = toLevelIndex(index);
    if (levelIndex < 0)
        return false;

    const int layerIndex = toLayerIndex(index);
    if (layerIndex < 0)
        return false;

    const MapLevel *mapLevel = mMap->levelAt(levelIndex);
    Layer *layer = mapLevel->layerAt(layerIndex);

    if (role == Qt::CheckStateRole) {
        Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
        layer->setVisible(c == Qt::Checked);
        emit dataChanged(index, index);
        emit layerChanged(levelIndex, layerIndex);
        return true;
    } else if (role == OpacityRole) {
        bool ok;
        const qreal opacity = value.toDouble(&ok);
        if (ok) {
            if (layer->opacity() != opacity) {
                layer->setOpacity(opacity);
                emit layerChanged(levelIndex, layerIndex);
            }
        }
    } else if (role == Qt::EditRole) {
        const QString newName = value.toString();
        if (layer->name() != newName) {
            RenameLayer *rename = new RenameLayer(mMapDocument, levelIndex, layerIndex,
                                                  value.toString());
            mMapDocument->undoStack()->push(rename);
        }
        return true;
    }

    return false;
}

Qt::ItemFlags LayerModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractListModel::flags(index);
    if (index.column() == 0)
        rc |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    return rc;
}

QVariant LayerModel::headerData(int section, Qt::Orientation orientation,
                                int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Layer");
        }
    }
    return QVariant();
}

int LayerModel::toLevelIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        return mPrivate->toItem(index)->mZ;
    } else {
        return -1;
    }
}

int LayerModel::toLayerIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        return mPrivate->toItem(index)->mIndex; // mMap->layerCount() - index.row() - 1;
    } else {
        return -1;
    }
}

int LayerModel::layerIndexToRow(int levelIndex, int layerIndex) const
{
    MapLevel *mapLevel = mMap->levelAt(levelIndex);
    return mapLevel->layerCount() - layerIndex - 1;
}

void LayerModel::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    beginResetModel();
    mMapDocument = mapDocument;
    mMap = mMapDocument->map();
    endResetModel();
}

void LayerModel::insertLayer(int index, Layer *layer)
{
    const int row = layerIndexToRow(layer->level(), index) + 1;
    QModelIndex parent = toIndex(layer->level());
    beginInsertRows(parent, row, row);
    mMap->insertLayer(index, layer);
    endInsertRows();
    emit layerAdded(layer->level(), index);
}

Layer *LayerModel::takeLayerAt(int z, int index)
{
    emit layerAboutToBeRemoved(z, index);
    QModelIndex parent = toIndex(z);
    const int row = layerIndexToRow(z, index);
    beginRemoveRows(parent, row, row);
    Layer *layer = mMap->takeLayerAt(z, index);
    endRemoveRows();
    emit layerRemoved(z, index);
    return layer;
}

void LayerModel::renameLayer(int z, int layerIndex, const QString &name)
{
    emit layerAboutToBeRenamed(z, layerIndex);
    const QModelIndex modelIndex = toIndex(z, layerIndex);
    Layer *layer = mMap->layerAt(z, layerIndex);
    layer->setName(name);
    emit layerRenamed(z, layerIndex);
    emit dataChanged(modelIndex, modelIndex);
    emit layerChanged(z, layerIndex);
}

void LayerModel::toggleOtherLayers(int z, int layerIndex)
{
    MapLevel *mapLevel = mMap->levelAt(z);
    bool visibility = true;
    for (int i = 0; i < mapLevel->layerCount(); i++) {
        if (i == layerIndex)
            continue;

        Layer *layer = mapLevel->layerAt(i);
        if (layer->isVisible()) {
            visibility = false;
            break;
        }
    }

    for (int i = 0; i < mapLevel->layerCount(); i++) {
        if (i == layerIndex)
            continue;

        const QModelIndex modelIndex = toIndex(z, i);
        Layer *layer = mapLevel->layerAt(i);
        layer->setVisible(visibility);
        emit dataChanged(modelIndex, modelIndex);
        emit layerChanged(z,i);
    }
}
