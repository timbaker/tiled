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
    "WallExtra",
    "Window",
    "WindowFrame",
    nullptr
};

TileRotationFile::TileRotationFile()
{

}

TileRotationFile::~TileRotationFile()
{
    qDeleteAll(mTilesets);
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

    QString txtName = QLatin1Literal("TileRotation.txt");

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    for (const SimpleFileBlock& block : simple.blocks) {
        if (block.name == QLatin1String("tileset")) {
            TilesetRotated* tileset = readTileset(block);
            if (tileset == nullptr) {
                return false;
            }
            mTilesets += tileset;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

TilesetRotated *TileRotationFile::readTileset(const SimpleFileBlock &tilesetBlock)
{
    TilesetRotated *tileset = new TilesetRotated();
    tileset->mName = tilesetBlock.value("name");
    bool ok = false;
    tileset->mColumnCount =  int(tilesetBlock.value("columns").toUInt(&ok));

    for (const SimpleFileBlock& block : tilesetBlock.blocks) {
        if (block.name == QLatin1String("tile")) {
            TileRotated *tile = readTile(block);
            if (tile == nullptr) {
                delete tileset;
                return nullptr;
            }
            tile->mTileset = tileset;
            tile->mID = tile->mXY.x() + tile->mXY.y() * tileset->mColumnCount;
            tileset->mTiles += tile; // TODO: column,row
            if (tile->mID >= tileset->mTileByID.size()) {
                tileset->mTileByID.resize(tile->mID + 1);
            }
            tileset->mTileByID[tile->mID] = tile;
        } else {
            mError = tr("Unknown block name '%1'.")
                    .arg(block.name);
            delete tileset;
            return nullptr;
        }
    }

    return tileset;
}

TileRotated *TileRotationFile::readTile(const SimpleFileBlock &tileBlock)
{
    TileRotated *tile = new TileRotated(); // QScopedPointer
    parse2Ints(tileBlock.value("xy"), &tile->mXY.rx(), &tile->mXY.ry());

    for (const SimpleFileBlock& block : tileBlock.blocks) {
        if (block.name == QLatin1Literal("r90")) {
            if (!readDirection(block, tile->r90)) {
                delete tile;
                return nullptr;
            }
        }
        else if (block.name == QLatin1Literal("r180")) {
            if (!readDirection(block, tile->r180)) {
                delete tile;
                return nullptr;
            }
        }
        else if (block.name == QLatin1Literal("r270")) {
            if (!readDirection(block, tile->r270)) {
                delete tile;
                return nullptr;
            }
        }
        else {
            mError = tr("Unknown block name '%1'.")
                    .arg(block.name);
            delete tile;
            return nullptr;
        }
    }

    return tile;
}

bool TileRotationFile::readDirection(const SimpleFileBlock &block, TileRotatedDirection &direction)
{
    for (const SimpleFileKeyValue& kv : block.values) {
        if (kv.name == QLatin1Literal("tile")) {
            QString tilesetName;
            int tileIndex;
            if (!BuildingTilesMgr::instance()->parseTileName(kv.value, tilesetName, tileIndex)) {
                mError = tr("Can't parse tile name '%1'").arg(kv.value);
                return false;
            }
            direction.mTileNames += BuildingTilesMgr::instance()->nameForTile(tilesetName, tileIndex);
        } else {
            mError = tr("Uknown value %1=%2.")
                    .arg(kv.name)
                    .arg(kv.value);
            return false;
        }
    }
    return true;
}

bool TileRotationFile::write(const QString &path, const QList<TilesetRotated *> &tilesets)
{
    SimpleFile simpleFile;

    for (TilesetRotated *tileset : tilesets) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("name", tileset->mName);
        tilesetBlock.addValue("columns", QString::number(tileset->mColumnCount));
        for (TileRotated *tile : tileset->mTiles) {
            SimpleFileBlock tileBlock;
            writeTile(tile, tileBlock);
            tilesetBlock.blocks += tileBlock;
        }
        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<TilesetRotated *> Tiled::TileRotationFile::takeTilesets()
{
    QList<TilesetRotated *> result(mTilesets);
    mTilesets.clear();
    return result;
}

bool TileRotationFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a;
    *pb = b;
    return true;
}

void TileRotationFile::writeTile(TileRotated *tile, SimpleFileBlock &tileBlock)
{
    tileBlock.name = QLatin1Literal("tile");
    tileBlock.addValue("xy", QString(QLatin1Literal("%1,%2")).arg(tile->mXY.x()).arg(tile->mXY.y()));

    SimpleFileBlock r90Block;
    r90Block.name = QLatin1Literal("r90");
    writeDirection(tile->r90, r90Block);
    tileBlock.blocks += r90Block;

    SimpleFileBlock r180Block;
    r180Block.name = QLatin1Literal("r180");
    writeDirection(tile->r180, r180Block);
    tileBlock.blocks += r180Block;

    SimpleFileBlock r270Block;
    r270Block.name = QLatin1Literal("r270");
    writeDirection(tile->r270, r270Block);
    tileBlock.blocks += r270Block;
}

void TileRotationFile::writeDirection(TileRotatedDirection &direction, SimpleFileBlock &directionBlock)
{
    for (const QString& tileName : direction.mTileNames) {
        directionBlock.addValue("tile", tileName);
    }
}
