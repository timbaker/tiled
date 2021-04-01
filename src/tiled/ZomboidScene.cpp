/*
 * ZomboidScene.cpp
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

#include "ZomboidScene.h"

#include "bmpblender.h"
#include "bmptool.h"
#include "map.h"
#include "mapbuildings.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapimagemanager.h"
#include "maplevel.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "zgriditem.h"
#include "objectgroup.h"
#include "preferences.h"
#include "tilelayer.h"
#include "tilelayeritem.h"
#include "toolmanager.h"
#include "zlevelsmodel.h"
#include "zlotmanager.h"

#include "worlded/worldcell.h"

#include <QGraphicsSceneMouseEvent>

using namespace Tiled;
using namespace Tiled::Internal;

extern bool gStartupBlockRendering;

///// ///// ///// ///// /////

#include <QStyleOptionGraphicsItem>

CompositeLayerGroupItem::CompositeLayerGroupItem(CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mLayerGroup(layerGroup)
    , mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = layerGroup->boundingRect(mRenderer);
}

QRectF CompositeLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void CompositeLayerGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    // This is a dumb hack used when restoring a session.
    if (gStartupBlockRendering)
        return;

    if (mLayerGroup->needsSynch() /*mBoundingRect != mLayerGroup->boundingRect(mRenderer)*/)
        return;

    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#ifdef _DEBUG
    p->drawRect(mBoundingRect);
#endif
}

void CompositeLayerGroupItem::synchWithTileLayers()
{
//    if (layerGroup()->needsSynch())
        layerGroup()->synch();
    update();
}

