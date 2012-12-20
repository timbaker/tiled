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

PathGeneratorProperty::PathGeneratorProperty(const QString &name, const QString &type) :
    mName(name),
    mType(type)
{
}

PGP_Boolean::PGP_Boolean(const QString &name) :
    PathGeneratorProperty(name, QLatin1String("Boolean")),
    mValue(false)
{
}

void PGP_Boolean::clone(PathGeneratorProperty *other)
{
    mValue = other->asBoolean()->mValue;
}

QString PGP_Boolean::valueToString() const
{
    return QLatin1String(mValue ? "true" : "false");
}

bool PGP_Boolean::valueFromString(const QString &s)
{
    if (s == QLatin1String("true"))
        mValue = true;
    else if (s == QLatin1String("false"))
        mValue = false;
    else
        return false;
    return true;
}

PGP_Integer::PGP_Integer(const QString &name) :
    PathGeneratorProperty(name, QLatin1String("Integer")),
    mValue(1),
    mMin(1),
    mMax(100)
{
}

void PGP_Integer::clone(PathGeneratorProperty *other)
{
    mValue = other->asInteger()->mValue;
}

QString PGP_Integer::valueToString() const
{
    return QString::number(mValue);
}

bool PGP_Integer::valueFromString(const QString &s)
{
    bool ok;
    int value = s.toInt(&ok);
    if (ok && value >= mMin && value <= mMax) {
        mValue = value;
        return true;
    }
    return false;
}

PGP_String::PGP_String(const QString &name) :
    PathGeneratorProperty(name, QLatin1String("String"))
{
}

void PGP_String::clone(PathGeneratorProperty *other)
{
    mValue = other->asString()->mValue;
}

QString PGP_String::valueToString() const
{
    return mValue;
}

bool PGP_String::valueFromString(const QString &s)
{
    mValue = s;
    return true;
}

PGP_Layer::PGP_Layer(const QString &name) :
    PathGeneratorProperty(name, QLatin1String("Layer"))
{
}

void PGP_Layer::clone(PathGeneratorProperty *other)
{
    mValue = other->asLayer()->mValue;
}

QString PGP_Layer::valueToString() const
{
    return mValue;
}

bool PGP_Layer::valueFromString(const QString &s)
{
    mValue = s;
    return true;
}

PGP_Tile::PGP_Tile(const QString &name) :
    PathGeneratorProperty(name, QLatin1String("Tile"))
{
}

void PGP_Tile::clone(PathGeneratorProperty *other)
{
    mTilesetName = other->asTile()->mTilesetName;
    mTileID = other->asTile()->mTileID;
}

QString PGP_Tile::valueToString() const
{
    return tileName(mTilesetName, mTileID);
}

static bool parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
    QString indexString = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1);

    // Strip leading zeroes from the tile index
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);

    bool ok;
    index = indexString.toInt(&ok);
    return ok;
}

bool PGP_Tile::valueFromString(const QString &s)
{
    if (s.isEmpty()) {
        mTilesetName.clear();
        mTileID = 0;
        return true;
    }
    QString tilesetName;
    int index;
    if (parseTileName(s, tilesetName, index)) {
        mTilesetName = tilesetName;
        mTileID = index;
        return true;
    }
    return false;
}

QString PGP_Tile::tileName(const QString &tilesetName, int tileID) const
{
    if (tilesetName.isEmpty())
        return QString();
    return tilesetName + QLatin1Char('_') + QString::number(tileID);
}

/////

PathGenerator::PathGenerator(const QString &label, const QString &type) :
    mLabel(label),
    mType(type),
    mRefCount(0),
    mPath(0)
{
}

void PathGenerator::generate(const Path *path, int level, QVector<TileLayer *> &layers)
{
    mPath = path;
    generate(level, layers);
}

static Tileset *findTileset(const QString &name, const QList<Tileset*> &tilesets)
{
    foreach (Tileset *ts, tilesets) {
        if (ts->name() == name)
            return ts;
    }
    return 0;
}

