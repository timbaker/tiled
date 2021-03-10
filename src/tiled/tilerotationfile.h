/*
 * tilerotationfile.h
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

#ifndef TILEROTATIONFILE_H
#define TILEROTATIONFILE_H

#include "maprotation.h"

#include <QObject>
#include <QList>

class SimpleFileBlock;

namespace Tiled
{

class TileRotated;
class TileRotatedVisual;
class TilesetRotated;

class TileRotationFile : public QObject
{
    Q_OBJECT
public:
    static const int VERSION1 = 1;
    static const int VERSION_LATEST = VERSION1;

    TileRotationFile();
    ~TileRotationFile();

    bool read(const QString& path);
    bool write(const QString& path, const QList<TilesetRotated*>& tilesets, const QMap<QString, QString>& mapping);

    const QString& errorString() const { return mError; }

    QList<TilesetRotated *> takeTilesets();
    QMap<QString, QString> takeMapping();

private:
    TilesetRotated *readTileset(const SimpleFileBlock& block);
    Tiled::MapRotation parseRotation(const QString& str);
    TileRotated *readTile(const SimpleFileBlock& block);
    bool readDirection(const SimpleFileBlock& block, TileRotatedVisual& direction);
    bool parse2Ints(const QString &s, int *pa, int *pb);
    QString twoInts(int a, int b);
    void writeTile(TileRotated *tile, SimpleFileBlock& tileBlock);
    void writeDirection(TileRotatedVisual& direction, SimpleFileBlock& directionBlock);

private:
    QString mError;
    QList<TilesetRotated*> mTilesets;
    QMap<QString, QString> mMapping;
};

extern const char *TILE_ROTATE_NAMES[];

// namespace Tiled
}

#endif // TILEROTATIONFILE_H
