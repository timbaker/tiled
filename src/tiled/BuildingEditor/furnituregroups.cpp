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

static const char *TXT_FILE = "BuildingFurniture.txt";

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

FurnitureGroups::FurnitureGroups() :
    QObject()
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

FurnitureTiles *FurnitureGroups::findMatch(FurnitureTiles *other)
{
    foreach (FurnitureGroup *candidate, mGroups) {
        if (FurnitureTiles *ftiles = candidate->findMatch(other))
            return ftiles;
    }
    return 0;
}

QString FurnitureGroups::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString FurnitureGroups::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool FurnitureGroups::readTxt()
{
    QFileInfo info(txtPath());
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
        if (block.name == QLatin1String("group")) {
            FurnitureGroup *group = new FurnitureGroup;
            group->mLabel = block.value("label");
            foreach (SimpleFileBlock furnitureBlock, block.blocks) {
                if (furnitureBlock.name == QLatin1String("furniture")) {
                    bool corners = furnitureBlock.value("corners") == QLatin1String("true");

                    QString layerString = furnitureBlock.value("layer");
                    FurnitureTiles::FurnitureLayer layer = layerString.isEmpty() ?
                            FurnitureTiles::LayerFurniture : FurnitureTiles::layerFromString(layerString);
                    if (layer == FurnitureTiles::InvalidLayer) {
                        mError = tr("Invalid furniture layer '%1'").arg(layerString);
                        return false;
                    }

                    FurnitureTiles *tiles = new FurnitureTiles(corners);
                    tiles->setLayer(layer);
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
                                if (x < 0 || x >= 50 || y < 0 || y >= 50) {
                                    mError = tr("Invalid tile coordinates (%1,%2)")
                                            .arg(x).arg(y);
                                    return false;
                                }
                                QString tilesetName;
                                int tileIndex;
                                if (!BuildingTilesMgr::parseTileName(kv.value, tilesetName, tileIndex)) {
                                    mError = tr("Can't parse tile name '%1'").arg(kv.value);
                                    return false;
                                }
                                tile->setTile(x, y, BuildingTilesMgr::instance()->get(kv.value));
                            }
                            tiles->setTile(tile);
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
        foreach (FurnitureTiles *ftiles, group->mTiles) {
            SimpleFileBlock furnitureBlock;
            furnitureBlock.name = QLatin1String("furniture");
            if (ftiles->hasCorners())
                furnitureBlock.addValue("corners", QLatin1String("true"));
            if (ftiles->layer() != FurnitureTiles::LayerFurniture)
                furnitureBlock.addValue("layer", ftiles->layerToString());
            foreach (FurnitureTile *ftile, ftiles->tiles()) {
                if (ftile->isEmpty())
                    continue;
                SimpleFileBlock entryBlock;
                entryBlock.name = QLatin1String("entry");
                entryBlock.values += SimpleFileKeyValue(QLatin1String("orient"),
                                                        ftile->orientToString());
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
            groupBlock.blocks += furnitureBlock;
        }
        simpleFile.blocks += groupBlock;
    }
    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

int FurnitureGroups::indexOf(const QString &name) const
{
    int index = 0;
    foreach (FurnitureGroup *group, mGroups) {
        if (group->mLabel == name)
            return index;
        ++index;
    }
    return -1;
}

void FurnitureGroups::tileChanged(FurnitureTile *ftile)
{
    emit furnitureTileChanged(ftile);
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

bool FurnitureGroups::upgradeTxt()
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

/////

FurnitureTile::FurnitureTile(FurnitureTiles *ftiles, FurnitureOrientation orient) :
    mOwner(ftiles),
    mOrient(orient),
    mSize(1, 1),
    mTiles(1, 0)
{
}

void FurnitureTile::clear()
{
    mTiles.fill(0, 1);
    mSize = QSize(1, 1);
}

QSize FurnitureTile::size() const
{
    return mSize;
}

bool FurnitureTile::equals(FurnitureTile *other) const
{
    return other->mTiles == mTiles &&
            other->mOrient == mOrient &&
            other->mSize == mSize;
}

void FurnitureTile::setTile(int x, int y, BuildingTile *btile)
{
    // Get larger if needed
    if ((btile != 0) && (x >= mSize.width() || y >= mSize.height())) {
        resize(qMax(mSize.width(), x + 1), qMax(mSize.height(), y + 1));
    }

    mTiles[x + y * mSize.width()] = btile;

    // Get smaller if needed
    if ((btile == 0) && (x == mSize.width() - 1 || y == mSize.height() - 1)) {
        int width = mSize.width(), height = mSize.height();
        while (width > 1 && columnEmpty(width - 1))
            width--;
        while (height > 1 && rowEmpty(height - 1))
            height--;
        if (width < mSize.width() || height < mSize.height())
            resize(width, height);
    }
}

BuildingTile *FurnitureTile::tile(int x, int y) const
{
    if (x + y * mSize.width() >= mTiles.size())
        return 0;
    return mTiles[x + y * mSize.width()];
}

bool FurnitureTile::isEmpty() const
{
    foreach (BuildingTile *btile, mTiles)
        if (btile != 0)
            return false;
    return true;
}

FurnitureTile *FurnitureTile::resolved()
{
    if (isEmpty()) {
        if (mOrient == FurnitureE)
            return mOwner->tile(FurnitureW);
        if (mOrient == FurnitureN)
            return mOwner->tile(FurnitureW);
        if (mOrient == FurnitureS) {
            if (!mOwner->tile(FurnitureN)->isEmpty())
                return mOwner->tile(FurnitureN);
            return mOwner->tile(FurnitureW);
        }
    }
    return this;
}

bool FurnitureTile::isCornerOrient(FurnitureTile::FurnitureOrientation orient)
{
    return orient == FurnitureSW || orient == FurnitureSE ||
            orient == FurnitureNW || orient == FurnitureNE;
}

void FurnitureTile::resize(int width, int height)
{
    QVector<BuildingTile*> newTiles(width * height);
    for (int i = 0; i < qMin(width, mSize.width()); i++)
        for (int j = 0; j < qMin(height, mSize.height()); j++)
            newTiles[i + j * width] = mTiles[i + j * mSize.width()];
    mTiles = newTiles;
    mSize = QSize(width, height);
}

bool FurnitureTile::columnEmpty(int x)
{
    for (int y = 0; y < mSize.height(); y++)
        if (tile(x, y))
            return false;
    return true;
}

bool FurnitureTile::rowEmpty(int y)
{
    for (int x = 0; x < mSize.width(); x++)
        if (tile(x, y))
            return false;
    return true;
}

/////

FurnitureTiles::FurnitureTiles(bool corners) :
    mTiles(8, 0),
    mCorners(corners),
    mLayer(LayerFurniture)
{
    mTiles[FurnitureTile::FurnitureW] = new FurnitureTile(this, FurnitureTile::FurnitureW);
    mTiles[FurnitureTile::FurnitureN] = new FurnitureTile(this, FurnitureTile::FurnitureN);
    mTiles[FurnitureTile::FurnitureE] = new FurnitureTile(this, FurnitureTile::FurnitureE);
    mTiles[FurnitureTile::FurnitureS] = new FurnitureTile(this, FurnitureTile::FurnitureS);
    mTiles[FurnitureTile::FurnitureSW] = new FurnitureTile(this, FurnitureTile::FurnitureSW);
    mTiles[FurnitureTile::FurnitureNW] = new FurnitureTile(this, FurnitureTile::FurnitureNW);
    mTiles[FurnitureTile::FurnitureNE] = new FurnitureTile(this, FurnitureTile::FurnitureNE);
    mTiles[FurnitureTile::FurnitureSE] = new FurnitureTile(this, FurnitureTile::FurnitureSE);
}

FurnitureTiles::~FurnitureTiles()
{
    qDeleteAll(mTiles);
}

bool FurnitureTiles::isEmpty() const
{
    for (int i = 0; i < mTiles.size(); i++)
        if (!mTiles.isEmpty())
            return false;
    return true;
}

void FurnitureTiles::setTile(FurnitureTile *ftile)
{
    delete mTiles[ftile->orient()];
    mTiles[ftile->orient()] = ftile;
}

FurnitureTile *FurnitureTiles::tile(FurnitureTile::FurnitureOrientation orient) const
{
    return mTiles[orient];
}

bool FurnitureTiles::equals(const FurnitureTiles *other)
{
    if (other->mLayer != mLayer)
        return false;

    for (int i = 0; i < mTiles.size(); i++)
        if (!other->mTiles[i]->equals(mTiles[i]))
            return false;
    return true;
}

QString FurnitureTiles::layerToString(FurnitureTiles::FurnitureLayer layer)
{
    switch (layer) {
    case LayerFurniture:
    case LayerWallOverlay:
    case LayerWallFurniture:
        initNames();
        return mLayerNames[layer];
    default:
        return QLatin1String("Invalid");
    }
}

FurnitureTiles::FurnitureLayer FurnitureTiles::layerFromString(const QString &s)
{
    if (s == QLatin1String("Furniture")) return LayerFurniture;
    if (s == QLatin1String("WallOverlay")) return LayerWallOverlay;
    if (s == QLatin1String("WallFurniture")) return LayerWallFurniture;
    return InvalidLayer;
}

QStringList FurnitureTiles::mLayerNames;

void FurnitureTiles::initNames()
{
    if (mLayerNames.size())
        return;
    mLayerNames += QLatin1String("Furniture");
    mLayerNames += QLatin1String("WallOverlay");
    mLayerNames += QLatin1String("WallFurniture");
}

/////

FurnitureTiles *FurnitureGroup::findMatch(FurnitureTiles *ftiles) const
{
    foreach (FurnitureTiles *candidate, mTiles) {
        if (candidate->equals(ftiles))
            return candidate;
    }
    return 0;
}
