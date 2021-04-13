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

#include "bmpblender.h"
#include "luaconsole.h"
#include "mapcomposite.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "tmxmapwriter.h"

#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "maplevel.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"
#include "tilelayer.h"

#include "tolua.h"

#include <QElapsedTimer>
#include <QFileInfo>
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

const char *Lua::cstring(const QString &qstring)
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

#if 0
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
#endif

/////

LuaScript::LuaScript(Map *map, int cellX, int cellY) :
    L(0),
    mMap(map, cellX, cellY)
{
}

LuaScript::LuaScript(Map *map) :
    L(0),
    mMap(map)
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
#if 0
    tolua_beginmodule(L,"Region");
    tolua_function(L,"rects",tolua_tiled_Region_rects00);
    tolua_endmodule(L);
    tolua_beginmodule(L,"BmpRule");
    tolua_function(L,"tiles",tolua_tiled_BmpRule_tiles00);
    tolua_endmodule(L);
#endif
    tolua_endmodule(L);

    return L;
}

extern "C" {

// see luaconf.h
// these are where print() calls go
void luai_writestring(const char *s, int len)
{
    LuaConsole::instance()->writestring(s, len);
}

void luai_writeline()
{
    LuaConsole::instance()->writeline();
}

int traceback(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
    if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
      lua_pushliteral(L, "(no error message)");
  }
  return 1;
}
}

bool LuaScript::dofile(const QString &f, QString &output)
{
    lua_State *L = init();

    QElapsedTimer elapsed;
    elapsed.start();

    tolua_pushusertype(L, &mMap, "LuaMap");
    lua_setglobal(L, "map");

    tolua_pushstring(L, Lua::cstring(QFileInfo(f).absolutePath()));
    lua_setglobal(L, "scriptDirectory");

    int status = luaL_loadfile(L, cstring(f));
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);
    }
    if (status != LUA_OK) {
        output = QString::fromLatin1(lua_tostring(L, -1));
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
    LuaConsole::instance()->write(qApp->tr("---------- script completed in %1s ----------")
                                  .arg(elapsed.elapsed()/1000.0));
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return status == LUA_OK;
}

/////

LuaLayer::LuaLayer() :
    mLevelIndex(0),
    mClone(nullptr),
    mOrig(nullptr)
{
}

LuaLayer::LuaLayer(Layer *orig) :
    mLevelIndex(orig->level()),
    mClone(nullptr),
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
        mLevelIndex = mClone->level();
        cloned();
    }
}

void LuaLayer::cloned()
{

}

void LuaLayer::setLevel(int level)
{
    mLevelIndex = level;
    if (mClone != nullptr) {
        mClone->setLevel(level);
    }
}

int LuaLayer::level() const
{
    return mLevelIndex;
//    int level;
//    MapComposite::levelForLayer(mName, &level);
//    return level;
}

/////

LuaTileLayer::LuaTileLayer(TileLayer *orig) :
    LuaLayer(orig),
    mCloneTileLayer(nullptr),
    mMap(nullptr)
{
}

LuaTileLayer::LuaTileLayer(const char *name, int x, int y, int width, int height) :
    LuaLayer(),
    mCloneTileLayer(new TileLayer(QString::fromLatin1(name), x, y, width, height)),
    mMap(nullptr)
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
    if (tile == LuaMap::noneTile()) tile = nullptr;
    mCloneTileLayer->setCell(x, y, Cell(tile));
    mAltered += QRect(x, y, 1, 1); // too slow?
}

Tile *LuaTileLayer::tileAt(int x, int y)
{
    if (mClone) {
        if (!mCloneTileLayer->contains(x, y))
            return nullptr; // TODO: lua error!
        return mCloneTileLayer->cellAt(x, y).tile;
    }
    Q_ASSERT(mOrig);
    if (!mOrig)
        return nullptr; // this layer was created by the script
    if (!mOrig->asTileLayer()->contains(x, y))
        return nullptr; // TODO: lua error!
    return mOrig->asTileLayer()->cellAt(x, y).tile;
}

void LuaTileLayer::clearTile(int x, int y)
{
    setTile(x, y, nullptr);
}

void LuaTileLayer::erase(int x, int y, int width, int height)
{
    fill(QRect(x, y, width, height), nullptr);
}

void LuaTileLayer::erase(const QRect &r)
{
    fill(r, nullptr);
}

void LuaTileLayer::erase(const LuaRegion &rgn)
{
    fill(rgn, nullptr);
}

void LuaTileLayer::erase()
{
    fill(nullptr);
}

