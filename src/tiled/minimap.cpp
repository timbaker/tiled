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

#include "map.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "maprenderer.h"
#include "mapview.h"
#include "preferences.h"
#include "tilelayer.h"
#include "ZomboidScene.h"

#include <QMouseEvent>
#include <QGraphicsPolygonItem>
#include <QGLWidget>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QToolButton>
#include <cmath>

using namespace Tiled;
using namespace Tiled::Internal;

/////

MiniMapItem::MiniMapItem(ZomboidScene *zscene, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mScene(zscene)
    , mRenderer(mScene->mapDocument()->renderer())
    , mMapImage(0)
    , mMiniMapVisible(false)
    , mUpdatePending(false)
    , mNeedsRecreate(false)
{
    mMapComposite = mScene->mapDocument()->mapComposite();

    connect(mScene, SIGNAL(sceneRectChanged(QRectF)),
            SLOT(sceneRectChanged(QRectF)));

    connect(mScene->mapDocument(), SIGNAL(layerAdded(int)),
            SLOT(layerAdded(int)));
    connect(mScene->mapDocument(), SIGNAL(layerRemoved(int)),
            SLOT(layerRemoved(int)));
    connect(mScene->mapDocument(), SIGNAL(regionAltered(QRegion,Layer*)),
            SLOT(regionAltered(QRegion,Layer*)));

    connect(&mScene->lotManager(), SIGNAL(lotAdded(MapComposite*,Tiled::MapObject*)),
        SLOT(onLotAdded(MapComposite*,Tiled::MapObject*)));
    connect(&mScene->lotManager(), SIGNAL(lotRemoved(MapComposite*,Tiled::MapObject*)),
        SLOT(onLotRemoved(MapComposite*,Tiled::MapObject*)));
    connect(&mScene->lotManager(), SIGNAL(lotUpdated(MapComposite*,Tiled::MapObject*)),
        SLOT(onLotUpdated(MapComposite*,Tiled::MapObject*)));

    QMap<MapObject*,MapComposite*>::const_iterator it;
    const QMap<MapObject*,MapComposite*> &map = mScene->lotManager().objectToLot();
    for (it = map.constBegin(); it != map.constEnd(); it++) {
        MapComposite *lot = it.value();
        mLotBounds[lot] = lot->boundingRect(mRenderer);
    }

    recreateImage();
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
    painter->drawRect(mMapImageBounds);
}

void MiniMapItem::updateImage(const QRectF &dirtyRect)
{
    Q_ASSERT(mMapImage);

    // Duplicating most of MapImageManager::generateMapImage

    QRectF sceneRect = mScene->sceneRect();
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return;

    qreal scale = mMapImage->width() / qreal(mapSize.width());

    QPainter painter(mMapImage);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    painter.setTransform(QTransform::fromScale(scale, scale)
                         .translate(-sceneRect.left(), -sceneRect.top()));

    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(dirtyRect, Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    mMapComposite->saveVisibility();
    CompositeLayerGroup *prevLayerGroup = 0;
    QVector<int> drawnLevels;
    foreach (Layer *layer, mMapComposite->map()->layers()) {
        if (TileLayer *tileLayer = layer->asTileLayer()) {
            int level;
            if (MapComposite::levelForLayer(tileLayer, &level)) {
                if (drawnLevels.contains(level))
                    continue;
                drawnLevels += level;
                // FIXME: LayerGroups should be drawn with the same Z-order the scene uses.
                // They will usually be in the same order anyways.
                CompositeLayerGroup *layerGroup = mMapComposite->tileLayersForLevel(level);
                if (layerGroup != prevLayerGroup) {
                    prevLayerGroup = layerGroup;

                    foreach (TileLayer *tl, layerGroup->layers()) {
                        bool isVisible = true;
                        if (tl->name().contains(QLatin1String("NoRender")))
                            isVisible = false;
                        layerGroup->setLayerVisibility(tl, isVisible);
                    }
                    layerGroup->synch();

                    mRenderer->drawTileLayerGroup(&painter, layerGroup, dirtyRect);
                }
            } else {
                if (layer->name().contains(QLatin1String("NoRender")))
                    continue;
                mRenderer->drawTileLayer(&painter, tileLayer, dirtyRect);
            }
        }
    }
    mMapComposite->restoreVisibility();
    foreach (CompositeLayerGroup *layerGroup, mMapComposite->sortedLayerGroups())
        layerGroup->synch();
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
    if (mNeedsUpdate.isEmpty())
        mNeedsUpdate = dirtyRect;
    else
        mNeedsUpdate |= dirtyRect;

    if (!mMiniMapVisible)
        return;

    if (mUpdatePending)
        return;
    mUpdatePending = true;
    QMetaObject::invokeMethod(this, "updateNow",
                              Qt::QueuedConnection);
}

void MiniMapItem::recreateLater()
{
    mNeedsRecreate = true;

    if (!mMiniMapVisible)
        return;

    if (mUpdatePending)
        return;
    mUpdatePending = true;
    QMetaObject::invokeMethod(this, "updateNow",
                              Qt::QueuedConnection);
}

void MiniMapItem::sceneRectChanged(const QRectF &sceneRect)
{
    Q_UNUSED(sceneRect)
    recreateLater();
}

void MiniMapItem::layerAdded(int index)
{
    Q_UNUSED(index)
    recreateLater();
}

void MiniMapItem::layerRemoved(int index)
{
    Q_UNUSED(index)
    recreateLater();
}

void MiniMapItem::onLotAdded(MapComposite *lot, Tiled::MapObject *mapObject)
{
    Q_UNUSED(mapObject)
    QRectF bounds = lot->boundingRect(mRenderer);
    updateLater(mLotBounds[lot] | bounds);
    mLotBounds[lot] = bounds;
}

void MiniMapItem::onLotRemoved(MapComposite *lot, Tiled::MapObject *mapObject)
{
    Q_UNUSED(mapObject)
    updateLater(mLotBounds[lot]);
    mLotBounds.remove(lot);
}

void MiniMapItem::onLotUpdated(MapComposite *lot, Tiled::MapObject *mapObject)
{
    Q_UNUSED(mapObject)
    QRectF bounds = lot->boundingRect(mRenderer);
    updateLater(mLotBounds[lot] | bounds);
    mLotBounds[lot] = bounds;
}

void MiniMapItem::regionAltered(const QRegion &region, Layer *layer)
{
    QRectF bounds = mRenderer->boundingRect(region.boundingRect(),
                                            layer->level());
    QMargins margins = mMapComposite->map()->drawMargins();
    bounds.adjust(-margins.left(),
                  -margins.top(),
                  margins.right(),
                  margins.bottom());
    updateLater(bounds);
}

void MiniMapItem::updateNow()
{
    if (mNeedsRecreate) {
        recreateImage();
        update();
    } else if (!mNeedsUpdate.isEmpty()) {
        updateImage(mNeedsUpdate);
        update(mNeedsUpdate);
    }
    mNeedsRecreate = false;
    mNeedsUpdate = QRectF();
    mUpdatePending = false;
}

/////

MiniMap::MiniMap(MapView *parent)
    : QGraphicsView(parent)
    , mMapView(parent)
    , mButtons(new QFrame(this))
    , mWidth(256.0)
    , mViewportItem(0)
    , mExtraItem(0)
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

    QToolButton *button = new QToolButton(mButtons);
    button->setAutoRaise(true);
    button->setAutoRepeat(true);
    button->setIconSize(QSize(16, 16));
    button->setIcon(QIcon(QLatin1String(":/images/16x16/zoom-out.png")));
    button->setToolTip(tr("Make the MiniMap smaller"));
    connect(button, SIGNAL(clicked()), SLOT(smaller()));
    layout->addWidget(button);

    button = new QToolButton(mButtons);
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
    sceneRectChanged(mMapScene->sceneRect());
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
    Preferences::instance()->setMiniMapWidth(qMin(mWidth + 32, 512));
}

void MiniMap::smaller()
{
    Preferences::instance()->setMiniMapWidth(qMax(mWidth - 32, 256));
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
