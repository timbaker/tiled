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

#include "luatiletool.h"

#include "bmptool.h"
#include "luaconsole.h"
#include "luatiled.h"
#include "luatooldialog.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapscene.h"
#include "painttilelayer.h"
#include "preferences.h"

#include "tolua.h"

#include "maprenderer.h"
#include "tilelayer.h"

#include <qmath.h>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QUndoStack>

extern "C" {
#include "lualib.h"
#include "lauxlib.h"
}

TOLUA_API int tolua_tiled_open(lua_State *L);

using namespace Tiled;
using namespace Tiled::Internal;
using namespace Tiled::Lua;

extern "C" {

static int loadToolData(lua_State *L)
{
    const char *f = lua_tostring(L, -1);
    {
        QString fileName = QString::fromLatin1("tool-%1-data.lua").arg(QLatin1String(f));
        QFileInfo info(Preferences::instance()->configPath(QLatin1String("lua/") + fileName));
        if (info.exists()) {
            if (luaL_loadfile(L, cstring(info.absoluteFilePath())) != LUA_OK)
                goto error;
            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
                goto error;
        }

        QFileInfo info2(Preferences::instance()->luaPath(fileName));
        if (info2.exists()) {
            if (luaL_loadfile(L, cstring(info2.absoluteFilePath())) != LUA_OK)
                goto error;
            if (lua_pcall(L, 0, 0, 0) != LUA_OK)
                goto error;
        }
    }
    return 0;
error:
    return lua_error(L); // !!! longjmp() !!! (unless Lua compiled as C++)
}

} // extern "C"

SINGLETON_IMPL(LuaTileTool)

LuaTileTool::LuaTileTool(const QString &scriptFileName,
                         const QString &dialogTitle,
                         const QString &name, const QIcon &icon,
                         const QKeySequence &shortcut, QObject *parent) :
    AbstractTileTool(name, icon, shortcut, parent),
    mFileName(scriptFileName),
    mDialogTitle(dialogTitle),
    L(0),
    mMap(0),
    mMapChanged(false),
    mCursorItem(new QGraphicsPathItem),
    mCursorType(None),
    mSaveOptionValue(true)
{
    mDistanceIndicators[0] = mDistanceIndicators[1] = mDistanceIndicators[2] = mDistanceIndicators[3] = 0;

    mCursorItem->setPen(QPen(QColor(0,255,0,96), 1));
    mCursorItem->setBrush(QColor(0,255,0,64));
}

LuaTileTool::~LuaTileTool()
{
    qDeleteAll(mToolTileLayers);
    qDeleteAll(mToolNoBlends);
    // qDeleteAll(mDistanceIndicators)
}

void LuaTileTool::loadScript()
{
    MapScene *scene = mScene;
    if (mScene) deactivate(scene);

    if (L)
        lua_close(L);
    L = luaL_newstate();
    luaL_openlibs(L);
    tolua_tiled_open(L);

    tolua_pushusertype(L, this, "LuaTileTool");
    lua_setglobal(L, "self");

    tolua_pushstring(L, Lua::cstring(QFileInfo(mFileName).absolutePath()));
    lua_setglobal(L, "scriptDirectory");

    lua_pushcfunction(L, loadToolData);
    lua_setglobal(L, "loadToolData");

    int status = luaL_loadfile(L, cstring(mFileName));
    if (status == LUA_OK) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);
    }

    if (status != LUA_OK) {
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }

    if (status == LUA_OK)
        setToolOptions();

    if (scene) activate(scene);
}

