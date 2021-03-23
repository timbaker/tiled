/*
 * tilerotationwindow.cpp
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

#include "tilerotationwindow.h"
#include "ui_tilerotationwindow.h"

#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "tilerotationfile.h"

#include "tileset.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/furnituregroups.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QUndoGroup>
#include <QUndoStack>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

// // // // //

class CreateVisual : public QUndoCommand
{
public:
    CreateVisual(TileRotationWindow *d, int index, QSharedPointer<TileRotatedVisual> visual) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Create Visual")),
        mDialog(d),
        mVisual(visual),
        mIndex(index)
    {
    }

    void undo() override
    {
        mVisual = mDialog->removeVisual(mIndex);
    }

    void redo() override
    {
        mDialog->addVisual(mVisual, mIndex);
    }

    TileRotationWindow *mDialog;
    QSharedPointer<TileRotatedVisual> mVisual;
    int mIndex;
};

class DeleteVisual : public QUndoCommand
{
public:
    DeleteVisual(TileRotationWindow *d, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Create Visual")),
        mDialog(d),
        mIndex(index)
    {
    }

    void undo() override
    {
        mDialog->addVisual(mVisual, mIndex);
        mVisual = nullptr;
    }

    void redo() override
    {
        mVisual = mDialog->removeVisual(mIndex);
    }

    TileRotationWindow *mDialog;
    QSharedPointer<TileRotatedVisual> mVisual;
    int mIndex;
};

class AssignVisual : public QUndoCommand
{
public:
    AssignVisual(TileRotationWindow *d, TileRotated *tileR, QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Assign Visual")),
        mDialog(d),
        mTileRotated(tileR),
        mAV(visual, mapRotation)
    {
    }

    void undo() override
    {
        mAV = mDialog->assignVisual(mTileRotated, mAV.mVisual, mAV.mMapRotation);
    }

    void redo() override
    {
        mAV = mDialog->assignVisual(mTileRotated, mAV.mVisual, mAV.mMapRotation);
    }

    TileRotationWindow *mDialog;
    TileRotated *mTileRotated;
    TileRotationWindow::AssignedVisual mAV;
};

class ChangeTiles : public QUndoCommand
{
public:
    ChangeTiles(TileRotationWindow *d, QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation, const TileRotatedVisualData& data) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Visual Appearance")),
        mDialog(d),
        mVisual(visual),
        mMapRotation(mapRotation),
        mData(data)
    {
    }

    void undo() override
    {
        mData = mDialog->changeVisualData(mVisual, mMapRotation, mData);
    }

    void redo() override
    {
        mData = mDialog->changeVisualData(mVisual, mMapRotation, mData);
    }

    TileRotationWindow *mDialog;
    QSharedPointer<TileRotatedVisual> mVisual;
    MapRotation mMapRotation;
    TileRotatedVisualData mData;
};

#if 1

static MapRotation ROTATION[MAP_ROTATION_COUNT] = {
    MapRotation::NotRotated,
    MapRotation::Clockwise90,
    MapRotation::Clockwise180,
    MapRotation::Clockwise270
};

// This is temporary code to bootstrap creation of TileRotation.txt from the existing BuildingEd information.
class InitFromBuildingTiles
{
public:
    void initFromBuildingTiles(const QList<BuildingTileEntry*> &entries, int n, int w, TileRotateType rotateType)
    {
        for (BuildingTileEntry* bte : entries) {
            initFromBuildingTiles(bte, n, w, rotateType);
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, int nw, TileRotateType rotateType)
    {
        BuildingTile *btileN = bte->tile(n);
        BuildingTile *btileW = bte->tile(w);
        if (btileN->isNone() || btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTileN(btileN->name());
        visual->mData[1].addTileE(btileW->name());
        visual->mData[2].addTileS(btileN->name());
        visual->mData[3].addTileW(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileW->name()] = tileRotatedW->name();

        if (nw != -1) {
            BuildingTile *btileNW = bte->tile(nw);
            if (!btileNW->isNone()) {
                visual = allocVisual();
                visual->mData[0].addTileN(btileNW->name());

                visual->mData[1].addTileN(btileN->name());
                visual->mData[1].addTileE(btileW->name());

                visual->mData[2].addTileE(btileW->name());
                visual->mData[2].addTileS(btileN->name());

                visual->mData[3].addTileW(btileW->name());
                visual->mData[3].addTileS(btileN->name());

                initVisual(btileNW, visual, MapRotation::NotRotated);

//                mMapping[btileNW->name()] = tileRotatedNW->name();
            }
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, TileRotateType rotateType)
    {
        int nw = (rotateType == TileRotateType::Wall) ? BTC_Walls::NorthWest : -1;
        initFromBuildingTiles(bte, n, w, nw, rotateType);
    }

    void initGrime()
    {
#if 0
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        WestTrim,
        NorthTrim,
        NorthWestTrim,
        SouthEastTrim,
        WestDoubleLeft,
        WestDoubleRight,
        NorthDoubleLeft,
        NorthDoubleRight,
#endif
        BuildingTileCategory *slopes = BuildingTilesMgr::instance()->catGrimeWall();
        for (BuildingTileEntry *entry : slopes->entries()) {
            initFromBuildingTiles(entry, BTC_GrimeWall::North, BTC_GrimeWall::West, BTC_GrimeWall::NorthWest, TileRotateType::Wall);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthDoor, BTC_GrimeWall::WestDoor, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthWindow, BTC_GrimeWall::WestWindow, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthTrim, BTC_GrimeWall::WestTrim, BTC_GrimeWall::NorthWestTrim, TileRotateType::Wall);
            // DoubleLeft/DoubleRight should be single FurnitureTile
        }
    }

    void initRoofCaps(BuildingTileEntry *entry, int n, int e, int s, int w)
    {
        BuildingTile *btileN = entry->tile(n);
        BuildingTile *btileE = entry->tile(e);
        BuildingTile *btileS = entry->tile(s);
        BuildingTile *btileW = entry->tile(w);
        if (btileN->isNone() || btileE->isNone() || btileS->isNone() || btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTileN(btileN->name());
        visual->mData[1].addTileE(btileE->name());
        visual->mData[2].addTileS(btileS->name());
        visual->mData[3].addTileW(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);
    }

    void initRoofCaps()
    {
#if 0
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        // Cap tiles for shallow (garage, trailer, etc) roofs
        CapShallowRiseS1, CapShallowRiseS2, CapShallowFallS1, CapShallowFallS2,
        CapShallowRiseE1, CapShallowRiseE2, CapShallowFallE1, CapShallowFallE2,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofCaps();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapFallE1, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapRiseE1);
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapFallE2, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapRiseE2);
            initRoofCaps(entry, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapFallE3, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapRiseE3);

            initRoofCaps(entry, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapRiseE1, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapFallE1);
            initRoofCaps(entry, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapRiseE2, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapFallE2);
            initRoofCaps(entry, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapRiseE3, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapFallE3);

            initFromBuildingTiles(entry, BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS3, BTC_RoofCaps::CapGapE3, TileRotateType::WallExtra);

            initRoofCaps(entry, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowFallE1, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowRiseE1);
            initRoofCaps(entry, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowFallE2, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowRiseE2);

            initRoofCaps(entry, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowRiseE1, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowFallE1);
            initRoofCaps(entry, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowRiseE2, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowFallE2);
        }
    }

    void initRoofSlope(BuildingTileEntry *entry, int north, int west)
    {
        BuildingTile *btileN = entry->tile(north);
        BuildingTile *btileW = entry->tile(west);
        if (btileN->isNone() && btileW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileW->name()] = tileRotatedW->name();
    }

    // Corners
    void initRoofSlope(BuildingTileEntry *bte, int se, int sw, int ne)
    {
        BuildingTile *btileSE = bte->tile(se);
        BuildingTile *btileSW = bte->tile(sw);
        BuildingTile *btileNE = bte->tile(ne);
         if (btileSE->isNone() || btileNE->isNone() || btileSW->isNone())
            return;

        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[1].addTile(btileNE->name());
        visual->mData[2].addTile(btileSE->name());
        visual->mData[3].addTile(btileSW->name());

        // Missing NW = R0

        initVisual(btileNE, visual, MapRotation::Clockwise90);
        initVisual(btileSE, visual, MapRotation::Clockwise180);
        initVisual(btileSW, visual, MapRotation::Clockwise270);

//        mMapping[btileNE->name()] = tileRotatedNE->name();
//        mMapping[btileSE->name()] = tileRotatedSE->name();
//        mMapping[btileSW->name()] = tileRotatedSW->name();
    }

    void initRoofSlope(BuildingTile *btileN, BuildingTile *btileE, BuildingTile *btileS, BuildingTile *btileW)
    {
        QSharedPointer<TileRotatedVisual> visual = allocVisual();
        visual->mData[0].addTile(btileN->name());
        visual->mData[1].addTile(btileE->name());
        visual->mData[2].addTile(btileS->name());
        visual->mData[3].addTile(btileW->name());

        initVisual(btileN, visual, MapRotation::NotRotated);
        initVisual(btileE, visual, MapRotation::Clockwise90);
        initVisual(btileS, visual, MapRotation::Clockwise180);
        initVisual(btileW, visual, MapRotation::Clockwise270);

//        mMapping[btileN->name()] = tileRotatedN->name();
//        mMapping[btileE->name()] = tileRotatedE->name();
//        mMapping[btileS->name()] = tileRotatedS->name();
//        mMapping[btileW->name()] = tileRotatedW->name();
    }

    void initRoofSlopes()
    {
#if 0
        // Sloped sides
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Shallow sides
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,

        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofSlopes();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeE1);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeE2);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS3, BTC_RoofSlopes::SlopeE3);

            initRoofSlope(entry, BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E);

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN1);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE1);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS1);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW1);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    initRoofSlope(tileN, tileE, tileS, tileW);
                    initRoofSlope(tileE, tileS, tileW, tileN);
                    initRoofSlope(tileS, tileW, tileN, tileE);
                    initRoofSlope(tileW, tileN, tileE, tileS);
                }
            }

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN2);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE2);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS2);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW2);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    initRoofSlope(tileN, tileE, tileS, tileW);
                    initRoofSlope(tileE, tileS, tileW, tileN);
                    initRoofSlope(tileS, tileW, tileN, tileE);
                    initRoofSlope(tileW, tileN, tileE, tileS);
                }
            }

            initRoofSlope(entry, BTC_RoofSlopes::Outer1, BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerNE1);
            initRoofSlope(entry, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerNE2);
            initRoofSlope(entry, BTC_RoofSlopes::Outer3, BTC_RoofSlopes::CornerSW3, BTC_RoofSlopes::CornerNE3);
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

    bool isFurnitureOK(FurnitureTile* ft[4])
    {
        if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
            return false;
        return true;
    }

    bool initFurniture(FurnitureTile* ft[4])
    {
        int width = ft[0]->width();
        int height = ft[0]->height();
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                BuildingTile *tiles[MAP_ROTATION_COUNT];
                BuildingTile *masterTile = nullptr; // normally this is the North/NotRotated tile
                for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
                    QPoint p = rotatePoint(width, height, ROTATION[i], QPoint(dx, dy));
                    tiles[i] = ft[i]->tile(p.x(), p.y());
                    if (masterTile == nullptr && tiles[i] != nullptr) {
                        masterTile = tiles[i];
                    }
                }
                if (masterTile == nullptr)
                    continue;
                QSharedPointer<TileRotatedVisual> visual = allocVisual();
                for (int i = 0; i < MAP_ROTATION_COUNT; i++) {
                    BuildingTile *btile = tiles[i];
                    if (btile == nullptr)
                        continue;
                    visual->mData[i].addTile(btile->name());
                    initVisual(btile, visual, ROTATION[i]);
//                    mMapping[btile->name()] = tileR->name();
                }
            }
        }
        return false;
    }

    void initFurniture()
    {
        const QList<FurnitureGroup*> furnitureGroups = FurnitureGroups::instance()->groups();
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (isFurnitureOK(ft)) {
                    initFurniture(ft);
                }

                if (furnitureTiles->hasCorners()) {
                    ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureNW);
                    ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureNE);
                    ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureSE);
                    ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureSW);
                    if (isFurnitureOK(ft)) {
                        initFurniture(ft);
                    }
                }
            }
        }
    }

    TilesetRotated* getTilesetRotated(const QString &tilesetName)
    {
        TilesetRotated *tilesetR = mTilesetByName[tilesetName];
        if (tilesetR == nullptr) {
            tilesetR = new TilesetRotated();
            tilesetR->mName = tilesetName;
            if (Tileset *tileset1 = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
                tilesetR->mColumnCount = tileset1->columnCount();
            } else {
                tilesetR->mColumnCount = 8; // FIXME
            }
            mTilesetByName[tilesetName] = tilesetR;
            mTilesets += tilesetR;
        }
        return tilesetR;
    }

    TileRotated* getTileRotated(BuildingTile *buildingTile)
    {
        TilesetRotated *tileset = getTilesetRotated(buildingTile->mTilesetName);
        return getTileRotated(tileset, buildingTile->mIndex);
    }

    TileRotated* getTileRotated(TilesetRotated *tileset, int tileID)
    {
        if (TileRotated *tileR = tileset->tileAt(tileID)) {
            return tileR;
        }
        return tileset->createTile(tileID);
    }

    void initVisual(BuildingTile *buildingTile, QSharedPointer<TileRotatedVisual> &visual, MapRotation mapRotation)
    {
        Q_ASSERT(buildingTile->isNone() == false);
        Q_ASSERT(BuildingTilesMgr::instance()->tileFor(buildingTile) != BuildingTilesMgr::instance()->noneTiledTile());
        Q_ASSERT(BuildingTilesMgr::instance()->tileFor(buildingTile) != TilesetManager::instance()->missingTile());

        TileRotated* tileRotatedN = getTileRotated(buildingTile);
        tileRotatedN->mVisual = visual;
        tileRotatedN->mRotation = mapRotation;
    }

    void init(TileRotationWindow *window)
    {
        mWindow = window;

        BuildingTileCategory *category = BuildingTilesMgr::instance()->catFloors();
        for (BuildingTileEntry *entry : category->entries()) {
            BuildingTile *btile = entry->tile(BTC_Floors::Floor);
            if (btile->isNone())
                continue;
            QSharedPointer<TileRotatedVisual> visual = allocVisual();
            visual->mData[0].addTile(btile->name(), QPoint(), TileRotatedVisualEdge::Floor);
            visual->mData[1].addTile(btile->name(), QPoint(), TileRotatedVisualEdge::Floor);
            visual->mData[2].addTile(btile->name(), QPoint(), TileRotatedVisualEdge::Floor);
            visual->mData[3].addTile(btile->name(), QPoint(), TileRotatedVisualEdge::Floor);
            initVisual(btile, visual, MapRotation::NotRotated);
        }

        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoors()->entries(), BTC_Doors::North, BTC_Doors::West, TileRotateType::Door);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoorFrames()->entries(), BTC_DoorFrames::North, BTC_DoorFrames::West, TileRotateType::DoorFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catWindows()->entries(), BTC_Windows::North, BTC_Windows::West, TileRotateType::Window);
        // TODO: BTC_Curtains

        initGrime();
        initRoofCaps();
        initRoofSlopes();

        initFurniture();
    }

    QSharedPointer<TileRotatedVisual> allocVisual()
    {
        QSharedPointer<TileRotatedVisual> visual(new TileRotatedVisual());
        visual->mUuid = QUuid::createUuid();
//        mVisualLookup[visual->mUuid] = visual;
        mVisuals += visual;
        return visual;
    }

    TileRotationWindow *mWindow;
    QList<TilesetRotated*> mTilesets;
    QList<QSharedPointer<TileRotatedVisual>> mVisuals;
    QMap<QString, TilesetRotated*> mTilesetByName;
};

#else

// This is temporary code to bootstrap creation of TileRotation.txt from the existing BuildingEd furniture information.
class InitFromBuildingTiles
{
public:
    QPoint rotatePoint(int width, int height, MapRotation rotation, const QPoint &pos) const
    {
        switch (rotation)
        {
        case MapRotation::NotRotated: // w,h=3,2 x,y=2,1 -> 2,1
            return pos;
        case MapRotation::Clockwise90:
            return QPoint(height - pos.y() - 1, pos.x()); // w,h=3,2 x,y=2,1 -> 0,2
        case MapRotation::Clockwise180:
            return QPoint(width - pos.x() - 1, height - pos.y() - 1); // w,h=3,2 x,y=2,1 -> 0,0
        case MapRotation::Clockwise270:
            return QPoint(pos.y(), width - pos.x() - 1); // w,h=3,2 x,y=2,1 -> 1,0
        }
    }

    void initFromBuildingTiles(const QList<BuildingTileEntry*> &entries, int n, int w, TileRotateType rotateType)
    {
        for (BuildingTileEntry* bte : entries) {
            initFromBuildingTiles(bte, n, w, rotateType);
        }
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, int nw, TileRotateType rotateType)
    {
        BuildingTile *tileN = bte->tile(n);
        BuildingTile *tileW = bte->tile(w);
        if (tileN->isNone() && tileW->isNone())
            return;
        QString tileNames[MAP_ROTATION_COUNT];
        tileNames[0] = tileN->name();
        tileNames[3] = tileW->name();
        if (nw != -1) {
            BuildingTile *tileNW = bte->tile(nw);
            if (!tileNW->isNone()) {
                tileNames[1] = tileNW->name();
            }
        }
        addFurnitureTilesForRotateInfo(tileNames, rotateType);
    }

    void initFromBuildingTiles(BuildingTileEntry* bte, int n, int w, TileRotateType rotateType)
    {
        int nw = (rotateType == TileRotateType::Wall) ? BTC_Walls::NorthWest : -1;
        initFromBuildingTiles(bte, n, w, nw, rotateType);
    }

    FurnitureTile::FurnitureOrientation orient[MAP_ROTATION_COUNT] = {
        FurnitureTile::FurnitureN,
        FurnitureTile::FurnitureE,
        FurnitureTile::FurnitureS,
        FurnitureTile::FurnitureW
    };

    void addFurnitureTilesForRotateInfo(QString tileNames[MAP_ROTATION_COUNT], TileRotateType rotateType)
    {
        TRWFurnitureTiles* furnitureTiles1 = new TRWFurnitureTiles();
        furnitureTiles1->mType = rotateType;
        for (int j = 0; j < 4; j++) {
            FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
            BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileNames[j]);
            if (!buildingTile->isNone()) {
                furnitureTile1->setTile(0, 0, buildingTile);
            }
            furnitureTiles1->setTile(furnitureTile1);
        }
        mFurnitureTiles += furnitureTiles1;
    }

    MapRotation rotation[MAP_ROTATION_COUNT] = {
        MapRotation::NotRotated,
        MapRotation::Clockwise90,
        MapRotation::Clockwise180,
        MapRotation::Clockwise270
    };

    bool initFromBuildingTiles(FurnitureTile* ft[4])
    {
        if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
            return false;
        int width = ft[0]->width();
        int height = ft[0]->height();
        for (int dy = 0; dy < height; dy++) {
            for (int dx = 0; dx < width; dx++) {
                for (int i = 0; i < 4; i++) {
                    QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                    BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                    if (buildingTile != nullptr) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void initGrime()
    {
#if 0
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        WestTrim,
        NorthTrim,
        NorthWestTrim,
        SouthEastTrim,
        WestDoubleLeft,
        WestDoubleRight,
        NorthDoubleLeft,
        NorthDoubleRight,
#endif
        BuildingTileCategory *slopes = BuildingTilesMgr::instance()->catGrimeWall();
        for (BuildingTileEntry *entry : slopes->entries()) {
            initFromBuildingTiles(entry, BTC_GrimeWall::North, BTC_GrimeWall::West, BTC_GrimeWall::NorthWest, TileRotateType::Wall);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthDoor, BTC_GrimeWall::WestDoor, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthWindow, BTC_GrimeWall::WestWindow, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_GrimeWall::NorthTrim, BTC_GrimeWall::WestTrim, BTC_GrimeWall::NorthWestTrim, TileRotateType::Wall);
            // DoubleLeft/DoubleRight should be single FurnitureTile
        }
    }

    void initRoofCaps()
    {
#if 0
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        // Cap tiles for shallow (garage, trailer, etc) roofs
        CapShallowRiseS1, CapShallowRiseS2, CapShallowFallS1, CapShallowFallS2,
        CapShallowRiseE1, CapShallowRiseE2, CapShallowFallE1, CapShallowFallE2,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofCaps();
        for (BuildingTileEntry *entry : category->entries()) {
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapRiseE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapRiseE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapRiseS3, BTC_RoofCaps::CapRiseE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapFallE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapFallE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapFallS3, BTC_RoofCaps::CapFallE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapE2, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapGapS3, BTC_RoofCaps::CapGapE3, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowRiseS1, BTC_RoofCaps::CapShallowRiseE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowRiseS2, BTC_RoofCaps::CapShallowRiseE2, TileRotateType::WallExtra);

            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowFallS1, BTC_RoofCaps::CapShallowFallE1, TileRotateType::WallExtra);
            initFromBuildingTiles(entry, BTC_RoofCaps::CapShallowFallS2, BTC_RoofCaps::CapShallowFallE2, TileRotateType::WallExtra);
        }
    }

    void initRoofSlope(BuildingTileEntry *entry, int north, int west)
    {
        initFromBuildingTiles(entry, north, west, TileRotateType::None);
    }

    void initRoofSlope(BuildingTileEntry *bte, int north, int east, int west)
    {
        BuildingTile *tileN = bte->tile(north);
        BuildingTile *tileE = bte->tile(east);
        BuildingTile *tileW = bte->tile(west);
        if (tileN->isNone() || tileE->isNone() || tileW->isNone())
            return;
        QString tileNames[MAP_ROTATION_COUNT];
        tileNames[TileRotateInfo::NORTH] = tileN->name();
        tileNames[TileRotateInfo::EAST] = tileE->name();
        tileNames[TileRotateInfo::WEST] = tileW->name();
        addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
    }

    void initRoofSlopes()
    {
#if 0
        // Sloped sides
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Shallow sides
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,

        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,
#endif
        BuildingTileCategory *category = BuildingTilesMgr::instance()->catRoofSlopes();
        for (BuildingTileEntry *entry : category->entries()) {
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeE1);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeE2);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeS3, BTC_RoofSlopes::SlopeE3);

            initRoofSlope(entry, BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E);
            initRoofSlope(entry, BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E);

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN1);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE1);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS1);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW1);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    QString tileNames[MAP_ROTATION_COUNT];
                    tileNames[TileRotateInfo::NORTH] = tileN->name();
                    tileNames[TileRotateInfo::EAST] = tileE->name();
                    tileNames[TileRotateInfo::SOUTH] = tileS->name();
                    tileNames[TileRotateInfo::WEST] = tileW->name();
                    addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
                }
            }

            {
                BuildingTile *tileN = entry->tile(BTC_RoofSlopes::ShallowSlopeN2);
                BuildingTile *tileE = entry->tile(BTC_RoofSlopes::ShallowSlopeE2);
                BuildingTile *tileS = entry->tile(BTC_RoofSlopes::ShallowSlopeS2);
                BuildingTile *tileW = entry->tile(BTC_RoofSlopes::ShallowSlopeW2);
                if (!tileN->isNone() && !tileE->isNone() && !tileS->isNone() && !tileW->isNone()) {
                    QString tileNames[MAP_ROTATION_COUNT];
                    tileNames[TileRotateInfo::NORTH] = tileN->name();
                    tileNames[TileRotateInfo::EAST] = tileE->name();
                    tileNames[TileRotateInfo::SOUTH] = tileS->name();
                    tileNames[TileRotateInfo::WEST] = tileW->name();
                    addFurnitureTilesForRotateInfo(tileNames, TileRotateType::None);
                }
            }

            initRoofSlope(entry, BTC_RoofSlopes::Outer1, BTC_RoofSlopes::CornerSW1, BTC_RoofSlopes::CornerNE1);
            initRoofSlope(entry, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::CornerSW2, BTC_RoofSlopes::CornerNE2);
            initRoofSlope(entry, BTC_RoofSlopes::Outer3, BTC_RoofSlopes::CornerSW3, BTC_RoofSlopes::CornerNE3);
        }
    }

    void initFromBuildingTiles()
    {
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoors()->entries(), BTC_Doors::North, BTC_Doors::West, TileRotateType::Door);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catDoorFrames()->entries(), BTC_DoorFrames::North, BTC_DoorFrames::West, TileRotateType::DoorFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::Wall);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthDoor, BTC_Walls::WestDoor, TileRotateType::DoorFrame);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWalls()->entries(), BTC_Walls::NorthWindow, BTC_Walls::WestWindow, TileRotateType::WindowFrame);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catEWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);
        initFromBuildingTiles(BuildingTilesMgr::instance()->catIWallTrim()->entries(), BTC_Walls::North, BTC_Walls::West, TileRotateType::WallExtra);

        initFromBuildingTiles(BuildingTilesMgr::instance()->catWindows()->entries(), BTC_Windows::North, BTC_Windows::West, TileRotateType::Window);

        initGrime();
        initRoofCaps();
        initRoofSlopes();


        QList<FurnitureTile*> addThese;
        const QList<FurnitureGroup*> furnitureGroups = FurnitureGroups::instance()->groups();
#if 1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (initFromBuildingTiles(ft)) {
                    addThese << ft[0] << ft[1] << ft[2] << ft[3];
                }

                if (furnitureTiles->hasCorners()) {
                    ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureNE);
                    ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureSE);
                    ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureSW);
                    ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureNW);
                    if (initFromBuildingTiles(ft)) {
                        addThese << ft[0] << ft[1] << ft[2] << ft[3];
                    }
                }
            }
        }
#else
        // size=1x1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
                if (ft[0]->size() != QSize(1, 1))
                    continue;
                TileRotateInfo entry;
                bool empty = true;
                for (int i = 0; i < 4; i++) {
                    BuildingTile* buildingTile = ft[i]->tile(0, 0);
                    if (buildingTile != nullptr) {
                        entry.mTileNames[i] = buildingTile->name();
                        empty = false;
                    }
                }
                if (empty)
                    continue;
                addThese << ft[0] << ft[1] << ft[2] << ft[3];
            }
        }
        // size > 1x1
        for (FurnitureGroup* furnitureGroup : furnitureGroups) {
            for (FurnitureTiles* furnitureTiles : furnitureGroup->mTiles) {
                FurnitureTile* ft[4];
                ft[0] = furnitureTiles->tile(FurnitureTile::FurnitureN);
                ft[1] = furnitureTiles->tile(FurnitureTile::FurnitureE);
                ft[2] = furnitureTiles->tile(FurnitureTile::FurnitureS);
                ft[3] = furnitureTiles->tile(FurnitureTile::FurnitureW);
                if (ft[0]->isEmpty() && ft[1]->isEmpty() && ft[2]->isEmpty() && ft[3]->isEmpty())
                    continue;
//                if (ft[0]->size() == QSize(1, 1))
//                    continue;
                int width = ft[0]->width();
                int height = ft[0]->height();
                int add = false;
                // One new TileRotateEntry per BuildingTile in the grid.
                for (int dy = 0; dy < height; dy++) {
                    for (int dx = 0; dx < width; dx++) {
                        TileRotateInfo entry;
                        bool empty = true;
                        for (int i = 0; i < 4; i++) {
                            QPoint p = rotatePoint(width, height, rotation[i], QPoint(dx, dy));
                            BuildingTile* buildingTile = ft[i]->tile(p.x(), p.y());
                            if (buildingTile != nullptr) {
                                entry.mTileNames[i] = buildingTile->name();
                                empty = false;
                            }
                        }
                        if (empty)
                            continue;
                        add = true;
                    }
                }
                if (add) {
                    addThese << ft[0] << ft[1] << ft[2] << ft[3];
                }
            }
        }
#endif

        FurnitureTile::FurnitureOrientation orient[4] = {
            FurnitureTile::FurnitureN,
            FurnitureTile::FurnitureE,
            FurnitureTile::FurnitureS,
            FurnitureTile::FurnitureW
        };

        for (int i = 0; i < addThese.size(); i += 4) {
            FurnitureTile* ft[4];
            ft[0] = addThese[i+0];
            ft[1] = addThese[i+1];
            ft[2] = addThese[i+2];
            ft[3] = addThese[i+3];
            TRWFurnitureTiles* furnitureTiles1 = new TRWFurnitureTiles();
            FurnitureTiles::FurnitureLayer layer = FurnitureTiles::FurnitureLayer::InvalidLayer;
            if (ft[0])
                layer = ft[0]->owner()->layer();
            else if (ft[1])
                layer = ft[1]->owner()->layer();
            switch (layer) {
            case FurnitureTiles::FurnitureLayer::LayerWallOverlay:
            case FurnitureTiles::FurnitureLayer::LayerFrames:
            case FurnitureTiles::FurnitureLayer::LayerWalls:
                furnitureTiles1->mType = TileRotateType::WallExtra;
                break;
            default:
                furnitureTiles1->mType = TileRotateType::None;
                break;
            }
            for (int j = 0; j < 4; j++) {
                FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, orient[j]);
                for (int dy = 0; dy < ft[j]->height(); dy++) {
                    for (int dx = 0; dx < ft[j]->width(); dx++) {
                        BuildingTile* buildingTile = ft[j]->tile(dx, dy);
                        if (buildingTile != nullptr) {
                            furnitureTile1->setTile(dx, dy, buildingTile);
                        }
                    }
                }
                furnitureTiles1->setTile(furnitureTile1);
            }
            mFurnitureTiles += furnitureTiles1;
        }
    }

    QList<TRWFurnitureTiles*> mFurnitureTiles;
};
#endif

// // // // //

TileRotationWindow::TileRotationWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileRotationWindow),
    mZoomable(new Zoomable),
    mCurrentVisual(nullptr),
    mCurrentVisualRotation(MapRotation::NotRotated),
    mCurrentTileset(nullptr),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);
    ui->toolBar->addActions(QList<QAction*>() << undoAction << redoAction);

    for (int i = 0; TileRotatedVisual::EDGE_NAMES[i] != nullptr; i++) {
        ui->edgeComboBox->addItem(QLatin1String(TileRotatedVisual::EDGE_NAMES[i]));
    }
    connect(ui->edgeComboBox, QOverload<int>::of(&QComboBox::activated), this, &TileRotationWindow::edgeComboActivated);

    QButtonGroup *radioEdgeGroup = new QButtonGroup(this);
    radioEdgeGroup->addButton(ui->radioEdgeNone);
    radioEdgeGroup->addButton(ui->radioEdgeN);
    radioEdgeGroup->addButton(ui->radioEdgeE);
    radioEdgeGroup->addButton(ui->radioEdgeS);
    radioEdgeGroup->addButton(ui->radioEdgeW);
    radioEdgeGroup->setId(ui->radioEdgeNone, 0);
    radioEdgeGroup->setId(ui->radioEdgeN, 1);
    radioEdgeGroup->setId(ui->radioEdgeE, 2);
    radioEdgeGroup->setId(ui->radioEdgeS, 3);
    radioEdgeGroup->setId(ui->radioEdgeW, 4);

    connect(radioEdgeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &TileRotationWindow::edgeRadioClicked);

    connect(ui->checkBoxDX, &QCheckBox::toggled, this, &TileRotationWindow::changeDataOffsetDX);
    connect(ui->checkBoxDY, &QCheckBox::toggled, this, &TileRotationWindow::changeDataOffsetDY);

    connect(mUndoGroup, &QUndoGroup::cleanChanged, this, &TileRotationWindow::syncUI);

    connect(ui->actionNew, &QAction::triggered, this, &TileRotationWindow::fileNew);
    connect(ui->actionOpen, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileOpen));
    connect(ui->actionSave, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileSave));
    connect(ui->actionSaveAs, &QAction::triggered, this, &TileRotationWindow::fileSaveAs);
    connect(ui->actionClose, &QAction::triggered, this, &TileRotationWindow::close);
    connect(ui->actionCreateVisual, &QAction::triggered, this, &TileRotationWindow::createVisual);
    connect(ui->actionClearVisualTiles, &QAction::triggered, this, &TileRotationWindow::clearVisual);
    connect(ui->actionDeleteVisual, &QAction::triggered, this, &TileRotationWindow::deleteVisual);

    ui->visualList->setAcceptDrops(true);
    ui->visualList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(ui->visualList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::visualListSelectionChanged);
    connect(ui->visualList, &QTableView::activated, this, &TileRotationWindow::visualActivated);
    connect(ui->visualList->model(), &TileRotatedVisualModel::tileDropped, this, &TileRotationWindow::tileDroppedOntoVisualView);

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(ui->tilesetList, &QListWidget::itemSelectionChanged, this, &TileRotationWindow::tilesetSelectionChanged);
    connect(ui->tilesetMgr, &QToolButton::clicked, this,&TileRotationWindow::manageTilesets);

    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded, this, &TileRotationWindow::tilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAboutToBeRemoved, this, &TileRotationWindow::tilesetAboutToBeRemoved);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved, this, &TileRotationWindow::tilesetRemoved);

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged, this, &TileRotationWindow::tilesetChanged);

    ui->tilesetFilter->setClearButtonEnabled(true);
    ui->tilesetFilter->setEnabled(false);
    connect(ui->tilesetFilter, &QLineEdit::textEdited, this, &TileRotationWindow::filterEdited);

//    ui->tilesetTilesView->setZoomable(mZoomable);
    ui->tilesetTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tilesetTilesView->setDragEnabled(true);
//    connect(ui->tilesetTilesView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->tilesetTilesView, &MixedTilesetView::activated, this, &TileRotationWindow::tileActivated);

    ui->visualDataView->setZoomable(mZoomable);
    ui->visualDataView->setAcceptDrops(true);
    connect(ui->visualDataView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::visualDataSelectionChanged);
    connect(ui->visualDataView->model(), &MixedTilesetModel::tileDropped, this, &TileRotationWindow::tileDroppedOntoVisualDataView);
#if 1
//    ui->tileRotateView->setItemDelegate(new ::TileRotateDelegate(ui->tileRotateView, ui->tileRotateView, this));
#else
    connect(ui->tileRotateView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->tileRotateView->model(), &TileRotateModel::tileDropped, this, &TileRotationWindow::tileDropped);
    connect(ui->tileRotateView, &TileRotateView::activated, this, &TileRotationWindow::tileRotatedActivated);
#endif
    setTilesetList();
    syncUI();

#if 0
    InitFromBuildingTiles initFromBuildingTiles;
    initFromBuildingTiles.init(this);
    mTilesetRotatedList = initFromBuildingTiles.mTilesets;
    mVisuals = initFromBuildingTiles.mVisuals;
    mCurrentVisual = nullptr;
    std::sort(mTilesetRotatedList.begin(), mTilesetRotatedList.end(),
              [](TilesetRotated* a, TilesetRotated *b) {
                  return a->name() < b->name();
              });
    for (TilesetRotated *tileset : mTilesetRotatedList) {
        mTilesetByName[tileset->name()] = tileset;
    }
    setVisualList();
    mFileName = QLatin1Literal("D:/pz/TileRotation.txt");
#endif
}

TileRotationWindow::~TileRotationWindow()
{
    delete ui;
    qDeleteAll(mTilesetRotatedList);
}

void TileRotationWindow::fileNew()
{
    if (!confirmSave())
        return;

    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return;

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mTilesetRotatedList);
    mTilesetRotatedList.clear();
    mVisuals.clear();
    mUnassignedVisuals.clear();
    mTilesetByName.clear();

    mCurrentVisual = nullptr;
    ui->visualList->clear();
    ui->visualDataView->clear();
    syncUI();
}

void TileRotationWindow::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("TileRotationWindow/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .txt file"),
                                                    lastPath,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    fileOpen(fileName);

    syncUI();
}

void TileRotationWindow::fileOpen(const QString &fileName)
{
    QList<TilesetRotated*> tilesets;
    QList<QSharedPointer<TileRotatedVisual>> visuals;
    if (!fileOpen(fileName, tilesets, visuals)) {
        QMessageBox::warning(this, tr("Error reading file"), mError);
        return;
    }

    mUndoStack->clear();
    mFileName = fileName;

    qDeleteAll(mTilesetRotatedList);
    mTilesetRotatedList = tilesets;
    mTilesetByName.clear();
    mUnassignedVisuals.clear(); // TODO: set this

    mTilesetRotatedList = tilesets;
    mVisuals = visuals;
    for (TilesetRotated *tileset : mTilesetRotatedList) {
        mTilesetByName[tileset->name()] = tileset;
    }
    mCurrentVisual = nullptr;
    setVisualList();
    syncUI();
}

void TileRotationWindow::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        mFileName.clear();
        ui->visualList->clear();
        ui->visualDataView->clear();
        qDeleteAll(mTilesetRotatedList);
        mTilesetRotatedList.clear();
        mUndoStack->clear();
        syncUI();
        event->accept();
    } else {
        event->ignore();
    }
}

bool TileRotationWindow::confirmSave()
{
    if (mFileName.isEmpty() || mUndoStack->isClean())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return fileSave();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

QString TileRotationWindow::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("TileRotationWindow/LastOpenPath");
    QString suggestedFileName = QLatin1String("TileRotation.txt");
    if (mFileName.isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty()) {
            suggestedFileName = lastPath + QLatin1String("/TileRotation.txt");
        }
    } else {
        suggestedFileName = mFileName;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absoluteFilePath());

    return fileName;
}

bool TileRotationWindow::fileSave()
{
    if (mFileName.length())
        return fileSave(mFileName);
    return fileSaveAs();
}

bool TileRotationWindow::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

void TileRotationWindow::createVisual()
{
    QSharedPointer<TileRotatedVisual> visual(new TileRotatedVisual());
    visual->mUuid = QUuid::createUuid();
    mUnassignedVisuals += visual;
    mUndoStack->push(new CreateVisual(this, mVisuals.size(), visual));
}

void TileRotationWindow::clearVisual()
{
    if (mCurrentVisual == nullptr) {
        return;
    }

    mUndoStack->beginMacro(tr("Clear Tiles"));

    for (TilesetRotated *tilesetR : mTilesetRotatedList) {
        for (TileRotated *tileR : tilesetR->mTiles) {
            if (tileR->mVisual != mCurrentVisual || tileR->mRotation != mCurrentVisualRotation) {
                continue;
            }
            mUndoStack->push(new AssignVisual(this, tileR, nullptr, MapRotation::NotRotated));
        }
    }

    TileRotatedVisualData data;
    mUndoStack->push(new ChangeTiles(this, mCurrentVisual, mCurrentVisualRotation, data));

    mUndoStack->endMacro();
}

void TileRotationWindow::deleteVisual()
{
    if (mCurrentVisual == nullptr) {
        return;
    }
    int index = mVisuals.indexOf(mCurrentVisual);
    if (index == -1) {
        return;
    }
    QSharedPointer<TileRotatedVisual> visual = mCurrentVisual;
    for (auto& data : visual->mData) {
        for (const QString& tileName : data.mTileNames) {
            int dbg = 1;
        }
    }
    // Un-assign from all tiles
    mUndoStack->beginMacro(tr("Delete Visual"));
    for (TilesetRotated *tilesetR : mTilesetRotatedList) {
        for (TileRotated *tileR : tilesetR->mTiles) {
            if (tileR->mVisual != visual) {
                continue;
            }
            mUndoStack->push(new AssignVisual(this, tileR, nullptr, MapRotation::NotRotated));
        }
    }
    mUndoStack->push(new DeleteVisual(this, index));
    mUndoStack->endMacro();
}

bool TileRotationWindow::fileSave(const QString &fileName)
{
    if (!fileSave(fileName, mTilesetRotatedList, mVisuals)) {
        QMessageBox::warning(this, tr("Error writing file"), mError);
        return false;
    }
    mFileName = fileName;
    mUndoStack->setClean();
    syncUI();
    TileRotation::instance()->reload(); // hack
    return true;
}

bool TileRotationWindow::fileOpen(const QString &fileName, QList<TilesetRotated *> &tilesets, QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals)
{
    TileRotationFile file;
    if (!file.read(fileName)) {
        mError = file.errorString();
        return false;
    }
    tilesets = file.takeTilesets();
    visuals = file.takeVisuals();
    return true;
}

bool TileRotationWindow::fileSave(const QString &fileName, const QList<TilesetRotated *> &tilesets, const QList<QSharedPointer<Tiled::TileRotatedVisual>>& visuals)
{
    TileRotationFile file;
    if (!file.write(fileName, tilesets, visuals)) {
        mError = file.errorString();
        return false;
    }
    return true;
}

void TileRotationWindow::updateWindowTitle()
{
    if (mFileName.length()) {
        QString fileName = QDir::toNativeSeparators(mFileName);
        setWindowTitle(tr("[*]%1 - Tile Rotation").arg(fileName));
    } else {
        setWindowTitle(QLatin1Literal("Tile Rotation"));
    }
    setWindowModified(!mUndoStack->isClean());
}

void TileRotationWindow::syncUI()
{
    bool wasSynchingUI = mSynchingUI;
    mSynchingUI = true;

    ui->actionSave->setEnabled(!mFileName.isEmpty() && !mUndoStack->isClean());
    ui->actionSaveAs->setEnabled(!mFileName.isEmpty());

    QModelIndexList selected = ui->visualList->selectionModel()->selectedIndexes();
    ui->actionCreateVisual->setEnabled(!mFileName.isEmpty());
    ui->actionClearVisualTiles->setEnabled(selected.size() > 0);
    ui->actionDeleteVisual->setEnabled(selected.size() > 0);

    bool dataSelected = mCurrentVisual != nullptr && !ui->visualDataView->selectionModel()->selectedIndexes().isEmpty();
    if (dataSelected) {
        int mr = int(mCurrentVisualRotation);
        const TileRotatedVisualData& data = mCurrentVisual->mData[mr];
        QModelIndex dataIndex = ui->visualDataView->selectionModel()->selectedIndexes().first();
        int di = dataIndex.column();
        if (di < data.mTileNames.size()) {
            ui->edgeComboBox->setCurrentIndex(int(data.mEdges[di]));
            switch (data.mEdges[di]) {
            case TileRotatedVisualEdge::None:
                ui->radioEdgeNone->setChecked(true);
                break;
            case TileRotatedVisualEdge::North:
                ui->radioEdgeN->setChecked(true);
                break;
            case TileRotatedVisualEdge::East:
                ui->radioEdgeE->setChecked(true);
                break;
            case TileRotatedVisualEdge::South:
                ui->radioEdgeS->setChecked(true);
                break;
            case TileRotatedVisualEdge::West:
                ui->radioEdgeW->setChecked(true);
                break;
            default:
                break;
            }
            ui->checkBoxDX->setChecked(data.mOffsets[di].x() != 0);
            ui->checkBoxDY->setChecked(data.mOffsets[di].y() != 0);
        } else {
            dataSelected = false;
        }
    }
    if (!dataSelected) {
        ui->edgeComboBox->setCurrentIndex(0);
        ui->radioEdgeNone->setChecked(true);
        ui->checkBoxDX->setChecked(false);
        ui->checkBoxDY->setChecked(false);
    }
    ui->edgeComboBox->setEnabled(dataSelected);
    ui->radioEdgeNone->setEnabled(dataSelected);
    ui->radioEdgeN->setEnabled(dataSelected);
    ui->radioEdgeE->setEnabled(dataSelected);
    ui->radioEdgeS->setEnabled(dataSelected);
    ui->radioEdgeW->setEnabled(dataSelected);
    ui->checkBoxDX->setEnabled(dataSelected);
    ui->checkBoxDY->setEnabled(dataSelected);

    updateWindowTitle();

    mSynchingUI = wasSynchingUI;
}

void TileRotationWindow::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == nullptr)
        return;
    QString tileName = BuildingTilesMgr::nameForTile(tile);
    if (TileRotated *tileR = getTileRotatedForTileReal(tile)) {
        if (tileR->mVisual == nullptr) {
            return;
        }
        ui->visualList->setCurrentIndex(ui->visualList->model()->index(tileR->mVisual, tileR->mRotation));
    }
#if 0
//    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileName);
    for (TileRotated *tile : mCurrentTilesetRotated->mTiles) {
        if (BuildingTilesMgr::instance()->nameForTile(tile->mTileset->nameUnrotated(), tile->mID) == tileName) {
            ui->tileRotateView->setCurrentIndex(ui->tileRotateView->model()->index(tile));
            return;
        }
    }
#endif
}

void TileRotationWindow::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : nullptr;
    mCurrentTileset = nullptr;
    if (item) {
        int row = ui->tilesetList->row(item);
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
        if (mCurrentTileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(mCurrentTileset);
            updateUsedTiles();
        }
    } else {
        ui->tilesetTilesView->clear();
    }
    setVisualList();
    syncUI();
}

void TileRotationWindow::tileRotatedActivated(const QModelIndex &index)
{
#if 0 // TODO
    TileRotateModel *m = ui->tileRotateView->model();
    if (TileRotated *tile = m->tileAt(index)) {
        for (BuildingTile *btile : ftile->tiles()) {
            if (btile != nullptr) {
                displayTileInTileset(btile);
                break;
            }
        }
    }
#endif
}

void TileRotationWindow::visualListSelectionChanged()
{
    QModelIndexList selection = ui->visualList->selectionModel()->selectedIndexes();
    mCurrentVisual = nullptr;
    mCurrentVisualRotation = MapRotation::NotRotated;
    if (selection.isEmpty() == false) {
        mCurrentVisual = ui->visualList->model()->visualAt(selection.first(), mCurrentVisualRotation);
    } else {
        ui->visualDataView->clear();
    }
    setVisualDataList();
    if ((mCurrentVisual != nullptr) && !mCurrentVisual->mData[int(mCurrentVisualRotation)].mTileNames.isEmpty()) {
        QModelIndex index = ui->visualDataView->model()->index(1, 0); // row=1 due to hidden header?
//        Tile *tile = ui->visualDataView->model()->tileAt(index);
        ui->visualDataView->selectionModel()->select(index, QItemSelectionModel::SelectCurrent/* | QItemSelectionModel::Rows*/);
    }
    syncUI();
}

