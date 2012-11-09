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

#include "buildingwriter.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

using namespace BuildingEditor;

class BuildingEditor::BuildingWriterPrivate
{
    Q_DECLARE_TR_FUNCTIONS(BuildingWriterPrivate)

public:
    BuildingWriterPrivate()
        : mBuilding(0)
    {
    }

    bool openFile(QFile *file)
    {
        if (!file->open(QIODevice::WriteOnly)) {
            mError = tr("Could not open file for writing.");
            return false;
        }

        return true;
    }

    void writeBuilding(Building *building, QIODevice *device, const QString &absDirPath)
    {
        mMapDir = QDir(absDirPath);
        mBuilding = building;

        QXmlStreamWriter writer(device);
        writer.setAutoFormatting(true);
        writer.setAutoFormattingIndent(1);

        writer.writeStartDocument();

        writeBuilding(writer, building);

        writer.writeEndDocument();
    }

    void writeBuilding(QXmlStreamWriter &w, Building *building)
    {
        w.writeStartElement(QLatin1String("building"));

        w.writeAttribute(QLatin1String("version"), QLatin1String("1.0"));
        w.writeAttribute(QLatin1String("width"), QString::number(building->width()));
        w.writeAttribute(QLatin1String("height"), QString::number(building->height()));

        w.writeAttribute(QLatin1String("ExteriorWall"), building->exteriorWall()->name());
        w.writeAttribute(QLatin1String("Door"), building->doorTile()->name());
        w.writeAttribute(QLatin1String("DoorFrame"), building->doorFrameTile()->name());
        w.writeAttribute(QLatin1String("Window"), building->windowTile()->name());
        w.writeAttribute(QLatin1String("Stairs"), building->stairsTile()->name());

        writeFurniture(w);

        foreach (Room *room, building->rooms())
            writeRoom(w, room);

        foreach (BuildingFloor *floor, building->floors())
            writeFloor(w, floor);

        w.writeEndElement(); // </building>
    }

    void writeRoom(QXmlStreamWriter &w, Room *room)
    {
        w.writeStartElement(QLatin1String("room"));
        w.writeAttribute(QLatin1String("Name"), room->Name);
        w.writeAttribute(QLatin1String("InternalName"), room->internalName);
        QString colorString = QString(QLatin1String("%1 %2 %3"))
                .arg(qRed(room->Color))
                .arg(qGreen(room->Color))
                .arg(qBlue(room->Color));
        w.writeAttribute(QLatin1String("Color"), colorString);
        w.writeAttribute(QLatin1String("InteriorWall"), room->Wall->name());
        w.writeAttribute(QLatin1String("Floor"), room->Floor->name());
        w.writeEndElement(); // </room>
    }

    void writeFurniture(QXmlStreamWriter &w)
    {
        foreach (BuildingFloor *floor, mBuilding->floors()) {
            foreach (BuildingObject *object, floor->objects()) {
                if (FurnitureObject *furniture = object->asFurniture()) {
                    FurnitureTiles *ftiles = furniture->furnitureTile()->owner();
                    if (!mFurnitureTiles.contains(ftiles))
                        mFurnitureTiles += ftiles;
                }
            }
        }

        foreach (FurnitureTiles *ftiles, mFurnitureTiles) {
            w.writeStartElement(QLatin1String("furniture"));
            writeFurnitureTile(w, ftiles->mTiles[0]);
            writeFurnitureTile(w, ftiles->mTiles[1]);
            writeFurnitureTile(w, ftiles->mTiles[2]);
            writeFurnitureTile(w, ftiles->mTiles[3]);
            w.writeEndElement(); // </furniture>
        }
    }

    void writeFurnitureTile(QXmlStreamWriter &w, FurnitureTile *ftile)
    {
        w.writeStartElement(QLatin1String("entry"));
        w.writeAttribute(QLatin1String("orient"), ftile->orientToString());
        writeFurnitureTile(w, 0, 0, ftile->mTiles[0]);
        writeFurnitureTile(w, 1, 0, ftile->mTiles[1]);
        writeFurnitureTile(w, 0, 1, ftile->mTiles[2]);
        writeFurnitureTile(w, 1, 1, ftile->mTiles[3]);
        w.writeEndElement(); // </entry>
    }

    void writeFurnitureTile(QXmlStreamWriter &w, int x, int y, BuildingTile *btile)
    {
        if (!btile)
            return;
        w.writeStartElement(QLatin1String("tile"));
        w.writeAttribute(QLatin1String("x"), QString::number(x));
        w.writeAttribute(QLatin1String("y"), QString::number(y));
        w.writeAttribute(QLatin1String("name"), btile->name());
        w.writeEndElement(); // </tile>
    }

    void writeFloor(QXmlStreamWriter &w, BuildingFloor *floor)
    {
        w.writeStartElement(QLatin1String("floor"));
//        w.writeAttribute(QLatin1String("level"), QString::number(floor->level()));

        foreach (BuildingObject *object, floor->objects())
            writeObject(w, object);

        QString text;
        text += QLatin1Char('\n');
        int count = 0, max = floor->height() * floor->width();
        for (int y = 0; y < floor->height(); y++) {
            for (int x = 0; x < floor->width(); x++) {
                if (Room *room = floor->GetRoomAt(x, y))
                    text += QString::number(mBuilding->rooms().indexOf(room) + 1);
                else
                    text += QLatin1Char('0');
                if (++count < max)
                    text += QLatin1Char(',');
            }
            text += QLatin1Char('\n');
        }
        w.writeStartElement(QLatin1String("rooms"));
        w.writeCharacters(text);
        w.writeEndElement();

        w.writeEndElement(); // </floor>
    }

