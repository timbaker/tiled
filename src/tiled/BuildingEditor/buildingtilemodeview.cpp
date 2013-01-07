#include "buildingtilemodeview.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreviewwindow.h" // for CompositeLayerGroupItem;
#include "buildingtiles.h"
#include "buildingtiletools.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

#include <qmath.h>
#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

const int BuildingTileModeScene::ZVALUE_CURSOR = 1000;

/////

TileModeGridItem::TileModeGridItem(BuildingDocument *doc, MapRenderer *renderer) :
    QGraphicsItem(),
    mDocument(doc),
    mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    synchWithBuilding();
}

void TileModeGridItem::synchWithBuilding()
{
    mTileBounds = QRect(0, 0,
                        mDocument->building()->width() + 1,
                        mDocument->building()->height() + 1);

    QRectF bounds = mRenderer->boundingRect(mTileBounds, mDocument->currentLevel());
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF TileModeGridItem::boundingRect() const
{
    return mBoundingRect;
}

void TileModeGridItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    mRenderer->drawGrid(p, option->exposedRect, Qt::black,
                        mDocument->currentLevel(), mTileBounds);
}

/////

TileModeSelectionItem::TileModeSelectionItem(BuildingTileModeScene *scene) :
    mScene(scene)
{
    setZValue(1000);

    connect(document(), SIGNAL(tileSelectionChanged(QRegion)),
            SLOT(tileSelectionChanged(QRegion)));
    connect(document(), SIGNAL(currentFloorChanged()),
            SLOT(currentLevelChanged()));

    updateBoundingRect();
}

QRectF TileModeSelectionItem::boundingRect() const
{
    return mBoundingRect;
}

void TileModeSelectionItem::paint(QPainter *p,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *)
{
    const QRegion &selection = document()->tileSelection();
    QColor highlight = QApplication::palette().highlight().color();
    highlight.setAlpha(128);

    MapRenderer *renderer = mScene->renderer();
    renderer->drawTileSelection(p, selection, highlight, option->exposedRect,
                                mScene->currentLevel());
}

BuildingDocument *TileModeSelectionItem::document() const
{
    return mScene->document();
}

void TileModeSelectionItem::tileSelectionChanged(const QRegion &oldSelection)
{
    prepareGeometryChange();
    updateBoundingRect();

    // Make sure changes within the bounding rect are updated
    QRegion newSelection = document()->tileSelection();
    const QRect changedArea = newSelection.xored(oldSelection).boundingRect();
    update(mScene->renderer()->boundingRect(changedArea, document()->currentLevel()));
}

void TileModeSelectionItem::currentLevelChanged()
{
    prepareGeometryChange();
    updateBoundingRect();
}

void TileModeSelectionItem::updateBoundingRect()
{
    const QRect r = document()->tileSelection().boundingRect();
    mBoundingRect = mScene->renderer()->boundingRect(r, document()->currentLevel());
}

/////

