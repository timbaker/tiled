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

#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "simplefile.h"

#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

#include <QCoreApplication>
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
    mNoneTileEntry(0)
{
    mCatCurtains = new BTC_Curtains(QLatin1String("Curtains"));
    mCatDoors = new BTC_Doors(QLatin1String("Doors"));
    mCatDoorFrames = new BTC_DoorFrames(QLatin1String("Door Frames"));
    mCatFloors = new BTC_Floors(QLatin1String("Floors"));
    mCatEWalls = new BTC_EWalls(QLatin1String("Exterior Walls"));
    mCatIWalls = new BTC_IWalls(QLatin1String("Interior Walls"));
    mCatStairs = new BTC_Stairs(QLatin1String("Stairs"));
    mCatWindows = new BTC_Windows(QLatin1String("Windows"));
    mCatRoofCaps = new BTC_RoofCaps(QLatin1String("Roof Caps"));
    mCatRoofSlopes = new BTC_RoofSlopes(QLatin1String("Roof Slopes"));
    mCatRoofTops = new BTC_RoofTops(QLatin1String("Roof Tops"));

    Tileset *tileset = new Tileset(QLatin1String("missing"), 64, 128);
    tileset->setTransparentColor(Qt::white);
    QString fileName = QLatin1String(":/BuildingEditor/icons/missing-tile.png");
    if (tileset->loadFromImage(QImage(fileName), fileName))
        mMissingTile = tileset->tileAt(0);
    else {
        QImage image(64, 128, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, fileName);
        mMissingTile = tileset->tileAt(0);
    }

    tileset = new Tileset(QLatin1String("none"), 64, 128);
    tileset->setTransparentColor(Qt::white);
    fileName = QLatin1String(":/BuildingEditor/icons/none-tile.png");
    if (tileset->loadFromImage(QImage(fileName), fileName))
        mNoneTiledTile = tileset->tileAt(0);
    else {
        QImage image(64, 128, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, fileName);
        mNoneTiledTile = tileset->tileAt(0);
    }

    mNoneBuildingTile = new NoneBuildingTile();

    mNoneTileEntry = new BuildingTileEntry(0);
}

BuildingTilesMgr::~BuildingTilesMgr()
{
    TilesetManager::instance()->removeReferences(tilesets());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
    qDeleteAll(mCategories);
    if (mMissingTile)
        delete mMissingTile->tileset();
    if (mNoneTiledTile)
        delete mNoneTiledTile->tileset();
    delete mNoneBuildingTile;
}

BuildingTile *BuildingTilesMgr::add(const QString &tileName)
{
    QString tilesetName;
    int tileIndex;
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

QString BuildingTilesMgr::nameForTile(const QString &tilesetName, int index)
{
    // The only reason I'm padding the tile index is so that the tiles are sorted
    // by increasing tileset name and index.
    return tilesetName + QLatin1Char('_') +
            QString(QLatin1String("%1")).arg(index, 3, 10, QLatin1Char('0'));
}

QString BuildingTilesMgr::nameForTile(Tile *tile)
{
    return nameForTile(tile->tileset()->name(), tile->id());
}

bool BuildingTilesMgr::parseTileName(const QString &tileName, QString &tilesetName, int &index)
{
    tilesetName = tileName.mid(0, tileName.lastIndexOf(QLatin1Char('_')));
    QString indexString = tileName.mid(tileName.lastIndexOf(QLatin1Char('_')) + 1);
    // Strip leading zeroes from the tile index
#if 1
    int i = 0;
    while (i < indexString.length() - 1 && indexString[i] == QLatin1Char('0'))
        i++;
    indexString.remove(0, i);
#else
    indexString.remove( QRegExp(QLatin1String("^[0]*")) );
#endif
    index = indexString.toInt();
    return true;
}

QString BuildingTilesMgr::adjustTileNameIndex(const QString &tileName, int offset)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
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

void BuildingTilesMgr::addTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()) == false);
    mTilesetByName[tileset->name()] = tileset;
    if (!mRemovedTilesets.contains(tileset))
        TilesetManager::instance()->addReference(tileset);
    mRemovedTilesets.removeAll(tileset);
    emit tilesetAdded(tileset);
}

void BuildingTilesMgr::removeTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()));
    Q_ASSERT(mRemovedTilesets.contains(tileset) == false);
    emit tilesetAboutToBeRemoved(tileset);
    mTilesetByName.remove(tileset->name());
    emit tilesetRemoved(tileset);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += tileset;
