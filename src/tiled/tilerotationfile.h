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

#include <QObject>
#include <QList>

class SimpleFileBlock;

namespace BuildingEditor
{
class FurnitureTiles;
}

namespace Tiled
{

class TileRotateFileInfo;

class TileRotationFile : public QObject
{
    Q_OBJECT
public:
    static const int VERSION1 = 1;
    static const int VERSION_LATEST = VERSION1;

    TileRotationFile();
    ~TileRotationFile();

    bool read(const QString& path);
    bool write(const QString& path, const QList<TileRotateFileInfo*>& tiles);

    const QString& errorString() const { return mError; }

    QList<TileRotateFileInfo *> takeTiles();

private:
    TileRotateFileInfo *furnitureTilesFromSFB(const SimpleFileBlock &furnitureBlock, QString &error);

private:
    QString mError;
    QList<TileRotateFileInfo*> mTiles;
};

extern const char *TILE_ROTATE_NAMES[];

// namespace Tiled
}

#endif // TILEROTATIONFILE_H
