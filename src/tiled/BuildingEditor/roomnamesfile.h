/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#ifndef ROOMNAMESFILE_H
#define ROOMNAMESFILE_H

#include <QColor>
#include <QStringList>

#include <set>

namespace BuildingEditor {

extern bool compareQColors(const QColor& a, const QColor& b);

class RoomName
{
public:
    QString label;
    QString internalName;
    QColor color;
};

class RoomNamesFile
{
public:
    RoomNamesFile();

    bool read(const QString &filePath);

    const QList<RoomName>& roomNames() const
    { return mRoomNames; }

    const std::set<QColor, decltype(&compareQColors)> &colorSet() const
    { return mRoomColorSet; }

    const QStringList internalNames() const;

    const QString errorString() const
    { return mError; }

private:
    QList<RoomName> mRoomNames;
    std::set<QColor, decltype(&compareQColors)> mRoomColorSet;
    QString mError;
};

} // namespace BuildingEditor

#endif // ROOMNAMESFILE_H
