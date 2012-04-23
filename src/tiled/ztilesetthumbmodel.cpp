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

#include <QApplication>
#include <QUndoCommand>

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
#if 1
    if (role == Qt::DisplayRole) {
        if (Tileset *ts = tilesetAt(index))
			return ts->name();
	}
    if (role == Qt::EditRole) {
        if (Tileset *ts = tilesetAt(index))
			return ts->name();
	}
#else
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
#endif

    return QVariant();
}

// FIXME: copied this from tilesetdock.cpp
class RenameTileset : public QUndoCommand
{
public:
    RenameTileset(MapDocument *mapDocument,
                  Tileset *tileset,
                  const QString &oldName,
                  const QString &newName)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Tileset name"))
        , mMapDocument(mapDocument)
        , mTileset(tileset)
        , mOldName(oldName)
        , mNewName(newName)
    {
        redo();
    }

    void undo()
    {
        mMapDocument->setTilesetName(mTileset, mOldName);
    }

    void redo()
    {
        mMapDocument->setTilesetName(mTileset, mNewName);
    }

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
    QString mOldName;
    QString mNewName;
};

bool ZTilesetThumbModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
    if (role == Qt::EditRole) {
        if (Tileset *ts = tilesetAt(index)) {
			QString newName = value.toString();
			RenameTileset *rename = new RenameTileset(mMapDocument,
													ts,
													ts->name(), newName);
			mMapDocument->undoStack()->push(rename);
			return true;
		}
	}
	return false;
}

QVariant ZTilesetThumbModel::headerData(int /* section */,
                                  Qt::Orientation /* orientation */,
                                  int role) const
{
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

Qt::ItemFlags ZTilesetThumbModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
	rc |= Qt::ItemIsEditable;
	return rc;
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
	int start = qMin(from, to);
	int end = qMax(from, to);
	// FIXME: should use beingMoveRows()/endMoveRows()
	emit dataChanged(createIndex(start, 0, 0), createIndex(end, 0, 0));
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