static QString layerNameWithoutPrefix(const QString &name)
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
    bool swapped = false;
    if (x0 > x1) {
        qSwap(x0, x1);
        qSwap(y0, y1);
        swapped = true;
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

    ret.reserve(x1 + 1 - x0 + 1);

    for (int x = x0; x < x1 + 1 ; x++) {
        if (swapped) {
            if (steep)
                ret.prepend( QPoint(y, x) );
            else
                ret.prepend( QPoint(x, y) );
        } else {
            if (steep)
                ret += QPoint(y, x);
            else
                ret += QPoint(x, y);
        }
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

QVector<QPoint> PathGenerator::pointsAlongPath(int offset, int spacing)
{
    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    QVector<QPoint> ret;

    int distance = -offset;
    for (int i = 0; i < points.size() - 1; i++) {
        QVector<QPoint> pts = calculateLine(points[i].x(), points[i].y(),
                                            points[i+1].x(), points[i+1].y());
        for (int j = 0; j < pts.size(); j++) {
            if (i > 0 || j == 0)
                continue; // skip shared point
            if (distance >= 0 && !(distance % spacing))
                ret += pts[j];
        }
    }

    return ret;
}

PathGenerator::Direction PathGenerator::direction(int segment)
{
    PathPoints pts = mPath->points();
    if (pts.isEmpty())
        return Angled;
    if (mPath->isClosed())
        pts += pts[0];
    if (segment >= 0 && segment < pts.size() - 1)
        return direction(pts[segment].toPoint(), pts[segment+1].toPoint());
    return Angled;
}

PathGenerator::Direction PathGenerator::direction(QPoint p0, QPoint p1)
{
    if (p0.x() == p1.x()) {
        if (p0.y() < p1.y()) return NorthSouth;
        else return SouthNorth;
    }

    if (p0.y() == p1.y()) {
        if (p0.x() < p1.x()) return WestEast;
        else return EastWest;
    }

    return Angled;
}

void PathGenerator::cloneProperties(const PathGenerator *other)
{
    for (int i = 0; i < mProperties.size(); i++)
        mProperties[i]->clone(other->mProperties[i]);
}

/////

PG_SingleTile::PG_SingleTile(const QString &label) :
    PathGenerator(label, QLatin1String("SingleTile")),
    mLayerName(QLatin1String("Floor")),
    mTilesetName(QLatin1String("floors_exterior_street_01")),
    mTileID(18)
{
}

PathGenerator *PG_SingleTile::clone() const
{
    PG_SingleTile *clone = new PG_SingleTile(mLabel);
    clone->cloneProperties(this);
    return clone;
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

PG_Fence::PG_Fence(const QString &label) :
    PathGenerator(label, QLatin1String("Fence"))
{
    mProperties.resize(PropertyCount);

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("West1"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 11;
        mProperties[West1] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("West2"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 10;
        mProperties[West2] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("North1"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 8;
        mProperties[North1] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("North2"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 9;
        mProperties[North2] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("NorthWest"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 12;
        mProperties[NorthWest] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("Post"))) {
        prop->mTilesetName = QLatin1String("fencing_01");
        prop->mTileID = 13;
        mProperties[Post] = prop;
    }

    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("Layer1"))) {
        prop->mValue = QLatin1String("Furniture");
        mProperties[Layer1] = prop;
    }

    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("Layer2"))) {
        prop->mValue = QLatin1String("Furniture2");
        mProperties[Layer2] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("PostJoin"))) {
        mProperties[PostJoin] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("PostStart"))) {
        mProperties[PostStart] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("PostEnd"))) {
        mProperties[PostEnd] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("SouthEastFixup"))) {
        mProperties[SouthEastFixup] = prop;
    }

#if 0
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
#endif

#if 0
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

