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

#include "buildingreader.h"

#include "building.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QXmlStreamReader>

using namespace BuildingEditor;

class BuildingEditor::BuildingReaderPrivate
{
    Q_DECLARE_TR_FUNCTIONS(BuildingReaderPrivate)

public:
    BuildingReaderPrivate(BuildingReader *reader):
        p(reader),
        mBuilding(0)
    {}

    Building *readBuilding(QIODevice *device, const QString &path);

    bool openFile(QFile *file);

    QString errorString() const;

private:
    void readUnknownElement();

    Building *readBuilding();

    FurnitureTiles *readFurnitureTiles();
    FurnitureTile *readFurnitureTile(FurnitureTiles *ftiles);
    BuildingTile *readFurnitureTile(FurnitureTile *ftile, QPoint &pos);

    Room *readRoom();

    BuildingFloor *readFloor();
    void decodeCSVFloorData(BuildingFloor *floor, const QString &text);
    Room *getRoom(BuildingFloor *floor, int x, int y, int index);

    BuildingObject *readObject(BuildingFloor *floor);

    BuildingReader *p;

    QString mError;
    QString mPath;
    Building *mBuilding;
    QList<FurnitureTiles*> mFurnitureTiles;

    QXmlStreamReader xml;
};

bool BuildingReaderPrivate::openFile(QFile *file)
{
    if (!file->exists()) {
        mError = tr("File not found: %1").arg(file->fileName());
        return false;
    } else if (!file->open(QFile::ReadOnly | QFile::Text)) {
        mError = tr("Unable to read file: %1").arg(file->fileName());
        return false;
    }

    return true;
}

QString BuildingReaderPrivate::errorString() const
{
    if (!mError.isEmpty()) {
        return mError;
    } else {
        return tr("%3\n\nLine %1, column %2")
                .arg(xml.lineNumber())
                .arg(xml.columnNumber())
                .arg(xml.errorString());
    }
}

Building *BuildingReaderPrivate::readBuilding(QIODevice *device, const QString &path)
{
    mError.clear();
    mPath = path;
    Building *building = 0;

    xml.setDevice(device);

    if (xml.readNextStartElement() && xml.name() == "building") {
        building = readBuilding();
    } else {
        xml.raiseError(tr("Not a building file."));
    }

    return building;
}

Building *BuildingReaderPrivate::readBuilding()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "building");

    const QXmlStreamAttributes atts = xml.attributes();
    const int width =
            atts.value(QLatin1String("width")).toString().toInt();
    const int height =
            atts.value(QLatin1String("height")).toString().toInt();

    const QString exteriorWall = atts.value(QLatin1String("ExteriorWall")).toString();
    const QString door = atts.value(QLatin1String("Door")).toString();
    const QString doorFrame = atts.value(QLatin1String("DoorFrame")).toString();
    const QString window = atts.value(QLatin1String("Window")).toString();
    const QString stairs = atts.value(QLatin1String("Stairs")).toString();

    mBuilding = new Building(width, height);
    mBuilding->setExteriorWall(BuildingTiles::instance()->getExteriorWall(exteriorWall));
    mBuilding->setDoorTile(BuildingTiles::instance()->getDoorTile(door));
    mBuilding->setDoorFrameTile(BuildingTiles::instance()->getDoorFrameTile(doorFrame));
    mBuilding->setWindowTile(BuildingTiles::instance()->getWindowTile(window));
    mBuilding->setStairsTile(BuildingTiles::instance()->getStairsTile(stairs));

    while (xml.readNextStartElement()) {
        if (xml.name() == "furniture") {
            if (FurnitureTiles *tiles = readFurnitureTiles())
                mFurnitureTiles += tiles;
        } else if (xml.name() == "room") {
            if (Room *room = readRoom())
                mBuilding->insertRoom(mBuilding->roomCount(), room);
        } else if (xml.name() == "floor") {
            if (BuildingFloor *floor = readFloor())
                mBuilding->insertFloor(mBuilding->floorCount(), floor);
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete mBuilding;
        mBuilding = 0;
    }

    return mBuilding;
}

