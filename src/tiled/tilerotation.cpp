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

    void fileLoadedRotated(TileRotated *tile, Tile* tile1)
    {
//        Tileset *tilesetR180 = getRotatedTileset(tile->mTileset->mName + QLatin1Literal("_R180"));
//        Tile *tileR180 = tilesetR180->tileAt(tile->mID);
//        mRenderInfo0[tileR180] = mRenderInfo180[tile1];
//        mRenderInfo90[tileR180] = mRenderInfo270[tile1];
//        mRenderInfo180[tileR180] = mRenderInfo0[tile1];
//        mRenderInfo270[tileR180] = mRenderInfo90[tile1];

//        Tileset *tilesetR270 = getRotatedTileset(tile->mTileset->mName + QLatin1Literal("_R270"));
//        Tile *tileR270 = tilesetR270->tileAt(tile->mID);
//        mRenderInfo0[tileR270] = mRenderInfo270[tile1];
//        mRenderInfo90[tileR270] = mRenderInfo0[tile1];
//        mRenderInfo180[tileR270] = mRenderInfo90[tile1];
//        mRenderInfo270[tileR270] = mRenderInfo180[tile1];
    }

    void fileLoaded(TileRotated *tile)
    {
//        Tileset *tilesetR = getRotatedTileset(tile->mTileset);
//        Tile *tileR = tilesetR->tileAt(tile->mID);
        TileRotatedVisual* visual = tile->mVisual.data();
        for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
            for (int j = 0; j < visual->mData[i].mTileNames.size(); j++) {
                const QString& tileName = visual->mData[i].mTileNames[j];
                ZTileRenderInfo renderInfo;
                renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
                renderInfo.mOffset = visual->mData[i].pixelOffset(j);
                renderInfo.mOrder = edgeToOrder(visual->mData[i].mEdges[j]);
                visual->mData[i].mRenderInfo += renderInfo;
#if 0
mRenderInfo[tileR] += renderInfo;

                Tileset *tileset = TileMetaInfoMgr::instance()->tileset(tile->mTileset->nameUnrotated());
                Tile *tile1 = tileset->tileAt(tile->mID);
                switch (tile->mTileset->mRotation) {
                case MapRotation::NotRotated:
                    mLookupR0[tile1] = tileR;
                    break;
                case MapRotation::Clockwise90:
                    mLookupR90[tile1] = tileR;
                    break;
                case MapRotation::Clockwise180:
                    mLookupR180[tile1] = tileR;
                    break;
                case MapRotation::Clockwise270:
                    mLookupR270[tile1] = tileR;
                    break;
                }
#endif
            }
        }

