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

#include "buildingtemplates.h"

#include "buildingtiles.h"
#include "buildingpreferences.h"
#include "simplefile.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>

using namespace BuildingEditor;

static const char *TXT_FILE = "BuildingTemplates.txt";

/////

BuildingTemplates *BuildingTemplates::mInstance = 0;

BuildingTemplates *BuildingTemplates::instance()
{
    if (!mInstance)
        mInstance = new BuildingTemplates;
    return mInstance;
}

void BuildingTemplates::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTemplates::BuildingTemplates()
{
}

BuildingTemplates::~BuildingTemplates()
{
    qDeleteAll(mTemplates);
}

void BuildingTemplates::addTemplate(BuildingTemplate *btemplate)
{
    mTemplates += btemplate;
}

void BuildingTemplates::replaceTemplates(const QList<BuildingTemplate *> &templates)
{
    qDeleteAll(mTemplates);
    mTemplates.clear();
    foreach (BuildingTemplate *btemplate, templates)
        mTemplates += new BuildingTemplate(btemplate);
}

QString BuildingTemplates::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTemplates::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

static void writeTileEntry(SimpleFileBlock &block, BuildingTileEntry *entry)
{

}

static BuildingTileEntry *readTileEntry(SimpleFileBlock &block, QString &error)
{
    QString categoryName = block.value("category");
    if (BuildingTileCategory *category = BuildingTilesMgr::instance()->category(categoryName)) {
        BuildingTileEntry *entry = new BuildingTileEntry(category);
        foreach (SimpleFileKeyValue kv, block.values) {
            if (kv.name == QLatin1String("category"))
                continue;
            int e = category->enumFromString(kv.name);
            if (e == BuildingTileCategory::Invalid) {
                error = BuildingTilesMgr::instance()->tr("Unknown %1 enum %2")
                        .arg(categoryName).arg(kv.name);
                return 0;
            }
            entry->mTiles[e] = BuildingTilesMgr::instance()->get(kv.value);
        }

        if (BuildingTileEntry *match = category->findMatch(entry)) {
            delete entry;
            entry = match;
        }

        return entry;
    }
    error = BuildingTilesMgr::instance()->tr("Unknown tile category '%1'")
            .arg(categoryName);
    return 0;
}

#define VERSION0 0

// VERSION1
// Massive rewrite -> BuildingTileEntry
#define VERSION1 1
#define VERSION_LATEST VERSION1