PathGenerator *PG_Fence::clone() const
{
    PG_Fence *clone = new PG_Fence(mLabel);
    clone->cloneProperties(this);
    return clone;
}

void PG_Fence::generate(int level, QVector<TileLayer *> &layers)
{
    if (level != mPath->level())
        return;
    if (mPath->points().size() < 2)
        return;

    QVector<TileLayer*> tl;
    tl += findTileLayer(mProperties[Layer1]->asLayer()->mValue, layers);
    if (!tl[0]) return;
    tl += findTileLayer(mProperties[Layer2]->asLayer()->mValue, layers);
    if (!tl[1]) return;

    QVector<Tile*> tiles(TileCount);
    for (int i = 0; i < TileCount; i++) {
        PGP_Tile *prop = mProperties[i]->asTile();
        if (prop->mTilesetName.isEmpty())
            continue;
        Tileset *ts = findTileset(prop->mTilesetName, layers[0]->map()->tilesets());
        if (!ts) return;
        tiles[i] = ts->tileAt(prop->mTileID);
        if (!tiles[i]) return;
    }

    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    for (int i = 0; i < points.size() - 1; i++) {
        QPoint p0 = points[i].toPoint(), p1 = points[i + 1].toPoint();
        Direction dir = direction(p0, p1);
        int alternate = 0;
        if (dir == WestEast || dir == EastWest) {
            if (dir == EastWest)
                qSwap(p0, p1);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                int x = pt.x(), y = pt.y();
                if (pt == p1) {

                    // Place SE post
                    int tileN = tileAt(tl, x, y - 1, tiles);
                    if (tileN == West2 || (tileN == West1 && tiles[West1] == tiles[West2])) {
                        setCell(OrientSE, alternate, tl, x, y, tiles);
                        continue;
                    }
                    continue;
                }

                if (tileAt(tl, x, y, tiles) == West1)
                    setCell(OrientNW, alternate, tl, x, y, tiles);
                else
                    setCell(OrientN, alternate, tl, x, y, tiles);
                alternate = !alternate;
            }

        } else if (dir == NorthSouth || dir == SouthNorth) {
            if (dir == SouthNorth)
                qSwap(p0, p1);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                int x = pt.x(), y = pt.y();
                if (pt == p1) {

                    int tileW = tileAt(tl, x - 1, y, tiles);
                    // Place SE post.
                    if (tileW == North2 || (tileW == North1 && tiles[North1] == tiles[North2]) || tileW == NorthWest) {
                        setCell(OrientSE, alternate, tl, x, y, tiles);
                        continue;
                    }
                    continue;
                }

                if (tileAt(tl, x, y, tiles) == North1)
                    setCell(OrientNW, alternate, tl, x, y, tiles);
                else
                    setCell(OrientW, alternate, tl, x, y, tiles);
                alternate = !alternate;
            }
        }
    }

    if (mProperties[PostStart]->asBoolean()->mValue && tiles[Post]) {
        PathPoint p = points[0];
        int tile = tileAt(tl, p.x(), p.y(), tiles);
        if (tile != NorthWest)
            setTile(tl, p.x(), p.y(), Post, tiles, false);
    }

    if (mProperties[PostEnd]->asBoolean()->mValue && tiles[Post]) {
        PathPoint p = points.last();
        if (tileAt(tl, p.x(), p.y(), tiles) != NorthWest)
            setTile(tl, p.x(), p.y(), Post, tiles, false);
    }
}

int PG_Fence::layerForTile(int tile)
{
    if (tile == Post)
        return Layer2;
    return Layer1;
}

int PG_Fence::tileAt(QVector<TileLayer *> layers, int x, int y, QVector<Tile *> &tiles)
{
    if (!layers[0]->contains(x, y))
        return -1;
    for (int i = 0; i < TileCount; i++) {
        if (i == Post) continue;
        if (!tiles[i]) continue;
        if (layers[layerForTile(i) - Layer1]->cellAt(x, y).tile == tiles[i])
            return i;
    }
    return -1;
}

