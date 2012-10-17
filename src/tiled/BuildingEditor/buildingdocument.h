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

class QUndoStack;

namespace BuildingEditor {

class Building;
class BuildingFloor;
class Layout;
class Room;

class BuildingDocument : public QObject
{
    Q_OBJECT
public:
    explicit BuildingDocument(Building *building, const QString &fileName);

    Building *building() const
    { return mBuilding; }

    QUndoStack *undoStack() const
    { return mUndoStack; }

    // +UNDO/REDO
    Room *changeRoomAtPosition(BuildingFloor *floor, const QPoint &pos, Room *room);
    // -UNDO/REDO
    
signals:
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    
public slots:
    
private:
    Building *mBuilding;
    QString mFilePath;
    QUndoStack *mUndoStack;
};

} // namespace BuildingEditor

#endif // BUILDINGDOCUMENT_H
