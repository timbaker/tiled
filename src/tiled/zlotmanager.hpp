/*
 * zlotmanager.hpp
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

#ifndef ZLOTMANAGER_H
#define ZLOTMANAGER_H

#include "tiled_global.h"

#include <QObject>
#include <QList>
#include <QMap>

namespace Tiled {

class Map;
class MapObject;
class ZLot;
class Tileset;

namespace Internal {
class MapDocument;
}

class ZLotManager : public QObject
{
    Q_OBJECT

public:
    ZLotManager();
    ~ZLotManager();

	void setMapDocument(Internal::MapDocument *mapDoc);
	Internal::MapDocument *mapDocument() { return mMapDocument; }

signals:
	void lotAdded(ZLot *lot, MapObject *mapObject);
	void lotRemoved(ZLot *lot, MapObject *mapObject);
	void lotUpdated(ZLot *lot, MapObject *mapObject);

private slots:
	void onMapsDirectoryChanged();

	void onLayerAdded(int index);
	void onLayerAboutToBeRemoved(int index);

	void onObjectsAdded(const QList<MapObject*> &objects);
	void onObjectsChanged(const QList<MapObject*> &objects);
	void onObjectsRemoved(const QList<MapObject*> &objects);

private:
    void handleMapObject(MapObject *mapObject);
	void shareTilesets(Map *map);
	void convertOrientation(Map *map0, const Map *map1);

	Internal::MapDocument *mMapDocument;
	QMap<QString,ZLot*> mLots; // One ZLot per different .lot file
	QMap<MapObject*,ZLot*> mMapObjectToLot;
	QList<Tileset*> mTilesets;
};

} // namespace Tiled

#endif // ZLOTMANAGER_H
