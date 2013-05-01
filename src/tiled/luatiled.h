/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef LUATILED_H
#define LUATILED_H

#include <QList>
#include <QMap>
#include <QRegion>
#include <QRgb>

extern "C" {
struct lua_State;
}

namespace Tiled {
class BmpRule;
class Layer;
class Map;
class MapBmp;
class Tile;
class TileLayer;
class Tileset;

namespace Lua {

class LuaMap;
class LuaTileLayer;

class LuaLayer
{
public:
    LuaLayer();
    LuaLayer(Layer *orig);
    virtual ~LuaLayer();

    const char *name();

    virtual LuaTileLayer *asTileLayer() { return 0; }

    virtual const char *type() const { return "unknown"; }

    void initClone();
    virtual void cloned();

    Layer *mClone;
    Layer *mOrig;
    QString mName;
};

class LuaTileLayer : public LuaLayer
{
public:
    LuaTileLayer(TileLayer *orig);
    LuaTileLayer(const char *name, int x, int y, int width, int height);
    ~LuaTileLayer();

    LuaTileLayer *asTileLayer() { return this; }
    const char *type() const { return "tile"; }

    void cloned();

    void setTile(int x, int y, Tile *tile);
    Tile *tileAt(int x, int y);

    void replaceTile(Tile *oldTile, Tile *newTile);

    TileLayer *mCloneTileLayer;
    QRegion mAltered;
    LuaMap *mMap;
};

class LuaColor
{
public:
    LuaColor() :
        r(0), g(0), b(0), pixel(qRgb(0, 0, 0))
    {}
    LuaColor(int r, int g, int b) :
        r(r), g(g), b(b), pixel(qRgb(r, g, b))
    {}
    LuaColor(QRgb pixel) :
        r(qRed(pixel)),
        g(qGreen(pixel)),
        b(qBlue(pixel)),
        pixel(pixel)
    {}

    int r;
    int g;
    int b;
    QRgb pixel;
};

class LuaMapBmp
{
public:
    LuaMapBmp(MapBmp &bmp);

    bool contains(int x, int y);

    void setPixel(int x, int y, LuaColor &c);
    LuaColor pixel(int x, int y);

    void erase(int x, int y, int width, int height);
    void erase(QRect &r);
    void erase(QRegion &rgn);
    void erase();

    void fill(int x, int y, int width, int height, LuaColor &c);
    void fill(QRect &r, LuaColor &c);
    void fill(QRegion &rgn, LuaColor &c);
    void fill(LuaColor &c);

    void replace(LuaColor &oldColor, LuaColor &newColor);

    MapBmp &mBmp;
    QRegion mAltered;
};

class LuaBmpRule
{
public:
    LuaBmpRule() :
        mRule(0)
    {}
    LuaBmpRule(BmpRule *rule) :
        mRule(rule)
    {}

    const char *label();
    int bmpIndex();
    LuaColor color();
    QStringList tiles();
    const char *layer();
    LuaColor condition();

    BmpRule *mRule;
};

class LuaMap
{
public:
    LuaMap(Map *orig);
    ~LuaMap();

    void setTileSelection(const QRegion &selection)
    { mSelection = selection; }

    QRegion tileSelection()
    { return mSelection; }

    int width() const;
    int height() const;

    int layerCount() const;
    LuaLayer *layerAt(int index) const;
    LuaLayer *layer(const char *name);
    LuaTileLayer *tileLayer(const char *name);

    LuaTileLayer *newTileLayer(const char *name);

    void insertLayer(int index, LuaLayer *layer);
    void removeLayer(int index);

    Tile *tile(const char *name);
    Tile *tile(const char *tilesetName, int tileID);

    Tileset *_tileset(const QString &name);
    Tileset *tileset(const char *name);

    LuaMapBmp &bmp(int index);
    LuaBmpRule *rule(const char *name);

    Map *mClone;
    Map *mOrig;
    QMap<QString,Tileset*> mTilesetByName;
    QList<LuaLayer*> mLayers;
    QList<LuaLayer*> mRemovedLayers;
    QMap<QString,LuaLayer*> mLayerByName;
    QList<Tileset*> mTilesets;
    QRegion mSelection;
    LuaMapBmp mBmpMain;
    LuaMapBmp mBmpVeg;
    QMap<QString,LuaBmpRule> mRules;
};

class LuaScript
{
public:
    LuaScript(Map *map);
    ~LuaScript();

    lua_State *init();
    bool dofile(const QString &f, QString &output);

    lua_State *L;
    LuaMap mMap;
};

extern LuaColor Lua_rgb(int r, int g, int b);
extern const char *cstring(const QString &qstring);

} // namespace Lua
} // namespace Tiled

#endif // LUATILED_H