void CompositeLayerGroupItem::updateBounds()
{
    QRectF bounds = layerGroup()->boundingRect(mRenderer);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

///// ///// ///// ///// /////

ZomboidScene::ZomboidScene(QObject *parent)
    : MapScene(parent)
    , mPendingActive(false)
    , mDnDItem(nullptr)
    , mWasHighlightCurrentLayer(false)
    , mMapBordersItem(new QGraphicsPolygonItem)
    , mMapBordersItem2(new QGraphicsPolygonItem)
    , mMapBuildings(new MapBuildings)
    , mMapBuildingsInvalid(true)
{
    connect(&mLotManager, QOverload<MapComposite*,MapObject*>::of(&ZLotManager::lotAdded),
        this, &ZomboidScene::onLotAdded);
    connect(&mLotManager, QOverload<MapComposite*,MapObject*>::of(&ZLotManager::lotRemoved),
        this, &ZomboidScene::onLotRemoved);
    connect(&mLotManager, QOverload<MapComposite*,MapObject*>::of(&ZLotManager::lotUpdated),
        this, QOverload<MapComposite*,MapObject*>::of(&ZomboidScene::onLotUpdated));

    connect(&mLotManager, QOverload<MapComposite*,WorldCellLot*>::of(&ZLotManager::lotAdded),
            this, QOverload<MapComposite*,WorldCellLot*>::of(&ZomboidScene::onLotUpdated));
    connect(&mLotManager, QOverload<MapComposite*,WorldCellLot*>::of(&ZLotManager::lotRemoved),
            this, QOverload<MapComposite*,WorldCellLot*>::of(&ZomboidScene::onLotUpdated));
    connect(&mLotManager, QOverload<MapComposite*,WorldCellLot*>::of(&ZLotManager::lotUpdated),
            this, QOverload<MapComposite*,WorldCellLot*>::of(&ZomboidScene::onLotUpdated));

    QPen pen(QColor(128, 128, 128, 128));
    pen.setWidth(28); // only good for isometric 64x32 tiles!
    pen.setJoinStyle(Qt::MiterJoin);
    mMapBordersItem->setPen(pen);
    mMapBordersItem->setZValue(20000 - 1); // ZVALUE_GRID - 1
#if 1
    mMapBordersItem2->setVisible(false);
#else
    pen = QPen();
    pen.setWidth(2);
    pen.setCosmetic(true);
    mMapBordersItem2->setPen(pen);
    addItem(mMapBordersItem2);
#endif
}

ZomboidScene::~ZomboidScene()
{
    mLotManager.disconnect(this);
    delete mMapBuildings;
}

void ZomboidScene::setMapDocument(MapDocument *mapDoc)
{
    MapScene::setMapDocument(mapDoc);
    mLotManager.setMapDocument(mapDocument());

    if (mapDocument()) {
        connect(mMapDocument, &MapDocument::regionAltered,
                this, &ZomboidScene::regionAltered);
        connect(mMapDocument, &MapDocument::layerGroupAdded, this, &ZomboidScene::layerGroupAdded);
        connect(mMapDocument, &MapDocument::layerGroupVisibilityChanged, this, &ZomboidScene::layerGroupVisibilityChanged);
        connect(mMapDocument, &MapDocument::layerAddedToGroup, this, &ZomboidScene::layerAddedToGroup);
        connect(mMapDocument, &MapDocument::layerRemovedFromGroup, this, &ZomboidScene::layerRemovedFromGroup);
        connect(mMapDocument, &MapDocument::layerLevelChanged, this, &ZomboidScene::layerLevelChanged);
        connect(mMapDocument, &MapDocument::mapCompositeChanged,
                this, &ZomboidScene::mapCompositeChanged);

        connect(mMapDocument, &MapDocument::objectsAdded,
                this, &ZomboidScene::invalidateMapBuildings);
        connect(mMapDocument, &MapDocument::objectsRemoved,
                this, &ZomboidScene::invalidateMapBuildings);
        connect(mMapDocument, &MapDocument::objectsChanged,
                this, &ZomboidScene::invalidateMapBuildings);

        connect(mMapDocument->mapComposite()->bmpBlender(), &BmpBlender::layersRecreated,
                this, &ZomboidScene::bmpBlenderLayersRecreated);
        connect(mMapDocument, &MapDocument::bmpPainted, this, &ZomboidScene::bmpPainted);
        connect(mMapDocument, &MapDocument::bmpAliasesChanged, this, &ZomboidScene::bmpXXXChanged);
        connect(mMapDocument, &MapDocument::bmpRulesChanged, this, &ZomboidScene::bmpXXXChanged);
        connect(mMapDocument, &MapDocument::bmpBlendsChanged, this, &ZomboidScene::bmpXXXChanged);

        connect(mMapDocument, &MapDocument::noBlendPainted, this, &ZomboidScene::noBlendPainted);
        connect(mMapDocument, &MapDocument::currentLayerIndexChanged, this, &ZomboidScene::synchNoBlendVisible);
        connect(ToolManager::instance(), &ToolManager::selectedToolChanged, this, &ZomboidScene::synchNoBlendVisible);

        connect(Preferences::instance(), &Preferences::highlightRoomUnderPointerChanged,
                this, &ZomboidScene::highlightRoomUnderPointerChanged);
        connect(Preferences::instance(), &Preferences::showLotFloorsOnlyChanged, this, &ZomboidScene::showLotFloorsOnlyChanged);
    }
}

void ZomboidScene::regionAltered(const QRegion &region, Layer *layer)
{
    // Painting tiles may update the draw margins of a layer.
    if (TileLayer *tl = layer->asTileLayer()) {
        // The drawMargins will only change the first time painting occurs
        // in an empty layer.
        if (tl->group() && mTileLayerGroupItems.contains(tl->level())) {
            if (mTileLayerGroupItems[tl->level()]->layerGroup()->regionAltered(tl))
                updateLayerGroupLater(tl->level(), Synch | Bounds); // recalculate CompositeLayerGroup::mDrawMargins
        } else {
            // TileLayer not part of a layer group.
            MapLevel *mapLevel = mapDocument()->map()->levelAt(layer->level());
            int layerIndex = mapLevel->layers().indexOf(tl);
            TileLayerItem *item = dynamic_cast<TileLayerItem*>(mLevelData[tl->level()].mLayerItems[layerIndex]);
            QRectF r = item->boundingRect();
            item->syncWithTileLayer();
            if (r != item->boundingRect())
                doLater(Bounds);
        }
    }

    MapScene::regionChanged(region, layer);
}

void ZomboidScene::updateLayerGroupsLater(PendingFlags flags)
{
    if (flags & Synch) {
        foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems)
            item->layerGroup()->setNeedsSynch(true);
    }
    doLater(AllGroups | flags);
}

void ZomboidScene::updateLayerGroupLater(int level, PendingFlags flags)
{
    if (mTileLayerGroupItems.contains(level)) {
        CompositeLayerGroupItem *item = mTileLayerGroupItems[level];
        if (!mPendingGroupItems.contains(item))
            mPendingGroupItems += item;
        if (flags & Synch)
            item->layerGroup()->setNeedsSynch(true);
        doLater(flags);
    }
}

void ZomboidScene::refreshScene()
{
    qDeleteAll(mTileLayerGroupItems); // QGraphicsScene.clear() will delete these actually
    mTileLayerGroupItems.clear();

    if (mMapBordersItem->scene()) {
        removeItem(mMapBordersItem);
        removeItem(mMapBordersItem2);
    }

    MapScene::refreshScene();

    addItem(mMapBordersItem);
    addItem(mMapBordersItem2);

    updateLayerGroupsLater(Synch | Bounds | ZOrder);
}

