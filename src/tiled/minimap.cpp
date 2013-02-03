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

/////

ShadowMap::ShadowMap(MapInfo *mapInfo)
{
    mMaster = mapInfo->map();
    Map *map = mMaster->clone();
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
    MapInfo *mapInfo = mMapComposite->mapInfo();
    delete mMapComposite;
    TilesetManager::instance()->removeReferences(mapInfo->map()->tilesets());
    delete mapInfo->map();
    delete mapInfo;
}

void ShadowMap::layerAdded(int index)
{
    Layer *layer = mMaster->layerAt(index)->clone();
    layer->setVisible(true);
    mMapComposite->map()->insertLayer(index, layer);
    mMapComposite->layerAdded(index);
}

void ShadowMap::layerRemoved(int index)
{
    mMapComposite->layerAboutToBeRemoved(index);
    delete mMapComposite->map()->takeLayerAt(index);
}

void ShadowMap::layerRenamed(int index)
{
    mMapComposite->layerRenamed(index);
}

void ShadowMap::regionAltered(const QRegion &rgn, Layer *layer)
{
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
}

void ShadowMap::lotAdded(MapComposite *lot, MapObject *mapObject)
{
    MapComposite *subMap = mMapComposite->addMap(lot->mapInfo(),
                                                 lot->origin(),
                                                 lot->levelOffset());
    mLots[quintptr(mapObject)] = subMap;
}

void ShadowMap::lotRemoved(MapComposite *lot, MapObject *mapObject)
{
    Q_UNUSED(lot)
    mMapComposite->removeMap(mLots[quintptr(mapObject)]);
    mLots.remove(quintptr(mapObject));
}

void ShadowMap::lotUpdated(MapComposite *lot, MapObject *mapObject)
{
    mLots[quintptr(mapObject)]->setOrigin(lot->origin());
}

void ShadowMap::mapAboutToChange(MapInfo *mapInfo)
{
    mMapComposite->mapAboutToChange(mapInfo);
}

void ShadowMap::mapChanged(MapInfo *mapInfo)
{
    mMapComposite->mapChanged(mapInfo);
    foreach (CompositeLayerGroup *lg, mMapComposite->layerGroups())
        lg->synch();
}

/////

MapRenderThread::MapRenderThread(MapComposite *mapComposite) :
    mRestart(false),
    mWaiting(false),
    mQuit(false),
    mAbortDrawing(false)
{
    mShadowMap = new ShadowMap(mapComposite->mapInfo());
    (mapComposite->map()->orientation() == Map::Isometric) ?
                mRenderer = new IsometricRenderer(mShadowMap->mMapComposite->map()) :
            mRenderer = new ZLevelRenderer(mShadowMap->mMapComposite->map());
    mRenderer->setMaxLevel(mShadowMap->mMapComposite->maxLevel());
    mRenderer->mAbortDrawing = &mAbortDrawing;
}

MapRenderThread::~MapRenderThread()
{
    if (isRunning()) {
        mSharedVarsMutex.lock();
        mAbortDrawing = true;
        mQuit = true;
        if (mWaiting) {
            mRenderSleepMutex.lock();
            mRenderSleepMutex.unlock();
            mRenderSleepCondition.wakeOne();
        }
        mSharedVarsMutex.unlock();
        wait();
    }
    delete mRenderer;
    delete mShadowMap;
}

