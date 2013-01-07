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

#include "buildingundoredo.h"

#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"

#include <QCoreApplication>

using namespace BuildingEditor;

ChangeRoomAtPosition::ChangeRoomAtPosition(BuildingDocument *doc, BuildingFloor *floor,
                                           const QPoint &pos, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room At Position")),
    mDocument(doc),
    mFloor(floor),
    mMergeable(false)
{
    Changed changed;
    changed.mPosition = pos;
    changed.mRoom = room;
    mChanged += changed;
}

bool ChangeRoomAtPosition::mergeWith(const QUndoCommand *other)
{
    const ChangeRoomAtPosition *o = static_cast<const ChangeRoomAtPosition*>(other);
    if (!(mFloor == o->mFloor &&
            o->mMergeable))
        return false;

    foreach (Changed changedOther, o->mChanged) {
        bool ignore = false;
        foreach (Changed changedSelf, mChanged) {
            if (changedSelf.mPosition == changedOther.mPosition) {
                ignore = true;
                break;
            }
        }
        if (!ignore)
            mChanged += changedOther;
    }

    return true;
}

void ChangeRoomAtPosition::swap()
{
    QVector<Changed> old;

    foreach (Changed changed, mChanged) {
        Changed changed2;
        changed2.mRoom = mDocument->changeRoomAtPosition(mFloor, changed.mPosition, changed.mRoom);
        changed2.mPosition = changed.mPosition;
        old += changed2;
    }

    mChanged = old;
}

/////

PaintRoom::PaintRoom(BuildingDocument *doc, BuildingFloor *floor, const QPoint &pos, Room *room) :
    ChangeRoomAtPosition(doc, floor, pos, room)
{
    setText(QCoreApplication::translate("Undo Commands", "Paint Room"));
}

EraseRoom::EraseRoom(BuildingDocument *doc, BuildingFloor *floor, const QPoint &pos) :
    ChangeRoomAtPosition(doc, floor, pos, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Erase Room"));
}

/////

ChangeEWall::ChangeEWall(BuildingDocument *doc, BuildingTileEntry *tile, bool mergeable) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Exterior Wall")),
    mDocument(doc),
    mTile(tile),
    mMergeable(mergeable)
{
}

bool ChangeEWall::mergeWith(const QUndoCommand *other)
{
    const ChangeEWall *o = static_cast<const ChangeEWall*>(other);
    if (!o->mMergeable)
        return false;
    return true;
}

void ChangeEWall::swap()
{
    mTile = mDocument->changeEWall(mTile);
}

/////

ChangeRoomTile::ChangeRoomTile(BuildingDocument *doc, Room *room, int tileEnum,
                               BuildingTileEntry *tile, bool mergeable) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room's Tile")),
    mDocument(doc),
    mRoom(room),
    mEnum(tileEnum),
    mTile(tile),
    mMergeable(mergeable)
{
}

bool ChangeRoomTile::mergeWith(const QUndoCommand *other)
{
    const ChangeRoomTile *o = static_cast<const ChangeRoomTile*>(other);
    if (!(o->mMergeable && (o->mEnum == mEnum)))
        return false;
    return true;
}

void ChangeRoomTile::swap()
{
    mTile = mDocument->changeRoomTile(mRoom, mEnum, mTile);
}

/////

AddRemoveObject::AddRemoveObject(BuildingDocument *doc, BuildingFloor *floor,
                                 int index, BuildingObject *object) :
    QUndoCommand(),
    mDocument(doc),
    mFloor(floor),
    mIndex(index),
    mObject(object)
{
}

AddRemoveObject::~AddRemoveObject()
{
    delete mObject;
}

void AddRemoveObject::add()
{
    mDocument->insertObject(mFloor, mIndex, mObject);
    mObject = 0;
}

void AddRemoveObject::remove()
{
    mObject = mDocument->removeObject(mFloor, mIndex);
}

AddObject::AddObject(BuildingDocument *doc, BuildingFloor *floor, int index,
                     BuildingObject *object) :
    AddRemoveObject(doc, floor, index, object)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Object"));
}