BuildingTileModeScene::BuildingTileModeScene(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMapComposite(0),
    mMap(0),
    mBlendMapComposite(0),
    mBlendMap(0),
    mRenderer(0),
    mGridItem(0),
    mTileSelectionItem(0),
    mDarkRectangle(new QGraphicsRectItem),
    mCurrentTool(0),
    mLayerGroupWithToolTiles(0)
{
    setBackgroundBrush(Qt::darkGray);

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(0.6);
    mDarkRectangle->setVisible(false);
    addItem(mDarkRectangle);

    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    connect(BuildingPreferences::instance(), SIGNAL(highlightFloorChanged(bool)),
            SLOT(highlightFloorChanged(bool)));

    connect(TileToolManager::instance(), SIGNAL(currentToolChanged(BaseTileTool*)),
            SLOT(currentToolChanged(BaseTileTool*)));
}

BuildingTileModeScene::~BuildingTileModeScene()
{
    delete mMapComposite;
    if (mMap)
        TilesetManager::instance()->removeReferences(mMap->tilesets());
    delete mMap;

    delete mBlendMapComposite;
    if (mBlendMap)
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
    delete mBlendMap;

    delete mRenderer;
}

void BuildingTileModeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void BuildingTileModeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void BuildingTileModeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void BuildingTileModeScene::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    // Delete before clearing mDocument.
    delete mTileSelectionItem;
    mTileSelectionItem = 0;

    mDocument = doc;

    if (mMap) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        mMapComposite = 0;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;
        mMap = 0;

        delete mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        mBlendMapComposite = 0;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;
        mBlendMap = 0;

        delete mRenderer;
        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;

        mRenderer = 0;

        mLayerGroupWithToolTiles = 0;
    }

    if (!mDocument)
        return;

    BuildingToMap();

    mLoading = true;
    currentFloorChanged();
    mLoading = false;

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());

    connect(mDocument, SIGNAL(currentFloorChanged()),
            SLOT(currentFloorChanged()));
    connect(mDocument, SIGNAL(currentLayerChanged()),
            SLOT(currentLayerChanged()));

    connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
            SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));

    connect(mDocument, SIGNAL(roomDefinitionChanged()),
            SLOT(roomDefinitionChanged()));

    connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(floorAdded(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorRemoved(BuildingFloor*)),
            SLOT(floorRemoved(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorEdited(BuildingFloor*)),
            SLOT(floorEdited(BuildingFloor*)));

    connect(mDocument, SIGNAL(floorTilesChanged(BuildingFloor*)),
            SLOT(floorTilesChanged(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorTilesChanged(BuildingFloor*,QString,QRect)),
            SLOT(floorTilesChanged(BuildingFloor*,QString,QRect)));

    connect(mDocument, SIGNAL(layerOpacityChanged(BuildingFloor*,QString)),
            SLOT(layerOpacityChanged(BuildingFloor*,QString)));
    connect(mDocument, SIGNAL(layerVisibilityChanged(BuildingFloor*,QString)),
            SLOT(layerVisibilityChanged(BuildingFloor*,QString)));

    connect(mDocument, SIGNAL(objectAdded(BuildingObject*)),
            SLOT(objectAdded(BuildingObject*)));
    connect(mDocument, SIGNAL(objectRemoved(BuildingObject*)),
            SLOT(objectRemoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectMoved(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectTileChanged(BuildingObject*)),
            SLOT(objectTileChanged(BuildingObject*)));
    connect(mDocument, SIGNAL(objectChanged(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));

    connect(mDocument, SIGNAL(buildingResized()), SLOT(buildingResized()));
    connect(mDocument, SIGNAL(buildingRotated()), SLOT(buildingRotated()));

    connect(mDocument, SIGNAL(roomAdded(Room*)), SLOT(roomAdded(Room*)));
    connect(mDocument, SIGNAL(roomRemoved(Room*)), SLOT(roomRemoved(Room*)));
    connect(mDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));

    emit documentChanged();
}

void BuildingTileModeScene::clearDocument()
{
    setDocument(0);
    emit documentChanged();
}

int BuildingTileModeScene::currentLevel()
{
    return mDocument ? mDocument->currentLevel() : -1;
}

BuildingFloor *BuildingTileModeScene::currentFloor()
{
    return mDocument ? mDocument->currentFloor() : 0;
}

QStringList BuildingTileModeScene::layerNames() const
{
    if (!mDocument)
        return QStringList();

    QStringList ret;
    int level = mDocument->currentLevel();
    if (CompositeLayerGroup *lg = mMapComposite->layerGroupForLevel(level)) {
        foreach (TileLayer *tl, lg->layers())
            ret += MapComposite::layerNameWithoutPrefix(tl);
    }
    return ret;
}

QString BuildingTileModeScene::currentLayerName() const
{
    return mDocument ? mDocument->currentLayer() : QString();
}

QPoint BuildingTileModeScene::sceneToTile(const QPointF &scenePos)
{
    QPointF p = mRenderer->pixelToTileCoords(scenePos, currentLevel());

    // x/y < 0 rounds up to zero
    qreal x = p.x(), y = p.y();
    if (x < 0)
        x = -qCeil(-x);
    if (y < 0)
        y = -qCeil(-y);
    return QPoint(x, y);
}

QPointF BuildingTileModeScene::sceneToTileF(const QPointF &scenePos)
{
    return mRenderer->pixelToTileCoords(scenePos, currentLevel());
}

QRect BuildingTileModeScene::sceneToTileRect(const QRectF &sceneRect)
{
    QPoint topLeft = sceneToTile(sceneRect.topLeft());
    QPoint botRight = sceneToTile(sceneRect.bottomRight());
    return QRect(topLeft, botRight);
}

QPointF BuildingTileModeScene::tileToScene(const QPoint &tilePos)
{
    return mRenderer->tileToPixelCoords(tilePos, currentLevel());
}

QPolygonF BuildingTileModeScene::tileToScenePolygon(const QPoint &tilePos)
{
    QPolygonF polygon;
    polygon += tilePos;
    polygon += tilePos + QPoint(1, 0);
    polygon += tilePos + QPoint(1, 1);
    polygon += tilePos + QPoint(0, 1);
    polygon += polygon.first();
    return mRenderer->tileToPixelCoords(polygon, currentLevel());
}

QPolygonF BuildingTileModeScene::tileToScenePolygon(const QRect &tileRect)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight() + QPoint(1, 0);
    polygon += tileRect.bottomRight() + QPoint(1, 1);
    polygon += tileRect.bottomLeft() + QPoint(0, 1);
    polygon += polygon.first();
    return mRenderer->tileToPixelCoords(polygon, currentLevel());
}

QPolygonF BuildingTileModeScene::tileToScenePolygonF(const QRectF &tileRect)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight();
    polygon += tileRect.bottomRight();
    polygon += tileRect.bottomLeft();
    polygon += polygon.first();
    return mRenderer->tileToPixelCoords(polygon, currentLevel());
}

