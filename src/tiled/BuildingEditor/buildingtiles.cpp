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

#include "buildingtiles.h"

#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingreader.h"
#include "buildingtilesfile.h"

#include "preferences.h"
#include "tiledeffile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMessageBox>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "BuildingTiles.txt";

/////

BuildingTilesMgr *BuildingTilesMgr::mInstance = 0;

BuildingTilesMgr *BuildingTilesMgr::instance()
{
    if (!mInstance)
        mInstance = new BuildingTilesMgr;
    return mInstance;
}

void BuildingTilesMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTilesMgr::BuildingTilesMgr() :
    mMissingTile(0),
    mNoneTiledTile(0),
    mNoneBuildingTile(0),
    mNoneCategory(0),
    mNoneTileEntry(0)
{
    QVector<BuildingTileCategory *> categories;
    createCategories(categories);

    mCatCurtains = static_cast<BTC_Curtains*>(categories[CategoryEnum::Curtains]);
    mCatDoors = static_cast<BTC_Doors*>(categories[CategoryEnum::Doors]);
    mCatDoorFrames = static_cast<BTC_DoorFrames*>(categories[CategoryEnum::DoorFrames]);
    mCatFloors = static_cast<BTC_Floors*>(categories[CategoryEnum::Floors]);
    mCatEWalls = static_cast<BTC_EWalls*>(categories[CategoryEnum::ExteriorWalls]);
    mCatIWalls = static_cast<BTC_IWalls*>(categories[CategoryEnum::InteriorWalls]);
    mCatEWallTrim = static_cast<BTC_EWallTrim*>(categories[CategoryEnum::ExteriorWallTrim]);
    mCatIWallTrim = static_cast<BTC_IWallTrim*>(categories[CategoryEnum::InteriorWallTrim]);
    mCatStairs = static_cast<BTC_Stairs*>(categories[CategoryEnum::Stairs]);
    mCatShutters = static_cast<BTC_Shutters*>(categories[CategoryEnum::Shutters]);
    mCatWindows = static_cast<BTC_Windows*>(categories[CategoryEnum::Windows]);
    mCatGrimeFloor = static_cast<BTC_GrimeFloor*>(categories[CategoryEnum::GrimeFloor]);
    mCatGrimeWall = static_cast<BTC_GrimeWall*>(categories[CategoryEnum::GrimeWall]);
    mCatRoofCaps = static_cast<BTC_RoofCaps*>(categories[CategoryEnum::RoofCaps]);
    mCatRoofSlopes = static_cast<BTC_RoofSlopes*>(categories[CategoryEnum::RoofSlopes]);
    mCatRoofTops = static_cast<BTC_RoofTops*>(categories[CategoryEnum::RoofTops]);
    mCatCeiling = static_cast<BTC_Ceiling*>(categories[CategoryEnum::Ceiling]);

    mCategories << mCatEWalls << mCatIWalls << mCatEWallTrim << mCatIWallTrim
                   << mCatFloors << mCatDoors << mCatDoorFrames << mCatWindows
                   << mCatCurtains << mCatShutters << mCatStairs
                   << mCatGrimeFloor << mCatGrimeWall
                   << mCatRoofCaps << mCatRoofSlopes << mCatRoofTops
                   << mCatCeiling;

    for (BuildingTileCategory *category : std::as_const(mCategories)) {
        mCategoryByName[category->name()] = category;
    }

    mCatEWalls->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_walls.png")));
    mCatIWalls->setShadowImage(mCatEWalls->shadowImage());
    mCatEWallTrim->setShadowImage(mCatEWalls->shadowImage());
    mCatIWallTrim->setShadowImage(mCatEWalls->shadowImage());
    mCatFloors->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_floors.png")));
    mCatDoors->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_doors.png")));
    mCatDoorFrames->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_door_frames.png")));
    mCatWindows->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_windows.png")));
    mCatCurtains->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_curtains.png")));
    mCatShutters->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_shutters.png")));
    mCatStairs->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_stairs.png")));
    mCatGrimeFloor->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_grime_floor.png")));
    mCatGrimeWall->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_grime_wall.png")));
    mCatCeiling->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_floors.png")));
    mCatRoofCaps->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_caps.png")));
    mCatRoofSlopes->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_slopes.png")));
    mCatRoofTops->setShadowImage(QImage(QLatin1String(":/BuildingEditor/icons/shadow_roof_tops.png")));

    mMissingTile = TilesetManager::instance()->missingTile();

    Tileset *tileset = new Tileset(QLatin1String("none"), 64, 128);
    tileset->setTransparentColor(Qt::white);
    QString fileName = QLatin1String(":/BuildingEditor/icons/none-tile.png");
    if (tileset->loadFromImage(QImage(fileName), fileName))
        mNoneTiledTile = tileset->tileAt(0);
    else {
        QImage image(64, 128, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, fileName);
        mNoneTiledTile = tileset->tileAt(0);
    }

    mNoneBuildingTile = new NoneBuildingTile();

    mNoneCategory = new NoneBuildingTileCategory();
    mNoneTileEntry = new NoneBuildingTileEntry(mNoneCategory);

    // Forward these signals (backwards compatibility).
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded,
            this, &BuildingTilesMgr::tilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAboutToBeRemoved,
            this, &BuildingTilesMgr::tilesetAboutToBeRemoved);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved,
             this, &BuildingTilesMgr::tilesetRemoved);
}

BuildingTilesMgr::~BuildingTilesMgr()
{
    qDeleteAll(mCategories);
    if (mNoneTiledTile)
        delete mNoneTiledTile->tileset();
    delete mNoneBuildingTile;
}

BuildingTile *BuildingTilesMgr::add(const QString &tileName)
{
    QString tilesetName;
    int tileIndex = 0;
    parseTileName(tileName, tilesetName, tileIndex);
    BuildingTile *btile = new BuildingTile(tilesetName, tileIndex);
    Q_ASSERT(!mTileByName.contains(btile->name()));
    mTileByName[btile->name()] = btile;
    mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
    return btile;
}

BuildingTile *BuildingTilesMgr::get(const QString &tileName, int offset)
{
    if (tileName.isEmpty())
        return noneTile();

    QString adjustedName = adjustTileNameIndex(tileName, offset); // also normalized

    if (!mTileByName.contains(adjustedName))
        add(adjustedName);
    return mTileByName[adjustedName];
}

void BuildingTilesMgr::createCategories(QVector<BuildingTileCategory *> &categories)
{
    qDeleteAll(categories);
    categories.resize(CategoryEnum::Count);
    categories[CategoryEnum::Curtains] = new BTC_Curtains(QLatin1String("Curtains"));
    categories[CategoryEnum::Doors] = new BTC_Doors(QLatin1String("Doors"));
    categories[CategoryEnum::DoorFrames] = new BTC_DoorFrames(QLatin1String("Door Frames"));
    categories[CategoryEnum::Floors] = new BTC_Floors(QLatin1String("Floors"));
    categories[CategoryEnum::ExteriorWalls] = new BTC_EWalls(QLatin1String("Exterior Walls"));
    categories[CategoryEnum::InteriorWalls] = new BTC_IWalls(QLatin1String("Interior Walls"));
    categories[CategoryEnum::ExteriorWallTrim] = new BTC_EWallTrim(QLatin1String("Trim - Exterior Walls"));
    categories[CategoryEnum::InteriorWallTrim] = new BTC_IWallTrim(QLatin1String("Trim - Interior Walls"));
    categories[CategoryEnum::Stairs] = new BTC_Stairs(QLatin1String("Stairs"));
    categories[CategoryEnum::Shutters] = new BTC_Shutters(QLatin1String("Shutters"));
    categories[CategoryEnum::Windows] = new BTC_Windows(QLatin1String("Windows"));
    categories[CategoryEnum::GrimeFloor] = new BTC_GrimeFloor(QLatin1String("Grime - Floors"));
    categories[CategoryEnum::GrimeWall] = new BTC_GrimeWall(QLatin1String("Grime - Walls"));
    categories[CategoryEnum::RoofCaps] = new BTC_RoofCaps(QLatin1String("Roof Caps"));
    categories[CategoryEnum::RoofSlopes] = new BTC_RoofSlopes(QLatin1String("Roof Slopes"));
    categories[CategoryEnum::RoofTops] = new BTC_RoofTops(QLatin1String("Roof Tops"));
    categories[CategoryEnum::Ceiling] = new BTC_Ceiling(QLatin1String("Ceilings"));
}

