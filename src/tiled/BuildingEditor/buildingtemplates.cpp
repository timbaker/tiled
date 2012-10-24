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
#include "simplefile.h"

#include <QCoreApplication>
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
    QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + QLatin1String("BuildingTemplates.txt");
    if (!simpleFile.write(path)) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

/////

