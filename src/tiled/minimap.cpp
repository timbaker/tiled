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

#include "minimap.h"

#include "bmpblender.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapmanager.h"
#include "maprenderer.h"
#include "mapview.h"
#include "preferences.h"
#include "tilesetmanager.h"
#include "ZomboidScene.h"

#include "isometricrenderer.h"
#include "map.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

#include <qmath.h>
#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsPolygonItem>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollBar>
#include <QToolButton>

using namespace Tiled;
using namespace Tiled::Internal;

#ifdef QT_NO_DEBUG
inline QNoDebug noise() { return QNoDebug(); }
#else
inline QDebug noise() { return QDebug(QtDebugMsg); }
#endif

/////

ShadowMap::ShadowMap(MapInfo *mapInfo)
{
    IN_APP_THREAD
    Map *map = mapInfo->map()->clone(); // FIXME: make copy of BMP images, not thread-safe
    TilesetManager::instance()->addReferences(map->tilesets());
    mapInfo = MapManager::instance()->newFromMap(map, mapInfo->path()); // FIXME: save as... changes path?
    mMapComposite = new MapComposite(mapInfo);

    foreach (CompositeLayerGroup *layerGroup, mMapComposite->sortedLayerGroups()) {
        foreach (TileLayer *tl, layerGroup->layers()) {
            bool isVisible = true;
            if (tl->name().contains(QLatin1String("NoRender")))
                isVisible = false;
            layerGroup->setLayerVisibility(tl, isVisible);
            layerGroup->setLayerOpacity(tl, 1.0f);
        }
        layerGroup->synch();
    }
}

ShadowMap::~ShadowMap()
{
    IN_APP_THREAD
    MapInfo *mapInfo = mMapComposite->mapInfo();
    delete mMapComposite;
    TilesetManager::instance()->removeReferences(mapInfo->map()->tilesets());
    delete mapInfo->map();
    delete mapInfo;
}

void ShadowMap::layerAdded(int index, Layer *layer)
{
    mMapComposite->map()->insertLayer(index, layer);
    mMapComposite->layerAdded(index);

    if (TileLayer *tl = layer->asTileLayer()) {
        if (CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLayer(tl)) {
            bool isVisible = true;
            if (tl->name().contains(QLatin1String("NoRender")))
                isVisible = false;
            layerGroup->setLayerVisibility(tl, isVisible);
            layerGroup->setLayerOpacity(tl, 1.0f);
        }
    }
}

void ShadowMap::layerRemoved(int index)
{
    mMapComposite->layerAboutToBeRemoved(index);
    delete mMapComposite->map()->takeLayerAt(index);
}

void ShadowMap::layerRenamed(int index, const QString &name)
{
    mMapComposite->map()->layerAt(index)->setName(name);
    mMapComposite->layerRenamed(index);
}

void ShadowMap::regionAltered(const QRegion &rgn, Layer *layer)
{
#if 0
    int index = mMaster->layers().indexOf(layer);
    if (TileLayer *tl = mMapComposite->map()->layerAt(index)->asTileLayer()) {
        foreach (QRect r, rgn.rects()) {
            for (int x = r.x(); x <= r.right(); x++) {
                for (int y = r.y(); y <= r.bottom(); y++) {
                    tl->setCell(x, y, layer->asTileLayer()->cellAt(x, y));
                }
            }
        }
    }
#endif
}

void ShadowMap::lotAdded(quintptr id, MapInfo *mapInfo, const QPoint &pos, int level)
{
    MapComposite *subMap = mMapComposite->addMap(mapInfo, pos, level);
    mIDToLot[id] = subMap;
    mLotToID[subMap] = id;
}

void ShadowMap::lotRemoved(quintptr id)
{
    Q_ASSERT(mIDToLot[id]);
    mMapComposite->removeMap(mIDToLot[id]);
    mLotToID.remove(mIDToLot[id]);
    mIDToLot.remove(id);
}

void ShadowMap::lotUpdated(quintptr id, const QPoint &pos)
{
    Q_ASSERT(mIDToLot[id]);
    mMapComposite->moveSubMap(mIDToLot[id], pos);
}

void ShadowMap::mapAboutToChange(MapInfo *mapInfo)
{
    mMapComposite->mapAboutToChange(mapInfo);
}

void ShadowMap::mapChanged(MapInfo *mapInfo)
{
    mMapComposite->mapChanged(mapInfo);
}

/////

class MapChange
{
public:
    enum Change {
        LayerAdded,
        LayerRemoved,
        LayerRenamed,
        RegionAltered,
        LotAdded,
        LotRemoved,
        LotUpdated,
        MapChanged,
        MapResized,
        TilesetAdded,
        TilesetRemoved,
        TilesetChanged,
        BmpPainted,
        BmpAliasesChanged,
        BmpRulesChanged,
        BmpBlendsChanged,
        Recreate
    };

