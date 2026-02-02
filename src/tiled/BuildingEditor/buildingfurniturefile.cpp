/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#include "buildingfurniturefile.h"

#include "buildingtiles.h"
#include "simplefile.h"

#include <QDir>

using namespace BuildingEditor;

#define VERSION0 0
#define VERSION_LATEST VERSION0

BuildingFurnitureFile::BuildingFurnitureFile()
{

}

BuildingEditor::BuildingFurnitureFile::~BuildingFurnitureFile()
{
    qDeleteAll(mGroups);
    mGroups.clear();
}

int BuildingFurnitureFile::getVersionLatest()
{
    return VERSION_LATEST;
}

bool BuildingFurnitureFile::read(const QString &fileName)
{
    const QString nativePath = QDir::toNativeSeparators(fileName);

    SimpleFile simple;
    if (!simple.read(fileName)) {
        mError = QStringLiteral("%1\n(while reading %2)").arg(simple.errorString()).arg(nativePath);
        return false;
    }

    mVersion = simple.version();

    if (mVersion > VERSION_LATEST) {
        mError = QStringLiteral("Version %1 is from a newer version of TileZed\n(while reading %2)").arg(mVersion).arg(nativePath);
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    for (const SimpleFileBlock &block : std::as_const(simple.blocks)) {
        if (block.name == QStringLiteral("group")) {
            FurnitureGroup *group = new FurnitureGroup;
            group->mLabel = block.value("label");
            for (const SimpleFileBlock &furnitureBlock : block.blocks) {
                if (furnitureBlock.name == QStringLiteral("furniture")) {
                    FurnitureTiles *tiles = furnitureTilesFromSFB(furnitureBlock, mError);
                    if (!tiles) {
                        delete group;
                        return false;
                    }
                    group->mTiles += tiles;
                    tiles->setGroup(group);
                } else {
                    mError = QStringLiteral("Unknown block name '%1'.\n%2").arg(block.name).arg(nativePath);
                    delete group;
                    return false;
                }
            }
            mGroups += group;
        } else {
            mError = QStringLiteral("Unknown block name '%1'.\n%2").arg(block.name).arg(nativePath);
            return false;
        }
    }

    return true;
}

bool BuildingFurnitureFile::write(const QString &fileName, const int revision, const int sourceRevision, const QList<FurnitureGroup *> &groups)
{
    SimpleFile simpleFile;
    for (const FurnitureGroup *group : groups) {
        SimpleFileBlock groupBlock;
        groupBlock.name = QStringLiteral("group");
        groupBlock.values += SimpleFileKeyValue(QStringLiteral("label"), group->mLabel);
        for (const FurnitureTiles *ftiles : group->mTiles) {
            SimpleFileBlock furnitureBlock = furnitureTilesToSFB(ftiles);
            groupBlock.blocks += furnitureBlock;
        }
        simpleFile.blocks += groupBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(revision));
    simpleFile.replaceValue("source_revision", QString::number(sourceRevision));
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool BuildingFurnitureFile::write(const QString &fileName)
{
    return write(fileName, mRevision, mSourceRevision, mGroups);
}

int BuildingFurnitureFile::getVersion() const
{
    return mVersion;
}

int BuildingFurnitureFile::getRevision() const
{
    return mRevision;
}

int BuildingFurnitureFile::getSourceRevision() const
{
    return mSourceRevision;
}

QList<FurnitureGroup *> BuildingFurnitureFile::takeGroups()
{
    QList<FurnitureGroup*> result = mGroups;
    mGroups.clear();
    return result;
}

const QString &BuildingFurnitureFile::errorString() const
{
    return mError;
}

FurnitureTiles *BuildingFurnitureFile::furnitureTilesFromSFB(const SimpleFileBlock &furnitureBlock, QString &error)
{
    bool corners = furnitureBlock.value("corners") == QStringLiteral("true");

    QString layerString = furnitureBlock.value("layer");
    FurnitureTiles::FurnitureLayer layer = layerString.isEmpty() ?
                                               FurnitureTiles::LayerFurniture : FurnitureTiles::layerFromString(layerString);
    if (layer == FurnitureTiles::InvalidLayer) {
        error = QStringLiteral("Invalid furniture layer '%1'.").arg(layerString);
        return nullptr;
    }

    FurnitureTiles *tiles = new FurnitureTiles(corners);
    tiles->setLayer(layer);
    for (const SimpleFileBlock &entryBlock : furnitureBlock.blocks) {
        if (entryBlock.name == QStringLiteral("entry")) {
            FurnitureTile::FurnitureOrientation orient
                = orientFromString(entryBlock.value(QStringLiteral("orient")));
            QString grimeString = entryBlock.value(QStringLiteral("grime"));
            bool grime = true;
            if (grimeString.length() && !booleanFromString(grimeString, grime, error)) {
                delete tiles;
                return nullptr;
            }
            FurnitureTile *tile = new FurnitureTile(tiles, orient);
            tile->setAllowGrime(grime);
            for (const SimpleFileKeyValue &kv : entryBlock.values) {
                if (!kv.name.contains(QLatin1Char(',')))
                    continue;
                QStringList values = kv.name.split(QLatin1Char(','),
                                                   Qt::SkipEmptyParts);
                int x = values[0].toInt();
                int y = values[1].toInt();
                if (x < 0 || x >= 50 || y < 0 || y >= 50) {
                    error = QStringLiteral("Invalid tile coordinates (%1,%2).").arg(x).arg(y);
                    delete tile;
                    delete tiles;
                    return nullptr;
                }
                QString tilesetName;
                int tileIndex;
                if (!BuildingTilesMgr::parseTileName(kv.value, tilesetName, tileIndex)) {
                    error = QStringLiteral("Can't parse tile name '%1'.").arg(kv.value);
                    delete tile;
                    delete tiles;
                    return nullptr;
                }
                tile->setTile(x, y, BuildingTilesMgr::instance()->get(kv.value));
            }
            tiles->setTile(tile);
        } else {
            error = QStringLiteral("Unknown block name '%1'.").arg(entryBlock.name);
            delete tiles;
            return nullptr;
        }
    }

    return tiles;
}

FurnitureTile::FurnitureOrientation BuildingFurnitureFile::orientFromString(const QString &s)
{
    if (s == QStringLiteral("W")) return FurnitureTile::FurnitureW;
    if (s == QStringLiteral("N")) return FurnitureTile::FurnitureN;
    if (s == QStringLiteral("E")) return FurnitureTile::FurnitureE;
    if (s == QStringLiteral("S")) return FurnitureTile::FurnitureS;
    if (s == QStringLiteral("SW")) return FurnitureTile::FurnitureSW;
    if (s == QStringLiteral("NW")) return FurnitureTile::FurnitureNW;
    if (s == QStringLiteral("NE")) return FurnitureTile::FurnitureNE;
    if (s == QStringLiteral("SE")) return FurnitureTile::FurnitureSE;
    return FurnitureTile::FurnitureUnknown;
}

bool BuildingFurnitureFile::booleanFromString(const QString &s, bool &result, QString &error)
{
    if (s == QStringLiteral("true")) {
        result = true;
        return true;
    }
    if (s == QStringLiteral("false")) {
        result = false;
        return true;
    }
    error = QStringLiteral("Expected boolean but got '%1'").arg(s);
    return false;
}

SimpleFileBlock BuildingFurnitureFile::furnitureTilesToSFB(const FurnitureTiles *ftiles)
{
    SimpleFileBlock furnitureBlock;
    furnitureBlock.name = QStringLiteral("furniture");
    if (ftiles->hasCorners()) {
        furnitureBlock.addValue("corners", QStringLiteral("true"));
    }
    if (ftiles->layer() != FurnitureTiles::LayerFurniture) {
        furnitureBlock.addValue("layer", ftiles->layerToString());
    }
    for (const FurnitureTile *ftile : ftiles->tiles()) {
        if (ftile->isEmpty()) {
            continue;
        }
        SimpleFileBlock entryBlock;
        entryBlock.name = QStringLiteral("entry");
        entryBlock.values += SimpleFileKeyValue(QStringLiteral("orient"), ftile->orientToString());
        if (!ftile->allowGrime()) {
            entryBlock.addValue("grime", QStringLiteral("false"));
        }
        for (int x = 0; x < ftile->width(); x++) {
            for (int y = 0; y < ftile->height(); y++) {
                if (BuildingTile *btile = ftile->tile(x, y)) {
                    entryBlock.values += SimpleFileKeyValue(
                        QString(QStringLiteral("%1,%2")).arg(x).arg(y),
                        btile->name());
                }
            }
        }
        furnitureBlock.blocks += entryBlock;
    }
    return furnitureBlock;
}
