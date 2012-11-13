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

#include "rooftiles.h"

#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "simplefile.h"

#include <QCoreApplication>
#include <QFileInfo>

using namespace BuildingEditor;

static const char *TXT_FILE = "RoofTiles.txt";

RoofTiles *RoofTiles::mInstance = 0;

RoofTiles *RoofTiles::instance()
{
    if (!mInstance)
        mInstance = new RoofTiles;
    return mInstance;
}

void RoofTiles::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

RoofTiles::RoofTiles()
{
}

RoofTiles::~RoofTiles()
{
    qDeleteAll(mSlopeTiles);
    qDeleteAll(mCapTiles);
}

QString RoofTiles::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString RoofTiles::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool RoofTiles::readTxt()
{
    QString fileName = txtPath();
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

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("cap")) {
            RoofCapTiles *tiles = new RoofCapTiles();
            foreach (SimpleFileKeyValue kv, block.values) {
                RoofCapTiles::TileEnum e = RoofCapTiles::enumFromString(kv.name);
                if (e == RoofCapTiles::Invalid) {
                    mError = tr("Unknown cap tile '%1'").arg(kv.name);
                    delete tiles;
                    return false;
                }
                QString tileName = BuildingTilesMgr::normalizeTileName(kv.value);
                BuildingTile *btile = BuildingTilesMgr::instance()->getRoofTile(tileName);
                tiles->mTiles[e] = RoofTile(btile);
            }
            mCapTiles += tiles;
        } else if (block.name == QLatin1String("slope")) {
            RoofSlopeTiles *tiles = new RoofSlopeTiles();
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("offset")) {
                    QStringList split = kv.value.split(QLatin1Char(' '), QString::SkipEmptyParts);
                    if (split.size() != 3) {
                        mError = tr("Expected 'offset = slope x y', got '%1'").arg(kv.value);
                        delete tiles;
                        return false;
                    }
                    RoofSlopeTiles::TileEnum e = RoofSlopeTiles::enumFromString(split[0]);
                    if (e == RoofSlopeTiles::Invalid) {
                        mError = tr("Unknown slope tile '%1'").arg(split[0]);
                        delete tiles;
                        return false;
                    }
                    QPoint offset = QPoint(split[1].toInt(), split[2].toInt());
                    tiles->mTiles[e].setOffset(offset);
                    continue;
                }
                RoofSlopeTiles::TileEnum e = RoofSlopeTiles::enumFromString(kv.name);
                if (e == RoofSlopeTiles::Invalid) {
                    mError = tr("Unknown slope tile '%1'").arg(kv.name);
                    delete tiles;
                    return false;
                }
                QString tileName = BuildingTilesMgr::normalizeTileName(kv.value);
                BuildingTile *btile = BuildingTilesMgr::instance()->getRoofTile(tileName);
                tiles->mTiles[e] = RoofTile(btile);
            }
            mSlopeTiles += tiles;
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    // Check that all the tiles exist
    foreach (RoofCapTiles *rtiles, mCapTiles) {
        foreach (RoofTile rtile, rtiles->roofTiles()) {
            if (!BuildingTilesMgr::instance()->tileFor(rtile.tile())) {
                mError = tr("Tile %1 #%2 doesn't exist.")
                        .arg(rtile.tile()->mTilesetName)
                        .arg(rtile.tile()->mIndex);
                return false;
            }
        }
    }
    foreach (RoofSlopeTiles *rtiles, mSlopeTiles) {
        foreach (RoofTile rtile, rtiles->roofTiles()) {
            if (!BuildingTilesMgr::instance()->tileFor(rtile.tile())) {
                mError = tr("Tile %1 #%2 doesn't exist.")
                        .arg(rtile.tile()->mTilesetName)
                        .arg(rtile.tile()->mIndex);
                return false;
            }
        }
    }

    return true;
}

bool RoofTiles::writeTxt()
{
    SimpleFile simpleFile;
    foreach (RoofCapTiles *tiles, mCapTiles) {
        SimpleFileBlock tileBlock;
        tileBlock.name = QLatin1String("cap");
        for (int e = RoofCapTiles::First; e < RoofCapTiles::NumTiles; e++) {
            tileBlock.values += SimpleFileKeyValue(RoofCapTiles::mEnumNames[e],
                                                   tiles->roofTile(e).tile()->name());
        }
        simpleFile.blocks += tileBlock;
    }
    foreach (RoofSlopeTiles *tiles, mSlopeTiles) {
        SimpleFileBlock tileBlock;
        tileBlock.name = QLatin1String("slope");
        for (int e = RoofSlopeTiles::First; e < RoofSlopeTiles::NumTiles; e++) {
            tileBlock.values += SimpleFileKeyValue(RoofSlopeTiles::mEnumNames[e],
                                                   tiles->roofTile(e).tile()->name());
        }
        simpleFile.blocks += tileBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

RoofCapTiles *RoofTiles::findMatch(RoofCapTiles *rtiles) const
{
    foreach (RoofCapTiles *candidate, mCapTiles) {
        if (candidate->equals(rtiles))
            return candidate;
    }
    return 0;
}

RoofSlopeTiles *RoofTiles::findMatch(RoofSlopeTiles *rtiles) const
{
    foreach (RoofSlopeTiles *candidate, mSlopeTiles) {
        if (candidate->equals(rtiles))
            return candidate;
    }
    return 0;
}

bool RoofTiles::upgradeTxt()
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

RoofTile::RoofTile()
    : mTile(BuildingTilesMgr::instance()->noneTile())
{
}

bool RoofTile::operator ==(const RoofTile &other)
{
    return other.mOffset == mOffset &&
            other.mTile == mTile;
}

/////

QStringList RoofSlopeTiles::mEnumNames;

RoofSlopeTiles::RoofSlopeTiles() :
    mTiles(NumTiles)
{
}

bool RoofSlopeTiles::equals(RoofSlopeTiles *other) const
{
    return mTiles == other->mTiles;
}

QString RoofSlopeTiles::enumToString(int tile)
{
    initNames();
    return mEnumNames[tile];
}

QString RoofSlopeTiles::enumToString(RoofSlopeTiles::TileEnum tile)
{
    initNames();
    return mEnumNames[tile];
}

RoofSlopeTiles::TileEnum RoofSlopeTiles::enumFromString(const QString &s)
{
    initNames();
    if (mEnumNames.contains(s))
        return static_cast<TileEnum>(mEnumNames.indexOf(s));
    return Invalid;
}

void RoofSlopeTiles::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
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
}

/////

QStringList RoofCapTiles::mEnumNames;

RoofCapTiles::RoofCapTiles() :
    mTiles(NumTiles)
{
}

bool RoofCapTiles::equals(RoofCapTiles *other) const
{
    return other->mTiles == mTiles;
}

QString RoofCapTiles::enumToString(int tile)
{
    initNames();
    return mEnumNames[tile];
}

QString RoofCapTiles::enumToString(RoofCapTiles::TileEnum tile)
{
    initNames();
    return mEnumNames[tile];
}

RoofCapTiles::TileEnum RoofCapTiles::enumFromString(const QString &s)
{
    initNames();
    if (mEnumNames.contains(s))
        return static_cast<TileEnum>(mEnumNames.indexOf(s));
    return Invalid;
}

void RoofCapTiles::initNames()
{
    if (!mEnumNames.isEmpty())
        return;
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
}

