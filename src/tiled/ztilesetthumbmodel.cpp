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

#include "ztilesetthumbmodel.h"

#include "map.h"
#include "mapdocument.h"
#include "preferences.h"
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
    , mSortByName(false)
{
    mSortByName = Preferences::instance()->sortTilesets();
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
        if (Tileset *ts = tilesetAt(index))
            return ts->name();
    }
    if (role == Qt::EditRole) {
        if (Tileset *ts = tilesetAt(index))
            return ts->name();
    }
    if (role == Qt::ForegroundRole) {
        if (Tileset *ts = tilesetAt(index))
            if (ts->isMissing())
                return Qt::red;
    }

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

    return mTilesets.at(index.row());
}

QModelIndex ZTilesetThumbModel::index(Tileset *ts) const
{
    int row = mTilesets.indexOf(ts);
    return createIndex(row, 0, 0);
}

void ZTilesetThumbModel::setMapDocument(MapDocument *mapDoc)
{
    if (mMapDocument == mapDoc)
        return;
    if (mMapDocument)
        mMapDocument->disconnect(this);
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
    }
    setModelData();
}

void ZTilesetThumbModel::setSortByName(bool sort)
{
    if (sort != mSortByName) {
        mSortByName = sort;
        setModelData();
    }
}

void ZTilesetThumbModel::setModelData()
{
    beginResetModel();
    mTilesets.clear();
    if (mMapDocument) {
        if (mSortByName) {
            QMap<QString,Tileset*> sorted;
            foreach (Tileset *ts, mMapDocument->map()->tilesets())
                sorted.insertMulti(ts->name(), ts);
            mTilesets = sorted.values();
        } else {
            mTilesets = mMapDocument->map()->tilesets();
        }
    }
    endResetModel();
}

void ZTilesetThumbModel::tilesetAdded(int index, Tileset *tileset)
{
    Q_UNUSED(index)
    Q_UNUSED(tileset)
    setModelData();
}

void ZTilesetThumbModel::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
    setModelData();
}

void ZTilesetThumbModel::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    setModelData();
}

void ZTilesetThumbModel::tilesetMoved(int from, int to)
{
    if (mSortByName)
        return;
    int start = qMin(from, to);
    int end = qMax(from, to);
    // FIXME: should use beingMoveRows()/endMoveRows()
    mTilesets.insert(to, mTilesets.takeAt(from));
    emit dataChanged(createIndex(start, 0, 0), createIndex(end, 0, 0));
}

void ZTilesetThumbModel::tilesetNameChanged(Tileset *tileset)
{
    if (mSortByName) {
        setModelData();
        return;
    }
    QModelIndex index = this->index(tileset);
    emit dataChanged(index, index);
}

void ZTilesetThumbModel::tilesetFileNameChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
}
