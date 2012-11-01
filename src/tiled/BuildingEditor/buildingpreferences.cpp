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

#include "buildingpreferences.h"

#include <QDir>

using namespace BuildingEditor;

static const char *KEY_TILES_DIR = "BuildingEditor/TilesDirectory";

BuildingPreferences *BuildingPreferences::mInstance = 0;

BuildingPreferences *BuildingPreferences::instance()
{
    if (!mInstance)
        mInstance = new BuildingPreferences;
    return mInstance;
}

void BuildingPreferences::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingPreferences::BuildingPreferences(QObject *parent) :
    QObject(parent)
{
    mTilesDirectory = mSettings.value(QLatin1String(KEY_TILES_DIR),
                                      QLatin1String("../Tiles")).toString();
}

QString BuildingPreferences::configPath() const
{
    return QDir::homePath() + QLatin1Char('/') + QLatin1String(".TileZed");
}

QString BuildingPreferences::configPath(const QString &fileName) const
{
    return configPath() + QLatin1Char('/') + fileName;
}

QString BuildingPreferences::tilesDirectory() const
{
    return mTilesDirectory;
}
