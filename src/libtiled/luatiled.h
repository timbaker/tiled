#ifndef LUATILED_H
#define LUATILED_H

#include "tiled_global.h"

#include <QList>
#include <QMap>
#include <QRegion>

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

class LuaMap;
class LuaTileLayer;

class TILEDSHARED_EXPORT LuaLayer
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

class TILEDSHARED_EXPORT LuaTileLayer : public LuaLayer
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

class TILEDSHARED_EXPORT LuaMap
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


    Map *mOrig;
    int mWidth;
    int mHeight;
    QMap<QString,Tileset*> mTilesetByName;
    QList<LuaLayer*> mLayers;
    QList<LuaLayer*> mRemovedLayers;
    QMap<QString,LuaLayer*> mLayerByName;
    QList<Tileset*> mTilesets;
    QRegion mSelection;
};

class TILEDSHARED_EXPORT LuaScript
{
public:
    LuaScript(Map *map);
    ~LuaScript();

    lua_State *init();
    bool dofile(const QString &f, QString &output);

    lua_State *L;
    LuaMap mMap;
};

} // namespace Lua
} // namespace Tiled

#endif // LUATILED_H
