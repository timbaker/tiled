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

#include "BuildingEditor/buildingtiles.h"

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

#include <QDebug>
#include <QFileInfo>
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

class LuaTable;

class LuaTableKeyValue
{
public:
    LuaTableKeyValue(QString &key, QString value) : key(key), s(value), t(0) {}
    LuaTableKeyValue(QString &key, LuaTable *value) : key(key), t(value) {}
    ~LuaTableKeyValue();

    QString key;
    QString s;
    LuaTable *t;
};

class LuaTable
{
public:
    ~LuaTable()
    {
        qDeleteAll(kv);
    }

    QList<LuaTableKeyValue*> kv;
};

LuaTableKeyValue::~LuaTableKeyValue() { delete t; }

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
    L(0)
{
}

WorldFillerFile::~WorldFillerFile()
{
    if (L)
        lua_close(L);
}

lua_State *WorldFillerFile::init()
{
    L = luaL_newstate();
    luaL_openlibs(L);
//    tolua_tiled_open(L);

//    tolua_beginmodule(L,NULL);
//    tolua_endmodule(L);

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
        status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);
    }
    if (status != LUA_OK) {
        output = QString::fromLatin1(lua_tostring(L, -1));
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
//    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return status == LUA_OK;
}

LuaTable *WorldFillerFile::parseOverlayMap()
{
    lua_getglobal(L, "overlayMap");
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

ContainerOverlayFile::ContainerOverlayFile()
{
}

bool ContainerOverlayFile::read(const QString &fileName)
{
    WorldFillerFile wff;
    QString output;
    if (wff.dofile(fileName, output)) {
        LuaTable *table = wff.parseOverlayMap();
        QMap<QString,ContainerOverlay*> map;
        foreach (LuaTableKeyValue *kv, table->kv) {
            ContainerOverlay *overlay = new ContainerOverlay;
            overlay->mTileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(kv->key);
            foreach (LuaTableKeyValue *kv2, kv->t->kv) {
                ContainerOverlayEntry *entry = new ContainerOverlayEntry;
                entry->mParent = overlay;
                entry->mRoomName = kv2->key;
                foreach (LuaTableKeyValue *kv3, kv2->t->kv) {
                    QString tileName = kv3->s;
                    if (tileName != QLatin1String("none"))
                        tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                    entry->mTiles << tileName;
                }
                while (entry->mTiles.size() < 2)
                    entry->mTiles += QLatin1String("none");
                overlay->mEntries += entry;
            }
            map[overlay->mTileName] = overlay;
        }
        mOverlays = map.values();
        delete table;
        return true;
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
        return QString(QLatin1Literal("%1_%2")).arg(tilesetName).arg(index);
    return tileName;
}

bool ContainerOverlayFile::write(const QString &filePath)
{
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        mError = tempFile.errorString();
        return false;
    }

    QTextStream ts(&tempFile);

    ts << QLatin1Literal("overlayMap = {}\n");
    foreach (ContainerOverlay *overlay, mOverlays) {
        QString line;
        bool prev = false;
        QMap<QString,ContainerOverlayEntry*> entryMap; // sort by room name
        foreach (ContainerOverlayEntry *entry, overlay->mEntries)
            entryMap[entry->mRoomName] = entry;
        foreach (ContainerOverlayEntry *entry, entryMap.values()) {
            QString tiles;
            foreach (QString tileName, entry->mTiles) {
                if (tileName.isEmpty())
                    continue;
                if (tileName == QLatin1String("none") && !tiles.isEmpty()) {
                    continue;
                }
                if (!tiles.isEmpty())
                    tiles += QLatin1Literal(", ");
                tiles += QString(QLatin1Literal("\"%1\"")).arg(shortTileName(tileName));
            }
            if (!tiles.isEmpty()) {
                if (prev)
                    line += QLatin1Literal(", ");
                line += QString(QLatin1Literal("%1 = {%2}")).arg(shortTileName(entry->mRoomName)).arg(tiles);
                prev = true;
            }
        }
        if (prev) {
            ts << QString(QLatin1Literal("overlayMap[\"%1\"] = {%2}\n")).arg(shortTileName(overlay->mTileName)).arg(line);
        }
    }

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
                mError = QString(QLatin1String("Error deleting file!\n%1\n\n%2"))
                        .arg(backupPath)
                        .arg(backupFile.errorString());
                return false;
            }
        }
        QFile destFile(filePath);
        if (!destFile.rename(backupPath)) {
            mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
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