bool BuildingTileModeScene::currentFloorContains(const QPoint &tilePos, int dw, int dh)
{
    return currentFloor() && currentFloor()->contains(tilePos, dw, dh);
}

void BuildingTileModeScene::setToolTiles(const FloorTileGrid *tiles,
                                         const QPoint &pos,
                                         const QString &layerName)
{
    clearToolTiles();

    CompositeLayerGroupItem *item = itemForFloor(currentFloor());
    CompositeLayerGroup *layerGroup = item->layerGroup();

    TileLayer *layer = 0;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    QMap<QString,Tileset*> tilesetByName;
    foreach (Tileset *ts, mMap->tilesets())
        tilesetByName[ts->name()] = ts;

    QVector<QVector<Tiled::Cell> > cells(tiles->width());
    for (int x = 0; x < tiles->width(); x++)
        cells[x].resize(tiles->height());

    for (int x = 0; x < tiles->width(); x++) {
        for (int y = 0; y < tiles->height(); y++) {
            QString tileName = tiles->at(x, y);
            Tile *tile = 0;
            if (!tileName.isEmpty()) {
                tile = TilesetManager::instance()->missingTile();
                QString tilesetName;
                int index;
                if (BuildingTilesMgr::parseTileName(tileName, tilesetName, index)) {
                    if (tilesetByName.contains(tilesetName))
                        tile = tilesetByName[tilesetName]->tileAt(index);
                }
            }
            cells[x][y] = Cell(tile);
        }
    }

    layerGroup->setToolTiles(cells, pos, layer);
    mLayerGroupWithToolTiles = layerGroup;

    QRectF r = mRenderer->boundingRect(tiles->bounds().translated(pos), currentLevel())
            .adjusted(0, -(128-32), 0, 0); // use mMap->drawMargins()
    update(r);
}

