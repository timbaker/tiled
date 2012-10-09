/*
 * pathgenerator.cpp
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
}

void PathGenerator::generate(Map *map, QVector<TileLayer *> &layers)
{
    if (!map->tilesets().count())
        return;
    if (!mPath->points().size())
        return;

    TileLayer *tl = findTileLayer(QLatin1String("Floor"), layers);
    if (!tl) return;
    Tileset *ts = findTileset(QLatin1String("floors_exterior_street_01"), map->tilesets());
    if (!ts) return;
    outlineWidth(ts->tileAt(18), tl, 4);

//    fill(ts->tileAt(3), layers.first());
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
