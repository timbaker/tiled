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

static const char *MAP_ROTATION_NAMES[] = {
    "R0",
    "R90",
    "R180",
    "R270",
    nullptr
};

static const char *MAP_ROTATION_SUFFIX[] = {
    "_R0",
    "_R90",
    "_R180",
    "_R270",
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
        if (block.name == QLatin1String("mapping")) {
            for (const SimpleFileKeyValue& kv : block.values) {
                mMapping[kv.name] = kv.value;
            }
        } else if (block.name == QLatin1String("tileset")) {
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
    tileset->mNameUnrotated = tilesetBlock.value("name");
    bool ok = false;
    tileset->mColumnCount =  int(tilesetBlock.value("columns").toUInt(&ok));
    tileset->mRotation = parseRotation(tilesetBlock.value("rotation")); // FIXME: get this from _R90 etc in the name?
    tileset->mNameRotated = tileset->mNameUnrotated + QLatin1Literal(MAP_ROTATION_SUFFIX[int(tileset->mRotation)]);

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

MapRotation TileRotationFile::parseRotation(const QString &str)
{
    for (int i = 0; MAP_ROTATION_NAMES[i] != nullptr; i++) {
        if (str == QLatin1Literal(MAP_ROTATION_NAMES[i])) {
            return static_cast<MapRotation>(i);
        }
    }
    return MapRotation::NotRotated;
}

TileRotated *TileRotationFile::readTile(const SimpleFileBlock &tileBlock)
{
    TileRotated *tile = new TileRotated(); // QScopedPointer
    parse2Ints(tileBlock.value("xy"), &tile->mXY.rx(), &tile->mXY.ry());

    for (const SimpleFileBlock& block : tileBlock.blocks) {
        if (block.name == QLatin1Literal("visual")) {
            if (!readDirection(block, tile->mVisual)) {
                delete tile;
                return nullptr;
            }
        }
#if 0
        else if (block.name == QLatin1Literal("r90")) {
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
#endif
        else {
            mError = tr("Unknown block name '%1'.")
                    .arg(block.name);
            delete tile;
            return nullptr;
        }
    }

    return tile;
}

bool TileRotationFile::readDirection(const SimpleFileBlock &block, TileRotatedVisual &direction)
{
    for (const SimpleFileBlock& block1 : block.blocks) {
        QString tileName = block1.value("name");
        QString tilesetName;
        int tileIndex;
        if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileIndex)) {
            mError = tr("Can't parse tile name '%1'").arg(tileName);
            return false;
        }
        QPoint offset;
        if (block1.hasValue("offset")) {
            if (!parse2Ints(block1.value("offset"), &offset.rx(), &offset.ry())) {
                mError = tr("Can't parse offset '%1'").arg(block1.value("offset"));
                return false;
            }
        }
        TileRotatedVisual::Edge edge = TileRotatedVisual::Edge::None;
        if (block1.hasValue("edge")) {
            QString edgeStr = block1.value("edge");
            for (int i = 0; TileRotatedVisual::EDGE_NAMES[i] != nullptr; i++) {
                if (edgeStr == QLatin1Literal(TileRotatedVisual::EDGE_NAMES[i])) {
                    edge = static_cast<TileRotatedVisual::Edge>(i);
                    break;
                }
            }
        }
        direction.addTile(tileName, offset ,edge);
    }
    return true;
}

bool TileRotationFile::write(const QString &path, const QList<TilesetRotated *> &tilesets, const QMap<QString, QString> &mapping)
{
    SimpleFile simpleFile;

    SimpleFileBlock mappingBlock;

    mappingBlock.name = QLatin1Literal("mapping");
    for (const QString& key : mapping.keys()) {
        mappingBlock.addValue(key, mapping[key]);
    }
    simpleFile.blocks += mappingBlock;

    for (TilesetRotated *tileset : tilesets) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("name", tileset->nameUnrotated());
        tilesetBlock.addValue("columns", QString::number(tileset->mColumnCount));
        tilesetBlock.addValue("rotation", QLatin1Literal(MAP_ROTATION_NAMES[int(tileset->mRotation)]));
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

QMap<QString, QString> TileRotationFile::takeMapping()
{
    return mMapping;
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

QString TileRotationFile::twoInts(int a, int b)
{
    return QString(QLatin1Literal("%1,%2")).arg(a).arg(b);
}

void TileRotationFile::writeTile(TileRotated *tile, SimpleFileBlock &tileBlock)
{
    tileBlock.name = QLatin1Literal("tile");
    tileBlock.addValue("xy", twoInts(tile->mXY.x(), tile->mXY.y()));

    SimpleFileBlock r0Block;
    r0Block.name = QLatin1Literal("visual");
    writeDirection(tile->mVisual, r0Block);
    tileBlock.blocks += r0Block;

//    SimpleFileBlock r90Block;
//    r90Block.name = QLatin1Literal("r90");
//    writeDirection(tile->r90, r90Block);
//    tileBlock.blocks += r90Block;

//    SimpleFileBlock r180Block;
//    r180Block.name = QLatin1Literal("r180");
//    writeDirection(tile->r180, r180Block);
//    tileBlock.blocks += r180Block;

//    SimpleFileBlock r270Block;
//    r270Block.name = QLatin1Literal("r270");
//    writeDirection(tile->r270, r270Block);
//    tileBlock.blocks += r270Block;
}

void TileRotationFile::writeDirection(TileRotatedVisual &direction, SimpleFileBlock &directionBlock)
{
    for (int i = 0; i < direction.mTileNames.size(); i++) {
        const QString& tileName = direction.mTileNames[i];
        QPoint tileOffset = direction.mOffsets[i];
        TileRotatedVisual::Edge edge = direction.mEdges[i];
        SimpleFileBlock block;
        block.name = QLatin1Literal("tile");
        block.addValue("name", tileName);
        if (!tileOffset.isNull()) {
            block.addValue("offset", twoInts(tileOffset.x(), tileOffset.y()));
        }
        if (edge != TileRotatedVisual::Edge::None) {
            block.addValue("edge", QLatin1Literal(TileRotatedVisual::EDGE_NAMES[int(edge)]));
        }
        directionBlock.blocks += block;
    }
}