void BuildingTileModeScene::clearToolTiles()
{
    if (mLayerGroupWithToolTiles) {
        mLayerGroupWithToolTiles->clearToolTiles();
        mLayerGroupWithToolTiles = 0;
    }
}

QString BuildingTileModeScene::buildingTileAt(int x, int y)
{
    CompositeLayerGroup *layerGroup = mBlendMapComposite->layerGroupForLevel(currentLevel());

    QString tileName;

    foreach (TileLayer *tl, layerGroup->layers()) {
        if (currentLayerName() == MapComposite::layerNameWithoutPrefix(tl)) {
            if (tl->contains(x, y)) {
                Tile *tile = tl->cellAt(x, y).tile;
                if (tile) {
                    tileName = BuildingTilesMgr::nameForTile(tile);
                }
            }
            break;
        }
    }

    return tileName;
}

void BuildingTileModeScene::BuildingToMap()
{
    if (mMapComposite) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;

        delete mBlendMapComposite->mapInfo();
        delete mBlendMapComposite;
        TilesetManager::instance()->removeReferences(mBlendMap->tilesets());
        delete mBlendMap;

        delete mRenderer;
        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;
        delete mTileSelectionItem;

        mLayerGroupWithToolTiles = 0;
    }

    Map::Orientation orient = Map::LevelIsometric;

    int maxLevel =  mDocument->building()->floorCount() - 1;
    int extraForWalls = 1;
    int extra = (orient == Map::LevelIsometric)
            ? extraForWalls : maxLevel * 3 + extraForWalls;
    QSize mapSize(mDocument->building()->width() + extra,
                  mDocument->building()->height() + extra);

    mMap = new Map(orient,
                   mapSize.width(), mapSize.height(),
                   64, 32);

    // Add tilesets from Tilesets.txt
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets())
        mMap->addTileset(ts);
    TilesetManager::instance()->addReferences(mMap->tilesets());

    switch (mMap->orientation()) {
    case Map::Isometric:
        mRenderer = new IsometricRenderer(mMap);
        break;
    case Map::LevelIsometric:
        mRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return;
    }

    // The order must match the LayerIndexXXX constants.
    // FIXME: add user-defined layers as well
    const char *layerNames[] = {
        "Floor",
        "FloorGrime",
        "FloorGrime2",
        "Walls",
        "Walls2",
        "RoofCap",
        "RoofCap2",
        "WallOverlay",
        "WallOverlay2",
        "WallGrime",
        "WallFurniture",
        "Frames",
        "Doors",
        "Curtains",
        "Furniture",
        "Furniture2",
        "Curtains2",
        "Roof",
        "Roof2",
        "RoofTop",
        0
    };
    Q_ASSERT(sizeof(layerNames)/sizeof(layerNames[0]) == BuildingFloor::Square::MaxSection + 1);

    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        for (int i = 0; layerNames[i]; i++) {
            QString name = QLatin1String(layerNames[i]);
            QString layerName = tr("%1_%2").arg(floor->level()).arg(name);
            TileLayer *tl = new TileLayer(layerName,
                                          0, 0, mapSize.width(), mapSize.height());
            mMap->addLayer(tl);
        }
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);
    mMapComposite = new MapComposite(mapInfo);

    // This map displays the automatically-generated tiles from the building.
    mBlendMap = mMap->clone();
    TilesetManager::instance()->addReferences(mBlendMap->tilesets());
    mapInfo = MapManager::instance()->newFromMap(mBlendMap);
    mBlendMapComposite = new MapComposite(mapInfo);
    mMapComposite->setBlendOverMap(mBlendMapComposite);

    // Set the automatically-generated tiles.
    foreach (CompositeLayerGroup *layerGroup, mBlendMapComposite->layerGroups()) {
        BuildingFloor *floor = mDocument->building()->floor(layerGroup->level());
        floor->LayoutToSquares();
        BuildingFloorToTileLayers(floor, layerGroup->layers());
    }

    // Set the user-drawn tiles.
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        foreach (QString layerName, floor->grimeLayers())
            floorTilesToLayer(floor, layerName, floor->bounds().adjusted(0, 0, 1, 1));
    }

    // Do this before calculating the bounds of CompositeLayerGroupItem
    mRenderer->setMaxLevel(mMapComposite->maxLevel());

    foreach (CompositeLayerGroup *layerGroup, mMapComposite->layerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mRenderer);
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }

    mGridItem = new TileModeGridItem(mDocument, mRenderer);
    mGridItem->synchWithBuilding();
    mGridItem->setZValue(1000);
    addItem(mGridItem);

    mTileSelectionItem = new TileModeSelectionItem(this);
    addItem(mTileSelectionItem);

    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());
}