RemoveObject::RemoveObject(BuildingDocument *doc, BuildingFloor *floor,
                           int index) :
    AddRemoveObject(doc, floor, index, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Object"));
}

/////

MoveObject::MoveObject(BuildingDocument *doc, BuildingObject *object, const QPoint &pos) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Move Object")),
    mDocument(doc),
    mObject(object),
    mPos(pos)
{
}

void MoveObject::swap()
{
    mPos = mDocument->moveObject(mObject, mPos);
}

/////

ChangeObjectTile::ChangeObjectTile(BuildingDocument *doc, BuildingObject *object,
                               BuildingTileEntry *tile, bool mergeable, int alternate) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Object Tile")),
    mDocument(doc),
    mObject(object),
    mTile(tile),
    mAlternate(alternate),
    mMergeable(mergeable)
{
}

bool ChangeObjectTile::mergeWith(const QUndoCommand *other)
{
    const ChangeObjectTile *o = static_cast<const ChangeObjectTile*>(other);
    if (!(o->mMergeable
          && (o->mObject == mObject)
          && (o->mAlternate == mAlternate)))
        return false;
    return true;
}

void ChangeObjectTile::swap()
{
    mTile = mDocument->changeObjectTile(mObject, mTile, mAlternate);
}

/////

AddRemoveRoom::AddRemoveRoom(BuildingDocument *doc, int index, Room *room) :
    QUndoCommand(),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

AddRemoveRoom::~AddRemoveRoom()
{
    delete mRoom;
}

void AddRemoveRoom::add()
{
    mDocument->insertRoom(mIndex, mRoom);
    mRoom = 0;
}

void AddRemoveRoom::remove()
{
    mRoom = mDocument->removeRoom(mIndex);
}

AddRoom::AddRoom(BuildingDocument *doc, int index, Room *room) :
    AddRemoveRoom(doc, index, room)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Room"));
}

RemoveRoom::RemoveRoom(BuildingDocument *doc, int index) :
    AddRemoveRoom(doc, index, 0)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Room"));
}

/////

ReorderRoom::ReorderRoom(BuildingDocument *doc, int index, Room *room) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Rooms")),
    mDocument(doc),
    mIndex(index),
    mRoom(room)
{
}

void ReorderRoom::swap()
{
    mIndex = mDocument->reorderRoom(mIndex, mRoom);
}

/////

ChangeRoom::ChangeRoom(BuildingDocument *doc, Room *room, const Room *data) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Room")),
    mDocument(doc),
    mRoom(room),
    mData(new Room(data))
{
}

ChangeRoom::~ChangeRoom()
{
    delete mData;
}

void ChangeRoom::swap()
{
    mData = mDocument->changeRoom(mRoom, mData);
}

/////

SwapFloorGrid::SwapFloorGrid(BuildingDocument *doc, BuildingFloor *floor,
                             const QVector<QVector<Room *> > &grid, char *undoText) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", undoText)),
    mDocument(doc),
    mFloor(floor),
    mGrid(grid)
{
}

void SwapFloorGrid::swap()
{
    mGrid = mDocument->swapFloorGrid(mFloor, mGrid);
}

/////

SwapFloorGrime::SwapFloorGrime(BuildingDocument *doc, BuildingFloor *floor,
                               const QMap<QString, FloorTileGrid *> &grid,
                               char *undoText, bool emitSignal) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", undoText)),
    mDocument(doc),
    mFloor(floor),
    mGrid(grid),
    mEmitSignal(emitSignal)
{
}

SwapFloorGrime::~SwapFloorGrime()
{
    qDeleteAll(mGrid.values());
}

void SwapFloorGrime::swap()
{
    mGrid = mDocument->swapFloorTiles(mFloor, mGrid, mEmitSignal);
}

/////

