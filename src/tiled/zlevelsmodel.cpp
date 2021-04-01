/*
 * zlevelsmodel.cpp
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

#include "zlevelsmodel.h"

#include "map.h"
#include "layermodel.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "maplevel.h"
#include "renamelayer.h"
#include "tilelayer.h"

using namespace Tiled;
using namespace Tiled::Internal;

ZLevelsModel::ZLevelsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mMapDocument(nullptr)
    , mMap(nullptr)
    , mRootItem(nullptr),
    mTileLayerIcon(QLatin1String(":/images/16x16/layer-tile.png")),
    mObjectGroupIcon(QLatin1String(":/images/16x16/layer-object.png")),
    mImageLayerIcon(QLatin1String(":/images/16x16/layer-image.png"))
{
}

ZLevelsModel::~ZLevelsModel()
{
    delete mRootItem;
}

QModelIndex ZLevelsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mRootItem)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row < mRootItem->children.size())
            return createIndex(row, column, mRootItem->children.at(row));
        return QModelIndex();
    }

    if (Item *item = toItem(parent)) {
        if (row < item->children.size())
            return createIndex(row, column, item->children.at(row));
        return QModelIndex();
    }

    return QModelIndex();
}

QModelIndex ZLevelsModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int ZLevelsModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int ZLevelsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant ZLevelsModel::data(const QModelIndex &index, int role) const
{
    Item *item = toItem(index);
    if (!item)
        return QVariant();

    if (Layer *layer = item->layer) {
        switch (role) {
        case Qt::DisplayRole:
            if (layer->isTileLayer() && !layer->asTileLayer()->group())
                return layer->name() + QLatin1String(" <no level>");
            return layer->name(); // MapComposite::layerNameWithoutPrefix(layer);
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
            return QVariant();
        case Qt::CheckStateRole:
            return layer->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    if (item->level >= 0) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return QString(tr("Level %1")).arg(item->level);
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            if (CompositeLayerGroup *g = mMapDocument->mapComposite()->layerGroupForLevel(item->level))
                return g->isVisible() ? Qt::Checked : Qt::Unchecked;
            return QVariant();
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool ZLevelsModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    Item *item = toItem(index);
    if (!item)
        return false;

    if (Layer *layer = item->layer) {
        switch (role) {
        case Qt::CheckStateRole: {
            LayerModel *layerModel = mMapDocument->layerModel();
            MapLevel *mapLevel = mMap->levelAt(layer->level());
            const int layerIndex = mapLevel->layers().indexOf(layer);
            layerModel->setData(layerModel->toIndex(layer->level(), layerIndex), value, role);
            return true;
        }
        case Qt::EditRole: {
            const QString newName = value.toString();
            if (layer->name() != newName) {
                const int layerIndex = mMap->layers().indexOf(layer);
                RenameLayer *rename = new RenameLayer(mMapDocument, layer->level(), layerIndex, newName);
                mMapDocument->undoStack()->push(rename);
            }
            return true;
        }
        }
        return false;
    }
    if (item->level >= 0) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            bool visible = c == Qt::Checked;
            if (CompositeLayerGroup *g = mMapDocument->mapComposite()->layerGroupForLevel(item->level))
                mMapDocument->setLayerGroupVisibility(g, visible);
            return true;
        }
        case Qt::EditRole: {
            return false;
        }
        }
        return false;
    }
    return false;
}

Qt::ItemFlags ZLevelsModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    rc |= Qt::ItemIsUserCheckable;
    if (index.parent().isValid())
        rc |= Qt::ItemIsEditable; // TileLayer name
    return rc;
}
QVariant ZLevelsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        }
    }
    return QVariant();
}

QModelIndex ZLevelsModel::index(int level) const
{
    Item *item = toItem(level);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex ZLevelsModel::index(CompositeLayerGroup *g) const
{
    Item *item = toItem(g);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex ZLevelsModel::index(Layer *layer) const
{
    Item *item = toItem(layer);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

CompositeLayerGroup *ZLevelsModel::toLayerGroup(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->level >= 0)
            return mMapDocument->mapComposite()->layerGroupForLevel(item->level);
    }
    return nullptr;
}

Layer *ZLevelsModel::toLayer(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->layer;
    return nullptr;
}

// FIXME: Each MapDocument has its own persistent LevelsModel, so this only does anything useful
// one time.
void ZLevelsModel::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    beginResetModel();

    mMapDocument = mapDocument;
    mMap = nullptr;

    delete mRootItem;
    mRootItem = nullptr;

    if (mMapDocument) {
        mMap = mMapDocument->map();

        connect(mMapDocument, &MapDocument::layerChanged,
                this, &ZLevelsModel::layerChanged);
        connect(mMapDocument, &MapDocument::layerGroupVisibilityChanged,
                this, &ZLevelsModel::layerGroupVisibilityChanged);
        connect(mMapDocument, &MapDocument::layerLevelChanged,
                this, &ZLevelsModel::layerLevelChanged);
        connect(mMapDocument, &MapDocument::layerAdded,
                this, &ZLevelsModel::layerAdded);
        connect(mMapDocument, &MapDocument::layerAboutToBeRemoved,
                this, &ZLevelsModel::layerAboutToBeRemoved);

        mRootItem = new Item();

        foreach (Layer *layer, mMapDocument->map()->layers())
            new Item(createLevelItemIfNeeded(layer->level()), 0, layer);
    }

    endResetModel();
}

QList<int> ZLevelsModel::levels() const
{
    QList<int> ret;
    for (Item *item : mRootItem->children) {
        ret += item->level;
    }
    return ret;
}

void ZLevelsModel::layerChanged(int z, int layerIndex)
{
    // Handle name, visibility changes
    Layer *layer = mMap->layerAt(z, layerIndex);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (/*Item *item =*/ toItem(tl)) {
            QModelIndex index = this->index(tl);
            emit dataChanged(index, index);
        }
    }
}

