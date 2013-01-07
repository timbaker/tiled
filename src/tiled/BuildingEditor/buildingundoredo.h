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

#ifndef BUILDINGUNDOREDO_H
#define BUILDINGUNDOREDO_H

#include <QMap>
#include <QRectF>
#include <QRegion>
#include <QUndoCommand>
#include <QVector>

namespace BuildingEditor {

class BuildingObject;
class BuildingDocument;
class BuildingFloor;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FloorTileGrid;
class FurnitureObject;
class FurnitureTile;
class FurnitureTiles;
class RoofObject;
class Room;
class WallObject;
class Window;

enum {
    UndoCmd_PaintRoom = 1000,
    UndoCmd_EraseRoom = 1001,
    UndoCmd_ChangeEWall = 1002,
    UndoCmd_ChangeWallForRoom = 1003,
    UndoCmd_ChangeFloorForRoom = 1004,
    UndoCmd_ChangeObjectTile = 1005
};

class ChangeRoomAtPosition : public QUndoCommand
{
public:
    ChangeRoomAtPosition(BuildingDocument *doc, BuildingFloor *floor,
                         const QPoint &pos, Room *room);

    void undo() { swap(); }
    void redo() { swap(); }

    bool mergeWith(const QUndoCommand *other);

    void setMergeable(bool mergeable)
    { mMergeable = mergeable; }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    class Changed {
    public:
        QPoint mPosition;
        Room *mRoom;
    };
    QVector<Changed> mChanged;
    bool mMergeable;
};

class PaintRoom : public ChangeRoomAtPosition
{
public:
    PaintRoom(BuildingDocument *doc, BuildingFloor *floor,
              const QPoint &pos, Room *room);

    int id() const { return UndoCmd_PaintRoom; }
};

class EraseRoom : public ChangeRoomAtPosition
{
public:
    EraseRoom(BuildingDocument *doc, BuildingFloor *floor,
              const QPoint &pos);

    int id() const { return UndoCmd_EraseRoom; }
};

class ChangeEWall : public QUndoCommand
{
public:
    ChangeEWall(BuildingDocument *doc, BuildingEditor::BuildingTileEntry *tile,
                bool mergeable);

    int id() const { return UndoCmd_ChangeEWall; }
    bool mergeWith(const QUndoCommand *other);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingTileEntry *mTile;
    bool mMergeable;
};

class ChangeRoomTile : public QUndoCommand
{
public:
    ChangeRoomTile(BuildingDocument *doc, Room *room, int tileEnum,
                   BuildingTileEntry *tile, bool mergeable);

    int id() const { return UndoCmd_ChangeWallForRoom; }
    bool mergeWith(const QUndoCommand *other);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    Room *mRoom;
    int mEnum;
    BuildingTileEntry *mTile;
    bool mMergeable;
};

class AddRemoveObject : public QUndoCommand
{
public:
    AddRemoveObject(BuildingDocument *doc, BuildingFloor *floor, int index,
                    BuildingObject *object);
    ~AddRemoveObject();

protected:
    void add();
    void remove();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    int mIndex;
    BuildingObject *mObject;
};

class AddObject : public AddRemoveObject
{
public:
    AddObject(BuildingDocument *doc, BuildingFloor *floor, int index, BuildingObject *object);

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveObject : public AddRemoveObject
{
public:
    RemoveObject(BuildingDocument *doc, BuildingFloor *floor, int index);

    void undo() { add(); }
    void redo() { remove(); }
};

class MoveObject : public QUndoCommand
{
public:
    MoveObject(BuildingDocument *doc, BuildingObject *object, const QPoint &pos);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingObject *mObject;
    QPoint mPos;
};

class ChangeObjectTile : public QUndoCommand
{
public:
    ChangeObjectTile(BuildingDocument *doc, BuildingObject *object,
                     BuildingTileEntry *tile, bool mergeable, int alternate);

    int id() const { return UndoCmd_ChangeObjectTile; }
    bool mergeWith(const QUndoCommand *other);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingObject *mObject;
    BuildingTileEntry *mTile;
    int mAlternate;
    bool mMergeable;
};

class AddRemoveRoom : public QUndoCommand
{
public:
    AddRemoveRoom(BuildingDocument *doc, int index, Room *room);
    ~AddRemoveRoom();

protected:
    void add();
    void remove();

    BuildingDocument *mDocument;
    int mIndex;
    Room *mRoom;
};

class AddRoom : public AddRemoveRoom
{
public:
    AddRoom(BuildingDocument *doc, int index, Room *room);

    void undo() { remove(); }
    void redo() { add(); }
};

class RemoveRoom : public AddRemoveRoom
{
public:
    RemoveRoom(BuildingDocument *doc, int index);

    void undo() { add(); }
    void redo() { remove(); }
};

class ReorderRoom : public QUndoCommand
{
public:
    ReorderRoom(BuildingDocument *doc, int index, Room *room);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    int mIndex;
    Room *mRoom;
};

class ChangeRoom : public QUndoCommand
{
public:
    ChangeRoom(BuildingDocument *doc, Room *room, const Room *data);
    ~ChangeRoom();

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    Room *mRoom;
    Room *mData;
};

class SwapFloorGrid : public QUndoCommand
{
public:
    SwapFloorGrid(BuildingDocument *doc, BuildingFloor *floor,
                  const QVector<QVector<Room*> > &grid, char *undoText);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QVector<QVector<Room*> > mGrid;
};

class SwapFloorGrime : public QUndoCommand
{
public:
    SwapFloorGrime(BuildingDocument *doc, BuildingFloor *floor,
                   const QMap<QString,FloorTileGrid*> &grid, char *undoText,
                   bool emitSignal);

