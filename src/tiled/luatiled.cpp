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

#include "luatiled.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include "tolua.h"

#include <QHash>
#include <QString>
#include <QTextStream>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

using namespace Tiled;
using namespace Tiled::Lua;

TOLUA_API int tolua_tiled_open(lua_State *L);

static const char *cstring(const QString &qstring)
{
    static QHash<QString,const char*> StringHash;
    if (!StringHash.contains(qstring)) {
        QByteArray b = qstring.toLatin1();
        char *s = new char[b.size() + 1];
        memcpy(s, (void*)b.data(), b.size() + 1);
        StringHash[qstring] = s;
    }
    return StringHash[qstring];
}

/////

/* function to release collected object via destructor */
static int tolua_collect_QRect(lua_State* tolua_S)
{
    QRect* self = (QRect*) tolua_tousertype(tolua_S,1,0);
    tolua_release(tolua_S,self);
    delete self;
    return 0;
}

/* method: rects of class QRegion */
static int tolua_tiled_Region_rects00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
    tolua_Error tolua_err;
    if (
            !tolua_isusertype(tolua_S,1,"QRegion",0,&tolua_err) ||
            !tolua_isnoobj(tolua_S,2,&tolua_err)
            )
        goto tolua_lerror;
    else
#endif
    {
        QRegion* self = (QRegion*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
        if (!self) tolua_error(tolua_S,"invalid 'self' in function 'rects'",NULL);
#endif
        {
            lua_newtable(tolua_S);
            for (int i = 0; i < self->rectCount(); i++) {
                void* tolua_obj = new QRect(self->rects()[i]);
                void* v = tolua_clone(tolua_S,tolua_obj,tolua_collect_QRect);
                tolua_pushfieldusertype(tolua_S,2,i+1,v,"QRect");
            }
        }
        return 1;
    }
    return 0;
#ifndef TOLUA_RELEASE
tolua_lerror:
    tolua_error(tolua_S,"#ferror in function 'rects'.",&tolua_err);
    return 0;
#endif
}

/* method: tiles of class LuaBmpRule */
static int tolua_tiled_BmpRule_tiles00(lua_State* tolua_S)
{
#ifndef TOLUA_RELEASE
    tolua_Error tolua_err;
    if (
            !tolua_isusertype(tolua_S,1,"LuaBmpRule",0,&tolua_err) ||
            !tolua_isnoobj(tolua_S,2,&tolua_err)
            )
        goto tolua_lerror;
    else
#endif
    {
        LuaBmpRule* self = (LuaBmpRule*)  tolua_tousertype(tolua_S,1,0);
#ifndef TOLUA_RELEASE
        if (!self) tolua_error(tolua_S,"invalid 'self' in function 'tiles'",NULL);
#endif
        {
            lua_newtable(tolua_S);
            for (int i = 0; i < self->mRule->tileChoices.size(); i++) {
                tolua_pushfieldstring(tolua_S,2,i+1,cstring(self->mRule->tileChoices[i]));
            }
        }
        return 1;
    }
    return 0;
#ifndef TOLUA_RELEASE
tolua_lerror:
    tolua_error(tolua_S,"#ferror in function 'tiles'.",&tolua_err);
    return 0;
#endif
}

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
    tolua_tiled_open(L);

    tolua_beginmodule(L,NULL);
    tolua_beginmodule(L,"Region");
    tolua_function(L,"rects",tolua_tiled_Region_rects00);
    tolua_endmodule(L);
    tolua_beginmodule(L,"BmpRule");
    tolua_function(L,"tiles",tolua_tiled_BmpRule_tiles00);
    tolua_endmodule(L);
    tolua_endmodule(L);

    return L;
}

bool LuaScript::dofile(const QString &f, QString &output)
{
    lua_State *L = init();

    tolua_pushusertype(L, &mMap, "LuaMap");
    lua_setglobal(L, "TheMap");

    bool fail = luaL_dofile(L, f.toLatin1().data());
    output = QString::fromLatin1(lua_tostring(L, -1));
    return !fail;
}

/////

LuaLayer::LuaLayer() :
    mClone(0),
    mOrig(0)
{
}

LuaLayer::LuaLayer(Layer *orig) :
    mClone(0),
    mOrig(orig),
    mName(orig->name())
{
}

LuaLayer::~LuaLayer()
{
    delete mClone;
}

const char *LuaLayer::name()
{
    return cstring(mName);
}

