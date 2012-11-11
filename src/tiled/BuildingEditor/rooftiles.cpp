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

#include "rooftiles.h"

#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "simplefile.h"

#include <QCoreApplication>
#include <QFileInfo>

using namespace BuildingEditor;

static const char *TXT_FILE = "RoofTiles.txt";

RoofTiles *RoofTiles::mInstance = 0;

RoofTiles *RoofTiles::instance()
{
    if (!mInstance)
        mInstance = new RoofTiles;
    return mInstance;
}

void RoofTiles::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

RoofTiles::RoofTiles()
{
}

QString RoofTiles::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString RoofTiles::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool RoofTiles::readTxt()
{
    QString fileName = txtPath();
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    QString path = info.canonicalFilePath();
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

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tiles")) {
            QString wall = block.value("exterior_wall");
            wall = BuildingTiles::normalizeTileName(wall);
            QString cap = block.value("cap_tile");
            cap = BuildingTiles::normalizeTileName(cap);
            RoofCapTile *tile = new RoofCapTile;
            tile->mExteriorWall = BuildingTiles::instance()->getExteriorWall(wall);
            tile->mCapTile = BuildingTiles::instance()->getRoofCapTile(cap);
            mTiles += tile;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    // Check that all the tiles exist
    foreach (RoofCapTile *tile, mTiles) {
        if (!BuildingTiles::instance()->tileFor(tile->mCapTile)) {
            mError = tr("Tile %1 #%2 doesn't exist.")
                    .arg(tile->mCapTile->mTilesetName)
                    .arg(tile->mCapTile->mIndex);
            return false;
        }
        if (!BuildingTiles::instance()->tileFor(tile->mExteriorWall)) {
            mError = tr("Tile %1 #%2 doesn't exist.")
                    .arg(tile->mExteriorWall->mTilesetName)
                    .arg(tile->mExteriorWall->mIndex);
            return false;
        }
    }

    return true;
}

bool RoofTiles::writeTxt()
{
    SimpleFile simpleFile;
    foreach (RoofCapTile *tile, mTiles) {
        SimpleFileBlock tileBlock;
        tileBlock.name = QLatin1String("tiles");
        tileBlock.values += SimpleFileKeyValue(QLatin1String("exterior_wall"),
                                               tile->mExteriorWall->name());
        tileBlock.values += SimpleFileKeyValue(QLatin1String("cap_tile"),
                                               tile->mCapTile->name());
        simpleFile.blocks += tileBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

BuildingTile *RoofTiles::gapTileForCap(BuildingTile *cap)
{
    foreach (RoofCapTile *tile, mTiles) {
        if (tile->mCapTile == cap)
            return tile->mExteriorWall;
    }
    return 0;
}

BuildingTile *RoofTiles::capTileForExteriorWall(BuildingTile *exteriorWall)
{
    foreach (RoofCapTile *tile, mTiles) {
        if (tile->mExteriorWall == exteriorWall)
            return tile->mCapTile;
    }
    return 0;
}

bool RoofTiles::upgradeTxt()
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