void ZomboidScene::mapChanged()
{
    MapScene::mapChanged();

    updateLayerGroupsLater(Bounds);
}

class DummyGraphicsItem : public QGraphicsItem
{
public:
    DummyGraphicsItem()
        : QGraphicsItem()
    {
        // Since we don't do any painting, we can spare us the call to paint()
        setFlag(QGraphicsItem::ItemHasNoContents);
    }
        
    // QGraphicsItem
    QRectF boundingRect() const
    {
        return QRectF();
    }
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0)
    {
        Q_UNUSED(painter)
        Q_UNUSED(option)
        Q_UNUSED(widget)
    }
};

QGraphicsItem *ZomboidScene::createLayerItem(Layer *layer)
{
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group()) {
            if (!mTileLayerGroupItems[tl->level()]) {
                mTileLayerGroupItems[tl->level()] = new CompositeLayerGroupItem((CompositeLayerGroup*)tl->group(),
                                                                                mMapDocument->renderer());
                addItem(mTileLayerGroupItems[tl->level()]);
                mMapDocument->renderer()->setMaxLevel(mMapDocument->mapComposite()->maxLevel());
                updateLayerGroupLater(tl->level(), Bounds);
            }
            return new DummyGraphicsItem();
        }
    }
    return MapScene::createLayerItem(layer);
}

void ZomboidScene::updateCurrentLayerHighlight()
{
    if (!mMapDocument)
        return;

    Layer *currentLayer = mMapDocument->currentLayer();
    int currentLayerIndex = mMapDocument->currentLayerIndex();

    if (currentLayer) {
        int level = currentLayer->level();
        if (mTileLayerGroupItems.contains(level)) {
            CompositeLayerGroup *layerGroup = mTileLayerGroupItems[level]->layerGroup();
            if (layerGroup->layerCount()) {
                currentLayer = layerGroup->layers().first();
                MapLevel *mapLevel = mMapDocument->map()->levelAt(level);
                currentLayerIndex = mapLevel->layers().indexOf(currentLayer);
            }
        }
    }

    if (!mHighlightCurrentLayer || !currentLayer) {
        mDarkRectangle->setVisible(false);

        // Restore visibility for all non-ZTileLayerGroupItem layers
        for (int z = 0; z < mLevelData.size(); z++) {
            LevelData &levelData = mLevelData[z];
            for (int i = 0; i < levelData.mLayerItems.size(); ++i) {
                const Layer *layer = mMapDocument->map()->levelAt(z)->layerAt(i);
                levelData.mLayerItems.at(i)->setVisible(layer->isVisible());
            }
        }

        // Restore visibility for all ZTileLayerGroupItem layers
        for (CompositeLayerGroupItem *item : mTileLayerGroupItems) {
            item->setVisible(item->layerGroup()->isVisible());
        }

        return;
    }

    LevelData &levelData = mLevelData[currentLayer->level()];
    QGraphicsItem *currentItem = levelData.mLayerItems[currentLayerIndex];
    if (currentLayer->asTileLayer() && currentLayer->asTileLayer()->group()) {
        Q_ASSERT(mTileLayerGroupItems.contains(currentLayer->level()));
        if (mTileLayerGroupItems.contains(currentLayer->level()))
            currentItem = mTileLayerGroupItems[currentLayer->level()];
    }

    // Hide items above the current item
    for (int z = 0; z < mLevelData.size(); z++) {
        LevelData &levelData2 = mLevelData[z];
        MapLevel *mapLevel = mMapDocument->map()->levelAt(z);
        for (int index = 0; index < levelData2.mLayerItems.size(); index++) {
            QGraphicsItem *item = levelData2.mLayerItems[index];
            Layer *layer = mapLevel->layerAt(index);
            bool visible = layer->isVisible() && (layer->level() <= currentLayer->level());
            if (layer->isObjectGroup() && (layer->level() != currentLayer->level())) {
                visible = false;
            }
            item->setVisible(visible);
        }
    }
    for (CompositeLayerGroupItem *item : mTileLayerGroupItems) {
        bool visible = item->layerGroup()->isVisible() && (item->layerGroup()->level() <= currentLayer->level());
        item->setVisible(visible);
    }

    // Darken layers below the current item
    mDarkRectangle->setZValue(currentItem->zValue() - 0.5);
    mDarkRectangle->setVisible(true);
}

void ZomboidScene::layerAdded(int z, int index)
{
    MapScene::layerAdded(z, index);

    if (mHighlightCurrentLayer) {
        if (ObjectGroup *og = mMapDocument->map()->layerAt(z, index)->asObjectGroup()) {
            int level = og->level();
            mLevelData[z].mLayerItems[index]->setVisible(og->isVisible() &&
                                           (level == mMapDocument->currentLevelIndex()));
        }
    }

    mMapBuildingsInvalid = true;

    doLater(ZOrder);
}

