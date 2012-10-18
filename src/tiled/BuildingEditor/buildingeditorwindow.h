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

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QVector>

class QComboBox;
class QLabel;
class QUndoGroup;

namespace Ui {
class BuildingEditorWindow;
}

namespace Tiled {
class Tile;
class Tileset;
}

namespace BuildingEditor {

class BaseTool;
class BuildingDocument;
class BuildingFloor;
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

    QString ToString()
    {
        return Tilesheet + QLatin1String("_") + QString::number(Index);
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

class DoorType
{
public:
    QString Tilesheet;
    int Index;

    DoorType(QString tile, int ind) :
        Tilesheet(tile),
        Index(ind)
    {

    }

    QString ToString()
    {
        return Tilesheet + QLatin1String("_") + QString::number(Index);
    }
};

class DoorTypes
{
public:
    static DoorTypes *instance;

    QList<DoorType*> Types;
    QMap<QString,DoorType*> TypesByName;

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

    BaseMapObject(BuildingFloor *floor, int x, int y, Direction mDir);

    BuildingFloor *floor() const
    { return mFloor; }

    int index();

    virtual QRect bounds() const
    { return QRect(mX, mY, 1, 1); }

    void setPos(int x, int y)
    { mX = x, mY = y; }

    void setPos(const QPoint &pos)
    { mX = pos.x(), mY = pos.y(); }

    QPoint pos() const
    { return QPoint(mX, mY); }

    void setDir(Direction dir)
    { mDir = dir; }

    Direction dir() const
    { return mDir; }

protected:
    BuildingFloor *mFloor;
    Direction mDir;
    int mX;
    int mY;
};

class Door : public BaseMapObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir)
    {

    }

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
    void setWallForRoom(Room *room, QString tile);
    void setFloorForRoom(Room *room, QString tile);
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
    bool LoadBuildingTiles();
    bool LoadMapBaseXMLLots();

    void setCurrentRoom(Room *room) const; // TODO: move to BuildingDocument
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Tiled::Tile *tileFor(const QString &tileName);

    QString nameForTile(Tiled::Tile *tile);

private slots:
    void roomIndexChanged(int index);

    void currentEWallChanged(const QItemSelection &selected);
    void currentIWallChanged(const QItemSelection &selected);
    void currentFloorChanged(const QItemSelection &selected);

    void upLevel();
    void downLevel();

private:
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    FloorEditor *roomEditor;
    QComboBox *room;
    QLabel *mFloorLabel;
    QUndoGroup *mUndoGroup;
    QMap<QString,Tiled::Tileset*> mTilesetByName;
    QString mError;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