void TileRotationWindow::visualActivated(const QModelIndex &index)
{
    MapRotation mapRotation;
    auto visual = ui->visualList->model()->visualAt(index, mapRotation);
    for (auto* tilesetR : mTilesetRotatedList) {
        for (auto *tileR : tilesetR->mTiles) {
            if (tileR->mVisual == visual) {
                auto& data = tileR->mVisual->mData[int(mapRotation)];
                if (data.mTileNames.isEmpty()) {
                    return;
                }
                QString tileName = data.mTileNames.first();
                QString tilesetName;
                int index;
                BuildingTilesMgr::instance()->parseTileName(tileName, tilesetName, index);
                Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName);
                for (int i = 0; i < ui->tilesetList->count(); i++) {
                    QListWidgetItem *item = ui->tilesetList->item(i);
                    if (item->text() == tilesetName) {
                        // FIXME: the tileset won't be displayed if the filter is in effect.
                        ui->tilesetList->setCurrentRow(i);
                        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
                        return;
                    }
                }
            }
        }
    }
}

void TileRotationWindow::visualDataSelectionChanged()
{
    syncUI();
}

void TileRotationWindow::edgeComboActivated(int index)
{
    if (mSynchingUI)
        return;
    QModelIndexList selection = ui->visualDataView->selectionModel()->selectedIndexes();
    if (selection.isEmpty())
        return;
    QModelIndex dataIndex = selection.first();
    if (dataIndex.isValid()) {
        int mr = int(mCurrentVisualRotation);
        TileRotatedVisualData data = mCurrentVisual->mData[mr];
        int di = dataIndex.column();
        data.mEdges[di] = TileRotatedVisualEdge(index);
        mUndoStack->push(new ChangeTiles(this, mCurrentVisual, mCurrentVisualRotation, data));
    }
}

