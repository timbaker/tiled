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
#include "tilelayer.h"
#include "tilelayeritem.h"
#include "zlot.hpp"
#include "zlotmanager.hpp"

using namespace Tiled;
using namespace Tiled::Internal;

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
{
	MapDocument *mapDoc = mMapScene->mapDocument();
	Map *map = mapDoc->map();
	if (level > map->maxLevel()) {
		// FIXME: this never goes down if whole levels are deleted, requires reload
		map->setMaxLevel(level);
	}
}

void ZomboidTileLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
	mPreparedLotLayers.clear();
	foreach (MapObject *mapObject, mMapScene->mLotMapObjects) {
		QPoint mapObjectPos = mapObject->position().toPoint();
		ZLot *lot = mMapScene->mMapObjectToLot[mapObject];
		const ZTileLayerGroup *layerGroup = lot->tileLayersForLevel(mLevel);
		if (layerGroup) {
			QRect r = layerGroup->bounds().translated(mapObjectPos);
			QMargins m = layerGroup->drawMargins();
			QRectF bounds = renderer->boundingRect(r, layerGroup->mLayers.isEmpty() ? 0 : layerGroup->mLayers.first());
			bounds.adjust(-m.right(), -m.bottom(), m.left(), m.top());
			if ((bounds & rect).isValid()) {
				// Set the visibility of lot map layers to match this layer-group's layers.
				// NOTE: This works best when the lot map layers match the current map layers in number and order.
				int n = 0;
				foreach (TileLayer *layer, layerGroup->mLayers) {
					if (n >= mLayers.count())
						layer->setVisible(true);
					else
						layer->setVisible(mLayers[n]->isVisible());
					++n;
				}
				mPreparedLotLayers.append(LotLayers(mapObjectPos, layerGroup));
			}
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
		lotLayer.mLayerGroup->orderedCellsAt(point - lotLayer.mMapObjectPos, cells);
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

QRect ZomboidTileLayerGroup::bounds() const
{
	QRect r;
	foreach (TileLayer *tl, mLayers)
		r |= tl->bounds();

	foreach (MapObject *mapObject, mMapScene->mLotMapObjects) {
		ZLot *lot = mMapScene->mMapObjectToLot[mapObject];
		QPoint mapObjectPos = mapObject->position().toPoint();
		const ZTileLayerGroup *layerGroup = lot->tileLayersForLevel(mLevel);
		if (layerGroup)
			r |= layerGroup->bounds().translated(mapObjectPos);
	}
	return r;
}

QMargins ZomboidTileLayerGroup::drawMargins() const
{
	QMargins m;
	foreach (TileLayer *tl, mLayers)
		maxMargins(m, tl->drawMargins(), m);

	foreach (MapObject *mapObject, mMapScene->mLotMapObjects) {
		ZLot *lot = mMapScene->mMapObjectToLot[mapObject];
		const ZTileLayerGroup *layerGroup = lot->tileLayersForLevel(mLevel);
		if (layerGroup)
			maxMargins(m, layerGroup->drawMargins(), m);
	}

	return m;
}

///// ///// ///// ///// /////

ZomboidScene::ZomboidScene(QObject *parent)
    : MapScene(parent)
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

void ZomboidScene::setMapDocument(MapDocument *map)
{
	MapScene::setMapDocument(map);
	mLotManager.setMapDocument(mapDocument());
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
			if (TileLayer *tl = layer->asTileLayer()) {
				if (mTileLayerGroups.contains(level) == false)
					mTileLayerGroups[level] = new ZomboidTileLayerGroup(this, level);
				mTileLayerGroups[level]->addTileLayer(tl, index);
			}
			layer->setLevel(level); // for ObjectGroup,ImageLayer as well
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

	foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems)
		item->syncWithTileLayers();
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
	}
};

QGraphicsItem *ZomboidScene::createLayerItem(Layer *layer)
{
	if (TileLayer *tl = layer->asTileLayer()) {
		if (tl->group()) {
			if (mTileLayerGroupItems[tl->level()] == 0) {
				mTileLayerGroupItems[tl->level()] = new ZTileLayerGroupItem(tl->group(), mMapDocument->renderer());
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

    const Layer *currentLayer = mMapDocument->currentLayer();

    if (!mHighlightCurrentLayer || !currentLayer) {
        mDarkRectangle->setVisible(false);

        // Restore opacity for all levels
        foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems) {
			if (item->getTileLayerGroup()->mLayers.isEmpty())
				continue;
			const Layer *layer = item->getTileLayerGroup()->mLayers.first();
            item->setOpacity(layer->opacity());
        }

        return;
    }

    // Set levels above the current level to half opacity
	ZTileLayerGroup *currentLevelGroup = 0;
	const qreal opacityFactor = 0.4;
    foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems) {
		if (item->getTileLayerGroup()->mLayers.isEmpty())
			continue;
		const Layer *layer = item->getTileLayerGroup()->mLayers.first();
		const int index = item->getTileLayerGroup()->mIndices.first();
        const qreal multiplier = (currentLayer->level() < layer->level()) ? opacityFactor : 1;
        item->setOpacity(layer->opacity() * multiplier);
		if (layer->level() == currentLayer->level())
			currentLevelGroup = item->getTileLayerGroup();
    }

    // Darken layers below the current layer
	if (currentLevelGroup)
		mDarkRectangle->setZValue(levelZOrder(currentLevelGroup->level()) - 0.5);
	else
		mDarkRectangle->setZValue(0 - 0.5); // ObjectGroup or not-in-a-level TileLayer
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

void ZomboidScene::layerAdded(int index)
{
	Layer *layer = mMapDocument->map()->layerAt(index);
	int level;
	if (levelForLayer(layer, &level)) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (mTileLayerGroups.contains(level) == false)
				mTileLayerGroups[level] = new ZomboidTileLayerGroup(this, level);
			mTileLayerGroups[level]->addTileLayer(tl, index);
		}
		layer->setLevel(level);
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
			tl->group()->removeTileLayer(tl); // Calls tl->setGroup(0)
			mTileLayerGroupItems[tl->level()]->syncWithTileLayers();
			tl->setLevel(0); // otherwise it can't be cleared
		}
	}
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
				foreach (TileLayer *tileLayer, layerGroupItem->getTileLayerGroup()->mLayers) {
					if (tileLayer != tl)
						tileLayer->setOpacity(layer->opacity()); // FIXME: should I do what LayerDock::setLayerOpacity does (which will be recursive)?
				}
				changingOpacity = false;
			}
			// Redraw
			layerGroupItem->tileLayerChanged(tl);
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
	bool hasGroup = levelForLayer(layer, &newLevel);

	if (oldLevel != newLevel) {
		layerLevelAboutToChange(index, newLevel);
		layer->setLevel(newLevel);
		layerLevelChanged(index, oldLevel);
	}
}

