#include "newmapbinaryfile.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilesetmanager.h"
#include "tilemetainfomgr.h"

#include "gidmapper.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tile.h"
#include "tileset.h"

#include <qmath.h>

using namespace Tiled;
using namespace Tiled::Internal;

NewMapBinaryFile::NewMapBinaryFile()
{

}

bool NewMapBinaryFile::write(MapComposite *mapComposite, const QString &filePath)
{
    MapInfo* mapInfo = mapComposite->mapInfo();

    mStats = LotFile::Stats();

    MaxLevel = mapComposite->maxLevel();

    // Check for missing tilesets.
    for (MapComposite *mc : mapComposite->maps()) {
        MaxLevel = qMax(MaxLevel, mc->maxLevel());
        if (mc->map()->hasUsedMissingTilesets()) {
            mError = tr("Some tilesets are missing:\n%1").arg(mc->mapInfo()->path());
            return false;
        }
    }

    MaxLevel += 1;

    if (!generateHeader(mapComposite))
        return false;
/*
    bool chunkDataOnly = false;
    if (chunkDataOnly) {
        for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
            lg->prepareDrawing2();
        }
        const GenerateLotsSettings &lotSettings = mWorldDoc->world()->getGenerateLotsSettings();
        Navigate::ChunkDataFile cdf;
        cdf.fromMap(cell->x(), cell->y(), mapComposite, mRoomRectByLevel[0], lotSettings);
        return true;
    }
*/

    int mapWidth = mapInfo->width();
    int mapHeight = mapInfo->height();

    int NUM_CHUNKS_X = (mapInfo->width() + CHUNK_WIDTH - 1) / CHUNK_WIDTH;
    int NUM_CHUNKS_Y = (mapInfo->height() + CHUNK_HEIGHT - 1) / CHUNK_HEIGHT;

    // Resize the grid and cleanup data from the previous cell.
    mGridData.resize(NUM_CHUNKS_X * CHUNK_WIDTH);
    for (int x = 0; x < NUM_CHUNKS_X * CHUNK_WIDTH; x++) {
        mGridData[x].resize(NUM_CHUNKS_Y * CHUNK_HEIGHT);
        for (int y = 0; y < NUM_CHUNKS_Y * CHUNK_HEIGHT; y++) {
            mGridData[x][y].fill(LotFile::Square(), MaxLevel);
        }
    }

    Tile *missingTile = Tiled::Internal::TilesetManager::instance()->missingTile();
    QVector<const Tiled::Cell *> cells(40);
    for (CompositeLayerGroup *lg : mapComposite->layerGroups()) {
        lg->prepareDrawing2();
        int d = (mapInfo->orientation() == Map::Isometric) ? -3 : 0;
        d *= lg->level();
        for (int y = d; y < mapHeight; y++) {
            for (int x = d; x < mapWidth; x++) {
                cells.resize(0);
                lg->orderedCellsAt2(QPoint(x, y), cells);
                for (const Tiled::Cell *cell : cells) {
                    if (cell->tile == missingTile) continue;
                    int lx = x, ly = y;
                    if (mapInfo->orientation() == Map::Isometric) {
                        lx = x + lg->level() * 3;
                        ly = y + lg->level() * 3;
                    }
                    if (lx >= mapWidth) continue;
                    if (ly >= mapHeight) continue;
                    LotFile::Entry *e = new LotFile::Entry(cellToGid(cell));
                    mGridData[lx][ly][lg->level()].Entries.append(e);
                    mTileMap[e->gid]->used = true;
                }
            }
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
        mError = tr("Could not open file for writing.");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    generateBuildingObjects(mapWidth, mapHeight);

    if (!generateHeaderAux(out, mapComposite))
        return false;

    qint64 chunkTablePosition = file.pos();

//    out << qint32(NUM_CHUNKS_X * NUM_CHUNKS_Y);
    for (int m = 0; m < NUM_CHUNKS_X * NUM_CHUNKS_Y; m++) {
        out << qint64(m);
    }

    QList<qint64> PositionMap;

    for (int y = 0; y < NUM_CHUNKS_Y; y++) {
        for (int x = 0; x < NUM_CHUNKS_X; x++) {
            PositionMap += file.pos();
            if (!generateChunk(out, mapComposite, x, y))
                return false;
        }
    }

    file.close();
    if (!file.open(QIODevice::ReadWrite | QIODevice::Append)) {
        mError = tr("Could not open file for writing.");
        return false;
    }
    file.seek(chunkTablePosition);

    QDataStream out2(&file);
    out2.setByteOrder(QDataStream::LittleEndian);

    for (int m = 0; m < NUM_CHUNKS_X * NUM_CHUNKS_Y; m++) {
        out2 << qint64(PositionMap[m]);
    }

    file.close();
#if 0
    Navigate::ChunkDataFile cdf;
    cdf.fromMap(cell->x(), cell->y(), mapComposite, mRoomRectByLevel[0], lotSettings);
#endif
    return true;
}

bool NewMapBinaryFile::generateHeader(MapComposite *mapComposite)
{
    qDeleteAll(mRoomRects);
    qDeleteAll(roomList);
    qDeleteAll(buildingList);
    qDeleteAll(ZoneList);

    mRoomRects.clear();
    mRoomRectByLevel.clear();
    roomList.clear();
    buildingList.clear();
    ZoneList.clear();

    // Create the set of all tilesets used by the map and its sub-maps.
    QList<Tileset*> tilesets;
    for (MapComposite *mc : mapComposite->maps()) {
        tilesets += mc->map()->tilesets();
    }

    mJumboTreeTileset = nullptr;
    if (mJumboTreeTileset == nullptr) {
        mJumboTreeTileset = new Tiled::Tileset(QLatin1Literal("jumbo_tree_01"), 64, 128);
        mJumboTreeTileset->loadFromNothing(QSize(64, 128), QLatin1String("jumbo_tree_01"));
    }
    tilesets += mJumboTreeTileset;
    QScopedPointer<Tiled::Tileset> scoped(mJumboTreeTileset);

    qDeleteAll(mTileMap.values());
    mTileMap.clear();
    mTileMap[0] = new LotFile::Tile;

    mTilesetToFirstGid.clear();
    uint firstGid = 1;
    for (Tileset *tileset : tilesets) {
        if (!handleTileset(tileset, firstGid)) {
            return false;
        }
    }

    if (!processObjectGroups(mapComposite)) {
        return false;
    }

    // Merge adjacent RoomRects on the same level into rooms.
    // Only RoomRects with matching names and with # in the name are merged.
    for (int level : mRoomRectByLevel.keys()) {
        QList<LotFile::RoomRect*> rrList = mRoomRectByLevel[level];
        for (LotFile::RoomRect *rr : rrList) {
            if (rr->room == nullptr) {
                rr->room = new LotFile::Room(rr->nameWithoutSuffix(),
                                             rr->floor);
                rr->room->rects += rr;
                roomList += rr->room;
            }
            if (!rr->name.contains(QLatin1Char('#'))) {
                continue;
            }
            for (LotFile::RoomRect *comp : rrList) {
                if (comp == rr)
                    continue;
                if (comp->room == rr->room)
                    continue;
                if (rr->inSameRoom(comp)) {
                    if (comp->room != nullptr) {
                        LotFile::Room *room = comp->room;
                        for (LotFile::RoomRect *rr2 : room->rects) {
                            Q_ASSERT(rr2->room == room);
                            Q_ASSERT(!rr->room->rects.contains(rr2));
                            rr2->room = rr->room;
                        }
                        rr->room->rects += room->rects;
                        roomList.removeOne(room);
                        delete room;
                    } else {
                        comp->room = rr->room;
                        rr->room->rects += comp;
                        Q_ASSERT(rr->room->rects.count(comp) == 1);
                    }
                }
            }
        }
    }
    for (int i = 0; i < roomList.size(); i++) {
        roomList[i]->ID = i;
    }
    mStats.numRoomRects += mRoomRects.size();
    mStats.numRooms += roomList.size();

    // Merge adjacent rooms into buildings.
    // Rooms on different levels that overlap in x/y are merged into the
    // same buliding.
    for (LotFile::Room *r : roomList) {
        if (r->building == nullptr) {
            r->building = new LotFile::Building();
            buildingList += r->building;
            r->building->RoomList += r;
        }
        for (LotFile::Room *comp : roomList) {
            if (comp == r)
                continue;
            if (r->building == comp->building)
                continue;
            if (r->inSameBuilding(comp)) {
                if (comp->building != nullptr) {
                    LotFile::Building *b = comp->building;
                    for (LotFile::Room *r2 : b->RoomList) {
                        Q_ASSERT(r2->building == b);
                        Q_ASSERT(!r->building->RoomList.contains(r2));
                        r2->building = r->building;
                    }
                    r->building->RoomList += b->RoomList;
                    buildingList.removeOne(b);
                    delete b;
                } else {
                    comp->building = r->building;
                    r->building->RoomList += comp;
                    Q_ASSERT(r->building->RoomList.count(comp) == 1);
                }
            }
        }
    }
    mStats.numBuildings += buildingList.size();

    return true;
}

bool NewMapBinaryFile::generateHeaderAux(QDataStream &out, MapComposite *mapComposite)
{
    Q_UNUSED(mapComposite)

//    QString fileName = tr("%1_%2.lotheader")
//            .arg(lotSettings.worldOrigin.x() + cell->x())
//            .arg(lotSettings.worldOrigin.y() + cell->y());

//    QString lotsDirectory = mWorldDoc->world()->getGenerateLotsSettings().exportDir;
//    QFile file(lotsDirectory + QLatin1Char('/') + fileName);
//    if (!file.open(QIODevice::WriteOnly /*| QIODevice::Text*/)) {
//        mError = tr("Could not open file for writing.");
//        return false;
//    }

    out << quint8('P') << quint8('Z') << quint8('B') << quint8('Y');
    Version = 0;
    out << qint32(Version);

    int tilecount = 0;
    for (LotFile::Tile *tile : mTileMap) {
        if (tile->used) {
            tile->id = tilecount;
            tilecount++;
//            if (tile->name.startsWith(QLatin1Literal("jumbo_tree_01"))) {
//                int nnn = 0;
//            }
        }
    }
    out << qint32(tilecount);

    for (LotFile::Tile *tile : mTileMap) {
        if (tile->used) {
            SaveString(out, tile->name);
        }
    }

    MapInfo* mapInfo = mapComposite->mapInfo();
    int NUM_CHUNKS_X = (mapInfo->width() + CHUNK_WIDTH - 1) / CHUNK_WIDTH;
    int NUM_CHUNKS_Y = (mapInfo->height() + CHUNK_WIDTH - 1) / CHUNK_WIDTH;

    out << qint32(NUM_CHUNKS_X);
    out << qint32(NUM_CHUNKS_Y);
    out << qint32(MaxLevel);

    out << qint32(roomList.count());
    for (LotFile::Room *room : roomList) {
        SaveString(out, room->name);
        out << qint32(room->floor);

        out << qint32(room->rects.size());
        for (LotFile::RoomRect *rr : room->rects) {
            out << qint32(rr->x);
            out << qint32(rr->y);
            out << qint32(rr->w);
            out << qint32(rr->h);
        }

        out << qint32(room->objects.size());
        for (const LotFile::RoomObject &object : room->objects) {
            out << qint32(object.metaEnum);
            out << qint32(object.x);
            out << qint32(object.y);
        }
    }

    out << qint32(buildingList.count());
    for (LotFile::Building *building : buildingList) {
        out << qint32(building->RoomList.count());
        for (LotFile::Room *room : building->RoomList) {
            out << qint32(room->ID);
        }
    }
/*
    for (int x = 0; x < 30; x++) {
        for (int y = 0; y < 30; y++) {
            QRgb pixel = ZombieSpawnMap.pixel(cell->x() * 30 + x,
                                              cell->y() * 30 + y);
            out << quint8(qRed(pixel));
        }
    }
*/

    return true;
}

bool NewMapBinaryFile::generateChunk(QDataStream &out, MapComposite *mapComposite, int cx, int cy)
{
    Q_UNUSED(mapComposite)

    int notdonecount = 0;
    for (int z = 0; z < MaxLevel; z++)  {
        for (int x = 0; x < CHUNK_WIDTH; x++) {
            for (int y = 0; y < CHUNK_HEIGHT; y++) {
                int gx = cx * CHUNK_WIDTH + x;
                int gy = cy * CHUNK_HEIGHT + y;
                const QList<LotFile::Entry*> &entries = mGridData[gx][gy][z].Entries;
                if (entries.count() == 0) {
                    notdonecount++;
                } else {
                    if (notdonecount > 0) {
                        out << qint32(-1);
                        out << qint32(notdonecount);
                    }
                    notdonecount = 0;
                    out << qint32(entries.count() + 1);
                    out << qint32(getRoomID(gx, gy, z));
                }
                for (LotFile::Entry *entry : entries) {
                    Q_ASSERT(mTileMap[entry->gid]);
                    Q_ASSERT(mTileMap[entry->gid]->id != -1);
                    out << qint32(mTileMap[entry->gid]->id);
                }
            }
        }
    }
    if (notdonecount > 0) {
        out << qint32(-1);
        out << qint32(notdonecount);
    }

    return true;
}

void NewMapBinaryFile::generateBuildingObjects(int mapWidth, int mapHeight)
{
    for (LotFile::Room *room : roomList) {
        for (LotFile::RoomRect *rr : room->rects) {
            generateBuildingObjects(mapWidth, mapHeight, room, rr);
        }
    }
}

void NewMapBinaryFile::generateBuildingObjects(int mapWidth, int mapHeight,
                                              LotFile::Room *room, LotFile::RoomRect *rr)
{
    for (int x = rr->x; x < rr->x + rr->w; x++) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {

            // Remember the room at each position in the map.
            mGridData[x][y][room->floor].roomID = room->ID;

            /* Examine every tile inside the room.  If the tile's metaEnum >= 0
               then create a new RoomObject for it. */
            for (LotFile::Entry *entry : mGridData[x][y][room->floor].Entries) {
                int metaEnum = mTileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y;
                    object.metaEnum = metaEnum;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check south of the room for doors.
    int y = rr->y + rr->h;
    if (y < mapHeight) {
        for (int x = rr->x; x < rr->x + rr->w; x++) {
            for (LotFile::Entry *entry : mGridData[x][y][room->floor].Entries) {
                int metaEnum = mTileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumNorth(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x;
                    object.y = y - 1;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }

    // Check east of the room for doors.
    int x = rr->x + rr->w;
    if (x < mapWidth) {
        for (int y = rr->y; y < rr->y + rr->h; y++) {
            for (LotFile::Entry *entry : mGridData[x][y][room->floor].Entries) {
                int metaEnum = mTileMap[entry->gid]->metaEnum;
                if (metaEnum >= 0 && TileMetaInfoMgr::instance()->isEnumWest(metaEnum)) {
                    LotFile::RoomObject object;
                    object.x = x - 1;
                    object.y = y;
                    object.metaEnum = metaEnum + 1;
                    room->objects += object;
                    ++mStats.numRoomObjects;
                }
            }
        }
    }
}

QString NewMapBinaryFile::nameOfTileset(const Tileset *tileset)
{
    QString name = tileset->imageSource();
    if (name.contains(QLatin1String("/"))) {
        name = name.mid(name.lastIndexOf(QLatin1String("/")) + 1);
    }
    name.replace(QLatin1String(".png"), QLatin1String(""));
    return name;
}

bool NewMapBinaryFile::handleTileset(const Tiled::Tileset *tileset, uint &firstGid)
{
    if (!tileset->fileName().isEmpty()) {
        mError = tr("Only tileset image files supported, not external tilesets");
        return false;
    }

    QString name = nameOfTileset(tileset);

    // TODO: Verify that two tilesets sharing the same name are identical
    // between maps.
    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end) {
        QString name2 = nameOfTileset(i.key());
        if (name == name2) {
            mTilesetToFirstGid.insert(tileset, i.value());
            return true;
        }
        ++i;
    }

    for (int i = 0; i < tileset->tileCount(); ++i) {
        uint localID = uint(i);
        uint ID = firstGid + localID;
        LotFile::Tile *tile = new LotFile::Tile(name + QLatin1String("_") + QString::number(localID));
        tile->metaEnum = TileMetaInfoMgr::instance()->tileEnumValue(tileset->tileAt(i));
        mTileMap[ID] = tile;
    }

    mTilesetToFirstGid.insert(tileset, firstGid);
    firstGid += uint(tileset->tileCount());

    return true;
}

int NewMapBinaryFile::getRoomID(int x, int y, int z)
{
    return mGridData[x][y][z].roomID;
}

uint NewMapBinaryFile::cellToGid(const Cell *cell)
{
    Tileset *tileset = cell->tile->tileset();

    QMap<const Tileset*,uint>::const_iterator i = mTilesetToFirstGid.begin();
    QMap<const Tileset*,uint>::const_iterator i_end = mTilesetToFirstGid.end();
    while (i != i_end && i.key() != tileset) {
        ++i;
    }
    if (i == i_end) {// tileset not found
        return 0;
    }
    return i.value() + uint(cell->tile->id());
}

bool NewMapBinaryFile::processObjectGroups(MapComposite *mapComposite)
{
    for (Layer *layer : mapComposite->map()->layers()) {
        if (ObjectGroup *og = layer->asObjectGroup()) {
            if (!processObjectGroup(og, mapComposite->levelRecursive(),
                                    mapComposite->originRecursive())) {
                return false;
            }
        }
    }

    for (MapComposite *subMap : mapComposite->subMaps()) {
        if (!processObjectGroups(subMap)) {
            return false;
        }
    }

    return true;
}

bool NewMapBinaryFile::processObjectGroup(ObjectGroup *objectGroup, int levelOffset, const QPoint &offset)
{
    int level;
    if (!MapComposite::levelForLayer(objectGroup, &level))
        return true;
    level += levelOffset;

    for (const MapObject *mapObject : objectGroup->objects()) {
#if 0
        if (mapObject->name().isEmpty() || mapObject->type().isEmpty())
            continue;
#endif
        if (!int(mapObject->width()) || !int(mapObject->height()))
            continue;

        int x = qFloor(mapObject->x());
        int y = qFloor(mapObject->y());
        int w = qCeil(mapObject->x() + mapObject->width()) - x;
        int h = qCeil(mapObject->y() + mapObject->height()) - y;

        QString name = mapObject->name();
        if (name.isEmpty())
            name = QLatin1String("unnamed");

        if (objectGroup->map()->orientation() == Map::Isometric) {
            x += 3 * level;
            y += 3 * level;
        }

        // Apply the MapComposite offset in the top-level map.
        x += offset.x();
        y += offset.y();

        if (objectGroup->name().contains(QLatin1String("RoomDefs"))) {
#if 0 // NOTE: can't place buildings across cell boundaries
            if (x < 0 || y < 0 || x + w > 300 || y + h > 300) {
                x = qBound(0, x, 300);
                y = qBound(0, y, 300);
                mError = tr("A RoomDef in cell %1,%2 overlaps cell boundaries.\nNear x,y=%3,%4")
                        .arg(cell->x()).arg(cell->y()).arg(x).arg(y);
                return false;
            }
#endif
            LotFile::RoomRect *rr = new LotFile::RoomRect(name, x, y, level,
                                                          w, h);
            mRoomRects += rr;
            mRoomRectByLevel[level] += rr;
        } else {
            LotFile::Zone *z = new LotFile::Zone(name,
                                                 mapObject->type(),
                                                 x, y, level, w, h);
            ZoneList.append(z);
        }
    }
    return true;
}

void NewMapBinaryFile::SaveString(QDataStream& out, const QString& str)
{
    for (int i = 0; i < str.length(); i++) {
        if (str[i].toLatin1() == '\n') continue;
        out << quint8(str[i].toLatin1());
    }
    out << quint8('\n');
}