FurnitureTiles *BuildingReaderPrivate::readFurnitureTiles()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "furniture");

    FurnitureTiles *ftiles = new FurnitureTiles;

    while (xml.readNextStartElement()) {
        if (xml.name() == "entry") {
            if (FurnitureTile *ftile = readFurnitureTile(ftiles))
                ftiles->mTiles[ftiles->orientIndex(ftile->mOrient)] = ftile;
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftiles;
        return 0;
    }

    return ftiles;
}

FurnitureTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTiles *ftiles)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "entry");

    const QXmlStreamAttributes atts = xml.attributes();
    QString orientString = atts.value(QLatin1String("orient")).toString();
    FurnitureTile::FurnitureOrientation orient =
            FurnitureGroups::orientFromString(orientString);
    if (orient == FurnitureTile::FurnitureUnknown) {
        xml.raiseError(tr("invalid furniture tile orientation '%1'").arg(orientString));
        return 0;
    }

    FurnitureTile *ftile = new FurnitureTile(ftiles, orient);

    while (xml.readNextStartElement()) {
        if (xml.name() == "tile") {
            QPoint pos;
            if (BuildingTile *btile = readFurnitureTile(ftile, pos))
                ftile->mTiles[pos.x() + pos.y() * 2] = btile;
        } else
            readUnknownElement();
    }

    // Clean up in case of error
    if (xml.hasError()) {
        delete ftile;
        return 0;
    }

    return ftile;
}

BuildingTile *BuildingReaderPrivate::readFurnitureTile(FurnitureTile *ftile, QPoint &pos)
{
    Q_UNUSED(ftile)
    Q_ASSERT(xml.isStartElement() && xml.name() == "tile");
    const QXmlStreamAttributes atts = xml.attributes();
    int x = atts.value(QLatin1String("x")).toString().toInt();
    int y = atts.value(QLatin1String("y")).toString().toInt();
    if (x < 0 || y < 0) {
        xml.raiseError(tr("invalid furniture tile coordinates (%1,%2)")
                       .arg(x).arg(y));
    }
    pos.setX(x);
    pos.setY(y);
    QString tileName = atts.value(QLatin1String("name")).toString();
    BuildingTile *btile = BuildingTiles::instance()->getFurnitureTile(tileName);
    xml.skipCurrentElement();
    return btile;
}

Room *BuildingReaderPrivate::readRoom()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "room");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString name = atts.value(QLatin1String("Name")).toString();
    const QString internalName = atts.value(QLatin1String("InternalName")).toString();
    const QString color = atts.value(QLatin1String("Color")).toString();
    const QString interiorWall = atts.value(QLatin1String("InteriorWall")).toString();
    const QString floor = atts.value(QLatin1String("Floor")).toString();

    Room *room = new Room();
    room->Name = name;
    room->internalName = internalName;
    QStringList rgb = color.split(QLatin1Char(' '), QString::SkipEmptyParts);
    room->Color = qRgb(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt());
    room->Wall = BuildingTiles::instance()->getInteriorWall(interiorWall);
    room->Floor = BuildingTiles::instance()->getFloorTile(floor);

    xml.skipCurrentElement();

    return room;
}

BuildingFloor *BuildingReaderPrivate::readFloor()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "floor");

    BuildingFloor *floor = new BuildingFloor(mBuilding, mBuilding->floorCount());

    while (xml.readNextStartElement()) {
        if (xml.name() == "object") {
            if (BuildingObject *object = readObject(floor))
                floor->insertObject(floor->objectCount(), object);
        } else if (xml.name() == "rooms") {
            while (xml.readNext() != QXmlStreamReader::Invalid) {
                if (xml.isEndElement())
                    break;
                if (xml.isCharacters() && !xml.isWhitespace()) {
                    decodeCSVFloorData(floor, xml.text().toString());
                }
            }
        } else
            readUnknownElement();
    }

    return floor;
}

