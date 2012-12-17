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

#include "tiled_global.h"

#include <QString>
#include <QVector>

namespace Tiled {

class Map;
class Path;
class Tile;
class TileLayer;

class PGP_Boolean;
class PGP_Integer;
class PGP_String;
class PGP_Layer;
class PGP_Tile;

class TILEDSHARED_EXPORT PathGeneratorProperty
{
public:
    PathGeneratorProperty(const QString &name, const QString &type);

    QString name() const
    { return mName; }

    QString type() const
    { return mType; }

//    virtual QString valueToString() const = 0;

    virtual PGP_Boolean *asBoolean() { return 0; }
    virtual PGP_Integer *asInteger() { return 0; }
    virtual PGP_String *asString() { return 0; }
    virtual PGP_Layer *asLayer() { return 0; }
    virtual PGP_Tile *asTile() { return 0; }

    virtual void clone(PathGeneratorProperty *other) = 0;

    QString mName; // West, Layer1
    QString mType; // String, Tile, Integer
};

class TILEDSHARED_EXPORT PGP_Boolean : public PathGeneratorProperty
{
public:
    PGP_Boolean(const QString &name);

    QString valueToString() const;

    PGP_Boolean *asBoolean() { return this; }

    void clone(PathGeneratorProperty *other);

    bool mValue;
};

class TILEDSHARED_EXPORT PGP_Integer : public PathGeneratorProperty
{
public:
    PGP_Integer(const QString &name);

    QString valueToString() const;

    PGP_Integer *asInteger() { return this; }

    void clone(PathGeneratorProperty *other);

    int mValue;
    int mMin;
    int mMax;
};

class TILEDSHARED_EXPORT PGP_String : public PathGeneratorProperty
{
public:
    PGP_String(const QString &name);

    PGP_String *asString() { return this; }

    void clone(PathGeneratorProperty *other);

    QString mValue;
};

class TILEDSHARED_EXPORT PGP_Layer : public PathGeneratorProperty
{
public:
    PGP_Layer(const QString &name);

    PGP_Layer *asLayer() { return this; }

    void clone(PathGeneratorProperty *other);

    QString mValue;
};

class TILEDSHARED_EXPORT PGP_Tile : public PathGeneratorProperty
{
public:
    PGP_Tile(const QString &name);

    PGP_Tile *asTile() { return this; }

    void clone(PathGeneratorProperty *other);

    QString mTilesetName;
    int mTileID;
};

class TILEDSHARED_EXPORT PathGenerator
{
public:
    PathGenerator(const QString &label, const QString &type);

    void setLabel(const QString &label)
    { mLabel = label; }

    QString label() const
    { return mLabel; }

    QString type() const
    { return mType; }

    const QVector<PathGeneratorProperty*> &properties() const
    { return mProperties; }

    virtual PathGenerator *clone() const = 0;

    virtual void generate(const Path *path, int level, QVector<TileLayer *> &layers);
    virtual void generate(int level, QVector<TileLayer *> &layers) = 0;

    void outline(Tile *tile, TileLayer *tl);
    void outlineWidth(Tile *tile, TileLayer *tl, int width);
    void fill(Tile *tile, TileLayer *tl);

protected:
    void cloneProperties(const PathGenerator *other);

protected:
    QString mLabel;
    QString mType;
    QVector<PathGeneratorProperty*> mProperties;
    const Path *mPath;
};

class TILEDSHARED_EXPORT PG_SingleTile : public PathGenerator
{
public:
    PG_SingleTile(const QString &label);

    PathGenerator *clone() const;

    void generate(int level, QVector<TileLayer*> &layers);

    QString mLayerName;
    QString mTilesetName;
    int mTileID;
};

class TILEDSHARED_EXPORT PG_Fence : public PathGenerator
{
public:
    PG_Fence(const QString &label);

    PathGenerator *clone() const;

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

    enum Properties
    {
        LayerName = TileCount,
        PropertyCount
    };
};

class TILEDSHARED_EXPORT PG_StreetLight : public PathGenerator
{
public:
    PG_StreetLight(const QString &label);

    PathGenerator *clone() const;

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

    enum Properties
    {
        LayerName = TileCount,
        Spacing,
        PropertyCount
    };
};

} // namespace Tiled

#endif // PATHGENERATOR_H
