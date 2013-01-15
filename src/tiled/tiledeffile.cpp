/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#include "tiledeffile.h"

#include "tilesetmanager.h"

#include "tileset.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImageReader>

using namespace Tiled;
using namespace Tiled::Internal;

TileDefFile::TileDefFile()
{
}

static QString ReadString(QDataStream &in)
{
    QString str;
    quint8 c = ' ';
    while (c != '\n') {
        in >> c;
        if (c != '\n')
            str += QLatin1Char(c);
    }
    return str;
}

bool TileDefFile::read(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Error opening file for reading.\n%1").arg(fileName);
        return false;
    }

    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    int numTilesets;
    in >> numTilesets;
    for (int i = 0; i < numTilesets; i++) {
        TileDefTileset *ts = new TileDefTileset;
        ts->mName = ReadString(in);
        ts->mImageSource = ReadString(in);
        qint32 columns, rows;
        in >> columns;
        in >> rows;
        qint32 tileCount;
        in >> tileCount;

        ts->mColumns = columns;
        ts->mRows = rows;

        QVector<TileDefTile*> tiles(columns * rows);
        for (int j = 0; j < tileCount; j++) {
            TileDefTile *tile = new TileDefTile(ts, j);
            qint32 numProperties;
            in >> numProperties;
            for (int k = 0; k < numProperties; k++) {
                QString propertyName = ReadString(in);
                QString propertyValue = ReadString(in);
                tile->mPropertyKeys += propertyName;
                tile->mProperties[propertyName] = propertyValue;
            }
            tile->mPropertyUI.FromProperties();
            tiles[j] = tile;
        }

        // Deal with the image being a different size now than it was when the
        // .tiles file was saved.
        QImageReader bmp(dir.filePath(ts->mImageSource));
        if (bmp.size().isValid()) {
            ts->mColumns = bmp.size().width() / 64;
            ts->mRows = bmp.size().height() / 128;
            ts->mTiles.resize(ts->mColumns * ts->mRows);
            for (int y = 0; y < qMin(ts->mRows, rows); y++) {
                for (int x = 0; x < qMin(ts->mColumns, columns); x++) {
                    ts->mTiles[x + y * ts->mColumns] = tiles[x + y * columns];
                }
            }
            for (int i = 0; i < ts->mTiles.size(); i++) {
                if (!ts->mTiles[i]) {
                    ts->mTiles[i] = new TileDefTile(ts, i);
                }
            }
        } else {
            ts->mTiles = tiles;
        }
        insertTileset(mTilesets.size(), ts);
    }

    mFileName = fileName;

    return true;
}

static void SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++)
        out << quint8(str[i].toAscii());
    out << quint8('\n');
}

bool TileDefFile::write(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        mError = tr("Error opening file for writing.\n%1").arg(fileName);
        return false;
    }

    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << qint32(mTilesets.size());
    foreach (TileDefTileset *ts, mTilesets) {
        SaveString(out, ts->mName);
        SaveString(out, ts->mImageSource);
        out << qint32(ts->mColumns);
        out << qint32(ts->mRows);
        out << qint32(ts->mTiles.size());
        foreach (TileDefTile *tile, ts->mTiles) {
            out << qint32(tile->mProperties.size());
            foreach (QString key, tile->mPropertyKeys/*tile->mProperties.keys()*/) {
                SaveString(out, key);
                SaveString(out, tile->mProperties[key]);
            }
        }
    }

    return true;
}

QString TileDefFile::directory() const
{
    return QFileInfo(mFileName).absolutePath();
}

void TileDefFile::insertTileset(int index, TileDefTileset *ts)
{
    Q_ASSERT(!mTilesets.contains(ts));
    Q_ASSERT(!mTilesetByName.contains(ts->mName));
    mTilesets.insert(index, ts);
    mTilesetByName[ts->mName] = ts;
}

