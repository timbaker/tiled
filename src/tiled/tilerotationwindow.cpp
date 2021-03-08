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

namespace  {

class TRWFurnitureTiles : public FurnitureTiles
{
public:
    TRWFurnitureTiles()
        : FurnitureTiles(false)
        , mType(TileRotateType::None)
    {

    }

    TRWFurnitureTiles(FurnitureTiles *other)
        : FurnitureTiles(false)
        , mType(TileRotateType::None)
    {
        for (FurnitureTile *furnitureTile : other->tiles()) {
            FurnitureTile* ftCopy = new FurnitureTile(this, furnitureTile);
            setTile(ftCopy);
        }
    }

    TileRotateType mType;
};

// namespace {}
}

class AddFurnitureTiles : public QUndoCommand
{
public:
    AddFurnitureTiles(TileRotationWindow *d, FurnitureGroup *group, int index, TRWFurnitureTiles *ftiles) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Furniture Tiles")),
        mDialog(d),
        mGroup(group),
        mIndex(index),
        mTiles(ftiles)
    {
    }

    ~AddFurnitureTiles() override
    {
        delete mTiles;
    }

    void undo() override
    {
        mTiles = mDialog->removeFurnitureTiles(mGroup, mIndex);
    }

    void redo() override
    {
        mDialog->insertFurnitureTiles(mGroup, mIndex, mTiles);
        mTiles = nullptr;
    }

    TileRotationWindow *mDialog;
    FurnitureGroup *mGroup;
    int mIndex;
    TRWFurnitureTiles *mTiles;
};

class ChangeFurnitureTile : public QUndoCommand
{
public:
    ChangeFurnitureTile(TileRotationWindow *d, FurnitureTile *ftile, int x, int y, const QString &tileName = QString()) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Tile")),
        mDialog(d),
        mTile(ftile),
        mX(x),
        mY(y),
        mTileName(tileName)
    {
    }

    void undo() override
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    void redo() override
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    TileRotationWindow *mDialog;
    FurnitureTile *mTile;
    int mX;
    int mY;
    QString mTileName;
};

class ChangeRotateType : public QUndoCommand
{
public:
    ChangeRotateType(TileRotationWindow *d, FurnitureTile *ftile, TileRotateType rotateType) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Rotate Type")),
        mDialog(d),
        mTile(ftile),
        mRotateType(rotateType)
    {
    }

    void undo() override
    {
        mRotateType = mDialog->changeRotateType(mTile, mRotateType);
    }

    void redo() override
    {
        mRotateType = mDialog->changeRotateType(mTile, mRotateType);
    }

    TileRotationWindow *mDialog;
    FurnitureTile *mTile;
    TileRotateType mRotateType;
};

class RemoveFurnitureTiles : public QUndoCommand
{
public:
    RemoveFurnitureTiles(TileRotationWindow *d, FurnitureGroup *group, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Furniture Tiles")),
        mDialog(d),
        mGroup(group),
        mIndex(index),
        mTiles(nullptr)
    {
    }

    ~RemoveFurnitureTiles() override
    {
        delete mTiles;
    }

    void undo() override
    {
        mDialog->insertFurnitureTiles(mGroup, mIndex, mTiles);
        mTiles = nullptr;
    }

    void redo() override
    {
        mTiles = mDialog->removeFurnitureTiles(mGroup, mIndex);
    }

    TileRotationWindow *mDialog;
    FurnitureGroup *mGroup;
    int mIndex;
    TRWFurnitureTiles *mTiles;
};

#if 1

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
    mCurrentTileset(nullptr),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this)),
    mFurnitureGroup(new FurnitureGroup())
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

    for (int i = 0; TILE_ROTATE_NAMES[i] != nullptr; i++) {
        ui->typeComboBox->addItem(QLatin1String(TILE_ROTATE_NAMES[i]));
    }
    connect(ui->typeComboBox, QOverload<int>::of(&QComboBox::activated), this, &TileRotationWindow::typeComboActivated);

    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(syncUI()));

    connect(ui->actionNew, &QAction::triggered, this, &TileRotationWindow::fileNew);
    connect(ui->actionOpen, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileOpen));
    connect(ui->actionSave, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileSave));
    connect(ui->actionSaveAs, &QAction::triggered, this, &TileRotationWindow::fileSaveAs);
    connect(ui->actionClose, &QAction::triggered, this, &TileRotationWindow::close);
    connect(ui->actionAddTiles, &QAction::triggered, this, &TileRotationWindow::addTiles);
    connect(ui->actionClearTiles, &QAction::triggered, this, &TileRotationWindow::clearTiles);
    connect(ui->actionRemove, &QAction::triggered, this, &TileRotationWindow::removeTiles);

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

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->model()->setShowResolved(false); // show the grid
    connect(ui->furnitureView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->furnitureView->model(), &FurnitureModel::furnitureTileDropped, this, &TileRotationWindow::furnitureTileDropped);
    connect(ui->furnitureView, &FurnitureView::activated, this, &TileRotationWindow::furnitureActivated);

    setTilesetList();
    syncUI();

