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

#include "furnituregroups.h"

#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "simplefile.h"

#include <QCoreApplication>
#include <QFileInfo>

using namespace BuildingEditor;

FurnitureGroups *FurnitureGroups::mInstance = 0;

FurnitureGroups *FurnitureGroups::instance()
{
    if (!mInstance)
        mInstance = new FurnitureGroups;
    return mInstance;
}

void FurnitureGroups::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

FurnitureGroups::FurnitureGroups()
{
}

void FurnitureGroups::addGroup(FurnitureGroup *group)
{
    mGroups += group;
}

void FurnitureGroups::insertGroup(int index, FurnitureGroup *group)
{
    mGroups.insert(index, group);
}

FurnitureGroup *FurnitureGroups::removeGroup(int index)
{
    return mGroups.takeAt(index);
}

void FurnitureGroups::removeGroup(FurnitureGroup *group)
{
    mGroups.removeAll(group);
}

bool FurnitureGroups::readTxt()
{
    QFileInfo info(BuildingPreferences::instance()
                   ->configPath(QLatin1String("BuildingFurniture.txt")));
    if (!info.exists()) {
        mError = tr("The BuildingFurniture.txt file doesn't exist.");
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("group")) {
            FurnitureGroup *group = new FurnitureGroup;
            group->mLabel = block.value("label");
            foreach (SimpleFileBlock furnitureBlock, block.blocks) {
                if (furnitureBlock.name == QLatin1String("furniture")) {
                    FurnitureTiles *tiles = new FurnitureTiles;
                    foreach (SimpleFileBlock entryBlock, furnitureBlock.blocks) {
                        if (entryBlock.name == QLatin1String("entry")) {
                            FurnitureTile::FurnitureOrientation orient
                                    = orientFromString(entryBlock.value(QLatin1String("orient")));
                            FurnitureTile *tile = new FurnitureTile(tiles, orient);
                            foreach (SimpleFileKeyValue kv, entryBlock.values) {
                                if (!kv.name.contains(QLatin1Char(',')))
                                    continue;
                                QStringList values = kv.name.split(QLatin1Char(','),
                                                                   QString::SkipEmptyParts);
                                int x = values[0].toInt();
                                int y = values[1].toInt();
                                if (x < 0 || x >= 2 || y < 0 || y >= 2) {
                                    mError = tr("Invalid tile coordinates (%1,%2)")
                                            .arg(x).arg(y);
                                    return false;
                                }
                                QString tilesetName;
                                int tileIndex;
                                if (!BuildingTiles::parseTileName(kv.value, tilesetName, tileIndex)) {
                                    mError = tr("Can't parse tile name '%1'").arg(kv.value);
                                    return false;
                                }
                                tile->mTiles[x + y * 2] = BuildingTiles::instance()->getFurnitureTile(kv.value);

                            }
                            tiles->mTiles[FurnitureTiles::orientIndex(tile->mOrient)] = tile;
                        } else {
                            mError = tr("Unknown block name '%1'.\n%2")
                                    .arg(block.name)
                                    .arg(path);
                            return false;
                        }
                    }
                    group->mTiles += tiles;
                } else {
                    mError = tr("Unknown block name '%1'.\n%2")
                            .arg(block.name)
                            .arg(path);
                    return false;
                }
            }
            mGroups += group;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
#if 0
    FurnitureTiles *tiles = new FurnitureTiles;

    FurnitureTile *tile = new FurnitureTile(FurnitureTile::FurnitureW);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 3);
    tile->mTiles[2] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 2);
    tiles->mTiles[0] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureN);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 0);
    tile->mTiles[1] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 1);
    tiles->mTiles[1] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureE);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 5);
    tile->mTiles[2] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 4);
    tiles->mTiles[2] = tile;

    tile = new FurnitureTile(FurnitureTile::FurnitureS);
    tile->mTiles[0] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 6);
    tile->mTiles[1] = new BuildingTile(QLatin1String("furniture_seating_indoor_01"), 7);
    tiles->mTiles[3] = tile;

    FurnitureGroup *group = new FurnitureGroup;
    group->mLabel = QLatin1String("Indoor Seating");
    group->mTiles += tiles;

    mGroups += group;

    return true;
#endif
}