void BuildingTileModeScene::BuildingFloorToTileLayers(BuildingFloor *floor,
                                                      const QVector<TileLayer *> &layers)
{
    int maxLevel = floor->building()->floorCount() - 1;
    int offset = (mMap->orientation() == Map::LevelIsometric)
            ? 0 : (maxLevel - floor->level()) * 3;

    int section = 0;
    foreach (TileLayer *tl, layers) {
        tl->erase();
        for (int x = 0; x <= floor->width(); x++) {
            for (int y = 0; y <= floor->height(); y++) {
                const BuildingFloor::Square &square = floor->squares[x][y];
                if (BuildingTile *btile = square.mTiles[section]) {
                    if (!btile->isNone()) {
                        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(btile))
                            tl->setCell(x + offset, y + offset, Cell(tile));
                    }
                    continue;
                }
                if (BuildingTileEntry *entry = square.mEntries[section]) {
                    int tileOffset = square.mEntryEnum[section];
                    if (entry->isNone() || entry->tile(tileOffset)->isNone())
                        continue;
                    if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->tile(tileOffset)))
                        tl->setCell(x + offset, y + offset, Cell(tile));
                }

            }
        }
        section++;
    }
}

void BuildingTileModeScene::floorTilesToLayer(BuildingFloor *floor,
                                              const QString &layerName,
                                              const QRect &bounds)
{
    TileLayer *layer = 0;
    foreach (TileLayer *tl, mMapComposite->layerGroupForLevel(floor->level())->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    QMap<QString,Tileset*> tilesetByName;
    foreach (Tileset *ts, mMap->tilesets())
        tilesetByName[ts->name()] = ts;

    for (int x = bounds.left(); x <= bounds.right(); x++) {
        for (int y = bounds.top(); y <= bounds.bottom(); y++) {
            QString tileName = floor->grimeAt(layerName, x, y);
            Tile *tile = 0;
            if (!tileName.isEmpty()) {
                tile = TilesetManager::instance()->missingTile();
                QString tilesetName;
                int index;
                if (BuildingTilesMgr::parseTileName(tileName, tilesetName, index)) {
                    if (tilesetByName.contains(tilesetName)) {
                        tile = tilesetByName[tilesetName]->tileAt(index);
                    }
                }
            }
            layer->setCell(x, y, Cell(tile));
        }
    }
}

CompositeLayerGroupItem *BuildingTileModeScene::itemForFloor(BuildingFloor *floor)
{
    if (mLayerGroupItems.contains(floor->level()))
        return mLayerGroupItems[floor->level()];
    return 0;
}

void BuildingTileModeScene::floorEdited(BuildingFloor *floor)
{
    // Existence check needed while loading a map.
    // floorAdded -> objectAdded -> floorEdited
    // With stairs, this may be the floor above the one the object is on, but
    // that floor hasn't been added yet.
    if (!mLayerGroupItems.contains(floor->level()))
        return;
    floor->LayoutToSquares();
    BuildingFloorToTileLayers(floor, mBlendMapComposite->tileLayersForLevel(floor->level())->layers());
    itemForFloor(floor)->synchWithTileLayers();
    itemForFloor(floor)->updateBounds();

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());
}