TileDefTileset *TileDefFile::removeTileset(int index)
{
    mTilesetByName.remove(mTilesets[index]->mName);
    return mTilesets.takeAt(index);
}

TileDefTileset *TileDefFile::tileset(const QString &name)
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

/////

TileDefProperties::TileDefProperties()
{
#if 0
    addBoolean("CollideNorth", "collideN");
    addBoolean("CollideWest", "collideW");
    addSeparator();

    static const char *DoorStyle[] = { "None", "North", "West", 0 };
    addEnum("Door", "door", DoorStyle);
    addEnum("DoorFrame", "doorFr", DoorStyle);
    addSeparator();

    addBoolean("IsBed", "bed");
    addBoolean("FloorOverlay");
    addBoolean("IsFloor", "solidfloor");
    addBoolean("IsIndoor", "exterior", true, true);
    addSeparator();

    static const char *TileBlockStyle[] = {
        "None",
        "Solid",
        "SolidTransparent",
        0
    };
    addEnum("TileBlockStyle", 0, TileBlockStyle);
    addSeparator();

    static const char *LightPolyStyle[] = {
        "None",
        "WallW",
        "WallN",
        0
    };
    addEnum("LightPolyStyle", 0, LightPolyStyle);
    addSeparator();

    addString("ContainerType", "container");
    addBoolean("WheelieBin");
    addSeparator();

    static const char *RoofStyle[] = {
        "None",
        "WestRoofB",
        "WestRoofM",
        "WestRoofT",
        0
    };
    addEnum("RoofStyle", 0, RoofStyle);
    addSeparator();

    addBoolean("ClimbSheetN", "climbSheetN");
    addBoolean("ClimbSheetW", "climbSheetW");
    addSeparator();

    static const char *Direction[] = {
        "None",
        "N",
        "NE",
        "E",
        "SE",
        "S",
        "SW",
        "W",
        "NW",
        0
    };
    addEnum("FloorItemShelf", "floor", Direction);
    addEnum("HighItemShelf", "shelf", Direction);
    addEnum("TableItemShelf", "table", Direction);
    addSeparator();

    static const char *StairStyle[] = {
        "None",
        "BottomW",
        "MiddleW",
        "TopW",
        "BottomN",
        "MiddleN",
        "TopN",
        0
    };
    addEnum("StairStyle", "stairs", StairStyle);
    addSeparator();

    addBoolean("PreSeen");
    addSeparator();

    addBoolean("HoppableN");
    addBoolean("HoppableW");
    addBoolean("WallOverlay");
    static const char *WallStyle[] = {
        "None",
        "WestWall",
        "WestWallTrans",
        "WestWindow",
        "WestDoorFrame",
        "NorthWall",
        "NorthWallTrans",
        "NorthWindow",
        "NorthDoorFrame",
        "NorthWestCorner",
        "NorthWestCornerTrans",
        "SouthEastCorner",
        0
    };
    addEnum("WallStyle", 0, WallStyle);
    addSeparator();

    addInteger("WaterAmount", "waterAmount");
    addInteger("WaterMaxAmount", "waterMaxAmount");
    addBoolean("WaterPiped", "waterPiped");
    addSeparator();

    addInteger("OpenTileOffset");
    addInteger("SmashedTileOffset");
    addEnum("Window", "window", DoorStyle);
    addBoolean("WindowLocked");
#endif
}

TileDefProperties::~TileDefProperties()
{
    qDeleteAll(mProperties);
}