void PG_Fence::setTile(QVector<TileLayer *> layers, int x, int y, int tileEnum,
                       QVector<Tile *> &tiles, bool clear)
{
    if (!layers[0]->contains(x, y))
        return;

    if (clear) {
        foreach (TileLayer *tl, layers)
            tl->setCell(x, y, Cell());
    }

    int layerIndex = layerForTile(tileEnum) - Layer1;
    layers[layerIndex]->setCell(x, y, Cell(tiles[tileEnum]));
}

void PG_Fence::setCell(TileOrientation orient, int alternate,
                       QVector<TileLayer *> layers,  int x, int y,
                       QVector<Tile *> &tiles)
{
    bool postJoin = mProperties[PostJoin]->asBoolean()->mValue && tiles[Post];

    switch (orient) {
    case OrientW:
        setTile(layers, x, y, West1 + alternate, tiles);
        if (postJoin) {
            int tileW = tileAt(layers, x - 1, y, tiles);
            if (tileW == North2 || (tileW == North1 && tiles[North1] == tiles[North2]) || tileW == NorthWest)
                setTile(layers, x, y, Post, tiles, false); // NE
            int tileS = tileAt(layers, x, y + 1, tiles);
            if (tileS == North2 || (tileS == North1 && tiles[North1] == tiles[North2]))
                setTile(layers, x, y + 1, Post, tiles, false); // SW
        }
        break;
    case OrientN:
        setTile(layers, x, y, North1 + alternate, tiles);
        if (mProperties[SouthEastFixup]->asBoolean()->mValue) {
            if (alternate && tileAt(layers, x + 1, y, tiles) == NorthWest)
                setTile(layers, x, y, North1, tiles, false);
        }
        if (postJoin) {
            int tileN = tileAt(layers, x, y - 1, tiles);
            if (tileN == West2 || (tileN == West1 && tiles[West1] == tiles[West2]))
                setTile(layers, x, y, Post, tiles, false); // SW
            int tileE = tileAt(layers, x + 1, y, tiles);
            if (tileE == West1)
                setTile(layers, x + 1, y, Post, tiles, false); // NE
        }
        break;
    case OrientSW:
        break;
    case OrientNW:
        setTile(layers, x, y, NorthWest, tiles);
        if (mProperties[SouthEastFixup]->asBoolean()->mValue) {
            if (tileAt(layers, x - 1, y, tiles) == North2)
                setTile(layers, x - 1, y, North1, tiles, false);
        }
        break;
    case OrientNE:
        break;
    case OrientSE:
        setTile(layers, x, y, Post, tiles, false);
        // This is a hack for the "farm" fence which has
        // West1/Post/North1 tiles at the SE corner.
        if (mProperties[SouthEastFixup]->asBoolean()->mValue) {
            setTile(layers, x, y - 1, West1, tiles, false);
            setTile(layers, x - 1, y, North1, tiles, false);
        }
        break;
    }
}

/////