//    TilesetManager::instance()->removeReference(tileset);
}

QString BuildingTilesMgr::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTilesMgr::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

static void writeTileEntry(SimpleFileBlock &block, BuildingTileEntry *entry)
{

}

static BuildingTileEntry *readTileEntry(SimpleFileBlock &block)
{
    return 0;
}

// VERSION0: original format without 'version' keyvalue
#define VERSION0 0

// VERSION2
// added 'version' keyvalue
// added 'curtains' category
#define VERSION1 1
#define VERSION_LATEST VERSION1

bool BuildingTilesMgr::readBuildingTilesTxt()
{
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String(TXT_FILE));
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

    static const char *validCategoryNamesC[] = {
        "exterior_walls", "interior_walls", "floors", "doors", "door_frames",
        "windows", "curtains", "stairs", "roof_caps", "roof_slopes", "roof_tops",
        0
    };
    QStringList validCategoryNames;
    for (int i = 0; validCategoryNamesC[i]; i++) {
        QString categoryName = QLatin1String(validCategoryNamesC[i]);
        validCategoryNames << categoryName;
    }

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            if (!validCategoryNames.contains(categoryName)) {
                mError = tr("Unknown category '%1' in BuildingTiles.txt.").arg(categoryName);
                return false;
            }
            BuildingTileCategory *category = this->category(categoryName);
            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("entry")) {
                    if (BuildingTileEntry *entry = readTileEntry(block2)) {
                        // read offset = a b c here too
                        category->insertEntry(category->entryCount(), entry);
                    } else
                        return false;
                } else {
                    mError = tr("Unknown block name '%1'.\n%2")
                            .arg(block2.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

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

    return true;
}

void BuildingTilesMgr::writeBuildingTilesTxt(QWidget *parent)
{
    SimpleFile simpleFile;
    foreach (BuildingTileCategory *category, categories()) {
        SimpleFileBlock categoryBlock;
        categoryBlock.name = QLatin1String("category");
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("label"),
                                                   category->label());
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("name"),
                                                   category->name());
        foreach (BuildingTileEntry *entry, category->entries()) {
            writeTileEntry(categoryBlock, entry);
        }

        simpleFile.blocks += categoryBlock;
    }
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String(TXT_FILE));
    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

static SimpleFileBlock findCategoryBlock(const SimpleFileBlock &parent,
                                         const QString &categoryName)
{
    foreach (SimpleFileBlock block, parent.blocks) {
        if (block.name == QLatin1String("category")) {
            if (block.value("name") == categoryName)
                return block;
        }
    }
    return SimpleFileBlock();
}

bool BuildingTilesMgr::upgradeTxt()
{
    QString userPath = BuildingPreferences::instance()
            ->configPath(QLatin1String(TXT_FILE));

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
            + QLatin1String(TXT_FILE);

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    if (userVersion == VERSION0) {
        userFile.blocks += findCategoryBlock(sourceFile, QLatin1String("curtains"));
    }

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
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
    if (!mTilesetByName.contains(tilesetName))
        return mMissingTile;
    if (index >= mTilesetByName[tilesetName]->tileCount())
        return mMissingTile;
    return mTilesetByName[tilesetName]->tileAt(index);
}

Tile *BuildingTilesMgr::tileFor(BuildingTile *tile, int offset)
{
    if (tile->isNone())
        return mNoneTiledTile;
    if (!mTilesetByName.contains(tile->mTilesetName))
        return mMissingTile;
    if (tile->mIndex + offset >= mTilesetByName[tile->mTilesetName]->tileCount())
        return mMissingTile;
    return mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + offset);
}

BuildingTile *BuildingTilesMgr::fromTiledTile(Tile *tile)
{
    if (tile == mNoneTiledTile)
        return mNoneBuildingTile;
    return get(nameForTile(tile));
}