void LuaTileLayer::fill(int x, int y, int width, int height, Tile *tile)
{
    fill(QRect(x, y, width, height), tile);
}

void LuaTileLayer::fill(const QRect &r, Tile *tile)
{
    if (tile == LuaMap::noneTile()) tile = nullptr;
    initClone();
    QRect r2 = r & mClone->bounds();
    for (int y = r2.y(); y <= r2.bottom(); y++) {
        for (int x = r2.x(); x <= r2.right(); x++) {
            mCloneTileLayer->setCell(x, y, Cell(tile));
        }
    }
    mAltered += r2;
}

void LuaTileLayer::fill(const LuaRegion &rgn, Tile *tile)
{
    for (const QRect &r : rgn) {
        fill(r, tile);
    }
}

void LuaTileLayer::fill(Tile *tile)
{
    fill(mClone ? mClone->bounds() : mOrig->bounds(), tile);
}

bool LuaTileLayer::replaceTile(Tile *oldTile, Tile *newTile)
{
    if (newTile == LuaMap::noneTile()) newTile = nullptr;
    initClone();
    bool replaced = false;
    for (int y = 0; y < mClone->width(); y++) {
        for (int x = 0; x < mClone->width(); x++) {
            if (mCloneTileLayer->cellAt(x, y).tile == oldTile) {
                mCloneTileLayer->setCell(x, y, Cell(newTile));
                mAltered += QRect(x, y, 1, 1);
                replaced = true;
            }
        }
    }
    return replaced;
}

bool LuaTileLayer::replaceTiles(const QList<Tile *> &tiles)
{
    if (tiles.size() % 2)
        return false;
    initClone();
    bool replaced = false;
    for (int y = 0; y < mClone->width(); y++) {
        for (int x = 0; x < mClone->width(); x++) {
            for (int i = 0; i < tiles.size(); i += 2) {
                Tile *oldTile = tiles[i];
                Tile *newTile = tiles[i + 1];
                if (newTile == LuaMap::noneTile())
                    newTile = 0;
                if (mCloneTileLayer->cellAt(x, y).tile == oldTile) {
                    mCloneTileLayer->setCell(x, y, Cell(newTile));
                    mAltered += QRect(x, y, 1, 1);
                    replaced = true;
                    break;
                }
            }
        }
    }
    return replaced;
}

/////

LuaMapLevel::LuaMapLevel(MapLevel *clone) :
    mClone(clone)
{

}

LuaMapLevel::~LuaMapLevel()
{
    // Remove all layers from the clone map.
    // Either they are the original unmodified layers or they are clones.
    // Original layers aren't to be deleted, clones delete themselves.
    for (int i = mClone->layerCount() - 1; i >= 0; i--) {
        mClone->takeLayerAt(i);
    }

    qDeleteAll(mLayers);
}

int LuaMapLevel::z() const
{
    return mClone->z();
}

int LuaMapLevel::layerCount() const
{
    return mLayers.size();
}

LuaLayer *LuaMapLevel::layerAt(int index) const
{
    if (index < 0 || index >= mLayers.size()) {
        return nullptr;
    }
    return mLayers.at(index);
}

void LuaMapLevel::addLayer(LuaLayer *layer)
{
    Q_UNUSED(layer);
}

void LuaMapLevel::insertLayer(int index, LuaLayer *layer)
{
    Q_UNUSED(index);
    Q_UNUSED(layer);
}

LuaLayer *LuaMapLevel::takeLayerAt(int index)
{
    Q_UNUSED(index);
    return nullptr;
}

/////