void ZomboidScene::layerRemoved(int z, int index)
{
    MapScene::layerRemoved(z, index);

    mMapBuildingsInvalid = true;

    doLater(ZOrder);
}

/**
 * A layer has changed. This can mean that the layer visibility, opacity or
 * name has changed.
 */
void ZomboidScene::layerChanged(int z, int index)
{
    MapScene::layerChanged(z, index);
    Layer *layer = mMapDocument->map()->layerAt(z, index);
    if (TileLayer *tl = layer->asTileLayer()) {
        // Changing the name of a layer affects MapComposite::mBmpBlendLayers.
        if (!tl->level() && mapDocument()->mapComposite()->layerGroupForLevel(0)->setBmpBlendLayers(
                    mapDocument()->mapComposite()->bmpBlender()->tileLayers())) {
            updateLayerGroupLater(0, Synch | Bounds);
        }
        if (tl->group() && mTileLayerGroupItems.contains(tl->level())) {
            CompositeLayerGroupItem *layerGroupItem = mTileLayerGroupItems[tl->level()];
            if (layerGroupItem->layerGroup()->setLayerVisibility(tl, tl->isVisible()))
                updateLayerGroupLater(tl->level(), Synch | Bounds);
            if (layerGroupItem->layerGroup()->setLayerOpacity(tl, tl->opacity())) {
                layerGroupItem->layerGroup()->synchSubMapLayerOpacity(tl->name(), tl->opacity());
                updateLayerGroupLater(tl->level(), Paint);
            }
        }
    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        bool synch = false;
        for (MapObject *mo : og->objects()) {
            if (mMapObjectToLot.contains(mo)) {
                // FIXME: layerVisibilityChanged() signal please
                if (mMapObjectToLot[mo]->isGroupVisible() != og->isVisible()) {
                    mMapObjectToLot[mo]->setGroupVisible(og->isVisible());
                    synch = true;
                }
            }
        }
        if (mHighlightCurrentLayer && (index < mLevelData[z].mLayerItems.size())) {
            QGraphicsItem *layerItem = mLevelData[z].mLayerItems.at(index);
            layerItem->setVisible(og->isVisible()
                                  && (og->level() == mMapDocument->currentLevelIndex()));
        }
        mMapBuildingsInvalid = true;
        if (synch)
            updateLayerGroupsLater(Synch | Bounds);
    }
#if 0
    const Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = mLayerItems.at(index);

    layerItem->setVisible(layer->isVisible());

    qreal multiplier = 1;
    if (mHighlightCurrentLayer && mMapDocument->currentLayerIndex() < index)
        multiplier = opacityFactor;

    layerItem->setOpacity(layer->opacity() * multiplier);
#endif
}

void ZomboidScene::layerGroupAdded(int level)
{
    if (!mTileLayerGroupItems.contains(level)) {
        CompositeLayerGroup *layerGroup = mMapDocument->mapComposite()->tileLayersForLevel(level);
        mTileLayerGroupItems[level] = new CompositeLayerGroupItem((CompositeLayerGroup*)layerGroup,
                                                                  mMapDocument->renderer());
        addItem(mTileLayerGroupItems[level]);
        mMapDocument->renderer()->setMaxLevel(mMapDocument->mapComposite()->maxLevel());
        updateLayerGroupLater(level, Synch | Bounds | ZOrder);
    }

    // Setting a new maxLevel() for a map resizes the scene, requiring all existing items to be repositioned.
    if (level == mMapDocument->mapComposite()->maxLevel()) {
        mMapDocument->renderer()->setMaxLevel(level);
        mapChanged();
    }
}

void ZomboidScene::layerGroupVisibilityChanged(CompositeLayerGroup *g)
{
    if (mTileLayerGroupItems.contains(g->level())) {
        mTileLayerGroupItems[g->level()]->setVisible(g->isVisible());
        updateLayerGroupLater(g->level(), Synch | Bounds);
    }
}

void ZomboidScene::layerAddedToGroup(int z, int index)
{
    Layer *layer = mMapDocument->map()->layerAt(z, index);
    Q_ASSERT(layer->isTileLayer());
    int level = layer->level();
    if (mTileLayerGroupItems.contains(level))
        updateLayerGroupLater(level, Synch | Bounds);

    if (!level)
        mapDocument()->mapComposite()->layerGroupForLevel(0)->setBmpBlendLayers(
                    mapDocument()->mapComposite()->bmpBlender()->tileLayers());

    // If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
    // managed by the base class.
    // If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
    // managed by the base class (MapScene) See createLayerItem().
    delete mLevelData[z].mLayerItems[index]; // TileLayerItem
    mLevelData[z].mLayerItems[index] = new DummyGraphicsItem();
    mLevelData[z].mLayerItems[index]->setVisible(layer->isVisible());
    addItem(mLevelData[z].mLayerItems[index]);

    doLater(ZOrder);
}

