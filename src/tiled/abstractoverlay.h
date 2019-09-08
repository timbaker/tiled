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

#ifndef ABSTRACTOVERLAY_H
#define ABSTRACTOVERLAY_H

#include <QStringList>

class AbstractOverlay;

class AbstractOverlayEntry
{
public:
    virtual ~AbstractOverlayEntry() {}
    virtual AbstractOverlay *parent() const = 0;
    virtual void setRoomName(const QString& roomName) = 0;
    virtual QString roomName() const = 0;
    virtual QStringList &tiles() = 0;
    virtual int indexOf() const = 0;
};

class AbstractOverlay
{
public:
    virtual ~AbstractOverlay() {}
    virtual void setTileName(const QString& tileName) = 0;
    virtual QString tileName() = 0;
    virtual int entryCount() const = 0;
    virtual AbstractOverlayEntry* entry(int index) const = 0;
    virtual void insertEntry(int index, AbstractOverlayEntry *entry) = 0;
    virtual AbstractOverlayEntry *removeEntry(int index) = 0;
};

#endif // ABSTRACTOVERLAY_H