void TileRotationWindow::edgeRadioClicked(int id)
{
    switch (id) {
    case 0:
        changeDataEdge(TileRotatedVisualEdge::None);
        break;
    case 1:
        changeDataEdge(TileRotatedVisualEdge::North);
        break;
    case 2:
        changeDataEdge(TileRotatedVisualEdge::East);
        break;
    case 3:
        changeDataEdge(TileRotatedVisualEdge::South);
        break;
    case 4:
        changeDataEdge(TileRotatedVisualEdge::West);
        break;
    }
}

void TileRotationWindow::changeDataOffsetDX(bool dx)
{
    changeDataOffset(dx ? 1 : 0, -1);
}

void TileRotationWindow::changeDataOffsetDY(bool dy)
{
    changeDataOffset(-1, dy ? 1 : 0);
}

void TileRotationWindow::displayTileInTileset(Tiled::Tile *tile)
{
    if (tile == nullptr)
        return;
    int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset());
    if (row >= 0) {
        // FIXME: filter changes displayed tilesets
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void TileRotationWindow::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
}

void TileRotationWindow::setVisualList()
{
    QSet<TileRotatedVisual*> visualSet;
    QList<QSharedPointer<TileRotatedVisual>> visuals;
    if (mCurrentTileset != nullptr) {
        if (TilesetRotated *tilesetR = mTilesetByName[mCurrentTileset->name()]) {
            for (int i = 0; i < tilesetR->mTileByID.size(); i++) {
                TileRotated *tileR = tilesetR->mTileByID[i];
                if (tileR == nullptr)
                    continue;
                if (tileR->mVisual && !visualSet.contains(tileR->mVisual.data())) {
                    visuals += tileR->mVisual;
                    visualSet += tileR->mVisual.data();
                }
            }
        }
    }

    // Also display any created but unassigned visuals.
    for (QSharedPointer<TileRotatedVisual> visual : mUnassignedVisuals) {
        visuals += visual;
    }
#if 0
    for (auto &visual : visuals) {
        for (auto &data : visual->mData) {
            if (data.mTileNames.isEmpty() == false) {
                int dbg = 1;
            }
        }
    }
#endif
    ui->visualList->setVisuals(visuals);
}