    MapChange(Change change)
        : mChange(change)
    {

    }

    Change mChange;
    struct LotInfo
    {
        MapInfo *mapInfo;
        quintptr id;
        QPoint pos;
        int level;
    };
    LotInfo mLotInfo;
    Tiled::Layer *mLayer;
    int mLayerIndex;
    QString mName;
    struct CellEntry
    {
        CellEntry(int x, int y, const Cell &cell) :
            x(x), y(y), cell(cell)
        {

        }

        int x, y;
        Tiled::Cell cell;
    };
    QList<CellEntry> mCells;
    Tileset *mTileset;
    int mTilesetIndex;
    QString mTilesetName;
    QSize mMapSize;

    QImage mBmps[2];
    int mBmpIndex;
    QList<BmpAlias*> mBmpAliases;
    QList<BmpRule*> mBmpRules;
    QList<BmpBlend*> mBmpBlends;
    QRegion mRegion;
};

MiniMapRenderWorker::MiniMapRenderWorker(MapInfo *mapInfo, InterruptibleThread *thread) :
    BaseWorker(thread),
    mRedrawAll(true),
    mVisible(false)
{
    IN_APP_THREAD
    mShadowMap = new ShadowMap(mapInfo);
    mShadowMap->mMapComposite->setParent(this); // so moveToThread() moves it
    (mapInfo->orientation() == Map::Isometric)
            ? mRenderer = new IsometricRenderer(mapInfo->map())
            : mRenderer = new ZLevelRenderer(mapInfo->map());
    mRenderer->setMaxLevel(mShadowMap->mMapComposite->maxLevel());
    mRenderer->mAbortDrawing = thread->var();

    QRectF r = mShadowMap->mMapComposite->boundingRect(mRenderer);
    qreal scale = 512.0 / r.width();
    QSize imageSize = QRectF(QPoint(), r.size() * scale).toAlignedRect().size();
    mImage = QImage(imageSize, QImage::Format_ARGB32);
}

MiniMapRenderWorker::~MiniMapRenderWorker()
{
    IN_APP_THREAD
    delete mRenderer;
    Q_ASSERT(mShadowMap->mMapComposite->QObject::parent() == this);
    mShadowMap->mMapComposite->setParent(0);
    delete mShadowMap;
    qDeleteAll(mPendingChanges);
}

