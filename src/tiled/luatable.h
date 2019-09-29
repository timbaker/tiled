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

#ifndef LUATABLE_H
#define LUATABLE_H

#include <QList>

class LuaTable;

class LuaTableKeyValue
{
public:
    LuaTableKeyValue(QString &key, QString value) : key(key), s(value), t(nullptr) {}
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

    LuaTableKeyValue* find(const QString& key);
    bool getString(const QString& key, QString& out);
    LuaTable* getTable(const QString& key);

    QList<LuaTableKeyValue*> kv;
};

#endif // LUATABLE_H
