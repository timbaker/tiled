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

#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "zgriditem.hpp"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tilelayeritem.h"
#include "zlevelsmodel.hpp"
#include "zlot.hpp"
#include "zlotmanager.hpp"

using namespace Tiled;
using namespace Tiled::Internal;

QRectF layerGroupSceneBounds(Map *map, const ZTileLayerGroup *layerGroup, const MapRenderer *renderer, int levelOffset, const QPoint &offset)
{
    QRect r = layerGroup->bounds().translated(offset);
    QRectF bounds = renderer->boundingRect(r, layerGroup->level() + levelOffset);

    QMargins m = map->drawMargins(); /* layerGroup->drawMargins(); */
    bounds.adjust(-m.left(), -m.top(), m.right(), m.bottom());
    return bounds;
}

///// ///// ///// ///// /////

// from map.cpp
static void maxMargins(const QMargins &a,
                           const QMargins &b,
                           QMargins &out)
{
    out.setLeft(qMax(a.left(), b.left()));
    out.setTop(qMax(a.top(), b.top()));
    out.setRight(qMax(a.right(), b.right()));
    out.setBottom(qMax(a.bottom(), b.bottom()));
}

ZomboidTileLayerGroup::ZomboidTileLayerGroup(ZomboidScene *mapScene, int level)
    : ZTileLayerGroup(level)
    , mMapScene(mapScene)
    , mLevel(level)
    , mAnyVisibleLayers(false)
{
}

void ZomboidTileLayerGroup::addTileLayer(TileLayer *layer, int index)
{
    ZTileLayerGroup::addTileLayer(layer, index);
    synch();
}

void ZomboidTileLayerGroup::removeTileLayer(TileLayer *layer)
{
    ZTileLayerGroup::removeTileLayer(layer);
    synch();
}

void ZomboidTileLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedLotLayers.clear();
    if (mAnyVisibleLayers == false)
        return;
    foreach (const LotLayers &lotLayer, mVisibleLotLayers) {
        const ZTileLayerGroup *layerGroup = lotLayer.mLayerGroup;
        const MapObject *mapObject = lotLayer.mMapObject;
        QPoint mapObjectPos = mapObject->position().toPoint();
        int levelOffset = mapObject->objectGroup()->level();
        QRectF bounds = layerGroupSceneBounds(lotLayer.mLot->map(), layerGroup, renderer, levelOffset, mapObjectPos);
        if ((bounds & rect).isValid()) {
            // Set the visibility of lot map layers to match this layer-group's layers.
            // Layers in the lot that don't exist in the edited map are always shown.
            foreach (Layer *layer, lotLayer.mLayerGroup->mLayers)
                layer->setVisible(true);
            foreach (Layer *layer, mLayers)
                lotLayer.mLot->setLayerVisibility(layer->name(), layer->isVisible());
            mPreparedLotLayers.append(lotLayer);
        }
    }
}

bool ZomboidTileLayerGroup::orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const
{
    cells.clear();
    foreach (TileLayer *tl, mLayers) {
        if (!tl->isVisible())
            continue;
#if 0 // DO NOT USE - VERY SLOW
        if (tl->isEmpty())
            continue;
#endif
        QPoint pos = point - tl->position();
        if (tl->contains(pos)) {
            const Cell *cell = &tl->cellAt(pos);
            if (!cell->isEmpty())
                cells.append(cell);
        }
    }

    // Overwrite map cells with .lot cells at this location
#if 1
    foreach (const LotLayers& lotLayer, mPreparedLotLayers) {
        lotLayer.mLayerGroup->orderedCellsAt(point - lotLayer.mMapObject->position().toPoint(), cells);
    }
#else
    foreach (MapObject *mapObject, mMapScene->mLotMapObjects) {
        ZLot *lot = mMapScene->mMapObjectToLot[mapObject];
        QPoint mapObjectPos = mapObject->position().toPoint();
        lot->orderedCellsAt(mLevel, point - mapObjectPos, cells);
    }
#endif

    return !cells.empty();
}

