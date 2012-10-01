// FIXME: license

#include "pathlayer.h"

#include "map.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;

Path::Path()
    : mIsClosed(false)
{
    mPoints += new PathPoint();
    mPoints.last()->x = 0, mPoints.last()->y = 0;
    mPoints += new PathPoint();
    mPoints.last()->x = 10, mPoints.last()->y = 0;
    mPoints += new PathPoint();
    mPoints.last()->x = 10, mPoints.last()->y = 10;
}

QPolygonF Path::polygon() const
{
    QPolygonF poly;

    foreach (PathPoint *pt, mPoints) {
        poly.append(QPointF(pt->x + 0.5, pt->y + 0.5));
    }

    return poly;
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

void Path::generate(Map *map, QVector<TileLayer *> &layers) const
{
    if (!map->tilesets().count())
        return;
    Tileset *ts = map->tilesets().first();
    for (int i = 0; i < mPoints.size() - 1; i++) {
        foreach (QPoint pt, calculateLine(mPoints[i]->x, mPoints[i]->y,
                                          mPoints[i+1]->x, mPoints[i+1]->y)) {
            if (!layers[0]->contains(pt))
                continue;
            Cell cell(ts->tileAt(3));
            layers[0]->setCell(pt.x(), pt.y(), cell);
        }
    }
}

/////

PathLayer::PathLayer()
    : Layer(PathLayerType, QString(), 0, 0, 0, 0)
{
}

PathLayer::PathLayer(const QString &name,
                     int x, int y, int width, int height)
    : Layer(PathLayerType, name, x, y, width, height)
{
    mPaths += new Path();
}

bool PathLayer::isEmpty() const
{
    return mPaths.isEmpty();
}

Layer *PathLayer::clone() const
{
    return initializeClone(new PathLayer(mName, mX, mY, mWidth, mHeight));
}

void PathLayer::generate(QVector<TileLayer *> &layers) const
{
    foreach (Path *path, mPaths)
        path->generate(map(), layers);
}

PathLayer *PathLayer::initializeClone(PathLayer *clone) const
{
    Layer::initializeClone(clone);

/*    clone->mImageSource = mImageSource;
    clone->mTransparentColor = mTransparentColor;
    clone->mImage = mImage;*/

    return clone;
}
