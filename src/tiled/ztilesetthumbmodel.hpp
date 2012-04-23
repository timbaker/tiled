/*
 * ztilesetthumbmodel.h
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

#ifndef ZTILESETTHUMBMODEL_H
#define ZTILESETTHUMBMODEL_H

#include <QAbstractListModel>

namespace Tiled {

class Tile;
class Tileset;

namespace Internal {

class MapDocument;

class ZTilesetThumbModel : public QAbstractListModel
{
	Q_OBJECT

public:
    ZTilesetThumbModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

	Qt::ItemFlags flags(const QModelIndex &index) const;

    Tileset *tilesetAt(const QModelIndex &index) const;
    Tile *tileAt(const QModelIndex &index) const;

	QModelIndex index(Tileset *ts) const;

	void setMapDocument(MapDocument *mapDoc);

private slots:
    void tilesetAdded(int index, Tileset *tileset);
    void tilesetChanged(Tileset *tileset);
    void tilesetRemoved(Tileset *tileset);
    void tilesetMoved(int from, int to);
    void tilesetNameChanged(Tileset *tileset);
	void tilesetFileNameChanged(Tileset *tileset);
    void tilesetThumbIndexChanged(Tileset *tileset);
    void tilesetThumbNameChanged(Tileset *tileset);

private:
	MapDocument *mMapDocument;
};

} // namespace Internal
} // namespace Tiled

#endif // ZTILESETTHUMBMODEL_H