void LuaTileTool::activate(MapScene *scene)
{
    AbstractTileTool::activate(scene);
    mScene = scene;

    mScene->addItem(mCursorItem);

    connect(mapDocument(), SIGNAL(mapChanged()), SLOT(mapChanged()));

    connect(mapDocument(), SIGNAL(tilesetAdded(int,Tileset*)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(tilesetRemoved(Tileset*)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(tilesetNameChanged(Tileset*)), SLOT(mapChanged()));

    connect(mapDocument(), SIGNAL(layerAdded(int)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(layerRemoved(int)), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(layerChanged(int)), SLOT(mapChanged())); // layer renamed
    connect(mapDocument(), SIGNAL(regionAltered(QRegion,Layer*)), SLOT(mapChanged()));

    connect(mapDocument(), SIGNAL(bmpAliasesChanged()), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(bmpBlendsChanged()), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(bmpRulesChanged()), SLOT(mapChanged()));
    connect(mapDocument(), SIGNAL(bmpPainted(int,QRegion)), SLOT(mapChanged()));

    connect(mapDocument(), SIGNAL(noBlendPainted(MapNoBlend*,QRegion)), SLOT(mapChanged()));

    if (!L && !mFileName.isEmpty()) {
        mScene = 0;
        loadScript();
        mScene = scene;
    }

    if (mOptions.mOptions.size()) {
        connect(LuaToolDialog::instancePtr(), SIGNAL(valueChanged(LuaToolOption*,QVariant)),
                SLOT(setOption(LuaToolOption*,QVariant)));
        LuaToolDialog::instancePtr()->setWindowTitle(mDialogTitle);
        LuaToolDialog::instancePtr()->setVisibleLater(true);
    }

    if (!L) return;

    checkMap();

    lua_getglobal(L, "activate");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    int base = lua_gettop(L);
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    int status = lua_pcall(L, 0, 0, base);
    lua_remove(L, base);

    if (status != LUA_OK) {
        mMapChanged = true; // discard any changes before the error occurred
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
}

void LuaTileTool::deactivate(MapScene *scene)
{
    LuaToolDialog::instancePtr()->disconnect(this);
    LuaToolDialog::instancePtr()->setToolOptions(0);
//    LuaToolDialog::instancePtr()->setWindowTitle(tr("Lua Tool"));
    LuaToolDialog::instancePtr()->setVisibleLater(false);

    clearToolTiles();
    clearToolNoBlends();
    clearDistanceIndicators();
    mapDocument()->disconnect(this, SLOT(mapChanged()));
    scene->removeItem(mCursorItem);
    mCursorType = LuaTileTool::None;

    AbstractTileTool::deactivate(scene);

    if (!L) return;

    lua_getglobal(L, "deactivate");
    if (lua_isfunction(L, -1)) {
        int base = lua_gettop(L);
        lua_pushcfunction(L, traceback);
        lua_insert(L, base);
        int status = lua_pcall(L, 0, 0, base);
        lua_remove(L, base);

        if (status != LUA_OK) {
            QString output = QString::fromLatin1(lua_tostring(L, -1));
            lua_pop(L, -1); // pop error
            LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
        }
    } else
        lua_pop(L, 1); // function deactivate()

    lua_close(L);
    L = 0;

    delete mMap;
    mMap = 0;

    mScene = 0;
}

void LuaTileTool::mouseLeft()
{
    clearToolTiles();
    clearToolNoBlends();
    AbstractTileTool::mouseLeft();
}

void LuaTileTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (mButtons & Qt::LeftButton) {
        mCursorItem->setVisible(false);
    } else {
        QPainterPath path = cursorShape(pos, modifiers);
        mCursorItem->setPath(path);
        mCursorItem->setVisible(true);
    }

    if (mButtons & Qt::LeftButton)
        mCurrentScenePos = pos;
    mouseEvent("mouseMoved", mButtons, pos, modifiers);

    int level = mapDocument()->currentLevel();
    QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(pos, level);
    if (true) {
        setStatusInfo(QString(QLatin1String("%1, %2"))
                      .arg(tilePos.x()).arg(tilePos.y()));
    } else {
        setStatusInfo(QString());
    }
}

void LuaTileTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    mButtons = event->buttons();
    if (event->button() == Qt::LeftButton)
        mStartScenePos = event->screenPos();
    mouseEvent("mousePressed", event->button(), event->scenePos(), event->modifiers());
}

void LuaTileTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    mButtons = event->buttons();
    if (event->button() == Qt::LeftButton)
        mCurrentScenePos = event->screenPos();
    mouseEvent("mouseReleased", event->button(), event->scenePos(), event->modifiers());
}

void LuaTileTool::modifiersChanged(Qt::KeyboardModifiers modifiers)
{
    if (!L) return;

    checkMap();

    lua_getglobal(L, "modifiersChanged");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L); // arg modifiers
    if (modifiers & Qt::AltModifier) {
        lua_pushstring(L, "alt");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ControlModifier) {
        lua_pushstring(L, "control");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ShiftModifier) {
        lua_pushstring(L, "shift");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    int nargs = 1;
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);

    int status = lua_pcall(L, nargs, 0, base);

    lua_remove(L, base);

    if (status != LUA_OK) {
        mMapChanged = true; // discard any changes before the error occurred
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
}

void LuaTileTool::languageChanged()
{

}

void LuaTileTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
}

void LuaTileTool::setOption(LuaToolOption *option, const QVariant &value)
{
    if (!L) return;

    if (mSaveOptionValue) {
        QSettings settings;
        settings.beginGroup(QLatin1String("LuaTileTool/Tool/")+QFileInfo(mFileName).completeBaseName());
        settings.setValue(option->mName, value);
        settings.endGroup();
    }

    lua_getglobal(L, "setOption");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_pushstring(L, cstring(option->mName));
    if (option->asBoolean())
        lua_pushboolean(L, value.toBool());
    else if (option->asEnum())
        lua_pushstring(L, cstring(value.toString()));
    else if (option->asInteger())
        lua_pushinteger(L, value.toInt());
    else if (option->asList())
        lua_pushinteger(L, value.toInt());
    else if (option->asString())
        lua_pushstring(L, cstring(value.toString()));
    else
        Q_ASSERT(false);

    int nargs = 2;
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);

    int status = lua_pcall(L, nargs, 0, base);
    lua_remove(L, base);

    if (status != LUA_OK) {
        mMapChanged = true; // discard any changes before the error occurred
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
}

