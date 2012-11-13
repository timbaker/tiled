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
#include "rooftiles.h"

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

    BuildingTileEntry *readTileEntry();

    Room *readRoom();

    BuildingFloor *readFloor();
    void decodeCSVFloorData(BuildingFloor *floor, const QString &text);
    Room *getRoom(BuildingFloor *floor, int x, int y, int index);

    BuildingObject *readObject(BuildingFloor *floor);

    bool booleanFromString(const QString &s, bool &result);
    bool readPoint(const QString &name, QPoint &result);

    BuildingTileEntry *getEntry(const QString &s);

    BuildingReader *p;

    QString mError;
    QString mPath;
    Building *mBuilding;
    QList<FurnitureTiles*> mFurnitureTiles;
    QList<BuildingTileEntry*> mEntries;

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
    const int width = atts.value(QLatin1String("width")).toString().toInt();
    const int height = atts.value(QLatin1String("height")).toString().toInt();

    const QString exteriorWall = atts.value(QLatin1String("ExteriorWall")).toString();
    const QString door = atts.value(QLatin1String("Door")).toString();
    const QString doorFrame = atts.value(QLatin1String("DoorFrame")).toString();
    const QString window = atts.value(QLatin1String("Window")).toString();
    const QString curtains = atts.value(QLatin1String("Curtains")).toString();
    const QString stairs = atts.value(QLatin1String("Stairs")).toString();

    while (xml.readNextStartElement()) {
        if (xml.name() == "furniture") {
            if (FurnitureTiles *tiles = readFurnitureTiles())
                mFurnitureTiles += tiles;
        } else if (xml.name() == "tile_entry") {
            if (BuildingTileEntry *entry = readTileEntry())
                mEntries += entry;
        } else if (xml.name() == "room") {
            if (Room *room = readRoom())
                mBuilding->insertRoom(mBuilding->roomCount(), room);
        } else if (xml.name() == "floor") {
            if (BuildingFloor *floor = readFloor())
                mBuilding->insertFloor(mBuilding->floorCount(), floor);
        } else
            readUnknownElement();
    }

    mBuilding = new Building(width, height);
    mBuilding->setExteriorWall(getEntry(exteriorWall)->asExteriorWall());
    mBuilding->setDoorTile(getEntry(door)->asDoor());
    mBuilding->setDoorFrameTile(getEntry(doorFrame)->asDoorFrame());
    mBuilding->setWindowTile(getEntry(window)->asWindow());
    mBuilding->setCurtainsTile(getEntry(curtains)->asCurtains());
    mBuilding->setStairsTile(getEntry(stairs)->asStairs());

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

    if (FurnitureTiles *match = FurnitureGroups::instance()->findMatch(ftiles)) {
        delete ftiles;
        return match;
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
    BuildingTile *btile = BuildingTilesMgr::instance()->get(tileName);
    xml.skipCurrentElement();
    return btile;
}