LuaMap::LuaMap(Map *orig, int cellX, int cellY) :
    mClone(new Map(orig->orientation(), orig->width(), orig->height(),
                   orig->tileWidth(), orig->tileHeight())),
    mOrig(orig),
    mBmpMain(mClone->rbmpMain()),
    mBmpVeg(mClone->rbmpVeg()),
    mRulesChanged(false),
    mBlendsChanged(false),
    mCellX(cellX),
    mCellY(cellY)
{
    mLevels.resize(orig->levelCount());
    for (int z = 0; z < orig->levelCount(); z++) {
        MapLevel *origLevel = orig->levelAt(z);
        LuaMapLevel *cloneLevel = new LuaMapLevel(mClone->levelAt(z));
        for (Layer *origLayer : origLevel->layers()) {
            if (TileLayer *tl = origLayer->asTileLayer()) {
                cloneLevel->mLayers += new LuaTileLayer(tl);
            } else if (ObjectGroup *og = origLayer->asObjectGroup()) {
                cloneLevel->mLayers+= new LuaObjectGroup(og);
            } else {
                cloneLevel->mLayers += new LuaLayer(origLayer);
            }
            cloneLevel->mLayerByName[origLayer->name()] = cloneLevel->mLayers.last(); // could be duplicates & empty names
//            mClone->addLayer(layer);
        }
        mLevels[z] = cloneLevel;
    }

    mClone->rbmpSettings()->clone(*mOrig->bmpSettings());
    mBmpMain.mBmp = orig->bmpMain();
    mBmpVeg.mBmp = orig->bmpVeg();

    foreach (BmpAlias *alias, mClone->bmpSettings()->aliases()) {
        mAliases += new LuaBmpAlias(alias);
        mAliasByName[alias->name] = mAliases.last();
    }
    foreach (BmpRule *rule, mClone->bmpSettings()->rules()) {
        mRules += new LuaBmpRule(rule);
        if (!rule->label.isEmpty())
            mRuleByName[rule->label] = mRules.last();
    }
    foreach (BmpBlend *blend, mClone->bmpSettings()->blends()) {
        mBlends += new LuaBmpBlend(blend);
    }

    foreach (Tileset *ts, mOrig->tilesets())
        addTileset(ts);
}

LuaMap::LuaMap(Map *orig) :
    LuaMap(orig, -1, -1)
{
}

LuaMap::LuaMap(LuaMap::Orientation orient, int width, int height, int tileWidth, int tileHeight) :
    mClone(new Map((Map::Orientation)orient, width, height, tileWidth, tileHeight)),
    mOrig(0),
    mBmpMain(mClone->rbmpMain()),
    mBmpVeg(mClone->rbmpVeg()),
    mCellX(-1),
    mCellY(-1)
{
    mLevels.resize(mClone->levelCount());
    for (int z = 0; z < mLevels.size(); z++) {
        mLevels[z] = new LuaMapLevel(mClone->levelAt(z));
    }
}

LuaMap::~LuaMap()
{
    qDeleteAll(mAliases);
    qDeleteAll(mRules);
    qDeleteAll(mBlends);
    qDeleteAll(mLevels);
    qDeleteAll(mRemovedLayers);
    delete mClone;

    Tiled::Internal::TilesetManager::instance()->removeReferences(mNewTilesets);
}

LuaMap::Orientation LuaMap::orientation()
{
    return (Orientation) mClone->orientation();
}

int LuaMap::width() const
{
    return mClone->width();
}

int LuaMap::height() const
{
    return mClone->height();
}

int LuaMap::maxLevel() const
{
    return mClone->levelCount();
}

int LuaMap::levelCount() const
{
    return mClone->levelCount();
}

LuaMapLevel *LuaMap::levelAt(int z) const
{
    if (z < 0 || z >= levelCount()) {
        return nullptr;
    }
    return mLevels[z];
}

int LuaMap::layerCount(int z) const
{
    if (z < 0 || z >= levelCount()) {
        return 0;
    }
    return mLevels[z]->layerCount();
}

LuaLayer *LuaMap::layerAt(int z, int index) const
{
    if (z < 0 || z >= levelCount()) {
        return 0;
    }
    return mLevels[z]->layerAt(index);
}

LuaLayer *LuaMap::layer(const char *name)
{
    QString _name = QString::fromLatin1(name);
    int level = 0;
    if (MapComposite::levelForLayer(_name, &level)) {
        if (LuaMapLevel *mapLevel = levelAt(level)) {
            if (mapLevel->mLayerByName.contains(_name)) {
                return mapLevel->mLayerByName[_name];
            }
        }
    }
    return nullptr;
}

LuaTileLayer *LuaMap::tileLayer(const char *name)
{
    if (LuaLayer *layer = this->layer(name))
        return layer->asTileLayer();
    return nullptr;
}

LuaTileLayer *LuaMap::newTileLayer(const char *name)
{
    QString name2 = QString::fromLatin1(name);
    int level = 0;
    if (MapComposite::levelForLayer(name2, &level)) {
        name2 = MapComposite::layerNameWithoutPrefix(name2);
        LuaTileLayer *tl = new LuaTileLayer(cstring(name2), 0, 0, width(), height());
        tl->setLevel(level);
        return tl;
    }
    LuaTileLayer *tl = new LuaTileLayer(name, 0, 0, width(), height());
    return tl;
}