PG_StreetLight::PG_StreetLight(const QString &label) :
    PathGenerator(label, QLatin1String("StreetLight"))
{
    mProperties.resize(PropertyCount);

    PGP_Tile *prop = new PGP_Tile(QLatin1String("West"));
    prop->mTilesetName = QLatin1String("lighting_outdoor_01");
    prop->mTileID = 9;
    mProperties[West] = prop;

    prop = new PGP_Tile(QLatin1String("North"));
    prop->mTilesetName = QLatin1String("lighting_outdoor_01");
    prop->mTileID = 10;
    mProperties[North] = prop;

    prop = new PGP_Tile(QLatin1String("East"));
    prop->mTilesetName = QLatin1String("lighting_outdoor_01");
    prop->mTileID = 11;
    mProperties[East] = prop;

    prop = new PGP_Tile(QLatin1String("South"));
    prop->mTilesetName = QLatin1String("lighting_outdoor_01");
    prop->mTileID = 8;
    mProperties[South] = prop;

    prop = new PGP_Tile(QLatin1String("Base"));
    prop->mTilesetName = QLatin1String("lighting_outdoor_01");
    prop->mTileID = 16;
    mProperties[Base] = prop;

    PGP_Layer *prop2 = new PGP_Layer(QLatin1String("Layer"));
    prop2->mValue = QLatin1String("Furniture");
    mProperties[LayerName] = prop2;

    if (PGP_Integer *prop = new PGP_Integer(QLatin1String("Offset"))) {
        prop->mMin = 0, prop->mMax = 300, prop->mValue = 0;
        mProperties[Offset] = prop;
    }

    PGP_Integer *prop3 = new PGP_Integer(QLatin1String("Spacing"));
    prop3->mMin = 1, prop3->mMax = 300, prop3->mValue = 10;
    mProperties[Spacing] = prop3;

    PGP_Boolean *prop4 = new PGP_Boolean(QLatin1String("Reverse"));
    prop4->mValue = false;
    mProperties[Reverse] = prop4;
}

PathGenerator *PG_StreetLight::clone() const
{
    PG_StreetLight *clone = new PG_StreetLight(mLabel);
    clone->cloneProperties(this);
    return clone;
}

void PG_StreetLight::generate(int level, QVector<TileLayer *> &layers)
{
    bool level0 = level == mPath->level();
    bool level1 = level == mPath->level() + 1;
    if (!level0 && !level1)
        return;
    if (mPath->points().size() < 2)
        return;

    PGP_Layer *prop = mProperties[LayerName]->asLayer();
    TileLayer *tl = findTileLayer(prop->mValue, layers);
    if (!tl) return;

    QVector<Tile*> tiles(TileCount);
    for (int i = 0; i < TileCount; i++) {
        PGP_Tile *prop = mProperties[i]->asTile();
        if (prop->mTilesetName.isEmpty())
            continue;
        Tileset *ts = findTileset(prop->mTilesetName, tl->map()->tilesets());
        if (!ts) return;
        tiles[i] = ts->tileAt(prop->mTileID);
        if (!tiles[i]) return;
    }

    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    if (tl->map()->orientation() == Map::Isometric && level1) {
        for (int i = 0; i < points.size(); i++)
            points[i].translate(QPoint(-3, -3));
    }

    int offset = mProperties[Offset]->asInteger()->mValue;
    int spacing = mProperties[Spacing]->asInteger()->mValue;
    bool reverse = mProperties[Reverse]->asBoolean()->mValue;

    int orient = -1;
    int distance = -offset;
    for (int i = 0; i < points.size() - 1; i++) {
        QPoint p0 = points[i].toPoint(), p1 = points[i + 1].toPoint();
        Direction dir = direction(p0, p1);
        if (dir == WestEast || dir == EastWest) {
            if (orient == -1) orient = reverse ? South : North;
            if (Tile *tile = tiles[level1 ? orient : Base]) {
                foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                                  points[i+1].x(), points[i+1].y())) {
                    if (i > 0 && pt == points[i].toPoint())
                        continue; // skip shared point
                    if (tl->contains(pt) && (distance >= 0) && !(distance % spacing)) {
                        tl->setCell(pt.x(), pt.y(), Cell(tile));
                    }
                    ++distance;
                }
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir == WestEast) {
                    if (dir2 == NorthSouth)
                        orient = (orient == South) ? West : East;
                    else if (dir2 == SouthNorth)
                        orient = (orient == South) ? East : West;
                } else {
                    if (dir2 == NorthSouth)
                        orient = (orient == South) ? East : West;
                    else if (dir2 == SouthNorth)
                        orient = (orient == South) ? West : East;
                }
            }
        } else if (dir == NorthSouth || dir == SouthNorth) {
            if (orient == -1) orient = reverse ? East : West;
            if (Tile *tile = tiles[level1 ? orient : Base]) {
                foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                                  points[i+1].x(), points[i+1].y())) {
                    if (i > 0 && pt == points[i].toPoint())
                        continue; // skip shared point
                    if (tl->contains(pt) && (distance >= 0) && !(distance % spacing)) {
                        tl->setCell(pt.x(), pt.y(), Cell(tile));
                    }
                    ++distance;
                }
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir == NorthSouth) {
                    if (dir2 == WestEast)
                        orient = (orient == East) ? North : South;
                    else if (dir2 == EastWest)
                        orient = (orient == East) ? South : North;
                } else {
                    if (dir2 == WestEast)
                        orient = (orient == East) ? South : North;
                    else if (dir2 == EastWest)
                        orient = (orient == East) ? North : South;
                }
            }
        } else {
            Tile *tile = tiles[level1 ? (reverse ? East : West) : Base];
            if (!tile)
                continue;
            foreach (QPoint pt, calculateLine(points[i].x(), points[i].y(),
                                              points[i+1].x(), points[i+1].y())) {
                if (i > 0 && pt == points[i].toPoint())
                    continue; // skip shared point
                if (tl->contains(pt) && (distance >= 0) && !(distance % spacing)) {
                    tl->setCell(pt.x(), pt.y(), Cell(tile));
                }
                ++distance;
            }
        }
    }
}

