/*
 * zlotmanager.cpp
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

#include "zlotmanager.hpp"

#include "mapdocument.h"
#include "mapobject.h"
#include "mapreader.h"
#include "zlot.hpp"

namespace Tiled {

ZLotManager *ZLotManager::mInstance = 0;

ZLotManager *ZLotManager::instance()
{
	if (!mInstance)
		mInstance = new ZLotManager();

	return mInstance;
}

ZLotManager::ZLotManager()
{
}

ZLotManager::~ZLotManager()
{
}

void ZLotManager::handleMapObject(Internal::MapDocument *mapDoc, MapObject *mapObject)
{
	const QString& name = mapObject->name();
	const QString& type = mapObject->type();

	ZLot *currLot = 0, *newLot = 0;

	if (mMapObjectToLot.keys().contains(mapObject))
		currLot = mMapObjectToLot[mapObject];

	if (name == QLatin1String("lot") && !type.isEmpty()) {

		if (mLots.keys().contains(type)) {
			newLot = mLots[type];
		} else {

			QString fileName = QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\maptools\\") + type + QLatin1String(".tmx");

			MapReader reader;
			Map *map = reader.readMap(fileName);
			if (map) {
				// TODO: sanity check the lot-map tile width and height against the current map
				mLots[type] = new ZLot(map);
				newLot = mLots[type];
			} else {
				// TODO: Add error handling
			}
		}
	}

	if (currLot != newLot) {
		if (currLot) {
			QMap<MapObject*,ZLot*>::iterator it = mMapObjectToLot.find(mapObject);
			mMapObjectToLot.erase(it);
			emit lotRemoved(currLot, mapDoc, mapObject); // remove from scene
		}
		if (newLot) {
			mMapObjectToLot[mapObject] = newLot;
			emit lotAdded(newLot, mapDoc, mapObject); // add to scene
		}
	} else if (currLot) {
		emit lotUpdated(currLot, mapDoc, mapObject); // position change, etc
	}
}

} // namespace Tiled