void MapRenderThread::run()
{
    forever {
        // Don't let the main thread modify mShadowMap or read/modify mImage until
        // painting is finished or aborted.
        QMutexLocker mapAndImageLocker(&mMapAndImageMutex);

        mRenderer->setMaxLevel(mShadowMap->mMapComposite->maxLevel());

        QRectF sceneRect = mShadowMap->mMapComposite->boundingRect(mRenderer);
        QSize mapSize = sceneRect.size().toSize();
        if (mapSize.isEmpty())
            return;

        mSharedVarsMutex.lock();
        QRectF paintRect = mDirtyRect;
        if (paintRect.isEmpty())
            paintRect = sceneRect;
        mSharedVarsMutex.unlock();

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

        MapComposite::ZOrderList zorder = mShadowMap->mMapComposite->zOrder();
        foreach (MapComposite::ZOrderItem zo, zorder) {
            if (zo.group)
                mRenderer->drawTileLayerGroup(&painter, zo.group, paintRect);
            else if (TileLayer *tl = zo.layer->asTileLayer()) {
                if (tl->name().contains(QLatin1String("NoRender")))
                    continue;
                mRenderer->drawTileLayer(&painter, tl, paintRect);
            }

            QMutexLocker locker(&mSharedVarsMutex);
            if (mQuit) {
                qDebug() << "MapRenderThread quitting" << this;
                return;
            }
            if (mAbortDrawing) {
                qDebug() << "MapRenderThread aborted" << this;
                break;
            }
            if (mRestart) {
                qDebug() << "MapRenderThread restarting" << this;
                break;
            }
        }

        // mImage is the painter's paint-device.  Since the image may be
        // recreated before the painter's destructor is called, we must
        // release it's paint device now.
        painter.end();

        mapAndImageLocker.unlock();

        qDebug() << "MapRenderThread painting took" << timer.elapsed() << "milliseconds" << this;

        mSharedVarsMutex.lock();
        if (mQuit) {
            mSharedVarsMutex.unlock();
            qDebug() << "MapRenderThread quitting" << this;
            return;
        }
        if (!mRestart && !mAbortDrawing) {
            emit rendered(this, paintRect);
            mDirtyRect = QRectF();
        }

        if (!mRestart) {
            mWaiting = true;
            mRenderSleepMutex.lock();
            mSharedVarsMutex.unlock();
            qDebug() << "MapRenderThread sleeping" << this;
            mRenderSleepCondition.wait(&mRenderSleepMutex);
            qDebug() << "MapRenderThread waking" << this;
            mSharedVarsMutex.lock();
            mRenderSleepMutex.unlock();
            mWaiting = false;
        }
        mRestart = false;
        mAbortDrawing = false;
        mSharedVarsMutex.unlock();
        if (mQuit) {
            qDebug() << "MapRenderThread quitting" << this;
            return;
        }
    }
}

void MapRenderThread::update(const QRectF &rect)
{
    QMutexLocker locker(&mSharedVarsMutex);
    if (!isRunning())
        start();
    else {
        if (rect.isEmpty())
            mDirtyRect = QRectF(); // redraw everything
        else
            mDirtyRect |= rect; // draw new area plus any interrupted area
        if (mWaiting) {
            mRenderSleepMutex.lock();
            mRenderSleepMutex.unlock();
            mRenderSleepCondition.wakeOne();
        } else {
            mAbortDrawing = true;
            mRestart = true;
        }
    }
}

void MapRenderThread::recreateImage(const QImage *other)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mImage = QImage(other->size(), other->format());
}

void MapRenderThread::abortDrawing()
{
    if (!isRunning())
        return;

    QMutexLocker locker(&mSharedVarsMutex);
    mAbortDrawing = true;
    mRestart = false;
}

void MapRenderThread::layerAdded(int index)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->layerAdded(index);
}

void MapRenderThread::layerRemoved(int index)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->layerRemoved(index);
}

void MapRenderThread::layerRenamed(int index)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->layerRenamed(index); // layer added/removed from group
}

void MapRenderThread::regionAltered(const QRegion &rgn, Layer *layer)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->regionAltered(rgn, layer);
}

void MapRenderThread::lotAdded(MapComposite *lot, MapObject *mapObject)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->lotAdded(lot, mapObject);
}

void MapRenderThread::lotRemoved(MapComposite *lot, MapObject *mapObject)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->lotRemoved(lot, mapObject);
}

void MapRenderThread::lotUpdated(MapComposite *lot, MapObject *mapObject)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->lotUpdated(lot, mapObject);
}

void MapRenderThread::mapAboutToChange(MapInfo *mapInfo)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->mapAboutToChange(mapInfo);
}

void MapRenderThread::mapChanged(MapInfo *mapInfo)
{
    abortDrawing();
    QMutexLocker locker(&mMapAndImageMutex);
    mShadowMap->mapChanged(mapInfo);
}

void MapRenderThread::getImage(QImage &dest)
{
    QMutexLocker locker(&mMapAndImageMutex);
    dest = mImage;
}

/////

