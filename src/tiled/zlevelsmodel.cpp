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

#include "zlevelsmodel.hpp"

#include "map.h"
#include "layermodel.h"
#include "mapdocument.h"
#include "renamelayer.h"
#include "tilelayer.h"

using namespace Tiled;
using namespace Tiled::Internal;

ZLevelsModel::ZLevelsModel(QObject *parent):
    QAbstractItemModel(parent),
    mMapDocument(0),
    mMap(0)
{
}

ZLevelsModel::~ZLevelsModel()
{
	qDeleteAll(mGroupToLOG);
	qDeleteAll(mLayerToLOG);
}

QModelIndex ZLevelsModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		if (row < mMap->tileLayerGroupCount()) {
			int groupIndex = toGroupIndex(row);
			return createIndex(row, column, mGroupToLOG[mMap->tileLayerGroups().at(groupIndex)]);
		}
		return QModelIndex();
	}

	ZTileLayerGroup *g = toTileLayerGroup(parent);
	if (row >= g->layerCount())
		return QModelIndex(); // happens when deleting the last item in a parent
	int layerIndex = toLayerIndex(g, row);
	if (!mLayerToLOG.contains(g->layers().at(layerIndex)))
		return QModelIndex(); // Paranoia
	return createIndex(row, column, mLayerToLOG[g->layers()[layerIndex]]);
}

QModelIndex ZLevelsModel::parent(const QModelIndex &index) const
{
	TileLayer *tl = toTileLayer(index);
	if (tl)
		return this->index(tl->group());
	return QModelIndex();
}

int ZLevelsModel::rowCount(const QModelIndex &parent) const
{
	if (!mMapDocument)
		return 0;
	if (!parent.isValid())
		return mMap->tileLayerGroupCount();
    if (ZTileLayerGroup *g = toTileLayerGroup(parent))
        return g->layerCount();
    return 0;
}

int ZLevelsModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}

QVariant ZLevelsModel::data(const QModelIndex &index, int role) const
{
	if (TileLayer *tl = toTileLayer(index)) {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return tl->name();
		case Qt::DecorationRole:
			return QVariant();
		case Qt::CheckStateRole:
			return tl->isVisible() ? Qt::Checked : Qt::Unchecked;
		case OpacityRole:
			return qreal(1);
		default:
			return QVariant();
		}
	}
	if (ZTileLayerGroup *g = toTileLayerGroup(index)) {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return QString(tr("Level %1")).arg(g->level());
		case Qt::DecorationRole:
			return QVariant();
		case Qt::CheckStateRole:
			return g->isVisible() ? Qt::Checked : Qt::Unchecked;
		case OpacityRole:
			return qreal(1);
		default:
			return QVariant();
		}
    }
	return QVariant();
}

