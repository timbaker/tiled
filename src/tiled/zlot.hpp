/*
 * zlot.hpp
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

#ifndef ZLOT_H
#define ZLOT_H

#include <QMap>

namespace Tiled {

class Map;
class MapObject;
class TileLayer;
class ZLotSceneItem;

namespace Internal {
class MapScene;
class ZTileLayerGroupItem;
}

class ZLot
{
public:
	ZLot(Map *map);
	~ZLot();

	void addToScene(Internal::MapScene *mapScene, MapObject *mapObject);
	void removeFromScene(Internal::MapScene *mapScene, MapObject *mapObject);
	void updateInScene(Internal::MapScene *mapScene, MapObject *mapObject);
	bool groupForTileLayer(TileLayer *tl, uint *group);

private:
	Map *mMap;
	QMap<MapObject*,ZLotSceneItem*> mMapObjectToSceneItem;
};

} // namespace Tiled

#endif // ZLOT_H