QString BuildingTilesMgr::nameForTile(const QString &tilesetName, int index)
{
    // The only reason I'm padding the tile index is so that the tiles are sorted
    // by increasing tileset name and index.
    return tilesetName + QLatin1Char('_') +
            QString(QLatin1String("%1")).arg(index, 3, 10, QLatin1Char('0'));
}

QString BuildingTilesMgr::nameForTile2(const QString &tilesetName, int index)
{
    return tilesetName + QLatin1Char('_') + QString::number(index);
}

QString BuildingTilesMgr::nameForTile(Tile *tile)
{
    return nameForTile(tile->tileset()->name(), tile->id());
}

bool BuildingTilesMgr::parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    int n = tileName.lastIndexOf(QLatin1Char('_'));
    if (n == -1)
        return false;
    tilesetName = tileName.mid(0, n);
    QString indexString = tileName.mid(n + 1);
    // Strip leading zeroes from the tile index
#if 1
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);
#else
    indexString.remove( QRegExp(QLatin1String("^[0]*")) );
#endif
    bool ok;
    index = indexString.toUInt(&ok);
    return !tilesetName.isEmpty() && ok;
}

bool BuildingTilesMgr::legalTileName(const QString &tileName)
{
    QString tilesetName;
    int index;
    return parseTileName(tileName, tilesetName, index);
}

QString BuildingTilesMgr::adjustTileNameIndex(const QString &tileName, int offset)
{
    QString tilesetName;
    int index = 0;
    parseTileName(tileName, tilesetName, index);

    // Currently, the only place this gets called with offset > 0 is by the
    // createEntryFromSingleTile() methods.  Those methods assume the tilesets
    // are 8 tiles wide.  Remap the offset onto the tileset's actual number of
    // columns.
    if (offset > 0) {
        if (Tileset *ts = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            TileMetaInfoMgr::instance()->loadTilesets(QList<Tileset*>() << ts);
            int rows = offset / 8;
            offset = rows * ts->columnCount() + offset % 8;
        }
    }

    index += offset;
    return nameForTile(tilesetName, index);
}

QString BuildingTilesMgr::normalizeTileName(const QString &tileName)
{
    if (tileName.isEmpty())
        return tileName;
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    return nameForTile(tilesetName, index);
}

void BuildingTilesMgr::entryTileChanged(BuildingTileEntry *entry, int e)
{
    Q_UNUSED(e)
    emit entryTileChanged(entry);
}

QString BuildingTilesMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTilesMgr::txtPath()
{
#ifdef WORLDED
    return Preferences::instance()->configPath(txtName());
#else
    return BuildingPreferences::instance()->configPath(txtName());
#endif
}

// VERSION0: original format without 'version' keyvalue
#define VERSION0 0

// VERSION1
// added 'version' keyvalue
// added 'curtains' category
#define VERSION1 1

// VERSION2
// massive rewrite!
#define VERSION2 2
#define VERSION_LATEST VERSION2

bool BuildingTilesMgr::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }
#if !defined(WORLDED)
    if (!mergeTxt())
        return false;
#endif
    QString path = info.canonicalFilePath();
    BuildingTilesFile file;
    if (!file.read(path)) {
        mError = file.errorString();
        return false;
    }

    if (file.getVersion() != BuildingTilesFile::getVersionLatest()) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(BuildingTilesFile::getVersionLatest()).arg(file.getVersion());
        return false;
    }

    mRevision = file.getRevision();
    mSourceRevision = file.getSourceRevision();

    for (int i = 0; i < CategoryEnum::Count; i++) {
        BuildingTileCategory *sourceCategory = file.categories().at(i);
        BuildingTileCategory *category = mCategories.at(i);
        for (BuildingTileEntry *sourceEntry : sourceCategory->entries()) {
            category->insertEntry(category->entryCount(), sourceEntry->createCopy(category));
        }
    }
#if 0
    // Check that all the tiles exist
    foreach (BuildingTileCategory *category, categories()) {
        foreach (BuildingTileEntry *entry, category->entries()) {
            for (int i = 0; i < category->enumCount(); i++) {
                if (tileFor(entry->tile(i)) == mMissingTile) {
                    mError = tr("Tile %1 #%2 doesn't exist.")
                            .arg(entry->tile(i)->mTilesetName)
                            .arg(entry->tile(i)->mIndex);
                    return false;
                }
            }
        }
    }
#endif
    for (BuildingTileCategory *category : std::as_const(mCategories)) {
        category->setDefaultEntry(category->entry(0));
    }
    mCatCurtains->setDefaultEntry(noneTileEntry());

    return true;
}

void BuildingTilesMgr::writeTxt(QWidget *parent)
{
#ifdef WORLDED
    return;
#endif

    BuildingTilesFile file;
    if (!file.write(txtPath(), mRevision + 1, mSourceRevision, categories().toVector())) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"), file.errorString());
        return;
    }
    ++mRevision;
}

int BuildingTilesMgr::setRevision(int revision)
{
    int old = mRevision;
    mRevision = revision;
    return old;
}

int BuildingTilesMgr::setSourceRevision(int sourceRevision)
{
    int old = mSourceRevision;
    mSourceRevision = sourceRevision;
    return old;
}

BuildingTileEntry *BuildingTilesMgr::defaultCategoryTile(int e) const
{
    return mCategories[e]->defaultEntry();
}

bool BuildingTilesMgr::mergeTxt()
{
    QString userPath = txtPath();
    BuildingTilesFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    QString sourcePath = Tiled::Internal::Preferences::instance()->appConfigPath(txtName());
    BuildingTilesFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.getVersion() == BuildingTilesFile::getVersionLatest());

    int sourceVersion = sourceFile.getVersion();
    int sourceRevision = sourceFile.getRevision();
    int userVersion = userFile.getVersion();
    int userSourceRevision = userFile.getSourceRevision();
    if (userVersion == sourceVersion && sourceRevision == userSourceRevision) {
        return true;
    }

    // MERGE HERE

    for (int i = 0; i < CategoryEnum::Count; i++) {
        BuildingTileCategory *sourceCategory = sourceFile.categories().at(i);
        BuildingTileCategory *userCategory = userFile.categories().at(i);
        // Copy unique source-entries to the user-category.
        for (BuildingTileEntry *sourceEntry : sourceCategory->entries()) {
            if (BuildingTileEntry *userEntry = userCategory->findMatchIgnoreCategory(sourceEntry, userVersion)) {
                userEntry->set(sourceEntry); // copy any new tiles
                continue;
            }
            BuildingTileEntry *userEntry = sourceEntry->createCopy(userCategory);
            userCategory->insertEntry(userCategory->entryCount(), userEntry);
        }
    }

    if (!userFile.write(userPath, sourceRevision + 1, sourceRevision, userFile.categories())) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

Tiled::Tile *BuildingTilesMgr::tileFor(const QString &tileName)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tilesetName);
    if (!tileset)
        return mMissingTile;
    if (index >= tileset->tileCount())
        return mMissingTile;
    return tileset->tileAt(index);
}

Tile *BuildingTilesMgr::tileFor(BuildingTile *tile, int offset)
{
    if (tile->isNone())
        return mNoneTiledTile;
    Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tile->mTilesetName);
    if (!tileset)
        return mMissingTile;
    if (tile->mIndex + offset >= tileset->tileCount())
        return tileset->isMissing() ? tileset->tileAt(0) : mMissingTile;
    return tileset->tileAt(tile->mIndex + offset);
}

BuildingTile *BuildingTilesMgr::fromTiledTile(Tile *tile)
{
    if (tile == mNoneTiledTile)
        return mNoneBuildingTile;
    return get(nameForTile(tile));
}

