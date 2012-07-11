#include "mapcomposite.h"

#include "map.h"
#include "mapmanager.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tilelayer.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
//#include <QMapIterator>

using namespace Tiled;

static void maxMargins(const QMargins &a,
                       const QMargins &b,
                       QMargins &out)
{
    out.setLeft(qMax(a.left(), b.left()));
    out.setTop(qMax(a.top(), b.top()));
    out.setRight(qMax(a.right(), b.right()));
    out.setBottom(qMax(a.bottom(), b.bottom()));
}

static void unionTileRects(const QRect &a,
                           const QRect &b,
                           QRect &out)
{
    if (a.isEmpty())
        out = b;
    else if (b.isEmpty())
        out = a;
    else
        out = a | b;
}

static void unionSceneRects(const QRectF &a,
                           const QRectF &b,
                           QRectF &out)
{
    if (a.isEmpty())
        out = b;
    else if (b.isEmpty())
        out = a;
    else
        out = a | b;
}

///// ///// ///// ///// /////

///// ///// ///// ///// /////

CompositeLayerGroup::CompositeLayerGroup(MapComposite *owner, int level)
    : ZTileLayerGroup(owner->map(), level)
    , mOwner(owner)
    , mAnyVisibleLayers(false)
{

}

void CompositeLayerGroup::addTileLayer(TileLayer *layer, int index)
{
    ZTileLayerGroup::addTileLayer(layer, index);

    // FIXME: name shouldn't include "N_" prefix
    mLayersByName[layer->name()].append(layer);

    // To optimize drawing of submaps, remember which layers are totally empty.
    // But don't do this for the top-level map (the one being edited).
    // TileLayer::isEmpty() is SLOW, it's why I'm caching it.
    mEmptyLayers.resize(layerCount());
    mEmptyLayers[mLayers.indexOf(layer)] = mOwner->parent()
            ? layer->isEmpty() || layer->name().contains(QLatin1String("NoRender"))
            : false;
}

void CompositeLayerGroup::removeTileLayer(TileLayer *layer)
{
    int index = mLayers.indexOf(layer);
    mEmptyLayers.remove(index);

    ZTileLayerGroup::removeTileLayer(layer);

    index = mLayersByName[layer->name()].indexOf(layer);
    mLayersByName[layer->name()].remove(index);
}

void CompositeLayerGroup::prepareDrawing(const MapRenderer *renderer, const QRect &rect)
{
    mPreparedSubMapLayers.clear();
    if (mAnyVisibleLayers == false)
        return;
    foreach (const SubMapLayers &subMapLayer, mVisibleSubMapLayers) {
        CompositeLayerGroup *layerGroup = subMapLayer.mLayerGroup;
        if (subMapLayer.mSubMap->isHiddenDuringDrag())
            continue;
        QRectF bounds = layerGroup->boundingRect(renderer);
        if ((bounds & rect).isValid()) {
            mPreparedSubMapLayers.append(subMapLayer);
            layerGroup->prepareDrawing(renderer, rect);
        }
    }
}

bool CompositeLayerGroup::orderedCellsAt(const QPoint &pos, QVector<const Cell *> &cells) const
{
    bool cleared = false;
    int index = -1;
    foreach (TileLayer *tl, mLayers) {
        ++index;
        if (!tl->isVisible() || mEmptyLayers[index])
            continue;
        QPoint subPos = pos - tl->position();
        if (tl->contains(subPos)) {
            const Cell *cell = &tl->cellAt(subPos);
            if (!cell->isEmpty()) {
                if (!cleared) {
                    cells.clear();
                    cleared = true;
                }
                cells.append(cell);
            }
        }
    }

    // Overwrite map cells with sub-map cells at this location
    foreach (const SubMapLayers& subMapLayer, mPreparedSubMapLayers)
        subMapLayer.mLayerGroup->orderedCellsAt(pos - subMapLayer.mSubMap->origin(), cells);

    return !cells.isEmpty();
}