void ZomboidScene::layerRemovedFromGroup(int z, int index, CompositeLayerGroup *oldGroup)
{
    Q_UNUSED(oldGroup)
    Layer *layer = mMapDocument->map()->layerAt(z, index);
    Q_ASSERT(layer->isTileLayer());
    TileLayer *tl = layer->asTileLayer();

    int level = layer->level(); // STILL VALID FOR GROUP IT WAS IN
    if (mTileLayerGroupItems.contains(level))
        updateLayerGroupLater(level, Synch | Bounds);

    // If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
    // managed by the base class.
    // If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
    // managed by the base class (MapScene) See createLayerItem().
    delete mLevelData[z].mLayerItems[index]; // DummyGraphicsItem
    mLevelData[z].mLayerItems[index] = new TileLayerItem(tl, mMapDocument->renderer());
    mLevelData[z].mLayerItems[index]->setVisible(tl->isVisible());
    mLevelData[z].mLayerItems[index]->setOpacity(tl->opacity());
    addItem(mLevelData[z].mLayerItems[index]);

    doLater(ZOrder);
}

void ZomboidScene::layerLevelChanged(int z, int index, int oldLevel)
{
    Q_UNUSED(oldLevel)
    Layer *layer = mMapDocument->map()->layerAt(z, index);

    if (/*TileLayer *tl =*/ layer->asTileLayer()) {

    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        bool synch = false;
        for (MapObject *mapObject : og->objects()) {
            if (mMapObjectToLot.contains(mapObject)) {
                MapComposite *lot = mMapObjectToLot[mapObject];
                lot->setGroupVisible(og->isVisible());
                lot->setLevel(og->level());
                mMapDocument->mapComposite()->ensureMaxLevels(lot->levelOffset() + lot->maxLevel());
                // Recalculate the MapObject bounds
                onLotUpdated(lot, mapObject);
                synch = true;
            }
            mLevelData[z].mObjectItems[mapObject]->syncWithMapObject();
        }
        if (synch)
            updateLayerGroupsLater(Synch | Bounds);
    } else {
        // ImageLayer
        mLevelData[z].mLayerItems[index]->update();
    }

    mGridItem->currentLayerIndexChanged(); // index didn't change, just updating the bounds
}

void ZomboidScene::doLater(PendingFlags flags)
{
#if 1
    mPendingFlags |= flags;
    handlePendingUpdates();
#else
    mPendingFlags |= flags;
    if (mPendingActive)
        return;
    QMetaObject::invokeMethod(this, "handlePendingUpdates",
                              Qt::QueuedConnection);
    mPendingActive = true;
#endif
}

// Determine sane Z-order for layers in and out of TileLayerGroups
void ZomboidScene::setGraphicsSceneZOrder()
{
    MapComposite::ZOrderList zorder = mMapDocument->mapComposite()->zOrder();
    int z = 0;
    for (MapComposite::ZOrderItem zo : zorder) {
        if (zo.group) {
            int level = zo.group->level();
            if (mTileLayerGroupItems.contains(level))
                mTileLayerGroupItems[level]->setZValue(z);
        } else if (mLevelData[zo.layer->level()].mLayerItems.size() > zo.layerIndex) {
            if (QGraphicsItem * layerItem = mLevelData[zo.layer->level()].mLayerItems[zo.layerIndex]) {
                layerItem->setZValue(z);
            }
        }
        ++z;
    }
}

QRegion ZomboidScene::getBuildingRegion(const QPoint &tilePos, QRegion &roomRgn)
{
    if (mMapBuildingsInvalid) {
        mMapBuildings->calculate(mMapDocument->mapComposite());
        mMapBuildingsInvalid = false;
    }
    if (MapBuildingsNS::Room *room = mMapBuildings->roomAt(tilePos,
                                                           mMapDocument->currentLevelIndex())) {
        roomRgn = room->region();
        return room->building->region();
    }
    return QRegion();
}