void MiniMapRenderWorker::work()
{
    IN_WORKER_THREAD

    processChanges(mPendingChanges);
    qDeleteAll(mPendingChanges);
    mPendingChanges.clear();

    if (!mVisible)
        return;

    if (!mRedrawAll && mDirtyRect.isEmpty())
        return;

    QRectF sceneRect = mShadowMap->mMapComposite->boundingRect(mRenderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return;

    QRectF paintRect = mDirtyRect;
    if (mRedrawAll)
        paintRect = sceneRect;

    qreal scale = mImage.width() / qreal(mapSize.width());

    QPainter painter(&mImage);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    QTransform xform = QTransform::fromScale(scale, scale)
            .translate(-sceneRect.left(), -sceneRect.top());
    painter.setTransform(xform);

    painter.setClipRect(paintRect);

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(paintRect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QElapsedTimer timer;
    timer.start();

    bool aborted = false;

    MapComposite::ZOrderList zorder = mShadowMap->mMapComposite->zOrder();
    foreach (MapComposite::ZOrderItem zo, zorder) {
        if (zo.group)
            mRenderer->drawTileLayerGroup(&painter, zo.group, paintRect);
        else if (TileLayer *tl = zo.layer->asTileLayer()) {
            if (tl->name().contains(QLatin1String("NoRender")))
                continue;
            mRenderer->drawTileLayer(&painter, tl, paintRect);
        }

        aborted = this->aborted();
        if (aborted) {
            noise() << "MiniMapRenderWorker: interrupting -" << timer.elapsed() << "ms wasted" << this;
            break;
        }
    }

    if (!aborted) {
        noise() << "MiniMapRenderWorker: painting took" << timer.elapsed() << "ms" << this;
        mRedrawAll = false;
        mDirtyRect = QRectF();
        emit painted(mImage, sceneRect);
    }
}

void MiniMapRenderWorker::applyChanges(const QList<MapChange *> &changes)
{
    IN_WORKER_THREAD
    mPendingChanges += changes;

    // I have a feeling this gets added to the queue we're processing now rather
    // than waiting for the next event loop in this thread.
    scheduleWork();
}

void MiniMapRenderWorker::processChanges(const QList<MapChange *> &changes)
{
    IN_WORKER_THREAD

    ShadowMap &sm = *mShadowMap;
    QRectF oldBounds = sm.mMapComposite->boundingRect(mRenderer);
    bool redrawAll = mRedrawAll;

    foreach (MapChange *cp, changes) {
        const MapChange &c = *cp;
        switch (c.mChange) {
        case MapChange::LayerAdded:
            sm.layerAdded(c.mLayerIndex, c.mLayer);
            redrawAll = true;
            break;
        case MapChange::LayerRemoved:
            sm.layerRemoved(c.mLayerIndex);
            redrawAll = true;
            break;
        case MapChange::LayerRenamed:
            sm.layerRenamed(c.mLayerIndex, c.mName);
            redrawAll = true;
            break;
        case MapChange::RegionAltered: {
            if (Layer *layer = sm.mMapComposite->map()->layerAt(c.mLayerIndex)) { // kinda slow
                if (TileLayer *tl = layer->asTileLayer()) {
                    foreach (const MapChange::CellEntry &ce, c.mCells)
                        tl->setCell(ce.x, ce.y, ce.cell);
                    if (CompositeLayerGroup *layerGroup = sm.mMapComposite->layerGroupForLayer(tl))
                        layerGroup->regionAltered(tl); // possibly set mNeedsSynch
                }
            }
            break;
        }
        case MapChange::LotAdded: {
            sm.lotAdded(c.mLotInfo.id, c.mLotInfo.mapInfo, c.mLotInfo.pos, c.mLotInfo.level);
            break;
        }
        case MapChange::LotRemoved: {
            sm.lotRemoved(c.mLotInfo.id);
            break;
        }
        case MapChange::LotUpdated: {
            sm.lotUpdated(c.mLotInfo.id, c.mLotInfo.pos);
            break;
        }
        case MapChange::TilesetAdded: {
            sm.mMapComposite->map()->insertTileset(c.mTilesetIndex, c.mTileset);
            sm.mMapComposite->bmpBlender()->tilesetAdded(c.mTileset);
            break;
        }
        case MapChange::TilesetRemoved: {
            int index = sm.mMapComposite->map()->indexOfTileset(c.mTileset);
            sm.mMapComposite->map()->removeTilesetAt(index);
            sm.mMapComposite->bmpBlender()->tilesetRemoved(c.mTilesetName);
            break;
        }
        case MapChange::MapChanged: {
            // A loaded map changed in memory
            sm.mMapComposite->mapChanged(c.mLotInfo.mapInfo);
            break;
        }
        case MapChange::MapResized: {
            sm.mMapComposite->map()->setWidth(c.mMapSize.width());
            sm.mMapComposite->map()->setHeight(c.mMapSize.height());
            sm.mMapComposite->map()->rbmp(0).rimage() = c.mBmps[0];
            sm.mMapComposite->map()->rbmp(1).rimage() = c.mBmps[1];
            sm.mMapComposite->map()->rbmp(0).rrands().setSize(c.mMapSize.width(),
                                                              c.mMapSize.height());
            sm.mMapComposite->map()->rbmp(1).rrands().setSize(c.mMapSize.width(),
                                                              c.mMapSize.height());
            sm.mMapComposite->bmpBlender()->recreate();
            sm.mMapComposite->layerGroupForLevel(0)->setNeedsSynch(true);
            break;
        }
        case MapChange::BmpPainted: {
            sm.mMapComposite->map()->rbmp(c.mBmpIndex).rimage() = c.mBmps[c.mBmpIndex];
            QRect r = c.mRegion.boundingRect();
            sm.mMapComposite->bmpBlender()->update(r.x(), r.y(), r.right(), r.bottom());
            break;
        }
        case MapChange::BmpAliasesChanged: {
            sm.mMapComposite->map()->rbmpSettings()->setAliases(c.mBmpAliases);
            sm.mMapComposite->bmpBlender()->fromMap();
            sm.mMapComposite->bmpBlender()->recreate();
            redrawAll = true;
            break;
        }
        case MapChange::BmpRulesChanged: {
            sm.mMapComposite->map()->rbmpSettings()->setRules(c.mBmpRules);
            sm.mMapComposite->bmpBlender()->fromMap();
            sm.mMapComposite->bmpBlender()->recreate();
            redrawAll = true;
            break;
        }
        case MapChange::BmpBlendsChanged: {
            sm.mMapComposite->map()->rbmpSettings()->setBlends(c.mBmpBlends);
            sm.mMapComposite->bmpBlender()->fromMap();
            sm.mMapComposite->bmpBlender()->recreate();
            redrawAll = true;
            break;
        }
        case MapChange::Recreate:
            redrawAll = true;
            break;
        default:
            break;
        }
    }

    // Now all the changes are applied, see if the size of the image should change.
    mRenderer->setMaxLevel(mShadowMap->mMapComposite->maxLevel());
    sm.mMapComposite->synch();
    QRectF newBounds = sm.mMapComposite->boundingRect(mRenderer);
    if (oldBounds != newBounds) {
        int width = 512;
        qreal scale = width / newBounds.width();
        int height = qCeil(newBounds.height() * scale);
        mImage = QImage(width, height, mImage.format());
        emit imageResized(mImage.size());
        redrawAll = true;
    }

    // Figure out which areas to redraw. Changes to lot bounds must be recorded
    // even if redrawAll is true.
    foreach (MapChange *cp, changes) {
        const MapChange &c = *cp;
        QRectF dirty;
        switch (c.mChange) {
        case MapChange::LayerAdded:
            break;
        case MapChange::LayerRemoved:
            break;
        case MapChange::LayerRenamed:
            break;
        case MapChange::RegionAltered: {
            // The layer might not exist anymore, redrawAll catches that.
            if (redrawAll) break;
            QRect tileBounds;
            TileLayer *tl = sm.mMapComposite->map()->layerAt(c.mLayerIndex)->asTileLayer(); // kinda slow
            foreach (const MapChange::CellEntry &ce, c.mCells) {
                if (tileBounds.isEmpty())
                    tileBounds = QRect(ce.x, ce.y, 1, 1);
                else
                    tileBounds |= QRect(ce.x, ce.y, 1, 1);
            }
            QMargins margins = sm.mMapComposite->map()->drawMargins();
            QRectF r = mRenderer->boundingRect(tileBounds, tl->level());
            r.adjust(-margins.left(),
                     -margins.top(),
                     margins.right(),
                     margins.bottom());
            if (dirty.isEmpty())
                dirty = r;
            else
                dirty |= r;
            break;
        }
        case MapChange::LotAdded: {
            // The lot may have been removed in the loop above.
            if (!sm.mIDToLot.contains(c.mLotInfo.id)) break;
            QRectF bounds = sm.mIDToLot[c.mLotInfo.id]->boundingRect(mRenderer);
            dirty = bounds;
            mLotBounds[c.mLotInfo.id] = bounds;
            break;
        }
        case MapChange::LotRemoved: {
            dirty = mLotBounds[c.mLotInfo.id];
            mLotBounds.remove(c.mLotInfo.id);
            break;
        }
        case MapChange::LotUpdated: {
            // The lot may have been removed in the loop above.
            if (!sm.mIDToLot.contains(c.mLotInfo.id)) break;
            QRectF bounds = sm.mIDToLot[c.mLotInfo.id]->boundingRect(mRenderer);
            dirty = mLotBounds[c.mLotInfo.id] | bounds;
            mLotBounds[c.mLotInfo.id] = bounds;
            break;
        }
        case MapChange::MapChanged: {
            // The lot may have been removed in the loop above.
            if (!sm.mIDToLot.contains(c.mLotInfo.id)) break;
            // A loaded map changed in memory
            QRectF bounds = sm.mIDToLot[c.mLotInfo.id]->boundingRect(mRenderer);
            dirty = mLotBounds[c.mLotInfo.id] | bounds;
            mLotBounds[c.mLotInfo.id] = bounds;
            break;
        }
        case MapChange::TilesetChanged: {
            if (sm.mMapComposite->isTilesetUsed(c.mTileset, false))
                redrawAll = true;
            else {
                QRectF dirty2;
                foreach (MapComposite *mc, sm.mMapComposite->subMaps()) {
                    if (mc->isTilesetUsed(c.mTileset)) {
                        QRectF bounds = mc->boundingRect(mRenderer);
                        dirty2 = mLotBounds[sm.mLotToID[mc]] | bounds;
                        if (dirty.isEmpty())
                            dirty = dirty2;
                        else
                            dirty |= dirty2;
                        mLotBounds[sm.mLotToID[mc]] = bounds;
                    }
                }
            }
            break;
        }
        case MapChange::BmpPainted: {
            if (redrawAll) break;
            dirty = mRenderer->boundingRect(c.mRegion.boundingRect());
            QMargins margins = sm.mMapComposite->map()->drawMargins();
            dirty.adjust(-margins.left(), -margins.top(),
                         margins.right(), margins.bottom());
            break;
        }
        default:
            break;
        }
        if (mDirtyRect.isEmpty())
            mDirtyRect = dirty;
        else if (!dirty.isEmpty())
            mDirtyRect |= dirty;
    }

    if (redrawAll) {
        mDirtyRect = QRectF();
        mRedrawAll = true;
    }

    noise() << "MiniMapRenderWorker: applied" << mPendingChanges.size() << "changes" << this;
}

void MiniMapRenderWorker::resume()
{
//    scheduleWork();
}

void MiniMapRenderWorker::setVisible(bool visible)
{
    if (visible == mVisible)
        return;
    mVisible = visible;
    if (mVisible)
        scheduleWork();
}

void MiniMapRenderWorker::returnToAppThread()
{
    IN_WORKER_THREAD
    moveToThread(qApp->thread());
}

/////

MiniMapItem::MiniMapItem(ZomboidScene *zscene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(zscene)
    , mMapImage(0)
    , mMiniMapVisible(false)
    , mNeedsResume(false)
{
    mMapComposite = mScene->mapDocument()->mapComposite();

    qRegisterMetaType<QList<MapChange*> >("QList<MapChange*>");
    mRenderThread = new InterruptibleThread;
    mRenderWorker = new MiniMapRenderWorker(mMapComposite->mapInfo(),
                                            mRenderThread);
    mRenderWorker->moveToThread(mRenderThread);
    connect(mRenderWorker, SIGNAL(painted(QImage,QRectF)),
            SLOT(painted(QImage,QRectF)));
    connect(mRenderWorker, SIGNAL(imageResized(QSize)),
            SLOT(imageResized(QSize)));
    mRenderThread->start();

    connect(mScene->mapDocument(), SIGNAL(layerAdded(int)),
            SLOT(layerAdded(int)));
    connect(mScene->mapDocument(), SIGNAL(layerRemoved(int)),
            SLOT(layerRemoved(int)));
    connect(mScene->mapDocument(), SIGNAL(layerRenamed(int)),
            SLOT(layerRenamed(int)));
    connect(mScene->mapDocument(), SIGNAL(regionAltered(QRegion,Layer*)),
            SLOT(regionAltered(QRegion,Layer*)));
    connect(mScene->mapDocument(), SIGNAL(mapChanged()),
            SLOT(mapChanged()));

    connect(MapManager::instance(), SIGNAL(mapAboutToChange(MapInfo*)),
            SLOT(mapAboutToChange(MapInfo*)));
    connect(MapManager::instance(), SIGNAL(mapChanged(MapInfo*)),
            SLOT(mapChanged(MapInfo*)));

    connect(&mScene->lotManager(), SIGNAL(lotAdded(MapComposite*,Tiled::MapObject*)),
        SLOT(lotAdded(MapComposite*,Tiled::MapObject*)));
    connect(&mScene->lotManager(), SIGNAL(lotRemoved(MapComposite*,Tiled::MapObject*)),
        SLOT(lotRemoved(MapComposite*,Tiled::MapObject*)));
    connect(&mScene->lotManager(), SIGNAL(lotUpdated(MapComposite*,Tiled::MapObject*)),
        SLOT(lotUpdated(MapComposite*,Tiled::MapObject*)));

    connect(mScene->mapDocument(), SIGNAL(tilesetAdded(int,Tileset*)),
            SLOT(tilesetAdded(int,Tileset*)));
    connect(mScene->mapDocument(), SIGNAL(tilesetRemoved(Tileset*)),
            SLOT(tilesetRemoved(Tileset*)));
    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(mScene->mapDocument(), SIGNAL(bmpPainted(int,QRegion)),
            SLOT(bmpPainted(int,QRegion)));
    connect(mScene->mapDocument(), SIGNAL(bmpAliasesChanged()),
            SLOT(bmpAliasesChanged()));
    connect(mScene->mapDocument(), SIGNAL(bmpRulesChanged()),
            SLOT(bmpRulesChanged()));
    connect(mScene->mapDocument(), SIGNAL(bmpBlendsChanged()),
            SLOT(bmpBlendsChanged()));

    QMap<MapObject*,MapComposite*>::const_iterator it;
    const QMap<MapObject*,MapComposite*> &map = mScene->lotManager().objectToLot();
    for (it = map.constBegin(); it != map.constEnd(); it++) {
        lotAdded(it.value(), it.key());
    }
}

MiniMapItem::~MiniMapItem()
{
    mRenderThread->interrupt();
    QMetaObject::invokeMethod(mRenderWorker, "returnToAppThread", Qt::QueuedConnection);
    while (mRenderWorker->thread() != qApp->thread())
        Sleep::msleep(1);
    mRenderThread->quit();
    mRenderThread->wait();
    delete mRenderWorker;
    delete mRenderThread;
}

QRectF MiniMapItem::boundingRect() const
{
    return mMapImageBounds;
}

void MiniMapItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!mMapImage.isNull()) {
        QRectF target = mMapImageBounds;
        QRectF source = QRect(QPoint(0, 0), mMapImage.size());
        painter->drawImage(target, mMapImage, source);
    }
#ifndef QT_NO_DEBUG
    painter->drawRect(mMapImageBounds);
#endif
}

void MiniMapItem::recreateImage()
{
    MapChange *c = new MapChange(MapChange::Recreate);
    queueChange(c);
}

void MiniMapItem::minimapVisibilityChanged(bool visible)
{
    mMiniMapVisible = visible;
    QMetaObject::invokeMethod(mRenderWorker, "setVisible", Qt::QueuedConnection,
                              Q_ARG(bool,visible));
}

void MiniMapItem::queueChange(MapChange *c)
{
    mPendingChanges += c;
    if (mPendingChanges.size() == 1) {
//        mRenderThread->interrupt();
//        mNeedsResume = true;
        QMetaObject::invokeMethod(this, "queueChanges", Qt::QueuedConnection);
    }
}

void MiniMapItem::layerAdded(int index)
{
    MapChange *c = new MapChange(MapChange::LayerAdded);
    c->mLayer = mMapComposite->map()->layerAt(index)->clone();
    c->mLayerIndex = index;
    queueChange(c);
}

void MiniMapItem::layerRemoved(int index)
{
    MapChange *c = new MapChange(MapChange::LayerRemoved);
    c->mLayerIndex = index;
    queueChange(c);
}

void MiniMapItem::layerRenamed(int index)
{
    MapChange *c = new MapChange(MapChange::LayerRenamed);
    c->mLayerIndex = index;
    c->mName = mMapComposite->map()->layerAt(index)->name();
    queueChange(c);
}

void MiniMapItem::lotAdded(MapComposite *lot, Tiled::MapObject *mapObject)
{
    mLots[lot] = mapObject;
    MapChange *c = new MapChange(MapChange::LotAdded);
    c->mLotInfo.id = quintptr(mapObject);
    c->mLotInfo.mapInfo = lot->mapInfo();
    c->mLotInfo.level = lot->levelOffset();
    c->mLotInfo.pos = lot->origin();
    queueChange(c);
}

void MiniMapItem::lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject)
{
    mLots.remove(lot);
    MapChange *c = new MapChange(MapChange::LotRemoved);
    c->mLotInfo.id = quintptr(mapObject);
    queueChange(c);
}