bool FurnitureGroups::writeTxt()
{
    SimpleFile simpleFile;
    foreach (FurnitureGroup *group, groups()) {
        SimpleFileBlock groupBlock;
        groupBlock.name = QLatin1String("group");
        groupBlock.values += SimpleFileKeyValue(QLatin1String("label"),
                                                   group->mLabel);
        foreach (FurnitureTiles *tiles, group->mTiles) {
            SimpleFileBlock furnitureBlock;
            furnitureBlock.name = QLatin1String("furniture");
            for (int i = 0; i < 4; i++) {
                FurnitureTile *tile = tiles->mTiles[i];
                SimpleFileBlock entryBlock;
                entryBlock.name = QLatin1String("entry");
                entryBlock.values += SimpleFileKeyValue(QLatin1String("orient"),
                                                        tile->orientToString());
                for (int j = 0; j < 4; j++) {
                    if (!tile->mTiles[j])
                        continue;
                    int x = j % 2;
                    int y = j / 2;
                    entryBlock.values += SimpleFileKeyValue(
                                QString(QLatin1String("%1,%2")).arg(x).arg(y),
                                tile->mTiles[j]->name());
                }
                furnitureBlock.blocks += entryBlock;
            }
            groupBlock.blocks += furnitureBlock;
        }
        simpleFile.blocks += groupBlock;
    }
    QString path = BuildingPreferences::instance()
            ->configPath(QLatin1String("BuildingFurniture.txt"));
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

FurnitureTile::FurnitureOrientation FurnitureGroups::orientFromString(const QString &s)
{
    if (s == QLatin1String("W")) return FurnitureTile::FurnitureW;
    if (s == QLatin1String("N")) return FurnitureTile::FurnitureN;
    if (s == QLatin1String("E")) return FurnitureTile::FurnitureE;
    if (s == QLatin1String("S")) return FurnitureTile::FurnitureS;
    if (s == QLatin1String("SW")) return FurnitureTile::FurnitureSW;
    if (s == QLatin1String("NW")) return FurnitureTile::FurnitureNW;
    if (s == QLatin1String("NE")) return FurnitureTile::FurnitureNE;
    if (s == QLatin1String("SE")) return FurnitureTile::FurnitureSE;
    return FurnitureTile::FurnitureUnknown;
}

/////

QSize FurnitureTile::size() const
{
    int width = (resolvedTiles()[1] || resolvedTiles()[3]) ? 2 : 1;
    int height = (resolvedTiles()[2] || resolvedTiles()[3]) ? 2 : 1;
    return QSize(width, height);
}

const QVector<BuildingTile *> &FurnitureTile::resolvedTiles() const
{
    if (isEmpty()) {
        if (mOrient == FurnitureE)
            return mOwner->mTiles[FurnitureW]->mTiles;
        if (mOrient == FurnitureN)
            return mOwner->mTiles[FurnitureW]->mTiles;
        if (mOrient == FurnitureS) {
            if (!mOwner->mTiles[FurnitureN]->isEmpty())
                return mOwner->mTiles[FurnitureN]->mTiles;
            return mOwner->mTiles[FurnitureW]->mTiles;
        }
    }
    return mTiles;
}

/////

bool FurnitureTiles::isCorners() const
{
    return mTiles[0]->isSW();
}

void FurnitureTiles::toggleCorners()
{
    if (isCorners()) {
        mTiles[0]->mOrient = FurnitureTile::FurnitureW;
        mTiles[1]->mOrient = FurnitureTile::FurnitureN;
        mTiles[2]->mOrient = FurnitureTile::FurnitureE;
        mTiles[3]->mOrient = FurnitureTile::FurnitureS;
    } else {
        mTiles[0]->mOrient = FurnitureTile::FurnitureSW;
        mTiles[1]->mOrient = FurnitureTile::FurnitureNW;
        mTiles[2]->mOrient = FurnitureTile::FurnitureNE;
        mTiles[3]->mOrient = FurnitureTile::FurnitureSE;
    }
}

FurnitureTile *FurnitureTiles::tile(FurnitureTile::FurnitureOrientation orient) const
{
    return mTiles[orientIndex(orient)];
}

int FurnitureTiles::orientIndex(FurnitureTile::FurnitureOrientation orient)
{
    int index[9] = {0, 1, 2, 3, 0, 1, 2, 3, -1};
    return index[orient];
}
