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
        Item(int levelIndex)
            : mLevelIndex(levelIndex)
            , mLayerIndex(-1)
            , mParent(nullptr)
        {

        }

        Item(Item *parent, int layerIndex)
            : mLevelIndex(parent->mLevelIndex)
            , mLayerIndex(layerIndex)
            , mParent(parent)
        {
        }

        ~Item()
        {
            qDeleteAll(mChildren);
        }

        void insertLayer(int layerIndex)
        {
            Item *child = new LayerModelPrivate::Item(this, layerIndex);
            int index = mChildren.size() + 1 - layerIndex - 1;
            mChildren.insert(index, child);
            for (int i = index - 1; i >= 0; i--) {
                mChildren[i]->mLayerIndex++;
            }
        }

        void removeLayer(int layerIndex)
        {
            int index = mChildren.size() - layerIndex - 1;
            delete mChildren.takeAt(index);
            for (int i = index - 1; i >= 0; i--) {
                mChildren[i]->mLayerIndex--;
            }
        }

        int mLevelIndex;
        int mLayerIndex;
        Item *mParent;
        QVector<Item*> mChildren;
    };

    LayerModelPrivate(LayerModel &model)
        : mModel(model)
    {

    }

    ~LayerModelPrivate()
    {
        qDeleteAll(mItems);
    }

    void setMap(Map *map)
    {
        qDeleteAll(mItems);
        mItems.clear();
        // Levels and layers from highest to lowest.
        for (int z = map->levelCount() - 1; z >= 0; z--) {
            Item *item = new Item(z);
            mItems += item;
            MapLevel *mapLevel = map->levelAt(z);
            for (int i = mapLevel->layerCount() - 1; i >= 0; i--) {
                Item *item1 = new Item(item, i);
                item->mChildren += item1;
            }
        }
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
    QAbstractItemModel(parent),
    mMapDocument(nullptr),
    mMap(nullptr),
    mTileLayerIcon(QLatin1String(":/images/16x16/layer-tile.png")),
    mObjectGroupIcon(QLatin1String(":/images/16x16/layer-object.png")),
    mImageLayerIcon(QLatin1String(":/images/16x16/layer-image.png")),
    mPrivate(new LayerModelPrivate(*this))
{
}

LayerModel::~LayerModel()
{
    delete mPrivate;
}

QModelIndex LayerModel::parent(const QModelIndex &index) const
{
    if (LayerModelPrivate::Item *item = mPrivate->toItem(index)) {
        if (item->mParent != nullptr) {
            return toIndex(item->mLevelIndex);
        }
    }
    return QModelIndex();
}

int LayerModel::rowCount(const QModelIndex &parent) const
{
    if (mMap == nullptr) {
        return 0;
    }
    if (!parent.isValid()) {
        return mPrivate->mItems.size();
    }
    if (LayerModelPrivate::Item *item = mPrivate->toItem(parent)) {
        if (item->mParent == nullptr) { // A MapLevel
            return item->mChildren.size();
        }
    }
    return 0;
}

int LayerModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant LayerModel::data(const QModelIndex &index, int role) const
{
    const int levelIndex = toLevelIndex(index);
    if (levelIndex < 0)
        return QVariant();

    const int layerIndex = toLayerIndex(index);
    if (layerIndex < 0)
    {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return QString(tr("Level %1")).arg(levelIndex);
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            if (MapLevel *mapLevel = mMapDocument->map()->levelAt(levelIndex)) {
                return mapLevel->isVisible() ? Qt::Checked : Qt::Unchecked;
            }
            return QVariant();
        default:
            return QVariant();
        }
    }

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
        Q_ASSERT(false);
        return QVariant();
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
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
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

QModelIndex LayerModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mMap)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row < mPrivate->mItems.size()) {
            return createIndex(row, column, mPrivate->mItems[row]);
        }
        return QModelIndex();
    }

    if (LayerModelPrivate::Item *item = mPrivate->toItem(parent)) {
        if (row < item->mChildren.size()) {
            return createIndex(row, column, item->mChildren[row]);
        }
        return QModelIndex();
    }

    return QModelIndex();
}

int LayerModel::toLevelIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        return mPrivate->toItem(index)->mLevelIndex;
    }
    return -1;
}

int LayerModel::toLayerIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        return mPrivate->toItem(index)->mLayerIndex; // mMap->layerCount() - index.row() - 1;
    }
    return -1;
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
    mPrivate->setMap(mMap);
    endResetModel();
}

void LayerModel::insertLayer(int index, Layer *layer)
{
    const int row = layerIndexToRow(layer->level(), index) + 1;
    QModelIndex parent = toIndex(layer->level());
    LayerModelPrivate::Item *parentItem = mPrivate->toItem(parent);
    beginInsertRows(parent, row, row);
    mMap->insertLayer(index, layer);
    parentItem->insertLayer(index);
    endInsertRows();
    emit layerAdded(layer->level(), index);
}

Layer *LayerModel::takeLayerAt(int z, int index)
{
    emit layerAboutToBeRemoved(z, index);
    QModelIndex parent = toIndex(z);
    LayerModelPrivate::Item *parentItem = mPrivate->toItem(parent);
    const int row = layerIndexToRow(z, index);
    beginRemoveRows(parent, row, row);
    Layer *layer = mMap->takeLayerAt(z, index);
    parentItem->removeLayer(index);
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

QModelIndex LayerModel::toIndex(int levelIndex) const
{
    if (levelIndex < 0 || levelIndex >= mPrivate->mItems.size())
        return QModelIndex();
    return index(mPrivate->mItems.size() - levelIndex - 1, 0);
}

QModelIndex LayerModel::toIndex(int levelIndex, int layerIndex) const
{
    QModelIndex parent = toIndex(levelIndex);
    return index(layerIndexToRow(levelIndex, layerIndex), 0, parent);
}
