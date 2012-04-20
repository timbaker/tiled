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

namespace Tiled {

class Map;
class TileLayer;
class ZTileLayerGroup;

namespace Internal {

class MapDocument;

class ZLevelsModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum UserRoles {
        OpacityRole = Qt::UserRole
    };

	struct LayerOrGroup
	{
		LayerOrGroup(ZTileLayerGroup *g)
			: mGroup(g)
			, mLayer(0)
		{
		}
		LayerOrGroup(TileLayer *tl)
			: mGroup(0)
			, mLayer(tl)
		{
		}
		ZTileLayerGroup *mGroup;
		TileLayer *mLayer;
	};

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

	QModelIndex index(ZTileLayerGroup *og) const;
	QModelIndex index(TileLayer *o) const;

	ZTileLayerGroup *toTileLayerGroup(const QModelIndex &index) const;
    TileLayer *toTileLayer(const QModelIndex &index) const;

    int toRow(ZTileLayerGroup *tileLayerGroup) const;
    int toRow(TileLayer *tileLayer) const;

	int toGroupRow(int groupIndex) const;
    int toLayerRow(ZTileLayerGroup *g, int layerIndex) const;

	int toGroupIndex(int row) const;
	int toLayerIndex(ZTileLayerGroup *g, int row) const;

    void setMapDocument(MapDocument *mapDocument);
    MapDocument *mapDocument() const { return mMapDocument; }

	void addTileLayerGroup(ZTileLayerGroup *g);
	void addLayerToGroup(ZTileLayerGroup *g, TileLayer *tl);
	void removeLayerFromGroup(TileLayer *tl);

signals:
	void layerGroupVisibilityChanged(ZTileLayerGroup *g);

private slots:
	void layerAdded(int index);
	void layerChanged(int index);
	void layerAboutToBeRemoved(int index);

private:
    MapDocument *mMapDocument;
    Map *mMap;
	QMap<ZTileLayerGroup*,LayerOrGroup*> mGroupToLOG;
	QMap<TileLayer*,LayerOrGroup*> mLayerToLOG;
};

} // namespace Internal
} // namespace Tiled

#endif // ZLEVELSMODEL_H