void LuaTileTool::setCursorType(LuaTileTool::CursorType type)
{
    mCursorType = type;
}

LuaTileLayer *LuaTileTool::currentLayer() const
{
    int index = mapDocument()->currentLayerIndex();
    return (mMap && mMap->layerAt(index)) ? mMap->layerAt(index)->asTileLayer() : 0;
}

void LuaTileTool::mouseEvent(const char *func, Qt::MouseButtons buttons,
                             const QPointF &scenePos, Qt::KeyboardModifiers modifiers)
{
    if (!L) return;

    checkMap();

    lua_getglobal(L, func); // mouseMoved/mousePressed/mouseReleased
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L); // arg buttons
    if (buttons & Qt::LeftButton) {
        lua_pushstring(L, "left");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (buttons & Qt::RightButton) {
        lua_pushstring(L, "right");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    int level = mapDocument()->currentLevel();
    const QPointF tilePosF = mapDocument()->renderer()->pixelToTileCoords(scenePos, level);
    lua_pushnumber(L, tilePosF.x()); // arg x
    lua_pushnumber(L, tilePosF.y()); // arg y

    lua_newtable(L); // arg modifiers
    if (modifiers & Qt::AltModifier) {
        lua_pushstring(L, "alt");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ControlModifier) {
        lua_pushstring(L, "control");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }
    if (modifiers & Qt::ShiftModifier) {
        lua_pushstring(L, "shift");
        lua_pushboolean(L, 1);
        lua_settable(L, -3);
    }

    int nargs = 4;
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    int status = lua_pcall(L, nargs, 0, base);

    lua_remove(L, base);

    if (status != LUA_OK) {
        mMapChanged = true; // discard any changes before the error occurred
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }
}

void LuaTileTool::checkMap()
{
    if (mMap && !mMapChanged)
        return;

    delete mMap;

    mMap = new LuaMap(mapDocument()->map());
    tolua_pushusertype(L, mMap, "LuaMap");
    lua_setglobal(L, "map");

    mMapChanged = false;

    qDebug() << "LuaTileTool::checkMap(): recreated map";
}

static bool getBooleanFromTable(lua_State *L, const char *key, bool &b)
{
    tolua_Error err;
    if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err) == 1) {
        int tblidx = lua_gettop(L);
        lua_pushstring(L, key);
        lua_gettable(L, tblidx);
        if (lua_isnil(L, -1) || !lua_isboolean(L, -1)) {
            lua_pop(L, 1); // i
            return false;
        }
        b = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return true;
    }
    return false;
}

static bool getIntegerFromTable(lua_State *L, const char *key, int &i)
{
    tolua_Error err;
    if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err) == 1) {
        int tblidx = lua_gettop(L);
        lua_pushstring(L, key);
        lua_gettable(L, tblidx);
        if (lua_isnil(L, -1) || !lua_isnumber(L, -1)) {
            lua_pop(L, 1); // i
            return false;
        }
        int isnum;
        i = lua_tointegerx(L, -1, &isnum);
        lua_pop(L, 1);
        return isnum;
    }
    return false;
}

static bool getStringFromTable(lua_State *L, const char *key, QString &string)
{
    tolua_Error err;
    if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err) == 1) {
        int tblidx = lua_gettop(L);
        lua_pushstring(L, key);
        lua_gettable(L, tblidx);
        if (lua_isnil(L, -1) || !lua_isstring(L, -1)) {
            lua_pop(L, 1); // i
            return false;
        }
        string = QLatin1String(lua_tostring(L, -1));
        lua_pop(L, 1);
        return true;
    }
    return false;
}

