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

#include <QPoint>
#include <QUndoCommand>
#include <QSize>
#include <QVector>

namespace BuildingEditor {

class BuildingObject;
class BuildingDocument;
class BuildingFloor;
class BuildingTile;
class Door;
class FurnitureObject;
class FurnitureTile;
class RoofCornerObject;
class RoofObject;
class Room;
class Window;

enum {
    UndoCmd_PaintRoom = 1000,
    UndoCmd_EraseRoom = 1001
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
    ChangeEWall(BuildingDocument *doc, BuildingTile *tile);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingTile *mTile;
};

class ChangeWallForRoom : public QUndoCommand
{
public:
    ChangeWallForRoom(BuildingDocument *doc, Room *room, BuildingTile *tile);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    Room *mRoom;
    BuildingTile *mTile;
};

class ChangeFloorForRoom : public QUndoCommand
{
public:
    ChangeFloorForRoom(BuildingDocument *doc, Room *room, BuildingTile *tile);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    Room *mRoom;
    BuildingTile *mTile;
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

class ChangeDoorTile : public QUndoCommand
{
public:
    ChangeDoorTile(BuildingDocument *doc, Door *door, BuildingTile *tile,
                   bool isFrame = false);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    Door *mDoor;
    BuildingTile *mTile;
    bool mIsFrame;
};

class ChangeObjectTile : public QUndoCommand
{
public:
    ChangeObjectTile(BuildingDocument *doc, BuildingObject *object,
                     BuildingTile *tile, int alternate = 0);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingObject *mObject;
    BuildingTile *mTile;
    int mAlternate;
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
                  const QVector<QVector<Room*> > &grid);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QVector<QVector<Room*> > mGrid;
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

class ResizeFloor : public QUndoCommand
{
public:
    ResizeFloor(BuildingDocument *doc, BuildingFloor *floor, const QSize &newSize);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    QSize mSize;
    QVector<QVector<Room*> > mGrid;
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

class ResizeBuildingBefore : public QUndoCommand
{
public:
    ResizeBuildingBefore(BuildingDocument *doc);

    void undo();
    void redo() {}

    BuildingDocument *mDocument;
};

class ResizeBuildingAfter : public QUndoCommand
{
public:
    ResizeBuildingAfter(BuildingDocument *doc);

    void undo() {}
    void redo();

    BuildingDocument *mDocument;
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
    ResizeRoof(BuildingDocument *doc, RoofObject *roof, int length,
               int thickness);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    RoofObject *mObject;
    int mLength;
    int mThickness;
};

class HandleRoof : public QUndoCommand
{
public:
    enum Handle {
        ToggleWidth1,
        ToggleWidth2,
        ToggleCapped1,
        ToggleCapped2,
        IncrDepth,
        DecrDepth,
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

class ResizeRoofCorner : public QUndoCommand
{
public:
    ResizeRoofCorner(BuildingDocument *doc, RoofCornerObject *corner, int width,
                     int height);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    RoofCornerObject *mObject;
    int mWidth;
    int mHeight;
};

class HandleRoofCorner : public QUndoCommand
{
public:
    enum Handle {
        ToggleOrientationLeft,
        ToggleOrientationRight,
        IncrDepth,
        DecrDepth,
    };

    HandleRoofCorner(BuildingDocument *doc, RoofCornerObject *corner, Handle handle);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    BuildingDocument *mDocument;
    RoofCornerObject *mObject;
    Handle mHandle;
};

} // namespace BuildingEditor

#endif // BUILDINGUNDOREDO_H
