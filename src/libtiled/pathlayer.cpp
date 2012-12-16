// FIXME: license

#include "pathlayer.h"

#include "map.h"
#include "pathgenerator.h"
#include "tilelayer.h"
#include "tileset.h"

using namespace Tiled;

Path::Path() :
    mLayer(0),
    mIsClosed(false),
    mVisible(true)
{
//    addGenerator(new PG_SingleTile(this));
//    addGenerator(new PG_Fence(this));
    addGenerator(new PG_StreetLight(this));
}

int Path::level() const
{
    return mLayer ? mLayer->level() : 0;
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

void Path::generate(int level, QVector<TileLayer *> &layers) const
{
    foreach (PathGenerator *pathGen, mGenerators)
        pathGen->generate(level, layers);
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

void Path::addGenerator(PathGenerator *pathGen)
{
    mGenerators.append(pathGen);
}

PathGenerator *Path::removeGenerator(int index)
{
    return mGenerators.takeAt(index);
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

void PathLayer::generate(int level, QVector<TileLayer*> &layers) const
{
    if (!isVisible())
        return;
    foreach (Path *path, mPaths) {
        if (!path->isVisible())
            continue;
        path->generate(level, layers);
    }
}

PathLayer *PathLayer::initializeClone(PathLayer *clone) const
{
    Layer::initializeClone(clone);

    foreach (Path *path, mPaths)
        clone->addPath(path->clone());

    clone->setColor(mColor);

    return clone;
}
