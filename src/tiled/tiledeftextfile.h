/*
 * Copyright 2022, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#ifndef TILEDEFTEXTFILE_H
#define TILEDEFTEXTFILE_H

#include <QObject>
#include <QMap>

class SimpleFileBlock;

namespace Tiled {

class Tileset;

namespace Internal {

class TileDefTile;
class TileDefTileset;

/**
  * This class represents a single *.tiles.txt file.
  */
class TileDefTextFile : public QObject
{
    Q_OBJECT
public:
    TileDefTextFile();
    ~TileDefTextFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<TileDefTileset*> &tilesets);

    QString directory() const;

    void insertTileset(int index, TileDefTileset *ts);
    TileDefTileset *removeTileset(int index);

    TileDefTileset *tileset(const QString &name) const;

    const QList<TileDefTileset*> &tilesets() const
    { return mTilesets; }

    QList<TileDefTileset*> takeTilesets();

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }

    QString errorString() const
    { return mError; }

private:
    TileDefTileset *readTileset(const SimpleFileBlock& block);
    TileDefTile *readTile(const SimpleFileBlock& block, TileDefTileset *tileset);
    bool parseUInt(const SimpleFileBlock& block, const QString& key, int& result);
    bool parseUInt(const QString& str, int& result);
    bool parse2Ints(const QString &s, int *pa, int *pb);

private:
    QList<TileDefTileset*> mTilesets;
    QMap<QString,TileDefTileset*> mTilesetByName;
    QString mFileName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // TILEDEFTEXTFILE_H