#if 1
    InitFromBuildingTiles initFromBuildingTiles;
    initFromBuildingTiles.initFromBuildingTiles();

    mUndoStack->beginMacro(QLatin1Literal("TEST"));
    auto& furnitureTiles = initFromBuildingTiles.mFurnitureTiles;
    for (auto* furnitureTiles1 : furnitureTiles) {
//        TRWFurnitureTiles* furnitureTiles1 = new TRWFurnitureTiles();
        furnitureTiles1->setGroup(mFurnitureGroup);
#if 0
//        furnitureTiles1->mType = TODO;
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, furnitureTile->orient());
            for (int dy = 0; dy < furnitureTile->height(); dy++) {
                for (int dx = 0; dx < furnitureTile->width(); dx++) {
                    if (BuildingTile* buildingTile = furnitureTile->tile(dx, dy)) {
                        furnitureTile1->setTile(dx, dy, buildingTile);
                    }
                }
            }
            furnitureTiles1->setTile(furnitureTile1);
        }
#endif
        mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup, mFurnitureGroup->mTiles.size(), furnitureTiles1));
//        mFurnitureGroup->mTiles += furnitureTiles1;
    }
    mUndoStack->endMacro();
    mFileName = QLatin1Literal("D:/pz/TileRotation.txt");
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);
#endif
}

TileRotationWindow::~TileRotationWindow()
{
    delete ui;
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    delete mFurnitureGroup;
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
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);

    syncUI();
}

void TileRotationWindow::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
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
    QList<TileRotateFileInfo*> tileInfos;
    if (!fileOpen(fileName, tileInfos)) {
        QMessageBox::warning(this, tr("Error reading file"), mError);
        return;
    }

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    for (TileRotateFileInfo *tileInfo : tileInfos) {
        TRWFurnitureTiles *ftCopy = new TRWFurnitureTiles(tileInfo->mFurnitureTiles);
        ftCopy->setGroup(mFurnitureGroup);
        ftCopy->mType = tileInfo->mType;
        mFurnitureGroup->mTiles += ftCopy;
    }
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);
    syncUI();
}

void TileRotationWindow::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        mFileName.clear();
        ui->furnitureView->clear();
        qDeleteAll(mFurnitureGroup->mTiles);
        mFurnitureGroup->mTiles.clear();
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
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
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

void TileRotationWindow::addTiles()
{
    int index = mFurnitureGroup->mTiles.size();
//    if (mCurrentFurniture)
//        index = mFurnitureGroup->mTiles.indexOf(mCurrentFurniture->owner()) + 1;
    TRWFurnitureTiles *tiles = new TRWFurnitureTiles();
    tiles->mType = TileRotateType::None;
    mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup, index, tiles));
}

