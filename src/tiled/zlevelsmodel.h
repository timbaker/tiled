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

class Layer;
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

    QModelIndex index(int level) const;
    QModelIndex index(CompositeLayerGroup *og) const;
    QModelIndex index(Layer *layer) const;

    CompositeLayerGroup *toLayerGroup(const QModelIndex &index) const;
    Layer *toLayer(const QModelIndex &index) const;

    void setMapDocument(MapDocument *mapDocument);
    MapDocument *mapDocument() const { return mMapDocument; }

    QList<int> levels() const;

private slots:
    void layerChanged(int index);

    void layerGroupVisibilityChanged(CompositeLayerGroup *g);

    void layerAdded(int layerIndex);
    void layerAboutToBeRemoved(int layerIndex);
    void layerLevelChanged(int layerIndex, int oldLevel);

private:
    void removeLayerFromLevel(int layerIndex, int oldLevel);

    class Item
    {
    public:
        Item()
            : parent(0)
            , level(-1)
            , layer(0)
        {

        }

        Item(Item *parent, int indexInParent, int level)
            : parent(parent)
            , level(level)
            , layer(0)
        {
            parent->children.insert(indexInParent, this);
        }

        Item(Item *parent, int indexInParent, Layer *layer)
            : parent(parent)
            , level(-1)
            , layer(layer)
        {
            parent->children.insert(indexInParent, this);
        }

        int indexOf(Layer *layer)
        {
            for (int i = 0; i < children.size(); i++)
                if (children[i]->layer == layer)
                    return i;
            return -1;
        }

        Item *parent;
        QList<Item*> children;
        int level;
        Layer *layer;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(CompositeLayerGroup *g) const;
    Item *toItem(int level) const;
    Item *toItem(Layer *layer) const;

    Item *createLevelItemIfNeeded(int level);

    MapDocument *mMapDocument;
    Map *mMap;
    Item *mRootItem;

    QIcon mTileLayerIcon;
    QIcon mObjectGroupIcon;
    QIcon mImageLayerIcon;
    QIcon mPathLayerIcon;
};

} // namespace Internal
} // namespace Tiled

#endif // ZLEVELSMODEL_H