void ZomboidScene::setHighlightRoomPosition(const QPoint &tilePos)
{
    MapComposite *mc = mMapDocument->mapComposite();
    int level = mMapDocument->currentLevelIndex();
    QRegion buildingRgn, roomRgn;
    if (Preferences::instance()->highlightRoomUnderPointer())
        buildingRgn = getBuildingRegion(tilePos, roomRgn);
    if (buildingRgn - roomRgn != mc->suppressRegion() ||
            level != mc->suppressLevel()) {
        mc->setSuppressRegion(buildingRgn - roomRgn, level);
        update();
    }
    mHighlightRoomPosition = tilePos;
}

void ZomboidScene::onLotAdded(MapComposite *lot, MapObject *mapObject)
{
    mMapObjectToLot[mapObject] = lot;

    MapObjectItem *item = itemForObject(mapObject); // FIXME: assumes createLayerItem() was called before this
    if (item) {
        item->setLot(lot);

        // Resize the map object to the size of the lot's map, and snap-to-grid
        mapObject->setPosition(lot->origin());
        item->resize(lot->map()->size());

        MapImage *mapImage = MapImageManager::instance()->getMapImage(lot->mapInfo()->path());
        item->setMapImage(mapImage);
    }

    mMapBuildingsInvalid = true;

    updateLayerGroupsLater(Synch | Bounds);
}

void ZomboidScene::onLotRemoved(MapComposite *lot, MapObject *mapObject)
{
    // NB: 'lot' is deleted already
    Q_UNUSED(lot)
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        item->setLot(nullptr);
        item->setMapImage(nullptr);
    }
    mMapObjectToLot.remove(mapObject);

    mMapBuildingsInvalid = true;

    updateLayerGroupsLater(Synch | Bounds);
}

void ZomboidScene::onLotUpdated(MapComposite *lot, MapObject *mapObject)
{
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        // Resize the map object to the size of the lot's map, and snap-to-grid
        mapObject->setPosition(lot->origin());
        item->resize(lot->map()->size());
    }

    mMapBuildingsInvalid = true;

    updateLayerGroupsLater(Synch | Bounds);
}

void ZomboidScene::onLotUpdated(MapComposite *mc, WorldCellLot *lot)
{
    Q_UNUSED(mc)
    Q_UNUSED(lot)
    mMapBuildingsInvalid = true;
    updateLayerGroupsLater(Synch | Bounds);
}

void ZomboidScene::invalidateMapBuildings()
{
    mMapBuildingsInvalid = true;
}

// Called when a map file displayed as a Lot is changed on disk.
void ZomboidScene::mapCompositeChanged()
{
    QMap<MapObject*,MapComposite*>::iterator it_begin = mMapObjectToLot.begin();
    QMap<MapObject*,MapComposite*>::iterator it_end = mMapObjectToLot.end();
    QMap<MapObject*,MapComposite*>::iterator it;
    for (it = it_begin; it != it_end; it++) {
        MapObject *mapObject = it.key();
        MapComposite *lot = it.value();
        if (MapObjectItem *item = itemForObject(mapObject))
            item->resize(lot->map()->size());
    }
    mMapBuildingsInvalid = true;
    updateLayerGroupsLater(Synch | Bounds);
}

void ZomboidScene::bmpBlenderLayersRecreated()
{
    if (mTileLayerGroupItems.contains(0))
        updateLayerGroupLater(0, Synch | Bounds);
}

void ZomboidScene::bmpPainted(int bmpIndex, const QRegion &region)
{
    Q_UNUSED(bmpIndex)
    const MapRenderer *renderer = mMapDocument->renderer();
    const QMargins margins = mMapDocument->map()->drawMargins();

    for (const QRect &r : region) {
        update(renderer->boundingRect(r, 0).adjusted(-margins.left(),
                                                     -margins.top(),
                                                     margins.right(),
                                                     margins.bottom()));
    }
}

void ZomboidScene::bmpXXXChanged()
{
    if (mTileLayerGroupItems.contains(0))
        updateLayerGroupLater(0, Synch | Bounds);
}

void ZomboidScene::noBlendPainted(MapNoBlend *noBlend, const QRegion &rgn)
{
    Q_UNUSED(noBlend)
    bmpPainted(0, rgn);
}

void ZomboidScene::synchNoBlendVisible()
{
    QString layerName;
    if (mapDocument()->currentLayer() && mapDocument()->currentLayer()->asTileLayer()) {
        layerName = mapDocument()->currentLayer()->name();
        if (!mapDocument()->mapComposite()->bmpBlender()->blendLayers().contains(layerName))
            layerName.clear();
    }
    if (NoBlendTool::instance() != ToolManager::instance()->selectedTool())
        layerName.clear();
    if (layerName != mapDocument()->mapComposite()->noBlendLayer()) {
        mapDocument()->mapComposite()->setNoBlendLayer(layerName);
        update();
    }
}