void TileRotationWindow::clearTiles()
{
    FurnitureView *v = ui->furnitureView;
    QList<FurnitureTile*> clear;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection) {
        FurnitureTile *tile = v->model()->tileAt(index);
        if (!tile->isEmpty())
            clear += tile;
    }
    if (clear.isEmpty())
        return;
    mUndoStack->beginMacro(tr("Clear Furniture Tiles"));
    for (FurnitureTile* ftile : clear) {
        for (int x = 0; x < ftile->width(); x++) {
            for (int y = 0; y < ftile->height(); y++) {
                if (ftile->tile(x, y)) {
                    mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y));
                }
            }
        }
    }
    mUndoStack->endMacro();
}

void TileRotationWindow::removeTiles()
{
    FurnitureView *v = ui->furnitureView;
    QList<FurnitureTiles*> remove;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    for (QModelIndex& index : selection) {
        FurnitureTile *tile = v->model()->tileAt(index);
        if (!remove.contains(tile->owner())) {
            remove += tile->owner();
        }
    }
    if (remove.count() > 1) {
        mUndoStack->beginMacro(tr("Remove Furniture Tiles"));
    }
    for (FurnitureTiles *tiles : remove) {
        mUndoStack->push(new RemoveFurnitureTiles(this, tiles->group(), tiles->indexInGroup()));
    }
    if (remove.count() > 1) {
        mUndoStack->endMacro();
    }
}

bool TileRotationWindow::fileSave(const QString &fileName)
{
    QList<TileRotateFileInfo*> tileInfos;
    for (FurnitureTiles *furnitureTiles : mFurnitureGroup->mTiles) {
        TRWFurnitureTiles *trwft = dynamic_cast<TRWFurnitureTiles*>(furnitureTiles);
        TileRotateFileInfo *tileInfo = new TileRotateFileInfo();
        tileInfo->mFurnitureTiles = furnitureTiles;
        tileInfo->mType = trwft->mType;
        tileInfo->mOwnsFurniture = false;
        tileInfos += tileInfo;
    }
    if (!fileSave(fileName, tileInfos)) {
        qDeleteAll(tileInfos);
        QMessageBox::warning(this, tr("Error writing file"), mError);
        return false;
    }
    qDeleteAll(tileInfos);
    mFileName = fileName;
    mUndoStack->setClean();
    syncUI();
    TileRotation::instance()->reload(); // hack
    return true;
}

bool TileRotationWindow::fileOpen(const QString &fileName, QList<TileRotateFileInfo *> &tiles)
{
    TileRotationFile file;
    if (!file.read(fileName)) {
        mError = file.errorString();
        return false;
    }
    tiles = file.takeTiles();
    return true;
}

bool TileRotationWindow::fileSave(const QString &fileName, const QList<TileRotateFileInfo *> &tiles)
{
    TileRotationFile file;
    if (!file.write(fileName, tiles)) {
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
    ui->actionSave->setEnabled(!mFileName.isEmpty() && !mUndoStack->isClean());
    ui->actionSaveAs->setEnabled(!mFileName.isEmpty());

    QModelIndexList selected = ui->furnitureView->selectionModel()->selectedIndexes();
    ui->actionAddTiles->setEnabled(!mFileName.isEmpty());
    ui->actionClearTiles->setEnabled(selected.size() > 0);
    ui->actionRemove->setEnabled(selected.size() > 0);

    ui->typeComboBox->setEnabled(selected.isEmpty() == false);
    if (selected.isEmpty()) {
        ui->typeComboBox->setCurrentIndex(0);
    } else {
        FurnitureTile* furnitureTile = ui->furnitureView->model()->tileAt(selected.first());
        TRWFurnitureTiles* furnitureTiles = dynamic_cast<TRWFurnitureTiles*>(furnitureTile->owner());
        ui->typeComboBox->setCurrentIndex(int(furnitureTiles->mType));
    }

    updateWindowTitle();
}

void TileRotationWindow::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == nullptr)
        return;
    QString tileName = BuildingTilesMgr::nameForTile(tile);
    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileName);
    for (FurnitureTiles* furnitureTiles : mFurnitureGroup->mTiles) {
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            if (furnitureTile->tiles().contains(buildingTile)) {
                ui->furnitureView->setCurrentIndex(ui->furnitureView->model()->index(furnitureTile));
                return;
            }
        }
    }
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
    syncUI();
}