/////

PG_WithCorners::PG_WithCorners(const QString &label) :
    PathGenerator(label, QLatin1String("WithCorners"))
{
    mProperties.resize(PropertyCount);

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("West"))) {
        mProperties[West] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("North"))) {
        mProperties[North] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("East"))) {
        mProperties[East] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("South"))) {
        mProperties[South] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("OuterSouthWest"))) {
        mProperties[OuterSouthWest] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("OuterNorthWest"))) {
        mProperties[OuterNorthWest] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("OuterNorthEast"))) {
        mProperties[OuterNorthEast] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("OuterSouthEast"))) {
        mProperties[OuterSouthEast] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("InnerSouthWest"))) {
        mProperties[InnerSouthWest] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("InnerNorthWest"))) {
        mProperties[InnerNorthWest] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("InnerNorthEast"))) {
        mProperties[InnerNorthEast] = prop;
    }

    if (PGP_Tile *prop = new PGP_Tile(QLatin1String("InnerSouthEast"))) {
        mProperties[InnerSouthEast] = prop;
    }


    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("LayerWest"))) {
        mProperties[LayerWest] = prop;
    }

    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("LayerNorth"))) {
        mProperties[LayerNorth] = prop;
    }

    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("LayerEast"))) {
        mProperties[LayerEast] = prop;
    }

    if (PGP_Layer *prop = new PGP_Layer(QLatin1String("LayerSouth"))) {
        mProperties[LayerSouth] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("BlendInner"))) {
        mProperties[BlendInner] = prop;
    }

    if (PGP_Boolean *prop = new PGP_Boolean(QLatin1String("Reverse"))) {
        mProperties[Reverse] = prop;
    }
}

PathGenerator *PG_WithCorners::clone() const
{
    PG_WithCorners *clone = new PG_WithCorners(mLabel);
    clone->cloneProperties(this);
    return clone;
}