void ZomboidTileLayerGroup::synch()
{
    QRect r;
    QMargins m;

    mAnyVisibleLayers = false;

    foreach (TileLayer *tl, mLayers) {
        if (tl->isVisible()) {
            r |= tl->bounds();
            maxMargins(m, tl->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
    }

    mTileBounds = r;

    r = QRect();
    mVisibleLotLayers.clear();
    if (mAnyVisibleLayers == true) {
        foreach (MapObject *mapObject, mMapScene->mLotMapObjects) {
            if (mapObject->objectGroup()->isVisible() == false)
                continue;
            if (mapObject->isVisible() == false)
                continue;
            ZLot *lot = mMapScene->mMapObjectToLot[mapObject];
            int levelOffset = mapObject->objectGroup()->level();
            const ZTileLayerGroup *layerGroup = lot->tileLayersForLevel(mLevel - levelOffset);
            if (layerGroup && !layerGroup->mLayers.isEmpty()) {
                mVisibleLotLayers.append(LotLayers(mapObject, lot, layerGroup));
                QPoint mapObjectPos = mapObject->position().toPoint();
                r |= QRect(layerGroup->bounds()).translated(mapObjectPos);
                maxMargins(m, layerGroup->drawMargins(), m);
            }
        }
    }

    mLotTileBounds = r;
    mDrawMargins = m;
}

QRect ZomboidTileLayerGroup::bounds() const
{
    return mTileBounds | mLotTileBounds;
}

QMargins ZomboidTileLayerGroup::drawMargins() const
{
    return mDrawMargins;
}

///// ///// ///// ///// /////

class ZomboidTileLayerGroupItem : public ZTileLayerGroupItem
{
public:
    ZomboidTileLayerGroupItem(ZomboidTileLayerGroup *layerGroup, MapDocument *mapDoc)
        : ZTileLayerGroupItem(layerGroup, mapDoc)
        , mZomboidLayerGroup(layerGroup)
    {
    }

    // ZTileLayerGroupItem
    virtual void syncWithTileLayers()
    {
        QRect lotTileBounds = mZomboidLayerGroup->mLotTileBounds;
        mZomboidLayerGroup->mLotTileBounds = QRect();
        ZTileLayerGroupItem::syncWithTileLayers();
        mZomboidLayerGroup->mLotTileBounds = lotTileBounds;

        foreach (const ZomboidTileLayerGroup::LotLayers &lotLayer, mZomboidLayerGroup->mVisibleLotLayers) {
            int levelOffset = lotLayer.mMapObject->objectGroup()->level();
            QPoint mapObjectPos = lotLayer.mMapObject->position().toPoint();
            QRectF bounds = layerGroupSceneBounds(lotLayer.mLot->map(),
                lotLayer.mLayerGroup, mMapDocument->renderer(), levelOffset, mapObjectPos);
            mBoundingRect |= bounds;
        }
    }

    ZomboidTileLayerGroup *mZomboidLayerGroup;
};

///// ///// ///// ///// /////

ZomboidScene::ZomboidScene(QObject *parent)
    : MapScene(parent)
    , mDnDMapObjectItem(0)
{
    connect(&mLotManager, SIGNAL(lotAdded(ZLot*,MapObject*)),
        this, SLOT(onLotAdded(ZLot*,MapObject*)));
    connect(&mLotManager, SIGNAL(lotRemoved(ZLot*,MapObject*)),
        this, SLOT(onLotRemoved(ZLot*,MapObject*)));
    connect(&mLotManager, SIGNAL(lotUpdated(ZLot*,MapObject*)),
        this, SLOT(onLotUpdated(ZLot*,MapObject*)));
}

ZomboidScene::~ZomboidScene()
{
    mLotManager.disconnect(this);

    qDeleteAll(mTileLayerGroups);
}

void ZomboidScene::setMapDocument(MapDocument *mapDoc)
{
    if (mapDocument() && (mapDocument() != mapDoc))
        mapDocument()->levelsModel()->disconnect(this, SLOT(layerGroupVisibilityChanged(ZTileLayerGroup*)));

    MapScene::setMapDocument(mapDoc);
    mLotManager.setMapDocument(mapDocument());

    if (mapDocument())
        connect(mapDocument()->levelsModel(), SIGNAL(layerGroupVisibilityChanged(ZTileLayerGroup*)),
            SLOT(layerGroupVisibilityChanged(ZTileLayerGroup*)));
}

// Painting tiles may update the draw margins of a layer.
void ZomboidScene::regionChanged(const QRegion &region, Layer *layer)
{
    if (TileLayer *tl = layer->asTileLayer())
        synchWithTileLayer(tl);

    MapScene::regionChanged(region, layer);
}

void ZomboidScene::synchWithTileLayers()
{
    foreach (ZTileLayerGroup *tlg, mTileLayerGroups)
        ((ZomboidTileLayerGroup*)tlg)->synch();
    foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems)
        item->syncWithTileLayers();
}

void ZomboidScene::synchWithTileLayer(TileLayer *tl)
{
    if (tl->group()) {
        ((ZomboidTileLayerGroup*)tl->group())->synch();
        if (mTileLayerGroupItems.contains(tl->level())) {
            ZTileLayerGroupItem *item = mTileLayerGroupItems[tl->level()];
            item->syncWithTileLayers();
        }
    }
}

void ZomboidScene::refreshScene()
{
    qDeleteAll(mTileLayerGroups);
    mTileLayerGroups.clear();
    qDeleteAll(mTileLayerGroupItems); // QGraphicsScene.clear() will delete these actually
    mTileLayerGroupItems.clear();

    Map *map =  mapDocument()->map();
    map->setMaxLevel(0);
    int index = 0;
    foreach (Layer *layer, map->layers()) {
        int level;
        if (levelForLayer(layer, &level)) {
            layer->setLevel(level); // for ObjectGroup,ImageLayer as well
            if (TileLayer *tl = layer->asTileLayer()) {
                if (mTileLayerGroups.contains(level) == false) {
                    mTileLayerGroups[level] = new ZomboidTileLayerGroup(this, level);
                    mapDocument()->levelsModel()->addTileLayerGroup(mTileLayerGroups[level]);
                }
                mapDocument()->levelsModel()->addLayerToGroup(mTileLayerGroups[level], tl); // mTileLayerGroups[level]->addTileLayer(tl, index);
            }
            if (level > map->maxLevel())
                // FIXME: this never goes down if whole levels are deleted, requires reload
                map->setMaxLevel(level);
        }
        ++index;
    }

    MapScene::refreshScene();

    setGraphicsSceneZOrder();
}

void ZomboidScene::mapChanged()
{
    MapScene::mapChanged();

    synchWithTileLayers();
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
            if (mTileLayerGroupItems[tl->level()] == 0) {
                mTileLayerGroupItems[tl->level()] = new ZomboidTileLayerGroupItem((ZomboidTileLayerGroup*)tl->group(), mMapDocument);
                addItem(mTileLayerGroupItems[tl->level()]);
            }
            mTileLayerGroupItems[tl->level()]->syncWithTileLayers();
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

    if (!mHighlightCurrentLayer || !currentLayer) {
        mDarkRectangle->setVisible(false);

        // Restore visibility for all non-ZTileLayerGroupItem layers
        for (int i = 0; i < mLayerItems.size(); ++i) {
            const Layer *layer = mMapDocument->map()->layerAt(i);
            mLayerItems.at(i)->setVisible(layer->isVisible());
        }

        // Restore visibility for all ZTileLayerGroupItem layers
        foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems) {
            item->setVisible(item->tileLayerGroup()->isVisible());
        }

        return;
    }

    QGraphicsItem *currentItem = mLayerItems[mMapDocument->currentLayerIndex()];
    if (currentLayer->asTileLayer() && currentLayer->asTileLayer()->group()) {
        Q_ASSERT(mTileLayerGroupItems.contains(currentLayer->level()));
        if (mTileLayerGroupItems.contains(currentLayer->level()))
            currentItem = mTileLayerGroupItems[currentLayer->level()];
    }

    // Hide items above the current item
    int index = 0;
    foreach (QGraphicsItem *item, mLayerItems) {
        Layer *layer = mMapDocument->map()->layerAt(index);
        if (!layer->isTileLayer()) continue; // leave ObjectGroups alone
        bool visible = layer->isVisible() && (item->zValue() <= currentItem->zValue());
        item->setVisible(visible);
        ++index;
    }
    foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems) {
        bool visible = item->tileLayerGroup()->isVisible() && (item->zValue() <= currentItem->zValue());
        item->setVisible(visible);
    }

    // Darken layers below the current item
    mDarkRectangle->setZValue(currentItem->zValue() - 0.5);
    mDarkRectangle->setVisible(true);
}