BuildingTileEntry *BuildingTilesMgr::defaultExteriorWall() const
{
    return mCatEWalls->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultInteriorWall() const
{
    return mCatIWalls->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultExteriorWallTrim() const
{
    return mCatEWallTrim->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultInteriorWallTrim() const
{
    return mCatIWallTrim->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultFloorTile() const
{
    return mCatFloors->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorTile() const
{
    return mCatDoors->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorFrameTile() const
{
    return mCatDoorFrames->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultWindowTile() const
{
    return mCatWindows->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultCurtainsTile() const
{
    return mCatCurtains->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultStairsTile() const
{
    return mCatStairs->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultCeilingTile() const
{
    return mCatCeiling->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofCapTiles() const
{
    return mCatRoofCaps->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofSlopeTiles() const
{
    return mCatRoofSlopes->defaultEntry();
}

BuildingTileEntry *BuildingTilesMgr::defaultRoofTopTiles() const
{
    return mCatRoofTops->defaultEntry();
}

/////

QString BuildingTile::name() const
{
    return BuildingTilesMgr::nameForTile(mTilesetName, mIndex);
}

/////

BuildingTileEntry::BuildingTileEntry(BuildingTileCategory *category) :
    mCategory(category)
{
    if (category) {
        mTiles.resize(category->enumCount());
        mOffsets.resize(category->enumCount());
        for (int i = 0; i < mTiles.size(); i++)
            mTiles[i] = BuildingTilesMgr::instance()->noneTile();
    }
}

void BuildingTileEntry::set(const BuildingTileEntry *other)
{
    // NOTE: Not changing mCategory
    mTiles = other->mTiles;
    mOffsets = other->mOffsets;
}

BuildingTileEntry *BuildingTileEntry::createCopy(BuildingTileCategory *category) const
{
    BuildingTileEntry *copy = new BuildingTileEntry(category);
    copy->mTiles = mTiles;
    copy->mOffsets = mOffsets;
    return copy;
}

BuildingTile *BuildingTileEntry::displayTile() const
{
    return tile(mCategory->displayIndex());
}

void BuildingTileEntry::setTile(int e, BuildingTile *btile)
{
    Q_ASSERT(btile);
    mTiles[e] = btile;
}

BuildingTile *BuildingTileEntry::tile(int n) const
{
    if (n < 0 || n >= mTiles.size())
        return BuildingTilesMgr::instance()->noneTile();
    return mTiles[n];
}

void BuildingTileEntry::setOffset(int e, const QPoint &offset)
{
    mOffsets[e] = offset;
}

QPoint BuildingTileEntry::offset(int e) const
{
    if (isNone())
        return QPoint();
    if (e < 0 || e >= mOffsets.size()) {
        Q_ASSERT(false);
        return QPoint();
    }
    return mOffsets[e];
}

bool BuildingTileEntry::usesTile(BuildingTile *btile) const
{
    return mTiles.contains(btile);
}

bool BuildingTileEntry::equals(BuildingTileEntry *other) const
{
    return (mCategory == other->mCategory) &&
            (mTiles == other->mTiles) &&
            (mOffsets == other->mOffsets);
}

bool BuildingTileEntry::equals(BuildingTileEntry *other, const QVector<int> &enums) const
{
    if (mCategory != other->mCategory) {
        return false;
    }
    for (int e : enums) {
        if (mTiles[e] != other->mTiles[e]) {
            return false;
        }
        if (mOffsets[e] != other->mOffsets[e]) {
            return false;
        }
    }
    return true;
}

bool BuildingTileEntry::equalsIgnoreCategory(BuildingTileEntry *other, const QVector<int> &enums) const
{
    if (mCategory->name() != other->mCategory->name()) {
        return false;
    }
    for (int e : enums) {
        if (mTiles[e] != other->mTiles[e]) {
            return false;
        }
        if (mOffsets[e] != other->mOffsets[e]) {
            return false;
        }
    }
    return true;
}

bool BuildingTileEntry::isNorth(int e) const
{
    return mCategory->isNorth(e);
}

bool BuildingTileEntry::isWest(int e) const
{
    return mCategory->isWest(e);
}

int BuildingTileEntry::wallEnum(int e) const
{
    return mCategory->wallEnum(this, e);
}

BuildingTileEntry *BuildingTileEntry::asCategory(int n)
{
    return (mCategory->name() == BuildingTilesMgr::instance()->category(n)->name())
            ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asExteriorWall()
{
    return mCategory->asExteriorWalls() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asInteriorWall()
{
    return mCategory->asInteriorWalls() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asExteriorWallTrim()
{
    return mCategory->asExteriorWallTrim() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asInteriorWallTrim()
{
    return mCategory->asInteriorWallTrim() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asFloor()
{
    return mCategory->asFloors() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asDoor()
{
    return mCategory->asDoors() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asDoorFrame()
{
    return mCategory->asDoorFrames() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asWindow()
{
    return mCategory->asWindows() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asCurtains()
{
    return mCategory->asCurtains() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asShutters()
{
    return mCategory->asShutters() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asStairs()
{
    return mCategory->asStairs() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asCeiling()
{
    return mCategory->asCeiling() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofCap()
{
    return mCategory->asRoofCaps() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofSlope()
{
    return mCategory->asRoofSlopes() ? this : 0;
}

BuildingTileEntry *BuildingTileEntry::asRoofTop()
{
    return mCategory->asRoofTops() ? this : 0;
}

/////

BTC_Curtains::BTC_Curtains(const QString &label) :
    BuildingTileCategory(QLatin1String("curtains"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("East");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("South");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Curtains::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[East] = getTile(tileName, 1);
    entry->mTiles[North] = getTile(tileName, 2);
    entry->mTiles[South] = getTile(tileName, 3);
    return entry;
}

int BTC_Curtains::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, East, North, South
    };
    return map[shadowIndex];
}

/////

BTC_Shutters::BTC_Shutters(const QString &label) :
    BuildingTileCategory(QLatin1String("shutters"), label, NorthLeft)
{
    mEnumNames += QLatin1String("WestBelow");
    mEnumNames += QLatin1String("WestAbove");
    mEnumNames += QLatin1String("NorthLeft");
    mEnumNames += QLatin1String("NorthRight");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Shutters::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[WestBelow] = getTile(tileName, 0);
    entry->mTiles[WestAbove] = getTile(tileName, 1);
    entry->mTiles[NorthLeft] = getTile(tileName, 2);
    entry->mTiles[NorthRight] = getTile(tileName, 3);
    return entry;
}

int BTC_Shutters::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        WestBelow, WestAbove, NorthLeft, NorthRight
    };
    return map[shadowIndex];
}

/////

BTC_Doors::BTC_Doors(const QString &label) :
    BuildingTileCategory(QLatin1String("doors"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("WestOpen");
    mEnumNames += QLatin1String("NorthOpen");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Doors::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[WestOpen] = getTile(tileName, 2);
    entry->mTiles[NorthOpen] = getTile(tileName, 3);
    return entry;
}

int BTC_Doors::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North, WestOpen, NorthOpen
    };
    return map[shadowIndex];
}

bool BTC_Doors::isNorth(int e) const
{
    return e == TileEnum::North || e == TileEnum::NorthOpen;
}

bool BTC_Doors::isWest(int e) const
{
    return e == TileEnum::West || e == TileEnum::WestOpen;
}

int BTC_Doors::wallEnum(const BuildingTileEntry *entry, int e) const
{
    switch (e) {
    case TileEnum::North:
    case TileEnum::NorthOpen:
        return BTC_Walls::TileEnum::NorthDoor;
    case TileEnum::West:
    case TileEnum::WestOpen:
        return BTC_Walls::TileEnum::WestDoor;
    default:
        return BuildingTileCategory::wallEnum(entry, e);
    }
}

/////

BTC_DoorFrames::BTC_DoorFrames(const QString &label) :
    BuildingTileCategory(QLatin1String("door_frames"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_DoorFrames::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    return entry;
}

int BTC_DoorFrames::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West, North
    };
    return map[shadowIndex];
}

/////

BTC_Floors::BTC_Floors(const QString &label) :
    BuildingTileCategory(QLatin1String("floors"), label, Floor)
{
    mEnumNames += QLatin1String("Floor");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Floors::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[Floor] = getTile(tileName);
    return entry;
}

int BTC_Floors::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        Floor
    };
    return map[shadowIndex];
}

/////

BTC_Stairs::BTC_Stairs(const QString &label) :
    BuildingTileCategory(QLatin1String("stairs"), label, West1)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Stairs::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = getTile(tileName);
    entry->mTiles[West2] = getTile(tileName, 1);
    entry->mTiles[West3] = getTile(tileName, 2);
    entry->mTiles[North1] = getTile(tileName, 8);
    entry->mTiles[North2] = getTile(tileName, 9);
    entry->mTiles[North3] = getTile(tileName, 10);
    return entry;
}

int BTC_Stairs::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount] = {
        West1, West2, West3, North1, North2, North3
    };
    return map[shadowIndex];
}

/////

BTC_Walls::BTC_Walls(const QString &name, const QString &label) :
    BuildingTileCategory(name, label, West)
{
    mEnumNames += QStringLiteral("West");
    mEnumNames += QStringLiteral("North");
    mEnumNames += QStringLiteral("NorthWest");
    mEnumNames += QStringLiteral("SouthEast");
    mEnumNames += QStringLiteral("WestWindow");
    mEnumNames += QStringLiteral("NorthWindow");
    mEnumNames += QStringLiteral("WestDoor");
    mEnumNames += QStringLiteral("NorthDoor");
    for (int i = 1; i <= NUM_WINDOW_FRAMES; i++) {
        mEnumNames += QStringLiteral("WestWindow%1").arg(i);
        mEnumNames += QStringLiteral("NorthWindow%1").arg(i);
    }
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Walls::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[SouthEast] = getTile(tileName, 3);
    entry->mTiles[WestWindow] = getTile(tileName, 8);
    entry->mTiles[NorthWindow] = getTile(tileName, 9);
    entry->mTiles[WestDoor] = getTile(tileName, 10);
    entry->mTiles[NorthDoor] = getTile(tileName, 11);
    return entry;
}

int BTC_Walls::shadowToEnum(int shadowIndex)
{
    int map[EnumCount + 2] = {
        West, North, NorthWest, SouthEast, WestWindow, NorthWindow, WestDoor, NorthDoor
    };
    for (int i = 0; i < 16; i++) {
        map[WestWindow1 + i * 2] = WestWindow1 + i * 2;
        map[NorthWindow1 + i * 2] = NorthWindow1 + i * 2;
    }
    // Order is west west north north
    map[40] = WestWindow17; // west trailer left
    map[41] = WestWindow18; // west trailer right
    map[42] = NorthWindow17; // north trailer left
    map[43] = NorthWindow18; // north trailer right
    map[44] = WestWindow19; // tall skinny round top
    map[45] = NorthWindow19;// tall skinny round top
    map[46] = -1;
    map[47] = -1;
    return map[shadowIndex];
}

bool BTC_Walls::isNorth(int e) const
{
    if (e == TileEnum::North || e == TileEnum::NorthDoor || e == TileEnum::NorthWindow) {
        return true;
    }
    for (int i = 0; i < NUM_WINDOW_FRAMES; i++) {
        if (e == TileEnum::NorthWindow1 + i * 2) {
            return true;
        }
    }
    return false;
}

bool BTC_Walls::isWest(int e) const
{
    if (e == TileEnum::West || e == TileEnum::WestDoor || e == TileEnum::WestWindow) {
        return true;
    }
    for (int i = 0; i < NUM_WINDOW_FRAMES; i++) {
        if (e == TileEnum::WestWindow1 + i * 2) {
            return true;
        }
    }
    return false;
}

static QMap<QString,BTC_Walls::TileEnum> WindowShapeW;
static QMap<QString,BTC_Walls::TileEnum> WindowShapeN;

BTC_Walls::TileEnum BTC_Walls::windowShapeToEnumW(const QString &windowShape)
{
    if (WindowShapeW.isEmpty()) {
        for (int i = 0; i < NUM_WINDOW_FRAMES; i++) {
            WindowShapeW.insert(QStringLiteral("%1").arg(i + 1), static_cast<TileEnum>(TileEnum::WestWindow1 + i * 2));
        }
    }
    return WindowShapeW.value(windowShape, TileEnum::WestWindow);
}

BTC_Walls::TileEnum BTC_Walls::windowShapeToEnumN(const QString &windowShape)
{
    if (WindowShapeN.isEmpty()) {
        for (int i = 0; i < NUM_WINDOW_FRAMES; i++) {
            WindowShapeN.insert(QStringLiteral("%1").arg(i + 1), static_cast<TileEnum>(TileEnum::NorthWindow1 + i * 2));
        }
    }
    return WindowShapeN.value(windowShape, TileEnum::NorthWindow);
}

/////

QVector<int> BTC_EWalls::enumsForVersion(int buildingTilesFileVersion) const
{
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION3) {
        // Version 3 added window-frame shapes 1-16 and 30-degree roofs.
        // Look for a match of the first 8 tiles only.
        return { West, North, NorthWest, SouthEast, WestWindow, NorthWindow, WestDoor, NorthDoor };
    }
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION4) {
        // Version 4 added window-frame shapes 17-19.
        QVector<int> ret;
        for (int i = 0; i < WestWindow17; i++) {
            ret += i;
        }
        return ret;
    }
    return BuildingTileCategory::enumsForVersion(buildingTilesFileVersion);
}

/////

QVector<int> BTC_IWalls::enumsForVersion(int buildingTilesFileVersion) const
{
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION3) {
        // Version 3 added window-frame shapes 1-16 and 30-degree roofs.
        // Look for a match of the first 8 tiles only.
        return { West, North, NorthWest, SouthEast, WestWindow, NorthWindow, WestDoor, NorthDoor };
    }
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION4) {
        // Version 4 added window-frame shapes 17-19.
        QVector<int> ret;
        for (int i = 0; i < WestWindow17; i++) {
            ret += i;
        }
        return ret;
    }
    return BuildingTileCategory::enumsForVersion(buildingTilesFileVersion);
}

/////

BTC_Windows::BTC_Windows(const QString &label) :
    BuildingTileCategory(QLatin1String("windows"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_Windows::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName);
    entry->mTiles[North] = getTile(tileName, 1);
    return entry;
}

int BTC_Windows::shadowColumns() const
{
    return 2;
}

int BTC_Windows::shadowRows() const
{
    return 1;
}

int BTC_Windows::shadowToEnum(int shadowIndex)
{
    int map[EnumCount] = {
        West, North
    };
    return map[shadowIndex];
}

bool BTC_Windows::isNorth(int e) const
{
    return e == TileEnum::North;
}

bool BTC_Windows::isWest(int e) const
{
    return e == TileEnum::West;
}

int BTC_Windows::wallEnum(const BuildingTileEntry *entry, int e) const
{
    if (entry == nullptr || entry->isNone()) {
        return defaultWallEnum(entry, e);
    }
    BuildingTile *tile = entry->tile(e);
    if (tile == nullptr || tile->isNone()) {
        return defaultWallEnum(entry, e);
    }
    TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    TileDefTileset *tdTileset = tileDefWatcher->tileset(tile->mTilesetName);
    if (tdTileset == nullptr) {
        return defaultWallEnum(entry, e);
    }
    TileDefTile *tdTile = tdTileset->tileAt(tile->mIndex);
    if (tdTile == nullptr || !tdTile->mProperties.contains(QStringLiteral("WindowShape"))) {
        return defaultWallEnum(entry, e);
    }
    const QString WindowShape = tdTile->mProperties.value(QStringLiteral("WindowShape"));
    switch (e) {
    case TileEnum::West:
        return BTC_Walls::windowShapeToEnumW(WindowShape);
    case TileEnum::North:
        return BTC_Walls::windowShapeToEnumN(WindowShape);
    default:
        return defaultWallEnum(entry, e);
    }
}

bool BTC_Windows::shadowHack(const BuildingTileEntry *entry, int e, QPoint &p) const
{
    BuildingTile *tile = entry->tile(e);
    if (tile == nullptr) {
        return false;
    }
    TileDefWatcher *tileDefWatcher = getTileDefWatcher();
    TileDefTileset *tdTileset = tileDefWatcher->tileset(tile->mTilesetName);
    if (tdTileset == nullptr) {
        return false;
    }
    TileDefTile *tdTile = tdTileset->tileAt(tile->mIndex);
    if (tdTile == nullptr || !tdTile->mProperties.contains(QStringLiteral("WindowShape"))) {
        return false; // original 2 window shapes don't use the WindowShape property
    }
    bool ok = false;
    int WindowShape = tdTile->mProperties.value(QStringLiteral("WindowShape")).toInt(&ok);
    if (!ok || WindowShape < 1 || WindowShape > BTC_Walls::NUM_WINDOW_FRAMES) {
        return false;
    }
    if (WindowShape == 17) { // trailer left
        if (e == TileEnum::West) {
            p = QPoint(2, 0);
            return true;
        }
        if (e == TileEnum::North) {
            p = QPoint(4-1, 0);
            return true;
        }
        return false;
    }
    if (WindowShape == 18) { // trailer right
        if (e == TileEnum::West) {
            p = QPoint(3, 0);
            return true;
        }
        if (e == TileEnum::North) {
            p = QPoint(5-1, 0);
            return true;
        }
        return false;
    }
    if (WindowShape == 19) { // tall skinny round top
        if (e == TileEnum::West) {
            p = QPoint(6, 0);
            return true;
        }
        if (e == TileEnum::North) {
            p = QPoint(7-1, 0);
            return true;
        }
        return false;
    }
    // Shape 1-16 on rows 1-4
    WindowShape -= 1;
    switch (e) {
    case TileEnum::West:
        p = QPoint((WindowShape % 4) * shadowColumns(), 1 + (WindowShape / 4)); // first row doesn't include shapes 1-16
        return true;
    case TileEnum::North:
        p = QPoint((WindowShape % 4) * shadowColumns(), 1 + (WindowShape / 4)); // first row doesn't include shapes 1-16
        return true;
    default:
        return false;
    }
}

int BTC_Windows::defaultWallEnum(const BuildingTileEntry *entry, int e) const
{
    switch (e) {
    case TileEnum::West:
        return BTC_Walls::TileEnum::WestWindow;
    case TileEnum::North:
        return BTC_Walls::TileEnum::NorthWindow;
    default:
        return BuildingTileCategory::wallEnum(entry, e);
    }
}

/////

BTC_Ceiling::BTC_Ceiling(const QString &label) :
    BuildingTileCategory(QLatin1String("ceiling"), label, TileEnum::Ceiling)
{
    mEnumNames += QLatin1String("Ceiling");
    Q_ASSERT(mEnumNames.size() == TileEnum::EnumCount);
}

BuildingTileEntry *BTC_Ceiling::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[TileEnum::Ceiling] = getTile(tileName);
    return entry;
}

int BTC_Ceiling::shadowToEnum(int shadowIndex)
{
    const int map[TileEnum::EnumCount] = {
        TileEnum::Ceiling
    };
    return map[shadowIndex];
}

/////

BTC_RoofCaps::BTC_RoofCaps(const QString &label) :
    BuildingTileCategory(QLatin1String("roof_caps"), label, CapRiseE3)
{
    mEnumNames += QStringLiteral("CapRiseE1");
    mEnumNames += QStringLiteral("CapRiseE2");
    mEnumNames += QStringLiteral("CapRiseE3");
    mEnumNames += QStringLiteral("CapFallE1");
    mEnumNames += QStringLiteral("CapFallE2");
    mEnumNames += QStringLiteral("CapFallE3");

    mEnumNames += QStringLiteral("CapRiseS1");
    mEnumNames += QStringLiteral("CapRiseS2");
    mEnumNames += QStringLiteral("CapRiseS3");
    mEnumNames += QStringLiteral("CapFallS1");
    mEnumNames += QStringLiteral("CapFallS2");
    mEnumNames += QStringLiteral("CapFallS3");

    mEnumNames += QStringLiteral("PeakPt5S");
    mEnumNames += QStringLiteral("PeakPt5E");
    mEnumNames += QStringLiteral("PeakOnePt5S");
    mEnumNames += QStringLiteral("PeakOnePt5E");
    mEnumNames += QStringLiteral("PeakTwoPt5S");
    mEnumNames += QStringLiteral("PeakTwoPt5E");

    mEnumNames += QStringLiteral("CapGapS1");
    mEnumNames += QStringLiteral("CapGapS2");
    mEnumNames += QStringLiteral("CapGapS3");
    mEnumNames += QStringLiteral("CapGapE1");
    mEnumNames += QStringLiteral("CapGapE2");
    mEnumNames += QStringLiteral("CapGapE3");

    mEnumNames += QStringLiteral("CapShallowRiseS1");
    mEnumNames += QStringLiteral("CapShallowRiseS2");
    mEnumNames += QStringLiteral("CapShallowFallS1");
    mEnumNames += QStringLiteral("CapShallowFallS2");
    mEnumNames += QStringLiteral("CapShallowRiseE1");
    mEnumNames += QStringLiteral("CapShallowRiseE2");
    mEnumNames += QStringLiteral("CapShallowFallE1");
    mEnumNames += QStringLiteral("CapShallowFallE2");

    mEnumNames += QStringLiteral("CapSlope30RiseE1");
    mEnumNames += QStringLiteral("CapSlope30RiseE2");
    mEnumNames += QStringLiteral("CapSlope30RiseE3");
    mEnumNames += QStringLiteral("CapSlope30RiseE4");
    mEnumNames += QStringLiteral("CapSlope30RiseE5");
    mEnumNames += QStringLiteral("CapSlope30RiseE6");

    mEnumNames += QStringLiteral("CapSlope30FallE1");
    mEnumNames += QStringLiteral("CapSlope30FallE2");
    mEnumNames += QStringLiteral("CapSlope30FallE3");
    mEnumNames += QStringLiteral("CapSlope30FallE4");
    mEnumNames += QStringLiteral("CapSlope30FallE5");
    mEnumNames += QStringLiteral("CapSlope30FallE6");

    mEnumNames += QStringLiteral("CapSlope30RiseS1");
    mEnumNames += QStringLiteral("CapSlope30RiseS2");
    mEnumNames += QStringLiteral("CapSlope30RiseS3");
    mEnumNames += QStringLiteral("CapSlope30RiseS4");
    mEnumNames += QStringLiteral("CapSlope30RiseS5");
    mEnumNames += QStringLiteral("CapSlope30RiseS6");

    mEnumNames += QStringLiteral("CapSlope30FallS1");
    mEnumNames += QStringLiteral("CapSlope30FallS2");
    mEnumNames += QStringLiteral("CapSlope30FallS3");
    mEnumNames += QStringLiteral("CapSlope30FallS4");
    mEnumNames += QStringLiteral("CapSlope30FallS5");
    mEnumNames += QStringLiteral("CapSlope30FallS6");

    mEnumNames += QStringLiteral("CapPeak30E1");
    mEnumNames += QStringLiteral("CapPeak30E2");
    mEnumNames += QStringLiteral("CapPeak30E3");
    mEnumNames += QStringLiteral("CapPeak30E4");
    mEnumNames += QStringLiteral("CapPeak30E5");
    mEnumNames += QStringLiteral("CapPeak30E6");
    mEnumNames += QStringLiteral("CapPeak30S1");
    mEnumNames += QStringLiteral("CapPeak30S2");
    mEnumNames += QStringLiteral("CapPeak30S3");
    mEnumNames += QStringLiteral("CapPeak30S4");
    mEnumNames += QStringLiteral("CapPeak30S5");
    mEnumNames += QStringLiteral("CapPeak30S6");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofCaps::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[CapRiseE1] = getTile(tileName, 0);
    entry->mTiles[CapRiseE2] = getTile(tileName, 1);
    entry->mTiles[CapRiseE3] = getTile(tileName, 2);
    entry->mTiles[CapFallE1] = getTile(tileName, 8);
    entry->mTiles[CapFallE2] = getTile(tileName, 9);
    entry->mTiles[CapFallE3] = getTile(tileName, 10);
    entry->mTiles[CapRiseS1] = getTile(tileName, 13);
    entry->mTiles[CapRiseS2] = getTile(tileName, 12);
    entry->mTiles[CapRiseS3] = getTile(tileName, 11);
    entry->mTiles[CapFallS1] = getTile(tileName, 5);
    entry->mTiles[CapFallS2] = getTile(tileName, 4);
    entry->mTiles[CapFallS3] = getTile(tileName, 3);
    entry->mTiles[PeakPt5S] = getTile(tileName, 7);
    entry->mTiles[PeakPt5E] = getTile(tileName, 15);
    entry->mTiles[PeakOnePt5S] = getTile(tileName, 6);
    entry->mTiles[PeakOnePt5E] = getTile(tileName, 14);
    entry->mTiles[PeakTwoPt5S] = getTile(tileName, 17);
    entry->mTiles[PeakTwoPt5E] = getTile(tileName, 16);
#if 0 // impossible to guess these
    entry->mTiles[CapGapS1] = getTile(tileName, );
    entry->mTiles[CapGapS2] = getTile(tileName, );
    entry->mTiles[CapGapS3] = getTile(tileName, );
    entry->mTiles[CapGapE1] = getTile(tileName, );
    entry->mTiles[CapGapE2] = getTile(tileName, );
    entry->mTiles[CapGapE3] = getTile(tileName, );
#endif
    return entry;
}

int BTC_RoofCaps::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount + 4] = {
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallS3, CapFallS2, CapFallS1,
        CapFallE1, CapFallE2, CapFallE3, CapRiseS3, CapRiseS2, CapRiseS1,
        PeakPt5E, PeakOnePt5E, PeakTwoPt5E, PeakTwoPt5S, PeakOnePt5S, PeakPt5S,
        CapGapE1, CapGapE2, CapGapE3, CapGapS3, CapGapS2, CapGapS1,
        CapShallowRiseE1, CapShallowRiseE2, -1, -1, CapShallowFallS2, CapShallowFallS1,
        CapShallowFallE1, CapShallowFallE2, -1, -1, CapShallowRiseS2, CapShallowRiseS1,

        CapSlope30RiseE1, CapSlope30RiseE2, CapSlope30RiseE3, CapSlope30RiseE4, CapSlope30RiseE5, CapSlope30RiseE6,
        CapSlope30FallE6, CapSlope30FallE5, CapSlope30FallE4, CapSlope30FallE3, CapSlope30FallE2, CapSlope30FallE1,
        CapSlope30RiseS1, CapSlope30RiseS2, CapSlope30RiseS3, CapSlope30RiseS4, CapSlope30RiseS5, CapSlope30RiseS6,
        CapSlope30FallS6, CapSlope30FallS5, CapSlope30FallS4, CapSlope30FallS3, CapSlope30FallS2, CapSlope30FallS1,
        CapPeak30E1, CapPeak30E2, CapPeak30E3, CapPeak30E4, CapPeak30E5, CapPeak30E6,
        CapPeak30S1, CapPeak30S2, CapPeak30S3, CapPeak30S4, CapPeak30S5, CapPeak30S6,
    };
    return map[shadowIndex];
}

QVector<int> BTC_RoofCaps::enumsForVersion(int buildingTilesFileVersion) const
{
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION3) {
        // Version 3 added 30-degree roofs
        QVector<int> ret;
        for (int i = CapRiseE1; i <= CapShallowFallE2; i++) {
            ret += i;
        }
        return ret;
    }
    return BuildingTileCategory::enumsForVersion(buildingTilesFileVersion);
}

/////

BTC_RoofSlopes::BTC_RoofSlopes(const QString &label) :
    BuildingTileCategory(QStringLiteral("roof_slopes"), label, SlopeS2)
{
    mEnumNames += QStringLiteral("SlopeS1");
    mEnumNames += QStringLiteral("SlopeS2");
    mEnumNames += QStringLiteral("SlopeS3");
    mEnumNames += QStringLiteral("SlopeE1");
    mEnumNames += QStringLiteral("SlopeE2");
    mEnumNames += QStringLiteral("SlopeE3");

    mEnumNames += QStringLiteral("SlopePt5S");
    mEnumNames += QStringLiteral("SlopePt5E");
    mEnumNames += QStringLiteral("SlopeOnePt5S");
    mEnumNames += QStringLiteral("SlopeOnePt5E");
    mEnumNames += QStringLiteral("SlopeTwoPt5S");
    mEnumNames += QStringLiteral("SlopeTwoPt5E");

    mEnumNames += QStringLiteral("ShallowSlopeW1");
    mEnumNames += QStringLiteral("ShallowSlopeW2");
    mEnumNames += QStringLiteral("ShallowSlopeE1");
    mEnumNames += QStringLiteral("ShallowSlopeE2");
    mEnumNames += QStringLiteral("ShallowSlopeN1");
    mEnumNames += QStringLiteral("ShallowSlopeN2");
    mEnumNames += QStringLiteral("ShallowSlopeS1");
    mEnumNames += QStringLiteral("ShallowSlopeS2");

    mEnumNames << QStringLiteral("Slope30S1") << QStringLiteral("Slope30S2") << QStringLiteral("Slope30S3") << QStringLiteral("Slope30S4") << QStringLiteral("Slope30S5") << QStringLiteral("Slope30S6");
    mEnumNames << QStringLiteral("Slope30E1") << QStringLiteral("Slope30E2") << QStringLiteral("Slope30E3") << QStringLiteral("Slope30E4") << QStringLiteral("Slope30E5") << QStringLiteral("Slope30E6");
    mEnumNames << QStringLiteral("Slope30W1") << QStringLiteral("Slope30W2") << QStringLiteral("Slope30W3") << QStringLiteral("Slope30W4") << QStringLiteral("Slope30W5") << QStringLiteral("Slope30W6");
    mEnumNames << QStringLiteral("Slope30N1") << QStringLiteral("Slope30N2") << QStringLiteral("Slope30N3") << QStringLiteral("Slope30N4") << QStringLiteral("Slope30N5") << QStringLiteral("Slope30N6");

    mEnumNames << QStringLiteral("Peak30NS1") << QStringLiteral("Peak30NS2") << QStringLiteral("Peak30NS3") << QStringLiteral("Peak30NS4") << QStringLiteral("Peak30NS5") << QStringLiteral("Peak30NS6");
    mEnumNames << QStringLiteral("Peak30WE1") << QStringLiteral("Peak30WE2") << QStringLiteral("Peak30WE3") << QStringLiteral("Peak30WE4") << QStringLiteral("Peak30WE5") << QStringLiteral("Peak30WE6");
    mEnumNames << QStringLiteral("Peak30Quad1") << QStringLiteral("Peak30Quad2") << QStringLiteral("Peak30Quad3") << QStringLiteral("Peak30Quad4") << QStringLiteral("Peak30Quad5") << QStringLiteral("Peak30Quad6");

    mEnumNames += QStringLiteral("Inner1");
    mEnumNames += QStringLiteral("Inner2");
    mEnumNames += QStringLiteral("Inner3");
    mEnumNames += QStringLiteral("Outer1");
    mEnumNames += QStringLiteral("Outer2");
    mEnumNames += QStringLiteral("Outer3");
    mEnumNames += QStringLiteral("InnerPt5");
    mEnumNames += QStringLiteral("InnerOnePt5");
    mEnumNames += QStringLiteral("InnerTwoPt5");
    mEnumNames += QStringLiteral("OuterPt5");
    mEnumNames += QStringLiteral("OuterOnePt5");
    mEnumNames += QStringLiteral("OuterTwoPt5");
    mEnumNames += QStringLiteral("CornerSW1");
    mEnumNames += QStringLiteral("CornerSW2");
    mEnumNames += QStringLiteral("CornerSW3");
    mEnumNames += QStringLiteral("CornerNE1");
    mEnumNames += QStringLiteral("CornerNE2");
    mEnumNames += QStringLiteral("CornerNE3");

    mEnumNames << QStringLiteral("InnerSlope30SE1") << QStringLiteral("InnerSlope30SE2") << QStringLiteral("InnerSlope30SE3") << QStringLiteral("InnerSlope30SE4") << QStringLiteral("InnerSlope30SE5") << QStringLiteral("InnerSlope30SE6");
    mEnumNames << QStringLiteral("InnerSlope30NE1") << QStringLiteral("InnerSlope30NE2") << QStringLiteral("InnerSlope30NE3") << QStringLiteral("InnerSlope30NE4") << QStringLiteral("InnerSlope30NE5") << QStringLiteral("InnerSlope30NE6");
    mEnumNames << QStringLiteral("InnerSlope30NW1") << QStringLiteral("InnerSlope30NW2") << QStringLiteral("InnerSlope30NW3") << QStringLiteral("InnerSlope30NW4") << QStringLiteral("InnerSlope30NW5") << QStringLiteral("InnerSlope30NW6");
    mEnumNames << QStringLiteral("InnerSlope30SW1") << QStringLiteral("InnerSlope30SW2") << QStringLiteral("InnerSlope30SW3") << QStringLiteral("InnerSlope30SW4") << QStringLiteral("InnerSlope30SW5") << QStringLiteral("InnerSlope30SW6");

    mEnumNames << QStringLiteral("OuterSlope30SE1") << QStringLiteral("OuterSlope30SE2") << QStringLiteral("OuterSlope30SE3") << QStringLiteral("OuterSlope30SE4") << QStringLiteral("OuterSlope30SE5") << QStringLiteral("OuterSlope30SE6");
    mEnumNames << QStringLiteral("OuterSlope30NE1") << QStringLiteral("OuterSlope30NE2") << QStringLiteral("OuterSlope30NE3") << QStringLiteral("OuterSlope30NE4") << QStringLiteral("OuterSlope30NE5") << QStringLiteral("OuterSlope30NE6");
    mEnumNames << QStringLiteral("OuterSlope30NW1") << QStringLiteral("OuterSlope30NW2") << QStringLiteral("OuterSlope30NW3") << QStringLiteral("OuterSlope30NW4") << QStringLiteral("OuterSlope30NW5") << QStringLiteral("OuterSlope30NW6");
    mEnumNames << QStringLiteral("OuterSlope30SW1") << QStringLiteral("OuterSlope30SW2") << QStringLiteral("OuterSlope30SW3") << QStringLiteral("OuterSlope30SW4") << QStringLiteral("OuterSlope30SW5") << QStringLiteral("OuterSlope30SW6");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofSlopes::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[SlopeS1] = getTile(tileName, 0);
    entry->mTiles[SlopeS2] = getTile(tileName, 1);
    entry->mTiles[SlopeS3] = getTile(tileName, 2);
    entry->mTiles[SlopeE1] = getTile(tileName, 5);
    entry->mTiles[SlopeE2] = getTile(tileName, 4);
    entry->mTiles[SlopeE3] = getTile(tileName, 3);
    entry->mTiles[SlopePt5S] = getTile(tileName, 15);
    entry->mTiles[SlopePt5E] = getTile(tileName, 14);
    entry->mTiles[SlopeOnePt5S] = getTile(tileName, 15);
    entry->mTiles[SlopeOnePt5E] = getTile(tileName, 14);
    entry->mTiles[SlopeTwoPt5S] = getTile(tileName, 15);
    entry->mTiles[SlopeTwoPt5E] = getTile(tileName, 14);
    entry->mTiles[Inner1] = getTile(tileName, 11);
    entry->mTiles[Inner2] = getTile(tileName, 12);
    entry->mTiles[Inner3] = getTile(tileName, 13);
    entry->mTiles[Outer1] = getTile(tileName, 8);
    entry->mTiles[Outer2] = getTile(tileName, 9);
    entry->mTiles[Outer3] = getTile(tileName, 10);
    entry->mTiles[CornerSW1] = getTile(tileName, 24);
    entry->mTiles[CornerSW2] = getTile(tileName, 25);
    entry->mTiles[CornerSW3] = getTile(tileName, 26);
    entry->mTiles[CornerNE1] = getTile(tileName, 29);
    entry->mTiles[CornerNE2] = getTile(tileName, 28);
    entry->mTiles[CornerNE3] = getTile(tileName, 27);

    entry->mOffsets[SlopePt5S] = QPoint(1, 1);
    entry->mOffsets[SlopePt5E] = QPoint(1, 1);
    entry->mOffsets[SlopeTwoPt5S] = QPoint(-1, -1);
    entry->mOffsets[SlopeTwoPt5E] = QPoint(-1, -1);
    return entry;
}

int BTC_RoofSlopes::shadowToEnum(int shadowIndex)
{
    const int map[EnumCount + 4] = {
        SlopeS1, SlopeS2, SlopeS3, SlopeE3, SlopeE2, SlopeE1,
        SlopePt5S, SlopeOnePt5S, SlopeTwoPt5S, SlopeTwoPt5E, SlopeOnePt5E, SlopePt5E,
        Outer1, Outer2, Outer3, Inner1, Inner2, Inner3,
        OuterPt5, OuterOnePt5, OuterTwoPt5, InnerPt5, InnerOnePt5, InnerTwoPt5,
        CornerSW1, CornerSW2, CornerSW3, CornerNE3, CornerNE2, CornerNE1,

        ShallowSlopeW1, ShallowSlopeW2, -1, -1, ShallowSlopeE2, ShallowSlopeE1,
        ShallowSlopeN1, ShallowSlopeN2, -1, -1, ShallowSlopeS2, ShallowSlopeS1,

        Slope30S1, Slope30S2, Slope30S3, Slope30S4, Slope30S5, Slope30S6,
        Slope30E6, Slope30E5, Slope30E4, Slope30E3, Slope30E2, Slope30E1,
        Slope30W1, Slope30W2, Slope30W3, Slope30W4, Slope30W5, Slope30W6,
        Slope30N6, Slope30N5, Slope30N4, Slope30N3, Slope30N2, Slope30N1,

        InnerSlope30NW1, InnerSlope30NW2, InnerSlope30NW3, InnerSlope30NW4, InnerSlope30NW5, InnerSlope30NW6,
        InnerSlope30SW6, InnerSlope30SW5, InnerSlope30SW4, InnerSlope30SW3, InnerSlope30SW2, InnerSlope30SW1,
        InnerSlope30SE1, InnerSlope30SE2, InnerSlope30SE3, InnerSlope30SE4, InnerSlope30SE5, InnerSlope30SE6,
        InnerSlope30NE6, InnerSlope30NE5, InnerSlope30NE4, InnerSlope30NE3, InnerSlope30NE2, InnerSlope30NE1,

        OuterSlope30SE1, OuterSlope30SE2, OuterSlope30SE3, OuterSlope30SE4, OuterSlope30SE5, OuterSlope30SE6,
        OuterSlope30NE6, OuterSlope30NE5, OuterSlope30NE4, OuterSlope30NE3, OuterSlope30NE2, OuterSlope30NE1,
        OuterSlope30SW1, OuterSlope30SW2, OuterSlope30SW3, OuterSlope30SW4, OuterSlope30SW5, OuterSlope30SW6,
        OuterSlope30NW6, OuterSlope30NW5, OuterSlope30NW4, OuterSlope30NW3, OuterSlope30NW2, OuterSlope30NW1,

        Peak30NS1, Peak30NS2, Peak30NS3, Peak30NS4, Peak30NS5, Peak30NS6, // intersection runs west-east
        Peak30WE6, Peak30WE5, Peak30WE4, Peak30WE3, Peak30WE2, Peak30WE1, // intersection runs north-south
        Peak30Quad1, Peak30Quad2, Peak30Quad3, Peak30Quad4, Peak30Quad5, Peak30Quad6,
    };
    return map[shadowIndex];
}

QVector<int> BTC_RoofSlopes::enumsForVersion(int buildingTilesFileVersion) const
{
    if (buildingTilesFileVersion < BuildingTilesFile::VERSION3) {
        // Version 3 added 30-degree roofs
        QVector<int> ret;
        for (int i = SlopeS1; i <= ShallowSlopeS2; i++) {
            ret += i;
        }
        for (int i = Inner1; i <= CornerNE3; i++) {
            ret += i;
        }
        return ret;
    }
    return BuildingTileCategory::enumsForVersion(buildingTilesFileVersion);
}

/////

BTC_RoofTops::BTC_RoofTops(const QString &label) :
    BuildingTileCategory(QLatin1String("roof_tops"), label, West2)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_RoofTops::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = getTile(tileName);
    entry->mTiles[West2] = getTile(tileName);
    entry->mTiles[West3] = getTile(tileName);
    entry->mTiles[North1] = getTile(tileName);
    entry->mTiles[North2] = getTile(tileName);
    entry->mTiles[North3] = getTile(tileName);
    entry->mOffsets[West1] = QPoint(-1, -1);
    entry->mOffsets[West2] = QPoint(-2, -2);
    entry->mOffsets[North1] = QPoint(-1, -1);
    entry->mOffsets[North2] = QPoint(-2, -2);
    return entry;
}

int BTC_RoofTops::shadowToEnum(int shadowIndex)
{
    const int map[3] = {
        West3, West1, West2
    };
    return map[shadowIndex];
}

/////

BTC_GrimeFloor::BTC_GrimeFloor(const QString &label) :
    BuildingTileCategory(QLatin1String("grime_floor"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("East");
    mEnumNames += QLatin1String("South");
    mEnumNames += QLatin1String("SouthWest");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("NorthEast");
    mEnumNames += QLatin1String("SouthEast");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_GrimeFloor::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName, 0);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[East] = getTile(tileName, 9);
    entry->mTiles[South] = getTile(tileName, 8);
    entry->mTiles[SouthWest] = getTile(tileName, 11);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[NorthEast] = getTile(tileName, 3);
    entry->mTiles[SouthEast] = getTile(tileName, 10);
    return entry;
}

int BTC_GrimeFloor::shadowToEnum(int shadowIndex)
{
    const int map[8] = {
        West, North, East, South,
        SouthWest, NorthWest, NorthEast, SouthEast
    };
    return map[shadowIndex];
}

/////

BTC_GrimeWall::BTC_GrimeWall(const QString &label) :
    BuildingTileCategory(QLatin1String("grime_wall"), label, West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("SouthEast");
    mEnumNames += QLatin1String("WestWindow");
    mEnumNames += QLatin1String("NorthWindow");
    mEnumNames += QLatin1String("WestDoor");
    mEnumNames += QLatin1String("NorthDoor");
    mEnumNames += QLatin1String("WestTrim");
    mEnumNames += QLatin1String("NorthTrim");
    mEnumNames += QLatin1String("NorthWestTrim");
    mEnumNames += QLatin1String("SouthEastTrim");
    mEnumNames += QLatin1String("WestDoubleLeft");
    mEnumNames += QLatin1String("WestDoubleRight");
    mEnumNames += QLatin1String("NorthDoubleLeft");
    mEnumNames += QLatin1String("NorthDoubleRight");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

BuildingTileEntry *BTC_GrimeWall::createEntryFromSingleTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = getTile(tileName, 0);
    entry->mTiles[North] = getTile(tileName, 1);
    entry->mTiles[NorthWest] = getTile(tileName, 2);
    entry->mTiles[SouthEast] = getTile(tileName, 3);
    entry->mTiles[WestWindow] = getTile(tileName, 8);
    entry->mTiles[NorthWindow] = getTile(tileName, 9);
    entry->mTiles[WestDoor] = getTile(tileName, 10);
    entry->mTiles[NorthDoor] = getTile(tileName, 11);
    return entry;
}

int BTC_GrimeWall::shadowToEnum(int shadowIndex)
{
    const int map[16] = {
        West, North, NorthWest, SouthEast,
        WestWindow, NorthWindow, WestDoor, NorthDoor,
        WestTrim, NorthTrim, NorthWestTrim, SouthEastTrim,
        WestDoubleLeft, WestDoubleRight, NorthDoubleLeft, NorthDoubleRight
    };
    return map[shadowIndex];
}

/////

BuildingTileCategory::BuildingTileCategory(const QString &name,
                                           const QString &label,
                                           int displayIndex) :
    mName(name),
    mLabel(label),
    mDisplayIndex(displayIndex),
    mDefaultEntry(0),
    mNoneTileEntry(0)
{
}

BuildingTileEntry *BuildingTileCategory::entry(int index) const
{
    if (index < 0 || index >= mEntries.size())
        return BuildingTilesMgr::instance()->noneTileEntry();
    return mEntries[index];
}

void BuildingTileCategory::insertEntry(int index, BuildingTileEntry *entry)
{
    Q_ASSERT(entry && !entry->isNone());
    Q_ASSERT(!mEntries.contains(entry));
    Q_ASSERT(entry->category() == this);
    mEntries.insert(index, entry);
}

BuildingTileEntry *BuildingTileCategory::removeEntry(int index)
{
    return mEntries.takeAt(index);
}

BuildingTileEntry *BuildingTileCategory::noneTileEntry()
{
    if (!mNoneTileEntry && canAssignNone())
        mNoneTileEntry = new NoneBuildingTileEntry(this);
    return mNoneTileEntry;
}

QString BuildingTileCategory::enumToString(int index) const
{
    if (index < 0 || index >= mEnumNames.size())
        return QLatin1String("Invalid");
    return mEnumNames[index];
}

int BuildingTileCategory::enumFromString(const QString &s) const
{
    if (mEnumNames.contains(s))
        return mEnumNames.indexOf(s);
    return Invalid;
}

int BuildingTileCategory::shadowColumns() const
{
    return shadowImage().width() / 64;
}

int BuildingTileCategory::shadowRows() const
{
    return shadowImage().height() / 128;
}

int BuildingTileCategory::enumToShadow(int e)
{
    QVector<int> map(enumCount());
    for (int i = 0; i < enumCount(); i++)
        map[i] = -1;
    for (int i = 0; i < shadowCount(); i++) {
        int e = shadowToEnum(i);
        if (e != -1)
            map[e] = i;
    }
    return map[e];
}

BuildingTileEntry *BuildingTileCategory::findMatch(BuildingTileEntry *entry) const
{
    foreach (BuildingTileEntry *candidate, mEntries) {
        if (candidate->equals(entry)) {
            return candidate;
        }
    }
    return nullptr;
}

BuildingTileEntry *BuildingTileCategory::findMatchForVersion(BuildingTileEntry *entry, int buildingTilesFileVersion) const
{
    const QVector<int> enums = enumsForVersion(buildingTilesFileVersion);
    for (BuildingTileEntry *candidate : std::as_const(mEntries)) {
        if (candidate->equals(entry, enums)) {
            return candidate;
        }
    }
    return nullptr;
}

QVector<int> BuildingTileCategory::enumsForVersion(int buildingTilesFileVersion) const
{
    Q_UNUSED(buildingTilesFileVersion)
    QVector<int> ret;
    for (int i = 0; i < enumCount(); i++) {
        ret += i;
    }
    return ret;
}

BuildingTileEntry *BuildingTileCategory::findMatchIgnoreCategory(BuildingTileEntry *entry, int buildingTilesFileVersion) const
{
    const QVector<int> enums = enumsForVersion(buildingTilesFileVersion);
    for (BuildingTileEntry *candidate : std::as_const(mEntries)) {
        if (candidate->equalsIgnoreCategory(entry, enums)) {
            return candidate;
        }
    }
    return nullptr;
}

bool BuildingTileCategory::usesTile(Tile *tile) const
{
    BuildingTile *btile = BuildingTilesMgr::instance()->fromTiledTile(tile);
    foreach (BuildingTileEntry *entry, mEntries) {
        if (entry->usesTile(btile))
            return true;
    }
    return false;
}

BuildingTile *BuildingTileCategory::getTile(const QString &tilesetName, int offset)
{
    return BuildingTilesMgr::instance()->get(tilesetName, offset);
}

BuildingTileEntry *BuildingTileCategory::createEntryFromSingleTile(const QString &tileName)
{
    Q_UNUSED(tileName)
    return 0;
}

/////