MiniMapItem::MiniMapItem(ZomboidScene *zscene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(zscene)
    , mRenderer(mScene->mapDocument()->renderer())
    , mMapImage(0)
    , mMiniMapVisible(false)
    , mUpdatePending(false)
    , mNeedsRecreate(false)
    , mRedrawAll(false)
    , mRenderThread(0)
{
    mMapComposite = mScene->mapDocument()->mapComposite();

    mRenderThread = new MapRenderThread(mMapComposite);
    connect(mRenderThread, SIGNAL(rendered(MapRenderThread*,QRectF)),
            SLOT(rendered(MapRenderThread*,QRectF)));

    connect(mScene, SIGNAL(sceneRectChanged(QRectF)),
            SLOT(sceneRectChanged(QRectF)));

    connect(mScene->mapDocument(), SIGNAL(layerAdded(int)),
            SLOT(layerAdded(int)));
    connect(mScene->mapDocument(), SIGNAL(layerRemoved(int)),
            SLOT(layerRemoved(int)));
    connect(mScene->mapDocument(), SIGNAL(regionAltered(QRegion,Layer*)),
            SLOT(regionAltered(QRegion,Layer*)));

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

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    QMap<MapObject*,MapComposite*>::const_iterator it;
    const QMap<MapObject*,MapComposite*> &map = mScene->lotManager().objectToLot();
    for (it = map.constBegin(); it != map.constEnd(); it++) {
        MapComposite *lot = it.value();
        mLotBounds[lot] = lot->boundingRect(mRenderer);
        mRenderThread->lotAdded(lot, it.key());
    }

    recreateLater();
}

MiniMapItem::~MiniMapItem()
{
    delete mMapImage;
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

    if (mMapImage) {
        QRectF target = mMapImageBounds;
        QRectF source = QRect(QPoint(0, 0), mMapImage->size());
        painter->drawImage(target, *mMapImage, source);
    }
#if _DEBUG
    painter->drawRect(mMapImageBounds);
#endif
}

void MiniMapItem::updateImage(const QRectF &dirtyRect)
{
    Q_ASSERT(mMapImage);
    mRenderThread->update(dirtyRect);
}

void MiniMapItem::updateImageBounds()
{
    QRectF bounds = mScene->sceneRect();
    if (bounds != mMapImageBounds) {
        prepareGeometryChange();
        mMapImageBounds = bounds;
    }
}

void MiniMapItem::recreateImage()
{
    delete mMapImage;

    QSizeF mapSize = mScene->sceneRect().size();
    qreal scale = 512.0 / qreal(mapSize.width());
    QSize imageSize = (mapSize * scale).toSize();

    mMapImage = new QImage(imageSize, QImage::Format_ARGB32);
    mMapImage->fill(Qt::transparent);

    mRenderThread->recreateImage(mMapImage);

    updateImage();

    updateImageBounds();
}

void MiniMapItem::minimapVisibilityChanged(bool visible)
{
    mMiniMapVisible = visible;
    if (mMiniMapVisible)
        updateNow();
}

void MiniMapItem::updateLater(const QRectF &dirtyRect)
{
    if (mNeedsRecreate || dirtyRect.isEmpty())
        mRedrawAll = true;
    else if (mNeedsUpdate.isEmpty())
        mNeedsUpdate = dirtyRect;
    else
        mNeedsUpdate |= dirtyRect;

    if (!mMiniMapVisible)
        return;

    if (mUpdatePending)
        return;
    mUpdatePending = true;
    QMetaObject::invokeMethod(this, "updateNow", Qt::QueuedConnection);
}

void MiniMapItem::recreateLater()
{
    mNeedsRecreate = true;
    updateLater();
}

void MiniMapItem::sceneRectChanged(const QRectF &sceneRect)
{
    Q_UNUSED(sceneRect)
    recreateLater();
}

void MiniMapItem::layerAdded(int index)
{
    Q_UNUSED(index)
    mRenderThread->layerAdded(index);
    recreateLater();
}

void MiniMapItem::layerRemoved(int index)
{
    Q_UNUSED(index)
    mRenderThread->layerRemoved(index);
    recreateLater();
}

void MiniMapItem::lotAdded(MapComposite *lot, Tiled::MapObject *mapObject)
{
    mRenderThread->lotAdded(lot, mapObject);
    QRectF bounds = lot->boundingRect(mRenderer);
    updateLater(mLotBounds[lot] | bounds);
    mLotBounds[lot] = bounds;
}

void MiniMapItem::lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject)
{
    mRenderThread->lotRemoved(lot, mapObject);
    updateLater(mLotBounds[lot]);
    mLotBounds.remove(lot);
}

void MiniMapItem::lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject)
{
    mRenderThread->lotUpdated(lot, mapObject);
    QRectF bounds = lot->boundingRect(mRenderer);
    updateLater(mLotBounds[lot] | bounds);
    mLotBounds[lot] = bounds;
}

void MiniMapItem::regionAltered(const QRegion &region, Layer *layer)
{
    mRenderThread->regionAltered(region, layer);

    QMargins margins = mMapComposite->map()->drawMargins();
    foreach (const QRect &r, region.rects()) {
        QRectF bounds = mRenderer->boundingRect(r, layer->level());
        bounds.adjust(-margins.left(),
                      -margins.top(),
                      margins.right(),
                      margins.bottom());
        updateLater(bounds);
    }
}

void MiniMapItem::mapAboutToChange(MapInfo *mapInfo)
{
    mRenderThread->mapAboutToChange(mapInfo);
}

void MiniMapItem::mapChanged(MapInfo *mapInfo)
{
    bool affected = false;
    foreach (MapComposite *lot, mMapComposite->subMaps()) {
        if (lot->mapInfo() == mapInfo) {
            QRectF bounds = lot->boundingRect(mRenderer);
            updateLater(mLotBounds[lot] | bounds);
            affected = true;
        }
    }
    if (affected)
        mRenderThread->mapChanged(mapInfo);
}

