/*
 * zmapobjectmodel.cpp
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

#include "zmapobjectmodel.hpp"

#include "changemapobject.h"
#include "map.h"
#include "layermodel.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "renamelayer.h"

#define GROUPS_IN_DISPLAY_ORDER 1

using namespace Tiled;
using namespace Tiled::Internal;

ZMapObjectModel::ZMapObjectModel(QObject *parent):
    QAbstractItemModel(parent),
    mMapDocument(0),
    mMap(0),
    mObjectGroupIcon(QLatin1String(":/images/16x16/layer-object.png"))
{
}

ZMapObjectModel::~ZMapObjectModel()
{
	qDeleteAll(mGroups);
	qDeleteAll(mObjects);
}

QModelIndex ZMapObjectModel::index(int row, int column, const QModelIndex &parent) const
{
	if (!parent.isValid()) {
		if (row < mObjectGroups.count()) {
			return createIndex(row, column, mGroups[mObjectGroups.at(row)]);
		}
		return QModelIndex();
	}

	ObjectGroup *og = toObjectGroup(parent);
	if (row >= og->objects().count())
		return QModelIndex(); // happens when deleting the last item in a parent
	if (!mObjects.contains(og->objects().at(row)))
		return QModelIndex(); // Paranoia: sometimes "fake" objects are in use (see createobjecttool)
	return createIndex(row, column, mObjects[og->objects()[row]]);
}

QModelIndex ZMapObjectModel::parent(const QModelIndex &index) const
{
	MapObject *mapObject = toMapObject(index);
	if (mapObject)
		return this->index(mapObject->objectGroup()); // createIndex(mObjectGroups.indexOf(mapObject->objectGroup()), 0, mGroups[mapObject->objectGroup()]);
	return QModelIndex();
}

int ZMapObjectModel::rowCount(const QModelIndex &parent) const
{
	if (!mMapDocument)
		return 0;
	if (!parent.isValid())
		return mMap->objectGroupCount();
    if (ObjectGroup *og = toObjectGroup(parent))
        return og->objectCount();
    return 0;
}

int ZMapObjectModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
	return 2; // MapObject name|type
}

QVariant ZMapObjectModel::data(const QModelIndex &index, int role) const
{
	if (MapObject *mapObject = toMapObject(index)) {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return index.column() ? mapObject->type() : mapObject->name();
		case Qt::DecorationRole:
			return QVariant(); // no icon -> maybe the color?
		case Qt::CheckStateRole:
			if (index.column() > 0)
				return QVariant();
			return mapObject->isVisible() ? Qt::Checked : Qt::Unchecked;
		case OpacityRole:
			return qreal(1);
		default:
			return QVariant();
		}
	}
	if (ObjectGroup *objectGroup = toObjectGroup(index)) {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return index.column() ? QVariant() : objectGroup->name();
		case Qt::DecorationRole:
			return index.column() ? QVariant() : mObjectGroupIcon;
		case Qt::CheckStateRole:
			if (index.column() > 0)
				return QVariant();
			return objectGroup->isVisible() ? Qt::Checked : Qt::Unchecked;
		case OpacityRole:
			return objectGroup->opacity();
		default:
			return QVariant();
		}
    }
	return QVariant();
}

bool ZMapObjectModel::setData(const QModelIndex &index, const QVariant &value,
                         int role)
{
	if (MapObject *mapObject = toMapObject(index)) {
		switch (role) {
		case Qt::CheckStateRole:
			{
			Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
			mapObject->setVisible(c == Qt::Checked);
			emit dataChanged(index, index);
			emit objectsChanged(QList<MapObject*>() << mapObject);
			return true;
			}
		case Qt::EditRole:
			{
				const QString s = value.toString();
				if (index.column() == 0 && s != mapObject->name()) {
					QUndoStack *undo = mMapDocument->undoStack();
					undo->beginMacro(tr("Change Object Name"));
					undo->push(new ChangeMapObject(mMapDocument, mapObject, s, mapObject->type()));
					undo->endMacro();
				}
				if (index.column() == 1 && s != mapObject->type()) {
					QUndoStack *undo = mMapDocument->undoStack();
					undo->beginMacro(tr("Change Object Type"));
					undo->push(new ChangeMapObject(mMapDocument, mapObject, mapObject->name(), s));
					undo->endMacro();
				}
				return true;
			}
		}
		return false;
	}
	if (ObjectGroup *objectGroup = toObjectGroup(index)) {
		switch (role) {
		case Qt::CheckStateRole:
			{
			LayerModel *layerModel = mMapDocument->layerModel();
			const int layerIndex = mMap->layers().indexOf(objectGroup);
			const int row = layerModel->layerIndexToRow(layerIndex);
			layerModel->setData(layerModel->index(row), value, role);
//			emit dataChanged(index, index); our layerChanged() will get called
			return true;
			}
		case Qt::EditRole:
			{
				const QString newName = value.toString();
				if (objectGroup->name() != newName) {
					const int layerIndex = mMap->layers().indexOf(objectGroup);
					RenameLayer *rename = new RenameLayer(mMapDocument, layerIndex, newName);
					mMapDocument->undoStack()->push(rename);
				}
				return true;
			}
		}
		return false;
	}
    return false;
}

Qt::ItemFlags ZMapObjectModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    if (index.column() == 0)
        rc |= Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
	else if (index.parent().isValid())
		rc |= Qt::ItemIsEditable; // MapObject type
    return rc;
}

QVariant ZMapObjectModel::headerData(int section, Qt::Orientation orientation,
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

QModelIndex ZMapObjectModel::index(ObjectGroup *og) const
{
	const int row = mObjectGroups.indexOf(og);
	Q_ASSERT(mGroups[og]);
	return createIndex(row, 0, mGroups[og]);
}

QModelIndex ZMapObjectModel::index(MapObject *o) const
{
	const int row = o->objectGroup()->objects().indexOf(o);
	Q_ASSERT(mObjects[o]);
	return createIndex(row, 0, mObjects[o]);
}

ObjectGroup *ZMapObjectModel::toObjectGroup(const QModelIndex &index) const
{
	if (index.isValid()) {
		ObjectOrGroup *oog = static_cast<ObjectOrGroup*>(index.internalPointer());
		return oog->mGroup;
	}
	return 0;
}

MapObject *ZMapObjectModel::toMapObject(const QModelIndex &index) const
{
	if (index.isValid()) {
		ObjectOrGroup *oog = static_cast<ObjectOrGroup*>(index.internalPointer());
		return oog->mObject;
	}
	return 0;
}

ObjectGroup *ZMapObjectModel::toLayer(const QModelIndex &index) const
{
	if (index.isValid()) {
		ObjectOrGroup *oog = static_cast<ObjectOrGroup*>(index.internalPointer());
		return oog->mGroup ? oog->mGroup : oog->mObject->objectGroup();
	}
	return 0;
}

// FIXME: Each MapDocument has its own persistent MapObjectModel, so this only does anything useful
// one time.
void ZMapObjectModel::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

	if (mMapDocument)
		mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
	mMap = 0;

	mObjectGroups.clear();
	qDeleteAll(mGroups);
	mGroups.clear();
	qDeleteAll(mObjects);
	mObjects.clear();

	if (mMapDocument) {
		mMap = mMapDocument->map();

		connect(mMapDocument, SIGNAL(layerAdded(int)),
				this, SLOT(layerAdded(int)));
		connect(mMapDocument, SIGNAL(layerChanged(int)),
				this, SLOT(layerChanged(int)));
		connect(mMapDocument, SIGNAL(layerAboutToBeRemoved(int)),
				this, SLOT(layerAboutToBeRemoved(int)));

		foreach (ObjectGroup *og, mMap->objectGroups()) {
#if GROUPS_IN_DISPLAY_ORDER
			mObjectGroups.prepend(og);
#else
			mObjectGroups.append(og);
#endif
			mGroups.insert(og, new ObjectOrGroup(og));
			foreach (MapObject *o, og->objects())
				mObjects.insert(o, new ObjectOrGroup(o));
		}
	}

    reset();
}

void ZMapObjectModel::layerAdded(int index)
{
	Layer *layer = mMap->layerAt(index);
	if (ObjectGroup *og = layer->asObjectGroup()) {
		if (!mGroups.contains(og)) {
			ObjectGroup *prev = 0;
			for (index = index - 1; index >= 0; --index)
				if (prev = mMap->layerAt(index)->asObjectGroup())
					break;
#if GROUPS_IN_DISPLAY_ORDER
			index = prev ? mObjectGroups.indexOf(prev) : mObjectGroups.count();
#else
			index = prev ? mObjectGroups.indexOf(prev) + 1 : 0;
#endif
			mObjectGroups.insert(index, og);
			const int row = mObjectGroups.indexOf(og);
			beginInsertRows(QModelIndex(), row, row);
			mGroups.insert(og, new ObjectOrGroup(og));
			foreach (MapObject *o, og->objects()) {
				if (!mObjects.contains(o))
					mObjects.insert(o, new ObjectOrGroup(o));
			}
			endInsertRows();
		}
	}
}

void ZMapObjectModel::layerChanged(int index)
{
	Layer *layer = mMap->layerAt(index);
	if (ObjectGroup *og = layer->asObjectGroup()) {
		if (mGroups.contains(og)) {
			QModelIndex index = this->index(og);
			emit dataChanged(index, index);
		}
	}
}

void ZMapObjectModel::layerAboutToBeRemoved(int index)
{
	Layer *layer = mMap->layerAt(index);
	if (ObjectGroup *og = layer->asObjectGroup()) {
		if (mGroups.contains(og)) {
			const int row = mObjectGroups.indexOf(og);
			beginRemoveRows(QModelIndex(), row, row);
			mObjectGroups.removeOne(og);
			delete mGroups[og];
			mGroups.remove(og);
			foreach (MapObject *o, og->objects()) {
				if (mObjects.contains(o)) {
					delete mObjects[0];
					mObjects.remove(o);
				}
			}
			endRemoveRows();
		}
	}
}

void ZMapObjectModel::insertObject(ObjectGroup *og, int index, MapObject *o)
{
	const int row = (index >= 0) ? index : og->objects().count();
	beginInsertRows(this->index(og), row, row);
	if (index < 0)
		og->addObject(o);
	else
		og->insertObject(index, o);
	mObjects.insert(o, new ObjectOrGroup(o));
	endInsertRows();
	emit objectsAdded(QList<MapObject*>() << o);
}

int ZMapObjectModel::removeObject(ObjectGroup *og, MapObject *o)
{
	emit objectsAboutToBeRemoved(QList<MapObject*>() << o);
	const int row = og->objects().indexOf(o);
	beginRemoveRows(index(og), row, row);
	int index = og->removeObject(o);
	delete mObjects[o];
	mObjects.remove(o);
	endRemoveRows();
	emit objectsRemoved(QList<MapObject*>() << o);
	return index;
}

// ObjectGroup color changed
// FIXME: layerChanged should let the scene know that objects need redrawing
void ZMapObjectModel::emitObjectsChanged(const QList<MapObject *> &objects)
{
	emit objectsChanged(objects);
}

void ZMapObjectModel::setObjectName(MapObject *o, const QString &name)
{
    o->setName(name);
	QModelIndex index = this->index(o);
	emit dataChanged(index, index);
	emit objectsChanged(QList<MapObject*>() << o);
}

void ZMapObjectModel::setObjectType(MapObject *o, const QString &type)
{
    o->setType(type);
	QModelIndex index = this->index(o);
	emit dataChanged(index, index);
	emit objectsChanged(QList<MapObject*>() << o);
}

void ZMapObjectModel::setObjectPolygon(MapObject *o, const QPolygonF &polygon)
{
	o->setPolygon(polygon);
	emit objectsChanged(QList<MapObject*>() << o);
}

void ZMapObjectModel::setObjectPosition(MapObject *o, const QPointF &pos)
{
	o->setPosition(pos);
	emit objectsChanged(QList<MapObject*>() << o);
}

void ZMapObjectModel::setObjectSize(MapObject *o, const QSizeF &size)
{
	o->setSize(size);
	emit objectsChanged(QList<MapObject*>() << o);
}