void CompositeLayerGroup::synch()
{
    QRect r;
    // See TileLayer::drawMargins()
    QMargins m(0, mOwner->map()->tileHeight(), mOwner->map()->tileWidth(), 0);

    mAnyVisibleLayers = false;

    int index = 0;
    foreach (TileLayer *tl, mLayers) {
        if (tl->isVisible() && !mEmptyLayers[index]) {
            unionTileRects(r, tl->bounds(), r);
            maxMargins(m, tl->drawMargins(), m);
            mAnyVisibleLayers = true;
        }
        ++index;
    }

    mTileBounds = r;

    r = QRect();
    mVisibleSubMapLayers.clear();

    foreach (MapComposite *subMap, mOwner->subMaps()) {
        if (!subMap->isGroupVisible() || !subMap->isVisible())
            continue;
        int levelOffset = subMap->levelOffset();
        CompositeLayerGroup *layerGroup = subMap->tileLayersForLevel(mLevel - levelOffset);
        if (layerGroup) {

            // Set the visibility of sub-map layers to match this layer-group's layers.
            // Layers in the sub-map without a matching name in the base map are always shown.
            foreach (Layer *layer, layerGroup->mLayers)
                layer->setVisible(true);
            foreach (Layer *layer, mLayers)
                layerGroup->setLayerVisibility(layer->name(), layer->isVisible());

#if 0 // Re-enable this if submaps ever change
            layerGroup->synch();
#endif
            if (layerGroup->mAnyVisibleLayers) {
                mVisibleSubMapLayers.append(SubMapLayers(subMap, layerGroup));
                unionTileRects(r, layerGroup->bounds().translated(subMap->origin()), r);
                maxMargins(m, layerGroup->drawMargins(), m);
                mAnyVisibleLayers = true;
            }
        }
    }

    mSubMapTileBounds = r;
    mDrawMargins = m;
}

QRect CompositeLayerGroup::bounds() const
{
    QRect bounds;
    unionTileRects(mTileBounds, mSubMapTileBounds, bounds);
    return bounds;
}

QMargins CompositeLayerGroup::drawMargins() const
{
    return mDrawMargins;
}

void CompositeLayerGroup::setLayerVisibility(const QString &name, bool visible) const
{
    if (!mLayersByName.contains(name))
        return;
    foreach (Layer *layer, mLayersByName[name])
        layer->setVisible(visible);
}

void CompositeLayerGroup::layerRenamed(TileLayer *layer)
{
    QMapIterator<QString,QVector<Layer*> > it(mLayersByName);
    while (it.hasNext()) {
        it.next();
        int index = it.value().indexOf(layer);
        if (index >= 0) {
            mLayersByName[it.key()].remove(index);
//            it.value().remove(index);
            break;
        }
    }

    mLayersByName[layer->name()].append(layer);
}

QRectF CompositeLayerGroup::boundingRect(const MapRenderer *renderer) const
{
    QRectF boundingRect = renderer->boundingRect(mTileBounds.translated(mOwner->originRecursive()),
                                                 mLevel + mOwner->levelRecursive());

    // The TileLayer includes the maximum tile size in its draw margins. So
    // we need to subtract the tile size of the map, since that part does not
    // contribute to additional margin.

    boundingRect.adjust(-mDrawMargins.left(),
                -qMax(0, mDrawMargins.top() - owner()->map()->tileHeight()),
                qMax(0, mDrawMargins.right() - owner()->map()->tileWidth()),
                mDrawMargins.bottom());

    foreach (const SubMapLayers &subMapLayer, mVisibleSubMapLayers) {
        QRectF bounds = subMapLayer.mLayerGroup->boundingRect(renderer);
        unionSceneRects(boundingRect, bounds, boundingRect);
    }

    return boundingRect;
}

///// ///// ///// ///// /////

// FIXME: If the MapDocument is saved to a new name, this MapInfo should be replaced with a new one
MapComposite::MapComposite(MapInfo *mapInfo, MapComposite *parent, const QPoint &positionInParent, int levelOffset)
    : mMapInfo(mapInfo)
    , mMap(mapInfo->map())
    , mParent(parent)
    , mPos(positionInParent)
    , mLevelOffset(levelOffset)
    , mMinLevel(0)
    , mVisible(true)
    , mGroupVisible(true)
    , mHiddenDuringDrag(false)
{
    int index = 0;
    foreach (Layer *layer, mMap->layers()) {
        if (mParent)
            layer->setVisible(!layer->name().contains(QLatin1String("NoRender")));
        int level;
        if (levelForLayer(layer, &level)) {
            // FIXME: no changing of mMap should happen after it is loaded!
            layer->setLevel(level); // for ObjectGroup,ImageLayer as well
        }
        if (TileLayer *tl = layer->asTileLayer()) {
            if (!mLayerGroups.contains(level))
                mLayerGroups[level] = new CompositeLayerGroup(this, level);
            mLayerGroups[level]->addTileLayer(tl, index);
        }
        ++index;
    }

#if 1
    // Load lots, but only if this is not the map being edited (that is handled
    // by the LotManager).
    if (mParent) {
        foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
            foreach (MapObject *object, objectGroup->objects()) {
                if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                    // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                    // then any sub-maps of its own will lose their level offsets.
                    MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(), mMap->orientation(),
                                                                          QFileInfo(mMapInfo->path()).absolutePath());
                    if (!subMapInfo) {
                        qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
#if 0 // FIXME: attempt to load this if mapsDirectory changes
                        subMapInfo = MapManager::instance()->getPlaceholderMap(object->type(), mMap->orientation(),
                                                                               32, 32); // FIXME: calculate map size
#endif
                    }
                    if (subMapInfo) {
                        int levelOffset;
                        (void) levelForLayer(objectGroup, &levelOffset);
                        qDebug() << "adding sub-map" << object->type() << "inside map" << mMapInfo->path();
#if 1
                        MapComposite *_subMap = new MapComposite(subMapInfo, this, object->position().toPoint(), levelOffset);
                        mSubMaps.append(_subMap);
#else
                        addMap(subMap, object->position().toPoint(), levelOffset);
#endif
                    }
                }
            }
        }
    }
