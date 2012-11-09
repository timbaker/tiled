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

/////

BuildingTiles *BuildingTiles::mInstance = 0;

BuildingTiles *BuildingTiles::instance()
{
    if (!mInstance)
        mInstance = new BuildingTiles;
    return mInstance;
}

void BuildingTiles::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTiles::BuildingTiles() :
    mFurnitureCategory(new BuildingTileCategory(QLatin1String("Furniture"),
                                    QLatin1String("furniture"))),
    mMissingTile(0)
{
    Tileset *tileset = new Tileset(QLatin1String("missing"), 64, 128);
    tileset->setTransparentColor(Qt::white);
    QString fileName = QLatin1String(":/BuildingEditor/icons/missing-tile.png");
    if (tileset->loadFromImage(QImage(fileName), fileName))
        mMissingTile = tileset->tileAt(0);
}

BuildingTiles::~BuildingTiles()
{
    TilesetManager::instance()->removeReferences(tilesets());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
    qDeleteAll(mCategories);
    delete mFurnitureCategory;
    if (mMissingTile)
        delete mMissingTile->tileset();
}

BuildingTileCategory *BuildingTiles::addCategory(const QString &categoryName, const QString &label)
{
    BuildingTileCategory *category = this->category(categoryName);
    if (!category) {
        category = new BuildingTileCategory(categoryName, label);
        mCategories += category;
        mCategoryByName[categoryName]= category;
    }
    return category;
}

BuildingTile *BuildingTiles::add(const QString &categoryName, const QString &tileName)
{
    BuildingTileCategory *category = this->category(categoryName);
#if 0
    if (!category) {
        category = new Category(categoryName);
        mCategories += category;
        mCategoryByName[categoryName]= category;
    }
#endif
    return category->add(tileName);
}

void BuildingTiles::add(const QString &categoryName, const QStringList &tileNames)
{
    QVector<BuildingTile*> tiles;
    foreach (QString tileName, tileNames)
        tiles += add(categoryName, tileName);
    foreach (BuildingTile *tile, tiles)
        tile->mAlternates = tiles;
}

BuildingTile *BuildingTiles::get(const QString &categoryName, const QString &tileName)
{
    BuildingTileCategory *category = this->category(categoryName);
    return category->get(tileName);
}

QString BuildingTiles::nameForTile(const QString &tilesetName, int index)
{
    // The only reason I'm padding the tile index is so that the tiles are sorted
    // by increasing tileset name and index.
    return tilesetName + QLatin1Char('_') +
            QString(QLatin1String("%1")).arg(index, 3, 10, QLatin1Char('0'));
}

QString BuildingTiles::nameForTile(Tile *tile)
{
    return nameForTile(tile->tileset()->name(), tile->id());
}

bool BuildingTiles::parseTileName(const QString &tileName, QString &tilesetName, int &index)
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

QString BuildingTiles::adjustTileNameIndex(const QString &tileName, int offset)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    index += offset;
    return nameForTile(tilesetName, index);
}

QString BuildingTiles::normalizeTileName(const QString &tileName)
{
    QString tilesetName;
    int index;
    parseTileName(tileName, tilesetName, index);
    return nameForTile(tilesetName, index);
}

void BuildingTiles::addTileset(Tileset *tileset)
{
    Q_ASSERT(mTilesetByName.contains(tileset->name()) == false);
    mTilesetByName[tileset->name()] = tileset;
    if (!mRemovedTilesets.contains(tileset))
        TilesetManager::instance()->addReference(tileset);
    mRemovedTilesets.removeAll(tileset);
    emit tilesetAdded(tileset);
}

void BuildingTiles::removeTileset(Tileset *tileset)
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