//        Tile *tile1 = BuildingTilesMgr::instance()->tileFor(tile->mTileset->mName, tile->mID);
//        if (isNone(tile1) || (tile1 == Tiled::Internal::TilesetManager::instance()->missingTile()))
//            return;
//        Tileset *tilesetR0 = getRotatedTileset(tile->mTileset->mName + QLatin1Literal("_R0"));
//        Tile *tileR0 = tilesetR0->tileAt(tile->mID);
//        mLookupR0[tile1] = tileR0;
//        tile1 = tileR0;
//        for (int i = 0; i < tile->mVisual.mTileNames.size(); i++) {
//            const QString& tileName = tile->mVisual.mTileNames[i];
//            ZTileRenderInfo renderInfo;
//            renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
//            renderInfo.mOffset = tile->mVisual.pixelOffset(i);
//            renderInfo.mOrder = edgeToOrder(tile->mVisual.mEdges[i]);
//            mRenderInfo0[tile1] += renderInfo;
//        }
//        for (int i = 0; i < tile->r90.mTileNames.size(); i++) {
//            const QString& tileName = tile->r90.mTileNames[i];
//            ZTileRenderInfo renderInfo;
//            renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
//            renderInfo.mOffset = tile->r90.pixelOffset(i);
//            renderInfo.mOrder = edgeToOrder(tile->r90.mEdges[i]);
//            mRenderInfo90[tile1] += renderInfo;
//        }
//        for (int i = 0; i < tile->r180.mTileNames.size(); i++) {
//            const QString& tileName = tile->r180.mTileNames[i];
//            ZTileRenderInfo renderInfo;
//            renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
//            renderInfo.mOffset = tile->r180.pixelOffset(i);
//            renderInfo.mOrder = edgeToOrder(tile->r180.mEdges[i]);
//            mRenderInfo180[tile1] += renderInfo;
//        }
//        for (int i = 0; i < tile->r270.mTileNames.size(); i++) {
//            const QString& tileName = tile->r270.mTileNames[i];
//            ZTileRenderInfo renderInfo;
//            renderInfo.mTile = BuildingTilesMgr::instance()->tileFor(tileName);
//            renderInfo.mOffset = tile->r270.pixelOffset(i);
//            renderInfo.mOrder = edgeToOrder(tile->r270.mEdges[i]);
//            mRenderInfo270[tile1] += renderInfo;
//        }
//        fileLoadedRotated(tile, tile1);
    }

    void fileLoaded()
    {
        for (TilesetRotated *tileset : mTilesets) {
            for (TileRotated *tile : tileset->mTiles) {
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

    void rotateTile(Tile *tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
    {
        // FIXME: temporary lazy init
        if (mTilesets.isEmpty()) {
            tempLazyInit();
        }
        // Is this a runtime fake tileset?
        if (isRotatedTile(tile)) {
#if 1
            QString tilesetNameR = tile->tileset()->name();
            TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
            if (TileRotated *tileR = tilesetR->mTileByID[tile->id()]) {
                int m = (int(rotation) + int(tileR->mTileset->mRotation)) % MAP_ROTATION_COUNT;
                tileInfos += tileR->mVisual->mData[m].mRenderInfo;
//                if (TileRotated *tile1 = cycleTile(tileR, rotation)) {
//                    if (mRenderInfo.contains(tile1)) {
//                        tileInfos += mRenderInfo[tile1];
//                        return;
//                    }
//                }
            }
            return;
#else
            int degrees = 0;
            switch (rotation) {
            case MapRotation::NotRotated:
                break;
            case MapRotation::Clockwise90:
                degrees += 90;
                break;
            case MapRotation::Clockwise180:
                degrees += 180;
                break;
            case MapRotation::Clockwise270:
                degrees += 270;
                break;
            }
//            degrees %= 360;
            TilesetRotated *tilesetR = mTilesetLookup[tile->tileset()->name()];
            switch (tilesetR->mRotation) {
            case MapRotation::NotRotated:
                break;
            case MapRotation::Clockwise90:
                degrees += 90;
                break;
            case MapRotation::Clockwise180:
                degrees += 180;
                break;
            case MapRotation::Clockwise270:
                degrees += 270;
                break;
            }
            switch (degrees) {
            case 90:
                if (mRenderInfo90.contains(tile))
                    tileInfos += mRenderInfo90[tile];
                return;
            case 180:
                if (mRenderInfo180.contains(tile))
                    tileInfos += mRenderInfo180[tile];
                return;
            case 270:
                if (mRenderInfo270.contains(tile))
                    tileInfos += mRenderInfo270[tile];
                return;
            default:
                if (mRenderInfo0.contains(tile))
                    tileInfos += mRenderInfo0[tile];
                return;
            }
#endif
        }

        int m = int(rotation);
        QString tilesetNameR = tile->tileset()->name() + QLatin1Literal("_R") + QString::number(m * 90);
        if (mTilesetByNameRotated.contains(tilesetNameR)) {
            TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
            if (tile->id() >= tilesetR->mTileByID.size()) {
                return;
            }
            if (TileRotated *tileR = tilesetR->mTileByID[tile->id()]) {
                int m2 = (m + int(tileR->mMapRotation)) % MAP_ROTATION_COUNT;
                tileInfos += tileR->mVisual->mData[m2].mRenderInfo;
                return;
            }
            return;
        }

        if (rotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
        }
    }
#if 0
    Tile *rotateTile(Tile *tile, MapRotation rotation)
    {
        // FIXME: temporary lazy init
        if (mTilesets.isEmpty()) {
            tempLazyInit();
        }
        // Is this a runtime fake tileset?
        if (isRotatedTile(tile)) {
#if 1
            QString tilesetNameR = tile->tileset()->name();
            TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
            if (TileRotated *tileR = tilesetR->mTileByID[tile->id()]) {
                if (Tile *tile1 = cycleTile(tileR, rotation)) {
                    if (mRenderInfo.contains(tile1)) {
                        tileInfos += mRenderInfo[tile1];
                        return;
                    }
                }
            }
            return;
#else
            int degrees = 0;
            switch (rotation) {
            case MapRotation::NotRotated:
                break;
            case MapRotation::Clockwise90:
                degrees += 90;
                break;
            case MapRotation::Clockwise180:
                degrees += 180;
                break;
            case MapRotation::Clockwise270:
                degrees += 270;
                break;
            }
//            degrees %= 360;
            TilesetRotated *tilesetR = mTilesetLookup[tile->tileset()->name()];
            switch (tilesetR->mRotation) {
            case MapRotation::NotRotated:
                break;
            case MapRotation::Clockwise90:
                degrees += 90;
                break;
            case MapRotation::Clockwise180:
                degrees += 180;
                break;
            case MapRotation::Clockwise270:
                degrees += 270;
                break;
            }
            switch (degrees) {
            case 90:
                if (mRenderInfo90.contains(tile))
                    tileInfos += mRenderInfo90[tile];
                return;
            case 180:
                if (mRenderInfo180.contains(tile))
                    tileInfos += mRenderInfo180[tile];
                return;
            case 270:
                if (mRenderInfo270.contains(tile))
                    tileInfos += mRenderInfo270[tile];
                return;
            default:
                if (mRenderInfo0.contains(tile))
                    tileInfos += mRenderInfo0[tile];
                return;
            }
#endif
        }

        QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
        if (!mMapping.contains(tileName)) {
            tileInfos += ZTileRenderInfo(tile);
            return;
        }

        QString tileNameR = mMapping[tileName]; // real tile -> RN tile
        QString tilesetNameR;
        int index;
        BuildingTilesMgr::instance()->parseTileName(tileNameR, tilesetNameR, index);
        TilesetRotated* tilesetR = mTilesetByNameRotated[tilesetNameR];
        TileRotated *tileR = tilesetR->mTileByID[index];
        if (Tile *tile1 = cycleTile(tileR, rotation)) {
            if (mRenderInfo.contains(tile1)) {
                tileInfos += mRenderInfo[tile1];
                return;
            }
        }

        if (rotation == MapRotation::NotRotated) {
            tileInfos += ZTileRenderInfo(tile);
        }
    }

    TileRotated *cycleTile(TileRotated *tileR, MapRotation mapRotation)
    {
        int m = (int(mapRotation) + int(tileR->mTileset->mRotation)) % MAP_ROTATION_COUNT;
        QString tilesetNameR = tileR->mTileset->mNameUnrotated + QLatin1Literal("_R") + QString::number(m * 90);
        if (!mTilesetByNameRotated.contains(tilesetNameR)) {
            return nullptr;
        }
        if (TilesetRotated *tilesetR = mTilesetByNameRotated[tilesetNameR]) {
            return tilesetR->mTileByID[tileR->mID];
        }
        return nullptr;
#if 0
        switch (mapRotation) {
        case MapRotation::NotRotated:
            return getRotatedTileset(tileR->mTileset)->tileAt(tileR->mID);
        case MapRotation::Clockwise90: {
        }
        case MapRotation::Clockwise180: {
            int m = (int(mapRotation) + int(tileR->mTileset->mRotation)) % MAP_ROTATION_COUNT;
            return getRotatedTileset(tileR->mTileset->mNameUnrotated + QLatin1Literal("_R") + QString::number(m * 90))->tileAt(tileR->mID);
        }
        case MapRotation::Clockwise270: {
            int m = (int(mapRotation) + int(tileR->mTileset->mRotation)) % MAP_ROTATION_COUNT;
            return getRotatedTileset(tileR->mTileset->mNameUnrotated + QLatin1Literal("_R") + QString::number(m * 90))->tileAt(tileR->mID);
        }
        }
#endif
    }
#endif
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

void TileRotation::rotateTile(Tile *tile, MapRotation rotation, QVector<Tiled::ZTileRenderInfo>& tileInfos)
{
    mPrivate->rotateTile(tile, rotation, tileInfos);
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

QString TileRotation::unrotateTile(const QString &tileName, MapRotation mapRotation)
{
    QString tilesetName;
    int tileIndex;
    if (!BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, tileIndex)) {
        return tileName;
    }
    int m = int(mapRotation);
    QString tilesetNameR = tilesetName + QLatin1Literal("_R") + QString::number(m * 90);
    if (mPrivate->mTilesetByNameRotated.contains(tilesetNameR)) {
        TilesetRotated* tilesetR = mPrivate->mTilesetByNameRotated[tilesetNameR];
        if (tileIndex >= tilesetR->mTileByID.size()) {
            return tileName;
        }
        if (TileRotated *tileR = tilesetR->mTileByID[tileIndex]) {
            m = (m + int(tileR->mMapRotation)) % MAP_ROTATION_COUNT;
        }
    }
    return BuildingTilesMgr::instance()->nameForTile(tilesetName + QLatin1Literal("_R") +  QString::number(m * 90), tileIndex);
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
