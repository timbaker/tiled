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

#include <QFileInfo>

using namespace BuildingEditor;

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

bool RoofTiles::readTxt()
{
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String("RoofTiles.txt"));
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The RoofTiles.txt file doesn't exist.");
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
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
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String("RoofTiles.txt"));
    if (!simpleFile.write(fileName)) {
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
