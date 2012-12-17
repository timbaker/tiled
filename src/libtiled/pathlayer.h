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


#ifndef PATHLAYER_H
#define PATHLAYER_H

#include "layer.h"

#include <QMetaType>
#include <QPolygon>

namespace Tiled {

class PathGenerator;

class TILEDSHARED_EXPORT PathPoint
{
public:
    PathPoint()
        : mX(0)
        , mY(0)
    {

    }

    PathPoint(int x, int y)
        : mX(x)
        , mY(y)
    {

    }

    int x() const
    { return mX; }

    int y() const
    { return mY; }

    bool operator == (const PathPoint &other) const
    { return mX == other.mX && mY == other.mY; }

    void translate(const QPoint &delta)
    { mX += delta.x(), mY += delta.y(); }

    QPoint toPoint() const
    { return QPoint(mX, mY); }

private:
    int mX, mY;
};

typedef QVector<PathPoint> PathPoints;

class TILEDSHARED_EXPORT Path
{
public:
    Path();

    PathLayer *pathLayer() const
    { return mLayer; }

    void setPathLayer(PathLayer *pathLayer)
    { mLayer = pathLayer; }

    int level() const;

    void setPoints(const PathPoints &points);
    void setPoint(int index, const PathPoint &point);

    const PathPoints points() const
    { return mPoints; }

    int numPoints() const
    { return mPoints.count(); }

    void setClosed(bool closed)
    { mIsClosed = closed; }

    bool isClosed() const
    { return mIsClosed; }

    bool isVisible() const
    { return mVisible; }

    void setVisible(bool visible)
    { mVisible = visible; }

    void setPolygon(const QPolygon &polygon);

    QPolygon polygon() const;

    QPolygonF polygonf() const;

    void generate(int level, QVector<TileLayer*> &layers) const;

    Path *clone() const;

    void translate(const QPoint &delta);

    void insertGenerator(int index, PathGenerator *pathGen);
    PathGenerator *removeGenerator(int index);

    const QList<PathGenerator*> &generators() const
    { return mGenerators; }

private:
    PathLayer *mLayer;
    PathPoints mPoints;
    bool mIsClosed;
    bool mVisible;

    QList<PathGenerator*> mGenerators;
};

class TILEDSHARED_EXPORT PathLayer : public Layer
{
public:
    PathLayer();
    PathLayer(const QString &name, int x, int y, int width, int height);
    ~PathLayer();

    QSet<Tileset*> usedTilesets() const { return QSet<Tileset*>(); }
    bool referencesTileset(const Tileset *) const { return false; }
    void replaceReferencesToTileset(Tileset *, Tileset *) {}

    void offset(const QPoint &/*offset*/, const QRect &/*bounds*/,
                bool /*wrapX*/, bool /*wrapY*/)
    {}

    bool canMergeWith(Layer *) const { return false; }
    Layer *mergedWith(Layer *) const { return 0; }

    bool isEmpty() const;

    Layer *clone() const;

    const QList<Path*> &paths() const
    { return mPaths; }

    int pathCount() const
    { return mPaths.count(); }

    void addPath(Path *path);
    void insertPath(int index, Path *path);
    int removePath(Path *path);

    const QColor &color() const { return mColor; }
    void setColor(const QColor &color) {  mColor = color; }

    void generate(int level, QVector<TileLayer*> &layers) const;

protected:
    PathLayer *initializeClone(PathLayer *clone) const;

private:
    QList<Path*> mPaths;
    QColor mColor;
};

} // namespace Tiled

Q_DECLARE_METATYPE(Tiled::PathLayer*)

#endif // PATHLAYER_H