void MiniMapItem::lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject)
{
    MapChange *c = new MapChange(MapChange::LotUpdated);
    c->mLotInfo.id = quintptr(mapObject);
    c->mLotInfo.pos = lot->origin();
    queueChange(c);
}

void MiniMapItem::regionAltered(const QRegion &region, Layer *layer)
{
    if (!layer->asTileLayer()) return;
    QRegion clipped = region & layer->bounds();
    if (clipped.isEmpty()) return;
    MapChange *c = new MapChange(MapChange::RegionAltered);
    c->mLayerIndex = mMapComposite->map()->layers().indexOf(layer);
    foreach (const QRect &r, clipped.rects()) {
        for (int y = r.y(); y <= r.bottom(); y++) {
            for (int x = r.x(); x <= r.right(); x++) {
                c->mCells += MapChange::CellEntry(x, y, layer->asTileLayer()->cellAt(x, y));
            }
        }
    }
    queueChange(c);
}

// A loaded map (not the one being edited) is about to change.
// If the map is being used as a lot, stop rendering immediately.
void MiniMapItem::mapAboutToChange(MapInfo *mapInfo)
{
    foreach (MapComposite *mc, mMapComposite->subMaps()) {
        if (mapInfo == mc->mapInfo()) {
            mRenderThread->interrupt(true);
            // do not resume painting until resume()
            break;
        }
    }
}

