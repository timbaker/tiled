#include "luatiled.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include "tolua.h"

#include <QString>
#include <QTextStream>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

using namespace Tiled;
using namespace Tiled::Lua;

TOLUA_API int tolua_libtiled_open(lua_State *L);

/////

LuaScript::LuaScript(Map *map) :
    mMap(map),
    L(0)
{
}

LuaScript::~LuaScript()
{
    if (L)
        lua_close(L);
}

lua_State *LuaScript::init()
{
    L = luaL_newstate();
    luaL_openlibs(L);
    tolua_libtiled_open(L);
    return L;
}

QString LuaScript::dofile(const QString &f)
{
    QString ret;
    QTextStream ts(&ret);

    lua_State *L = init();

    LuaMap map(mMap);
    tolua_pushusertype(L, &map, "LuaMap");
    lua_setglobal(L, "TheMap");

    if (luaL_dofile(L, f.toLatin1().data()))
        ts << "LUA Error: " << lua_tostring(L, -1);
    else {
        ts << "LUA OK: " << lua_tostring(L, -1);
    }

    return ret;
}

/////

LuaLayer::LuaLayer(Layer *orig) :
    mClone(0),
    mOrig(orig)
{
}

LuaLayer::~LuaLayer()
{
    delete mClone;
}

const char *LuaLayer::name()
{
    static QByteArray ba;
    ba = mOrig->name().toLatin1();
    return ba.data(); // think this is ok
}

LuaTileLayer *LuaLayer::asTileLayer()
{
    return dynamic_cast<LuaTileLayer*>(this);
}

const char *LuaLayer::type() const
{
    if (mOrig->isTileLayer())
        return "tile";
    if (mOrig->isObjectGroup())
        return "object";
    if (mOrig->isImageLayer())
        return "image";
    return "unknown";
}

void LuaLayer::initClone()
{
    if (!mClone) {
        mClone = mOrig->clone();
        cloned();
    }
}

void LuaLayer::cloned()
{

}

/////

LuaTileLayer::LuaTileLayer(TileLayer *orig) :
    LuaLayer(orig),
    mCloneTileLayer(0)
{
}

LuaTileLayer::~LuaTileLayer()
{
}

void LuaTileLayer::cloned()
{
    LuaLayer::cloned();
    mCloneTileLayer = mClone->asTileLayer();
}

void LuaTileLayer::setTile(int x, int y, Tile *tile)
{
    initClone();
    if (!mCloneTileLayer->contains(x, y)) return; // TODO: lua error!
    mCloneTileLayer->setCell(x, y, Cell(tile));
}

Tile *LuaTileLayer::tileAt(int x, int y)
{
    initClone();
    if (!mCloneTileLayer->contains(x, y)) return 0; // TODO: lua error!
    return mCloneTileLayer->cellAt(x, y).tile;
}

/////

LuaMap::LuaMap(Map *orig) :
    mOrig(orig)
{
    foreach (Layer *layer, orig->layers()) {
        if (layer->asTileLayer())
            mLayers += new LuaTileLayer(layer->asTileLayer());
        else
            mLayers += new LuaLayer(layer);
        mLayerByName[layer->name()] = mLayers.last(); // could be duplicates & empty names
    }
}

LuaMap::~LuaMap()
{
    qDeleteAll(mLayers);
}

int LuaMap::width() const
{
    return mOrig->width(); // FIXME: let lua resize it
}

int LuaMap::height() const
{
    return mOrig->height(); // FIXME: let lua resize it
}

int LuaMap::layerCount() const
{
    return mLayers.size();
}

LuaLayer *LuaMap::layerAt(int index) const
{
    if (index < 0 || index >= mLayers.size())
        return 0; // TODO: lua error
    return mLayers.at(index);
}

LuaLayer *LuaMap::layer(const char *name)
{
    QString _name = QString::fromLatin1(name);
    if (mLayerByName.contains(_name))
        return mLayerByName[_name];
    return 0;
}

LuaTileLayer *LuaMap::tileLayer(const char *name)
{
    if (LuaLayer *layer = this->layer(name))
        return layer->asTileLayer();
    return 0;
}

static bool parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    int n = tileName.lastIndexOf(QLatin1Char('_'));
    if (n == -1)
        return false;
    tilesetName = tileName.mid(0, n);
    QString indexString = tileName.mid(n + 1);

    // Strip leading zeroes from the tile index
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);

    bool ok;
    index = indexString.toUInt(&ok);
    return !tilesetName.isEmpty() && ok;
}

Tile *LuaMap::tile(const char *name)
{
    QString tilesetName;
    int tileID;
    if (!parseTileName(QString::fromLatin1(name), tilesetName, tileID))
        return 0;

    if (Tileset *ts = _tileset(tilesetName))
        return ts->tileAt(tileID);
    return 0;
}

Tile *LuaMap::tile(const char *tilesetName, int tileID)
{
    if (Tileset *ts = tileset(tilesetName))
        return ts->tileAt(tileID);
    return 0;
}

Tileset *LuaMap::tileset(const char *name)
{
    return _tileset(QString::fromLatin1(name));
}

Tileset *LuaMap::_tileset(const QString &name)
{
    if (mTilesetByName.isEmpty()) {
        foreach (Tileset *ts, mOrig->tilesets())
            mTilesetByName[ts->name()] = ts;
    }
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}


