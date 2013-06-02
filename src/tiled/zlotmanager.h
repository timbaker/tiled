/*
 * zlotmanager.h
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

class MapComposite;
class MapInfo;
class WorldCellLot;

namespace Tiled {

class Map;
class MapObject;

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

    // Yuck: used by MiniMap
    const QMap<MapObject*,MapComposite*> &objectToLot()
    { return mMapObjectToLot; }

    void worldCellLevelChanged(int level, bool visible);
    void worldCellLotChanged(WorldCellLot*lot);

signals:
    void lotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void lotUpdated(MapComposite *mc, WorldCellLot *lot);

private slots:
    void onLayerAdded(int index);
    void onLayerAboutToBeRemoved(int index);

    void onObjectsAdded(const QList<MapObject*> &objects);
    void onObjectsChanged(const QList<MapObject*> &objects);
    void onObjectsRemoved(const QList<MapObject*> &objects);

    void mapLoaded(MapInfo *mapInfo);
    void mapFailedToLoad(MapInfo *mapInfo);

    void beforeWorldChanged();
    void afterWorldChanged();

private:
    void handleMapObject(MapObject *mapObject);
    void setMapInfo(MapObject *mapObject, MapInfo *mapInfo);
    void setMapComposite(MapObject *mapObject, MapComposite *mapComposite);
    int findLoading(MapObject *mapObject);

    Internal::MapDocument *mMapDocument;
    QMap<MapObject*,MapComposite*> mMapObjectToLot;
    QMap<MapObject*,MapInfo*> mMapObjectToInfo;

    struct MapLoading
    {
        MapLoading(MapInfo *info, MapObject *object) :
            info(info), object(object) {}
        MapInfo *info;
        MapObject *object;
    };

    QList<MapLoading> mMapsLoading;

    /////

    void setMapInfo(WorldCellLot *lot, MapInfo *mapInfo);
    void setMapComposite(WorldCellLot *lot, MapComposite *mapComposite);

    QMap<WorldCellLot*,MapComposite*> mWorldCellLotToMC;
    QMap<WorldCellLot*,MapInfo*> mWorldCellLotToMI;

    struct MapLoading2
    {
        MapLoading2(MapInfo *info, WorldCellLot *lot) :
            info(info), lot(lot) {}
        MapInfo *info;
        WorldCellLot *lot;
    };

    QList<MapLoading2> mMapsLoading2;
};

} // namespace Tiled

#endif // ZLOTMANAGER_H