bool BuildingTiles::readBuildingTilesTxt()
{
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String("BuildingTiles.txt"));
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The BuildingTiles.txt file doesn't exist.");
        return false;
    }

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.").arg(path);
        return false;
    }

    static const char *validCategoryNamesC[] = {
        "exterior_walls", "interior_walls", "floors", "doors", "door_frames",
        "windows", "stairs", "roofs", "roof_caps", 0
    };
    QStringList validCategoryNames;
    for (int i = 0; validCategoryNamesC[i]; i++)
        validCategoryNames << QLatin1String(validCategoryNamesC[i]);

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("category")) {
            QString categoryName = block.value("name");
            if (!validCategoryNames.contains(categoryName)) {
                mError = tr("Unknown category '%1' in BuildingTiles.txt.").arg(categoryName);
                return false;
            }

            QString label = block.value("label");
            addCategory(categoryName, label);
            SimpleFileBlock tilesBlock = block.block("tiles");
            foreach (SimpleFileKeyValue kv, tilesBlock.values) {
                if (kv.name == QLatin1String("tile")) {
                    QStringList values = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                    for (int i = 0; i < values.count(); i++)
                        values[i] = BuildingTiles::normalizeTileName(values[i]);
                    add(categoryName, values);
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
            foreach (SimpleFileBlock rangeBlock, block.blocks) {
                if (rangeBlock.name == QLatin1String("range")) {
                    QString tilesetName = rangeBlock.value("tileset");
                    int start = rangeBlock.value("start").toInt();
                    int offset = rangeBlock.value("offset").toInt();
                    QString countStr = rangeBlock.value("count");
                    int count = tilesetFor(tilesetName)->tileCount(); // FIXME: tileset might not exist
                    if (countStr != QLatin1String("all"))
                        count = countStr.toInt();
                    for (int i = start; i < start + count; i += offset)
                        add(categoryName, BuildingTiles::nameForTile(tilesetName, i));
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
        foreach (BuildingTile *tile, category->tiles()) {
            if (!tileFor(tile)) {
                mError = tr("Tile %1 #%2 doesn't exist.")
                        .arg(tile->mTilesetName)
                        .arg(tile->mIndex);
                return false;
            }
        }
    }

    return true;
}

void BuildingTiles::writeBuildingTilesTxt(QWidget *parent)
{
    SimpleFile simpleFile;
    foreach (BuildingTileCategory *category, categories()) {
        SimpleFileBlock categoryBlock;
        categoryBlock.name = QLatin1String("category");
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("label"),
                                                   category->label());
        categoryBlock.values += SimpleFileKeyValue(QLatin1String("name"),
                                                   category->name());
        SimpleFileBlock tileBlock;
        tileBlock.name = QLatin1String("tiles");
        foreach (BuildingTile *btile, category->tiles())
            tileBlock.values += SimpleFileKeyValue(QLatin1String("tile"),
                                                   btile->name());
        categoryBlock.blocks += tileBlock;
        simpleFile.blocks += categoryBlock;
    }
    QString fileName = BuildingPreferences::instance()
            ->configPath(QLatin1String("BuildingTiles.txt"));
    if (!simpleFile.write(fileName)) {
        QMessageBox::warning(parent, tr("It's no good, Jim!"),
                             simpleFile.errorString());
    }
}

Tiled::Tile *BuildingTiles::tileFor(const QString &tileName)
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

Tile *BuildingTiles::tileFor(BuildingTile *tile, int offset)
{
    if (!mTilesetByName.contains(tile->mTilesetName))
        return mMissingTile;
    if (tile->mIndex + offset >= mTilesetByName[tile->mTilesetName]->tileCount())
        return mMissingTile;
    return mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + offset);
}

BuildingTile *BuildingTiles::fromTiledTile(const QString &categoryName, Tile *tile)
{
    if (BuildingTileCategory *category = this->category(categoryName))
        return category->get(nameForTile(tile));
    return 0;
}

BuildingTile *BuildingTiles::defaultExteriorWall() const
{
    return category(QLatin1String("exterior_walls"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultInteriorWall() const
{
    return category(QLatin1String("interior_walls"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultFloorTile() const
{
    return category(QLatin1String("floors"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultDoorTile() const
{
    return category(QLatin1String("doors"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultDoorFrameTile() const
{
    return category(QLatin1String("door_frames"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultWindowTile() const
{
    return category(QLatin1String("windows"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultStairsTile() const
{
    return category(QLatin1String("stairs"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultRoofTile() const
{
    return category(QLatin1String("roofs"))->tileAt(0);
}

BuildingTile *BuildingTiles::defaultRoofCapTile() const
{
    return category(QLatin1String("roof_caps"))->tileAt(0);
}

BuildingTile *BuildingTiles::getExteriorWall(const QString &tileName)
{
    return category(QLatin1String("exterior_walls"))->get(tileName);
}

BuildingTile *BuildingTiles::getInteriorWall(const QString &tileName)
{
    return category(QLatin1String("interior_walls"))->get(tileName);
}

BuildingTile *BuildingTiles::getFloorTile(const QString &tileName)
{
    return category(QLatin1String("floors"))->get(tileName);
}

BuildingTile *BuildingTiles::getDoorTile(const QString &tileName)
{
    return category(QLatin1String("doors"))->get(tileName);
}

BuildingTile *BuildingTiles::getDoorFrameTile(const QString &tileName)
{
    return category(QLatin1String("door_frames"))->get(tileName);
}

BuildingTile *BuildingTiles::getWindowTile(const QString &tileName)
{
    return category(QLatin1String("windows"))->get(tileName);
}

BuildingTile *BuildingTiles::getStairsTile(const QString &tileName)
{
    return category(QLatin1String("stairs"))->get(tileName);
}

BuildingTile *BuildingTiles::getRoofTile(const QString &tileName)
{
    return category(QLatin1String("roofs"))->get(tileName);
}

BuildingTile *BuildingTiles::getRoofCapTile(const QString &tileName)
{
    return category(QLatin1String("roof_caps"))->get(tileName);
}

BuildingTile *BuildingTiles::getFurnitureTile(const QString &tileName)
{
    return mFurnitureCategory->get(tileName);
}

/////

BuildingTile *BuildingTileCategory::add(const QString &tileName)
{
    QString tilesetName;
    int tileIndex;
    BuildingTiles::parseTileName(tileName, tilesetName, tileIndex);
    BuildingTile *tile = new BuildingTile(tilesetName, tileIndex);
    Q_ASSERT(!mTileByName.contains(tile->name()));
    mTileByName[tileName] = tile;
    mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
    return tile;
}

void BuildingTileCategory::remove(const QString &tileName)
{
    if (!mTileByName.contains(tileName))
        return;
    mTileByName.remove(tileName);
    mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
}

BuildingTile *BuildingTileCategory::get(const QString &tileName)
{
    if (!mTileByName.contains(tileName))
        add(tileName);
    return mTileByName[tileName];
}

bool BuildingTileCategory::usesTile(Tile *tile) const
{
    return mTileByName.contains(BuildingTiles::nameForTile(tile));
}

QRect BuildingTileCategory::categoryBounds() const
{
    if (mName == QLatin1String("exterior_walls"))
        return QRect(0, 0, 4, 2);
    if (mName == QLatin1String("interior_walls"))
        return QRect(0, 0, 4, 2);
    if (mName == QLatin1String("floors"))
        return QRect(0, 0, 1, 1);
    if (mName == QLatin1String("doors"))
        return QRect(0, 0, 4, 1);
    if (mName == QLatin1String("door_frames"))
        return QRect(0, 0, 2, 1);
    if (mName == QLatin1String("windows"))
        return QRect(0, 0, 2, 1);
    if (mName == QLatin1String("stairs"))
        return QRect(0, 0, 3, 2);
    if (mName == QLatin1String("roofs"))
        return QRect(0, 0, 8, 2);
    if (mName == QLatin1String("roof_caps"))
        return QRect(0, 0, 8, 2);
    qFatal("unhandled category name in categoryBounds");
    return QRect();
}

/////

QString BuildingTile::name() const
{
    return BuildingTiles::nameForTile(mTilesetName, mIndex);
}

/////
