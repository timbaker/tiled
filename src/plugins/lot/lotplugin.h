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

#ifndef LUAPLUGIN_H
#define LUAPLUGIN_H

#include "lot_global.h"

#include "gidmapper.h"
#include "mapwriterinterface.h"

#include <QDir>
#include <QObject>

namespace Tiled {
class MapObject;
class ObjectGroup;
class Properties;
class TileLayer;
class Tileset;
}

namespace Lot {

class Tile
{
public:
	Tile(const QString& name)
	{
		this->name = name;
		this->used = false;
		this->id = -1;
	}
	QString name;
	bool used;
	int id;
};

class Lot
{
public:
	Lot(QString name, int x, int y, int w, int h)
	{
		this->name = name;
		this->x = x;
		this->y = y;
		this->w = w;
		this->h = h;
	}
	QString name;
	int x;
	int y;
	int w;
	int h;
};

class Entry
{
public:
	Entry(int gid)
	{
		this->gid = gid;
	}
	int gid;
};

class Square
{
public:
	QList<Entry*> Entries;
};

class Zone
{
public:
	Zone(const QString& name, const QString& val, int x, int y, int z, int width, int height)
	{
        this->name = name;
        this->val = val;
        this->x = x;
        this->y = y;
        this->z = z;
        this->width = width;
        this->height = height;
	}

	QString name;
	QString val;
	int x;
	int y;
	int z;
	int width;
	int height;
};

/**
 * This plugin allows exporting maps as .lot files.
 */
class LOTSHARED_EXPORT LotPlugin : public QObject,
                                   public Tiled::MapWriterInterface
{
    Q_OBJECT
    Q_INTERFACES(Tiled::MapWriterInterface)

public:
    LotPlugin();

    // MapWriterInterface
    bool write(const Tiled::Map *map, const QString &fileName);
    QString nameFilter() const;
    QString errorString() const;


private:
	bool handleTileset(QFile& file, const Tiled::Tileset *tileset, uint firstGid);
	bool handleTileLayer(QFile& file, const Tiled::TileLayer *tileLayer);

	bool parseNameToLevel(const QString& name, int *level);

	QString mError;
    QDir mMapDir;     // The directory in which the map is being saved
    Tiled::GidMapper mGidMapper;

	QList<Zone*> ZoneList;
	QList<Lot*> LotList;
	QMap<int,Tile*> TileMap;
	int StartX;
	int StartY;
	int EndX;
	int EndY;
	int MaxLevel;
	int Version;
};

} // namespace Lot

#endif // LOTPLUGIN_H