    ~SwapFloorGrime();

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QMap<QString,FloorTileGrid*> mGrid;
    bool mEmitSignal;
};

class PaintFloorTiles : public QUndoCommand
{
public:
    PaintFloorTiles(BuildingDocument *doc, BuildingFloor *floor,
                    const QString &layerName, const QPoint &pos,
                    FloorTileGrid *tiles, char *undoText);

    PaintFloorTiles(BuildingDocument *doc, BuildingFloor *floor,
                    const QString &layerName, const QRegion &rgn,
                    const QPoint &pos,
                    FloorTileGrid *tiles, char *undoText);

    ~PaintFloorTiles();

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QString mLayerName;
    QRegion mRegion;
    QPoint mPos;
    FloorTileGrid *mTiles;
};

class ResizeBuilding : public QUndoCommand
{
public:
    ResizeBuilding(BuildingDocument *doc, const QSize &newSize);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    QSize mSize;
};

class ChangeUsedTiles : public QUndoCommand
{
public:
    ChangeUsedTiles(BuildingDocument *doc, const QList<BuildingTileEntry*> &tiles);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    QList<BuildingTileEntry*> mTiles;
};

class ChangeUsedFurniture : public QUndoCommand
{
public:
    ChangeUsedFurniture(BuildingDocument *doc, const QList<FurnitureTiles*> &tiles);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    QList<FurnitureTiles*> mTiles;
};

class ResizeFloor : public QUndoCommand
{
public:
    ResizeFloor(BuildingDocument *doc, BuildingFloor *floor, const QSize &newSize);

    ~ResizeFloor();

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QSize mSize;
    QVector<QVector<Room*> > mGrid;
    QMap<QString,FloorTileGrid*> mGrime;
};

class InsertFloor : public QUndoCommand
{
public:
    InsertFloor(BuildingDocument *doc, int index, BuildingFloor *floor);
    ~InsertFloor();

    void undo() { remove(); }
    void redo() { add(); }

private:
    void add();
    void remove();

    BuildingDocument *mDocument;
    int mIndex;
    BuildingFloor *mFloor;
};

class RemoveFloor : public QUndoCommand
{
public:
    RemoveFloor(BuildingDocument *doc, int index);
    ~RemoveFloor();

    void undo() { add(); }
    void redo() { remove(); }

private:
    void add();
    void remove();

    BuildingDocument *mDocument;
    int mIndex;
    BuildingFloor *mFloor;
};

class ReorderFloor : public QUndoCommand
{
public:
    ReorderFloor(BuildingDocument *doc, int oldIndex, int newIndex);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    int mOldIndex;
    int mNewIndex;
    BuildingFloor *mFloor;
};

class EmitResizeBuilding : public QUndoCommand
{
public:
    EmitResizeBuilding(BuildingDocument *doc, bool before);

    void undo();
    void redo();

    BuildingDocument *mDocument;
    bool mBefore;
};

class EmitRotateBuilding : public QUndoCommand
{
public:
    EmitRotateBuilding(BuildingDocument *doc, bool before);

    void undo();
    void redo();

    BuildingDocument *mDocument;
    bool mBefore;
};

class RotateBuilding : public QUndoCommand
{
public:
    RotateBuilding(BuildingDocument *doc, bool right);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    bool mRight;
};

class FlipBuilding : public QUndoCommand
{
public:
    FlipBuilding(BuildingDocument *doc, bool horizontal);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    bool mHorizontal;
};

class ChangeFurnitureObjectTile : public QUndoCommand
{
public:
    ChangeFurnitureObjectTile(BuildingDocument *doc, FurnitureObject *object,
                        FurnitureTile *ftile);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    FurnitureObject *mObject;
    FurnitureTile *mTile;
};

class ResizeRoof : public QUndoCommand
{
public:
    ResizeRoof(BuildingDocument *doc, RoofObject *roof, int width,
               int height);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    RoofObject *mObject;
    int mWidth;
    int mHeight;
};

class HandleRoof : public QUndoCommand
{
public:
    enum Handle {
        ToggleCappedW,
        ToggleCappedN,
        ToggleCappedE,
        ToggleCappedS,
        IncrDepth,
        DecrDepth
    };

    HandleRoof(BuildingDocument *doc, RoofObject *roof, Handle handle);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    RoofObject *mObject;
    Handle mHandle;
};

class ResizeWall : public QUndoCommand
{
public:
    ResizeWall(BuildingDocument *doc, WallObject *wall, int length);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    WallObject *mObject;
    int mLength;
};

class ChangeTileSelection : public QUndoCommand
{
public:
    ChangeTileSelection(BuildingDocument *doc, const QRegion &selection);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    QRegion mSelection;
};


} // namespace BuildingEditor

#endif // BUILDINGUNDOREDO_H
