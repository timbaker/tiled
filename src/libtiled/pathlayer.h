// FIXME: license

#ifndef PATHLAYER_H
#define PATHLAYER_H

#include "layer.h"

#include <QMetaType>

namespace Tiled {

class TILEDSHARED_EXPORT PathPoint
{
public:
    int x, y;
};

class TILEDSHARED_EXPORT Path
{
public:
    Path();

    bool isClosed() const
    { return mIsClosed; }

    QPolygonF polygon() const;

    void generate(Map *map, QVector<TileLayer *> &layers) const;

private:
    QVector<PathPoint*> mPoints;
    bool mIsClosed;
};

class TILEDSHARED_EXPORT PathLayer : public Layer
{
public:
    PathLayer();
    PathLayer(const QString &name, int x, int y, int width, int height);

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

    void generate(QVector<TileLayer*> &layers) const;

protected:
    PathLayer *initializeClone(PathLayer *clone) const;

private:
    QList<Path*> mPaths;
};

} // namespace Tiled

Q_DECLARE_METATYPE(Tiled::PathLayer*)

#endif // PATHLAYER_H
