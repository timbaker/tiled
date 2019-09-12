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

#ifndef CONTAINEROVERLAYFILE_H
#define CONTAINEROVERLAYFILE_H

#include "abstractoverlay.h"

#include <QStringList>

class LuaTable;

class ContainerOverlay;

class ContainerOverlayEntry : public AbstractOverlayEntry
{
public:
    AbstractOverlay *parent() const override;
    void setRoomName(const QString& roomName) { mRoomName = roomName; }
    QString roomName() const override { return mRoomName; }
    QStringList &tiles() override { return mTiles; }
    virtual void setUsage(const QString& usage) override { mUsage = usage; }
    virtual QString usage() const override { return mUsage; };
    int indexOf() const override;

    ContainerOverlay *mParent;
    QString mRoomName;
    QStringList mTiles;
    QString mUsage;
};

class ContainerOverlay : public AbstractOverlay
{
public:
    void setTileName(const QString& tileName) { mTileName = tileName; }
    QString tileName() override { return mTileName; }
    int entryCount() const override { return mEntries.size(); }
    AbstractOverlayEntry *entry(int index) const override
    {
        return (index >= 0 && index < mEntries.size()) ? mEntries[index] : nullptr;
    }
    void insertEntry(int index, AbstractOverlayEntry *entry) { mEntries.insert(index, static_cast<ContainerOverlayEntry*>(entry)); }
    AbstractOverlayEntry *removeEntry(int index) { return mEntries.takeAt(index); }

    QString mTileName;
    QList<ContainerOverlayEntry*> mEntries;
};

class ContainerOverlayFile
{
public:
    ContainerOverlayFile();
    ~ContainerOverlayFile() { qDeleteAll(mOverlays); }

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<ContainerOverlay*> overlays);

    QList<ContainerOverlay*> takeOverlays();

    QString errorString() { return mError; }

private:
    bool readV0(LuaTable* table);
    bool readV1(LuaTable* table);

private:
    QList<ContainerOverlay*> mOverlays;
    QString mError;
};

#endif // CONTAINEROVERLAYFILE_H