static bool getStringsFromTable(lua_State *L, QStringList &strings)
{
    tolua_Error err;
    if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err) == 1) {
        int tblidx = lua_gettop(L);
        int len = luaL_len(L, tblidx);
        for (int i = 1; i <= len; ++i) {
            lua_pushnumber(L, i);
            lua_gettable(L, tblidx);
            if (lua_isnil(L, -1) || !lua_isstring(L, -1))
                return false;
            strings += QLatin1String(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        return true;
    }
    return false;
}

void LuaTileTool::setToolOptions()
{
    LuaToolDialog::instancePtr()->setToolOptions(0);
    mOptions.clear();

    if (!L) return;

    // Call the options() function.
    // It should return a table of subtables.
    // subtable = { name, type, args } where 'args' depends on type.

    lua_getglobal(L, "options");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    int nargs = 0, nret = 1;
    int base = lua_gettop(L) - nargs;
    lua_pushcfunction(L, traceback);
    lua_insert(L, base);
    int status = lua_pcall(L, nargs, nret, base);
    lua_remove(L, base);

    if (status != LUA_OK) {
        QString output = QString::fromLatin1(lua_tostring(L, -1));
        lua_pop(L, -1); // pop error
        LuaConsole::instance()->write(output, (status == LUA_OK) ? Qt::black : Qt::red);
    }

    if (status == LUA_OK) {
        tolua_Error err;
        if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err) == 1) {
            int tblidx = lua_gettop(L);
            int len = luaL_len(L, tblidx);
            for (int i = 1; i <= len; ++i) {
                lua_pushnumber(L, i);
                lua_gettable(L, tblidx);
                QString err;
                if (!parseToolOption(err)) {
                    lua_pop(L, 1 + nret);
                    LuaConsole::instance()->write(tr("error parsing result of options(): ") + err, Qt::red);
                    return;
                }
                lua_pop(L, 1);
            }
        }

        lua_pop(L, nret); // result of options()

        // We got a valid set of options, set the current values to those saved
        // in the settings (or the defaults).
        mSaveOptionValue = false;
        QSettings settings;
        settings.beginGroup(QLatin1String("LuaTileTool/Tool/")+QFileInfo(mFileName).completeBaseName());
        QMap<LuaToolOption*,QVariant> current;
        foreach (LuaToolOption *option, mOptions.mOptions) {
            if (BooleanLuaToolOption *o = option->asBoolean())
                current[option] = settings.value(o->mName, o->mDefault);
            else if (EnumLuaToolOption *o = option->asEnum()) {
                QString value = settings.value(o->mName, o->mDefault).toString();
                if (!o->mEnums.contains(value))
                    value = o->mDefault;
                current[option] = value;
            } else if (IntegerLuaToolOption *o = option->asInteger()) {
                int value = settings.value(o->mName, o->mDefault).toInt();
                current[option] = qBound(o->mMin, value, o->mMax);
            } else if (ListLuaToolOption *o = option->asList()) {
                int value = settings.value(o->mName, o->mDefault).toInt();
                current[option] = qBound(1, value, o->mItems.size());
            } else if (StringLuaToolOption *o = option->asString())
                current[option] = settings.value(o->mName, o->mDefault);
            else
                Q_ASSERT(false);
            setOption(option, current[option]);
        }
        settings.endGroup();
        mSaveOptionValue = true;

        LuaToolDialog::instancePtr()->setToolOptions(&mOptions);
        foreach (LuaToolOption *option, mOptions.mOptions)
            LuaToolDialog::instancePtr()->setToolOptionValue(option, current[option]);
    }
}

bool LuaTileTool::parseToolOption(QString &err)
{
    tolua_Error err2;
    if (lua_gettop(L) >= 1 && tolua_istable(L, -1, 0, &err2) == 1) {
        int tblidx = lua_gettop(L);
        QString name, label, type;
        if (!getStringFromTable(L, "name", name) || name.isEmpty()) {
            err = tr("missing 'name'");
            return false;
        }
        if (mOptions.option(name) != 0) {
            err = tr("duplicate option '%1'").arg(name);
            return false;
        }
        if (!getStringFromTable(L, "label", label) || label.isEmpty()) {
            err = tr("missing 'label'");
            return false;
        }
        if (!getStringFromTable(L, "type", type)) {
            err = tr("missing 'type' in option '%2'").arg(name);
            return false;
        }
        if (type == QLatin1String("bool")) {
            bool defaultValue;
            if (!getBooleanFromTable(L, "default", defaultValue)) {
                err = tr("bad 'default' in option '%1'").arg(name);
                return false;
            }
            mOptions.addBoolean(name, label, defaultValue);
        } else if (type == QLatin1String("enum")) {
            QStringList enumNames;
            lua_pushstring(L, "choices");
            lua_gettable(L, tblidx);
            if (!getStringsFromTable(L, enumNames) || enumNames.isEmpty() || enumNames.contains(QString())) {
                lua_pop(L, 1);
                err = tr("bad 'choices' in option '%1'").arg(name);
                return false;
            }
            lua_pop(L, 1);
            mOptions.addEnum(name, label, enumNames,
                             enumNames.size() ? enumNames.first() : QString());
        } else if (type == QLatin1String("int")) {
            int min, max, defaultValue;
            if (!getIntegerFromTable(L, "min", min)) {
                err = tr("bad 'min' in option '%1'").arg(name);
                return false;
            }
            if (!getIntegerFromTable(L, "max", max)) {
                err = tr("bad 'max' in option '%1'").arg(name);
                return false;
            }
            if (!getIntegerFromTable(L, "default", defaultValue)) {
                err = tr("bad 'default' in option '%1'").arg(name);
                return false;
            }
            mOptions.addInteger(name, label, min, max, defaultValue);
        } else if (type == QLatin1String("list")) {
            QStringList items;
            lua_pushstring(L, "items");
            lua_gettable(L, tblidx);
            if (!getStringsFromTable(L, items)) {
                lua_pop(L, 1);
                err = tr("bad 'items' in option '%1'").arg(name);
                return false;
            }
            lua_pop(L, 1);
            mOptions.addList(name, label, items, 1);
        } else if (type == QLatin1String("string")) {
            QString defaultValue;
            if (!getStringFromTable(L, "default", defaultValue)) {
                err = tr("bad 'default' in option '%1'").arg(name);
                return false;
            }
            mOptions.addString(name, label, defaultValue);
        } else {
            err = tr("unknown type '%1' in option '%2'").arg(type).arg(name);
            return false;
        }
        return true;
    }
    return false;
}

