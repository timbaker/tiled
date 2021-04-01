/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#include "containeroverlayfile.h"

#include "luaconsole.h"
#include "luatable.h"

#include "BuildingEditor/buildingtiles.h"

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

#include <QDebug>
#include <QFileInfo>
#include <QScopedPointer>
#include <QTemporaryFile>

namespace Tiled {
namespace Lua {
extern const char *cstring(const QString &qstring);
}
}

extern "C" {
extern int traceback(lua_State *L);
}

namespace {

class WorldFillerFile
{
public:
    WorldFillerFile();
    ~WorldFillerFile();

    lua_State *init();
    bool dofile(const QString &f, QString &output);
    LuaTable *parseOverlayMap();
    LuaTable *parseTable();

    lua_State *L;
};

} // namespace anon

WorldFillerFile::WorldFillerFile() :
    L(nullptr)
{
}

WorldFillerFile::~WorldFillerFile()
{
    if (L) {
        lua_close(L);
    }
}

lua_State *WorldFillerFile::init()
{
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushboolean(L, 1);
    lua_setglobal(L, "TILEZED");

    return L;
}

bool WorldFillerFile::dofile(const QString &f, QString &output)
{
    lua_State *L = init();

    int status = luaL_loadfile(L, Tiled::Lua::cstring(f));
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        int nargs = 0;
        int nret = 1;
        status = lua_pcall(L, nargs, nret, base);
        lua_remove(L, base);
    }
    if (status != LUA_OK) {
        output = QString::fromLatin1(lua_tostring(L, -1));
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
//    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return status == LUA_OK;
}

#include <tolua.h>

LuaTable *WorldFillerFile::parseOverlayMap()
{
    tolua_Error err;
    if ((lua_gettop(L) >= 1) && (tolua_istable(L, -1, 0, &err) == 1)) {
        // VERSION 1: file-local "overlayMap" return-value
    } else {
        // VERSION 0: global "overlayMap"
        lua_getglobal(L, "overlayMap");
    }
    LuaTable *table = parseTable();
    lua_pop(L, 1);
    return table;
}

LuaTable *WorldFillerFile::parseTable()
{
    LuaTable *table = new LuaTable;
    lua_pushnil(L); // push key
    while (lua_next(L, -2)) {
        // stack now contains: -1 => value; -2 => key; -3 => table
        // copy the key so that lua_tostring does not modify the original
        lua_pushvalue(L, -2);
        QString key = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, 1); // pop copy of key
//        qDebug() << key;
        if (lua_istable(L, -1)) {
            table->kv += new LuaTableKeyValue(key, parseTable());
        } else {
            QString value = QString::fromLatin1(lua_tostring(L, -1));
            table->kv += new LuaTableKeyValue(key, value);
        }
        lua_pop(L, 1); // pop value
        // stack now contains: -1 => key; -2 => table
    }
    return table;
}

/////

AbstractOverlay* ContainerOverlayEntry::parent() const
{
    return mParent;
}

int ContainerOverlayEntry::indexOf() const
{
    return mParent->mEntries.indexOf(const_cast<ContainerOverlayEntry*>(this));
}

/////

ContainerOverlayFile::ContainerOverlayFile()
{
}

// overlayMap["location_shop_generic_01_40"] = {
//   clothesstore = { "clothing_01_8", "clothing_01_16" },
//   departmentstore = {"clothing_01_8", "clothing_01_16"},
//   generalstore = {"clothing_01_8", "clothing_01_16"}
// }
bool ContainerOverlayFile::readV0(LuaTable* table)
{
    QMap<QString,ContainerOverlay*> map;
    for (LuaTableKeyValue *kv : table->kv) {
        ContainerOverlay *overlay = new ContainerOverlay;
        overlay->mTileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(kv->key);
        for (LuaTableKeyValue *kv2 : kv->t->kv) {
            ContainerOverlayEntry *entry = new ContainerOverlayEntry;
            entry->mParent = overlay;
            entry->mRoomName = kv2->key;
            for (LuaTableKeyValue *kv3 : kv2->t->kv) {
                QString tileName = kv3->s;
                if (tileName != QLatin1String("none")) {
                    tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                }
                entry->mTiles << tileName;
            }
            while (entry->mTiles.size() < 2) {
                entry->mTiles += QLatin1String("none");
            }
            overlay->mEntries += entry;
        }
        map[overlay->mTileName] = overlay;
    }
    mOverlays = map.values();
    return true;
}

// overlayMap["location_shop_generic_01_40"] = {
//   { name = "clothesstore", tiles = { "clothing_01_8", "clothing_01_16" } },
//   { name = "departmentstore", tiles = { "clothing_01_8", "clothing_01_16" } },
//   { name = "generalstore", tiles = { "clothing_01_8", "clothing_01_16" } }
// }
bool ContainerOverlayFile::readV1(LuaTable* table)
{
    QMap<QString,ContainerOverlay*> map;
    for (LuaTableKeyValue *overlayKV : table->kv) {
        LuaTable* overlayTable = overlayKV->t;
        if (overlayTable == nullptr) {
            Q_ASSERT(overlayKV->key == QLatin1String("VERSION"));
            continue;
        }
        ContainerOverlay *overlay = new ContainerOverlay;
        overlay->mTileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(overlayKV->key);
        for (LuaTableKeyValue *roomKV : overlayTable->kv) {
            LuaTable* roomTable = roomKV->t;
            if (roomTable == nullptr) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected room table");
                return false;
            }
            ContainerOverlayEntry *entry = new ContainerOverlayEntry;
            entry->mParent = overlay;
            if (!roomTable->getString(QLatin1String("name"), entry->mRoomName)) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected \"name\"");
                return false;
            }
            LuaTable* tileTable = roomTable->getTable(QLatin1String("tiles"));
            if (tileTable == nullptr) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected tiles table");
                return false;
            }
            for (LuaTableKeyValue *tileKV : tileTable->kv) {
                QString tileName = tileKV->s;
                if (tileName != QLatin1String("none")) {
                    tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                }
                entry->mTiles << tileName;
            }
            while (entry->mTiles.size() < 2) {
                entry->mTiles += QLatin1String("none");
            }
            overlay->mEntries += entry;
        }
        map[overlay->mTileName] = overlay;
    }
    mOverlays = map.values();
    return true;
}