void LuaMap::addLayer(LuaLayer *layer)
{
    if (layer == nullptr) {
        return;
    }
    LuaMapLevel *mapLevel = levelAt(layer->level());
    if (mapLevel == nullptr) {
        return;
    }
    if (mapLevel->mLayers.contains(layer)) {
        return; // error!
    }

    if (mRemovedLayers.contains(layer)) {
        mRemovedLayers.removeAll(layer);
    }

    mapLevel->mLayers += layer;
//    mClone->addLayer(layer->mClone ? layer->mClone : layer->mOrig);

    mapLevel->mLayerByName.clear(); // FIXME: make more efficient
    for (LuaLayer *ll : qAsConst(mapLevel->mLayers)) {
        mapLevel->mLayerByName[ll->mName] = ll;
    }
}

void LuaMap::insertLayer(int index, LuaLayer *layer)
{
    if (layer == nullptr) {
        return;
    }
    LuaMapLevel *mapLevel = levelAt(layer->level());
    if (mapLevel == nullptr) {
        return;
    }
    if (mapLevel->mLayers.contains(layer)) {
        return; // error!
    }

    if (mRemovedLayers.contains(layer)) {
        mRemovedLayers.removeAll(layer);
    }

    index = qBound(0, index, mapLevel->mLayers.size());
    mapLevel->mLayers.insert(index, layer);
//    mClone->insertLayer(index, layer->mClone ? layer->mClone : layer->mOrig);

    mapLevel->mLayerByName.clear(); // FIXME: make more efficient
    for (LuaLayer *ll : qAsConst(mapLevel->mLayers)) {
        mapLevel->mLayerByName[ll->mName] = ll;
    }
}

void LuaMap::removeLayer(int z, int index)
{
    LuaMapLevel *mapLevel = levelAt(z);
    if (mapLevel == nullptr) {
        return;
    }
    if (index < 0 || index >= mapLevel->mLayers.size()) {
        return; // error!
    }
    LuaLayer *layer = mapLevel->mLayers.takeAt(index);
    mRemovedLayers += layer;
//    mClone->takeLayerAt(index);

    mapLevel->mLayerByName.clear(); // FIXME: make more efficient
    for (LuaLayer *ll : qAsConst(mapLevel->mLayers)) {
        mapLevel->mLayerByName[ll->mName] = ll;
    }
}

int LuaMap::levelForLayerName(const char *name) const
{
    int level = 0;
    if (MapComposite::levelForLayer(QString::fromLatin1(name), &level)) {
        return level;
    }
    return -1;
}