bool ZomboidScene::levelForLayer(Layer *layer, int *level)
{
    (*level) = 0;

    // See if the layer name matches "0_foo" or "1_bar" etc.
    const QString& name = layer->name();
    QStringList sl = name.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        (*level) = sl[0].toInt(&conversionOK);
        return conversionOK;
    }
    return false;
}

void ZomboidScene::layerGroupVisibilityChanged(ZTileLayerGroup *g)
{
    if (mTileLayerGroupItems.contains(g->level()))
        mTileLayerGroupItems[g->level()]->setVisible(g->isVisible());
}

void ZomboidScene::layerAdded(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    int level;
    if (levelForLayer(layer, &level)) {
        layer->setLevel(level);
        if (TileLayer *tl = layer->asTileLayer()) {
            if (mTileLayerGroups.contains(level) == false) {
                mTileLayerGroups[level] = new ZomboidTileLayerGroup(this, level);
                mapDocument()->levelsModel()->addTileLayerGroup(mTileLayerGroups[level]);
            }
            mMapDocument->levelsModel()->addLayerToGroup(mTileLayerGroups[level], tl); // mTileLayerGroups[level]->addTileLayer(tl, index);
        }
    } else {
        // This handles duplicating layers
        layer->setLevel(0);
    }

    MapScene::layerAdded(index);

    setGraphicsSceneZOrder();
}

