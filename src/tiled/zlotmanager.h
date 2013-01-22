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

signals:
    void lotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

private slots:
    void onLayerAdded(int index);
    void onLayerAboutToBeRemoved(int index);

    void onObjectsAdded(const QList<MapObject*> &objects);
    void onObjectsChanged(const QList<MapObject*> &objects);
    void onObjectsRemoved(const QList<MapObject*> &objects);

private:
    void handleMapObject(MapObject *mapObject);

    Internal::MapDocument *mMapDocument;
    QMap<MapObject*,MapComposite*> mMapObjectToLot;
};

} // namespace Tiled

#endif // ZLOTMANAGER_H