void TileRotationWindow::setVisualDataList()
{
    QModelIndexList dataSelection = ui->visualDataView->selectionModel()->selectedIndexes();
    QModelIndex dataIndex = dataSelection.isEmpty() ? QModelIndex() : dataSelection.first();

    ui->visualDataView->clear();

    if (mCurrentVisual == nullptr) {
        return;
    }

    MapRotation mapRotation;
    QModelIndex index = ui->visualList->selectionModel()->selectedIndexes().first();
    ui->visualList->model()->visualAt(index, mapRotation);
    QList<Tile*> tiles;
    for (const QString &tileName : mCurrentVisual->mData[int(mapRotation)].mTileNames) {
        if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) {
            tiles += tile;
        }
    }
    ui->visualDataView->setTiles(tiles);

    if (dataIndex.isValid() && (dataIndex.column() < tiles.size())) {
//        bool bWasSynch = mSynchingUI;
//        mSynchingUI = true;
        dataIndex = ui->visualDataView->model()->index(1, dataIndex.column()); // row=1 due to hidden header?
        ui->visualDataView->selectionModel()->select(dataIndex, QItemSelectionModel::SelectionFlag::SelectCurrent);
//        mSynchingUI = bWasSynch;
    }
}

void TileRotationWindow::setTilesetList()
{
    ui->tilesetList->clear();
    ui->tilesetFilter->setEnabled(!TileMetaInfoMgr::instance()->tilesets().isEmpty());
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    for (Tileset *tileset : TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing()) {
            item->setForeground(Qt::red);
        }
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setFixedWidth(width + 16 + sbw);
    ui->tilesetFilter->setFixedWidth(ui->tilesetList->width());
}