void ZLevelsModel::layerGroupVisibilityChanged(CompositeLayerGroup *g)
{
    QModelIndex index = this->index(g);
    emit dataChanged(index, index);
}

void ZLevelsModel::layerAdded(int z, int layerIndex)
{
    layerLevelChanged(z, layerIndex, -1);
}

void ZLevelsModel::layerAboutToBeRemoved(int z, int layerIndex)
{
    removeLayerFromLevel(z, layerIndex, z);
}

void ZLevelsModel::layerLevelChanged(int z, int layerIndex, int oldLevel)
{
    if (oldLevel != -1)
        removeLayerFromLevel(z, layerIndex, oldLevel);

    Layer *layer = mMap->layerAt(z, layerIndex);
    if (Item *parentItem = createLevelItemIfNeeded(layer->level())) {
        int row;
        for (row = 0; row < parentItem->children.size(); row++) {
            Layer *otherLayer = parentItem->children[row]->layer;
            int otherIndex = mMap->layers().indexOf(otherLayer);
            if (otherIndex < layerIndex)
                break;
        }
        QModelIndex parent = index(parentItem->level);
        beginInsertRows(parent, row, row);
        new Item(parentItem, row, layer);
        endInsertRows();
    }
}

ZLevelsModel::Item *ZLevelsModel::createLevelItemIfNeeded(int level)
{
    int index;
    for (index = 0; index < mRootItem->children.size(); index++) {
        Item *item = mRootItem->children[index];
        if (item->level == level)
            return item;
        if (item->level < level)
            break;
    }
    beginInsertRows(QModelIndex(), index, index);
    Item *item = new Item(mRootItem, index, level);
    endInsertRows();
    return item;
}

void ZLevelsModel::removeLayerFromLevel(int z, int layerIndex, int oldLevel)
{
    Layer *layer = mMap->layerAt(z, layerIndex);
    if (Item *parentItem = toItem(oldLevel)) {
        int row = parentItem->indexOf(layer);
        Q_ASSERT(row >= 0);
        if (row >= 0) {
            QModelIndex parent = index(parentItem->level);
            beginRemoveRows(parent, row, row);
            delete parentItem->children.takeAt(row);
            endRemoveRows();
        }

        if (parentItem->children.isEmpty()) {
            row = mRootItem->children.indexOf(parentItem);
            beginRemoveRows(QModelIndex(), row, row);
            delete mRootItem->children.takeAt(row);
            endRemoveRows();
        }
    }
}

ZLevelsModel::Item *ZLevelsModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

ZLevelsModel::Item *ZLevelsModel::toItem(CompositeLayerGroup *g) const
{
    if (!mRootItem)
        return nullptr;
    for (Item *item : mRootItem->children)
        if (item->level == g->level())
            return item;
    return nullptr;
}

ZLevelsModel::Item *ZLevelsModel::toItem(int level) const
{
    if (!mRootItem)
        return nullptr;
    for (Item *item : mRootItem->children)
        if (item->level == level)
            return item;
    return nullptr;
}

ZLevelsModel::Item *ZLevelsModel::toItem(Layer *layer) const
{
    if (!mRootItem)
        return nullptr;
    Item *parent = toItem(layer->level());
    for (Item *item : parent->children)
        if (item->layer == layer)
            return item;
    return nullptr;
}
