/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGTEMPLATES_H
#define BUILDINGTEMPLATES_H

#include <QList>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QRgb>
#include <QVector>

namespace BuildingEditor {

class BuildingTileCategory;
class BuildingTileEntry;

class Room
{
public:
    Room()
    {
    }

    Room(const Room *room)
    {
        *this = *room;
    }

    void copy(const Room *other)
    {
        *this = *other;
    }

    bool operator != (const Room &other)
    {
        return !(Name == other.Name &&
                 Color == other.Color &&
                 internalName == other.internalName &&
                 Floor == other.Floor &&
                 Wall == other.Wall);
    }

    QString Name;
    QRgb Color;
    QString internalName;
    BuildingTileEntry *Floor;
    BuildingTileEntry *Wall;
};

class BuildingTemplate
{
public:
    // Must match Building::Tiles enum
    enum Tiles {
        ExteriorWall,
        Door,
        DoorFrame,
        Window,
        Curtains,
        Stairs,
        RoofCap,
        RoofSlope,
        RoofTop,
        TileCount
    };

    BuildingTemplate();

    BuildingTemplate(BuildingTemplate *other)
    {
        Name = other->Name;
        mTiles = other->mTiles;
        foreach (Room *room, other->RoomList)
            RoomList += new Room(room);
    }

    ~BuildingTemplate()
    {
        qDeleteAll(RoomList);
    }

    void setName(const QString &name)
    { Name = name; }

    QString name() const
    { return Name; }

    void setTile(int n, BuildingTileEntry *entry);
    void setTiles(const QVector<BuildingTileEntry*> &entries);

    BuildingTileEntry *tile(int n)
    { return mTiles[n]; }

    const QVector<BuildingTileEntry*> &tiles() const
    { return mTiles; }

    static int categoryEnum(int n);

    void addRoom(Room *room)
    { RoomList += room; }

    void clearRooms()
    { qDeleteAll(RoomList); RoomList.clear(); }

    const QList<Room*> &rooms() const
    { return RoomList; }

    static QString enumToString(int n);

private:
    static void initNames();

private:
    QString Name;
    QVector<BuildingTileEntry*> mTiles;
    static QStringList mEnumNames;
    QList<Room*> RoomList;
};

class BuildingTemplates : public QObject
{
    Q_OBJECT
public:
    static BuildingTemplates *instance();
    static void deleteInstance();

    BuildingTemplates();
    ~BuildingTemplates();

    void addTemplate(BuildingTemplate *btemplate);

    void replaceTemplates(const QList<BuildingTemplate *> &templates);

    int templateCount() const
    { return mTemplates.count(); }

    const QList<BuildingTemplate*> &templates() const
    { return mTemplates; }

    BuildingTemplate *templateAt(int index) const
    { return mTemplates.at(index); }

    QString txtName();
    QString txtPath();

    bool readTxt();
    void writeTxt(QWidget *parent = 0);

    QString errorString() const
    { return mError; }

private:
    bool upgradeTxt();

    void addEntry(BuildingTileEntry *entry);
    QString entryIndex(BuildingTileEntry *entry);
    BuildingTileEntry *getEntry(const QString &s);

    void addEntry(BuildingTileCategory *category, const QString &tileName);
    QString entryIndex(BuildingTileCategory *category, const QString &tileName);

private:
    static BuildingTemplates *mInstance;
    QList<BuildingTemplate*> mTemplates;
    QString mError;

    QList<BuildingTileEntry*> mEntries; // Used during readTxt()/writeTxt()
    QMap<QString,BuildingTileEntry*> mEntriesByCategoryName;
    QMap<QPair<BuildingTileCategory*,QString>,BuildingTileEntry*> mEntryMap;
};

} // namespace BuildingEditor

#endif // BUILDINGTEMPLATES_H
