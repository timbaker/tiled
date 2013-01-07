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
#include <QRect>
#include <QRegion>
#include <QSet>

class QUndoStack;

namespace BuildingEditor {

class BuildingObject;
class Building;
class BuildingFloor;
class BuildingTileEntry;
class Door;
class FurnitureObject;
class FurnitureTile;
class FurnitureTiles;
class RoofObject;
class Room;
class FloorTileGrid;
class WallObject;
class Window;

class BuildingDocument : public QObject
{
    Q_OBJECT
public:
    explicit BuildingDocument(Building *building, const QString &fileName);

    Building *building() const
    { return mBuilding; }

    QString fileName() const
    { return mFileName; }

    static BuildingDocument *read(const QString &fileName, QString &error);
    bool write(const QString &fileName, QString &error);

    void setCurrentFloor(BuildingFloor *floor);

    BuildingFloor *currentFloor() const
    { return mCurrentFloor; }

    int currentLevel() const;

    bool currentFloorIsTop();
    bool currentFloorIsBottom();

    void setCurrentLayer(const QString &layerName);

    QString currentLayer() const
    { return mCurrentLayerName; }

    void setLayerOpacity(BuildingFloor *floor, const QString &layerName,
                         qreal opacity);
    void setLayerVisibility(BuildingFloor *floor, const QString &layerName,
                            bool visible);

    QUndoStack *undoStack() const
    { return mUndoStack; }

    bool isModified() const;

    void setSelectedObjects(const QSet<BuildingObject*> &selection);

    const QSet<BuildingObject*> &selectedObjects() const
    { return mSelectedObjects; }

    const QRegion &tileSelection() const
    { return mTileSelection; }

    void emitBuildingResized()
    { emit buildingResized(); }

    void emitBuildingRotated()
    { emit buildingRotated(); }

    void emitObjectChanged(BuildingObject *object)
    { emit objectChanged(object); }


    // +UNDO/REDO
    Room *changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room);
    BuildingEditor::BuildingTileEntry *changeEWall(BuildingEditor::BuildingTileEntry *tile);
    BuildingTileEntry *changeRoomTile(Room *room, int tileEnum, BuildingTileEntry *tile);

    void insertFloor(int index, BuildingFloor *floor);
    BuildingFloor *removeFloor(int index);
    void reorderFloor(int oldIndex, int newIndex);

    void insertObject(BuildingFloor *floor, int index, BuildingObject *object);
    BuildingObject *removeObject(BuildingFloor *floor, int index);
    QPoint moveObject(BuildingObject *object, const QPoint &pos);
    BuildingTileEntry *changeObjectTile(BuildingObject *object,
                                        BuildingTileEntry *tile, int alternate);

    void insertRoom(int index, Room *room);
    Room *removeRoom(int index);
    int reorderRoom(int index, Room *room);
    Room *changeRoom(Room *room, const Room *data);

    QVector<QVector<Room *> > swapFloorGrid(BuildingFloor *floor,
                                            const QVector<QVector<Room*> > &grid);

    QMap<QString,FloorTileGrid*> swapFloorTiles(BuildingFloor *floor,
                                                const QMap<QString,FloorTileGrid*> &grid,
                                                bool emitSignal);

    FloorTileGrid *swapFloorTiles(BuildingFloor *floor,
                                  const QString &layerName,
                                  const QRegion &rgn,
                                  const FloorTileGrid *tiles);

    QSize resizeBuilding(const QSize &newSize);
    QVector<QVector<Room *> > resizeFloor(BuildingFloor *floor,
                                          const QVector<QVector<Room*> > &grid,
                                          QMap<QString,FloorTileGrid*> &grime);
    void rotateBuilding(bool right);
    void flipBuilding(bool horizontal);

    FurnitureTile *changeFurnitureTile(FurnitureObject *object, FurnitureTile *ftile);

    void resizeRoof(RoofObject *roof, int &width, int &height);

    int resizeWall(WallObject *wall, int length);

    QList<BuildingTileEntry*> changeUsedTiles(const QList<BuildingTileEntry*> &tiles);
    QList<FurnitureTiles *> changeUsedFurniture(const QList<FurnitureTiles *> &tiles);

    QRegion setTileSelection(const QRegion &selection);
    // -UNDO/REDO
    
signals:
    void currentFloorChanged();

    void currentLayerChanged();

    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    void roomDefinitionChanged();

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void floorTilesChanged(BuildingFloor *floor);
    void floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                           const QRect &bounds);

    void layerOpacityChanged(BuildingFloor *floor, const QString &layerName);
    void layerVisibilityChanged(BuildingFloor *floor, const QString &layerName);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);
    void objectChanged(BuildingObject *object);

    void roomAdded(Room *room);
    void roomAboutToBeRemoved(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void buildingResized();
    void buildingRotated();

    void selectedObjectsChanged();

    void tileSelectionChanged(const QRegion &old);

    void cleanChanged();

    void usedTilesChanged();
    void usedFurnitureChanged();

private:
    void checkUsedTile(BuildingTileEntry *entry);
    void checkUsedFurniture(FurnitureTiles *ftiles);

private slots:
    void entryTileChanged(BuildingTileEntry *entry);
    void furnitureTileChanged(FurnitureTile *ftile);
    void furnitureLayerChanged(FurnitureTiles *ftiles);
    
private:
    Building *mBuilding;
    QString mFileName;
    QUndoStack *mUndoStack;
    bool mTileChanges;
    BuildingFloor *mCurrentFloor;
    QString mCurrentLayerName;
    QSet<BuildingObject*> mSelectedObjects;
    QRegion mTileSelection;
};

} // namespace BuildingEditor

#endif // BUILDINGDOCUMENT_H