PaintFloorTiles::PaintFloorTiles(BuildingDocument *doc, BuildingFloor *floor,
                                 const QString &layerName,
                                 const QPoint &pos,
                                 FloorTileGrid *tiles,
                                 char *undoText) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", undoText)),
    mDocument(doc),
    mFloor(floor),
    mLayerName(layerName),
    mRegion(tiles->bounds().translated(pos)),
    mTiles(tiles)
{
}

PaintFloorTiles::PaintFloorTiles(BuildingDocument *doc, BuildingFloor *floor,
                                 const QString &layerName, const QRegion &rgn,
                                 const QPoint &pos,
                                 FloorTileGrid *tiles, char *undoText) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", undoText)),
    mDocument(doc),
    mFloor(floor),
    mLayerName(layerName),
    mRegion(rgn),
    mPos(pos),
    mTiles(tiles)
{
}

PaintFloorTiles::~PaintFloorTiles()
{
    delete mTiles;
}

void PaintFloorTiles::swap()
{
    FloorTileGrid *old = mTiles;
    mTiles = mDocument->swapFloorTiles(mFloor, mLayerName, mRegion, mPos, mTiles);
    delete old;
}

/////

ResizeBuilding::ResizeBuilding(BuildingDocument *doc, const QSize &newSize) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Building")),
    mDocument(doc),
    mSize(newSize)
{
}

void ResizeBuilding::swap()
{
    mSize = mDocument->resizeBuilding(mSize);
}

/////

ChangeUsedTiles::ChangeUsedTiles(BuildingDocument *doc, const QList<BuildingTileEntry *> &tiles) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Used Tiles")),
    mDocument(doc),
    mTiles(tiles)
{
}

void ChangeUsedTiles::swap()
{
    mTiles = mDocument->changeUsedTiles(mTiles);
}

/////

ChangeUsedFurniture::ChangeUsedFurniture(BuildingDocument *doc, const QList<FurnitureTiles *> &tiles) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Used Furniture")),
    mDocument(doc),
    mTiles(tiles)
{
}

void ChangeUsedFurniture::swap()
{
    mTiles = mDocument->changeUsedFurniture(mTiles);
}

/////

ResizeFloor::ResizeFloor(BuildingDocument *doc, BuildingFloor *floor,
                             const QSize &newSize) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Floor")),
    mDocument(doc),
    mFloor(floor),
    mSize(newSize)
{
    mGrid = floor->resizeGrid(newSize);
    mGrime = floor->resizeGrime(newSize + QSize(1, 1));
}

ResizeFloor::~ResizeFloor()
{
}

void ResizeFloor::swap()
{
    mGrid = mDocument->resizeFloor(mFloor, mGrid, mGrime);
}

/////

InsertFloor::InsertFloor(BuildingDocument *doc, int index, BuildingFloor *floor) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Insert Floor")),
    mDocument(doc),
    mIndex(index),
    mFloor(floor)
{
}

InsertFloor::~InsertFloor()
{
    delete mFloor;
}

void InsertFloor::add()
{
    mDocument->insertFloor(mIndex, mFloor);
    mFloor = 0;
}

void InsertFloor::remove()
{
    mFloor = mDocument->removeFloor(mIndex);
}

/////

RemoveFloor::RemoveFloor(BuildingDocument *doc, int index) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Remove Floor")),
    mDocument(doc),
    mIndex(index),
    mFloor(0)
{
}

RemoveFloor::~RemoveFloor()
{
    delete mFloor;
}

void RemoveFloor::add()
{
    mDocument->insertFloor(mIndex, mFloor);
    mFloor = 0;
}

void RemoveFloor::remove()
{
    mFloor = mDocument->removeFloor(mIndex);
}

/////

ReorderFloor::ReorderFloor(BuildingDocument *doc, int oldIndex, int newIndex) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Reorder Floor")),
    mDocument(doc),
    mOldIndex(oldIndex),
    mNewIndex(newIndex),
    mFloor(0)
{
}