BuildingTileEntry *BuildingReaderPrivate::readTileEntry()
{
    Q_ASSERT(xml.isStartElement() && xml.name() == "tile_entry");

    const QXmlStreamAttributes atts = xml.attributes();
    const QString categoryName = atts.value(QLatin1String("category")).toString();
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(categoryName);
    if (!category) {
        xml.raiseError(tr("unknown category '%1'").arg(categoryName));
        return 0;
    }

    BuildingTileEntry *entry = new BuildingTileEntry(category);

    while (xml.readNextStartElement()) {
        if (xml.name() == "tile") {
            const QString enumName = atts.value(QLatin1String("enum")).toString();
            int e = category->enumFromString(enumName);
            if (e == BuildingTileCategory::Invalid) {
                xml.raiseError(tr("Unknown %1 enum '%1'").arg(categoryName).arg(enumName));
                return false;
            }
            const QString tileName = atts.value(QLatin1String("tile")).toString();
            BuildingTile *btile = BuildingTilesMgr::instance()->get(tileName);

            QPoint offset;
            if (!readPoint(QLatin1String("offset"), offset))
                return false;

            entry->mTiles[e] = btile;
            entry->mOffsets[e] = offset;
        } else
            readUnknownElement();
    }

    return entry;
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
    room->Wall = getEntry(interiorWall)->asInteriorWall();
    room->Floor = getEntry(floor)->asFloor();

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
            type == QLatin1String("roof"))
        readDir = false;

    BuildingObject::Direction dir = BuildingObject::dirFromString(dirString);
    if (readDir && dir == BuildingObject::Invalid) {
        xml.raiseError(tr("Invalid object direction '%1'").arg(dirString));
        return 0;
    }

    BuildingObject *object = 0;
    if (type == QLatin1String("door")) {
        Door *door = new Door(floor, x, y, dir);
        door->setTile(getEntry(tile)->asDoor());
        const QString frame = atts.value(QLatin1String("FrameTile")).toString();
        door->setTile(getEntry(frame)->asDoorFrame(), 1);
        object = door;
    } else if (type == QLatin1String("stairs")) {
        object = new Stairs(floor, x, y, dir);
        object->setTile(getEntry(tile)->asStairs());
    } else if (type == QLatin1String("window")) {
        object = new Window(floor, x, y, dir);
        object->setTile(getEntry(tile)->asWindow());
        const QString curtains = atts.value(QLatin1String("CurtainsTile")).toString();
        object->setTile(getEntry(curtains)->asCurtains(), 1);
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
        int width = atts.value(QLatin1String("width")).toString().toInt();
        int height = atts.value(QLatin1String("height")).toString().toInt();
        QString typeString = atts.value(QLatin1String("RoofType")).toString();
        RoofObject::RoofType roofType = RoofObject::typeFromString(typeString);
        if (roofType == RoofObject::InvalidType) {
            xml.raiseError(tr("Invalid roof type '%1'").arg(typeString));
            return 0;
        }

        QString capTilesString = atts.value(QLatin1String("CapTiles")).toString();
        BuildingTileEntry *capTiles = getEntry(capTilesString)->asRoofCap();

        QString slopeTileString = atts.value(QLatin1String("SlopeTiles")).toString();
        BuildingTileEntry *slopeTiles = getEntry(slopeTileString)->asRoofSlope();

        bool cappedW, cappedN, cappedE, cappedS;
        if (!booleanFromString(atts.value(QLatin1String("cappedW")).toString(), cappedW))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedN")).toString(), cappedN))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedE")).toString(), cappedE))
            return 0;
        if (!booleanFromString(atts.value(QLatin1String("cappedS")).toString(), cappedS))
            return 0;
        RoofObject *roof = new RoofObject(floor, x, y, width, height,
                                          roofType,
                                          cappedW, cappedN, cappedE, cappedS);
        roof->setCapTiles(capTiles);
        roof->setSlopeTiles(slopeTiles);
        object = roof;
    } else {
        xml.raiseError(tr("Unknown object type '%1'").arg(type));
        return 0;
    }

    xml.skipCurrentElement();

    return object;
}

bool BuildingReaderPrivate::booleanFromString(const QString &s, bool &result)
{
    if (s == QLatin1String("true")) {
        result = true;
        return true;
    }
    if (s == QLatin1String("false")) {
        result = false;
        return true;
    }
    xml.raiseError(tr("Expected boolean but got '%1'").arg(s));
    return false;
}

bool BuildingReaderPrivate::readPoint(const QString &name, QPoint &result)
{
    const QXmlStreamAttributes atts = xml.attributes();
    QString s = atts.value(name).toString();
    QStringList split = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (split.size() != 2) {
        xml.raiseError(tr("expected point, got '%1'").arg(s));
        return false;
    }
    result.setX(split[0].toInt());
    result.setY(split[1].toInt());
    return true;
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