void ZomboidScene::layerAboutToBeRemoved(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group()) {
            mMapDocument->levelsModel()->removeLayerFromGroup(tl); // tl->group()->removeTileLayer(tl); // Calls tl->setGroup(0)
            mTileLayerGroupItems[tl->level()]->syncWithTileLayers();
            tl->setLevel(0); // otherwise it can't be cleared
        }
    }

    MapScene::layerAboutToBeRemoved(index);
}

void ZomboidScene::layerRemoved(int index)
{
    MapScene::layerRemoved(index);
}

/**
 * A layer has changed. This can mean that the layer visibility, opacity or
 * name has changed.
 */
void ZomboidScene::layerChanged(int index)
{
    // This gateway var isn't needed since I'm not going through LayerModel when setting opacity,
    // so no layerChanged signals are emitted, so no recursion happens here.
    static bool changingOpacity = false;
    if (changingOpacity)
        return;

    MapScene::layerChanged(index);

    Layer *layer = mMapDocument->map()->layerAt(index);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group() && mTileLayerGroupItems.contains(tl->level())) {
            ZTileLayerGroupItem *layerGroupItem = mTileLayerGroupItems[tl->level()];
            // Set the group item's opacity whenever the opacity of any owned layer changes
            if (layer->opacity() != layerGroupItem->opacity()) {
                layerGroupItem->setOpacity(layer->opacity());
                // Set the opacity of all the other layers in this group so the opacity slider
                // reflects the change when a new layer is selected.
                changingOpacity = true; // HACK - prevent recursion (see note above)
                foreach (TileLayer *tileLayer, layerGroupItem->tileLayerGroup()->mLayers) {
                    if (tileLayer != tl)
                        tileLayer->setOpacity(layer->opacity()); // FIXME: should I do what LayerDock::setLayerOpacity does (which will be recursive)?
                }
                changingOpacity = false;
            }
        }
        synchWithTileLayer(tl);
    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        foreach (MapObject *mo, og->objects()) {
            if (mMapObjectToLot.contains(mo)) {
                synchWithTileLayers();
                break;
            }
        }
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