void LuaLayer::initClone()
{
    // A script-created layer will have mOrig == 0
    Q_ASSERT(mOrig || mClone);

    if (!mClone && mOrig) {
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
    mCloneTileLayer(0),
    mMap(0)
{
}

LuaTileLayer::LuaTileLayer(const char *name, int x, int y, int width, int height) :
    LuaLayer(),
    mCloneTileLayer(new TileLayer(QString::fromLatin1(name), x, y, width, height)),
    mMap(0)
{
    mName = mCloneTileLayer->name();
    mClone = mCloneTileLayer;
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
    // Forbid changing tiles outside the current tile selection.
    // See the PaintTileLayer undo command.
    if (mMap && !mMap->mSelection.isEmpty() && !mMap->mSelection.contains(QPoint(x, y)))
        return;

    initClone();
    if (!mCloneTileLayer->contains(x, y))
        return; // TODO: lua error!
    mCloneTileLayer->setCell(x, y, Cell(tile));
    mAltered += QRect(x, y, 1, 1); // too slow?
}

Tile *LuaTileLayer::tileAt(int x, int y)
{
    if (mClone) {
        if (!mCloneTileLayer->contains(x, y))
            return 0; // TODO: lua error!
        return mCloneTileLayer->cellAt(x, y).tile;
    }
    Q_ASSERT(mOrig);
    if (!mOrig)
        return 0; // this layer was created by the script
    if (!mOrig->asTileLayer()->contains(x, y))
        return 0; // TODO: lua error!
    return mOrig->asTileLayer()->cellAt(x, y).tile;
}

void LuaTileLayer::replaceTile(Tile *oldTile, Tile *newTile)
{
    initClone();
    for (int y = 0; y < mClone->width(); y++) {
        for (int x = 0; x < mClone->width(); x++) {
            if (mCloneTileLayer->cellAt(x, y).tile == oldTile) {
                mCloneTileLayer->setCell(x, y, Cell(newTile));
                mAltered += QRect(x, y, 1, 1);
            }
        }
    }
}

/////

LuaMap::LuaMap(Map *orig) :
    mClone(new Map(orig->orientation(), orig->width(), orig->height(),
                   orig->tileWidth(), orig->tileHeight())),
    mOrig(orig),
    mBmpMain(mClone->rbmpMain()),
    mBmpVeg(mClone->rbmpVeg())
{
    foreach (Layer *layer, orig->layers()) {
        if (layer->asTileLayer())
            mLayers += new LuaTileLayer(layer->asTileLayer());
        else
            mLayers += new LuaLayer(layer);
        mLayerByName[layer->name()] = mLayers.last(); // could be duplicates & empty names
    }

    mClone->rbmpSettings()->clone(*mOrig->bmpSettings());
    foreach (BmpRule *rule, mClone->bmpSettings()->rules()) {
        if (!rule->label.isEmpty())
            mRules[rule->label] = LuaBmpRule(rule);
    }
}

LuaMap::~LuaMap()
{
    qDeleteAll(mLayers);
    delete mClone;
}

int LuaMap::width() const
{
    return mClone->width();
}

int LuaMap::height() const
{
    return mClone->height();
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

LuaTileLayer *LuaMap::newTileLayer(const char *name)
{
    LuaTileLayer *tl = new LuaTileLayer(name, 0, 0, width(), height());
    return tl;
}

void LuaMap::insertLayer(int index, LuaLayer *layer)
{
    index = qBound(0, index, mLayers.size());
    mLayers.insert(index, layer);

    mLayerByName.clear(); // FIXME: make more efficient
    foreach (LuaLayer *ll, mLayers)
        mLayerByName[ll->mName] = ll;
}

void LuaMap::removeLayer(int index)
{
    if (index < 0 || index >= mLayers.size())
        return; // error!
    LuaLayer *layer = mLayers.takeAt(index);
    mRemovedLayers += layer;

    mLayerByName.clear(); // FIXME: make more efficient
    foreach (LuaLayer *ll, mLayers)
        mLayerByName[layer->mName] = ll;
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

LuaMapBmp &LuaMap::bmp(int index)
{
    return index ? mBmpVeg : mBmpMain;
}

LuaBmpRule *LuaMap::rule(const char *name)
{
    QString qname(QString::fromLatin1(name));

    if (mRules.contains(qname))
        return &mRules[qname];
    return 0;
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

/////

LuaMapBmp::LuaMapBmp(MapBmp &bmp) :
    mBmp(bmp)
{
}

bool LuaMapBmp::contains(int x, int y)
{
    return QRect(0, 0, mBmp.width(), mBmp.height()).contains(x, y);
}

void LuaMapBmp::setPixel(int x, int y, LuaColor &c)
{
    if (!contains(x, y)) return; // error!
    mBmp.setPixel(x, y, c.pixel);
    mAltered += QRect(x, y, 1, 1);
}

LuaColor LuaMapBmp::pixel(int x, int y)
{
    if (!contains(x, y)) return LuaColor(); // error!
    return mBmp.pixel(x, y);
}

void LuaMapBmp::erase(int x, int y, int width, int height)
{
    fill(x, y, width, height, LuaColor());
}

void LuaMapBmp::erase(QRect &r)
{
    fill(r, LuaColor());
}

void LuaMapBmp::erase(QRegion &rgn)
{
    fill(rgn, LuaColor());
}

void LuaMapBmp::erase()
{
    fill(LuaColor());
}

void LuaMapBmp::fill(int x, int y, int width, int height, LuaColor &c)
{
    fill(QRect(x, y, width, height), c);
}

void LuaMapBmp::fill(QRect &r, LuaColor &c)
{
    r &= QRect(0, 0, mBmp.width(), mBmp.height());

    for (int y = r.y(); y <= r.bottom(); y++) {
        for (int x = r.x(); x <= r.right(); x++) {
            mBmp.setPixel(x, y, c.pixel);
        }
    }

    mAltered += r;
}

void LuaMapBmp::fill(QRegion &rgn, LuaColor &c)
{
    foreach (QRect r, rgn.rects())
        fill(r, c);
}

void LuaMapBmp::fill(LuaColor &c)
{
    fill(QRect(0, 0, mBmp.width(), mBmp.height()), c);
}

void LuaMapBmp::replace(LuaColor &oldColor, LuaColor &newColor)
{
    for (int y = 0; y < mBmp.height(); y++) {
        for (int x = 0; x < mBmp.width(); x++) {
            if (mBmp.pixel(x, y) == oldColor.pixel)
                mBmp.setPixel(x, y, newColor.pixel);
        }
    }
}

/////

LuaColor Lua::Lua_rgb(int r, int g, int b)
{
    return LuaColor(r, g, b);
}

/////

const char *LuaBmpRule::label()
{
    return cstring(mRule->label);
}

int LuaBmpRule::bmpIndex()
{
    return mRule->bitmapIndex;
}

LuaColor LuaBmpRule::color()
{
    return mRule->color;
}

const char *LuaBmpRule::layer()
{
    return cstring(mRule->targetLayer);
}

LuaColor LuaBmpRule::condition()
{
    return mRule->condition;
}