void MiniMapItem::tilesetChanged(Tileset *ts)
{
    foreach (MapComposite *mc, mMapComposite->maps()) {
        if (mc->map()->isTilesetUsed(ts)) {
            updateLater();
            return;
        }
    }
}

void MiniMapItem::updateNow()
{
    if (mNeedsRecreate) {
        recreateImage();
        update();
    } else if (mRedrawAll) {
        updateImage();
    } else if (!mNeedsUpdate.isEmpty()) {
        updateImage(mNeedsUpdate);
        update(mNeedsUpdate);
    }
    mNeedsRecreate = false;
    mNeedsUpdate = QRectF();
    mRedrawAll = false;
    mUpdatePending = false;
}

void MiniMapItem::rendered(MapRenderThread *t, const QRectF r)
{
    t->getImage(*mMapImage);

    // There seems to be a conflict with QGraphicsScene's use of QueuedConnection.
    // Since this slot is called as a QueuedConnection (due to the render thread
    // emitting it signal) I'm calling update in the middle of QGraphicsScene
    // handling its own _q_emitUpdated and _q_processDirtyItems.  I'm hitting
    // Q_ASSERT(calledEmitUpdated) in _q_processDirtyItems.
    update(r);
}

/////

MiniMap::MiniMap(MapView *parent)
    : QGraphicsView(parent)
    , mMapView(parent)
    , mButtons(new QFrame(this))
    , mWidth(256.0)
    , mViewportItem(0)
    , mExtraItem(0)
    , mBiggerButton(new QToolButton(mButtons))
    , mSmallerButton(new QToolButton(mButtons))
{
    setFrameStyle(NoFrame);

    // For the smaller/bigger buttons
    setMouseTracking(true);

    Preferences *prefs = Preferences::instance();
    setVisible(prefs->showMiniMap());
    mWidth = prefs->miniMapWidth();
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

    setGeometry(20, 20, 220, 220);
}

void MiniMap::setMapScene(MapScene *scene)
{
    mMapScene = scene;
    widthChanged(mWidth);
    connect(mMapScene, SIGNAL(sceneRectChanged(QRectF)),
            SLOT(sceneRectChanged(QRectF)));
}

void MiniMap::viewRectChanged()
{
    QRect rect = mMapView->rect();

    int hsbh = mMapView->horizontalScrollBar()->isVisible()
            ? mMapView->horizontalScrollBar()->height() : 0;
    int vsbw = mMapView->verticalScrollBar()->isVisible()
            ? mMapView->verticalScrollBar()->width() : 0;
    rect.adjust(0, 0, -vsbw, -hsbh);

    QPolygonF polygon = mMapView->mapToScene(rect);
    mViewportItem->setPolygon(polygon);
}

void MiniMap::setExtraItem(MiniMapItem *item)
{
    mExtraItem = item;
    scene()->addItem(mExtraItem);
}

// FIXME: the size of the minimap should only be affected by the size of the
// image, not the size of the MapScene's sceneRect.
void MiniMap::sceneRectChanged(const QRectF &sceneRect)
{
    qreal scale = this->scale();
    QSizeF size = sceneRect.size() * scale;
    // No idea where the extra 3 pixels is coming from...
    int width = std::ceil(size.width()) + 3;
    int height = std::ceil(size.height()) + 3;
    setGeometry(20, 20, width, height);

    setTransform(QTransform::fromScale(scale, scale));
    scene()->setSceneRect(sceneRect);

    viewRectChanged();
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
    if (mExtraItem) {
        mExtraItem->updateImage();
        mExtraItem->update();
    }
}

void MiniMap::widthChanged(int width)
{
    mWidth = width;
    sceneRectChanged(mMapScene->sceneRect());

    mBiggerButton->setEnabled(mWidth < MINIMAP_MAX_WIDTH);
    mSmallerButton->setEnabled(mWidth > MINIMAP_MIN_WIDTH);
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
        mMapView->centerOn(mapToScene(event->pos()));}

void MiniMap::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        mMapView->centerOn(mapToScene(event->pos()));
    else {
        QRect hotSpot = mButtons->rect().adjusted(0, 0, 12, 12);
        mButtons->setVisible(hotSpot.contains(event->pos()));
    }
}

void MiniMap::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

qreal MiniMap::scale()
{
    QRectF sceneRect = mMapScene->sceneRect();
    QSizeF size = sceneRect.size();
    if (size.isEmpty())
        return 1.0;
    return (size.width() > size.height()) ? mWidth / size.width()
                                          : mWidth / size.height();
}