void TileRotationWindow::updateUsedTiles()
{
    if (mCurrentTileset == nullptr)
        return;

    for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
        QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(mCurrentTileset->name(), i);
        if (mHoverTileName.isEmpty() == false) {
            if (mHoverTileName == tileName) {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
            } else {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
            }
            continue;
        }
        if (isTileUsed(tileName)) {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
        } else {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
        }
    }
    ui->tilesetTilesView->model()->redisplay();
}

void TileRotationWindow::filterEdited(const QString &text)
{
    QListWidget* mTilesetNamesView = ui->tilesetList;

    for (int row = 0; row < mTilesetNamesView->count(); row++) {
        QListWidgetItem* item = mTilesetNamesView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = mTilesetNamesView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = mTilesetNamesView->row(current) - 1;
        while (row >= 0 && mTilesetNamesView->item(row)->isHidden()) {
            row--;
        }
        if (row >= 0) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = mTilesetNamesView->row(current) + 1;
        while (row < mTilesetNamesView->count() && mTilesetNamesView->item(row)->isHidden()) {
            row++;
        }
        if (row < mTilesetNamesView->count()) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // All items hidden
        mTilesetNamesView->setCurrentItem(nullptr);
    }

    current = mTilesetNamesView->currentItem();
    if (current != nullptr) {
        mTilesetNamesView->scrollToItem(current);
    }
}

