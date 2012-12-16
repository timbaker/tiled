/*
 * pathmodel.cpp
 * Copyright 2012, Tim Baker
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

#include "pathmodel.h"

//#include "changepath.h"
#include "layermodel.h"
#include "map.h"
#include "mapdocument.h"
#include "pathlayer.h"
#include "renamelayer.h"

#define GROUPS_IN_DISPLAY_ORDER 1

using namespace Tiled;
using namespace Tiled::Internal;

PathModel::PathModel(QObject *parent):
    QAbstractItemModel(parent),
    mMapDocument(0),
    mMap(0),
    mRootItem(0),
    mPathLayerIcon(QLatin1String(":/images/16x16/layer-tile-stop.png"))
{
}

QModelIndex PathModel::index(int row, int column,
                                  const QModelIndex &parent) const
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

QModelIndex PathModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRootItem) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0,
                               item->parent);
        }
    }
    return QModelIndex();
}

int PathModel::rowCount(const QModelIndex &parent) const
{
    if (!mRootItem)
        return 0;
    if (!parent.isValid())
        return mRootItem->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int PathModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2; // Path name|type
}

QVariant PathModel::data(const QModelIndex &index, int role) const
{
    if (Path *path = toPath(index)) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return index.column() ? QLatin1String("type") : QLatin1String("name");
        case Qt::DecorationRole:
            return QVariant(); // no icon -> maybe the color?
        case Qt::CheckStateRole:
            if (index.column() > 0)
                return QVariant();
            return path->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    if (PathLayer *pathLayer = toLayer(index)) {
        switch (role) {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return index.column() ? QVariant() : pathLayer->name();
        case Qt::DecorationRole:
            return index.column() ? QVariant() : mPathLayerIcon;
        case Qt::CheckStateRole:
            if (index.column() > 0)
                return QVariant();
            return pathLayer->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool PathModel::setData(const QModelIndex &index, const QVariant &value,
                             int role)
{
    if (Path *path = toPath(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            path->setVisible(c == Qt::Checked);
            emit dataChanged(index, index);
            emit pathsChanged(QList<Path*>() << path);
            return true;
        }
        case Qt::EditRole: {
#if 0
            const QString s = value.toString();
            if (index.column() == 0 && s != path->name()) {
                QUndoStack *undo = mMapDocument->undoStack();
                undo->beginMacro(tr("Change Object Name"));
                undo->push(new ChangeMapObject(mMapDocument, path,
                                               s, path->type()));
                undo->endMacro();
            }
            if (index.column() == 1 && s != path->type()) {
                QUndoStack *undo = mMapDocument->undoStack();
                undo->beginMacro(tr("Change Object Type"));
                undo->push(new ChangeMapObject(mMapDocument, path,
                                               path->name(), s));
                undo->endMacro();
            }
#endif
            return true;
        }
        }
        return false;
    }
    if (PathLayer *pathLayer = toLayer(index)) {
        switch (role) {
        case Qt::CheckStateRole: {
            LayerModel *layerModel = mMapDocument->layerModel();
            const int layerIndex = mMap->layers().indexOf(pathLayer);
            const int row = layerModel->layerIndexToRow(layerIndex);
            layerModel->setData(layerModel->index(row), value, role);
            return true;
        }
        case Qt::EditRole: {
            const QString newName = value.toString();
            if (pathLayer->name() != newName) {
                const int layerIndex = mMap->layers().indexOf(pathLayer);
                RenameLayer *rename = new RenameLayer(mMapDocument, layerIndex,
                                                      newName);
                mMapDocument->undoStack()->push(rename);
            }
            return true;
        }
        }
        return false;
    }
    return false;
}

Qt::ItemFlags PathModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (index.column() == 0)
        rc |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    else if (index.parent().isValid())
        rc |= Qt::ItemIsEditable; // Path type
    return rc;
}

QVariant PathModel::headerData(int section, Qt::Orientation orientation,
                                    int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        case 1: return tr("Type");
        }
    }
    return QVariant();
}

QModelIndex PathModel::index(PathLayer *pathLayer) const
{
    Item *item = toItem(pathLayer);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex PathModel::index(Path *path, int column) const
{
    Item *item = toItem(path);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, column, item);
}

PathLayer *PathModel::toLayer(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->layer;
    return 0;
}

Path *PathModel::toPath(const QModelIndex &index) const
{
    if (index.isValid())
        return toItem(index)->path;
    return 0;
}

void PathModel::setMapDocument(MapDocument *mapDocument)
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

        connect(mMapDocument, SIGNAL(layerAdded(int)),
                this, SLOT(layerAdded(int)));
        connect(mMapDocument, SIGNAL(layerChanged(int)),
                this, SLOT(layerChanged(int)));
        connect(mMapDocument, SIGNAL(layerAboutToBeRemoved(int)),
                this, SLOT(layerAboutToBeRemoved(int)));

        mRootItem = new Item();

        foreach (PathLayer *pathLayer, mMap->pathLayers()) {
            Item *parent = new Item(mRootItem, 0, pathLayer);
            foreach (Path *path, pathLayer->paths())
                new Item(parent, 0, path);
        }
    }

    reset();
}

void PathModel::layerAdded(int index)
{
    Layer *layer = mMap->layerAt(index);
    if (PathLayer *pathLayer = layer->asPathLayer()) {
        int row = mMap->pathLayerCount() - mMap->pathLayers().indexOf(pathLayer) - 1;
        beginInsertRows(QModelIndex(), row, row);
        new Item(mRootItem, row, pathLayer);
        endInsertRows();
    }
}

void PathModel::layerChanged(int index)
{
    Layer *layer = mMap->layerAt(index);
    if (PathLayer *pathLayer = layer->asPathLayer()) {
        QModelIndex index = this->index(pathLayer);
        emit dataChanged(index, index);
    }
}

void PathModel::layerAboutToBeRemoved(int index)
{
    Layer *layer = mMap->layerAt(index);
    if (PathLayer *pathLayer = layer->asPathLayer()) {
        if (Item *item = toItem(pathLayer)) {
            Item *parentItem = item->parent; // mRootItem
            int row = parentItem->children.indexOf(item);
            beginRemoveRows(QModelIndex(), row, row);
            delete parentItem->children.takeAt(row);
            endRemoveRows();
        }
    }
}

void PathModel::insertPath(PathLayer *pathLayer, int index, Path *path)
{
    const int row = (index >= 0) ? index : pathLayer->pathCount();
    beginInsertRows(this->index(pathLayer), row, row);
    pathLayer->insertPath(row, path);
    new Item(toItem(pathLayer), row, path);
    endInsertRows();
    emit pathsAdded(QList<Path*>() << path);
}

int PathModel::removePath(PathLayer *pathLayer, Path *path)
{
    emit pathsAboutToBeRemoved(QList<Path*>() << path);
    if (Item *item = toItem(path)) {
        Item *parentItem = item->parent;
        QModelIndex parentIndex = index(/*parentItem->*/pathLayer);
        int row = parentItem->children.indexOf(item);
        beginRemoveRows(parentIndex, row, row);
        pathLayer->removePath(path);
        delete parentItem->children.takeAt(row);
        endRemoveRows();
        emit pathsRemoved(QList<Path*>() << path);
        return row;
    }
    return -1;
}