void ZomboidScene::layerGroupAboutToChange(TileLayer *tl, ZTileLayerGroup *newGroup)
{
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
			mTileLayerGroupItems[newLevel] = new ZTileLayerGroupItem(newGroup, mMapDocument->renderer());
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
		}
		if (hasGroup)
			newGroup = mTileLayerGroups[newLevel];

		if (oldGroup != newGroup) {
			layerGroupAboutToChange(tl, newGroup);
			if (oldGroup) {
				oldGroup->removeTileLayer(tl);
			}
			if (newGroup) {
				newGroup->addTileLayer(tl, index);
			}
			layerGroupChanged(tl, oldGroup);
		}

	} else {
		// Renaming an ObjectGroup or ImageLayer
		if (ObjectGroup *og = layer->asObjectGroup()) {
			foreach (MapObject *mapObject, og->objects())
				mObjectItems[mapObject]->syncWithMapObject();
			// An ObjectGroup with no items which changes level will not cause any redrawing.
			// However, the grid may need to be redrawn.
			if (isGridVisible() && !og->objectCount())
				update();
		} else {
			// ImageLayer
			mLayerItems[index]->update();
		}
	}
}

int ZomboidScene::levelZOrder(int level)
{
	return (level + 1) * 100;
}

// Determine sane Z-order for layers in and out of TileLayerGroups
void ZomboidScene::setGraphicsSceneZOrder()
{
	foreach (ZTileLayerGroupItem *item, mTileLayerGroupItems)
		item->setZValue(levelZOrder(item->getTileLayerGroup()->level()));

	ZTileLayerGroupItem *previousLevelItem = 0;
	QMap<ZTileLayerGroupItem*,QVector<QGraphicsItem*>> layersAboveLevel;
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

	QMap<ZTileLayerGroupItem*,QVector<QGraphicsItem*>>::const_iterator it,
		it_start = layersAboveLevel.begin(),
		it_end = layersAboveLevel.end();
	for (it = it_start; it != it_end; it++) {
		int index = 1;
		foreach (QGraphicsItem *item, *it) {
			item->setZValue(levelZOrder(it.key()->getTileLayerGroup()->level()) + index);
			++index;
		}
	}
}

void ZomboidScene::onLotAdded(ZLot *lot, MapObject *mapObject)
{
	mLotMapObjects.append(mapObject);
	mMapObjectToLot[mapObject] = lot;

	// Resize the map object to the size of the lot's map, and snap-to-grid
	MapObjectItem *item = itemForObject(mapObject); // FIXME: assumes createLayerItem() was called before this
	if (item) {
		QRect mapBounds(0, 0, lot->map()->width(), lot->map()->height());
		QRectF levelBoundsBottom = mMapDocument->renderer()->boundingRect(mapBounds);
		const ZTileLayerGroup *lotGrp = lot->tileLayersForLevel(lot->map()->maxLevel());
		QRectF levelBoundsTop = mMapDocument->renderer()->boundingRect(mapBounds, lotGrp->mLayers[0]);
		QRectF lotBounds = levelBoundsBottom | levelBoundsTop;
		QMargins lotMargins = lot->map()->drawMargins();
		lotBounds.adjust(-lotMargins.left(), -lotMargins.top(), lotMargins.right(), lotMargins.bottom());
		QRectF mapObjectBounds = levelBoundsBottom;
		item->setDrawMargins(QMargins(mapObjectBounds.left() - lotBounds.left(),
			mapObjectBounds.top() - lotBounds.top(),
			lotBounds.right() - mapObjectBounds.right(),
			lotBounds.bottom() - mapObjectBounds.bottom()));

		mapObject->setPosition(mapObject->position().toPoint());
		item->resize(QSizeF(lot->map()->size()));
	}
}

void ZomboidScene::onLotRemoved(ZLot *lot, MapObject *mapObject)
{
	mLotMapObjects.removeOne(mapObject);
	mMapObjectToLot.remove(mapObject);
}

void ZomboidScene::onLotUpdated(ZLot *lot, MapObject *mapObject)
{
	// Resize the map object to the size of the lot's map, and snap-to-grid
	MapObjectItem *item = itemForObject(mapObject);
	if (item) {
		mapObject->setPosition(mapObject->position().toPoint());
		item->resize(QSizeF(lot->map()->size()));
	}
}