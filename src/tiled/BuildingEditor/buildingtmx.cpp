/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "buildingtmx.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "simplefile.h"

#include "mapcomposite.h"

#include "tilesetmanager.h"
#include "tmxmapwriter.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QDir>
#include <QImage>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTMX *BuildingTMX::mInstance = 0;

BuildingTMX *BuildingTMX::instance()
{
    if (!mInstance)
        mInstance = new BuildingTMX;
    return mInstance;
}

void BuildingTMX::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTMX::BuildingTMX()
{
}

bool BuildingTMX::exportTMX(Building *building, const MapComposite *mapComposite,
                            const QString &fileName)
{
    Map *clone = mapComposite->map()->clone();

    foreach (BuildingFloor *floor, building->floors()) {

        // The given map has layers required by the editor, i.e., Floors, Walls,
        // Doors, etc.  The TMXConfig.txt file may specify extra layer names.
        // So we need to insert any extra layers in the order specified in
        // TMXConfig.txt.  If the layer name has a N_ prefix, it is only added
        // to level N, otherwise it is added to every level.  Object layers are
        // added above *all* the tile layers in the map.
        int previousExistingLayer = -1;
        foreach (LayerInfo layerInfo, mLayers) {
            QString layerName = layerInfo.mName;
            int level;
            if (MapComposite::levelForLayer(layerName, &level)) {
                if (level != floor->level())
                    continue;
            } else {
                layerName = tr("%1_%2").arg(floor->level()).arg(layerName);
            }
            int n;
            if ((n = clone->indexOfLayer(layerName)) >= 0) {
                previousExistingLayer = n;
                continue;
            }
            if (layerInfo.mType == LayerInfo::Tile) {
                TileLayer *tl = new TileLayer(layerName, 0, 0,
                                              clone->width(), clone->height());
                if (previousExistingLayer < 0)
                    previousExistingLayer = 0;
                clone->insertLayer(previousExistingLayer + 1, tl);
                previousExistingLayer++;
            } else {
                ObjectGroup *og = new ObjectGroup(layerName,
                                                  0, 0, clone->width(), clone->height());
                clone->addLayer(og);
            }
        }

        ObjectGroup *objectGroup = new ObjectGroup(tr("%1_RoomDefs").arg(floor->level()),
                                                   0, 0, clone->width(), clone->height());

        // Add the RoomDefs layer above all the tile layers
        clone->addLayer(objectGroup);

        int delta = (mapComposite->maxLevel() - floor->level()) * 3;
        QPoint offset(delta, delta); // FIXME: not for LevelIsometric
        foreach (Room *room, building->rooms()) {
            QRegion roomRegion = floor->roomRegion(room);
            foreach (QRect rect, roomRegion.rects()) {
                MapObject *mapObject = new MapObject(room->internalName, tr("room"),
                                                     rect.topLeft() + offset,
                                                     rect.size());
                objectGroup->addObject(mapObject);
            }
        }
    }

    TmxMapWriter writer;
    if (!writer.write(clone, fileName)) {
        mError = writer.errorString();
        delete clone;
        return false;
    }
    delete clone;
    return true;
}

bool BuildingTMX::readTxt()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = BuildingPreferences::instance()->tilesDirectory();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        mError = tr("The Tiles directory specified in the preferences doesn't exist!\n%1")
                .arg(tilesDirectory);
        return false;
    }

    QString path = BuildingPreferences::instance()
            ->configPath(QLatin1String("TMXConfig.txt"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Couldn't open %1").arg(path);
        return false;
    }

//    PROGRESS progress(tr("Reading TMXConfig.txt tilesets"), this);

    SimpleFile simpleFile;
    if (!simpleFile.read(path)) {
        mError = simpleFile.errorString();
        return false;
    }

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        if (block.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tileset")) {
                    QString source = tilesDirectory + QLatin1Char('/') + kv.value
                            + QLatin1String(".png");
                    QFileInfo info(source);
                    if (!info.exists()) {
                        mError = tr("Tileset in TMXConfig.txt doesn't exist.\n%1").arg(source);
                        return false;
                    }
                    source = info.canonicalFilePath();

                    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);

                    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
                    Tileset *cached = cache->findMatch(ts, source);
                    if (!cached || !ts->loadFromCache(cached)) {
                        const QImage tilesetImage = QImage(source);
                        if (ts->loadFromImage(tilesetImage, source))
                            cache->addTileset(ts);
                        else {
                            mError = tr("Error loading tileset image:\n'%1'").arg(source);
                            return false;
                        }
                    }

                    BuildingTiles::instance()->addTileset(ts);

                    mTilesets += ts->name();
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else if (block.name == QLatin1String("layers")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tile")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Tile);
                } else if (kv.name == QLatin1String("object")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Object);
                } else {
                    mError = tr("Unknown layer type '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}