// PathLayer color changed
// FIXME: layerChanged should let the scene know that objects need redrawing
void PathModel::emitPathsChanged(const QList<Path *> &objects)
{
    emit pathsChanged(objects);
}

void PathModel::movePath(Path *path, const QPoint &delta)
{
    path->translate(delta);
    emit pathsChanged(QList<Path*>() << path);
}

void PathModel::setPathPolygon(Path *path, const QPolygon &polygon)
{
    path->setPolygon(polygon);
    emit pathsChanged(QList<Path*>() << path);
}

#if 0
void PathModel::setObjectName(Path *o, const QString &name)
{
    o->setName(name);
    QModelIndex index = this->index(o);
    emit dataChanged(index, index);
    emit objectsChanged(QList<Path*>() << o);
}

void PathModel::setObjectType(Path *o, const QString &type)
{
    o->setType(type);
    QModelIndex index = this->index(o, 1);
    emit dataChanged(index, index);
    emit objectsChanged(QList<Path*>() << o);
}

void PathModel::setObjectPolygon(Path *o, const QPolygonF &polygon)
{
    o->setPolygon(polygon);
    emit objectsChanged(QList<Path*>() << o);
}

void PathModel::setObjectPosition(Path *o, const QPointF &pos)
{
    o->setPosition(pos);
    emit objectsChanged(QList<Path*>() << o);
}

void PathModel::setObjectSize(Path *o, const QSizeF &size)
{
    o->setSize(size);
    emit objectsChanged(QList<Path*>() << o);
}
#endif

PathModel::Item *PathModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

PathModel::Item *PathModel::toItem(PathLayer *pathLayer) const
{
    if (!mRootItem)
        return 0;
    foreach (Item *item, mRootItem->children)
        if (item->layer == pathLayer)
            return item;
    return 0;
}

PathModel::Item *PathModel::toItem(Path *path) const
{
    if (!mRootItem)
        return 0;
    if (!path->pathLayer())
        return 0;
    Item *parent = toItem(path->pathLayer());
    foreach (Item *item, parent->children)
        if (item->path == path)
            return item;
    return 0;
}

