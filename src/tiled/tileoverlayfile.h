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

#ifndef TILEOVERLAYFILE_H
#define TILEOVERLAYFILE_H

#include "abstractoverlay.h"

#include <QStringList>

class LuaTable;

class TileOverlay;

class TileOverlayEntry : public AbstractOverlayEntry
{
public:
    AbstractOverlay *parent() const override;
    void setRoomName(const QString& roomName) override { mRoomName = roomName; }
    QString roomName() const override { return mRoomName; }
    QStringList &tiles() override { return mTiles; }
    virtual void setUsage(const QString& usage) override { mUsage = usage; }
    virtual QString usage() const override { return mUsage; };
    int indexOf() const override;

    TileOverlay *mParent;
    QString mRoomName;
    int mChance = 1;
    QStringList mTiles;
    QString mUsage;
};

class TileOverlay : public AbstractOverlay
{
public:
    void setTileName(const QString& tileName) override { mTileName = tileName; }
    QString tileName() override { return mTileName; }
    int entryCount() const override { return mEntries.size(); }
    AbstractOverlayEntry *entry(int index) const override
    {
        return (index >= 0 && index < mEntries.size()) ? mEntries[index] : nullptr;
    }
    void insertEntry(int index, AbstractOverlayEntry *entry) override { mEntries.insert(index, static_cast<TileOverlayEntry*>(entry)); }
    TileOverlayEntry *removeEntry(int index) override { return mEntries.takeAt(index); }

    QString mTileName;
    QList<TileOverlayEntry*> mEntries;
};

class TileOverlayFile
{
public:
    TileOverlayFile();
    ~TileOverlayFile() { qDeleteAll(mOverlays); }

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<TileOverlay*> &overlays);

    QList<TileOverlay*> takeOverlays();

    QString errorString() { return mError; }

private:
    bool readV0(LuaTable* table);
    bool readV1(LuaTable* table);

public:
    QList<TileOverlay*> mOverlays;
    QString mError;
};

#endif // TILEOVERLAYFILE_H