void ZomboidScene::highlightRoomUnderPointerChanged(bool highlight)
{
    Q_UNUSED(highlight)
    setHighlightRoomPosition(mHighlightRoomPosition);
}

void ZomboidScene::showLotFloorsOnlyChanged(bool show)
{
    MapComposite *mc = mMapDocument->mapComposite();
    mc->setShowLotFloorsOnly(show);
    update();
}

void ZomboidScene::handlePendingUpdates()
{
    MapComposite *mapComposite = mMapDocument->mapComposite();
    if (mTileLayerGroupItems.size() != mapComposite->layerGroupCount()) {
        foreach (CompositeLayerGroup *layerGroup, mapComposite->layerGroups()) {
            int level = layerGroup->level();
            if (!mTileLayerGroupItems.contains(level)) {
                mTileLayerGroupItems[level]
                        = new CompositeLayerGroupItem(layerGroup,
                                                      mMapDocument->renderer());
                addItem(mTileLayerGroupItems[level]);
                mPendingFlags |= ZOrder;
            }
        }
        mMapDocument->renderer()->setMaxLevel(mapComposite->maxLevel());
    }

    if (mPendingFlags & AllGroups)
        mPendingGroupItems = mTileLayerGroupItems.values();
    if (mPendingFlags & Synch) {
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->synchWithTileLayers();
    }
    if (mPendingFlags & Bounds) {
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->updateBounds();
        QRectF sceneRect = mMapDocument->mapComposite()->boundingRect(mMapDocument->renderer());
        if (sceneRect != this->sceneRect()) {
            MapScene::mapChanged(); // must reposition items
//            setSceneRect(sceneRect);
//            mDarkRectangle->setRect(sceneRect);
        }
        QPolygonF polygon;
        QRectF rect(0 - 0.5, 0 - 0.5,
                    mapDocument()->map()->width() + 1.0,
                    mapDocument()->map()->height() + 1.0);
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.topLeft()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.topRight()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.bottomRight()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.bottomLeft()));
        mMapBordersItem->setPolygon(polygon);
        mMapBordersItem->setVisible(mapDocument()->map()->size() == QSize(300, 300));
#if 0
        rect = QRect(0, 0,
                     mapDocument()->map()->width(),
                     mapDocument()->map()->height());
        polygon.clear();
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.topLeft()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.topRight()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.bottomRight()));
        polygon << QPointF(mapDocument()->renderer()->tileToPixelCoords(rect.bottomLeft()));
        mMapBordersItem2->setPolygon(polygon);
#endif
    }
    if (mPendingFlags & ZOrder)
        setGraphicsSceneZOrder();
    if (mPendingFlags & Paint) {
        foreach (CompositeLayerGroupItem *item, mPendingGroupItems)
            item->update();
    }
    if (mPendingFlags & Highlight)
        updateCurrentLayerHighlight();

    mPendingFlags = None;
    mPendingGroupItems.clear();
    mPendingActive = false;
}

void ZomboidScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
   MapScene::mouseMoveEvent(mouseEvent);

   QPoint tilePos = mapDocument()->renderer()->pixelToTileCoordsInt(mouseEvent->scenePos(),
                                                                    mapDocument()->currentLevelIndex());
   if (tilePos != mHighlightRoomPosition)
       setHighlightRoomPosition(tilePos);
}

#include "preferences.h"
#include "addremovemapobject.h"
#include <QFileInfo>
#include <QMimeData>
#include <QStringList>
#include <qgraphicssceneevent.h>
#include <QUrl>

/////


/**
 * Item that represents a map during drag-and-drop.
 */
class DnDItem : public QGraphicsItem
{
public:
    DnDItem(const QString &path, Tiled::MapRenderer *renderer, int level, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *);

    QPainterPath shape() const;

    void setTilePosition(QPoint tilePos);

    void setHotSpot(const QPoint &pos);
    void setHotSpot(int x, int y) { setHotSpot(QPoint(x, y)); }
    QPoint hotSpot() { return mHotSpot; }

    QPoint dropPosition();

    MapInfo *mapInfo();

private:
    MapImage *mMapImage;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
    QPoint mPositionInMap;
    QPoint mHotSpot;
    int mLevel;
};

/////

