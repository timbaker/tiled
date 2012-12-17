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

#include "pathgenerator.h"

#include "map.h"
#include "pathlayer.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;

PathGeneratorType::PathGeneratorType(const QString &name)
{
}

/////

PathGenerator::PathGenerator(PathGeneratorType *type, Path *path)
    : mType(type)
    , mPath(path)
{
}

Tileset *findTileset(const QString &name, const QList<Tileset*> &tilesets)
{
    foreach (Tileset *ts, tilesets) {
        if (ts->name() == name)
            return ts;
    }
    return 0;
}

QString layerNameWithoutPrefix(const QString &name)
{
    int pos = name.indexOf(QLatin1Char('_')) + 1; // Could be "-1 + 1 == 0"
    return name.mid(pos);
}


TileLayer *findTileLayer(const QString &name, const QVector<TileLayer*> &layers)
{
    foreach (TileLayer *tl, layers) {
        if (layerNameWithoutPrefix(tl->name()) == name)
            return tl;
    }
    return 0;
}

/**
 * Returns the lists of points on a line from (x0,y0) to (x1,y1).
 *
 * This is an implementation of bresenhams line algorithm, initially copied
 * from http://en.wikipedia.org/wiki/Bresenham's_line_algorithm#Optimization
 * changed to C++ syntax.
 */
// from stampBrush.cpp
static QVector<QPoint> calculateLine(int x0, int y0, int x1, int y1)
{
    QVector<QPoint> ret;

    bool steep = qAbs(y1 - y0) > qAbs(x1 - x0);
    if (steep) {
        qSwap(x0, y0);
        qSwap(x1, y1);
    }
    if (x0 > x1) {
        qSwap(x0, x1);
        qSwap(y0, y1);
    }
    const int deltax = x1 - x0;
    const int deltay = qAbs(y1 - y0);
    int error = deltax / 2;
    int ystep;
    int y = y0;

    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    for (int x = x0; x < x1 + 1 ; x++) {
        if (steep)
            ret += QPoint(y, x);
        else
            ret += QPoint(x, y);
        error = error - deltay;
        if (error < 0) {
             y = y + ystep;
             error = error + deltax;
        }
    }

    return ret;
}

void PathGenerator::outline(Tile *tile, TileLayer *tl)
{
    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    for (int i = 0; i < points.size() - 1; i++) {
        foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                          points[i+1].x(), points[i+1].y())) {
            if (!tl->contains(pt))
                continue;
            Cell cell(tile);
            tl->setCell(pt.x(), pt.y(), cell);
        }
    }
}

void PathGenerator::outlineWidth(Tile *tile, TileLayer *tl, int width)
{
    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    for (int i = 0; i < points.size() - 1; i++) {
        bool vert = points[i].x() == points[i+1].x();
        bool horiz = points[i].y() == points[i+1].y();
        int dx = horiz ? width-width/2 : 0;
        int dy = vert ? width-width/2 : 0;
        bool firstSeg = i == 0;
        bool lastSeg = i < points.size() - 1;
        foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                          points[i+1].x()+ dx, points[i+1].y() + dy)) {
            Cell cell(tile);
            if (vert) {
                for (int j = 0; j < width; j++) {
                    if (tl->contains(pt.x() - width / 2 + j, pt.y()))
                        tl->setCell(pt.x() - width / 2 + j, pt.y(), cell);
                }
            } else if (horiz) {
                for (int j = 0; j < width; j++) {
                    if (tl->contains(pt.x(), pt.y() - width / 2 + j))
                        tl->setCell(pt.x(), pt.y() - width / 2 + j, cell);
                }
            } else {
                if (!tl->contains(pt))
                    continue;
                Cell cell(tile);
                tl->setCell(pt.x(), pt.y(), cell);
            }
        }
    }
}