#include "mainwindow.h"
void LuaTileTool::applyChanges(const char *undoText)
{
#if 1
    MainWindow::instance()->ApplyScriptChanges(mapDocument(), QLatin1String(undoText), mMap);
#else
    QUndoStack *us = mapDocument()->undoStack();
    QList<QUndoCommand*> cmds;

    foreach (Lua::LuaLayer *ll, mMap->mLayers) {
        // Apply changes to tile layers.
        if (Lua::LuaTileLayer *tl = ll->asTileLayer()) {
            if (tl->mOrig == 0)
                continue; // Ignore new layers.
            if (!tl->mCloneTileLayer || tl->mAltered.isEmpty())
                continue; // No changes.
            TileLayer *source = tl->mCloneTileLayer->copy(tl->mAltered);
            QRect r = tl->mAltered.boundingRect();
            cmds += new PaintTileLayer(mapDocument(), tl->mOrig->asTileLayer(),
                                                   r.x(), r.y(), source, tl->mAltered, true);
            delete source;
        }
    }

    foreach (LuaMapNoBlend *nb, mMap->mNoBlends) {
        if (!nb->mAltered.isEmpty()) {
            cmds += new PaintNoBlend(mapDocument(), mapDocument()->map()->noBlend(nb->mClone->layerName()),
                                     nb->mClone->copy(nb->mAltered), nb->mAltered);
        }
    }

    if (cmds.size()) {
        us->beginMacro(QLatin1String(undoText));
        foreach (QUndoCommand *cmd, cmds)
            us->push(cmd);
        us->endMacro();
    }
#endif

    mMapChanged = true;
}

int LuaTileTool::angle(float x1, float y1, float x2, float y2)
{
    QLineF line(x1, y1, x2, y2);
    return line.angle();
}

void LuaTileTool::clearToolTiles()
{
    foreach (QString layerName, mToolTileLayers.keys()) {
        if (!mToolTileRegions[layerName].isEmpty()) {
            mToolTileLayers[layerName]->erase();
            int n = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
            if (n >= 0) {
                TileLayer *tl = mapDocument()->map()->layerAt(n)->asTileLayer();
                mapDocument()->mapComposite()->layerGroupForLayer(tl)->clearToolTiles();
                QRect r = mapDocument()->renderer()->boundingRect(mToolTileRegions[layerName].boundingRect(),
                                                                  tl->level()).adjusted(-3, -(128-32)*(mapDocument()->renderer()->is2x()?2:1) - 3, 3, 3);
                mScene->update(r);
            }
            mToolTileRegions[layerName] = QRegion();
        }
     }
}

void LuaTileTool::setToolTile(const char *layer, int x, int y, Tile *tile)
{
    if (tile == LuaMap::noneTile()) tile = 0;

    QString layerName = QLatin1String(layer);
    Map *map = mapDocument()->map();
    if (!mToolTileLayers.keys().contains(layerName))
        mToolTileLayers[layerName] = new Tiled::TileLayer(layerName, 0, 0,
                                                          map->width(),
                                                          map->height());

    TileLayer *tlTool = mToolTileLayers[layerName];
    if (tlTool->width() != map->width() || tlTool->height() != map->height()) {
        tlTool->resize(map->size(), QPoint());
        mToolTileRegions[layerName] &= QRect(QPoint(), map->size());
    }

    if (tlTool->contains(x, y)) {
        tlTool->setCell(x, y, Cell(tile));
        mToolTileRegions[layerName] += QRect(x, y, 1, 1);
        int n = map->indexOfLayer(layerName, Layer::TileLayerType);
        if (n >= 0) {
            TileLayer *tl = map->layerAt(n)->asTileLayer();
            mapDocument()->mapComposite()->layerGroupForLayer(tl)->setToolTiles(
                        tlTool, QPoint(), mToolTileRegions[layerName], tl);
            QRect r = mapDocument()->renderer()->boundingRect(
                        mToolTileRegions[layerName].boundingRect(),
                        tl->level()).adjusted(-3, -(128-32)*(mapDocument()->renderer()->is2x() ? 2 : 1) - 3, 3, 3);
            mScene->update(r);
        }
    }
}