void MiniMapItem::mapChanged(MapInfo *mapInfo)
{
    bool affected = false;
    foreach (MapComposite *lot, mMapComposite->subMaps()) {
        // There could be several lots using the same map
        if (lot->mapInfo() == mapInfo) {
            MapChange *c = new MapChange(MapChange::MapChanged);
            c->mLotInfo.id = quintptr(mLots[lot]);
            c->mLotInfo.mapInfo = mapInfo;
            queueChange(c);
            affected = true;
        }
    }
    if (affected) {
        mNeedsResume = true;
    }
}

// called after the map being edited is resized
void MiniMapItem::mapChanged()
{
    MapChange *c = new MapChange(MapChange::MapResized);
    c->mMapSize = mMapComposite->map()->size();
    c->mBmps[0] = mMapComposite->map()->bmp(0).image();
    c->mBmps[1] = mMapComposite->map()->bmp(1).image();
    queueChange(c);
}

void MiniMapItem::tilesetAdded(int index, Tileset *tileset)
{
    TilesetManager::instance()->addReference(tileset); // for ShadowMap
    MapChange *c = new MapChange(MapChange::TilesetAdded);
    c->mTileset = tileset;
    c->mTilesetIndex = index;
    queueChange(c);
}

void MiniMapItem::tilesetRemoved(Tileset *tileset)
{
    mRenderThread->interrupt(true);
    mNeedsResume = true;

    MapChange *c = new MapChange(MapChange::TilesetRemoved);
    c->mTileset = tileset; // THIS MAY BE DELETED BY removeReference() below!
    c->mTilesetName = tileset->name();
    queueChange(c);

    TilesetManager::instance()->removeReference(tileset); // for ShadowMap
}

