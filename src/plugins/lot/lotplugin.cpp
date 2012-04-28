/*
 * Project Zomboid .lot TileZed Plugin
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

#include "lotplugin.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "properties.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QFile>

using namespace Tiled;
using namespace Lot;

LotPlugin::LotPlugin()
{
}

static void SaveString(QDataStream& out, const QString& str)
{
	for (int i = 0; i < str.length(); i++)
		out << quint8(str[i].toAscii());
	out << quint8('\n');
}

bool LotPlugin::write(const Map *map, const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    mMapDir = QFileInfo(fileName).path();

	StartX = 10000;
	StartY = 10000;
	EndX = -10000;
	EndY = -10000;
	MaxLevel = 15; // FIXME: shouldn't this start at zero?
	Version = 0;

	ZoneList.clear();
	LotList.clear();
	TileMap.clear();

    Properties::const_iterator it = map->properties().constBegin();
    Properties::const_iterator it_end = map->properties().constEnd();
    for (; it != it_end; ++it) {
		if (it.key() == QLatin1String("version"))
			Version = it.value().toInt();
	}

    mGidMapper.clear();
    uint firstGid = 1;
    foreach (Tileset *tileset, map->tilesets()) {
        handleTileset(file, tileset, firstGid);
        mGidMapper.insert(firstGid, tileset);
        firstGid += tileset->tileCount();
    }

    foreach (Layer *layer, map->layers()) {
        if (TileLayer *tileLayer = layer->asTileLayer())
            handleTileLayer(file, tileLayer);
	}

    QVector<QVector<QVector<Square> > > griddata;
	griddata.resize(EndX - StartX);
	for (int x = 0; x < EndX - StartX; x++) {
		griddata[x].resize(EndY - StartY);
		for (int y = 0; y < EndY - StartY; y++)
			griddata[x][y].resize(MaxLevel);
	}

    foreach (Layer *layer, map->layers()) {
		// Ignore layers without the proper naming convention
		int level = -1;
		if (!parseNameToLevel(layer->name(), &level))
			continue;
        if (TileLayer *tileLayer = layer->asTileLayer()) {
			for (int x = 0; x < tileLayer->width(); x++) {
				for (int y = 0; y < tileLayer->height(); y++) {
					const Tiled::Cell& cell = tileLayer->cellAt(x, y);
					if (!cell.isEmpty()) {
						int lx = x, ly = y;
						if (map->orientation() == Map::Isometric) {
							lx = x + (level * 3);
							ly = y + (level * 3);
						}
						lx -= StartX;
						ly -= StartY;
						if (lx >= 0 && ly >= 0 && lx < tileLayer->width() && ly < tileLayer->height()) {
							Entry *e = new Entry(mGidMapper.cellToGid(cell));
							griddata[lx][ly][level].Entries.append(e);
							TileMap[e->gid]->used = true;
						}
					}
				}
			}
		} else if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
			foreach (const MapObject *mapObject, objectGroup->objects()) {
				// try ... catch in original code caught cases of objects without name/type/width/height
				if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
					continue;
				if (!mapObject->width() || !mapObject->height())
					continue;

				// See mapwriter.cpp
				int px = qRound(mapObject->x() * map->tileHeight());
				int py = qRound(mapObject->y() * map->tileHeight());
				int pw = qRound(mapObject->width() * map->tileHeight());
				int ph = qRound(mapObject->height() * map->tileHeight());

				int x = px / map->tileHeight();
				int y = py / map->tileHeight();
				int w = pw / map->tileHeight();
				int h = ph / map->tileHeight();
				Zone *z = new Zone(mapObject->name(), mapObject->type(), x, y, level, w, h);
				ZoneList.append(z);
			}
		}
	}

	QDataStream out(&file);
	out.setByteOrder(QDataStream::LittleEndian);

	out << qint32(Version);

	int tilecount = 0;
	foreach (Tile *tile, TileMap) {
		if (tile->used) {
			tile->id = tilecount;
			tilecount++;
		}
	}
	out << qint32(tilecount);

	foreach (Tile *tile, TileMap) {
		if (tile->used) {
			SaveString(out, tile->name);
		}
	}

    int width = EndX - StartX;
    int height = EndY - StartY;
	out << quint8(0);
	out << qint32(width);
	out << qint32(height);
	out << qint32(MaxLevel);

	out << qint32(ZoneList.count());
	foreach (Zone *zone, ZoneList) {
		SaveString(out, zone->name);
		SaveString(out, zone->val);
		out << qint32(zone->x);
		out << qint32(zone->y);
		out << qint32(zone->z);
		out << qint32(zone->width);
		out << qint32(zone->height);
	}

	int notdonecount = 0;
    for (int z = 0; z < MaxLevel; z++)  {
        for (int x = 0; x < EndX - StartX; x++) {
            for (int y = 0; y < EndY - StartY; y++) {
				const QList<Entry*> &entries = griddata[x][y][z].Entries;
				if (entries.count() == 0)
					notdonecount++;
				else {
					if (notdonecount > 0) {
						out << qint32(-1);
						out << qint32(notdonecount);
					}
					notdonecount = 0;
					out << qint32(entries.count());
				}
				foreach (Entry *entry, entries) {
					out << qint32(TileMap[entry->gid]->id);
				}
			}
		}
	}
	if (notdonecount > 0) {
		out << qint32(-1);
		out << qint32(notdonecount);
	}

	file.close();

    for (int z = 0; z < MaxLevel; z++)  {
        for (int x = 0; x < EndX - StartX; x++) {
            for (int y = 0; y < EndY - StartY; y++) {
				const QList<Entry*> &entries = griddata[x][y][z].Entries;
				foreach (Entry *entry, entries)
					delete entry;
			}
		}
	}
	foreach (Tile *tile, TileMap)
		delete tile;
	foreach (Zone *zone, ZoneList)
		delete zone;


    return true;
}

QString LotPlugin::nameFilter() const
{
    return tr("Lot files (*.lot)");
}

QString LotPlugin::errorString() const
{
    return mError;
}

bool LotPlugin::handleTileset(QFile& file, const Tiled::Tileset *tileset, uint firstGid)
{
    if (!tileset->fileName().isEmpty()) {
		mError = tr("Only tileset image files supported, not external tilesets");
		return false;
	}

	QString name = tileset->imageSource();
	if (name.contains(QLatin1String("/")))
		name = name.mid(name.lastIndexOf(QLatin1String("/")) + 1);
	name.replace(".png", "");

	for (int i = 0; i < tileset->tileCount(); ++i) {
        int localID = i;
		int ID = firstGid + localID;
		TileMap[ID] = new Tile(name + "_" + QString::number(localID));
    }

	return true;
}

bool LotPlugin::handleTileLayer(QFile& file, const Tiled::TileLayer *tileLayer)
{
	// FIXME: Assumes all layers are the same size
	StartX = 0;
	StartY = 0;
	EndX = tileLayer->width();
	EndY = tileLayer->height();

	if (tileLayer->isEmpty())
		return true;

	// See if the name matches "0_foo" or "1_bar" etc
	int level;
	if (parseNameToLevel(tileLayer->name(), &level)) {
		if (level + 1 > MaxLevel)
			MaxLevel = level + 1;
	}

	return true;
}

bool LotPlugin::parseNameToLevel(const QString& name, int *level)
{
	// See if the name matches "0_foo" or "1_bar" etc
	QStringList sl = name.trimmed().split(QLatin1Char('_'));
	if (sl.count() > 1 && !sl[1].isEmpty()) {
		bool conversionOK;
		int _level = sl[0].toInt(&conversionOK);
		if (conversionOK) {
			(*level) = _level;
			return true;
		}
	}
	return false;
}

Q_EXPORT_PLUGIN2(Lot, LotPlugin)
