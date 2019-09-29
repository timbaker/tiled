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

#include "luatable.h"

LuaTableKeyValue::~LuaTableKeyValue() { delete t; }

LuaTableKeyValue *LuaTable::find(const QString &key)
{
    for (LuaTableKeyValue* keyValue : kv) {
        if (keyValue->key == key) {
            return keyValue;
        }
    }
    return nullptr;
}

bool LuaTable::getString(const QString &key, QString &out)
{
    if (LuaTableKeyValue* kv = find(key)) {
        if (kv->t == nullptr) {
            out = kv->s;
            return true;
        }
    }
    return false;
}

LuaTable *LuaTable::getTable(const QString &key)
{
    if (LuaTableKeyValue* kv = find(key)) {
        return kv->t;
    }
    return nullptr;
}