const char *LuaMap::layerNameWithoutPrefix(const char *name) const
{
    QString name2 = MapComposite::layerNameWithoutPrefix(QLatin1String(name));
    return cstring(name2);
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

Tile *LuaMap::noneTile()
{
    return BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
}

void LuaMap::addTileset(Tileset *tileset)
{
    if (mClone->tilesets().contains(tileset))
        return; // error!

    mClone->addTileset(tileset);
    mTilesetByName[tileset->name()] = tileset;
}

int LuaMap::tilesetCount()
{
    return mClone->tilesets().size();
}

Tileset *LuaMap::_tileset(const QString &name)
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

Tileset *LuaMap::tileset(const char *name)
{
    return _tileset(QString::fromLatin1(name));
}

Tileset *LuaMap::tilesetAt(int index)
{
    if (index >= 0 && index < mClone->tilesets().size())
        return mClone->tilesets()[index];
    return 0;
}

void LuaMap::replaceTilesByName(const char *names)
{
    QString ss = QString::fromLatin1(names);
    QStringList _names = ss.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (_names.size() % 2)
        return; // error

    QList<Tileset*> addTilesets;
    QList<Tile*> tiles;
    for (int i = 0; i < _names.size(); i += 2) {
        TileName from(_names[i]);
        TileName to(_names[i+1]);
        if (!from.isValid() || !to.isValid())
            goto errorExit;
        Tileset *tsFrom = _tileset(from.tileset());
        if (tsFrom == 0) {
            for (int j = 0; j < addTilesets.size(); j++) {
                if (addTilesets[j]->name() == from.tileset()) {
                    tsFrom = addTilesets[j];
                    break;
                }
            }
        }
        if (tsFrom == 0) {
            if (Tileset *ts = Tiled::Internal::TileMetaInfoMgr::instance()->tileset(from.tileset())) {
                Tiled::Internal::TileMetaInfoMgr::instance()->loadTilesets(QList<Tileset*>() << ts);
                tsFrom = ts->clone();
            }
            if (tsFrom == 0) {
                goto errorExit;
            }
            addTilesets += tsFrom;
        }
        Tileset *tsTo = _tileset(to.tileset());
        if (tsTo == 0) {
            for (int j = 0; j < addTilesets.size(); j++) {
                if (addTilesets[j]->name() == to.tileset()) {
                    tsTo = addTilesets[j];
                    break;
                }
            }
        }
        if (tsTo == 0) {
            if (Tileset *ts = Tiled::Internal::TileMetaInfoMgr::instance()->tileset(to.tileset())) {
                Tiled::Internal::TileMetaInfoMgr::instance()->loadTilesets(QList<Tileset*>() << ts);
                tsTo = ts->clone();
            }
            if (tsTo == 0)
                goto errorExit;
            addTilesets += tsTo;
        }
        Tile *tileFrom = tsFrom->tileAt(from.id());
        if (tileFrom == 0)
            goto errorExit;
        Tile *tileTo = tsTo->tileAt(to.id());
        if (tileTo == 0)
            goto errorExit;
        tiles += tileFrom;
        tiles += tileTo;
    }
    {
        bool replaced = false;
        for (LuaMapLevel *mapLevel : qAsConst(mLevels)) {
            for (int layerIndex = 0; layerIndex < mapLevel->layerCount(); layerIndex++) {
                LuaLayer *layer = mapLevel->layerAt(layerIndex);
                if (LuaTileLayer *tl = layer->asTileLayer()) {
                    if (tl->replaceTiles(tiles)) {
                        replaced = true;
                    }
                }
            }
        }

        for (Tileset *ts : qAsConst(addTilesets)) {
            if (replaced) {
                addTileset(ts);
                Tiled::Internal::TilesetManager::instance()->addReference(ts);
                mNewTilesets += ts;
            } else {
                delete ts;
            }
        }
        return;
    }
errorExit:
    qDeleteAll(addTilesets);
}

LuaMapBmp &LuaMap::bmp(int index)
{
    return index ? mBmpVeg : mBmpMain;
}

void LuaMap::reloadRules()
{
    QString f = mClone->bmpSettings()->rulesFile();
    if (f.isEmpty() || !QFileInfo(f).exists())
        return; // error

    Tiled::Internal::BmpRulesFile rulesFile;
    if (!rulesFile.read(f))
        return; // error

    // FIXME: Not safe if any Lua variables are using these.
    qDeleteAll(mAliases);
    mAliases.clear();
    qDeleteAll(mRules);
    mRules.clear();
    mRuleByName.clear();

    QList<BmpAlias*> aliases = rulesFile.aliasesCopy();
    mClone->rbmpSettings()->setAliases(aliases);
    foreach (BmpAlias *alias, aliases) {
        mAliases += new LuaBmpAlias(alias);
        mAliasByName[alias->name] = mAliases.last();
    }
    QList<BmpRule*> rules = rulesFile.rulesCopy();
    mClone->rbmpSettings()->setRules(rules);
    foreach (BmpRule *rule, rules) {
        mRules += new LuaBmpRule(rule);
        if (!rule->label.isEmpty())
            mRuleByName[rule->label] = mRules.last();
    }

    mRulesChanged = true;
}

void LuaMap::reloadBlends()
{
    QString f = mClone->bmpSettings()->blendsFile();
    if (f.isEmpty() || !QFileInfo(f).exists())
        return; // error

    Tiled::Internal::BmpBlendsFile blendsFile;
    if (!blendsFile.read(f, mClone->rbmpSettings()->aliases()))
        return; // error

    // FIXME: Not safe if any Lua variables are using these.
    qDeleteAll(mBlends);
    mBlends.clear();

    QList<BmpBlend*> blends = blendsFile.blendsCopy();
    mClone->rbmpSettings()->setBlends(blends);
    foreach (BmpBlend *blend, blends)
        mBlends += new LuaBmpBlend(blend);

    mBlendsChanged = true;
}

QList<LuaBmpAlias *> LuaMap::aliases()
{
    return mAliases;
}

LuaBmpAlias *LuaMap::alias(const char *name)
{
    QString qname(QString::fromLatin1(name));
    if (mAliasByName.contains(qname))
        return mAliasByName[qname];
    return 0;
}

int LuaMap::ruleCount()
{
    return mRules.size();
}

QList<LuaBmpRule *> LuaMap::rules()
{
    return mRules;
}

LuaBmpRule *LuaMap::ruleAt(int index)
{
    if (index >= 0 && index < mRules.size())
        return mRules[index];
    return 0;
}

LuaBmpRule *LuaMap::rule(const char *name)
{
    QString qname(QString::fromLatin1(name));

    if (mRuleByName.contains(qname))
        return mRuleByName[qname];
    return 0;
}

QList<LuaBmpBlend *> LuaMap::blends()
{
    return mBlends;
}

bool LuaMap::isBlendTile(Tile *tile)
{
    QSet<Tile*> ret;
    foreach (LuaBmpBlend *blend, mBlends) {
        if (blend->mBlend->blendTile.isEmpty()) continue;
        if (mAliasByName.contains(blend->mBlend->blendTile))
            foreach (QString tileName, mAliasByName[blend->mBlend->blendTile]->tiles())
                ret += this->tile(tileName.toLatin1().constData());
        else
            ret += this->tile(blend->blendTile());
    }
    return ret.contains(tile);
}

QList<QString> LuaMap::blendLayers()
{
    QSet<QString> ret;
    foreach (LuaBmpBlend *blend, mBlends)
        ret += blend->mBlend->targetLayer;
    return QList<QString>(ret.constBegin(), ret.constEnd());
}

LuaMapNoBlend *LuaMap::noBlend(const char *layerName)
{
    QLatin1String qLayerName(layerName);
    if (!mNoBlends.contains(qLayerName))
        mNoBlends[qLayerName] = new LuaMapNoBlend(mClone->noBlend(qLayerName));
    return mNoBlends[qLayerName];
}

bool LuaMap::write(const char *path)
{
    QScopedPointer<Map> map(mClone->clone());
    Q_ASSERT(map->layerCount() == 0);
    for (LuaMapLevel *mapLevel : qAsConst(mLevels)) {
        for (LuaLayer *ll : qAsConst(mapLevel->mLayers)) {
            Layer *newLayer = ll->mClone ? ll->mClone->clone() : ll->mOrig->clone();
            if (LuaObjectGroup *og = ll->asObjectGroup()) {
                const QList<LuaMapObject*> objects = og->objects();
                for (LuaMapObject *o : objects) {
                    newLayer->asObjectGroup()->addObject(o->mClone->clone());
                }
            }
            map->addLayer(newLayer);
        }
    }

    Internal::TmxMapWriter writer;
    if (!writer.write(map.data(), QString::fromLatin1(path))) {
        // mError = write.errorString();
        return false;
    }
    return true;
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

void LuaMapBmp::setPixel(int x, int y, const LuaColor &c)
{
    if (!contains(x, y)) return; // error!
    if (mBmp.pixel(x, y) != c.pixel) {
        mBmp.setPixel(x, y, c.pixel);
        mAltered += QRect(x, y, 1, 1);
    }
}

unsigned int LuaMapBmp::pixel(int x, int y)
{
    if (!contains(x, y)) return qRgb(0,0,0); // error!
    return mBmp.pixel(x, y);
}

void LuaMapBmp::erase(int x, int y, int width, int height)
{
    fill(x, y, width, height, LuaColor());
}

void LuaMapBmp::erase(const QRect &r)
{
    fill(r, LuaColor());
}

void LuaMapBmp::erase(const LuaRegion &rgn)
{
    fill(rgn, LuaColor());
}

void LuaMapBmp::erase()
{
    fill(LuaColor());
}

void LuaMapBmp::fill(int x, int y, int width, int height, const LuaColor &c)
{
    fill(QRect(x, y, width, height), c);
}

void LuaMapBmp::fill(const QRect &r, const LuaColor &c)
{
    QRect r2 = r & QRect(0, 0, mBmp.width(), mBmp.height());

    for (int y = r2.y(); y <= r2.bottom(); y++) {
        for (int x = r2.x(); x <= r2.right(); x++) {
            if (mBmp.pixel(x, y) != c.pixel) {
                mBmp.setPixel(x, y, c.pixel);
                mAltered += QRect(x, y, 1, 1);
            }
        }
    }

}

void LuaMapBmp::fill(const LuaRegion &rgn, const LuaColor &c)
{
    for (const QRect &r : rgn)
        fill(r, c);
}

void LuaMapBmp::fill(const LuaColor &c)
{
    fill(QRect(0, 0, mBmp.width(), mBmp.height()), c);
}

void LuaMapBmp::replace(const LuaColor &oldColor, const LuaColor &newColor)
{
    for (int y = 0; y < mBmp.height(); y++) {
        for (int x = 0; x < mBmp.width(); x++) {
            if (mBmp.pixel(x, y) == oldColor.pixel)
                mBmp.setPixel(x, y, newColor.pixel);
        }
    }
}

int LuaMapBmp::rand(int x, int y)
{
    return mBmp.rand(x, y);
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

QStringList LuaBmpRule::tiles()
{
    return mRule->tileChoices;
}

const char *LuaBmpRule::layer()
{
    return cstring(mRule->targetLayer);
}

/////

LuaColor LuaBmpRule::condition()
{
    return mRule->condition;
}

////

LuaMapObject::LuaMapObject(MapObject *orig) :
    mClone(0),
    mOrig(orig)
{
}

LuaMapObject::LuaMapObject(const char *name, const char *type,
                           int x, int y, int width, int height) :
    mClone(new MapObject(QString::fromLatin1(name), QString::fromLatin1(type),
           QPointF(x, y), QSizeF(width, height))),
    mOrig(0)
{

}

const char *LuaMapObject::name()
{
    return mClone ? cstring(mClone->name()) : cstring(mOrig->name());
}

const char *LuaMapObject::type()
{
    return mClone ? cstring(mClone->type()) : cstring(mOrig->type());
}

QRect LuaMapObject::bounds()
{
    return mClone ? mClone->bounds().toAlignedRect() : mOrig->bounds().toAlignedRect();
}

/////

LuaObjectGroup::LuaObjectGroup(ObjectGroup *orig) :
    LuaLayer(orig),
    mCloneObjectGroup(nullptr),
    mColor(orig->color())
{
    for (MapObject *mo : orig->objects())
        addObject(new LuaMapObject(mo));
}

LuaObjectGroup::LuaObjectGroup(const char *name, int x, int y, int width, int height) :
    LuaLayer(),
    mCloneObjectGroup(new ObjectGroup(QString::fromLatin1(name), x, y, width, height))
{
    mName = mCloneObjectGroup->name();
    mClone = mCloneObjectGroup;
}

LuaObjectGroup::~LuaObjectGroup()
{
    qDeleteAll(mObjects);
}

void LuaObjectGroup::cloned()
{
    LuaLayer::cloned();
    mCloneObjectGroup = mClone->asObjectGroup();
}

void LuaObjectGroup::setColor(LuaColor &color)
{
    mColor = QColor(color.r, color.g, color.b);
}

LuaColor LuaObjectGroup::color()
{
    return LuaColor(mColor.red(), mColor.green(), mColor.blue());
}

void LuaObjectGroup::addObject(LuaMapObject *object)
{
    initClone();
    // FIXME: MainWindow::LuaScript must use these
//    mCloneObjectGroup->addObject(object->mClone ? object->mClone : object->mOrig);
    mObjects += object;
}

QList<LuaMapObject *> LuaObjectGroup::objects()
{
    return mObjects;
}

/////

QStringList LuaBmpAlias::tiles()
{
    return mAlias->tiles;
}

/////

const char *LuaBmpBlend::layer()
{
    return cstring(mBlend->targetLayer);
}

const char *LuaBmpBlend::mainTile()
{
    return cstring(mBlend->mainTile);
}

const char *LuaBmpBlend::blendTile()
{
    return cstring(mBlend->blendTile);
}

LuaBmpBlend::Direction LuaBmpBlend::direction()
{
    return (Direction) mBlend->dir;
}

QStringList LuaBmpBlend::exclude()
{
    return mBlend->ExclusionList;
}

/////

LuaMapNoBlend::LuaMapNoBlend(MapNoBlend *clone) :
    mClone(clone)
{

}

LuaMapNoBlend::~LuaMapNoBlend()
{
}

void LuaMapNoBlend::set(int x, int y, bool noblend)
{
    if (x < 0 || x >= mClone->width()) return; // error
    if (y < 0 || y >= mClone->height()) return; // error
    if (mClone->get(x, y) != noblend) {
        mClone->set(x, y, noblend);
        mAltered += QRect(x, y, 1, 1);
    }
}

void LuaMapNoBlend::set(const LuaRegion &rgn, bool noblend)
{
    for (const QRect &r : rgn)
        for (int y = r.top(); y <= r.bottom(); y++)
            for (int x = r.left(); x <= r.right(); x++)
                set(x, y, noblend);
}

bool LuaMapNoBlend::get(int x, int y)
{
    if (x < 0 || x >= mClone->width()) return false; // error
    if (y < 0 || y >= mClone->height()) return false; // error
    return mClone->get(x, y);
}

/////

TileName::TileName(const char *tileName)
{
    if (!parseTileName(QString::fromLatin1(tileName), mTilesetName, mTileID)) {
        mTilesetName.clear();
    }
}

TileName::TileName(const QString &tileName)
{
    if (!parseTileName(tileName, mTilesetName, mTileID)) {
        mTilesetName.clear();
    }
}

/////

// http://flafla2.github.io/2014/08/09/perlinnoise.html

// Hash lookup table as defined by Ken Perlin.  This is a randomly
// arranged array of all numbers from 0-255 inclusive.
const int LuaPerlin::permutation[] = {
    151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
    88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
    77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
    135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
    5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
    223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
    129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
    251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
    49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
    138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

LuaPerlin::LuaPerlin()
{
    p = new int[512];
    for(int x=0;x<512;x++) {
        p[x] = permutation[x%256];
    }
}

double LuaPerlin::perlin(double x, double y, double z)
{
#if 0
    if(repeat > 0) {									// If we have any repeat on, change the coordinates to their "local" repetitions
        x = x%repeat;
        y = y%repeat;
        z = z%repeat;
    }
#endif
    int xi = (int)x & 255;								// Calculate the "unit cube" that the point asked will be located in
    int yi = (int)y & 255;								// The left bound is ( |_x_|,|_y_|,|_z_| ) and the right bound is that
    int zi = (int)z & 255;								// plus 1.  Next we calculate the location (from 0.0 to 1.0) in that cube.
    double xf = x-(int)x;								// We also fade the location to smooth the result.
    double yf = y-(int)y;
    double zf = z-(int)z;
    double u = fade(xf);
    double v = fade(yf);
    double w = fade(zf);

    int aaa, aba, aab, abb, baa, bba, bab, bbb;
    aaa = p[p[p[    xi ]+    yi ]+    zi ];
    aba = p[p[p[    xi ]+inc(yi)]+    zi ];
    aab = p[p[p[    xi ]+    yi ]+inc(zi)];
    abb = p[p[p[    xi ]+inc(yi)]+inc(zi)];
    baa = p[p[p[inc(xi)]+    yi ]+    zi ];
    bba = p[p[p[inc(xi)]+inc(yi)]+    zi ];
    bab = p[p[p[inc(xi)]+    yi ]+inc(zi)];
    bbb = p[p[p[inc(xi)]+inc(yi)]+inc(zi)];

    double x1, x2, y1, y2;
    x1 = lerp(	grad (aaa, xf  , yf  , zf),				// The gradient function calculates the dot product between a pseudorandom
                grad (baa, xf-1, yf  , zf),				// gradient vector and the vector from the input coordinate to the 8
                u);										// surrounding points in its unit cube.
    x2 = lerp(	grad (aba, xf  , yf-1, zf),				// This is all then lerped together as a sort of weighted average based on the faded (u,v,w)
                grad (bba, xf-1, yf-1, zf),				// values we made earlier.
                  u);
    y1 = lerp(x1, x2, v);

    x1 = lerp(	grad (aab, xf  , yf  , zf-1),
                grad (bab, xf-1, yf  , zf-1),
                u);
    x2 = lerp(	grad (abb, xf  , yf-1, zf-1),
                grad (bbb, xf-1, yf-1, zf-1),
                u);
    y2 = lerp (x1, x2, v);

    return (lerp (y1, y2, w)+1)/2;						// For convenience we bound it to 0 - 1 (theoretical min/max before is -1 - 1)
}

int LuaPerlin::inc(int num)
{
    num++;
    if (repeat > 0)
        num %= repeat;
    return num;
}

double LuaPerlin::grad(int hash, double x, double y, double z)
{
    int h = hash & 15;									// Take the hashed value and take the first 4 bits of it (15 == 0b1111)
    double u = h < 8 /* 0b1000 */ ? x : y;				// If the most significant bit (MSB) of the hash is 0 then set u = x.  Otherwise y.

    double v;											// In Ken Perlin's original implementation this was another conditional operator (?:).  I
                                                        // expanded it for readability.

    if(h < 4 /* 0b0100 */)								// If the first and second significant bits are 0 set v = y
        v = y;
    else if(h == 12 /* 0b1100 */ || h == 14 /* 0b1110*/)// If the first and second significant bits are 1 set v = x
        v = x;
    else 												// If the first and second significant bits are not equal (0/1, 1/0) set v = z
        v = z;

    return ((h&1) == 0 ? u : -u)+((h&2) == 0 ? v : -v); // Use the last 2 bits to decide if u and v are positive or negative.  Then return their addition.
}

double LuaPerlin::fade(double t)
{
    // Fade function as defined by Ken Perlin.  This eases coordinate values
    // so that they will "ease" towards integral values.  This ends up smoothing
    // the final output.
    return t * t * t * (t * (t * 6 - 15) + 10); // 6t^5 - 15t^4 + 10t^3
}

double LuaPerlin::lerp(double a, double b, double x)
{
    return a + x * (b - a);
}