DnDItem::DnDItem(const QString &path, MapRenderer *renderer, int level, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mMapImage(MapImageManager::instance()->getMapImage(path))
    , mRenderer(renderer)
    , mLevel(level)
{
    setHotSpot(mMapImage->mapInfo()->width() / 2, mMapImage->mapInfo()->height() / 2);
}

QRectF DnDItem::boundingRect() const
{
    return mBoundingRect;
}

void DnDItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    painter->setOpacity(0.5);
    QRectF target = mBoundingRect;
    QRectF source = QRect(QPoint(0, 0), mMapImage->image().size());
    painter->drawImage(target, mMapImage->image(), source);
    painter->setOpacity(effectiveOpacity());

    QRect tileBounds(mPositionInMap.x() - mHotSpot.x(), mPositionInMap.y() - mHotSpot.y(),
                     mMapImage->mapInfo()->width(), mMapImage->mapInfo()->height());
    mRenderer->drawFancyRectangle(painter, tileBounds, Qt::darkGray, mLevel);

#ifdef _DEBUG
    painter->drawRect(mBoundingRect);
#endif
}

QPainterPath DnDItem::shape() const
{
    // FIXME: need polygon
    return QGraphicsItem::shape();
}

void DnDItem::setTilePosition(QPoint tilePos)
{
    mPositionInMap = tilePos;
    QRectF bounds;
    QSize scaledImageSize(mMapImage->image().size() / mMapImage->scale());
    bounds = QRectF(-mMapImage->tileToImageCoords(mHotSpot) / mMapImage->scale(), scaledImageSize);
    bounds.translate(mRenderer->tileToPixelCoords(mPositionInMap, mLevel));
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

void DnDItem::setHotSpot(const QPoint &pos)
{
    // Position the item so that the top-left corner of the hotspot tile is at the item's origin
    mHotSpot = pos;
    QSize scaledImageSize(mMapImage->image().size() / mMapImage->scale());
    mBoundingRect = QRectF(-mMapImage->tileToImageCoords(mHotSpot) / mMapImage->scale(), scaledImageSize);
}

QPoint DnDItem::dropPosition()
{
    return mPositionInMap - mHotSpot;
}

MapInfo *DnDItem::mapInfo()
{
    return mMapImage->mapInfo();
}

/////

void ZomboidScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    Layer *layer = mMapDocument->currentLayer();
    if (!layer) {
        event->ignore();
        return;
    }
    ObjectGroup *objectGroup = layer->asObjectGroup();
    if (!objectGroup) {
        event->ignore();
        return;
    }

    foreach (const QUrl &url, event->mimeData()->urls()) {
        QFileInfo info(url.toLocalFile());
        if (!info.exists()) continue;
        if (!info.isFile()) continue;
        if (info.suffix() != QLatin1String("tmx") &&
                info.suffix() != QLatin1String("tbx")) continue;
        if (!MapManager::instance()->mapInfo(info.canonicalFilePath()))
            continue;

        QString path = info.canonicalFilePath();
        MapRenderer *renderer = mMapDocument->renderer();
        mDnDItem = new DnDItem(path, renderer, objectGroup->level());
        QPoint tilePos = renderer->pixelToTileCoords(event->scenePos(), objectGroup->level()).toPoint();
        mDnDItem->setTilePosition(tilePos);
        addItem(mDnDItem);
        mDnDItem->setZValue(10001);

        mWasHighlightCurrentLayer = mHighlightCurrentLayer;
        if (!mWasHighlightCurrentLayer)
            Preferences::instance()->setHighlightCurrentLayer(true);
        else
            updateCurrentLayerHighlight();

        event->accept();
        return;
    }

    event->ignore();
}

void ZomboidScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    if (mDnDItem) {
        int level = mMapDocument->currentLevelIndex();
        QPoint tilePos = mMapDocument->renderer()->pixelToTileCoords(event->scenePos(), level).toPoint();
        mDnDItem->setTilePosition(tilePos);
    }
}

void ZomboidScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (mDnDItem) {
        delete mDnDItem;
        mDnDItem = 0;

        if (!mWasHighlightCurrentLayer)
            Preferences::instance()->setHighlightCurrentLayer(false);
        else
            updateCurrentLayerHighlight();
    }
}

void ZomboidScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    Layer *layer = mMapDocument->currentLayer();
    if (!layer) return;
    ObjectGroup *objectGroup = layer->asObjectGroup();
    if (!objectGroup) return;

    if (mDnDItem) {

        QString mapName = mDnDItem->mapInfo()->path();
        MapObject *newMapObject = new MapObject(QLatin1String("lot"),
                                                mapName,
                                                mDnDItem->dropPosition(),
                                                mDnDItem->mapInfo()->size());
        delete mDnDItem;
        mDnDItem = nullptr;

        if (!mWasHighlightCurrentLayer)
            Preferences::instance()->setHighlightCurrentLayer(false);
        else
            updateCurrentLayerHighlight();

        mapDocument()->undoStack()->push(new AddMapObject(mapDocument(),
                                                          objectGroup,
                                                          newMapObject));
    }
}
