/*
 * tilerotationfile.cpp
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

#include "tilerotationfile.h"

#include "tilerotation.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"
#include "BuildingEditor/simplefile.h"

#include <QFileInfo>

using namespace BuildingEditor;
using namespace Tiled;

// enum TileRotateType
const char *Tiled::TILE_ROTATE_NAMES[] = {
    "None",
    "Door",
    "DoorFrame",
    "Wall",
    "Window",
    "WindowFrame",
    nullptr
};

TileRotationFile::TileRotationFile()
{

}

TileRotationFile::~TileRotationFile()
{
//    for (auto *tile : mTiles) {
//        delete tile->mFurnitureTiles;
//    }
    qDeleteAll(mTiles);
}

bool TileRotationFile::read(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(path);
        return false;
    }

    QString path2 = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path2)) {
        mError = simple.errorString();
        return false;
    }

    QString txtName = QLatin1Literal("Tilesets.txt");

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    for (const SimpleFileBlock& block : simple.blocks) {
        if (block.name == QLatin1String("tiles")) {
            TileRotateFileInfo* furnitureTiles = furnitureTilesFromSFB(block, mError);
            if (furnitureTiles == nullptr) {
                return false;
            }
            mTiles += furnitureTiles;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

TileRotateFileInfo *TileRotationFile::furnitureTilesFromSFB(const SimpleFileBlock &furnitureBlock, QString &error)
{
#if 0
    bool corners = furnitureBlock.value("corners") == QLatin1String("true");

    QString layerString = furnitureBlock.value("layer");
    FurnitureTiles::FurnitureLayer layer = layerString.isEmpty() ?
            FurnitureTiles::LayerFurniture : FurnitureTiles::layerFromString(layerString);
    if (layer == FurnitureTiles::InvalidLayer) {
        error = tr("Invalid furniture layer '%1'.").arg(layerString);
        return nullptr;
    }
#endif

    TileRotateFileInfo *tiles = new TileRotateFileInfo();
#if 0
    tiles->setLayer(layer);
#endif
    if (furnitureBlock.hasValue("type")) {
        QString typeStr = furnitureBlock.value("type");
        for (int i = 0; TILE_ROTATE_NAMES[i]; i++) {
            if (typeStr == QLatin1String(TILE_ROTATE_NAMES[i])) {
                tiles->mType = TileRotateType(i);
                break;
            }
        }
    }
    for (const SimpleFileBlock& entryBlock : furnitureBlock.blocks) {
        if (entryBlock.name == QLatin1String("entry")) {
            FurnitureTile::FurnitureOrientation orient = FurnitureGroups::orientFromString(entryBlock.value(QLatin1String("orient")));
#if 0
            QString grimeString = entryBlock.value(QLatin1String("grime"));
            bool grime = true;
            if (grimeString.length() && !booleanFromString(grimeString, grime)) {
                error = mError;
                delete tiles;
                return nullptr;
            }
#endif
            FurnitureTile *tile = new FurnitureTile(tiles->mFurnitureTiles, orient);
#if 0
            tile->setAllowGrime(grime);
#endif
            for (const SimpleFileKeyValue& kv : entryBlock.values) {
                if (!kv.name.contains(QLatin1Char(',')))
                    continue;
                QStringList values = kv.name.split(QLatin1Char(','), QString::SkipEmptyParts);
                int x = values[0].toInt();
                int y = values[1].toInt();
                if (x < 0 || x >= 50 || y < 0 || y >= 50) {
                    error = tr("Invalid tile coordinates (%1,%2).")
                            .arg(x).arg(y);
                    delete tiles;
                    return nullptr;
                }
                QString tilesetName;
                int tileIndex;
                if (!BuildingTilesMgr::parseTileName(kv.value, tilesetName, tileIndex)) {
                    error = tr("Can't parse tile name '%1'.").arg(kv.value);
                    delete tiles;
                    return nullptr;
                }
                tile->setTile(x, y, BuildingTilesMgr::instance()->get(kv.value));
            }
            tiles->mFurnitureTiles->setTile(tile);
        } else {
            error = tr("Unknown block name '%1'.")
                    .arg(entryBlock.name);
            delete tiles;
            return nullptr;
        }
    }

    return tiles;
}

bool TileRotationFile::write(const QString &path, const QList<TileRotateFileInfo *> &tiles)
{
    SimpleFile simpleFile;

    for (TileRotateFileInfo *ftiles : tiles) {
        SimpleFileBlock furnitureBlock;
        furnitureBlock.name = QLatin1String("tiles");
//        if (ftiles->hasCorners())
//            furnitureBlock.addValue("corners", QLatin1String("true"));
//        if (ftiles->layer() != FurnitureTiles::LayerFurniture)
//            furnitureBlock.addValue("layer", ftiles->layerToString());
        int typeIndex = static_cast<int>(ftiles->mType);
        if (typeIndex >= 0 && typeIndex < int(TileRotateType::MAX)) {
            furnitureBlock.addValue("type", QLatin1String(TILE_ROTATE_NAMES[int(ftiles->mType)]));
        }
        for (FurnitureTile *ftile : ftiles->mFurnitureTiles->tiles()) {
            if (ftile->isEmpty())
                continue;
            SimpleFileBlock entryBlock;
            entryBlock.name = QLatin1String("entry");
            entryBlock.values += SimpleFileKeyValue(QLatin1String("orient"),
                                                    ftile->orientToString());
//            if (!ftile->allowGrime())
//                entryBlock.addValue("grime", QLatin1String("false"));
            for (int x = 0; x < ftile->width(); x++) {
                for (int y = 0; y < ftile->height(); y++) {
                    if (BuildingTile *btile = ftile->tile(x, y)) {
                        entryBlock.values += SimpleFileKeyValue(
                                    QString(QLatin1String("%1,%2")).arg(x).arg(y),
                                    btile->name());
                    }
                }
            }
            furnitureBlock.blocks += entryBlock;
        }
        simpleFile.blocks += furnitureBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<TileRotateFileInfo *> Tiled::TileRotationFile::takeTiles()
{
    QList<TileRotateFileInfo *> result(mTiles);
    mTiles.clear();
    return result;
}