void BuildingTileModeScene::floorTilesChanged(BuildingFloor *floor)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        foreach (TileLayer *tl, item->layerGroup()->layers())
            tl->erase();
        foreach (QString layerName, floor->grimeLayers())
            floorTilesToLayer(floor, layerName, floor->bounds().adjusted(0, 0, 1, 1));
        item->synchWithTileLayers();
        item->updateBounds();
    }
    QRectF sceneRect = mMapComposite->boundingRect(mRenderer);
    if (sceneRect != this->sceneRect()) {
        setSceneRect(sceneRect);
        mDarkRectangle->setRect(sceneRect);
    }
}

void BuildingTileModeScene::floorTilesChanged(BuildingFloor *floor,
                                              const QString &layerName,
                                              const QRect &bounds)
{
    floorTilesToLayer(floor, layerName, bounds);
    itemForFloor(floor)->synchWithTileLayers();
    itemForFloor(floor)->updateBounds();
    QRectF sceneRect = mMapComposite->boundingRect(mRenderer);
    if (sceneRect != this->sceneRect()) {
        setSceneRect(sceneRect);
        mDarkRectangle->setRect(sceneRect);
    }
}

void BuildingTileModeScene::layerOpacityChanged(BuildingFloor *floor,
                                                const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerOpacity(layerName, floor->layerOpacity(layerName)))
            item->update();
    }
}

void BuildingTileModeScene::layerVisibilityChanged(BuildingFloor *floor, const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerVisibility(layerName, floor->layerVisibility(layerName))) {
            item->synchWithTileLayers();
            item->updateBounds();
        }
    }
}

void BuildingTileModeScene::currentFloorChanged()
{
    highlightFloorChanged(BuildingPreferences::instance()->highlightFloor());
    mGridItem->synchWithBuilding();
}

void BuildingTileModeScene::currentLayerChanged()
{
    if (CompositeLayerGroupItem *item = itemForFloor(currentFloor())) {
        QRectF bounds = item->boundingRect();

        if (!mNonEmptyLayer.isEmpty())
            item->layerGroup()->setLayerNonEmpty(mNonEmptyLayer, false);
        QString layerName = currentLayerName();
        if (!layerName.isEmpty())
            item->layerGroup()->setLayerNonEmpty(layerName, true);
        mNonEmptyLayer = layerName;

        if (bounds.isEmpty()) {
            item->synchWithTileLayers();
            item->updateBounds();
        }
        QRectF sceneRect = mMapComposite->boundingRect(mRenderer);
        if (sceneRect != this->sceneRect()) {
            setSceneRect(sceneRect);
            mDarkRectangle->setRect(sceneRect);
        }
    }
}
void BuildingTileModeScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    Q_UNUSED(pos);
    floorEdited(floor);
}

void BuildingTileModeScene::roomDefinitionChanged()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomChanged(Room *room)
{
    Q_UNUSED(room)
    roomDefinitionChanged();
}

void BuildingTileModeScene::floorAdded(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    BuildingToMap();
}

void BuildingTileModeScene::floorRemoved(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    BuildingToMap();
}