void ReorderFloor::swap()
{
    mDocument->reorderFloor(mOldIndex, mNewIndex);
    qSwap(mOldIndex, mNewIndex);
}

/////

EmitResizeBuilding::EmitResizeBuilding(BuildingDocument *doc, bool before) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Building")),
    mDocument(doc),
    mBefore(before)
{
}

void EmitResizeBuilding::undo()
{
    if (mBefore)
        mDocument->emitBuildingResized();
}

void EmitResizeBuilding::redo()
{
    if (!mBefore)
        mDocument->emitBuildingResized();
}

/////

EmitRotateBuilding::EmitRotateBuilding(BuildingDocument *doc, bool before) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Rotate Building")),
    mDocument(doc),
    mBefore(before)
{
}

void EmitRotateBuilding::undo()
{
    if (mBefore)
        mDocument->emitBuildingRotated();
}

void EmitRotateBuilding::redo()
{
    if (!mBefore)
        mDocument->emitBuildingRotated();
}

/////

RotateBuilding::RotateBuilding(BuildingDocument *doc, bool right) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Rotate Building")),
    mDocument(doc),
    mRight(right)
{
}

void RotateBuilding::swap()
{
    mDocument->rotateBuilding(mRight);
    mRight = !mRight;
}

/////

FlipBuilding::FlipBuilding(BuildingDocument *doc, bool horizontal) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Flip Building")),
    mDocument(doc),
    mHorizontal(horizontal)
{
}

void FlipBuilding::swap()
{
    mDocument->flipBuilding(mHorizontal);
}

/////

ChangeFurnitureObjectTile::ChangeFurnitureObjectTile(BuildingDocument *doc,
                                         FurnitureObject *object,
                                         FurnitureTile *ftile) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Furniture Tile")),
    mDocument(doc),
    mObject(object),
    mTile(ftile)
{
}

void ChangeFurnitureObjectTile::swap()
{
    mTile = mDocument->changeFurnitureTile(mObject, mTile);
}

/////

ResizeRoof::ResizeRoof(BuildingDocument *doc, RoofObject *roof, int width, int height) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Roof")),
    mDocument(doc),
    mObject(roof),
    mWidth(width),
    mHeight(height)
{
}

void ResizeRoof::swap()
{
    mDocument->resizeRoof(mObject, mWidth, mHeight);
}

/////

HandleRoof::HandleRoof(BuildingDocument *doc, RoofObject *roof, HandleRoof::Handle handle) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Edit Roof")),
    mDocument(doc),
    mObject(roof),
    mHandle(handle)
{
}

void HandleRoof::swap()
{
    switch (mHandle) {
    case ToggleCappedW:
        mObject->toggleCappedW();
        break;
    case ToggleCappedN:
        mObject->toggleCappedN();
        break;
    case ToggleCappedE:
        mObject->toggleCappedE();
        break;
    case ToggleCappedS:
        mObject->toggleCappedS();
        break;
    case IncrDepth:
        mObject->depthUp();
        mHandle = DecrDepth;
        break;
    case DecrDepth:
        mObject->depthDown();
        mHandle = IncrDepth;
        break;
    }
    mDocument->emitObjectChanged(mObject);
}

/////

ResizeWall::ResizeWall(BuildingDocument *doc, WallObject *wall, int length) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Resize Wall")),
    mDocument(doc),
    mObject(wall),
    mLength(length)
{
}

void ResizeWall::swap()
{
    mLength = mDocument->resizeWall(mObject, mLength);
}

/////

ChangeTileSelection::ChangeTileSelection(BuildingDocument *doc, const QRegion &selection) :
    QUndoCommand(QCoreApplication::translate("Undo Commands", "Change Tile Selection")),
    mDocument(doc),
    mSelection(selection)
{
}

void ChangeTileSelection::swap()
{
    mSelection = mDocument->setTileSelection(mSelection);
}

/////
