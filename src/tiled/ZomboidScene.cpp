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
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "zgriditem.hpp"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tilelayeritem.h"
#include "zlevelsmodel.hpp"
#include "zlotmanager.hpp"

using namespace Tiled;
using namespace Tiled::Internal;

///// ///// ///// ///// /////

#include <QStyleOptionGraphicsItem>

class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

    void synchWithTileLayers();
    void updateBounds();

    CompositeLayerGroup *layerGroup() const { return mLayerGroup; }

private:
    CompositeLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
};

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
    if (mBoundingRect != mLayerGroup->boundingRect(mRenderer)) {
        return;
    }
    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#ifdef _DEBUG
    p->drawRect(mBoundingRect);
#endif
}

void CompositeLayerGroupItem::synchWithTileLayers()
{
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
    , mDnDMapObjectItem(0)
    , mPendingActive(false)
{
    connect(&mLotManager, SIGNAL(lotAdded(MapComposite*,MapObject*)),
        this, SLOT(onLotAdded(MapComposite*,MapObject*)));
    connect(&mLotManager, SIGNAL(lotRemoved(MapComposite*,MapObject*)),
        this, SLOT(onLotRemoved(MapComposite*,MapObject*)));
    connect(&mLotManager, SIGNAL(lotUpdated(MapComposite*,MapObject*)),
        this, SLOT(onLotUpdated(MapComposite*,MapObject*)));
}

ZomboidScene::~ZomboidScene()
{
    mLotManager.disconnect(this);
}

void ZomboidScene::setMapDocument(MapDocument *mapDoc)
{
    MapScene::setMapDocument(mapDoc);
    mLotManager.setMapDocument(mapDocument());

    if (mapDocument()) {
        connect(mMapDocument, SIGNAL(layerGroupAdded(int)), SLOT(layerGroupAdded(int)));
        connect(mMapDocument, SIGNAL(layerGroupVisibilityChanged(CompositeLayerGroup*)), SLOT(layerGroupVisibilityChanged(CompositeLayerGroup*)));
        connect(mMapDocument, SIGNAL(layerAddedToGroup(int)), SLOT(layerAddedToGroup(int)));
        connect(mMapDocument, SIGNAL(layerRemovedFromGroup(int,CompositeLayerGroup*)), SLOT(layerRemovedFromGroup(int,CompositeLayerGroup*)));
        connect(mMapDocument, SIGNAL(layerLevelChanged(int,int)), SLOT(layerLevelChanged(int,int)));
    }
}

void ZomboidScene::regionChanged(const QRegion &region, Layer *layer)
{
    // Painting tiles may update the draw margins of a layer.
    if (TileLayer *tl = layer->asTileLayer()) {
        // The drawMargins will only change the first time painting occurs
        // in an empty layer.
        if (tl->group() && mTileLayerGroupItems.contains(tl->level())) {
            if (tl->drawMargins() != tl->group()->drawMargins())
                synchWithTileLayer(tl); // recalculate CompositeLayerGroup::mDrawMargins
        }
    }

    MapScene::regionChanged(region, layer);
}

void ZomboidScene::synchWithTileLayers()
{
    doLater(AllGroups | Synch | Bounds);
}

void ZomboidScene::synchWithTileLayer(TileLayer *tl)
{
    if (tl->group()) {
        if (mTileLayerGroupItems.contains(tl->level())) {
            CompositeLayerGroupItem *item = mTileLayerGroupItems[tl->level()];
            if (!mPendingGroupItems.contains(item))
                mPendingGroupItems += item;
            doLater(Synch | Bounds);
        }
    }
}

void ZomboidScene::updateLayerGroupItemBounds()
{
    doLater(Bounds | AllGroups);
}

void ZomboidScene::updateLayerGroupItemBounds(TileLayer *tl)
{
    if (mTileLayerGroupItems.contains(tl->level())) {
        CompositeLayerGroupItem *item = mTileLayerGroupItems[tl->level()];
        if (!mPendingGroupItems.contains(item))
            mPendingGroupItems += item;
        doLater(Bounds);
    }
}

void ZomboidScene::refreshScene()
{
    qDeleteAll(mTileLayerGroupItems); // QGraphicsScene.clear() will delete these actually
    mTileLayerGroupItems.clear();

    MapScene::refreshScene();

    doLater(ZOrder);
    synchWithTileLayers();
}

void ZomboidScene::mapChanged()
{
    MapScene::mapChanged();

    updateLayerGroupItemBounds();
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
                updateLayerGroupItemBounds(tl);
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

    if (!mHighlightCurrentLayer || !currentLayer) {
        mDarkRectangle->setVisible(false);

        // Restore visibility for all non-ZTileLayerGroupItem layers
        for (int i = 0; i < mLayerItems.size(); ++i) {
            const Layer *layer = mMapDocument->map()->layerAt(i);
            mLayerItems.at(i)->setVisible(layer->isVisible());
        }

        // Restore visibility for all ZTileLayerGroupItem layers
        foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems) {
            item->setVisible(item->layerGroup()->isVisible());
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
    foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems) {
        bool visible = item->layerGroup()->isVisible() && (item->zValue() <= currentItem->zValue());
        item->setVisible(visible);
    }

    // Darken layers below the current item
    mDarkRectangle->setZValue(currentItem->zValue() - 0.5);
    mDarkRectangle->setVisible(true);
}

void ZomboidScene::layerAdded(int index)
{
    MapScene::layerAdded(index);

    doLater(ZOrder);
}

void ZomboidScene::layerRemoved(int index)
{
    MapScene::layerRemoved(index);

    doLater(ZOrder);
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
            CompositeLayerGroupItem *layerGroupItem = mTileLayerGroupItems[tl->level()];
            // Set the group item's opacity whenever the opacity of any owned layer changes
            if (layer->opacity() != layerGroupItem->opacity()) {
                layerGroupItem->setOpacity(layer->opacity());
                // Set the opacity of all the other layers in this group so the opacity slider
                // reflects the change when a new layer is selected.
                changingOpacity = true; // HACK - prevent recursion (see note above)
                foreach (TileLayer *tileLayer, layerGroupItem->layerGroup()->mLayers) {
                    if (tileLayer != tl)
                        tileLayer->setOpacity(layer->opacity()); // FIXME: should I do what LayerDock::setLayerOpacity does (which will be recursive)?
                }
                changingOpacity = false;
            }
        }
        synchWithTileLayer(tl);
    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        bool synch = false;
        foreach (MapObject *mo, og->objects()) {
            if (mMapObjectToLot.contains(mo)) {
                // FIXME: layerVisibilityChanged() signal please
                if (mMapObjectToLot[mo]->isGroupVisible() != og->isVisible()) {
                    mMapObjectToLot[mo]->setGroupVisible(og->isVisible());
                    synch = true;
                }
            }
        }
        if (synch)
            synchWithTileLayers();
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
    }

    // Setting a new maxLevel() for a map resizes the scene, requiring all existing items to be repositioned.
    if (level == mMapDocument->map()->maxLevel())
        mapChanged();
}

