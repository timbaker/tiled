/*
 * pathgenerator.h
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
