#ifndef LUATILED_H
#define LUATILED_H

#include "tiled_global.h"

#include <QList>
#include <QMap>

extern "C" {
struct lua_State;
}

namespace Tiled {
class Layer;
class Map;
class Tile;
class TileLayer;
class Tileset;

namespace Lua {

class LuaTileLayer;

class LuaLayer
{
public:
    LuaLayer(Layer *orig);
    virtual ~LuaLayer();

    const char *name();

    LuaTileLayer *asTileLayer();

    const char *type() const;

    void initClone();
    virtual void cloned();

    Layer *mClone;
    Layer *mOrig;
};

class LuaTileLayer : public LuaLayer
{
public:
    LuaTileLayer(TileLayer *orig);
    ~LuaTileLayer();

    void cloned();

    void setTile(int x, int y, Tile *tile);
    Tile *tileAt(int x, int y);

    TileLayer *mCloneTileLayer;
};

class LuaMap
{
public:
    LuaMap(Map *orig);
    ~LuaMap();

    int width() const;
    int height() const;

    int layerCount() const;
    LuaLayer *layerAt(int index) const;
    LuaLayer *layer(const char *name);
    LuaTileLayer *tileLayer(const char *name);

    Tile *tile(const char *name);
    Tile *tile(const char *tilesetName, int tileID);

    Tileset *_tileset(const QString &name);
    Tileset *tileset(const char *name);

    Map *mOrig;
    QMap<QString,Tileset*> mTilesetByName;
    QList<LuaLayer*> mLayers;
    QMap<QString,LuaLayer*> mLayerByName;
    QList<Tileset*> mTilesets;
};

class TILEDSHARED_EXPORT  LuaScript
{
public:
    LuaScript(Map *map);
    ~LuaScript();

    lua_State *init();
    QString dofile(const QString &f);

    lua_State *L;
    Map *mMap;
};

} // namespace Lua
} // namespace Tiled

#endif // LUATILED_H