void LuaTileTool::setToolTile(const char *layer, const QRegion &rgn, Tile *tile)
{
    foreach (QRect r, rgn.rects())
        for (int y = r.top(); y <= r.bottom(); y++)
            for (int x = r.left(); x <= r.right(); x++)
                setToolTile(layer, x, y, tile);
}

void LuaTileTool::clearToolNoBlends()
{
    foreach (QString layerName, mToolNoBlends.keys()) {
        if (!mToolNoBlendRegions[layerName].isEmpty()) {
            //mToolNoBlends[layerName]->clear();
            int n = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
            if (n >= 0) {
                TileLayer *tl = mapDocument()->map()->layerAt(n)->asTileLayer();
                mapDocument()->mapComposite()->layerGroupForLayer(tl)->clearToolNoBlends();
                QRect r = mapDocument()->renderer()->boundingRect(mToolNoBlendRegions[layerName].boundingRect(),
                                                                  tl->level()).adjusted(-3, -(128-32)*(mapDocument()->renderer()->is2x()?2:1) - 3, 3, 3);
                mScene->update(r);
            }
            mToolNoBlendRegions[layerName] = QRegion();
        }
     }
}

void LuaTileTool::setToolNoBlend(const char *layer, int x, int y, bool noBlend)
{
    QString layerName = QLatin1String(layer);
    Map *map = mapDocument()->map();
    if (!mToolNoBlends.keys().contains(layerName))
        mToolNoBlends[layerName] = new MapNoBlend(layerName, map->width(), map->height());

    MapNoBlend *nb = mToolNoBlends[layerName];
    QRegion &rgn = mToolNoBlendRegions[layerName];
    if (nb->width() != map->width() || nb->height() != map->height()) {
        delete mToolNoBlends[layerName];
        nb = mToolNoBlends[layerName] = new MapNoBlend(layerName, map->width(), map->height());
        rgn &= QRect(QPoint(), map->size());
    }

    if (QRect(QPoint(), map->size()).contains(x, y)) {
        nb->set(x, y, noBlend);
        rgn += QRect(x, y, 1, 1);
        int n = map->indexOfLayer(layerName, Layer::TileLayerType);
        if (n >= 0) {
            TileLayer *tl = map->layerAt(n)->asTileLayer();
            mapDocument()->mapComposite()->layerGroupForLayer(tl)->setToolNoBlend(
                        nb->copy(rgn), rgn.boundingRect().topLeft(), rgn, tl);
            QRect r = mapDocument()->renderer()->boundingRect(
                        rgn.boundingRect(),
                        tl->level()).adjusted(-3, -(128-32)*(mapDocument()->renderer()->is2x()?2:1) - 3, 3, 3);
            mScene->update(r);
        }
    }
}

void LuaTileTool::setToolNoBlend(const char *layer, const QRegion &rgn, bool noBlend)
{
    foreach (QRect r, rgn.rects())
        for (int y = r.top(); y <= r.bottom(); y++)
            for (int x = r.left(); x <= r.right(); x++)
                setToolNoBlend(layer, x, y, noBlend);
}

void LuaTileTool::clearDistanceIndicators()
{
    for (int i = 0; i < 4; i++) {
        if (mDistanceIndicators[i] && mDistanceIndicators[i]->isVisible()) {
            mScene->removeItem(mDistanceIndicators[i]);
            mDistanceIndicators[i]->hide();
        }
    }
}

void LuaTileTool::indicateDistance(int x1, int y1, int x2, int y2)
{
    int d = -1, dist = -1;
    DistanceIndicator::Dir dir;
    if (x1 > x2 && y1 == y2) d = 0, dir = DistanceIndicator::West, dist = x1 - x2;
    else if (x1 == x2 && y1 > y2) d = 1, dir = DistanceIndicator::North, dist = y1 - y2;
    else if (x1 < x2 && y1 == y2) d = 2, dir = DistanceIndicator::East, dist = x2 - x1;
    else if (x1 == x2 && y1 < y2) d = 3, dir = DistanceIndicator::South, dist = y2 - y1;
    if (d != -1) {
        if (!mDistanceIndicators[d]) {
            mDistanceIndicators[d] = new DistanceIndicator(dir);
            mDistanceIndicators[d]->setVisible(false);
        }
        mDistanceIndicators[d]->setRenderer(mapDocument()->renderer());
        mDistanceIndicators[d]->setInfo(QPoint(x1, y1), dist);
        if (!mDistanceIndicators[d]->isVisible()) {
            mScene->addItem(mDistanceIndicators[d]);
            mDistanceIndicators[d]->show();
        }
    }
}

