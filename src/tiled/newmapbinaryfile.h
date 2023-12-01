#ifndef TMXBINARY_H
#define TMXBINARY_H

#include "BuildingEditor/buildingfloor.h"

#include <QMap>
#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>

#include <cmath>

class MapComposite;

namespace Tiled {
class Cell;
class ObjectGroup;
class Tile;
class Tileset;
}

namespace LotFile
{

template<typename T>
T clamp(T v, T min, T max)
{
    return qMin(qMax(v, min), max);
};

class Tile
{
public:
    Tile(const QString &name = QString())
    {
        this->name = name;
        this->used = false;
        this->id = -1;
        this->metaEnum = -1;
    }
    QString name;
    bool used;
    int id; // index into .lotheader's list of used tiles
    int metaEnum; // Tilesets.txt enumeration
};

class Lot
{
public:
    Lot(QString name, int x, int y, int w, int h)
    {
        this->name = name;
        this->x = x;
        this->y = y;
        this->w = w;
        this->h = h;
    }
    QString name;
    int x;
    int y;
    int w;
    int h;
};

class Entry
{
public:
    Entry(uint gid) :
        gid(gid)
    {
    }

    uint gid;
};

class Square
{
public:
    Square() :
        roomID(-1)
    {

    }
    ~Square()
    {
        qDeleteAll(Entries);
    }
    Square &operator=(const Square &other)
    {
        qDeleteAll(Entries);
        Entries = other.Entries;
        roomID = other.roomID;
        return *this;
    }

    QList<Entry*> Entries;
    int roomID;
    Tiled::Properties properties;
};

class Zone
{
public:
    Zone(const QString& name, const QString& val, int x, int y, int z, int width, int height)
    {
        this->name = name;
        this->val = val;
        this->x = x;
        this->y = y;
        this->z = z;
        this->width = width;
        this->height = height;
    }

    QString name;
    QString val;
    int x;
    int y;
    int z;
    int width;
    int height;
};

class Building;
class Room;

class RoomObject
{
public:
    int metaEnum; // Tilesets.txt enumeration
    int x; // Cell coord
    int y; // Cell coord
};

class RoomRect
{
public:
    RoomRect(const QString &name, int x, int y, int level, int w, int h)
        : x(x)
        , y(y)
        , w(w)
        , h(h)
        , floor(level)
        , name(name)
        , room(0)
    {

    }

    QRect bounds() const
    {
        return QRect(x, y, w, h);
    }

    QPoint topLeft() const { return QPoint(x, y); }
    QPoint topRight() const { return QPoint(x + w, y); }
    QPoint bottomLeft() const { return QPoint(x, y + h); }
    QPoint bottomRight() const { return QPoint(x + w, y + h); }

    bool isAdjacent(RoomRect *comp) const
    {
        QRect a(x - 1, y - 1, w + 2, h + 2);
        QRect b(comp->x, comp->y, comp->w, comp->h);
        return a.intersects(b);
    }

    bool isTouchingCorners(RoomRect *comp) const
    {
        return topLeft() == comp->bottomRight() ||
                topRight() == comp->bottomLeft() ||
                bottomLeft() == comp->topRight() ||
                bottomRight() == comp->topLeft();
    }

    bool inSameRoom(RoomRect *comp) const
    {
        if (floor != comp->floor) return false;
        if (name != comp->name) return false;
        if (!name.contains(QLatin1Char('#'))) return false;
        return isAdjacent(comp) && !isTouchingCorners(comp);
    }

    QString nameWithoutSuffix() const
    {
        int pos = name.indexOf(QLatin1Char('#'));
        if (pos == -1) return name;
        return name.left(pos);
    }

    int x;
    int y;
    int w;
    int h;
    int floor;
    QString name;
    Room *room;
};

class Room
{
public:
    Room(const QString &name, int level)
        : ID(-1)
        , floor(level)
        , name(name)
        , building(0)
    {

    }

    bool inSameBuilding(Room *comp)
    {
        foreach (RoomRect *rr, rects) {
            foreach (RoomRect *rr2, comp->rects) {
                if (rr->isAdjacent(rr2))
                    return true;
            }
        }
        return false;
    }

    const QRect& bounds() const
    {
        return mBounds;
    }

    QRect calculateBounds() const
    {
        QRect bounds;
        if (rects.isEmpty()) {
            return bounds;
        }
        bounds = rects[0]->bounds();
        for (int i = 1; i < rects.size(); i++) {
            bounds = bounds.united(rects[i]->bounds());
        }
        return bounds;
    }

    int ID;
    int floor;
    QString name;
    Building *building;
    QList<RoomRect*> rects;
    QList<RoomObject> objects;
    QRect mBounds;
};

class Building
{
public:
    QRect calculateBounds() const
    {
        QRect bounds;
        if (RoomList.isEmpty()) {
            return bounds;
        }
        bounds = RoomList[0]->bounds();
        for (int i = 1; i < RoomList.size(); i++) {
            bounds = bounds.united(RoomList[i]->bounds());
        }
        return bounds;
    }

