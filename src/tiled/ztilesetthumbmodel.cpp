/*
 * ztilesetthumbmodel.cpp
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

#include "ztilesetthumbmodel.hpp"

#include "map.h"
#include "mapdocument.h"
#include "tile.h"
#include "tileset.h"
#include "tilesetmanager.h"

using namespace Tiled;
using namespace Tiled::Internal;

ZTilesetThumbModel::ZTilesetThumbModel(QObject *parent)
	: QAbstractListModel(parent)
	, mMapDocument(0)
{
}

int ZTilesetThumbModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
	if (!mMapDocument)
		return 0;
	return mMapDocument->map()->tilesets().count();
}

int ZTilesetThumbModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

QVariant ZTilesetThumbModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (Tile *tile = tileAt(index))
            return tile->image();
    }

    if (role == Qt::DecorationRole) {
        if (Tileset *ts = tilesetAt(index)) {
            QString &thumbName = TilesetManager::instance()->thumbName(ts);
			return thumbName.isEmpty() ? ts->name() : thumbName;
		}
    }

    return QVariant();
}

QVariant ZTilesetThumbModel::headerData(int /* section */,
                                  Qt::Orientation /* orientation */,
                                  int role) const
{
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

Tileset *ZTilesetThumbModel::tilesetAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

	if (!mMapDocument)
		return 0;
	return mMapDocument->map()->tilesets().at(index.row());
}

Tile *ZTilesetThumbModel::tileAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

	if (!mMapDocument)
		return 0;

	Tileset *ts = mMapDocument->map()->tilesets().at(index.row());
	int thumbIndex = TilesetManager::instance()->thumbIndex(ts);
	return ts->tileAt(thumbIndex >= 0 ? thumbIndex : 0);
}

QModelIndex ZTilesetThumbModel::index(Tileset *ts) const
{
	const int row = mMapDocument->map()->tilesets().indexOf(ts);
	return createIndex(row, 0, 0);
}

void ZTilesetThumbModel::setMapDocument(MapDocument *mapDoc)
{
	if (mMapDocument == mapDoc)
		return;
	if (mMapDocument)
		mMapDocument->disconnect(this);
	beginResetModel();
	mMapDocument = mapDoc;
	if (mMapDocument) {
        connect(mMapDocument, SIGNAL(tilesetAdded(int,Tileset*)),
                SLOT(tilesetAdded(int,Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetRemoved(Tileset*)),
                SLOT(tilesetRemoved(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetMoved(int,int)),
                SLOT(tilesetMoved(int,int)));
        connect(mMapDocument, SIGNAL(tilesetNameChanged(Tileset*)),
                SLOT(tilesetNameChanged(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetFileNameChanged(Tileset*)),
                SLOT(tilesetFileNameChanged(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetThumbIndexChanged(Tileset*)),
                SLOT(tilesetThumbIndexChanged(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetThumbNameChanged(Tileset*)),
                SLOT(tilesetThumbNameChanged(Tileset*)));
	}
	endResetModel();
}

void ZTilesetThumbModel::tilesetAdded(int index, Tileset *tileset)
{
	reset();
}

void ZTilesetThumbModel::tilesetChanged(Tileset *tileset)
{
	reset();
}

void ZTilesetThumbModel::tilesetRemoved(Tileset *tileset)
{
	reset();
}

void ZTilesetThumbModel::tilesetMoved(int from, int to)
{
	reset();
}

void ZTilesetThumbModel::tilesetNameChanged(Tileset *tileset)
{
	QModelIndex index = this->index(tileset);
	emit dataChanged(index, index);
}

void ZTilesetThumbModel::tilesetFileNameChanged(Tileset *tileset)
{
}

void ZTilesetThumbModel::tilesetThumbIndexChanged(Tileset *tileset)
{
	QModelIndex index = this->index(tileset);
	emit dataChanged(index, index);
}

void ZTilesetThumbModel::tilesetThumbNameChanged(Tileset *tileset)
{
	QModelIndex index = this->index(tileset);
	emit dataChanged(index, index);
}