#else
    foreach (ObjectGroup *objectGroup, mMap->objectGroups()) {
        foreach (MapObject *object, objectGroup->objects()) {
            if (object->name() == QLatin1String("lot") && !object->type().isEmpty()) {
                // FIXME: if this sub-map is converted from LevelIsometric to Isometric,
                // then any sub-maps of its own will lose their level offsets.
                // FIXME: look in the same directory as the parent map, then the maptools directory.
                MapInfo *subMapInfo = MapManager::instance()->loadMap(object->type(), mMap->orientation(),
                                                                      QFileInfo(mMapInfo->path()).absolutePath());
                if (!subMapInfo) {
                    qDebug() << "failed to find sub-map" << object->type() << "inside map" << mMapInfo->path();
                    subMapInfo = MapManager::instance()->getPlaceholderMap(object->type(), mMap->orientation(),
                                                                           32, 32); // FIXME: calculate map size
                }
                if (subMapInfo) {
                    int levelOffset;
                    (void) levelForLayer(objectGroup, &levelOffset);
#if 1
                    MapComposite *_subMap = new MapComposite(subMapInfo);
                    _subMap->setParentInfo(this, object->position().toPoint(), levelOffset);
                    mSubMaps.append(_subMap);
#else
                    addMap(subMap, object->position().toPoint(), levelOffset);
#endif
                }
            }
        }
    }
#endif

    mMinLevel = 10000;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        layerGroup->synch();
        if (layerGroup->level() < mMinLevel)
            mMinLevel = layerGroup->level();
        // FIXME: no changing of mMap should happen after it is loaded!
        if (layerGroup->level() > mMap->maxLevel())
            mMap->setMaxLevel(layerGroup->level());
    }

    if (mMinLevel == 10000)
        mMinLevel = 0;

    for (int level = mMinLevel; level <= mMap->maxLevel(); ++level) {
        if (mLayerGroups.contains(level))
            mSortedLayerGroups.append(mLayerGroups[level]);
    }
}

MapComposite::~MapComposite()
{
    qDeleteAll(mSubMaps);
    qDeleteAll(mLayerGroups);
}

bool MapComposite::levelForLayer(Layer *layer, int *level)
{
    (*level) = 0;

    // See if the layer name matches "0_foo" or "1_bar" etc.
    const QString& name = layer->name();
    QStringList sl = name.trimmed().split(QLatin1Char('_'));
    if (sl.count() > 1 && !sl[1].isEmpty()) {
        bool conversionOK;
        (*level) = sl[0].toUInt(&conversionOK);
        return conversionOK;
    }
    return false;
}

MapComposite *MapComposite::addMap(MapInfo *mapInfo, const QPoint &pos, int levelOffset)
{
    MapComposite *subMap = new MapComposite(mapInfo, this, pos, levelOffset);
    mSubMaps.append(subMap);

//    ensureMaxLevels(levelOffset + mapInfo->map()->maxLevel());

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->synch();

    return subMap;
}

void MapComposite::removeMap(MapComposite *subMap)
{
    Q_ASSERT(mSubMaps.contains(subMap));
    mSubMaps.remove(mSubMaps.indexOf(subMap));
    delete subMap;

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->synch();
}

void MapComposite::moveSubMap(MapComposite *subMap, const QPoint &pos)
{
    Q_ASSERT(mSubMaps.contains(subMap));
    subMap->setOrigin(pos);

    foreach (CompositeLayerGroup *layerGroup, mLayerGroups)
        layerGroup->synch();
}

void MapComposite::layerAdded(int index)
{
    layerRenamed(index);
}