void ZomboidScene::layerRenamed(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    int oldLevel = layer->level();
    int newLevel;
    bool hadGroup = false;
    bool hasGroup = levelForLayer(layer, &newLevel);

    if (TileLayer *tl = layer->asTileLayer()) {
        hadGroup = tl->group() != 0;
    }

    if ((oldLevel != newLevel) || (hadGroup != hasGroup)) {
        layerLevelAboutToChange(index, newLevel);
        layer->setLevel(newLevel);
        layerLevelChanged(index, oldLevel);
    }
}

void ZomboidScene::layerGroupAboutToChange(TileLayer *tl, ZTileLayerGroup *newGroup)
{
    Q_UNUSED(tl)
    Q_UNUSED(newGroup)
}

void ZomboidScene::layerGroupChanged(TileLayer *tl, ZTileLayerGroup *oldGroup)
{
    ZTileLayerGroup *newGroup = tl->group();

    int oldLevel = oldGroup ? oldGroup->level() : 0;
    int newLevel = newGroup ? newGroup->level() : 0;

    ZTileLayerGroupItem *oldOwner = 0, *newOwner = 0;

    // Find the old TileLayerGroupItem owner
    if (oldGroup && mTileLayerGroupItems.contains(oldLevel))
        oldOwner = mTileLayerGroupItems[oldLevel];

    // Find (or create) the new TileLayerGroupItem owner
    if (newGroup) {
        if (!mTileLayerGroupItems.contains(newLevel)) {
            mTileLayerGroupItems[newLevel] = new ZomboidTileLayerGroupItem((ZomboidTileLayerGroup*)newGroup, mMapDocument);
            addItem(mTileLayerGroupItems[newLevel]);
        }
        newOwner = mTileLayerGroupItems[newLevel];
    }

    if (oldOwner) {
        oldOwner->syncWithTileLayers();
    }
    if (newOwner) {
        tl->setOpacity(newOwner->opacity()); // FIXME: need to update the LayerDock slider
        newOwner->syncWithTileLayers();
    }

    // If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
    // managed by the base class.
    // If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
    // managed by the base class (MapScene) See createLayerItem().
    int index = mMapDocument->map()->layers().indexOf(tl);
    if (oldOwner && !newOwner) {
        delete mLayerItems[index]; // DummyGraphicsItem
        mLayerItems[index] = new TileLayerItem(tl, mMapDocument->renderer());
        mLayerItems[index]->setVisible(tl->isVisible());
        mLayerItems[index]->setOpacity(tl->opacity());
        addItem(mLayerItems[index]);
    }
    if (!oldOwner && newOwner) {
        delete mLayerItems[index]; // TileLayerItem
        mLayerItems[index] = new DummyGraphicsItem();
        mLayerItems[index]->setVisible(tl->isVisible());
        addItem(mLayerItems[index]);
    }

    setGraphicsSceneZOrder();
}

void ZomboidScene::layerLevelAboutToChange(int index, int newLevel)
{
    Q_UNUSED(index)
    Q_UNUSED(newLevel)
}