bool BuildingTemplates::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mEntries.clear();
    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("TileEntry")) {
            if (BuildingTileEntry *entry = readTileEntry(block, mError))
                addEntry(entry);
            else
                return false;
            continue;
        }
        if (block.name == QLatin1String("Template")) {
            BuildingTemplate *def = new BuildingTemplate;
            def->setName(block.value("Name"));
            for (int i = 0; i < BuildingTemplate::TileCount; i++)
                def->setTile(i, getEntry(block.value(def->enumToString(i))));
            foreach (SimpleFileBlock roomBlock, block.blocks) {
                if (roomBlock.name == QLatin1String("Room")) {
                    Room *room = new Room;
                    room->Name = roomBlock.value("Name");
                    QStringList rgb = roomBlock.value("Color").split(QLatin1String(" "), QString::SkipEmptyParts);
                    room->Color = qRgb(rgb.at(0).toInt(),
                                       rgb.at(1).toInt(),
                                       rgb.at(2).toInt());
                    room->Wall = getEntry(roomBlock.value("Wall"))->asInteriorWall();
                    room->Floor = getEntry(roomBlock.value("Floor"))->asFloor();
                    room->internalName = roomBlock.value("InternalName");
                    def->addRoom(room);
                } else {
                    mError = tr("Unknown block name '%1': expected 'Room'.\n%2")
                            .arg(roomBlock.name)
                            .arg(path);
                    return false;
                }
            }
            addTemplate(def);
        } else {
            mError = tr("Unknown block name '%1': expected 'Template'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

void BuildingTemplates::writeTxt(QWidget *parent)
{
    mEntries.clear();
    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        foreach (BuildingTileEntry *entry, btemplate->tiles())
            addEntry(entry);
        foreach (Room *room, btemplate->rooms()) {
            addEntry(room->Floor);
            addEntry(room->Wall);
        }
    }

    SimpleFile simpleFile;
    foreach (BuildingTileEntry *entry, mEntries)
        writeTileEntry(simpleFile, entry);

    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        SimpleFileBlock templateBlock;
        templateBlock.name = QLatin1String("Template");
        templateBlock.addValue("Name", btemplate->name());
        for (int i = 0; i < BuildingTemplate::TileCount; i++)
            templateBlock.addValue(btemplate->enumToString(i), entryIndex(btemplate->tile(i)));
        foreach (Room *room, btemplate->rooms()) {
            SimpleFileBlock roomBlock;
            roomBlock.name = QLatin1String("Room");
            roomBlock.addValue("Name", room->Name);
            QString colorString = QString(QLatin1String("%1 %2 %3"))
                    .arg(qRed(room->Color))
                    .arg(qGreen(room->Color))
                    .arg(qBlue(room->Color));
            roomBlock.addValue("Color", colorString);
            roomBlock.addValue("InternalName", room->internalName);
            roomBlock.addValue("Wall", entryIndex(room->Wall));
            roomBlock.addValue("Floor", entryIndex(room->Floor));
            templateBlock.blocks += roomBlock;
        }
        simpleFile.blocks += templateBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(txtPath())) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

QString BuildingTemplates::entryIndex(BuildingTileCategory *category,
                                      const QString &tileName)
{
    if (tileName.isEmpty())
        return QString::number(0);
    BuildingTileEntry *entry = mEntryMap[qMakePair(category,tileName)];
    return QString::number(mEntries.indexOf(entry) + 1);
}

bool BuildingTemplates::upgradeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    int userVersion = userFile.version(); // may be zero for unversioned file
    if (userVersion == VERSION_LATEST)
        return true;

    // Not the latest version -> upgrade it.

    QString sourcePath = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + txtName();

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    if (VERSION_LATEST == VERSION1) {
        // Massive rewrite -> BuildingTileEntry stuff

        // Step 1: read all the single-tile assignments and convert them to
        // BuildingTileEntry.
        mEntries.clear();
        mEntryMap.clear();
        mEntriesByCategoryName.clear();
        BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
        foreach (SimpleFileBlock block, userFile.blocks) {
            if (block.name == QLatin1String("Template")) {
                addEntry(btiles->catEWalls(), block.value("Wall"));
                addEntry(btiles->catDoors(), block.value("Door"));
                addEntry(btiles->catDoorFrames(), block.value("DoorFrame"));
                addEntry(btiles->catWindows(), block.value("Window"));
                addEntry(btiles->catCurtains(), block.value("Curtains"));
                addEntry(btiles->catStairs(), block.value("Stairs"));
                foreach (SimpleFileBlock roomBlock, block.blocks) {
                    if (roomBlock.name == QLatin1String("Room")) {
                        addEntry(btiles->category(QLatin1String("interior_walls")), roomBlock.value("Wall"));
                        addEntry(btiles->category(QLatin1String("floors")), roomBlock.value("Floor"));
                    }
                }
            }
        }

        // Step 3: add the TileEntry blocks
        SimpleFileBlock newFile;
        foreach (BuildingTileEntry *entry, mEntries) {
            SimpleFileBlock block;
            block.name = QLatin1String("TileEntry");
            block.addValue("category", entry->category()->name());
            for (int i = 0; i < entry->category()->enumCount(); i++)
                block.addValue(entry->category()->enumToString(i), entry->tile(i)->name());
            newFile.blocks += block;
        }

        // Step 3: replace tile names with entry indices
        foreach (SimpleFileBlock block, userFile.blocks) {
            SimpleFileBlock newBlock;
            newBlock.name = block.name;
            newBlock.values = block.values;
            if (block.name == QLatin1String("Template")) {
                newBlock.replaceValue("Wall", entryIndex(btiles->catEWalls(), block.value("Wall")));
                newBlock.renameValue("Wall", QLatin1String("ExteriorWall"));
                newBlock.replaceValue("Door", entryIndex(btiles->catDoors(), block.value("Door")));
                newBlock.replaceValue("DoorFrame",  entryIndex(btiles->catDoorFrames(), block.value("DoorFrame")));
                newBlock.replaceValue("Window", entryIndex(btiles->catWindows(), block.value("Window")));
                newBlock.replaceValue("Curtains", entryIndex(btiles->catCurtains(), block.value("Curtains")));
                newBlock.replaceValue("Stairs", entryIndex(btiles->catStairs(), block.value("Stairs")));

                foreach (SimpleFileBlock roomBlock, block.blocks) {
                    SimpleFileBlock newRoomBlock = roomBlock;
                    if (roomBlock.name == QLatin1String("Room")) {
                        newRoomBlock.replaceValue("Wall", entryIndex(btiles->catIWalls(), roomBlock.value("Wall")));
                        newRoomBlock.replaceValue("Floor", entryIndex(btiles->catFloors(), roomBlock.value("Floor")));
                    }
                    newBlock.blocks += newRoomBlock;
                }
            }
            newFile.blocks += newBlock;
        }

        userFile.blocks = newFile.blocks;
        userFile.values = newFile.values;
    }

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

void BuildingTemplates::addEntry(BuildingTileEntry *entry)
{
    if (entry && !entry->isNone() && !mEntries.contains(entry))
        mEntries += entry;
}

void BuildingTemplates::addEntry(BuildingTileCategory *category,
                                 const QString &tileName)
{
    if (tileName.isEmpty())
        return;
    QPair<BuildingTileCategory*,QString> p = qMakePair(category, tileName);
    if (mEntryMap.contains(p))
        return;

    BuildingTileEntry *entry = category->createEntryFromSingleTile(tileName);
    mEntriesByCategoryName[category->name() + tileName] = entry;
    mEntries = mEntriesByCategoryName.values(); // sorted
    mEntryMap[p] = entry;
}

QString BuildingTemplates::entryIndex(BuildingTileEntry *entry)
{
    if (!entry || entry->isNone())
        return QString::number(0);
    return QString::number(mEntries.indexOf(entry) + 1);
}

BuildingTileEntry *BuildingTemplates::getEntry(const QString &s)
{
    int index = s.toInt();
    if (index >= 1 && index <= mEntries.size())
        return mEntries[index - 1];
    return BuildingTilesMgr::instance()->noneTileEntry();
}

/////

QStringList BuildingTemplate::mEnumNames;

BuildingTemplate::BuildingTemplate() :
    mTiles(TileCount)
{
}

void BuildingTemplate::setTile(int n, BuildingTileEntry *entry)
{
    if (entry)
        entry = entry->asCategory(categoryEnum(n));

    if (!entry)
        entry = BuildingTilesMgr::instance()->noneTileEntry();

    mTiles[n] = entry;
}

void BuildingTemplate::setTiles(const QVector<BuildingTileEntry *> &entries)
{
    for (int i = 0; i < entries.size(); i++)
        setTile(i, entries[i]);
}

int BuildingTemplate::categoryEnum(int n)
{
    switch (n) {
    case ExteriorWall: return BuildingTilesMgr::ExteriorWalls;
    case Door: return BuildingTilesMgr::Doors;
    case DoorFrame: return BuildingTilesMgr::DoorFrames;
    case Window: return BuildingTilesMgr::Windows;
    case Curtains: return BuildingTilesMgr::Curtains;
    case Stairs: return BuildingTilesMgr::Stairs;
    case RoofCap: return BuildingTilesMgr::RoofCaps;
    case RoofSlope: return BuildingTilesMgr::RoofSlopes;
    case RoofTop: return BuildingTilesMgr::RoofTops;
    default:
        qFatal("Invalid enum passed to BuildingTemplate::categoryEnum");
        break;
    }
    return 0;
}

QString BuildingTemplate::enumToString(int n)
{
    initNames();
    return mEnumNames[n];
}

void BuildingTemplate::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
    mEnumNames.reserve(TileCount);
    mEnumNames += QLatin1String("ExteriorWall");
    mEnumNames += QLatin1String("Door");
    mEnumNames += QLatin1String("DoorFrame");
    mEnumNames += QLatin1String("Window");
    mEnumNames += QLatin1String("Curtains");
    mEnumNames += QLatin1String("Stairs");
    mEnumNames += QLatin1String("RoofCap");
    mEnumNames += QLatin1String("RoofSlope");
    mEnumNames += QLatin1String("RoofTop");
}