BuildingObject *BuildingReaderPrivate::readObject(BuildingFloor *floor)
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "object");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString type = atts.value(QLatin1String("type")).toString();
    const int x = atts.value(QLatin1String("x")).toString().toInt();
    const int y = atts.value(QLatin1String("y")).toString().toInt();
    const QString dirString = atts.value(QLatin1String("dir")).toString();
    const QString tile = atts.value(QLatin1String("Tile")).toString();

    if (x < 0 || x >= mBuilding->width() + 1 || y < 0 || y >= mBuilding->height() + 1) {
        xml.raiseError(tr("Invalid object coordinates (%1,%2")
                       .arg(x).arg(y));
        return 0;
    }

    bool readDir = true;
    if (type == QLatin1String("furniture") ||
            type == QLatin1String("roof_corner"))
        readDir = false;

    BuildingObject::Direction dir = BuildingObject::dirFromString(dirString);
    if (readDir && dir == BuildingObject::Invalid) {
        xml.raiseError(tr("Invalid object direction '%1'").arg(dirString));
        return 0;
    }

    BuildingObject *object = 0;
    if (type == QLatin1String("door")) {
        Door *door = new Door(floor, x, y, dir);
        door->setTile(BuildingTiles::instance()->getDoorTile(tile));
        const QString frame = atts.value(QLatin1String("FrameTile")).toString();
        door->setFrameTile(BuildingTiles::instance()->getDoorFrameTile(frame));
        object = door;
    } else if (type == QLatin1String("stairs")) {
        object = new Stairs(floor, x, y, dir);
        object->setTile(BuildingTiles::instance()->getStairsTile(tile));
    } else if (type == QLatin1String("window")) {
        object = new Window(floor, x, y, dir);
        object->setTile(BuildingTiles::instance()->getWindowTile(tile));
    } else if (type == QLatin1String("furniture")) {
        FurnitureObject *furniture = new FurnitureObject(floor, x, y);
        int index = atts.value(QLatin1String("FurnitureTiles")).toString().toInt();
        if (index < 0 || index >= mFurnitureTiles.count()) {
            xml.raiseError(tr("Furniture index %1 out of range").arg(index));
            delete furniture;
            return 0;
        }
        QString orientString = atts.value(QLatin1String("orient")).toString();
        FurnitureTile::FurnitureOrientation orient =
                FurnitureGroups::orientFromString(orientString);
        if (orient == FurnitureTile::FurnitureUnknown) {
            xml.raiseError(tr("Unknown furniture orientation '%1'").arg(orientString));
            delete furniture;
            return 0;
        }
        furniture->setFurnitureTile(mFurnitureTiles.at(index)->tile(orient));
        object = furniture;
    } else if (type == QLatin1String("roof")) {
        int length = atts.value(QLatin1String("length")).toString().toInt();
        int thickness = atts.value(QLatin1String("thickness")).toString().toInt();
        int width1 = atts.value(QLatin1String("width1")).toString().toInt();
        int width2 = atts.value(QLatin1String("width2")).toString().toInt();
        bool capped = atts.value(QLatin1String("capped")).toString().toInt() ? true : false;
        int depth = atts.value(QLatin1String("depth")).toString().toInt();
        RoofObject *roof = new RoofObject(floor, x, y, dir,
                                          length, thickness, width1, width2,
                                          capped, depth);
        roof->setTile(BuildingTiles::instance()->getRoofTile(tile));
        const QString capTile = atts.value(QLatin1String("CapTile")).toString();
        roof->setTile(BuildingTiles::instance()->getRoofCapTile(capTile), 1);
        object = roof;
    } else if (type == QLatin1String("roof_corner")) {
        int width = atts.value(QLatin1String("width")).toString().toInt();
        int height = atts.value(QLatin1String("height")).toString().toInt();
        int depth = atts.value(QLatin1String("depth")).toString().toInt();
        QString orientString = atts.value(QLatin1String("orient")).toString();
        RoofCornerObject::Orient orient = RoofCornerObject::orientFromString(
                    orientString);
        if (orient == RoofCornerObject::Invalid) {
            xml.raiseError(tr("Unknown roof_corner orient '%1'").arg(orientString));
            return 0;
        }
        RoofCornerObject *corner = new RoofCornerObject(floor, x, y,
                                                        width, height, depth,
                                                        orient);
        corner->setTile(BuildingTiles::instance()->getRoofTile(tile));
        object = corner;
    } else {
        xml.raiseError(tr("Unknown object type '%1'").arg(type));
        return 0;
    }

    xml.skipCurrentElement();

    return object;
}