void MiniMapItem::tilesetChanged(Tileset *tileset)
{
    // FIXME: stop rendering *before* it changes, need tilesetAboutToChange()
    if (mMapComposite->isTilesetUsed(tileset)) {
        MapChange *c = new MapChange(MapChange::TilesetChanged);
        c->mTileset = tileset;
        queueChange(c);
    }
}

void MiniMapItem::bmpPainted(int bmpIndex, const QRegion &region)
{
    MapChange *c = new MapChange(MapChange::BmpPainted);
    c->mBmpIndex = bmpIndex;
    c->mBmps[bmpIndex] = mMapComposite->map()->rbmp(bmpIndex).rimage().copy(); // FIXME: send changed part only
    c->mRegion = region;
    queueChange(c);
}

void MiniMapItem::bmpAliasesChanged()
{
    MapChange *c = new MapChange(MapChange::BmpAliasesChanged);
    c->mBmpAliases = mMapComposite->map()->bmpSettings()->aliasesCopy();
    queueChange(c);
}

void MiniMapItem::bmpRulesChanged()
{
    MapChange *c = new MapChange(MapChange::BmpRulesChanged);
    c->mBmpRules = mMapComposite->map()->bmpSettings()->rulesCopy();
    queueChange(c);
}

void MiniMapItem::bmpBlendsChanged()
{
    MapChange *c = new MapChange(MapChange::BmpBlendsChanged);
    c->mBmpBlends = mMapComposite->map()->bmpSettings()->blendsCopy();
    queueChange(c);
}