void PG_WithCorners::generate(int level, QVector<TileLayer *> &layers)
{
    if (level != mPath->level())
        return;

    QVector<TileLayer*> tl(4);
    for (int i = 0; i < 4; i++) {
        if (PGP_Layer *prop = mProperties[LayerWest+i]->asLayer()) {
            tl[i] = findTileLayer(prop->mValue, layers);
            if (!tl[i]) return;
        }
    }

    QVector<Tile*> tiles(TileCount);
    for (int i = 0; i < TileCount; i++) {
        PGP_Tile *prop = mProperties[i]->asTile();
        if (prop->mTilesetName.isEmpty())
            continue;
        Tileset *ts = findTileset(prop->mTilesetName, tl[0]->map()->tilesets());
        if (!ts) return;
        tiles[i] = ts->tileAt(prop->mTileID);
        if (!tiles[i]) return;
    }

    bool reverse = false;
    if (PGP_Boolean *prop = mProperties[Reverse]->asBoolean())
        reverse = prop->mValue;

    PathPoints points = mPath->points();
    if (mPath->isClosed())
        points += points.first();

    int orient = -1;
    for (int i = 0; i < points.size() - 1; i++) {
        QPoint p0 = points[i].toPoint(), p1 = points[i + 1].toPoint();
        Direction dir = direction(p0, p1);
        if (dir == WestEast) {
            if (orient == -1) orient = reverse ? South : North;
            Q_ASSERT(orient == South || orient == North);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                setCell(tl, pt, orient, tiles);
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir2 == NorthSouth)
                    orient = (orient == South) ? West : East;
                else if (dir2 == SouthNorth)
                    orient = (orient == South) ? East : West;
            }
        } else if (dir == EastWest) {
            if (orient == -1) orient = reverse ? South : North;
            Q_ASSERT(orient == South || orient == North);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                setCell(tl, pt, orient, tiles);
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir2 == NorthSouth)
                    orient = (orient == South) ? East : West;
                else if (dir2 == SouthNorth)
                    orient = (orient == South) ? West : East;
            }
        } else if (dir == NorthSouth) {
            if (orient == -1) orient = reverse ? East : West;
            Q_ASSERT(orient == East || orient == West);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                setCell(tl, pt, orient, tiles);
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir2 == WestEast)
                    orient = (orient == East) ? North : South;
                else if (dir2 == EastWest)
                    orient = (orient == East) ? South : North;
            }
        } else if (dir == SouthNorth) {
            if (orient == -1) orient = reverse ? East : West;
            Q_ASSERT(orient == East || orient == West);
            foreach (QPoint pt, calculateLine(p0.x(), p0.y(),
                                              p1.x(), p1.y())) {
                setCell(tl, pt, orient, tiles);
            }
            if (i + 1 < points.size() - 1) {
                QPoint p2 = points[i+2].toPoint();
                Direction dir2 = direction(p1, p2);
                if (dir2 == WestEast)
                    orient = (orient == East) ? South : North;
                else if (dir2 == EastWest)
                    orient = (orient == East) ? North : South;
            }
        }
    }
}

int PG_WithCorners::tileAt(QVector<TileLayer *> &layers, int x, int y,
                           QVector<Tile*> &tiles)
{
    if (!layers[0]->contains(x, y))
        return -1;

    if (mProperties[BlendInner]->asBoolean()->mValue) {
        TileLayer *tW = layers.at(layerForTile(West)-LayerWest);
        TileLayer *tS = layers.at(layerForTile(South)-LayerWest);
        if (tW->cellAt(x, y).tile == tiles[West] && tS->cellAt(x, y).tile == tiles[South])
            return InnerNorthEast;
        TileLayer *tN = layers.at(layerForTile(South)-LayerWest);
        if (tW->cellAt(x, y).tile == tiles[West] && tN->cellAt(x, y).tile == tiles[North])
            return InnerSouthEast;
        TileLayer *tE = layers.at(layerForTile(South)-LayerWest);
        if (tE->cellAt(x, y).tile == tiles[East] && tN->cellAt(x, y).tile == tiles[North])
            return InnerSouthWest;
        if (tE->cellAt(x, y).tile == tiles[East] && tS->cellAt(x, y).tile == tiles[South])
            return InnerNorthWest;
    }

    for (int i = 0; i < TileCount; i++) {
        TileLayer *tl = layers.at(layerForTile(i)-LayerWest);
        if (tl->contains(x, y) && tl->cellAt(x, y).tile == tiles[i]) {
            return i;
        }
    }

    return -1;
}