void PathGenerator::fill(Tile *tile, TileLayer *tl)
{
    if (!mPath->isClosed())
        return;

    QRect bounds = mPath->polygon().boundingRect();

    QPolygonF polygon = mPath->polygonf();
    for (int x = bounds.left(); x <= bounds.right(); x++) {
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            QPointF pt(x + 0.5, y + 0.5);
            if (polygon.containsPoint(pt, Qt::WindingFill)) {
                if (!tl->contains(pt.toPoint()))
                    continue;
                Cell cell(tile);
                tl->setCell(pt.x(), pt.y(), cell);
            }
        }
    }
}

/////

PG_SingleTile::PG_SingleTile(Path *path) :
    PathGenerator(0, path),
    mLayerName(QLatin1String("Floor")),
    mTilesetName(QLatin1String("floors_exterior_street_01")),
    mTileID(18)
{
}

void PG_SingleTile::generate(int level, QVector<TileLayer *> &layers)
{
    if (level != mPath->level())
        return;
    if (!mPath->points().size())
        return;

    TileLayer *tl = findTileLayer(mLayerName, layers);
    if (!tl) return;

    Tileset *ts = findTileset(mTilesetName, tl->map()->tilesets());
    if (!ts) return;

    if (mPath->isClosed())
        fill(ts->tileAt(mTileID), tl);

    outline(ts->tileAt(mTileID), tl);
}

/////

PG_Fence::PG_Fence(Path *path) :
    PathGenerator(0, path),
    mLayerName(QLatin1String("Furniture")),
    mLayerName2(QLatin1String("Furniture2")),
    mTilesetName(TileCount),
    mTileID(TileCount)
{
    // Tall wooden
    mTilesetName[West1] = QLatin1String("fencing_01");
    mTileID[West1] = 11;
    mTilesetName[West2] = QLatin1String("fencing_01");
    mTileID[West2] = 10;
    mTilesetName[North1] = QLatin1String("fencing_01");
    mTileID[North1] = 8;
    mTilesetName[North2] = QLatin1String("fencing_01");
    mTileID[North2] = 9;
    mTilesetName[NorthWest] = QLatin1String("fencing_01");
    mTileID[NorthWest] = 12;
    mTilesetName[SouthEast] = QLatin1String("fencing_01");
    mTileID[SouthEast] = 13;

#if 1
    for (int i = 0; i < TileCount; i++)
        mTileID[i] += 16; // Chainlink
#elif 0
    for (int i = 0; i < TileCount; i++)
        mTileID[i] += 16 + 8; // Short wooden
#elif 0
    // Black metal
    mTileID[West1] = mTileID[West2] = 2;
    mTileID[North1] = mTileID[North2] = 1;
    mTileID[NorthWest] = 3;
    mTileID[SouthEast] = 0;
#elif 0
    // White picket
    mTileID[West1] = mTileID[West2] = 4;
    mTileID[North1] = mTileID[North2] = 5;
    mTileID[NorthWest] = 6;
    mTileID[SouthEast] = 7;
#endif
}