void ZomboidScene::layerGroupVisibilityChanged(CompositeLayerGroup *g)
{
    if (mTileLayerGroupItems.contains(g->level()))
        mTileLayerGroupItems[g->level()]->setVisible(g->isVisible());
}

void ZomboidScene::layerAddedToGroup(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    Q_ASSERT(layer->isTileLayer());
    int level = layer->level();
    if (mTileLayerGroupItems.contains(level))
        synchWithTileLayer(layer->asTileLayer());

    // If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
    // managed by the base class.
    // If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
    // managed by the base class (MapScene) See createLayerItem().
    delete mLayerItems[index]; // TileLayerItem
    mLayerItems[index] = new DummyGraphicsItem();
    mLayerItems[index]->setVisible(layer->isVisible());
    addItem(mLayerItems[index]);

    doLater(ZOrder);
}

void ZomboidScene::layerRemovedFromGroup(int index, CompositeLayerGroup *oldGroup)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    Q_ASSERT(layer->isTileLayer());
    TileLayer *tl = layer->asTileLayer();

    int level = layer->level(); // STILL VALID FOR GROUP IT WAS IN
    if (mTileLayerGroupItems.contains(level))
        synchWithTileLayer(layer->asTileLayer());

    // If a TileLayerGroup owns a layer, then a DummyGraphicsItem is created which is
    // managed by the base class.
    // If no TileLayerGroup owns a layer, then a TileLayerItem is created which is
    // managed by the base class (MapScene) See createLayerItem().
    delete mLayerItems[index]; // DummyGraphicsItem
    mLayerItems[index] = new TileLayerItem(tl, mMapDocument->renderer());
    mLayerItems[index]->setVisible(tl->isVisible());
    mLayerItems[index]->setOpacity(tl->opacity());
    addItem(mLayerItems[index]);

    doLater(ZOrder);
}

