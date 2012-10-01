/*
 * pathmodel.h
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

#ifndef PATHMODEL_H
#define PATHMODEL_H

#include <QAbstractItemModel>
#include <QIcon>

namespace Tiled {

class Map;
class Path;
class PathLayer;

namespace Internal {

class MapDocument;

class PathModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    PathModel(QObject *parent = 0);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(PathLayer *pathLayer) const;
    QModelIndex index(Path *path, int column = 0) const;

    PathLayer *toLayer(const QModelIndex &index) const;
    Path *toPath(const QModelIndex &index) const;

    void setMapDocument(MapDocument *mapDocument);
    MapDocument *mapDocument() const { return mMapDocument; }

    void insertPath(PathLayer *pathLayer, int index, Path *path);
    int removePath(PathLayer *pathLayer, Path *path);
    void emitPathsChanged(const QList<Path *> &paths);

    void setObjectName(MapObject *o, const QString &name);
    void setObjectType(MapObject *o, const QString &type);
    void setObjectPolygon(MapObject *o, const QPolygonF &polygon);
    void setObjectPosition(MapObject *o, const QPointF &pos);
    void setObjectSize(MapObject *o, const QSizeF &size);

signals:
    void pathsAdded(const QList<Path *> &paths);
    void pathsChanged(const QList<Path *> &paths);
    void pathsAboutToBeRemoved(const QList<Path *> &paths);
    void pathsRemoved(const QList<Path *> &paths);

private slots:
    void layerAdded(int index);
    void layerChanged(int index);
    void layerAboutToBeRemoved(int index);

private:
    class Item
    {
    public:
        Item()
            : parent(0)
            , layer(0)
            , path(0)
        {

        }

        Item(Item *parent, int indexInParent, PathLayer *pathLayer)
            : parent(parent)
            , layer(pathLayer)
            , path(0)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, Path *path)
            : parent(parent)
            , layer(0)
            , path(path)
        {
            parent->children.insert(indexInParent, this);
        }

        Item *parent;
        QList<Item*> children;
        PathLayer *layer;
        Path *path;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(PathLayer *pathLayer) const;
    Item *toItem(Path *path) const;

    MapDocument *mMapDocument;
    Map *mMap;
    Item *mRootItem;

    QIcon mPathLayerIcon;
};

} // namespace Internal
} // namespace Tiled

#endif // MAPOBJECTMODEL_H