void MiniMapItem::painted(QImage image, QRectF sceneRect)
{
#if 1
    if (mMapImage.size() != image.size()) {
        mMapImageBounds = sceneRect;
        emit resized(image.size(), sceneRect); // update the minimap scene + view
    }
    mMapImage = image;
#else
    QPainter painter(&mMapImage);
    painter.drawImage(r, image, r);
#endif

    // There seems to be a conflict with QGraphicsScene's use of QueuedConnection.
    // Since this slot is called as a QueuedConnection (due to the render thread
    // emitting it signal) I'm calling update in the middle of QGraphicsScene
    // handling its own _q_emitUpdated and _q_processDirtyItems.  I'm hitting
    // Q_ASSERT(calledEmitUpdated) in _q_processDirtyItems.
    update(sceneRect);
}

void MiniMapItem::imageResized(QSize sz)
{
    Q_UNUSED(sz)
    // the image was resized, but isn't painted yet
    // wait until the image is painted before updated the scene + view
}

void MiniMapItem::queueChanges()
{
    if (mNeedsResume) {
        mNeedsResume = false;
        mRenderThread->resume();
        QMetaObject::invokeMethod(mRenderWorker, "resume", Qt::QueuedConnection);
    }

    // FIXME: merge sequences of RegionAltered changes into a single change.
    // Maybe use a SparseTileLayerGrid to pass changes.
    QMetaObject::invokeMethod(mRenderWorker, "applyChanges", Qt::QueuedConnection,
                              Q_ARG(QList<MapChange*>,mPendingChanges));
    mPendingChanges.clear();
}

/////

