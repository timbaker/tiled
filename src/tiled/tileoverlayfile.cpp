/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#include "tileoverlayfile.h"

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

class TileOverlayParser
{
public:
    TileOverlayParser();
    ~TileOverlayParser();

    lua_State *init();
    bool dofile(const QString &f, QString &output);
    LuaTable *parseOverlayMap();
    LuaTable *parseTable();

    lua_State *L;
};

} // namespace anon

TileOverlayParser::TileOverlayParser() :
    L(nullptr)
{
}

TileOverlayParser::~TileOverlayParser()
{
    if (L) {
        lua_close(L);
    }
}

lua_State *TileOverlayParser::init()
{
    L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushboolean(L, 1);
    lua_setglobal(L, "TILEZED");

    return L;
}

bool TileOverlayParser::dofile(const QString &f, QString &output)
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

LuaTable *TileOverlayParser::parseOverlayMap()
{
    tolua_Error err;
    if ((lua_gettop(L) >= 1) && (tolua_istable(L, -1, 0, &err) == 1)) {
        // VERSION 1: file-local "overlayMap" return-value
    } else {
       return nullptr;
    }
    LuaTable *table = parseTable();
    lua_pop(L, 1);
    return table;
}

LuaTable *TileOverlayParser::parseTable()
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

AbstractOverlay *TileOverlayEntry::parent() const
{
    return mParent;
}

int TileOverlayEntry::indexOf() const
{
    return mParent->mEntries.indexOf(const_cast<TileOverlayEntry*>(this));
}

/////

TileOverlayFile::TileOverlayFile()
{
}

// overlayMap["location_business_office_generic_01_7"] = {
//     { name = "other", chance = 2, tiles = {"papernotices_01_9"} }
// }

bool TileOverlayFile::readV1(LuaTable* table)
{
    QMap<QString,TileOverlay*> map;
    for (LuaTableKeyValue *overlayKV : qAsConst(table->kv)) {
        LuaTable* overlayTable = overlayKV->t;
        if (overlayTable == nullptr) {
            Q_ASSERT(overlayKV->key == QLatin1String("VERSION"));
            continue;
        }
        TileOverlay *overlay = new TileOverlay;
        overlay->mTileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(overlayKV->key);
        for (LuaTableKeyValue *roomKV : qAsConst(overlayTable->kv)) {
            LuaTable* roomTable = roomKV->t;
            if (roomTable == nullptr) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected room table");
                return false;
            }
            TileOverlayEntry *entry = new TileOverlayEntry;
            entry->mParent = overlay;

            if (!roomTable->getString(QLatin1String("name"), entry->mRoomName)) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected \"name\"");
                return false;
            }

            QString chanceStr;
            if (!roomTable->getString(QLatin1String("chance"), chanceStr)) {
                mError = QString::fromUtf8("expected \"chance\"");
                return false;
            }

            roomTable->getString(QLatin1String("usage"), entry->mUsage);

            bool ok;
            entry->mChance = chanceStr.toInt(&ok);
            if ((ok == false) || (entry->mChance < 1)) {
                mError = QString::fromUtf8("expected integer chance > 1");
                return false;
            }

            LuaTable* tileTable = roomTable->getTable(QLatin1String("tiles"));
            if (tileTable == nullptr) {
                // FIXME: delete 'map'
                mError = QString::fromUtf8("expected tiles table");
                return false;
            }
            for (LuaTableKeyValue *tileKV : qAsConst(tileTable->kv)) {
                QString tileName = tileKV->s;
                if (tileName != QLatin1String("none")) {
                    tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);
                }
                entry->mTiles << tileName;
            }
            overlay->mEntries += entry;
        }
        map[overlay->mTileName] = overlay;
    }
    mOverlays = map.values();
    return true;
}

bool TileOverlayFile::read(const QString &fileName)
{
    TileOverlayParser wff;
    QString output;
    if (wff.dofile(fileName, output)) {
        QScopedPointer<LuaTable> table(wff.parseOverlayMap());
        if (table.isNull()) {
            mError = QString::fromUtf8("failed to parse overlayMap");
            return false;
        }
        LuaTableKeyValue* versionKV = table->find(QLatin1String("VERSION"));
        if (versionKV == nullptr) {
            mError = QString::fromUtf8("missing overlayMap.VERSION");
            return false;
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

bool TileOverlayFile::write(const QString &filePath, const QList<TileOverlay*> &overlays)
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
    for (TileOverlay *overlay : overlays) {
        QString line;
        bool prev = false;
        // sort by room name
        QMap<QString,QList<TileOverlayEntry*>> entryMap;
        for (TileOverlayEntry *entry : qAsConst(overlay->mEntries)) {
            entryMap[entry->mRoomName] += entry;
        }
        QList<QList<TileOverlayEntry*>> entryLists = entryMap.values();
        for (const QList<TileOverlayEntry *> &entries : entryLists) {
            for (TileOverlayEntry* entry : entries) {
                QString tiles;
                for (const QString &tileName : qAsConst(entry->mTiles)) {
                    if (tileName.isEmpty())
                        continue;
//                    if (tileName == QLatin1String("none") && !tiles.isEmpty()) {
//                        continue;
//                    }
                    if (!tiles.isEmpty()) {
                        tiles += QLatin1String(", ");
                    }
                    tiles += QString(QLatin1String("\"%1\"")).arg(shortTileName(tileName));
                }
                if (!tiles.isEmpty()) {
                    if (prev) {
                        line += QLatin1String(", ");
                    }
                    line += QStringLiteral("{ name = \"%1\", chance = %2, usage = \"%3\", tiles = {%4} }")
                            .arg(entry->mRoomName)
                            .arg(entry->mChance)
                            .arg(entry->mUsage)
                            .arg(tiles);
                    prev = true;
                }
            }
        }
        if (prev) {
            ts << QStringLiteral("overlayMap[\"%1\"] = {%2}\n").arg(shortTileName(overlay->mTileName)).arg(line);
        }
    }
    ts << QLatin1String("\nif not TILEZED then\n\tgetTileOverlays():addOverlays(overlayMap)\nend\n");
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

QList<TileOverlay *> TileOverlayFile::takeOverlays()
{
    QList<TileOverlay *> overlays = mOverlays;
    mOverlays.clear();
    return overlays;
}