void ZomboidScene::layerLevelChanged(int index, int oldLevel)
{
    Layer *layer = mMapDocument->map()->layerAt(index);

    int newLevel;
    bool hasGroup = levelForLayer(layer, &newLevel);
    Q_ASSERT(newLevel == layer->level());

    // Setting a new maxLevel() for a map resizes the scene, requiring all existing items to be repositioned.
    if (newLevel > mMapDocument->map()->maxLevel()) {
        mMapDocument->map()->setMaxLevel(newLevel);
        mapChanged();
    }

    if (TileLayer *tl = layer->asTileLayer()) {
        ZTileLayerGroup *oldGroup = tl->group(), *newGroup = 0;

        if (hasGroup && mTileLayerGroups[newLevel] == 0) {
            mTileLayerGroups[newLevel] = new ZomboidTileLayerGroup(this, newLevel);
            mapDocument()->levelsModel()->addTileLayerGroup(mTileLayerGroups[newLevel]);
        }
        if (hasGroup)
            newGroup = mTileLayerGroups[newLevel];

        if (oldGroup != newGroup) {
            layerGroupAboutToChange(tl, newGroup);
            if (oldGroup) {
                mapDocument()->levelsModel()->removeLayerFromGroup(tl); // oldGroup->removeTileLayer(tl);
            }
            if (newGroup) {
                mapDocument()->levelsModel()->addLayerToGroup(newGroup, tl); // newGroup->addTileLayer(tl, index);
            }
            layerGroupChanged(tl, oldGroup);
        }

    } else {
        // Renaming an ObjectGroup or ImageLayer
        if (ObjectGroup *og = layer->asObjectGroup()) {
            foreach (MapObject *mapObject, og->objects())
                mObjectItems[mapObject]->syncWithMapObject();
        } else {
            // ImageLayer
            mLayerItems[index]->update();
        }
    }

    mGridItem->currentLayerIndexChanged(); // index didn't change, just updating the bounds
}

int ZomboidScene::levelZOrder(int level)
{
    return (level + 1) * 100;
}

// Determine sane Z-order for layers in and out of TileLayerGroups
void ZomboidScene::setGraphicsSceneZOrder()
{
    foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems)
        item->setZValue(levelZOrder(item->tileLayerGroup()->level()));

    ZTileLayerGroupItem *previousLevelItem = 0;
    QMap<ZTileLayerGroupItem*,QVector<QGraphicsItem*> > layersAboveLevel;
    int layerIndex = 0;
    foreach (Layer *layer, mMapDocument->map()->layers()) {
        if (TileLayer *tl = layer->asTileLayer()) {
            int level = tl->level();
            if (tl->group() && mTileLayerGroupItems.contains(level)) {
                previousLevelItem = mTileLayerGroupItems[level];
                ++layerIndex;
                continue;
            }
        }

        // Handle any layers not in a TileLayerGroup.
        // Layers between the first and last in a TileLayerGroup will be displayed above that TileLayerGroup.
        // Layers before the first TileLayerGroup will be displayed below the first TileLayerGroup.
        if (previousLevelItem)
            layersAboveLevel[previousLevelItem].append(mLayerItems[layerIndex]);
        else
            mLayerItems[layerIndex]->setZValue(layerIndex);
        ++layerIndex;
    }

    QMap<ZTileLayerGroupItem*,QVector<QGraphicsItem*> >::const_iterator it,
        it_start = layersAboveLevel.begin(),
        it_end = layersAboveLevel.end();
    for (it = it_start; it != it_end; it++) {
        int index = 1;
        foreach (QGraphicsItem *item, *it) {
            item->setZValue(levelZOrder(it.key()->tileLayerGroup()->level()) + index);
            ++index;
        }
    }
}

void ZomboidScene::onLotAdded(ZLot *lot, MapObject *mapObject)
{
    mLotMapObjects.append(mapObject);
    mMapObjectToLot[mapObject] = lot;

    MapObjectItem *item = itemForObject(mapObject); // FIXME: assumes createLayerItem() was called before this
    if (item) {
        // Set the drawMargins of the MapObjectItem to contain the whole lot map.
        const ZTileLayerGroup *lotGrp = lot->tileLayersForLevel(lot->minLevel());
        QRectF levelBoundsBottom = layerGroupSceneBounds(lot->map(), lotGrp, mMapDocument->renderer(),
            mapObject->objectGroup()->level(), mapObject->position().toPoint());
        lotGrp = lot->tileLayersForLevel(lot->map()->maxLevel());
        QRectF levelBoundsTop = layerGroupSceneBounds(lot->map(), lotGrp, mMapDocument->renderer(),
            mapObject->objectGroup()->level(), mapObject->position().toPoint());
        QRect mapBounds(mapObject->position().toPoint(), lot->map()->size());
        QRectF mapObjectBounds = mMapDocument->renderer()->boundingRect(mapBounds, mapObject->objectGroup()->level());
        QRectF lotBounds = levelBoundsTop | levelBoundsBottom;
        item->setDrawMargins(QMargins(mapObjectBounds.left() - lotBounds.left(),
            mapObjectBounds.top() - lotBounds.top(),
            lotBounds.right() - mapObjectBounds.right(),
            lotBounds.bottom() - mapObjectBounds.bottom()));

        // Resize the map object to the size of the lot's map, and snap-to-grid
        mapObject->setPosition(mapObject->position().toPoint());
        item->resize(lot->map()->size());
    }

    synchWithTileLayers();
}