MiniMap::MiniMap(MapView *parent)
    : QGraphicsView(parent)
    , mMapView(parent)
    , mButtons(new QFrame(this))
    , mWidth(256)
    , mHeight(128)
    , mViewportItem(0)
    , mExtraItem(0)
    , mBiggerButton(new QToolButton(mButtons))
    , mSmallerButton(new QToolButton(mButtons))
{
    setFrameStyle(NoFrame);

    // For the smaller/bigger buttons
    setMouseTracking(true);

    setRenderHint(QPainter::SmoothPixmapTransform, true);

    Preferences *prefs = Preferences::instance();
    setVisible(prefs->showMiniMap());
    mRatio = 1.5;
    mWidth = prefs->miniMapWidth();
    mHeight = mWidth / mRatio;
    connect(prefs, SIGNAL(showMiniMapChanged(bool)), SLOT(setVisible(bool)));
    connect(prefs, SIGNAL(miniMapWidthChanged(int)), SLOT(widthChanged(int)));

    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::gray);
    // Set the sceneRect explicitly so it doesn't grow itself
    scene->setSceneRect(0, 0, 1, 1);
    setScene(scene);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    mViewportItem = new QGraphicsPolygonItem();
    mViewportItem->setPen(QPen(Qt::white));
    mViewportItem->setZValue(100);
    scene->addItem(mViewportItem);

    QHBoxLayout *layout = new QHBoxLayout(mButtons);
    layout->setContentsMargins(2, 2, 0, 0);
    layout->setSpacing(2);
    mButtons->setLayout(layout);
    mButtons->setVisible(false);

    QToolButton *button = mSmallerButton;
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/zoom-out.png")));
    button->setToolTip(tr("Make the MiniMap smaller"));
    connect(button, SIGNAL(clicked()), SLOT(smaller()));
    layout->addWidget(button);

    button = mBiggerButton;
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/zoom-in.png")));
    button->setToolTip(tr("Make the MiniMap larger"));
    connect(button, SIGNAL(clicked()), SLOT(bigger()));
    layout->addWidget(button);

    button = new QToolButton(mButtons);
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/edit-redo.png")));
    button->setToolTip(tr("Refresh the MiniMap image"));
    connect(button, SIGNAL(clicked()), SLOT(updateImage()));
    layout->addWidget(button);

    setGeometry(20, 20, mWidth, mHeight);
}

void MiniMap::setMapScene(MapScene *scene)
{
    mMapScene = scene;
    widthChanged(mWidth);
}

void MiniMap::viewRectChanged()
{
    QRect rect = mMapView->rect();

    int hsbh = mMapView->horizontalScrollBar()->isVisible()
            ? mMapView->horizontalScrollBar()->height() : 0;
    int vsbw = mMapView->verticalScrollBar()->isVisible()
            ? mMapView->verticalScrollBar()->width() : 0;
    rect.adjust(0, 0, -vsbw, -hsbh);

    QPolygonF polygon = mMapView->mapToScene(rect); // FIXME
    mViewportItem->setPolygon(polygon);
}

void MiniMap::setExtraItem(MiniMapItem *item)
{
    mExtraItem = item;
    scene()->addItem(mExtraItem);
    connect(item, SIGNAL(resized(QSize,QRectF)), SLOT(miniMapItemResized(QSize,QRectF)));
}

void MiniMap::bigger()
{
    Preferences::instance()->setMiniMapWidth(qMin(mWidth + 32, MINIMAP_MAX_WIDTH));
}

void MiniMap::smaller()
{
    Preferences::instance()->setMiniMapWidth(qMax(mWidth - 32, MINIMAP_MIN_WIDTH));
}

void MiniMap::updateImage()
{
    mExtraItem->recreateImage();
}

void MiniMap::widthChanged(int width)
{
    mWidth = width;
    mHeight = qCeil(mWidth / mRatio);

    // No idea where the extra 3 pixels is coming from...
    setGeometry(20, 20, mWidth + 3, mHeight + 3);

    qreal scale = mWidth / scene()->sceneRect().width();
    setTransform(QTransform::fromScale(scale, scale));

    mBiggerButton->setEnabled(mWidth < MINIMAP_MAX_WIDTH);
    mSmallerButton->setEnabled(mWidth > MINIMAP_MIN_WIDTH);
}

void MiniMap::miniMapItemResized(const QSize &imageSize, const QRectF &sceneRect)
{
    mRatio = sceneRect.width() / sceneRect.height();
    mHeight = qCeil(mWidth / mRatio);

    // No idea where the extra 3 pixels is coming from...
    setGeometry(20, 20, mWidth + 3, mHeight + 3);

    qreal scale = mWidth / sceneRect.width();
    setTransform(QTransform::fromScale(scale, scale));
    scene()->setSceneRect(sceneRect);

    viewRectChanged();
}

bool MiniMap::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter:
        break;
    case QEvent::Leave:
        mButtons->setVisible(false);
        break;
    default:
        break;
    }

    return QGraphicsView::event(event);
}

void MiniMap::showEvent(QShowEvent *event)
{
    Q_UNUSED(event)
    if (mExtraItem)
        mExtraItem->minimapVisibilityChanged(true);
}

void MiniMap::hideEvent(QHideEvent *event)
{
    Q_UNUSED(event)
    if (mExtraItem)
        mExtraItem->minimapVisibilityChanged(false);
}

void MiniMap::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mMapView->centerOn(mapToScene(event->pos())); // FIXME
}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        mMapView->centerOn(mapToScene(event->pos())); // FIXME
    else {
        QRect hotSpot = mButtons->rect().adjusted(0, 0, 12, 12);
        mButtons->setVisible(hotSpot.contains(event->pos()));
    }
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}
