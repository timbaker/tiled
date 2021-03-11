/*
 * tilerotation.cpp
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

#include "tilerotation.h"

#include "tilerotationfile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QPainter>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

// // // // //

namespace Tiled
{

const char *TileRotatedVisual::EDGE_NAMES[] =
{
    "None",
    "North",
    "East",
    "South",
    "West",
    nullptr
};

class TileRotationPrivate
{
public:
    TileRotationPrivate(TileRotation& outer)
        : mOuter(outer)
    {

    }

    ZTileRenderOrder edgeToOrder(TileRotatedVisualEdge edge)
    {
        switch (edge)
        {
        case TileRotatedVisualEdge::North:
            return ZTileRenderOrder::NorthWall;
        case TileRotatedVisualEdge::East:
            return ZTileRenderOrder::EastWall;
        case TileRotatedVisualEdge::South:
            return ZTileRenderOrder::SouthWall;
        case TileRotatedVisualEdge::West:
            return ZTileRenderOrder::WestWall;
        default:
            return ZTileRenderOrder::West;
        }
    }

    void fileLoaded(TileRotated *tile)
    {
        // One or more tiles share the same TileRotatedVisual, don't do this more than once per TileRotatedVisual.
        TileRotatedVisual* visual = tile->mVisual.data();
        for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
            for (int j = 0; j < visual->mData[i].mTileNames.size(); j++) {
                const QString& tileName = visual->mData[i].mTileNames[j];
                ZTileRenderInfo renderInfo;
                renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
                renderInfo.mOffset = visual->mData[i].pixelOffset(j);
                renderInfo.mOrder = edgeToOrder(visual->mData[i].mEdges[j]);
                visual->mData[i].mRenderInfo += renderInfo;
            }
        }
    }

    void fileLoaded()
    {
        QSet<TileRotatedVisual*> doneVisuals;
        for (TilesetRotated *tileset : mTilesets) {
            for (TileRotated *tile : tileset->mTiles) {
                if (doneVisuals.contains(tile->mVisual.data())) {
                    continue;
                }
                doneVisuals += tile->mVisual.data();
                fileLoaded(tile);
            }
        }
    }

    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x());
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1);
        }
    }

    QPoint unrotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated:
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(pos.y(), height - pos.x() - 1); // ok
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1);
        case MapRotation::Clockwise270:
            return QPoint(width - pos.y() - 1, pos.x());
        }
    }

    bool isNone(Tile *tile)
    {
        return tile == nullptr || tile == BuildingTilesMgr::instance()->noneTiledTile();
    }

    MapRotation rotation[4] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    void tempLazyInit()
    {
//      initFromBuildingTiles();
        TileRotationFile file;
        if (file.read(QLatin1Literal("D:\\pz\\TileRotation.txt"))) {
            mTilesets = file.takeTilesets();
            mVisuals = file.takeVisuals();
            mTilesetByNameRotated.clear();
            for (TilesetRotated *tilesetR : mTilesets) {
                mTilesetByNameRotated[tilesetR->nameRotated()] = tilesetR;
            }
//            mLookupR0.clear();
//            mRenderInfo.clear();
//            mRenderInfo90.clear();
//            mRenderInfo180.clear();
//            mRenderInfo270.clear();
            fileLoaded();
        }
    }

    bool isRotatedTile(Tile *tile)
    {
        if (tile == nullptr) {
            return false;
        }
        return mTilesetByNameRotated.contains(tile->tileset()->name());
    }

    void rotateTile(Tile *tile, MapRotation viewRotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
    {
        // FIXME: temporary lazy init
        if (mTilesets.isEmpty()) {
            tempLazyInit();
        }
        // Is this a runtime fake tileset?
        if (isRotatedTile(tile)) {
            QString tilesetNameR = tile->tileset()->name();
            TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
            if (TileRotated *tileR = tilesetR->mTileByID[tile->id()]) {
                int m = (int(viewRotation) + int(tilesetR->mRotation) + int(tileR->mRotation)) % MAP_ROTATION_COUNT;
                tileInfos += tileR->mVisual->mData[m].mRenderInfo;
            }
            return;
        }

        // This is a real tile in an unrotated view.
        if (viewRotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
            return;
        }

        // This is a real tile in a rotated view.
        int m = int(viewRotation);
        QString tilesetNameR = tile->tileset()->name() + QLatin1Literal("_R") + QString::number(m * 90);
        if (mTilesetByNameRotated.contains(tilesetNameR)) {
            TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
            if (tile->id() >= tilesetR->mTileByID.size()) {
                return;
            }
            if (TileRotated *tileR = tilesetR->mTileByID[tile->id()]) {
                int m2 = (m + int(tileR->mRotation)) % MAP_ROTATION_COUNT;
                tileInfos += tileR->mVisual->mData[m2].mRenderInfo;
                return;
            }
            return;
        }

        if (viewRotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
        }
    }


    QSharedPointer<TileRotatedVisual> allocVisual()
    {
        QSharedPointer<TileRotatedVisual> visual(new TileRotatedVisual());
        visual->mUuid = QUuid::createUuid();
//        mVisualLookup[visual->mUuid] = visual;
        return visual;
    }

    Tile *tileFor(const QString &tilesetName, int tileID)
    {
        Tileset* tileset = getRotatedTileset(tilesetName);
        if (tileset == nullptr)
            return nullptr;
        return tileset->tileAt(tileID);
    }

    void reload()
    {
        qDeleteAll(mTilesets);
        mTilesets.clear();
        mTilesetByNameRotated.clear();
        mTilesetRotated.clear();
//        mLookupR0.clear();
//        mLookupR90.clear();
//        mLookupR180.clear();
//        mLookupR270.clear();
//        mRenderInfo.clear();
    }

    Tileset* getRotatedTileset(const QString tilesetName)
    {
        if (!tilesetName.contains(QLatin1Literal("_R")))
            return nullptr;
        if (mTilesetByNameRotated.contains(tilesetName) == false) {
            return nullptr;
        }
        Tileset *tileset = mTilesetRotated[tilesetName];
        if (tileset == nullptr) {
            tileset = new Tileset(tilesetName, 64, 128);
            tileset->loadFromNothing(QSize(64 * 8, 128 * 16), tilesetName + QLatin1Literal(".png"));
            mTilesetRotated[tilesetName] = tileset;
        }
        return tileset;
    }

    Tileset* getRotatedTileset(TilesetRotated* tilesetR)
    {
        return getRotatedTileset(tilesetR->nameRotated());
    }

    TileRotation& mOuter;
    QList<TilesetRotated*> mTilesets;
    QMap<QString, TilesetRotated*> mTilesetByNameRotated;
    QList<QSharedPointer<TileRotatedVisual>> mVisuals;

//    QMap<Tile*, Tile*> mLookupR0;
//    QMap<Tile*, Tile*> mLookupR90;
//    QMap<Tile*, Tile*> mLookupR180;
//    QMap<Tile*, Tile*> mLookupR270;
//    QMap<TileRotatedVisual*, QVector<ZTileRenderInfo>> mRenderInfo;
    QMap<QString, Tileset*> mTilesetRotated;
};

// // // // //

TilesetRotated::~TilesetRotated()
{
    qDeleteAll(mTiles);
    mTiles.clear();
}

QString TileRotated::name() const
{
    return BuildingTilesMgr::instance()->nameForTile(mTileset->mNameRotated, mID);
}

// namespace Tiled
}

// // // // //

TileRotation *TileRotation::mInstance = nullptr;

TileRotation *TileRotation::instance()
{
    if (mInstance == nullptr) {
        mInstance = new TileRotation();
    }
    return mInstance;
}

void TileRotation::deleteInstance()
{
    delete mInstance;
    mInstance = nullptr;
}

TileRotation::TileRotation()
    : mPrivate(new TileRotationPrivate(*this))
{

}

void TileRotation::readFile(const QString &filePath)
{
    TileRotationFile file;
    if (!file.read(filePath)) {
        return;
    }
    qDeleteAll(mPrivate->mTilesets);
    mPrivate->mTilesets = file.takeTilesets();
    mPrivate->mTilesetByNameRotated.clear();
    for (TilesetRotated *tilesetR : mPrivate->mTilesets) {
        mPrivate->mTilesetByNameRotated[tilesetR->nameRotated()] = tilesetR;
    }
    mPrivate->fileLoaded();
}

QSharedPointer<TileRotatedVisual> TileRotation::allocVisual()
{
    return mPrivate->allocVisual();
}

Tile *TileRotation::tileFor(const QString &tilesetName, int tileID)
{
    return mPrivate->tileFor(tilesetName, tileID);
}

void TileRotation::rotateTile(Tile *tile, MapRotation viewRotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
{
    mPrivate->rotateTile(tile, viewRotation, tileInfos);
}

#if 0
Tile *TileRotation::rotateTile(Tile *tile, MapRotation rotation)
{
    return mPrivate->rotateTile(tile, rotation);
}
#endif

void TileRotation::reload()
{
    mPrivate->reload();
}

TileRotated *TileRotation::rotatedTileFor(Tile *tileR)
{
    if (!mPrivate->mTilesetByNameRotated.contains(tileR->tileset()->name())) {
        return nullptr;
    }
    auto* tilesetR = mPrivate->mTilesetByNameRotated[tileR->tileset()->name()];
    if (tileR->id() >= tilesetR->mTileByID.size()) {
        return nullptr;
    }
    return tilesetR->mTileByID[tileR->id()];
}

Tileset *TileRotation::rotatedTilesetFor(TilesetRotated *tilesetR)
{
    return mPrivate->getRotatedTileset(tilesetR);
}

static bool isRotatedTilesetName(const QString& tilesetName)
{
    return tilesetName.endsWith(QLatin1Literal("_R0")) ||
            tilesetName.endsWith(QLatin1Literal("_R90")) ||
            tilesetName.endsWith(QLatin1Literal("_R180")) ||
            tilesetName.endsWith(QLatin1Literal("_R270"));
}

QString TileRotation::unrotateTile(const QString &tileName, MapRotation viewRotation)
{
    QString tilesetName;
    int tileIndex;
    if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileIndex)) {
        return tileName;
    }
    int tilesetRInt = int(viewRotation);
    if (isRotatedTilesetName(tilesetName)) {
        // Placing a runtime tile into the building.
        // We can't do this yet, because only real tilesets are available to choose tiles from.
    } else {
        // Placing a real tile into the building.
        // The given tileset is not R0 R90 R180 or R270, it's a real tileset.
        QString tilesetNameR = tilesetName + QLatin1Literal("_R0"); // any _RN tileset will do, we just want the original tile's rotation.
        if (mPrivate->mTilesetByNameRotated.contains(tilesetNameR)) {
            TilesetRotated* tilesetR = mPrivate->mTilesetByNameRotated[tilesetNameR];
            if (tileIndex >= tilesetR->mTileByID.size()) {
                return tileName;
            }
            if (TileRotated *tileR = tilesetR->mTileByID[tileIndex]) {
                // if viewRotation=0 then use tileset=0
                // if viewRotation=90 then use tileset=0-90=270
                // if viewRotation=180 then use tileset=0-180=180
                // if viewRotation=270 then use tileset=0-270=90
#if 1
                switch (viewRotation) {
                case MapRotation::NotRotated:
                    // 4 - 4
                    break;
                case MapRotation::Clockwise90:
                    // 4 - 1
                    tilesetRInt = 3;
                    break;
                case MapRotation::Clockwise180:
                    // 4 - 2
                    tilesetRInt = 2;
                    break;
                case MapRotation::Clockwise270:
                    // 4 - 3
                    tilesetRInt = 1;
                    break;
                }
#else
                // This always gives R270
                viewRotationInt = MAP_ROTATION_COUNT - 1 - ((viewRotationInt + int(tileR->mRotation)) % MAP_ROTATION_COUNT);
#endif
            }
        }
    }
    return BuildingTilesMgr::instance()->nameForTile(tilesetName + QLatin1Literal("_R") +  QString::number(tilesetRInt * 90), tileIndex);
}

#if 0
Tile *TileRotation::getRotatedTileDX(const QString &tilesetName, int index)
{
    return mPrivate->getRotatedTileDX(tilesetName, index);
}

Tile *TileRotation::getRotatedTileDY(const QString &tilesetName, int index)
{
    return mPrivate->getRotatedTileDY(tilesetName, index);
}
Tile *TileRotation::getRotatedTile(const QString &tileName, QPoint &offset)
{
    offset = QPoint();
    QString tilesetName;
    int index;
    if (BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, index) == false) {
        return nullptr;
    }
    int floorWidth = 64;
    int floorHeight = 32;
    if (tilesetName.endsWith(QLatin1Literal("_DX"))) {
        offset = QPoint(floorWidth / 2, floorHeight / 2);
        return mPrivate->getRotatedTileDX(tilesetName.left(tilesetName.length() - 3), index);
    }
    if (tilesetName.endsWith(QLatin1Literal("_DY"))) {
        offset = QPoint(-floorWidth / 2, floorHeight / 2);
        return mPrivate->getRotatedTileDY(tilesetName.left(tilesetName.length() - 3), index);
    }
    return BuildingTilesMgr::instance()->tileFor(tilesetName, index);
}
#endif