void TileRotationWindow::furnitureActivated(const QModelIndex &index)
{
    FurnitureModel *m = ui->furnitureView->model();
    if (FurnitureTile *ftile = m->tileAt(index)) {
        for (BuildingTile *btile : ftile->tiles()) {
            if (btile != nullptr) {
                displayTileInTileset(btile);
                break;
            }
        }
    }
}

void TileRotationWindow::typeComboActivated(int index)
{
    if (index < 0 || index >= int(TileRotateType::MAX))
        return;
    TileRotateType type = TileRotateType(index);
    QModelIndexList selected = ui->furnitureView->selectionModel()->selectedIndexes();
    if (selected.isEmpty())
        return;
    FurnitureTile* furnitureTile = ui->furnitureView->model()->tileAt(selected.first());
    mUndoStack->push(new ChangeRotateType(this, furnitureTile, type));
}

void TileRotationWindow::displayTileInTileset(Tiled::Tile *tile)
{
    if (tile == nullptr)
        return;
    int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset());
    if (row >= 0) {
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void TileRotationWindow::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
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
    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(_tileName);
    if (buildingTile == nullptr)
        return false;
    for (FurnitureTiles* furnitureTiles : mFurnitureGroup->mTiles) {
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            if (furnitureTile->tiles().contains(buildingTile)) {
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

void TileRotationWindow::furnitureTileDropped(FurnitureTile *ftile, int x, int y, const QString &tileName)
{
    mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y, tileName));
}

void TileRotationWindow::insertFurnitureTiles(FurnitureGroup *group, int index, TRWFurnitureTiles *ftiles)
{
    group->mTiles.insert(index, ftiles);
    ftiles->setGroup(group);
    ui->furnitureView->setTiles(group->mTiles);
    updateUsedTiles();
}

TRWFurnitureTiles *TileRotationWindow::removeFurnitureTiles(FurnitureGroup *group, int index)
{
    TRWFurnitureTiles *ftiles = static_cast<TRWFurnitureTiles*>(group->mTiles.takeAt(index));
    ftiles->setGroup(nullptr);
    ui->furnitureView->model()->removeTiles(ftiles);
    updateUsedTiles();
    return ftiles;
}

QString TileRotationWindow::changeFurnitureTile(FurnitureTile *ftile, int x, int y, const QString &tileName)
{
    QString old = ftile->tile(x, y) ? ftile->tile(x, y)->name() : QString();
    QSize oldSize = ftile->size();
    BuildingTile *btile = tileName.isEmpty() ? nullptr : BuildingTilesMgr::instance()->get(tileName);
    if (btile && BuildingTilesMgr::instance()->tileFor(btile) && BuildingTilesMgr::instance()->tileFor(btile)->image().isNull()) {
        btile = nullptr;
    }
    ftile->setTile(x, y, btile);
    if (ftile->size() != oldSize) {
        ui->furnitureView->furnitureTileResized(ftile);
    }
    ui->furnitureView->update(ui->furnitureView->model()->index(ftile));
    updateUsedTiles();
    return old;
}

TileRotateType TileRotationWindow::changeRotateType(FurnitureTile *ftile, TileRotateType rotateType)
{
    TRWFurnitureTiles *ftiles = static_cast<TRWFurnitureTiles*>(ftile->owner());
    TileRotateType old = ftiles->mType;
    ftiles->mType = rotateType;
    ui->typeComboBox->setCurrentIndex(int(rotateType));
    return old;
}

