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
#include <QString>
#include <QStringList>
#include <QRgb>

namespace BuildingEditor {

class BuildingTile;

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
    BuildingTile *Floor;
    BuildingTile *Wall;
};

class BuildingTemplate
{
public:
    QString Name;
    BuildingTile *Wall;
    BuildingTile *DoorTile;
    BuildingTile *DoorFrameTile;
    BuildingTile *WindowTile;
    BuildingTile *StairsTile;

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
        StairsTile = other->StairsTile;

        foreach (Room *room, other->RoomList)
            RoomList += new Room(room);
    }

    ~BuildingTemplate()
    {
        qDeleteAll(RoomList);
    }
};

class BuildingTemplates
{
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

    void writeBuildingTemplatesTxt();

private:
    static BuildingTemplates *mInstance;
    QList<BuildingTemplate*> mTemplates;
};

} // namespace BuildingEditor

#endif // BUILDINGTEMPLATES_H