    void writeObject(QXmlStreamWriter &w, BuildingObject *object)
    {
        w.writeStartElement(QLatin1String("object"));
        bool writeDir = true, writeTile = true;
        if (Door *door = object->asDoor()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("door"));
            w.writeAttribute(QLatin1String("FrameTile"), door->frameTile()->name());
        } else if (object->asWindow())
            w.writeAttribute(QLatin1String("type"), QLatin1String("window"));
        else if (object->asStairs())
            w.writeAttribute(QLatin1String("type"), QLatin1String("stairs"));
        else if (FurnitureObject *furniture = object->asFurniture()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("furniture"));

            FurnitureTile *ftile = furniture->furnitureTile();
            int index = mFurnitureTiles.indexOf(ftile->owner());
            w.writeAttribute(QLatin1String("FurnitureTiles"), QString::number(index));
            w.writeAttribute(QLatin1String("orient"), ftile->orientToString());

            writeDir = false;
            writeTile = false;
        } else if (RoofObject *roof = object->asRoof()) {
            w.writeAttribute(QLatin1String("type"), QLatin1String("roof"));
            w.writeAttribute(QLatin1String("width"), QString::number(roof->width()));
            w.writeAttribute(QLatin1String("height"), QString::number(roof->height()));
            w.writeAttribute(QLatin1String("RoofType"), roof->typeToString());
            w.writeAttribute(QLatin1String("Depth"), roof->depthToString());
            writeBoolean(w, QLatin1String("cappedW"), roof->isCappedW());
            writeBoolean(w, QLatin1String("cappedN"), roof->isCappedN());
            writeBoolean(w, QLatin1String("cappedE"), roof->isCappedE());
            writeBoolean(w, QLatin1String("cappedS"), roof->isCappedS());
            if (roof->capTile())
                w.writeAttribute(QLatin1String("CapTile"), roof->capTile()->name());
            writeDir = false;
        } else {
            qFatal("Unhandled object type in BuildingWriter::writeObject");
        }
        w.writeAttribute(QLatin1String("x"), QString::number(object->x()));
        w.writeAttribute(QLatin1String("y"), QString::number(object->y()));
        if (writeDir)
            w.writeAttribute(QLatin1String("dir"), object->dirString());
        if (writeTile)
            w.writeAttribute(QLatin1String("Tile"), object->tile()->name());
        w.writeEndElement(); // </object>
    }

    void writeBoolean(QXmlStreamWriter &w, const QString &name, bool value)
    {
        w.writeAttribute(name, value ? QLatin1String("true") : QLatin1String("false"));
    }

    Building *mBuilding;
    QString mError;
    QDir mMapDir;
    QList<FurnitureTiles*> mFurnitureTiles;
};

/////

BuildingWriter::BuildingWriter()
    : d(new BuildingWriterPrivate)
{
}

BuildingWriter::~BuildingWriter()
{
    delete d;
}

bool BuildingWriter::write(Building *building, const QString &filePath)
{
    QTemporaryFile tempFile;
    if (!d->openFile(&tempFile))
        return false;

    write(building, &tempFile, QFileInfo(filePath).absolutePath());

    if (tempFile.error() != QFile::NoError) {
        d->mError = tempFile.errorString();
        return false;
    }

    // foo.tbx -> foo.tbx.bak
    QFileInfo destInfo(filePath);
    QString backupPath = filePath + QLatin1String(".bak");
    QFile backupFile(backupPath);
    if (destInfo.exists()) {
        if (backupFile.exists()) {
            if (!backupFile.remove()) {
                d->mError = QString(QLatin1String("Error deleting file!\n%1\n\n%2"))
                        .arg(backupPath)
                        .arg(backupFile.errorString());
                return false;
            }
        }
        QFile destFile(filePath);
        if (!destFile.rename(backupPath)) {
            d->mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                    .arg(filePath)
                    .arg(backupPath)
                    .arg(destFile.errorString());
            return false;
        }
    }

    // /tmp/tempXYZ -> foo.tbx
    tempFile.close();
    if (!tempFile.rename(filePath)) {
        d->mError = QString(QLatin1String("Error renaming file!\nFrom: %1\nTo: %2\n\n%3"))
                .arg(tempFile.fileName())
                .arg(filePath)
                .arg(tempFile.errorString());
        // Try to un-rename the backup file
        if (backupFile.exists())
            backupFile.rename(filePath); // might fail
        return false;
    }

    // If anything above failed, the temp file should auto-remove, but not after
    // a successful save.
    tempFile.setAutoRemove(false);

    return true;
}

void BuildingWriter::write(Building *building, QIODevice *device, const QString &absDirPath)
{
    d->writeBuilding(building, device, absDirPath);
}

QString BuildingWriter::errorString() const
{
    return d->mError;
}
