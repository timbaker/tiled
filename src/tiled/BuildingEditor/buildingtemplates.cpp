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

bool BuildingTemplates::readBuildingTemplatesTxt()
{
    QFileInfo info(BuildingPreferences::instance()
                   ->configPath(QLatin1String("BuildingTemplates.txt")));
    if (!info.exists()) {
        mError = tr("The BuildingTemplates.txt file doesn't exist.");
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }
    BuildingTiles *btiles = BuildingTiles::instance();
    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("Template")) {
            BuildingTemplate *def = new BuildingTemplate;
            def->Name = block.value("Name");
            def->Wall = btiles->getExteriorWall(block.value("Wall"));
            def->DoorTile = btiles->getDoorTile(block.value("Door"));
            def->DoorFrameTile = btiles->getDoorFrameTile(block.value("DoorFrame"));
            def->WindowTile = btiles->getWindowTile(block.value("Window"));
            def->StairsTile = btiles->getStairsTile(block.value("Stairs"));
            foreach (SimpleFileBlock roomBlock, block.blocks) {
                if (roomBlock.name == QLatin1String("Room")) {
                    Room *room = new Room;
                    room->Name = roomBlock.value("Name");
                    QStringList rgb = roomBlock.value("Color").split(QLatin1String(" "), QString::SkipEmptyParts);
                    room->Color = qRgb(rgb.at(0).toInt(),
                                       rgb.at(1).toInt(),
                                       rgb.at(2).toInt());
                    room->Wall = btiles->getInteriorWall(roomBlock.value("Wall"));
                    room->Floor = btiles->getFloorTile(roomBlock.value("Floor"));
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

#if 0
    if (BuildingTemplates::instance()->templateCount() == 0) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("No buildings were defined in BuildingTemplates.txt."));
        return false;
    }
#endif

    return true;
}

void BuildingTemplates::writeBuildingTemplatesTxt(QWidget *parent)
{
    SimpleFile simpleFile;
    foreach (BuildingTemplate *btemplate, BuildingTemplates::instance()->templates()) {
        SimpleFileBlock templateBlock;
        templateBlock.name = QLatin1String("Template");
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Name"), btemplate->Name);
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Wall"), btemplate->Wall->name());
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Door"), btemplate->DoorTile->name());
        templateBlock.values += SimpleFileKeyValue(QLatin1String("DoorFrame"), btemplate->DoorFrameTile->name());
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Window"), btemplate->WindowTile->name());
        templateBlock.values += SimpleFileKeyValue(QLatin1String("Stairs"), btemplate->StairsTile->name());
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
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Wall"), room->Wall->name());
            roomBlock.values += SimpleFileKeyValue(QLatin1String("Floor"), room->Floor->name());
            templateBlock.blocks += roomBlock;
        }
        simpleFile.blocks += templateBlock;
    }
    QString path = BuildingPreferences::instance()
            ->configPath(QLatin1String("BuildingTemplates.txt"));
    if (!simpleFile.write(path)) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

/////

