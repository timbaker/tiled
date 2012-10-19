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

#ifndef BUILDINGDOCUMENT_H
#define BUILDINGDOCUMENT_H

#include <QObject>
#include <QPoint>
#include <QSet>

class QUndoStack;

namespace BuildingEditor {

class BaseMapObject;
class Building;
class BuildingFloor;
class BuildingTile;
class Door;
class Layout;
class Room;

class BuildingDocument : public QObject
{
    Q_OBJECT
public:
    explicit BuildingDocument(Building *building, const QString &fileName);

    Building *building() const
    { return mBuilding; }

    void setCurrentFloor(BuildingFloor *floor);

    BuildingFloor *currentFloor() const
    { return mCurrentFloor; }

    QUndoStack *undoStack() const
    { return mUndoStack; }

    void setSelectedObjects(const QSet<BaseMapObject*> &selection);

    const QSet<BaseMapObject*> &selectedObjects() const
    { return mSelectedObjects; }

    // +UNDO/REDO
    Room *changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room);
    QString changeEWall(const QString &tileName);
    QString changeWallForRoom(Room *room, const QString &tileName);
    QString changeFloorForRoom(Room *room, const QString &tileName);
    void insertFloor(int index, BuildingFloor *floor);
    void insertObject(BuildingFloor *floor, int index, BaseMapObject *object);
    BaseMapObject *removeObject(BuildingFloor *floor, int index);
    QPoint moveObject(BaseMapObject *object, const QPoint &pos);
    BuildingTile *changeDoorTile(Door *door, BuildingTile *tile);
    // -UNDO/REDO
    
signals:
    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    void roomDefinitionChanged();
    void floorAdded(BuildingFloor *floor);
    void objectAdded(BaseMapObject *object);
    void objectAboutToBeRemoved(BaseMapObject *object);
    void objectRemoved(BuildingFloor *floor, int index);
    void objectMoved(BaseMapObject *object);
    void doorTileChanged(Door *door);

    void selectedObjectsChanged();

public slots:
    
private:
    Building *mBuilding;
    QString mFilePath;
    QUndoStack *mUndoStack;
    BuildingFloor *mCurrentFloor;
    QSet<BaseMapObject*> mSelectedObjects;
};

} // namespace BuildingEditor

#endif // BUILDINGDOCUMENT_H
