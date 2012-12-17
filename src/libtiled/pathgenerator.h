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

#ifndef PATHGENERATOR_H
#define PATHGENERATOR_H

#include <QString>
#include <QVector>

namespace Tiled {

class Map;
class Path;
class Tile;
class TileLayer;

class PathGeneratorType
{
public:
    PathGeneratorType(const QString &name);

private:
    QString mName;
};

class PathGenerator
{
public:
    PathGenerator(PathGeneratorType *type, Path *path);

    virtual void generate(int level, QVector<TileLayer *> &layers) = 0;

    void outline(Tile *tile, TileLayer *tl);
    void outlineWidth(Tile *tile, TileLayer *tl, int width);
    void fill(Tile *tile, TileLayer *tl);

protected:
    PathGeneratorType *mType;
    Path *mPath;

};

class PG_SingleTile : public PathGenerator
{
public:
    PG_SingleTile(Path *path);

    void generate(int level, QVector<TileLayer*> &layers);

    QString mLayerName;
    QString mTilesetName;
    int mTileID;
};

class PG_Fence : public PathGenerator
{
public:
    PG_Fence(Path *path);

    void generate(int level, QVector<TileLayer*> &layers);

    enum TileNames
    {
        West1,
        West2,
        North1,
        North2,
        NorthWest,
        SouthEast,
        TileCount
    };

    QString mLayerName;
    QString mLayerName2;
    QVector<QString> mTilesetName;
    QVector<int> mTileID;
};

class PG_StreetLight : public PathGenerator
{
public:
    PG_StreetLight(Path *path);

    void generate(int level, QVector<TileLayer*> &layers);

    enum TileNames
    {
        West,
        North,
        East,
        South,
        Base,
        TileCount
    };

    int mGap;
    QString mLayerName;
    QString mLayerName2;
    QVector<QString> mTilesetName;
    QVector<int> mTileID;
};

} // namespace Tiled

#endif // PATHGENERATOR_H