void PG_Fence::generate(int level, QVector<TileLayer *> &layers)
{
    if (level != mPath->level())
        return;
    if (mPath->points().size() < 2)
        return;

    TileLayer *tl = findTileLayer(mLayerName, layers);
    if (!tl) return;

    QVector<Tile*> tiles(TileCount);
    for (int i = 0; i < TileCount; i++) {
        Tileset *ts = findTileset(mTilesetName[i], tl->map()->tilesets());
        if (!ts) return;
        tiles[i] = ts->tileAt(mTileID[i]);
        if (!tiles[i]) return;
    }

    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    for (int i = 0; i < points.size() - 1; i++) {
        bool vert = points[i].x() == points[i+1].x();
        bool horiz = points[i].y() == points[i+1].y();
        int alternate = 0;
        if (horiz) {
            foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                              points[i+1].x(), points[i+1].y())) {
                if (pt.x() == qMax(points[i].x(), points[i+1].x())) {
                    if (tl->contains(pt.x(), pt.y() - 1)) {
                        if (tl->cellAt(pt.x(), pt.y() - 1).tile == tiles[West2])
                            tl->setCell(pt.x(), pt.y(), Cell(tiles[SouthEast]));
                    }
                    break;
                }
                if (tl->contains(pt)) {
                    Tile *tile = tl->cellAt(pt).tile;
                    if (tile == tiles[West1])
                        tl->setCell(pt.x(), pt.y(), Cell(tiles[NorthWest]));
                    else
                        tl->setCell(pt.x(), pt.y(), Cell(tiles[North1 + alternate]));
                }
                alternate = !alternate;
            }
        } else if (vert) {
            foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                              points[i+1].x(), points[i+1].y())) {
                if (pt.y() == qMax(points[i].y(), points[i+1].y())) {
                    if (tl->contains(pt.x() - 1, pt.y())) {
                        if (tl->cellAt(pt.x() - 1, pt.y()).tile == tiles[North2])
                            tl->setCell(pt.x(), pt.y(), Cell(tiles[SouthEast]));
                    }
                    break;
                }
                if (tl->contains(pt)) {
                    Tile *tile = tl->cellAt(pt).tile;
                    if (tile == tiles[North1])
                        tl->setCell(pt.x(), pt.y(), Cell(tiles[NorthWest]));
                    else
                        tl->setCell(pt.x(), pt.y(), Cell(tiles[West1 + alternate]));
                }
                alternate = !alternate;
            }
        }
    }
}

/////

PG_StreetLight::PG_StreetLight(Path *path) :
    PathGenerator(0, path),
    mGap(10),
    mLayerName(QLatin1String("Furniture")),
    mLayerName2(QLatin1String("Furniture2")),
    mTilesetName(TileCount),
    mTileID(TileCount)
{
    mTilesetName[West] = QLatin1String("lighting_outdoor_01");
    mTileID[West] = 9;
    mTilesetName[North] = QLatin1String("lighting_outdoor_01");
    mTileID[North] = 10;
    mTilesetName[East] = QLatin1String("lighting_outdoor_01");
    mTileID[East] = 11;
    mTilesetName[South] = QLatin1String("lighting_outdoor_01");
    mTileID[South] = 8;
    mTilesetName[Base] = QLatin1String("lighting_outdoor_01");
    mTileID[Base] = 16;
}

void PG_StreetLight::generate(int level, QVector<TileLayer *> &layers)
{
    bool level0 = level == mPath->level();
    bool level1 = level == mPath->level() + 1;
    if (!level0 && !level1)
        return;
    if (mPath->points().size() < 2)
        return;

    TileLayer *tl = findTileLayer(mLayerName, layers);
    if (!tl) return;

    QVector<Tile*> tiles(TileCount);
    for (int i = 0; i < TileCount; i++) {
        Tileset *ts = findTileset(mTilesetName[i], tl->map()->tilesets());
        if (!ts) return;
        tiles[i] = ts->tileAt(mTileID[i]);
        if (!tiles[i]) return;
    }

    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    if (tl->map()->orientation() == Map::Isometric && level1) {
        for (int i = 0; i < points.size(); i++)
            points[i].translate(QPoint(-3, -3));
    }

    for (int i = 0; i < points.size() - 1; i++) {
        bool vert = points[i].x() == points[i+1].x();
        bool horiz = points[i].y() == points[i+1].y();
        int distance = 0;
        if (horiz) {
            foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                              points[i+1].x(), points[i+1].y())) {
                if (tl->contains(pt) && !(distance % mGap)) {
                    tl->setCell(pt.x(), pt.y(), Cell(tiles[level1 ? North : Base]));
                }
                ++distance;
            }
        } else if (vert) {
            foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                              points[i+1].x(), points[i+1].y())) {
                if (tl->contains(pt) && !(distance % mGap)) {
                    tl->setCell(pt.x(), pt.y(), Cell(tiles[level1 ? West : Base]));
                }
                ++distance;
            }
        }
    }
}