bool TileRotationWindow::isTileUsed(const QString &_tileName)
{
    QString tilesetName;
    int index;
    if (!BuildingTilesMgr::instance()->parseTileName(_tileName, tilesetName, index)) {
        return false;
    }
    if (TilesetRotated *tilesetR = getTilesetRotated(tilesetName)) {
        if (TileRotated *tileR = tilesetR->tileAt(index)) {
            if (tileR->mVisual != nullptr) {
                return true;
            }
        }
    }
    return false;
}

void TileRotationWindow::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
    }
}

void TileRotationWindow::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
}

void TileRotationWindow::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void TileRotationWindow::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
}

// Called when a tileset image changes or a missing tileset was found.
void TileRotationWindow::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        if (tileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(tileset);
        }
    }

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesetList->item(row)) {
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
    }
}

void TileRotationWindow::tileDroppedOntoVisualView(QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation, const QString &tileName)
{
    Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName);
    if ((tile == BuildingTilesMgr::instance()->noneTiledTile()) || (tile == TilesetManager::instance()->missingTile())) {
        return;
    }

    mUndoStack->beginMacro(tr("Change Visual Appearance"));

    TileRotatedVisualData data;
    data.addTile(tileName);
    mUndoStack->push(new ChangeTiles(this, visual, mapRotation, data));

    if (TileRotated *tileR = getOrCreateTileRotatedForTileReal(tile)) {
        if ((tileR->mVisual != visual) || (tileR->mRotation != mapRotation)) {
            mUndoStack->push(new AssignVisual(this, tileR, visual, mapRotation));
        }
    }

    mUndoStack->endMacro();
}

