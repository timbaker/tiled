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
#include "renamelayer.h"
#include "tilelayer.h"

using namespace Tiled;
using namespace Tiled::Internal;

ZLevelsModel::ZLevelsModel(QObject *parent)
    : QAbstractItemModel(parent)
    , mMapDocument(0)
    , mMap(0)
    , mRootItem(0)
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
    if (TileLayer *tl = toLayer(index)) {
        switch (role) {
        case Qt::DisplayRole: {
            int i = tl->name().indexOf(QLatin1Char('_')) + 1;
            return tl->name().mid(i);
        }
        case Qt::EditRole:
            return tl->name();
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            return tl->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    if (ZTileLayerGroup *g = toLayerGroup(index)) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return QString(tr("Level %1")).arg(g->level());
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            return g->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool ZLevelsModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    if (TileLayer *tl = toLayer(index)) {
        switch (role) {
        case Qt::CheckStateRole:
            {
            LayerModel *layerModel = mMapDocument->layerModel();
            const int layerIndex = mMap->layers().indexOf(tl);
            const int row = layerModel->layerIndexToRow(layerIndex);
            layerModel->setData(layerModel->index(row), value, role);
            return true;
            }
        case Qt::EditRole:
            {
            const QString newName = value.toString();
            if (tl->name() != newName) {
                const int layerIndex = mMap->layers().indexOf(tl);
                RenameLayer *rename = new RenameLayer(mMapDocument, layerIndex, newName);
                mMapDocument->undoStack()->push(rename);
            }
            return true;
            }
        }
        return false;
    }
    if (CompositeLayerGroup *g = toLayerGroup(index)) {
        switch (role) {
        case Qt::CheckStateRole:
            {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            bool visible = c == Qt::Checked;
            mMapDocument->setLayerGroupVisibility(g, visible);
            return true;
            }
        case Qt::EditRole:
            {
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

QModelIndex ZLevelsModel::index(CompositeLayerGroup *g) const
{
    Item *item = toItem(g);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex ZLevelsModel::index(TileLayer *tl) const
{
    Item *item = toItem(tl);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

CompositeLayerGroup *ZLevelsModel::toLayerGroup(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->group;
    return 0;
}

TileLayer *ZLevelsModel::toLayer(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->layer;
    return 0;
}

// FIXME: Each MapDocument has its own persistent LevelsModel, so this only does anything useful
// one time.
void ZLevelsModel::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
    mMap = 0;

    delete mRootItem;
    mRootItem = 0;

    if (mMapDocument) {
        mMap = mMapDocument->map();

        connect(mMapDocument, SIGNAL(layerChanged(int)),
                SLOT(layerChanged(int)));
        connect(mMapDocument, SIGNAL(layerGroupAdded(int)),
                SLOT(layerGroupAdded(int)));
        connect(mMapDocument, SIGNAL(layerGroupVisibilityChanged(CompositeLayerGroup*)),
                SLOT(layerGroupVisibilityChanged(CompositeLayerGroup*)));
        connect(mMapDocument, SIGNAL(layerAddedToGroup(int)),
                SLOT(layerAddedToGroup(int)));
        connect(mMapDocument, SIGNAL(layerAboutToBeRemovedFromGroup(int)),
                SLOT(layerAboutToBeRemovedFromGroup(int)));

        mRootItem = new Item();

        foreach (CompositeLayerGroup *g, mMapDocument->mapComposite()->sortedLayerGroups()) {
            Item *parent = new Item(mRootItem, 0, g);
            foreach (TileLayer *tl, g->layers())
                new Item(parent, 0, tl);
        }
    }

    reset();
}

void ZLevelsModel::layerChanged(int layerIndex)
{
    // Handle name, visibility changes
    Layer *layer = mMap->layerAt(layerIndex);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (/*Item *item =*/ toItem(tl)) {
            QModelIndex index = this->index(tl);
            emit dataChanged(index, index);
        }
    }
}

void ZLevelsModel::layerGroupAdded(int level)
{
    MapComposite *mapComposite = mMapDocument->mapComposite();
    CompositeLayerGroup *g = mapComposite->tileLayersForLevel(level);
    Q_ASSERT(g);
    int row = mapComposite->layerGroupCount() - mapComposite->sortedLayerGroups().indexOf(g) - 1;
    QModelIndex parent;
    beginInsertRows(parent, row, row);
    new Item(mRootItem, row, g);
    endInsertRows();
}

void ZLevelsModel::layerGroupVisibilityChanged(CompositeLayerGroup *g)
{
    QModelIndex index = this->index(g);
    emit dataChanged(index, index);
}

void ZLevelsModel::layerAddedToGroup(int layerIndex)
{
    Layer *layer = mMap->layerAt(layerIndex);
    if (TileLayer *tl = layer->asTileLayer()) {
        CompositeLayerGroup *g = (CompositeLayerGroup*) tl->group();
        Q_ASSERT(g);
        Item *parentItem = toItem(g);
        Q_ASSERT(parentItem);
        QModelIndex parent = index(parentItem->group);
        int row = g->layerCount() - g->layers().indexOf(tl) - 1;
        beginInsertRows(parent, row, row);
        new Item(parentItem, row, tl);
        endInsertRows();
    }
}

void ZLevelsModel::layerAboutToBeRemovedFromGroup(int layerIndex)
{
    Layer *layer = mMap->layerAt(layerIndex);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (Item *item = toItem(tl)) {
            Item *parentItem = item->parent;
            Q_ASSERT(tl->group());
            QModelIndex parent = index(parentItem->group);
            int row = parentItem->children.indexOf(item);
            beginRemoveRows(parent, row, row);
            delete parentItem->children.takeAt(row);
            endRemoveRows();
        }
    }
}

ZLevelsModel::Item *ZLevelsModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

ZLevelsModel::Item *ZLevelsModel::toItem(CompositeLayerGroup *g) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->group == g)
            return item;
    return 0;
}

ZLevelsModel::Item *ZLevelsModel::toItem(TileLayer *tl) const
{
    if (!mRootItem)
        return 0;
    if (!tl->group())
        return 0;
    Item *parent = toItem((CompositeLayerGroup*)tl->group());
    foreach (Item *item, parent->children)
        if (item->layer == tl)
            return item;
    return 0;
}