bool LuaTileTool::dragged()
{
    int d = (mCurrentScenePos - mStartScenePos).manhattanLength();
    return d >= 4; //QApplication::startDragDistance();
}

QPainterPath LuaTileTool::cursorShape(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    int level = mapDocument()->currentLevel();
    QPointF tilePosF = renderer->pixelToTileCoords(pos, level);
    QPoint tilePos = QPoint(qFloor(tilePosF.x()), qFloor(tilePosF.y()));
    QPointF m(tilePosF.x() - tilePos.x(), tilePosF.y() - tilePos.y());
    qreal dW = m.x(), dN = m.y(), dE = 1.0 - dW, dS = 1.0 - dN;

    QPainterPath path;
    if (mCursorType == LuaTileTool::CurbTool) {
        qreal dx = 0, dy = 0;
        if (dE <= dW) dx = 0.5;
        if (dS <= dN) dy = 0.5;
        QPolygonF poly = renderer->tileToPixelCoords(QRectF(tilePos.x() + dx, tilePos.y() + dy, 0.5, 0.5), level);
        path.addPolygon(poly);
    }
    if (mCursorType == LuaTileTool::EdgeTool) {
        Edge edge;
        if (dW < dE) {
            edge = (dW < dN && dW < dS) ? EdgeW : ((dN < dS) ? EdgeN : EdgeS);
        } else {
            edge = (dE < dN && dE < dS) ? EdgeE : ((dN < dS) ? EdgeN : EdgeS);
        }

        QPolygonF polyN = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 1, 0.25), level);
        QPolygonF polyS = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y() + 0.75, 1, 0.25), level);
        QPolygonF polyW = renderer->tileToPixelCoords(QRectF(tilePos.x(), tilePos.y(), 0.25, 1), level);
        QPolygonF polyE = renderer->tileToPixelCoords(QRectF(tilePos.x() + 0.75, tilePos.y(), 0.25, 1), level);
        polyN += polyN.first(), polyS += polyS.first(), polyW += polyW.first(), polyE += polyE.first();
        if ((edge == EdgeN && dW < 0.5) || (edge == EdgeW && dN < 0.5))
            path.addPolygon(polyN), path.addPolygon(polyW);
        if ((edge == EdgeS && dW < 0.5) || (edge == EdgeW && dS <= 0.5))
            path.addPolygon(polyS), path.addPolygon(polyW);
        if ((edge == EdgeN && dE <= 0.5) || (edge == EdgeE && dN < 0.5))
            path.addPolygon(polyN), path.addPolygon(polyE);
        if ((edge == EdgeS && dE <= 0.5) || (edge == EdgeE && dS <= 0.5))
            path.addPolygon(polyS), path.addPolygon(polyE);
        path.setFillRule(Qt::WindingFill);
        path = path.simplified();
    }
    return path;
}

/////

#include "BuildingEditor/simplefile.h"

#include <QDir>
#include <QFileInfo>

LuaToolFile::LuaToolFile()
{
}

LuaToolFile::~LuaToolFile()
{
//    qDeleteAll(mScripts);
}

bool LuaToolFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mFileName = path;
    QDir dir(info.absoluteDir());

//    int version = simple.version();

    // There can be a LuaTools.txt in Preferences::configPath() and Preferences::appConfigPath().
    // In the former case, there should be a subdirectory "lua" containing the tool files.
    // In the latter case, we expect Preferences::luaPath() to contain the tool files.
    QDir luaDir = dir.filePath(QLatin1String("lua"));
    if (!luaDir.exists())
#if defined(Q_OS_MAC)
        luaDir = dir.filePath(QLatin1String("../Lua"));
#else
        luaDir = dir.filePath(QLatin1String("../lua"));