BuildingTileEntry *BuildingTilesMgr::defaultExteriorWall() const
{
    return mCatEWalls->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultInteriorWall() const
{
    return mCatIWalls->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultFloorTile() const
{
    return mCatFloors->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorTile() const
{
    return mCatDoors->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultDoorFrameTile() const
{
    return mCatDoorFrames->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultWindowTile() const
{
    return mCatWindows->entry(0);
}

BuildingTileEntry *BuildingTilesMgr::defaultCurtainsTile() const
{
    return mNoneTileEntry;
}

BuildingTileEntry *BuildingTilesMgr::defaultStairsTile() const
{
    return mCatStairs->entry(0);
}

/////

bool BuildingTileCategory::usesTile(Tile *tile) const
{
    BuildingTile *btile = BuildingTilesMgr::instance()->fromTiledTile(tile);
    foreach (BuildingTileEntry *entry, mEntries) {
        if (entry->usesTile(btile))
            return true;
    }
    return false;
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
}

BuildingTile *BuildingTileEntry::displayTile() const
{
    return tile(mCategory->displayIndex());
}

BuildingTile *BuildingTileEntry::tile(int n) const
{
    if (n < 0 || n >= mTiles.size())
        return 0;
    return mTiles[n];
}

QPoint BuildingTileEntry::offset(int n) const
{
    if (n < 0 || n >= mOffsets.size())
        return QPoint();
    return mOffsets[n];
}

bool BuildingTileEntry::usesTile(BuildingTile *btile) const
{
    return mTiles.contains(btile);
}

/////

BTC_Doors::BTC_Doors(const QString &label) :
    BuildingTileCategory(label, QLatin1String("doors"), West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("WestOpen");
    mEnumNames += QLatin1String("NorthOpen");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Doors::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North] = BuildingTilesMgr::instance()->get(tileName, 1);
    entry->mTiles[WestOpen] = BuildingTilesMgr::instance()->get(tileName, 2);
    entry->mTiles[NorthOpen] = BuildingTilesMgr::instance()->get(tileName, 3);
    mEntries += entry;
}

/////

BTC_Curtains::BTC_Curtains(const QString &label) :
    BuildingTileCategory(label, QLatin1String("door_frames"), West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("East");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("South");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Curtains::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[East] = BuildingTilesMgr::instance()->get(tileName, 1);
    entry->mTiles[North] = BuildingTilesMgr::instance()->get(tileName, 2);
    entry->mTiles[South] = BuildingTilesMgr::instance()->get(tileName, 3);
    mEntries += entry;
}

/////

BTC_DoorFrames::BTC_DoorFrames(const QString &label) :
    BuildingTileCategory(label, QLatin1String("door_frames"), West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_DoorFrames::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North] = BuildingTilesMgr::instance()->get(tileName, 1);
    mEntries += entry;
}

/////

BTC_Floors::BTC_Floors(const QString &label) :
    BuildingTileCategory(label, QLatin1String("floors"), Floor)
{
    mEnumNames += QLatin1String("Floor");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Floors::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[Floor] = BuildingTilesMgr::instance()->get(tileName);
    mEntries += entry;
}

/////

BTC_Stairs::BTC_Stairs(const QString &label) :
    BuildingTileCategory(label, QLatin1String("stairs"), West1)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Stairs::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[West2] = BuildingTilesMgr::instance()->get(tileName, 1);
    entry->mTiles[West3] = BuildingTilesMgr::instance()->get(tileName, 2);
    entry->mTiles[North1] = BuildingTilesMgr::instance()->get(tileName, 8);
    entry->mTiles[North2] = BuildingTilesMgr::instance()->get(tileName, 9);
    entry->mTiles[North3] = BuildingTilesMgr::instance()->get(tileName, 10);
    mEntries += entry;
}

/////

BTC_Walls::BTC_Walls(const QString &label) :
    BuildingTileCategory(label, QLatin1String("walls"), West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    mEnumNames += QLatin1String("NorthWest");
    mEnumNames += QLatin1String("SouthEast");
    mEnumNames += QLatin1String("WestWindow");
    mEnumNames += QLatin1String("NorthWindow");
    mEnumNames += QLatin1String("WestDoor");
    mEnumNames += QLatin1String("NorthDoor");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Walls::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North] = BuildingTilesMgr::instance()->get(tileName, 1);
    entry->mTiles[NorthWest] = BuildingTilesMgr::instance()->get(tileName, 2);
    entry->mTiles[SouthEast] = BuildingTilesMgr::instance()->get(tileName, 3);
    entry->mTiles[WestWindow] = BuildingTilesMgr::instance()->get(tileName, 8);
    entry->mTiles[NorthWindow] = BuildingTilesMgr::instance()->get(tileName, 9);
    entry->mTiles[WestDoor] = BuildingTilesMgr::instance()->get(tileName, 10);
    entry->mTiles[NorthWindow] = BuildingTilesMgr::instance()->get(tileName, 11);
    mEntries += entry;
}

/////

BTC_Windows::BTC_Windows(const QString &label) :
    BuildingTileCategory(label, QLatin1String("windows"), West)
{
    mEnumNames += QLatin1String("West");
    mEnumNames += QLatin1String("North");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_Windows::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North] = BuildingTilesMgr::instance()->get(tileName, 1);
    mEntries += entry;
}