bool ContainerOverlayFile::read(const QString &fileName)
{
    WorldFillerFile wff;
    QString output;
    if (wff.dofile(fileName, output)) {
        QScopedPointer<LuaTable> table(wff.parseOverlayMap());
        LuaTableKeyValue* versionKV = table->find(QLatin1String("VERSION"));
        if (versionKV == nullptr) {
            return readV0(table.data());
        }
        if (versionKV->s == QLatin1String("1")) {
            return readV1(table.data());
        }
        mError = QString(QLatin1String("unknown version %1")).arg(versionKV->s);
        return false;
    }
    mError = output;
    return false;
}

static QString shortTileName(const QString &tileName)
{
    if (tileName.isEmpty() || (tileName == QLatin1String("none")))
        return tileName;
    QString tilesetName;
    int index;
    if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, index))
        return QString(QLatin1String("%1_%2")).arg(tilesetName).arg(index);
    return tileName;
}

static const int VERSION1 = 1;
static const int VERSION = VERSION1;

bool ContainerOverlayFile::write(const QString &filePath, const QList<ContainerOverlay*> overlays)
{
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        mError = tempFile.errorString();
        return false;
    }

    QTextStream ts(&tempFile);

    ts << QLatin1String("-- THIS FILE WAS AUTOMATICALLY GENERATED BY TileZed\n");
    ts << QLatin1String("local overlayMap = {}\n");
    ts << QString::fromUtf8("overlayMap.VERSION = %1\n").arg(VERSION);
    for (ContainerOverlay *overlay : overlays) {
        QString line;
        bool prev = false;
        // sort by room name
        QMap<QString,QList<ContainerOverlayEntry*>> entryMap;
        for (ContainerOverlayEntry *entry : overlay->mEntries) {
            entryMap[entry->mRoomName] += entry;
        }
        for (QList<ContainerOverlayEntry *> entries : entryMap.values()) {
            for (ContainerOverlayEntry* entry : entries) {
                QString tiles;
                for (QString tileName : entry->mTiles) {
                    if (tileName.isEmpty()) {
                        continue;
                    }
                    if (tileName == QLatin1String("none") && !tiles.isEmpty()) {
                        continue;
                    }
                    if (!tiles.isEmpty()) {
                        tiles += QLatin1String(", ");
                    }
                    tiles += QStringLiteral("\"%1\"").arg(shortTileName(tileName));
                }
                if (!tiles.isEmpty()) {
                    if (prev) {
                        line += QLatin1String(", ");
                    }
                    line += QStringLiteral("{ name = \"%1\", tiles = {%2} }").arg(entry->mRoomName).arg(tiles);
                    prev = true;
                }
            }
        }
        if (prev) {
            ts << QStringLiteral("overlayMap[\"%1\"] = {%2}\n").arg(shortTileName(overlay->mTileName)).arg(line);
        }
    }
    ts << QLatin1String("\nif not TILEZED then\n\tgetContainerOverlays():addOverlays(overlayMap)\nend\n");
    ts << QLatin1String("\nreturn overlayMap\n");

    if (tempFile.error() != QFile::NoError) {
        mError = tempFile.errorString();
        return false;
    }

    // foo.lua -> foo.lua.bak
    QFileInfo destInfo(filePath);
    QString backupPath = filePath + QLatin1String(".bak");
    QFile backupFile(backupPath);
    if (destInfo.exists()) {
        if (backupFile.exists()) {
            if (!backupFile.remove()) {
                mError = QStringLiteral("Error deleting file!\n%1\n\n%2")
                        .arg(backupPath)
                        .arg(backupFile.errorString());
                return false;
            }
        }
        QFile destFile(filePath);
        if (!destFile.rename(backupPath)) {
            mError = QStringLiteral("Error renaming file!\nFrom: %1\nTo: %2\n\n%3")
                    .arg(filePath)
                    .arg(backupPath)
                    .arg(destFile.errorString());
            return false;
        }
    }

    // /tmp/tempXYZ -> foo.tbx
    tempFile.close();
    if (!tempFile.rename(filePath)) {
        mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                .arg(tempFile.fileName())
                .arg(filePath)
                .arg(tempFile.errorString());
        // Try to un-rename the backup file
        if (backupFile.exists())
            backupFile.rename(filePath); // might fail
        return false;
    }

    // If anything above failed, the temp file should auto-remove, but not after
    // a successful save.
    tempFile.setAutoRemove(false);

    return true;
}

QList<ContainerOverlay *> ContainerOverlayFile::takeOverlays()
{
    QList<ContainerOverlay*> overlays = mOverlays;
    mOverlays.clear();
    return overlays;
}