void MapComposite::layerAboutToBeRemoved(int index)
{
    Layer *layer = mMap->layerAt(index);
    if (TileLayer *tl = layer->asTileLayer()) {
        if (tl->group()) {
            CompositeLayerGroup *oldGroup = (CompositeLayerGroup*)tl->group();
            emit layerAboutToBeRemovedFromGroup(index);
            removeLayerFromGroup(index);
            emit layerRemovedFromGroup(index, oldGroup);
        }
    }
}

void MapComposite::layerRenamed(int index)
{
    Layer *layer = mMap->layerAt(index);

    int oldLevel = layer->level();
    int newLevel;
    bool hadGroup = false;
    bool hasGroup = levelForLayer(layer, &newLevel);
    CompositeLayerGroup *oldGroup = 0;

    if (TileLayer *tl = layer->asTileLayer()) {
        oldGroup = (CompositeLayerGroup*)tl->group();
        hadGroup = oldGroup != 0;
        if (oldGroup)
            oldGroup->layerRenamed(tl);
    }

    if ((oldLevel != newLevel) || (hadGroup != hasGroup)) {
        if (hadGroup) {
            emit layerAboutToBeRemovedFromGroup(index);
            removeLayerFromGroup(index);
            emit layerRemovedFromGroup(index, oldGroup);
        }
        if (oldLevel != newLevel) {
            layer->setLevel(newLevel);
            emit layerLevelChanged(index, oldLevel);
        }
        if (hasGroup && layer->isTileLayer()) {
            addLayerToGroup(index);
            emit layerAddedToGroup(index);
        }
    }
}

void MapComposite::addLayerToGroup(int index)
{
    Layer *layer = mMap->layerAt(index);
    Q_ASSERT(layer->isTileLayer());
    if (TileLayer *tl = layer->asTileLayer()) {
        int level = tl->level();
        if (!mLayerGroups.contains(level)) {
            mLayerGroups[level] = new CompositeLayerGroup(this, level);

            if (level < mMinLevel)
                mMinLevel = level;
            if (level > mMap->maxLevel())
                mMap->setMaxLevel(level);

            mSortedLayerGroups.clear();
            for (int n = mMinLevel; n <= mMap->maxLevel(); ++n) {
                if (mLayerGroups.contains(n))
                    mSortedLayerGroups.append(mLayerGroups[n]);
            }

            emit layerGroupAdded(level);
        }
        mLayerGroups[level]->addTileLayer(tl, index);
        tl->setGroup(mLayerGroups[level]);
    }
}

void MapComposite::removeLayerFromGroup(int index)
{
    Layer *layer = mMap->layerAt(index);
    Q_ASSERT(layer->isTileLayer());
    if (TileLayer *tl = layer->asTileLayer()) {
        Q_ASSERT(tl->group());
        if (CompositeLayerGroup *layerGroup = (CompositeLayerGroup*)tl->group()) {
            layerGroup->removeTileLayer(tl);
            tl->setGroup(0);
        }
    }
}

CompositeLayerGroup *MapComposite::tileLayersForLevel(int level) const
{
    if (mLayerGroups.contains(level))
        return mLayerGroups[level];
    return 0;
}

CompositeLayerGroup *MapComposite::layerGroupForLayer(TileLayer *tl) const
{
    return tileLayersForLevel(tl->level());
}

void MapComposite::setOrigin(const QPoint &origin)
{
    mPos = origin;
}

QPoint MapComposite::originRecursive() const
{
    return mPos + (mParent ? mParent->originRecursive() : QPoint());
}

int MapComposite::levelRecursive() const
{
    return mLevelOffset + (mParent ? mParent->levelRecursive() : 0);
}

}

QRectF MapComposite::boundingRect(MapRenderer *renderer, bool forceMapBounds) const
{
    QRectF bounds;
    foreach (CompositeLayerGroup *layerGroup, mLayerGroups) {
        unionSceneRects(bounds,
                        layerGroup->boundingRect(renderer),
                        bounds);
    }
    if (forceMapBounds) {
        QRect mapTileBounds(mPos, mMap->size());

        // Always include level 0, even if there are no layers or only empty/hidden
        // layers on level 0, otherwise a SubMapItem's bounds won't include the
        // fancy rectangle.
        unionSceneRects(bounds,
                        renderer->boundingRect(mapTileBounds),
                        bounds);
        // When setting the bounds of the scene, make sure the highest level is included
        // in the sceneRect() so the grid won't be cut off.
        unionSceneRects(bounds,
                        renderer->boundingRect(mapTileBounds, mLevelOffset + mMap->maxLevel()),
                        bounds);
    }
    return bounds;
}
