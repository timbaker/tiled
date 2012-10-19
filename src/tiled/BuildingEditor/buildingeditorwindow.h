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
#include <QSettings>
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
class Door;
class FloorEditor;
class Room;
class Window;
class Stairs;

class BuildingTile
{
public:
    BuildingTile(const QString &tilesetName, int index) :
        mTilesetName(tilesetName),
        mIndex(index)
    {}

    QString mTilesetName;
    int mIndex;

    QVector<BuildingTile*> mAlternates;
};

class BuildingTiles
{
public:
    static BuildingTiles *instance();

    class Category
    {
    public:
        Category(const QString &name, const QString &label) :
            mName(name),
            mLabel(label)
        {}

        BuildingTile *add(const QString &tileName)
        {
            QString tilesetName;
            int tileIndex;
            parseTileName(tileName, tilesetName, tileIndex);
            BuildingTile *tile = new BuildingTile(tilesetName, tileIndex);
            mTiles += tile;
            mTileByName[tileName] = tile;
            return tile;
        }

        BuildingTile *get(const QString &tileName)
        {
            if (!mTileByName.contains(tileName))
                add(tileName);
            return mTileByName[tileName];
        }

        QString name() const
        { return mName; }

        QString label() const
        { return mLabel; }

        const QList<BuildingTile*> &tiles() const
        { return mTiles; }

    private:
        QString mName;
        QString mLabel;
        QList<BuildingTile*> mTiles;
        QMap<QString,BuildingTile*> mTileByName;
    };

    Category *addCategory(const QString &categoryName, const QString &label)
    {
        Category *category = this->category(categoryName);
        if (!category) {
            category = new Category(categoryName, label);
            mCategories += category;
            mCategoryByName[categoryName]= category;
        }
        return category;
    }

    BuildingTile *add(const QString &categoryName, const QString &tileName)
    {
        Category *category = this->category(categoryName);
#if 0
        if (!category) {
            category = new Category(categoryName);
            mCategories += category;
            mCategoryByName[categoryName]= category;
        }
#endif
        return category->add(tileName);
    }

    void add(const QString &categoryName, const QStringList &tileNames)
    {
        QVector<BuildingTile*> tiles;
        foreach (QString tileName, tileNames)
            tiles += add(categoryName, tileName);
        foreach (BuildingTile *tile, tiles)
            tile->mAlternates = tiles;
    }

    BuildingTile *get(const QString &categoryName, const QString &tileName);

    const QList<Category*> &categories() const
    { return mCategories; }

    Category *category(const QString &name)
    {
        if (mCategoryByName.contains(name))
            return mCategoryByName[name];
        return 0;
    }

    static bool parseTileName(const QString &tileName, QString &tilesetName, int &index);
    static QString adjustTileNameIndex(const QString &tileName, int offset);

    BuildingTile *tileForDoor(Door *door, const QString &tileName,
                              bool isFrame = false);

    BuildingTile *tileForWindow(Window *window, const QString &tileName);

    BuildingTile *tileForStairs(Stairs *stairs, const QString &tileName);

private:
    static BuildingTiles *mInstance;
    QList<Category*> mCategories;
    QMap<QString,Category*> mCategoryByName;
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

    BuildingTile *mTile;

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
        BaseMapObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    BuildingTile *mFrameTile;
};

class Stairs : public BaseMapObject
{
public:
    Stairs(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir)
    {
    }

    QRect bounds() const;

    int getStairsOffset(int x, int y);
};

class Window : public BaseMapObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir)
    {

    }
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

    BuildingDefinition *mBuildingDefinition;
    QMap<QRgb,Room*> ColorToRoom;
    QMap<Room*,QRgb> RoomToColor;

    QString ExteriorWall;
    QString mDoorTile;
    QString mDoorFrameTile;
    QString mWindowTile;
    QString mStairsTile;

    void Add(QString roomName, QRgb col, QString wall, QString floor);

    void Init(BuildingDefinition *definition);

    void Init();

    QStringList FillCombo();

    int GetIndex(QRgb col);
    int GetIndex(Room *room);
    Room *getRoom(int index);
    int getRoomCount();

    BuildingTile *getWallForRoom(int i);
    BuildingTile *getFloorForRoom(int i);
    int getFromColor(QRgb pixel);

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

    void closeEvent(QCloseEvent *event);

    bool Startup();

    bool LoadBuildingTemplates();
    bool LoadBuildingTiles();
    bool LoadMapBaseXMLLots();

    void setCurrentRoom(Room *room) const; // TODO: move to BuildingDocument
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Tiled::Tile *tileFor(const QString &tileName);
    Tiled::Tile *tileFor(BuildingTile *tile);

    QString nameForTile(Tiled::Tile *tile);

    void readSettings();
    void writeSettings();

private slots:
    void roomIndexChanged(int index);

    void currentEWallChanged(const QItemSelection &selected);
    void currentIWallChanged(const QItemSelection &selected);
    void currentFloorChanged(const QItemSelection &selected);
    void currentDoorChanged(const QItemSelection &selected);
    void currentDoorFrameChanged(const QItemSelection &selected);
    void currentWindowChanged(const QItemSelection &selected);
    void currentStairsChanged(const QItemSelection &selected);

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
    QSettings mSettings;
    QString mError;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