void BuildingReaderPrivate::decodeCSVFloorData(BuildingFloor *floor,
                                               const QString &text)
{
    int start = 0;
    int end = text.length();
    while (start < end && text.at(start).isSpace())
        start++;
    int x = 0, y = 0;
    const QChar sep(QLatin1Char(','));
    const QChar nullChar(QLatin1Char('0'));
    while ((end = text.indexOf(sep, start, Qt::CaseSensitive)) != -1) {
        if (end - start == 1 && text.at(start) == nullChar) {
            floor->SetRoomAt(x, y, 0);
        } else {
            bool conversionOk;
            uint index = text.mid(start, end - start).toUInt(&conversionOk);
            if (!conversionOk) {
                xml.raiseError(
                        tr("Unable to parse room at (%1,%2) on floor %3")
                               .arg(x + 1).arg(y + 1).arg(floor->level()));
                return;
            }
            floor->SetRoomAt(x, y, getRoom(floor, x, y, index));
        }
        start = end + 1;
        if (++x == floor->width()) {
            ++y;
            if (y >= floor->height()) {
                xml.raiseError(tr("Corrupt <rooms> for floor %1")
                               .arg(floor->level()));
                return;
            }
            x = 0;
        }
    }
    end = text.size();
    while (start < end && text.at(end-1).isSpace())
        end--;
    if (end - start == 1 && text.at(start) == nullChar) {
        floor->SetRoomAt(x, y, 0);
    } else {
        bool conversionOk;
        uint index = text.mid(start, end - start).toUInt(&conversionOk);
        if (!conversionOk) {
            xml.raiseError(
                    tr("Unable to parse room at (%1,%2) on floor %3")
                           .arg(x + 1).arg(y + 1).arg(floor->level()));
            return;
        }
        floor->SetRoomAt(x, y, getRoom(floor, x, y, index));
    }
}

Room *BuildingReaderPrivate::getRoom(BuildingFloor *floor, int x, int y, int index)
{
    if (!index)
        return 0;
    if (index > 0 && index - 1 < mBuilding->roomCount())
        return mBuilding->room(index - 1);
    xml.raiseError(tr("Invalid room index at (%1,%2) on floor %3")
                   .arg(x).arg(y).arg(floor->level()));
    return 0;
}

void BuildingReaderPrivate::readUnknownElement()
{
    qDebug() << "Unknown element (fixme):" << xml.name();
    xml.skipCurrentElement();
}

/////

BuildingReader::BuildingReader()
    : d(new BuildingReaderPrivate(this))
{
}

BuildingReader::~BuildingReader()
{
    delete d;
}

Building *BuildingReader::read(QIODevice *device, const QString &path)
{
    return d->readBuilding(device, path);
}

Building *BuildingReader::read(const QString &fileName)
{
    QFile file(fileName);
    if (!d->openFile(&file))
        return 0;

    return read(&file, QFileInfo(fileName).absolutePath());
}

QString BuildingReader::errorString() const
{
    return d->errorString();
}