void TileDefProperties::addBoolean(const QString &name, const QString &shortName,
                                   bool defaultValue, bool reverseLogic)
{
    TileDefProperty *prop = new BooleanTileDefProperty(name, shortName,
                                                       defaultValue,
                                                       reverseLogic);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addInteger(const QString &name, const QString &shortName,
                                   int min, int max, int defaultValue)
{
    TileDefProperty *prop = new IntegerTileDefProperty(name, shortName,
                                                       min, max, defaultValue);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addString(const QString &name, const QString &shortName,
                                  const QString &defaultValue)
{
    TileDefProperty *prop = new StringTileDefProperty(name, shortName,
                                                      defaultValue);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addEnum(const QString &name, const QString &shortName,
                                const QStringList enums, const QString &defaultValue)
{
    TileDefProperty *prop = new EnumTileDefProperty(name, shortName, enums,
                                                    defaultValue);
    mProperties += prop;
    mPropertyByName[name] = prop;
}

void TileDefProperties::addEnum(const char *name, const char *shortName,
                                const char *enums[], const char *defaultValue)
{
    QStringList enums2;
    for (int i = 0; enums[i]; i++)
        enums2 += QLatin1String(enums[i]);
    addEnum(QLatin1String(name), QLatin1String(shortName ? shortName : name),
            enums2, QLatin1String(defaultValue));
}

/////

UIProperties::UIProperties(QMap<QString, QString> &properties, QList<QString> &keys)
{
    const TileDefProperties &props = TilePropertyMgr::instance()->properties();
    foreach (TileDefProperty *prop, props.mProperties) {
        if (prop->mName == QLatin1String("Door") ||
                prop->mName == QLatin1String("DoorFrame") ||
                prop->mName == QLatin1String("Window")) {
            mProperties[prop->mName] = new PropDoorStyle(prop->mName,
                                                         prop->mShortName,
                                                         properties, keys,
                                                         prop->asEnum()->mEnums);
            continue;
        }
        if (prop->mName == QLatin1String("TileBlockStyle")) {
            mProperties[prop->mName] = new PropTileBlockStyle(prop->mName,
                                                              properties, keys,
                                                              prop->asEnum()->mEnums);
            continue;
        }
#if 0
        if (prop->mName == QLatin1String("LightPolyStyle")) {
            mProperties[prop->mName] = new PropLightPolyStyle(prop->mName,
                                                              properties, keys,
                                                              prop->asEnum()->mEnums);
            continue;
        }
#endif
        if (prop->mName == QLatin1String("RoofStyle")) {
            mProperties[prop->mName] = new PropRoofStyle(prop->mName,
                                                         properties, keys,
                                                         prop->asEnum()->mEnums);
            continue;
        }
        if (prop->mName == QLatin1String("StairStyle")) {
            mProperties[prop->mName] = new PropStairStyle(prop->mName,
                                                          prop->mShortName,
                                                          properties, keys,
                                                          prop->asEnum()->mEnums);
            continue;
        }
        if (prop->mName.contains(QLatin1String("ItemShelf"))) {
            mProperties[prop->mName] = new PropDirection(prop->mName,
                                                         prop->mShortName,
                                                         properties, keys,
                                                         prop->asEnum()->mEnums);
            continue;
        }
        if (prop->mName == QLatin1String("WallStyle")) {
            mProperties[prop->mName] = new PropWallStyle(prop->mName,
                                                         properties, keys,
                                                         prop->asEnum()->mEnums);
            continue;
        }
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            mProperties[p->mName] = new PropGenericBoolean(prop->mName,
                                                           p->mShortName,
                                                           properties, keys,
                                                           p->mDefault,
                                                           p->mReverseLogic);
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            mProperties[p->mName] = new PropGenericInteger(prop->mName,
                                                           p->mShortName,
                                                           properties, keys,
                                                           p->mMin,
                                                           p->mMax,
                                                           p->mDefault);
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            mProperties[p->mName] = new PropGenericString(prop->mName,
                                                          p->mShortName,
                                                          properties, keys,
                                                          p->mDefault);
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            int defaultValue = p->mEnums.indexOf(p->mDefault);
            mProperties[p->mName] = new PropGenericEnum(prop->mName,
                                                        p->mShortName,
                                                        properties, keys,
                                                        p->mEnums,
                                                        defaultValue);
            continue;
        }
    }
}

/////

TileDefTileset::TileDefTileset(Tileset *ts)
{
    mName = ts->name();
    mImageSource = QFileInfo(ts->imageSource()).fileName();
    mColumns = ts->columnCount();
    mRows = ts->tileCount() / mColumns;
    mTiles.resize(ts->tileCount());
    for (int i = 0; i < mTiles.size(); i++)
        mTiles[i] = new TileDefTile(this, i);
}

/////

#include "BuildingEditor/buildingpreferences.h"
#include "BuildingEditor/simplefile.h"

#include <QCoreApplication>

TilePropertyMgr *TilePropertyMgr::mInstance = 0;

TilePropertyMgr *TilePropertyMgr::instance()
{
    if (!mInstance)
        mInstance = new TilePropertyMgr;
    return mInstance;
}

void TilePropertyMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

QString TilePropertyMgr::txtName()
{
    return QLatin1String("TileProperties.txt");
}

QString TilePropertyMgr::txtPath()
{
    return BuildingEditor::BuildingPreferences::instance()->configPath(txtName());
}

bool TilePropertyMgr::readTxt()
{
    QFileInfo info(txtPath());

    // Copy TileProperties.txt from the application directory to the ~/.TileZed
    // directory if needed.
    if (!info.exists()) {
        QString source = QCoreApplication::applicationDirPath() + QLatin1Char('/')
                + txtName();
        if (QFileInfo(source).exists()) {
            if (!QFile::copy(source, txtPath())) {
                mError = tr("Failed to copy file:\nFrom: %1\nTo: %2")
                        .arg(source).arg(txtPath());
                return false;
            }
        }
    }

    info.refresh();
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("property")) {
            if (!addProperty(block))
                return false;
            continue;
        }
        if (block.name == QLatin1String("separator")) {
            mProperties.addSeparator();
            continue;
        }
        if (block.name == QLatin1String("special")) {
            QString Name = block.value(("Name"));
            static const char *DoorStyle[] = { "None", "North", "West", 0 };
            static const char *TileBlockStyle[] = {
                "None",
                "Solid",
                "SolidTransparent",
                0
            };
            static const char *RoofStyle[] = {
                "None",
                "WestRoofB",
                "WestRoofM",
                "WestRoofT",
                0
            };
            static const char *Direction[] = {
                "None",
                "N",
                "NE",
                "E",
                "SE",
                "S",
                "SW",
                "W",
                "NW",
                0
            };
            static const char *StairStyle[] = {
                "None",
                "BottomW",
                "MiddleW",
                "TopW",
                "BottomN",
                "MiddleN",
                "TopN",
                0
            };
            static const char *WallStyle[] = {
                "None",
                "WestWall",
                "WestWallTrans",
                "WestWindow",
                "WestDoorFrame",
                "NorthWall",
                "NorthWallTrans",
                "NorthWindow",
                "NorthDoorFrame",
                "NorthWestCorner",
                "NorthWestCornerTrans",
                "SouthEastCorner",
                0
            };
            if (Name == QLatin1String("Door")) {
                mProperties.addEnum("Door", "door", DoorStyle, "None");
            } else if (Name == QLatin1String("DoorFrame")) {
                mProperties.addEnum("DoorFrame", "doorFr", DoorStyle, "None");
            } else if (Name == QLatin1String("TileBlockStyle")) {
                mProperties.addEnum("TileBlockStyle", 0, TileBlockStyle, "None");
            } else if (Name == QLatin1String("RoofStyle")) {
                mProperties.addEnum("RoofStyle", 0, RoofStyle, "None");
            } else if (Name == QLatin1String("FloorItemShelf")) {
                mProperties.addEnum("FloorItemShelf", "floor", Direction, "None");
            } else if (Name == QLatin1String("HighItemShelf")) {
                mProperties.addEnum("HighItemShelf", "shelf", Direction, "None");
            } else if (Name == QLatin1String("TableItemShelf")) {
                mProperties.addEnum("TableItemShelf", "table", Direction, "None");
            } else if (Name == QLatin1String("StairStyle")) {
                mProperties.addEnum("StairStyle", "stairs", StairStyle, "None");
            } else if (Name == QLatin1String("WallStyle")) {
                mProperties.addEnum("WallStyle", 0, WallStyle, "None");
            } else if (Name == QLatin1String("Window")) {
                mProperties.addEnum("Window", "window", DoorStyle, "None");
            } else {
                mError = tr("Unknown special property '%1'").arg(Name);
                return false;
            }
            continue;
        }
        mError = tr("Unknown block name '%1'\n%2")
                .arg(block.name)
                .arg(path);
        return false;
    }

    return true;
}