    QList<Room*> RoomList;
};

template <typename T>
class RectLookup
{
public:
    RectLookup()
    {

    }

    void clear(int widthInChunks, int heightInChunks, int squaresPerChunk)
    {
        for (int i = 0; i < mGrid.size(); i++) {
            mGrid[i].clear();
        }
        mWidthInChunks = widthInChunks;
        mHeightInChunks = heightInChunks;
        mSquaresPerChunk = squaresPerChunk;
        mGrid.resize(mWidthInChunks * mHeightInChunks);
    }

    void add(T* element, const QRect& bounds)
    {
        int xMin = bounds.x() / mSquaresPerChunk;
        int yMin = bounds.y() / mSquaresPerChunk;
        int xMax = std::ceil((bounds.x() + bounds.width()) / float(mSquaresPerChunk));
        int yMax = std::ceil((bounds.y() + bounds.height()) / float(mSquaresPerChunk));
        xMin = clamp(xMin, 0, mWidthInChunks-1);
        yMin = clamp(yMin, 0, mHeightInChunks-1);
        xMax = clamp(xMax, 0, mWidthInChunks-1);
        yMax = clamp(yMax, 0, mHeightInChunks-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                mGrid[x + y * mWidthInChunks] += element;
            }
        }
    }

    void overlapping(const QRect& rect, QList<T*>& elements) const
    {
        int xMin = rect.x() / mSquaresPerChunk;
        int yMin = rect.y() / mSquaresPerChunk;
        int xMax = std::ceil((rect.x() + rect.width()) / float(mSquaresPerChunk));
        int yMax = std::ceil((rect.y() + rect.height()) / float(mSquaresPerChunk));
        xMin = clamp(xMin, 0, mWidthInChunks-1);
        yMin = clamp(yMin, 0, mHeightInChunks-1);
        xMax = clamp(xMax, 0, mWidthInChunks-1);
        yMax = clamp(yMax, 0, mHeightInChunks-1);
        for (int y = yMin; y <= yMax; y++) {
            for (int x = xMin; x <= xMax; x++) {
                for (T* e : mGrid[x + y * mWidthInChunks]) {
                    if (elements.contains(e) == false) {
                        elements += e;
                    }
                }
            }
        }
    }

    int mWidthInChunks;
    int mHeightInChunks;
    int mSquaresPerChunk;
    QVector<QVector<T*>> mGrid;
};

class Stats
{
public:
    Stats() :
        numBuildings(0),
        numRooms(0),
        numRoomRects(0),
        numRoomObjects(0)
    {
    }

    int numBuildings;
    int numRooms;
    int numRoomRects;
    int numRoomObjects;
};

} // namespace LotFile

class NewMapBinaryFile : public QObject
{
    Q_OBJECT
public:
    NewMapBinaryFile(int squaresPerChunk);

    bool write(MapComposite* mapComposite, const QVector<Tiled::PropertiesGrid*>& propertiesGrids, const QString& filePath);

    bool generateHeader(MapComposite *mapComposite);
    bool generateHeaderAux(QDataStream& out, MapComposite *mapComposite);
    bool generateChunk(QDataStream &out, MapComposite *mapComposite, int cx, int cy);
    void generateBuildingObjects(int mapWidth, int mapHeight);
    void generateBuildingObjects(int mapWidth, int mapHeight,
                                 LotFile::Room *room, LotFile::RoomRect *rr);
    QString nameOfTileset(const Tiled::Tileset *tileset);
    bool handleTileset(const Tiled::Tileset *tileset, uint &firstGid);

    int getRoomID(int x, int y, int z);

    QString errorString() const { return mError; }

signals:

private:
    uint cellToGid(const Tiled::Cell *cell);
    bool processObjectGroups(MapComposite *mapComposite);
    bool processObjectGroup(Tiled::ObjectGroup *objectGroup,
                            int levelOffset, const QPoint &offset);
    void SaveString(QDataStream& out, const QString& str);
    int toBits(const Tiled::Properties &properties, const QStringList &attributeNames) const;

private:
    int mSquaresPerChunk;
    QMap<const Tiled::Tileset*,uint> mTilesetToFirstGid;
    QMap<QString ,uint> mTilesetNameToFirstGid;
    Tiled::Tileset *mJumboTreeTileset;
    QMap<uint,LotFile::Tile*> mTileMap;
    QVector<QVector<QVector<LotFile::Square> > > mGridData;
    int MaxLevel;
    int Version;
    QList<LotFile::RoomRect*> mRoomRects;
    QMap<int,QList<LotFile::RoomRect*> > mRoomRectByLevel;
    QList<LotFile::Room*> roomList;
    QList<LotFile::Building*> buildingList;
    LotFile::Stats mStats;
    QString mError;
};

#endif // TMXBINARY_H