void TileRotationWindow::tileDroppedOntoVisualDataView(const QString &tilesetName, int tileID)
{
    QString tileName = BuildingTilesMgr::instance()->nameForTile(tilesetName, tileID);

    TileRotatedVisualData data = mCurrentVisual->mData[int(mCurrentVisualRotation)];
    data.addTile(tileName);
    mUndoStack->push(new ChangeTiles(this, mCurrentVisual, mCurrentVisualRotation, data));

    QModelIndex dataIndex = ui->visualDataView->model()->index(1, data.mTileNames.size() - 1); // row=1 due to hidden header?
    ui->visualDataView->selectionModel()->select(dataIndex, QItemSelectionModel::SelectionFlag::SelectCurrent);
}

void TileRotationWindow::addVisual(QSharedPointer<TileRotatedVisual> visual, int index)
{
    mVisuals.insert(index, visual);
    setVisualList();
//    tile->mVisual.addTile(tileName);
//    updateUsedTiles();
}

QSharedPointer<TileRotatedVisual> TileRotationWindow::removeVisual(int index)
{
    if (index < 0 || index >= mVisuals.size()) {
        return QSharedPointer<TileRotatedVisual>();
    }
    QSharedPointer<TileRotatedVisual> result = mVisuals.takeAt(index);
    if (result == mCurrentVisual) {
        mCurrentVisual = nullptr;
    }
    if (mUnassignedVisuals.contains(result)) {
        mUnassignedVisuals.removeOne(result);
    }
    setVisualList();
    return result;
}

