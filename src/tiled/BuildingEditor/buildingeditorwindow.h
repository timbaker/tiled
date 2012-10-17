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

#ifndef BUILDINGEDITORWINDOW_H
#define BUILDINGEDITORWINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QVector>

class QComboBox;
class QUndoGroup;

namespace Ui {
class BuildingEditorWindow;
}

namespace BuildingEditor {

class BaseTool;
class BuildingDocument;
class FloorEditor;
class Room;

class WallType
{
public:
    QString Tilesheet;
    int FirstIndex;

    WallType(QString tile, int ind) :
        Tilesheet(tile),
        FirstIndex(ind)
    {
    }

    QString ToString()
    {
        return Tilesheet + QLatin1String("_") + QString::number(FirstIndex);
    }
};

class WallTypes
{
public:

    static WallTypes *instance;
    QList<WallType*> ETypes;
    QList<WallType*> ITypes;
    QMap<QString,WallType*> ITypesByName;

    void Add(QString name, int first);
    void AddExt(QString name, int first);

    QStringList AddExteriorWallsToList();

    WallType *getEWallFromName(QString exteriorWall);
    WallType *getOrAdd(QString exteriorWall);
};

class FloorType
{
public:
    QString Tilesheet;
    int Index;

    FloorType(QString tile, int ind) :
        Tilesheet(tile),
        Index(ind)
    {

    }
};

class FloorTypes
{
public:
    static FloorTypes *instance;

    QList<FloorType*> Types;
    QMap<QString,FloorType*> TypesByName;

    void Add(QString name, int first);
};

class BaseMapObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W
    };

    virtual QRect bounds() const
    { return QRect(X, Y, 1, 1); }

    Direction dir;
    int X, Y;
 };

class Door : public BaseMapObject
{
public:
};

class Stairs : public BaseMapObject
{
public:
    QRect bounds() const;

    QString getStairsTexture(int x, int y);
};

class Window : public BaseMapObject
{
public:
};

class Layout
{
public:
    Layout(int w, int h);

    Room *roomAt(int x, int y);
    Room *roomAt(const QPoint &pos)
    { return roomAt(pos.x(), pos.y()); }

    QVector<QVector<int> > grid;
    int w, h;
    WallType *exteriorWall;
    QVector<WallType*> interiorWalls;
    QVector<FloorType*> floors;
//    QList<QRgb> colList;
};

class Room
{
public:
    QString Name;
    QRgb Color;
    QString internalName;
    QString Floor;
    QString Wall;
};

class BuildingDefinition
{
public:
    QString Name;
    QString Wall;

    QList<Room*> RoomList;

    static QList<BuildingDefinition*> Definitions;
    static QMap<QString,BuildingDefinition*> DefinitionMap;
};

class RoomDefinitionManager
{
public:
    static RoomDefinitionManager *instance;
#if 1
    BuildingDefinition *mBuildingDefinition;
    QMap<QRgb,Room*> ColorToRoom;
    QMap<Room*,QRgb> RoomToColor;
#else
    QString BuildingName;
    QMap<QRgb,QString> ColorToRoomName;
    QMap<QString,QRgb> RoomNameToColor;
    QMap<QString,QString> WallForRoom;
    QMap<QString,QString> FloorForRoom;
    QStringList Rooms;
#endif
    QString ExteriorWall;
    QString FrameStyleTilesheet;
    QString DoorStyleTilesheet;
    int DoorFrameStyleIDW;
    int DoorFrameStyleIDN;
    int DoorStyleIDW;
    int DoorStyleIDN;
    QString TopStairNorth;
    QString MidStairNorth;
    QString BotStairNorth;
    QString TopStairWest;
    QString MidStairWest;
    QString BotStairWest;

    void Add(QString roomName, QRgb col, QString wall, QString floor);

    void Init(BuildingDefinition *definition);

    void Init();

    QStringList FillCombo();
#if 0
    QRgb Get(QString roomName);
#endif
    int GetIndex(QRgb col);
    int GetIndex(Room *room);
    Room *getRoom(int index);
    int getRoomCount();

    WallType *getWallForRoom(int i);
    FloorType *getFloorForRoom(int i);
    int getFromColor(QRgb pixel);
    WallType *getWallForRoom(QString room);
    FloorType *getFloorForRoom(QString room);
    void setWallForRoom(QString room, QString tile);
    void setFloorForRoom(QString room, QString tile);
};

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    static BuildingEditorWindow *instance;

    bool Startup();

    bool LoadBuildingTemplates();

    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

private slots:
    void roomIndexChanged(int index);

private:
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    FloorEditor *roomEditor;
    QComboBox *room;
    QUndoGroup *mUndoGroup;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