#endif

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tool")) {
            QStringList keys;
            keys << QLatin1String("label")
                 << QLatin1String("icon")
                 << QLatin1String("script")
                 << QLatin1String("dialog-title");
            if (!validKeys(block, keys))
                return false;

            LuaToolInfo info;
            info.mLabel = block.value("label");

            QString icon = block.value("icon");
            if (QFileInfo(icon).isRelative())
                icon = luaDir.filePath(icon);
            info.mIcon = QIcon(icon);

            info.mScript = block.value("script");
            if (QFileInfo(info.mScript).isRelative())
                info.mScript = luaDir.filePath(info.mScript);

            info.mDialogTitle = block.value("dialog-title");

            mTools += info;
        } else {
            mError = tr("Line %1: Unknown block name '%2'.\n%3")
                    .arg(block.lineNumber)
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

QList<LuaToolInfo> LuaToolFile::takeTools()
{
    QList<LuaToolInfo> ret = mTools;
    mTools.clear();
    return ret;
}

bool LuaToolFile::validKeys(SimpleFileBlock &block, const QStringList &keys)
{
    foreach (SimpleFileKeyValue kv, block.values) {
        if (!keys.contains(kv.name)) {
            mError = tr("Line %1: Unknown attribute '%2'.\n%3")
                    .arg(kv.lineNumber)
                    .arg(kv.name)
                    .arg(mFileName);
            return false;
        }
    }
    return true;
}

/////

UnscaledLabelItem::UnscaledLabelItem(QGraphicsItem *parent)
    : QGraphicsSimpleTextItem(parent)
{
    setFlag(ItemIgnoresTransformations);

    synch();
}

QRectF UnscaledLabelItem::boundingRect() const
{
    QRectF r = QGraphicsSimpleTextItem::boundingRect().adjusted(-3, -3, 2, 2);
    return r.translated(-r.center());
}

QPainterPath UnscaledLabelItem::shape() const
{
    QPainterPath path;
    path.addRect(boundingRect());
    return path;
}

bool UnscaledLabelItem::contains(const QPointF &point) const
{
    return boundingRect().contains(point);
}

void UnscaledLabelItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                            QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    mBgColor = Qt::white;
    mBgColor.setAlphaF(0.75);

    QRectF r = boundingRect();
    painter->fillRect(r, mBgColor);
    painter->drawText(r, Qt::AlignCenter, text());
}

void UnscaledLabelItem::synch()
{
}

/////

DistanceIndicator::DistanceIndicator(DistanceIndicator::Dir direction, QGraphicsItem *parent) :
    QGraphicsItem(parent),
    mDirection(direction),
    mRenderer(0),
    mTextItem(new UnscaledLabelItem(this))
{
}

void DistanceIndicator::setInfo(const QPoint &tilePos, int dist)
{
    prepareGeometryChange();
    mTilePos = tilePos;
    mDistance = dist;

    mTextItem->setText(QString::number(dist));
    int level = 0;
    QPointF centerW = mRenderer->tileToPixelCoords(mTilePos.x() - 0.5, mTilePos.y() + 0.5, level);
    QPointF centerN = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() - 0.5, level);
    QPointF centerE = mRenderer->tileToPixelCoords(mTilePos.x() + 1.5, mTilePos.y() + 0.5, level);
    QPointF centerS = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() + 1.5, level);
    QPointF pos;
    switch (mDirection) {
    case West: pos = centerW; break;
    case North: pos = centerN; break;
    case East: pos = centerE; break;
    case South: pos = centerS; break;
    }
    mTextItem->setPos(pos);
}

void DistanceIndicator::setRenderer(MapRenderer *renderer)
{
    mRenderer = renderer;
}

QRectF DistanceIndicator::boundingRect() const
{
    return mRenderer ? mRenderer->boundingRect(QRect(mTilePos, QSize(1, 1))) : QRectF();
}

void DistanceIndicator::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!mRenderer) return;

    int level = 0;
    QPointF topLeft = mRenderer->tileToPixelCoords(mTilePos.x(), mTilePos.y(), level);
    QPointF topRight = mRenderer->tileToPixelCoords(mTilePos.x() + 1, mTilePos.y(), level);
    QPointF bottomRight = mRenderer->tileToPixelCoords(mTilePos.x() + 1, mTilePos.y() + 1, level);
    QPointF bottomLeft = mRenderer->tileToPixelCoords(mTilePos.x(), mTilePos.y() + 1, level);
    QPointF centerW = mRenderer->tileToPixelCoords(mTilePos.x() - 0.5, mTilePos.y() + 0.5, level);
    QPointF centerN = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() - 0.5, level);
    QPointF centerE = mRenderer->tileToPixelCoords(mTilePos.x() + 1.5, mTilePos.y() + 0.5, level);
    QPointF centerS = mRenderer->tileToPixelCoords(mTilePos.x() + 0.5, mTilePos.y() + 1.5, level);

    QPolygonF poly;
    switch (mDirection) {
    case West: poly << centerW << topLeft << bottomLeft; break;
    case North: poly << centerN << topLeft << topRight; break;
    case East: poly << centerE << topRight << bottomRight; break;
    case South: poly << centerS << bottomLeft << bottomRight; break;
    }
    poly << poly.first();

    painter->setPen(QPen(Qt::lightGray, 2));
    painter->setBrush(Qt::gray);
    painter->drawPolygon(poly);
}