void TileRotationWindow::changeDataEdge(TileRotatedVisualEdge edge)
{
    if (mSynchingUI)
        return;
    QModelIndexList selection = ui->visualDataView->selectionModel()->selectedIndexes();
    if (selection.isEmpty())
        return;
    QModelIndex dataIndex = selection.first();
    if (!dataIndex.isValid())
        return;
    int mr = int(mCurrentVisualRotation);
    TileRotatedVisualData data = mCurrentVisual->mData[mr];
    int di = dataIndex.column();
    data.mEdges[di] = edge;
    mUndoStack->push(new ChangeTiles(this, mCurrentVisual, mCurrentVisualRotation, data));
}

void TileRotationWindow::changeDataOffset(int x, int y)
{
    if (mSynchingUI)
        return;
    QModelIndexList selection = ui->visualDataView->selectionModel()->selectedIndexes();
    if (selection.isEmpty())
        return;
    QModelIndex dataIndex = selection.first();
    if (dataIndex.isValid()) {
        int mr = int(mCurrentVisualRotation);
        TileRotatedVisualData data = mCurrentVisual->mData[mr];
        int di = dataIndex.column();
        if (x != -1)
            data.mOffsets[di].setX(x);
        if (y != -1)
            data.mOffsets[di].setY(y);
        mUndoStack->push(new ChangeTiles(this, mCurrentVisual, mCurrentVisualRotation, data));
    }
}

TileRotationWindow::AssignedVisual TileRotationWindow::assignVisual(TileRotated *tileR, QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation)
{
    AssignedVisual old(tileR);
    tileR->mVisual = visual;
    tileR->mRotation = mapRotation;
    if ((visual != nullptr) && mUnassignedVisuals.contains(visual)) {
        mUnassignedVisuals.removeAll(visual);
    }
    updateUsedTiles();
    return old;
}

TileRotatedVisualData TileRotationWindow::changeVisualData(QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation, const TileRotatedVisualData &data)
{
    TileRotatedVisualData old = visual->mData[int(mapRotation)];
    visual->mData[int(mapRotation)] = data;
    setVisualDataList();
    syncUI();
    ui->visualList->model()->redisplay();
    return old;
}

TilesetRotated* TileRotationWindow::getOrCreateTilesetRotated(const QString &tilesetName)
{
    TilesetRotated *tileset = mTilesetByName[tilesetName];
    if (tileset == nullptr) {
        tileset = new TilesetRotated();
        tileset->mName = tilesetName;
        if (Tileset *tileset1 = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            tileset->mColumnCount = tileset1->columnCount();
        } else {
            tileset->mColumnCount = 8; // FIXME
        }
        mTilesetByName[tilesetName] = tileset;
        mTilesetRotatedList += tileset;
    }
    return tileset;
}

TilesetRotated *TileRotationWindow::getTilesetRotated(const QString &tilesetName)
{
    if (mTilesetByName[tilesetName]) {
        return mTilesetByName[tilesetName];
    }
    return nullptr;
}

TileRotated* TileRotationWindow::getOrCreateTileRotated(TilesetRotated *tilesetR, int tileID)
{
    if (TileRotated *tileR = tilesetR->tileAt(tileID)) {
        return tileR;
    }
    return tilesetR->createTile(tileID);
}

TileRotated *TileRotationWindow::rotatedTileFor(Tile *tileR)
{
    if (!mTilesetByName.contains(tileR->tileset()->name())) {
        return nullptr;
    }
    TilesetRotated* tilesetR = mTilesetByName[tileR->tileset()->name()];
    return tilesetR->tileAt(tileR->id());
}

TileRotated *TileRotationWindow::getOrCreateTileRotatedForTileReal(Tile *tileReal)
{
    if (TilesetRotated *tilesetR = getOrCreateTilesetRotated(tileReal->tileset()->name())) {
        return getOrCreateTileRotated(tilesetR, tileReal->id());
    }
    return nullptr;
}

TileRotated *TileRotationWindow::getTileRotatedForTileReal(Tile *tileReal)
{
    if (TilesetRotated *tilesetR = getTilesetRotated(tileReal->tileset()->name())) {
        return tilesetR->tileAt(tileReal->id());
    }
    return nullptr;
}