void ZomboidScene::onLotRemoved(ZLot *lot, MapObject *mapObject)
{
    Q_UNUSED(lot)
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        item->setDrawMargins(QMargins());
    }
    mLotMapObjects.removeOne(mapObject);
    mMapObjectToLot.remove(mapObject);

    synchWithTileLayers();
}

void ZomboidScene::onLotUpdated(ZLot *lot, MapObject *mapObject)
{
    // Resize the map object to the size of the lot's map, and snap-to-grid
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        mapObject->setPosition(mapObject->position().toPoint());
        item->resize(lot->map()->size());
    }

    synchWithTileLayers();
}

#include "addremovemapobject.h"
#include <QFileInfo>
#include <QMimeData>
#include <QStringList>
#include <qgraphicssceneevent.h>
#include <QUrl>

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
        if (info.suffix() != QLatin1String("tmx")) continue;

        MapObject *newMapObject = new MapObject;
        newMapObject->setPosition(mMapDocument->renderer()->pixelToTileCoords(event->scenePos(), objectGroup->level()));
        newMapObject->setShape(MapObject::Rectangle);
        newMapObject->setSize(4, 4);
        objectGroup->addObject(newMapObject);
        mDnDMapObjectItem = new MapObjectItem(newMapObject, mapDocument());
        addItem(mDnDMapObjectItem);

        event->accept();
        return;
    }

    event->ignore();
}

void ZomboidScene::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    ObjectGroup *objectGroup = mDnDMapObjectItem->mapObject()->objectGroup();
    mDnDMapObjectItem->mapObject()->setPosition(mMapDocument->renderer()->pixelToTileCoords(event->scenePos(), objectGroup->level()));
    mDnDMapObjectItem->syncWithMapObject();
//    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}

void ZomboidScene::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    if (mDnDMapObjectItem) {
        MapObject *newMapObject = mDnDMapObjectItem->mapObject();

        ObjectGroup *objectGroup = newMapObject->objectGroup();
        objectGroup->removeObject(newMapObject);

        delete mDnDMapObjectItem;
        mDnDMapObjectItem = 0;

        delete newMapObject;
    }
}

void ZomboidScene::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    Layer *layer = mMapDocument->currentLayer();
    if (!layer) return;
    ObjectGroup *objectGroup = layer->asObjectGroup();
    if (!objectGroup) return;

    foreach (const QUrl &url, event->mimeData()->urls()) {
        QFileInfo info(url.toLocalFile());
        if (!info.exists()) continue;
        if (!info.isFile()) continue;
        if (info.suffix() != QLatin1String("tmx")) continue;

        MapObject *newMapObject = mDnDMapObjectItem->mapObject();
        delete mDnDMapObjectItem;

        ObjectGroup *objectGroup = newMapObject->objectGroup();
        objectGroup->removeObject(newMapObject);

        newMapObject->setPosition(mMapDocument->renderer()->pixelToTileCoords(event->scenePos(), objectGroup->level()));
        newMapObject->setName(QLatin1String("lot"));
        newMapObject->setType(info.baseName());

        mapDocument()->undoStack()->push(new AddMapObject(mapDocument(),
                                                          objectGroup,
                                                          newMapObject));
    }
}