/////

BTC_RoofCaps::BTC_RoofCaps(const QString &label) :
    BuildingTileCategory(label, QLatin1String("roof_caps"), CapRiseE3)
{
    mEnumNames += QLatin1String("CapRiseE1");
    mEnumNames += QLatin1String("CapRiseE2");
    mEnumNames += QLatin1String("CapRiseE3");
    mEnumNames += QLatin1String("CapFallE1");
    mEnumNames += QLatin1String("CapFallE2");
    mEnumNames += QLatin1String("CapFallE3");

    mEnumNames += QLatin1String("CapRiseS1");
    mEnumNames += QLatin1String("CapRiseS2");
    mEnumNames += QLatin1String("CapRiseS3");
    mEnumNames += QLatin1String("CapFallS1");
    mEnumNames += QLatin1String("CapFallS2");
    mEnumNames += QLatin1String("CapFallS3");

    mEnumNames += QLatin1String("PeakPt5S");
    mEnumNames += QLatin1String("PeakPt5E");
    mEnumNames += QLatin1String("PeakOnePt5S");
    mEnumNames += QLatin1String("PeakOnePt5E");
    mEnumNames += QLatin1String("PeakTwoPt5S");
    mEnumNames += QLatin1String("PeakTwoPt5E");

    mEnumNames += QLatin1String("CapGapS1");
    mEnumNames += QLatin1String("CapGapS2");
    mEnumNames += QLatin1String("CapGapS3");
    mEnumNames += QLatin1String("CapGapE1");
    mEnumNames += QLatin1String("CapGapE2");
    mEnumNames += QLatin1String("CapGapE3");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_RoofCaps::addTile(const QString &tileName)
{
    // TODO
}

/////

BTC_RoofSlopes::BTC_RoofSlopes(const QString &label) :
    BuildingTileCategory(label, QLatin1String("roof_slopes"), SlopeS2)
{
    mEnumNames += QLatin1String("SlopeS1");
    mEnumNames += QLatin1String("SlopeS2");
    mEnumNames += QLatin1String("SlopeS3");
    mEnumNames += QLatin1String("SlopeE1");
    mEnumNames += QLatin1String("SlopeE2");
    mEnumNames += QLatin1String("SlopeE3");

    mEnumNames += QLatin1String("SlopePt5S");
    mEnumNames += QLatin1String("SlopePt5E");
    mEnumNames += QLatin1String("SlopeOnePt5S");
    mEnumNames += QLatin1String("SlopeOnePt5E");
    mEnumNames += QLatin1String("SlopeTwoPt5S");
    mEnumNames += QLatin1String("SlopeTwoPt5E");

    mEnumNames += QLatin1String("FlatTopW1");
    mEnumNames += QLatin1String("FlatTopW2");
    mEnumNames += QLatin1String("FlatTopW3");
    mEnumNames += QLatin1String("FlatTopN1");
    mEnumNames += QLatin1String("FlatTopN2");
    mEnumNames += QLatin1String("FlatTopN3");

    mEnumNames += QLatin1String("Inner1");
    mEnumNames += QLatin1String("Inner2");
    mEnumNames += QLatin1String("Inner3");
    mEnumNames += QLatin1String("Outer1");
    mEnumNames += QLatin1String("Outer2");
    mEnumNames += QLatin1String("Outer3");

    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_RoofSlopes::addTile(const QString &tileName)
{
    // TODO
}

/////

BTC_RoofTops::BTC_RoofTops(const QString &label) :
    BuildingTileCategory(label, QLatin1String("roof_tops"), West2)
{
    mEnumNames += QLatin1String("West1");
    mEnumNames += QLatin1String("West2");
    mEnumNames += QLatin1String("West3");
    mEnumNames += QLatin1String("North1");
    mEnumNames += QLatin1String("North2");
    mEnumNames += QLatin1String("North3");
    Q_ASSERT(mEnumNames.size() == EnumCount);
}

void BTC_RoofTops::addTile(const QString &tileName)
{
    BuildingTileEntry *entry = new BuildingTileEntry(this);
    entry->mTiles[West1] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[West2] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[West3] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North1] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North2] = BuildingTilesMgr::instance()->get(tileName);
    entry->mTiles[North3] = BuildingTilesMgr::instance()->get(tileName);
    mEntries += entry;
}

/////
