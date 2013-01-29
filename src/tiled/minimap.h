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

#ifndef MINIMAP_H
#define MINIMAP_H

#include <QGraphicsItem>
#include <QGraphicsView>

class MapComposite;
class MapInfo;

namespace Tiled {
class Layer;
class Map;
class MapObject;
class MapRenderer;
class Tileset;

namespace Internal {
class MapScene;
class MapView;
class ZomboidScene;
}
}

class QToolButton;

/**
  * The MiniMap
  *
  * In the first implementation, the mini-map displayed the actual MapScene,
  * i.e., the map being edited. This is possible because a single
  * QGraphicsScene can be displayed by multiple QGraphicsViews. However, this
  * displayed everything, including the grid, level-highlight rectangle, map
  * objects, and was updated when layers were hidden/shown and during editing.
  *
  * In the second implementation, a new scene was created that only showed the
  * tile layers in the map being edited, and those layers were always fully
  * visible. Changes during editing were reflected right away in the mini-map.
  * It looks good, but displaying a full 300x300 map with 100 tile layers
  * scaled down is slow at times. I tried using a QGLWidget, but having 2
  * OpenGL widgets (1 for the map scene, 1 for the mini-map) was very slow.
  *
  * The third (and current) implementation uses a map image. The image is
  * updated when the map is edited and when lots are added/removed/moved.
  */

/**
  * This class maintains a copy of another map.
  */
class ShadowMap
{
public:
    ShadowMap(MapInfo *mapInfo);
    ~ShadowMap();

    void layerAdded(int index);
    void layerRemoved(int index);
    void layerRenamed(int index);
    void regionAltered(const QRegion &rgn, Tiled::Layer *layer);

    void lotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void mapAboutToChange(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);

    MapComposite *mMapComposite;
    Tiled::Map *mMaster; // Never accessed from the thread!
    QMap<quintptr,MapComposite*> mLots;
};

#include <QMutexLocker>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

class MapRenderThread : public QThread
{
    Q_OBJECT
public:
    MapRenderThread(MapComposite *mapComposite, QImage *image, const QRectF dirtyRect);
    ~MapRenderThread();

    void run();

    void update(const QRectF &rect);
    void recreateImage(const QImage *other);
    void abortDrawing();

    void layerAdded(int index);
    void layerRemoved(int index);
    void layerRenamed(int index);
    void regionAltered(const QRegion &rgn, Tiled::Layer *layer);

    void lotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void mapAboutToChange(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);

    void lockMapAndImage();

    void getImage(QImage &dest);

signals:
    void rendered(MapRenderThread *t);

private:
    QMutex mMapAndImageMutex; // lock on mImage and mShadowMap
    QMutex mSharedVarsMutex; // lock on most other vars
    QMutex mRenderSleepMutex;
    QWaitCondition mRenderSleepCondition;
    ShadowMap *mShadowMap;
    Tiled::MapRenderer *mRenderer;
    QImage mImage;
    QRectF mDirtyRect;
    bool mAbortDrawing; // flag for MapRenderer
    bool mRestart; // restart rendering ASAP
    bool mWaiting; // inside mWaitCondition.wait()
    bool mQuit; // exit from run() ASAP
};

/**
  * MiniMap item for drawing a map image.
  */
class MiniMapItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MiniMapItem(Tiled::Internal::ZomboidScene *scene, QGraphicsItem *parent = 0);
    ~MiniMapItem();

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateImage(const QRectF &dirtyRect = QRectF());
    void updateImageBounds();
    void recreateImage();

    void minimapVisibilityChanged(bool visible);

private:
    void updateLater(const QRectF &dirtyRect = QRectF());
    void recreateLater();

    typedef Tiled::Layer Layer; // hack for signals/slots
    typedef Tiled::Tileset *Tileset; // hack for signals/slots

private slots:
    void sceneRectChanged(const QRectF &sceneRect);

    void layerAdded(int index);
    void layerRemoved(int index);

    void lotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void lotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void regionAltered(const QRegion &region, Layer *layer);

    void mapAboutToChange(MapInfo *mapInfo);
    void mapFileChanged(MapInfo *mapInfo);

    void tilesetChanged(Tileset *ts);

    void updateNow();

    void rendered(MapRenderThread *t);

private:
    Tiled::Internal::ZomboidScene *mScene;
    Tiled::MapRenderer *mRenderer;
    QImage *mMapImage;
    QRectF mMapImageBounds;
    QRectF mLevelZeroBounds;
    MapComposite *mMapComposite;
    QMap<MapComposite*,QRectF> mLotBounds;
    bool mMiniMapVisible;
    bool mUpdatePending;
    QRectF mNeedsUpdate;
    bool mNeedsRecreate;
    MapRenderThread *mRenderThread;
};

class MiniMap : public QGraphicsView
{
    Q_OBJECT
public:
    MiniMap(Tiled::Internal::MapView *parent);

    void setMapScene(Tiled::Internal::MapScene *scene);
    void viewRectChanged();
    qreal scale();

    void setExtraItem(MiniMapItem *item);

public slots:
    void sceneRectChanged(const QRectF &sceneRect);
    void bigger();
    void smaller();
    void updateImage();
    void widthChanged(int width);

protected:
    bool event(QEvent *event);
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);

private:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    Tiled::Internal::MapView *mMapView;
    Tiled::Internal::MapScene *mMapScene;
    QFrame *mButtons;
    int mWidth;
    QGraphicsPolygonItem *mViewportItem;
    MiniMapItem *mExtraItem;
    QToolButton *mBiggerButton;
    QToolButton *mSmallerButton;
};

#endif // MINIMAP_H