void BuildingTileModeScene::objectAdded(BuildingObject *object)
{
    if (mLoading)
        return;

    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingTileModeScene::objectRemoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingTileModeScene::objectMoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingTileModeScene::objectTileChanged(BuildingEditor::BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingTileModeScene::buildingResized()
{
    buildingRotated();
}

// Called when the building is flipped, rotated or resized.
void BuildingTileModeScene::buildingRotated()
{
    int extra = (mMap->orientation() == Map::LevelIsometric) ?
                1 : mMapComposite->maxLevel() * 3 + 1;
    int width = mDocument->building()->width() + extra;
    int height = mDocument->building()->height() + extra;

    foreach (Layer *layer, mMap->layers())
        layer->resize(QSize(width, height), QPoint());
    mMap->setWidth(width);
    mMap->setHeight(height);

    foreach (Layer *layer, mBlendMap->layers())
        layer->resize(QSize(width, height), QPoint());
    mBlendMap->setWidth(width);
    mBlendMap->setHeight(height);

    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        // If floor mGrimeGrid is set to empty, we don't know which layers had
        // tiles in them, so we must erase every layer.
        foreach (TileLayer *tl, itemForFloor(floor)->layerGroup()->layers())
            tl->erase();
        foreach (QString layerName, floor->grimeLayers())
            floorTilesToLayer(floor, layerName, floor->bounds().adjusted(0, 0, 1, 1));
        floorEdited(floor);
    }
    mGridItem->synchWithBuilding();
}

void BuildingTileModeScene::highlightFloorChanged(bool highlight)
{
    qreal z = 0;

    if (!mDocument) {
        mDarkRectangle->setVisible(false);
        return;
    }

    int currentLevel = mDocument->currentLevel();
    foreach (CompositeLayerGroupItem *item, mLayerGroupItems) {
        item->setVisible(!highlight ||
                         (item->layerGroup()->level() <= currentLevel));
        if (item->layerGroup()->level() == currentLevel)
            z = item->zValue() - 0.5;
    }

    mDarkRectangle->setVisible(highlight);
    mDarkRectangle->setZValue(z);
}

void BuildingTileModeScene::tilesetAdded(Tileset *tileset)
{
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    BuildingToMap();
}

void BuildingTileModeScene::tilesetAboutToBeRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();
}

void BuildingTileModeScene::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    BuildingToMap();
}

void BuildingTileModeScene::currentToolChanged(BaseTileTool *tool)
{
    if (mCurrentTool == DrawTileTool::instance())
        clearToolTiles();
    mCurrentTool = tool;
}

/////

BuildingTileModeView::BuildingTileModeView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this)),
    mHandScrolling(false)
{
    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

BuildingTileModeView::~BuildingTileModeView()
{
    setHandScrolling(false); // Just in case we didn't get a hide event
}

bool BuildingTileModeView::event(QEvent *e)
{
    // Ignore space bar events since they're handled by the MainWindow
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Space) {
            e->ignore();
            return false;
        }
    }

    return QGraphicsView::event(e);
}

bool BuildingTileModeView::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object)
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            TileToolManager::instance()->checkKeyboardModifiers(keyEvent->modifiers());
        }
        break;
    default:
        break;
    }

    return false;
}

void BuildingTileModeView::hideEvent(QHideEvent *event)
{
    // Disable hand scrolling when the view gets hidden in any way
    setHandScrolling(false);
    QGraphicsView::hideEvent(event);
}

void BuildingTileModeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void BuildingTileModeView::mouseMoveEvent(QMouseEvent *event)
{
    if (mHandScrolling) {
        QScrollBar *hBar = horizontalScrollBar();
        QScrollBar *vBar = verticalScrollBar();
        const QPoint d = event->globalPos() - mLastMousePos;
        hBar->setValue(hBar->value() + (isRightToLeft() ? d.x() : -d.x()));
        vBar->setValue(vBar->value() - d.y());

        mLastMousePos = event->globalPos();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);

    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));

    if (!scene()->document())
        return;

    QPoint tilePos = scene()->sceneToTile(mLastMouseScenePos);
    if (tilePos != mLastMouseTilePos) {
        mLastMouseTilePos = tilePos;
        emit mouseCoordinateChanged(mLastMouseTilePos);
    }
}

void BuildingTileModeView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(false);
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void BuildingTileModeView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(event->delta());

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMousePos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void BuildingTileModeView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
    qDebug() << "setHandScrolling" << mHandScrolling;
    setInteractive(!mHandScrolling);

    if (mHandScrolling) {
        mLastMousePos = QCursor::pos();
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        viewport()->grabMouse();
    } else {
        viewport()->releaseMouse();
        QApplication::restoreOverrideCursor();
    }
}

void BuildingTileModeView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}