bool ZLevelsModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
	if (TileLayer *tl = toTileLayer(index)) {
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
	if (ZTileLayerGroup *g = toTileLayerGroup(index)) {
		switch (role) {
		case Qt::CheckStateRole:
			{
			Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
			g->setVisible(c == Qt::Checked);
			emit dataChanged(index, index);
			emit layerGroupVisibilityChanged(g);
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

QModelIndex ZLevelsModel::index(ZTileLayerGroup *g) const
{
	const int row = toRow(g);
	Q_ASSERT(mGroupToLOG[g]);
	return createIndex(row, 0, mGroupToLOG[g]);
}

QModelIndex ZLevelsModel::index(TileLayer *tl) const
{
	Q_ASSERT(tl->group());
	const int row = toRow(tl);
	Q_ASSERT(mLayerToLOG[tl]);
	return createIndex(row, 0, mLayerToLOG[tl]);
}

ZTileLayerGroup *ZLevelsModel::toTileLayerGroup(const QModelIndex &index) const
{
	if (index.isValid()) {
		LayerOrGroup *oog = static_cast<LayerOrGroup*>(index.internalPointer());
		return oog->mGroup;
	}
	return 0;
}

TileLayer *ZLevelsModel::toTileLayer(const QModelIndex &index) const
{
	if (index.isValid()) {
		LayerOrGroup *oog = static_cast<LayerOrGroup*>(index.internalPointer());
		return oog->mLayer;
	}
	return 0;
}

int ZLevelsModel::toRow(ZTileLayerGroup *tileLayerGroup) const
{
	Q_ASSERT(mMap->tileLayerGroups().contains(tileLayerGroup));
	return toGroupRow(mMap->tileLayerGroups().indexOf(tileLayerGroup));
}

int ZLevelsModel::toRow(TileLayer *tileLayer) const
{
	ZTileLayerGroup *g = tileLayer->group();
	Q_ASSERT(g);
	Q_ASSERT(g->layers().contains(tileLayer));
	return toLayerRow(g, g->layers().indexOf(tileLayer));
}

int ZLevelsModel::toGroupRow(int groupIndex) const
{
	Q_ASSERT(groupIndex >= 0 && groupIndex <= mMap->tileLayerGroupCount());
	// Display order, reverse of order in the map's list
	return mMap->tileLayerGroupCount() - groupIndex - 1;
}

int ZLevelsModel::toLayerRow(ZTileLayerGroup *g, int layerIndex) const
{
	Q_ASSERT(layerIndex >= 0 && layerIndex <= g->layerCount());
	// Display order, reverse of order in the group's list
	return g->layerCount() - layerIndex - 1;
}

int ZLevelsModel::toGroupIndex(int row) const
{
	// Reverse of toRow
	return mMap->tileLayerGroupCount() - row - 1;
}

int ZLevelsModel::toLayerIndex(ZTileLayerGroup *g, int row) const
{
	// Reverse of toRow
	return g->layerCount() - row - 1;
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

//	mLayerOrGroups.clear();
	qDeleteAll(mGroupToLOG);
	mGroupToLOG.clear();
	qDeleteAll(mLayerToLOG);
	mLayerToLOG.clear();

	if (mMapDocument) {
		mMap = mMapDocument->map();

		connect(mMapDocument, SIGNAL(layerAdded(int)),
				this, SLOT(layerAdded(int)));
		connect(mMapDocument, SIGNAL(layerChanged(int)),
				this, SLOT(layerChanged(int)));
		connect(mMapDocument, SIGNAL(layerAboutToBeRemoved(int)),
				this, SLOT(layerAboutToBeRemoved(int)));

		foreach (ZTileLayerGroup *g, mMap->tileLayerGroups()) {
			mGroupToLOG.insert(g, new LayerOrGroup(g));
			foreach (TileLayer *tl, g->layers())
				mLayerToLOG.insert(tl, new LayerOrGroup(tl));
		}
	}

    reset();
}

void ZLevelsModel::layerAdded(int index)
{
}

void ZLevelsModel::layerChanged(int index)
{
}

void ZLevelsModel::layerAboutToBeRemoved(int layerIndex)
{
}

void ZLevelsModel::addTileLayerGroup(ZTileLayerGroup *g)
{
	Q_ASSERT(!mMap->tileLayerGroups().contains(g));
	Q_ASSERT(!mGroupToLOG.contains(g));
	if (!mGroupToLOG.contains(g)) {
		///// this must match Map::addTileLayerGroup
		int arrayIndex = 0;
		foreach(ZTileLayerGroup *g1, mMap->tileLayerGroups()) {
			if (g1->level() >= g->level())
				break;
			arrayIndex++;
		}
		/////
		const int row = toGroupRow(arrayIndex) + 1; // plus one because mMap->tileLayerGroupCount() is one less before insert
		beginInsertRows(QModelIndex(), row, row);
		mMap->addTileLayerGroup(g);
		mGroupToLOG.insert(g, new LayerOrGroup(g));
		endInsertRows();
	}
}

void ZLevelsModel::addLayerToGroup(ZTileLayerGroup *g, TileLayer *tl)
{
	Q_ASSERT(!g->layers().contains(tl));
	Q_ASSERT(!mLayerToLOG.contains(tl));
	if (!g->layers().contains(tl) && !mLayerToLOG.contains(tl)) {
		int layerIndex = mMap->layers().indexOf(tl);
		///// This must match ZTileLayerGroup::addTileLayer
		int arrayIndex = 0;
		foreach(int index1, g->mIndices) {
			if (index1 >= layerIndex)
				break;
			arrayIndex++;
		}
		/////
		const int row = toLayerRow(g, arrayIndex) + 1; // plus one because g->layerCount() is one less before insert
		beginInsertRows(index(g), row, row);
		g->addTileLayer(tl, layerIndex);
		mLayerToLOG.insert(tl, new LayerOrGroup(tl));
		endInsertRows();
	}
}

void ZLevelsModel::removeLayerFromGroup(TileLayer *tl)
{
	Q_ASSERT(tl->group());
	Q_ASSERT(mLayerToLOG.contains(tl));
	ZTileLayerGroup *g = tl->group();
	if (g && mLayerToLOG.contains(tl)) {
		const int row = toRow(tl);
		beginRemoveRows(index(g), row, row);
		g->removeTileLayer(tl);
		delete mLayerToLOG[tl];
		mLayerToLOG.remove(tl);
		endRemoveRows();
	}
}