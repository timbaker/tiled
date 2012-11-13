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

static BuildingTileEntry *readTileEntry(SimpleFileBlock &block)
{
    return 0;
}

#define VERSION0 0
#define VERSION_LATEST 0

bool BuildingTemplates::readBuildingTemplatesTxt()
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
    BuildingTilesMgr *btiles = BuildingTilesMgr::instance();
    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("TileEntry")) {
            if (BuildingTileEntry *entry = readTileEntry(block))
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

void BuildingTemplates::writeBuildingTemplatesTxt(QWidget *parent)
{
    mEntries.clear();
    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        addEntry(btemplate->DoorTile);
        addEntry(btemplate->DoorFrameTile);
        addEntry(btemplate->CurtainsTile);
        addEntry(btemplate->StairsTile);
        addEntry(btemplate->Wall);
        addEntry(btemplate->StairsTile);
        addEntry(btemplate->WindowTile);
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
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Name"), btemplate->Name);
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Wall"), entryIndex(btemplate->Wall));
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Door"), entryIndex(btemplate->DoorTile));
        templateBlock.values += SimpleFileKeyValue(QLatin1String("DoorFrame"), entryIndex(btemplate->DoorFrameTile));
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Window"), entryIndex(btemplate->WindowTile));
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Curtains"), entryIndex(btemplate->CurtainsTile));
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Stairs"), entryIndex(btemplate->StairsTile));
        foreach (Room *room, btemplate->RoomList) {
            SimpleFileBlock roomBlock;
            roomBlock.name = QLatin1String("Room");
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Name"), room->Name);
            QString colorString = QString(QLatin1String("%1 %2 %3"))
                    .arg(qRed(room->Color))
                    .arg(qGreen(room->Color))
                    .arg(qBlue(room->Color));
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Color"), colorString);
            roomBlock.values += SimpleFileKeyValue(QLatin1String("InternalName"), room->internalName);
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Wall"), entryIndex(room->Wall));
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Floor"), entryIndex(room->Floor));
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

    // UPGRADE HERE

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

/////

