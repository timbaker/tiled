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
    QString Name;
    BuildingTileEntry *Wall;
    BuildingTileEntry *DoorTile;
    BuildingTileEntry *DoorFrameTile;
    BuildingTileEntry *WindowTile;
    BuildingTileEntry *CurtainsTile;
    BuildingTileEntry *StairsTile;
    BuildingTileEntry *RoofCap;
    BuildingTileEntry *RoofSlope;
    BuildingTileEntry *RoofTop;

    QList<Room*> RoomList;

    BuildingTemplate()
    {}

    BuildingTemplate(BuildingTemplate *other)
    {
        Name = other->Name;
        Wall = other->Wall;
        DoorTile = other->DoorTile;
        DoorFrameTile = other->DoorFrameTile;
        WindowTile = other->WindowTile;
        CurtainsTile = other->CurtainsTile;
        StairsTile = other->StairsTile;
        RoofCap = other->RoofCap;
        RoofSlope = other->RoofSlope;
        RoofTop = other->RoofTop;

        foreach (Room *room, other->RoomList)
            RoomList += new Room(room);
    }

    ~BuildingTemplate()
    {
        qDeleteAll(RoomList);
    }
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