void ZomboidScene::layerLevelChanged(int index, int oldLevel)
{
    Layer *layer = mMapDocument->map()->layerAt(index);

    if (TileLayer *tl = layer->asTileLayer()) {

    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        bool synch = false;
        foreach (MapObject *mapObject, og->objects()) {
            if (mMapObjectToLot.contains(mapObject)) {
                mMapObjectToLot[mapObject]->setGroupVisible(og->isVisible());
                mMapObjectToLot[mapObject]->setLevel(og->level());
                synch = true;
            }
            mObjectItems[mapObject]->syncWithMapObject();
        }
        if (synch)
            synchWithTileLayers();
    } else {
        // ImageLayer
        mLayerItems[index]->update();
    }

    mGridItem->currentLayerIndexChanged(); // index didn't change, just updating the bounds
}

int ZomboidScene::levelZOrder(int level)
{
    return (level + 1) * 100;
}

void ZomboidScene::doLater(PendingFlags flags)
{
    mPendingFlags |= flags;
    if (mPendingActive)
        return;
    QMetaObject::invokeMethod(this, "handlePendingUpdates",
                              Qt::QueuedConnection);
    mPendingActive = true;
}

// Determine sane Z-order for layers in and out of TileLayerGroups
void ZomboidScene::setGraphicsSceneZOrder()
{
    foreach (CompositeLayerGroupItem *item, mTileLayerGroupItems)
        item->setZValue(levelZOrder(item->layerGroup()->level()));

    CompositeLayerGroupItem *previousLevelItem = 0;
    QMap<CompositeLayerGroupItem*,QVector<QGraphicsItem*> > layersAboveLevel;
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

    QMap<CompositeLayerGroupItem*,QVector<QGraphicsItem*> >::const_iterator it,
        it_start = layersAboveLevel.begin(),
        it_end = layersAboveLevel.end();
    for (it = it_start; it != it_end; it++) {
        int index = 1;
        foreach (QGraphicsItem *item, *it) {
            item->setZValue(levelZOrder(it.key()->layerGroup()->level()) + index);
            ++index;
        }
    }
}

void ZomboidScene::onLotAdded(MapComposite *lot, MapObject *mapObject)
{
    mLotMapObjects.append(mapObject);
    mMapObjectToLot[mapObject] = lot;

    MapObjectItem *item = itemForObject(mapObject); // FIXME: assumes createLayerItem() was called before this
    if (item) {
        QRect mapBounds(lot->origin(), lot->map()->size());
        QRectF mapObjectBounds = mMapDocument->renderer()->boundingRect(mapBounds, lot->levelOffset());
        QRectF lotBounds = lot->boundingRect(mMapDocument->renderer());
        item->setDrawMargins(QMargins(mapObjectBounds.left() - lotBounds.left(),
            mapObjectBounds.top() - lotBounds.top(),
            lotBounds.right() - mapObjectBounds.right(),
            lotBounds.bottom() - mapObjectBounds.bottom()));

        // Resize the map object to the size of the lot's map, and snap-to-grid
        mapObject->setPosition(lot->origin());
        item->resize(lot->map()->size());

        mOldMapObjectBounds[item] = lotBounds;
    }

    // MapComposite::synch was already called
    updateLayerGroupItemBounds();
}

void ZomboidScene::onLotRemoved(MapComposite *lot, MapObject *mapObject)
{
    // NB: 'lot' is deleted already
    Q_UNUSED(lot)
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        item->setDrawMargins(QMargins());
        mOldMapObjectBounds.remove(item);
    }
    mLotMapObjects.removeOne(mapObject);
    mMapObjectToLot.remove(mapObject);

    updateLayerGroupItemBounds();
}

void ZomboidScene::onLotUpdated(MapComposite *lot, MapObject *mapObject)
{
    // Resize the map object to the size of the lot's map, and snap-to-grid
    MapObjectItem *item = itemForObject(mapObject);
    if (item) {
        mapObject->setPosition(lot->origin());
        item->resize(lot->map()->size());

        // When a MapObject is moved by the user, no redrawing takes place
        // once the move is complete (because the MapObject was redrawn
        // while being dragged).  So the map won't be redrawn under the old
        // lot bounds or at the new location; Therefore those areas must be
        // repainted manually.

        // Can't call item->boundingRect() because the LotManager gets the
        // objectsChanged() signal before MapScene.
        QRectF newBounds = lot->boundingRect(mMapDocument->renderer());

        if (mOldMapObjectBounds.contains(item)) {
            QRectF oldBounds = mOldMapObjectBounds[item];
            if (newBounds != oldBounds)
                update(oldBounds);
        }
        // FIXME: unset this if the item is deleted
        mOldMapObjectBounds[item] = newBounds;
        update(newBounds);
    }

    updateLayerGroupItemBounds();
}

void ZomboidScene::handlePendingUpdates()
{
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
    }
    if (mPendingFlags & ZOrder)
        setGraphicsSceneZOrder();

    mPendingFlags = None;
    mPendingGroupItems.clear();
    mPendingActive = false;
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
