// FIXME: license

#include "pathlayer.h"

#include "map.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;

Path::Path()
    : mIsClosed(false)
{
}

void Path::setPoints(const PathPoints &points)
{
    mPoints = points;
}

void Path::setPoint(int index, const PathPoint &point)
{
    mPoints[index] = point;
}

void Path::setPolygon(const QPolygon &polygon)
{
    mPoints.clear();
    foreach (QPoint pt, polygon)
        mPoints += PathPoint(pt.x(), pt.y());
}

QPolygon Path::polygon() const
{
    QPolygon poly;

    foreach (PathPoint pt, mPoints)
        poly.append(QPoint(pt.x(), pt.y()));

    return poly;
}

QPolygonF Path::polygonf() const
{
    QPolygonF poly;

    foreach (PathPoint pt, mPoints)
        poly.append(QPointF(pt.x() + 0.5, pt.y() + 0.5));

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
    if (!mPoints.size())
        return;

    Tileset *ts = map->tilesets().first();
    PathPoints points = mPoints;
    if (mIsClosed)
        points += points.first();
    for (int i = 0; i < points.size() - 1; i++) {
        foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                          points[i+1].x(), points[i+1].y())) {
            if (!layers[0]->contains(pt))
                continue;
            Cell cell(ts->tileAt(3));
            layers[0]->setCell(pt.x(), pt.y(), cell);
        }
    }

    if (!mIsClosed)
        return;

    QRect bounds(mPoints.first().x(), mPoints.first().y(), 1, 1);
    foreach (const PathPoint &pt, mPoints) {
        bounds |= QRect(pt.x(), pt.y(), 1, 1);
    }

    QPolygonF polygon = this->polygonf();
    for (int x = bounds.left(); x <= bounds.right(); x++)
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            QPointF pt(x + 0.5, y + 0.5);
            if (polygon.containsPoint(pt, Qt::WindingFill)) {
                if (!layers[0]->contains(pt.toPoint()))
                    continue;
                Cell cell(ts->tileAt(3));
                layers[0]->setCell(pt.x(), pt.y(), cell);
            }
        }
}

Path *Path::clone() const
{
    Path *klone = new Path();
    klone->setPathLayer(mLayer);
    klone->setPoints(mPoints);
    klone->setClosed(mIsClosed);
    klone->setVisible(mVisible);
    return klone;
}

void Path::translate(const QPoint &delta)
{
    for (int i = 0; i < mPoints.count(); i++) {
        mPoints[i].translate(delta);
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
}

PathLayer::~PathLayer()
{
    qDeleteAll(mPaths);
}

bool PathLayer::isEmpty() const
{
    return mPaths.isEmpty();
}

Layer *PathLayer::clone() const
{
    return initializeClone(new PathLayer(mName, mX, mY, mWidth, mHeight));
}

void PathLayer::addPath(Path *path)
{
    mPaths.append(path);
    path->setPathLayer(this);
}

void PathLayer::insertPath(int index, Path *path)
{
    mPaths.insert(index, path);
    path->setPathLayer(this);
}

int PathLayer::removePath(Path *path)
{
    const int index = mPaths.indexOf(path);
    Q_ASSERT(index != -1);

    mPaths.removeAt(index);
    path->setPathLayer(0);
    return index;
}

void PathLayer::generate(QVector<TileLayer *> &layers) const
{
    foreach (Path *path, mPaths)
        path->generate(map(), layers);
}

PathLayer *PathLayer::initializeClone(PathLayer *clone) const
{
    Layer::initializeClone(clone);

    foreach (Path *path, mPaths)
        clone->addPath(path->clone());

    clone->setColor(mColor);

    return clone;
}
