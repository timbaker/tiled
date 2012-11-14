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

    BuildingTileEntry *noneEntry = BuildingTilesMgr::instance()->noneTileEntry();

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
            def->Name = block.value("Name");
            def->Wall = getEntry(block.value("Wall"))->asExteriorWall();
            def->DoorTile = getEntry(block.value("Door"))->asDoor();
            def->DoorFrameTile = getEntry(block.value("DoorFrame"))->asDoorFrame();
            def->WindowTile = getEntry(block.value("Window"))->asWindow();
            def->CurtainsTile = getEntry(block.value("Curtains"))->asCurtains();
            def->StairsTile = getEntry(block.value("Stairs"))->asStairs();
            def->RoofCap = getEntry(block.value("RoofCap"))->asRoofCap();
            def->RoofSlope = getEntry(block.value("RoofSlope"))->asRoofSlope();
            def->RoofTop = getEntry(block.value("RoofTop"))->asRoofTop();
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
                    def->RoomList += room;
                } else {
                    mError = tr("Unknown block name '%1': expected 'Room'.\n%2")
                            .arg(roomBlock.name)
                            .arg(path);
                    return false;
                }
            }
            if (!def->Wall) def->Wall = noneEntry;
            if (!def->DoorTile) def->DoorTile = noneEntry;
            if (!def->DoorFrameTile) def->DoorFrameTile = noneEntry;
            if (!def->WindowTile) def->WindowTile = noneEntry;
            if (!def->CurtainsTile) def->CurtainsTile = noneEntry;
            if (!def->StairsTile) def->StairsTile = noneEntry;
            if (!def->RoofCap) def->RoofCap = noneEntry;
            if (!def->RoofSlope) def->RoofSlope = noneEntry;
            if (!def->RoofTop) def->RoofTop = noneEntry;
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
        addEntry(btemplate->Wall);
        addEntry(btemplate->DoorTile);
        addEntry(btemplate->DoorFrameTile);
        addEntry(btemplate->WindowTile);
        addEntry(btemplate->CurtainsTile);
        addEntry(btemplate->StairsTile);
        addEntry(btemplate->RoofCap);
        addEntry(btemplate->RoofSlope);
        addEntry(btemplate->RoofTop);
        foreach (Room *room, btemplate->RoomList) {
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
        templateBlock.addValue("Name", btemplate->Name);
        templateBlock.addValue("Wall", entryIndex(btemplate->Wall));
        templateBlock.addValue("Door", entryIndex(btemplate->DoorTile));
        templateBlock.addValue("DoorFrame", entryIndex(btemplate->DoorFrameTile));
        templateBlock.addValue("Window", entryIndex(btemplate->WindowTile));
        templateBlock.addValue("Curtains", entryIndex(btemplate->CurtainsTile));
        templateBlock.addValue("Stairs", entryIndex(btemplate->StairsTile));
        templateBlock.addValue("RoofCap", entryIndex(btemplate->RoofCap));
        templateBlock.addValue("RoofSlope", entryIndex(btemplate->RoofSlope));
        templateBlock.addValue("RoofTop", entryIndex(btemplate->RoofTop));
        foreach (Room *room, btemplate->RoomList) {
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

