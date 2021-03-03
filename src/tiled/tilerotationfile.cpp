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

#include "BuildingEditor/simplefile.h"

#include <QFileInfo>

using namespace Tiled;

TileRotationFile::TileRotationFile()
{

}

TileRotationFile::~TileRotationFile()
{
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
            TileRotateInfo entry;
            entry.mTileNames[0] = block.value("tile1");
            entry.mTileNames[1] = block.value("tile2");
            entry.mTileNames[2] = block.value("tile3");
            entry.mTileNames[3] = block.value("tile4");
            mTiles += new TileRotateInfo(entry);
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

bool TileRotationFile::write(const QString &path, const QList<TileRotateInfo *> &tiles)
{
    SimpleFile simpleFile;

    for (const TileRotateInfo *entry : tiles) {
        SimpleFileBlock tilesBlock;
        tilesBlock.name = QLatin1String("tiles");
        tilesBlock.addValue("tile1", entry->mTileNames[0]);
        tilesBlock.addValue("tile2", entry->mTileNames[1]);
        tilesBlock.addValue("tile3", entry->mTileNames[2]);
        tilesBlock.addValue("tile4", entry->mTileNames[3]);
        simpleFile.blocks += tilesBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(path)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

QList<Tiled::TileRotateInfo *> Tiled::TileRotationFile::takeTiles()
{
    QList<Tiled::TileRotateInfo *> result(mTiles);
    mTiles.clear();
    return result;
}