int PG_WithCorners::layerForTile(int tile)
{
    switch (tile) {
    case West:
        return LayerWest;
    case North:
    case OuterNorthWest:
    case OuterNorthEast:
    case InnerNorthWest:
    case InnerNorthEast:
        return LayerNorth;
    case East:
        return LayerEast;
    case South:
    case OuterSouthWest:
    case OuterSouthEast:
    case InnerSouthWest:
    case InnerSouthEast:
        return LayerSouth;
    }
    Q_ASSERT_X(false, "PG_WithCorners::layerForTile", "unhandled tile");
    return LayerWest;
}

void PG_WithCorners::setTile(QVector<TileLayer *> layers, QPoint p,
                             int tileEnum, QVector<Tile *> &tiles)
{
    foreach (TileLayer *tl, layers)
        tl->setCell(p, Cell());
    if (mProperties[BlendInner]->asBoolean()->mValue) {
        if (tileEnum == InnerSouthWest) {
            layers[layerForTile(North)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[North]));
            layers[layerForTile(East)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[East]));
            return;
        } else if (tileEnum == InnerNorthWest) {
            layers[layerForTile(East)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[East]));
            layers[layerForTile(South)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[South]));
        } else if (tileEnum == InnerNorthEast) {
            layers[layerForTile(West)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[West]));
            layers[layerForTile(South)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[South]));
            return;
        } else if (tileEnum == InnerSouthEast) {
            layers[layerForTile(West)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[West]));
            layers[layerForTile(North)-LayerWest]->setCell(p.x(), p.y(), Cell(tiles[North]));
            return;
        }
    }
    int layerIndex = layerForTile(tileEnum) - LayerWest;
    layers[layerIndex]->setCell(p, Cell(tiles[tileEnum]));
}

void PG_WithCorners::setCell(QVector<TileLayer *> layers, QPoint p,
                             int tileEnum, QVector<Tile *> &tiles)
{
    if (layers[0]->contains(p)) {
        int tileCur = tileAt(layers, p.x(), p.y(), tiles);
        if ((tileEnum == East && tileCur == South) || (tileEnum == South && tileCur == East)) {
            int tileE = tileAt(layers, p.x()+1,p.y(), tiles);
            int tileS = tileAt(layers,p.x(),p.y()+1, tiles);
            bool inner = tileE == South || tileE == OuterSouthEast ||
                    tileS == East || tileS == OuterSouthEast;
            setTile(layers, p, inner ? InnerNorthWest : OuterSouthEast, tiles);

        } else if ((tileEnum == East && tileCur == North) || (tileEnum == North && tileCur == East)) {
            int tileE = tileAt(layers, p.x()+1,p.y(), tiles);
            int tileN = tileAt(layers,p.x(),p.y()-1,tiles);
            bool inner = tileE == North || tileE == OuterNorthEast ||
                    tileN == East || tileN == OuterNorthEast;
            setTile(layers, p, inner ? InnerSouthWest : OuterNorthEast, tiles);
        } else if ((tileEnum == West && tileCur == South) || (tileEnum == South && tileCur == West)) {
            int tileW = tileAt(layers,p.x()-1,p.y(),tiles);
            int tileS = tileAt(layers,p.x(),p.y()+1,tiles);
            bool inner = tileW == South || tileW == OuterSouthWest ||
                    tileS == West || tileS == OuterSouthWest;
            setTile(layers, p, inner ? InnerNorthEast : OuterSouthWest, tiles);

        } else if ((tileEnum == West && tileCur == North) || (tileEnum == North && tileCur == West)) {
            int tileW = tileAt(layers,p.x()-1,p.y(),tiles);
            int tileN = tileAt(layers,p.x(),p.y()-1,tiles);
            bool inner = tileW == North || tileW == OuterNorthWest ||
                    tileN == West || tileN == OuterNorthWest;
            setTile(layers, p, inner ? InnerSouthEast : OuterNorthWest, tiles);
        } else {
            setTile(layers, p, tileEnum, tiles);
        }
    }
}
