/*
 * zlevelsmodel.h
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

#ifndef ZLEVELSMODEL_H
#define ZLEVELSMODEL_H

#include <QAbstractItemModel>
#include <QIcon>

class CompositeLayerGroup;

namespace Tiled {

class Map;
class TileLayer;

namespace Internal {

class MapDocument;

class ZLevelsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ZLevelsModel(QObject *parent = 0);
    ~ZLevelsModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(CompositeLayerGroup *og) const;
    QModelIndex index(TileLayer *o) const;

    CompositeLayerGroup *toLayerGroup(const QModelIndex &index) const;
    TileLayer *toLayer(const QModelIndex &index) const;

    void setMapDocument(MapDocument *mapDocument);
    MapDocument *mapDocument() const { return mMapDocument; }

private slots:
    void layerChanged(int index);

    void layerGroupAdded(int level);
    void layerGroupVisibilityChanged(CompositeLayerGroup *g);

    void layerAddedToGroup(int layerIndex);
    void layerAboutToBeRemovedFromGroup(int layerIndex);

private:
    class Item
    {
    public:
        Item()
            : parent(0)
            , group(0)
            , layer(0)
        {

        }

        Item(Item *parent, int indexInParent, CompositeLayerGroup *g)
            : parent(parent)
            , group(g)
            , layer(0)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, TileLayer *tl)
            : parent(parent)
            , group(0)
            , layer(tl)
        {
            parent->children.insert(indexInParent, this);
        }

        CompositeLayerGroup *group;
        TileLayer *layer;
        Item *parent;
        QList<Item*> children;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(CompositeLayerGroup *g) const;
    Item *toItem(TileLayer *tl) const;

    MapDocument *mMapDocument;
    Map *mMap;
    Item *mRootItem;
};

} // namespace Internal
} // namespace Tiled

#endif // ZLEVELSMODEL_H