bool TilePropertyMgr::addProperty(SimpleFileBlock &block)
{
    QString Type = block.value("Type");
    QString Name = block.value("Name");
    QString ShortName = block.value("ShortName");

    if (Name.isEmpty()) {
        mError = tr("Empty or missing Name value.");
        return false;
    }
    if (mProperties.property(Name) != 0) {
        mError = tr("Duplicate property name '%1'").arg(Name);
        return false;
    }

    if (ShortName.isEmpty())
        ShortName = Name;

    bool ok;
    if (Type == QLatin1String("Boolean")) {
        bool Default = toBoolean(block.value("Default"), ok);
        if (!ok) return false;
        bool ReverseLogic = toBoolean(block.value("ReverseLogic"), ok);
        if (!ok) return false;
        mProperties.addBoolean(Name, ShortName, Default, ReverseLogic);
        return true;
    }

    if (Type == QLatin1String("Integer")) {
        int Min = toInt(block.value("Min"), ok);
        if (!ok) return false;
        int Max = toInt(block.value("Max"), ok);
        if (!ok) return false;
        int Default = toInt(block.value("Default"), ok);
        if (!ok) return false;
        if (Min > Max || Default < Min || Default > Max) {
            mError = tr("Weird integer values: Min=%1 Max=%2 Default=%3.")
                    .arg(Min).arg(Max).arg(Default);
        }
        mProperties.addInteger(Name, ShortName, Min, Max, Default);
        return true;
    }

    if (Type == QLatin1String("String")) {
        QString Default = block.value("Default");
        mProperties.addString(Name, ShortName, Default);
        return true;
    }

    if (Type == QLatin1String("Enum")) {
        if (block.findBlock(QLatin1String("Enums")) < 0) {
            mError = tr("Enum property '%1' is missing an Enums block.").arg(Name);
            return false;
        }
        QStringList enums;
        SimpleFileBlock enumsBlock = block.block("Enums");
        foreach (SimpleFileKeyValue kv, enumsBlock.values)
            enums += kv.value;
        QString Default = block.value("Default");
        if (!enums.contains(Default)) {
            mError = tr("Enum property '%1' Default=%2 missing from Enums block.")
                    .arg(Name).arg(Default);
        }
        mProperties.addEnum(Name, ShortName, enums, Default);
        return true;
    }

    mError = tr("Unknown property Type '%1'.").arg(Type);
    return false;
}

bool TilePropertyMgr::toBoolean(const QString &s, bool &ok)
{
    if (s == QLatin1String("true")) {
        ok = true;
        return true;
    }
    if (s == QLatin1String("false")) {
        ok = true;
        return false;
    }
    mError = tr("Expected boolean but got '%1'.").arg(s);
    ok = false;
    return false;
}

int TilePropertyMgr::toInt(const QString &s, bool &ok)
{
    int i = s.toInt(&ok);
    if (ok) return i;
    mError = tr("Expected integer but got '%1'.").arg(s);
    ok = false;
    return 0;
}

TilePropertyMgr::TilePropertyMgr()
{
}

TilePropertyMgr::~TilePropertyMgr()
{
}

