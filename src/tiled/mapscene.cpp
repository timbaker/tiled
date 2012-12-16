/*
 * mapscene.cpp
 * Copyright 2008-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
 *
 * This file is part of Tiled.
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

#include "mapscene.h"

#include "abstracttool.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "mapobjectitem.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "objectgroupitem.h"
#include "preferences.h"
#include "tilelayer.h"
#include "tilelayeritem.h"
#include "tileselectionitem.h"
#include "imagelayer.h"
#include "imagelayeritem.h"
#include "toolmanager.h"
#include "tilesetmanager.h"
#ifdef ZOMBOID
#include "mapcomposite.h"
#include "pathitem.h"
#include "pathlayer.h"
#include "pathlayeritem.h"
#include "zgriditem.h"
#endif

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>

#include <cmath>

using namespace Tiled;
using namespace Tiled::Internal;

static const qreal darkeningFactor = 0.6;
static const qreal opacityFactor = 0.4;

MapScene::MapScene(QObject *parent):
    QGraphicsScene(parent),
    mMapDocument(0),
    mSelectedTool(0),
    mActiveTool(0),
    mGridVisible(true),
    mUnderMouse(false),
    mCurrentModifiers(Qt::NoModifier),
    mDarkRectangle(new QGraphicsRectItem)
#ifdef ZOMBOID
    ,
    mGridItem(new ZGridItem)
#endif
{
    setBackgroundBrush(Qt::darkGray);

    TilesetManager *tilesetManager = TilesetManager::instance();
    connect(tilesetManager, SIGNAL(tilesetChanged(Tileset*)),
            this, SLOT(tilesetChanged(Tileset*)));

    Preferences *prefs = Preferences::instance();
    connect(prefs, SIGNAL(objectTypesChanged()), SLOT(syncAllObjectItems()));
    connect(prefs, SIGNAL(highlightCurrentLayerChanged(bool)),
            SLOT(setHighlightCurrentLayer(bool)));
    connect(prefs, SIGNAL(gridColorChanged(QColor)), SLOT(update()));

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(darkeningFactor);
    addItem(mDarkRectangle);

#ifdef ZOMBOID
    addItem(mGridItem);
#endif

    mHighlightCurrentLayer = prefs->highlightCurrentLayer();

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

MapScene::~MapScene()
{
    qApp->removeEventFilter(this);
}

void MapScene::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
#ifdef ZOMBOID
    mGridItem->setMapDocument(mapDocument);
#endif

    refreshScene();

    if (mMapDocument) {
        connect(mMapDocument, SIGNAL(mapChanged()),
                this, SLOT(mapChanged()));
#ifdef ZOMBOID
        connect(mMapDocument, SIGNAL(regionChanged(QRegion,Layer*)),
                this, SLOT(regionChanged(QRegion,Layer*)));
#else
        connect(mMapDocument, SIGNAL(regionChanged(QRegion)),
                this, SLOT(repaintRegion(QRegion)));
#endif
        connect(mMapDocument, SIGNAL(layerAdded(int)),
                this, SLOT(layerAdded(int)));
#ifdef ZOMBOID
        connect(mMapDocument, SIGNAL(layerAboutToBeRemoved(int)),
                this, SLOT(layerAboutToBeRemoved(int)));
        connect(mMapDocument, SIGNAL(layerRenamed(int)),
                this, SLOT(layerRenamed(int)));
#endif
        connect(mMapDocument, SIGNAL(layerRemoved(int)),
                this, SLOT(layerRemoved(int)));
        connect(mMapDocument, SIGNAL(layerChanged(int)),
                this, SLOT(layerChanged(int)));
        connect(mMapDocument, SIGNAL(currentLayerIndexChanged(int)),
                this, SLOT(currentLayerIndexChanged()));
        connect(mMapDocument, SIGNAL(objectsAdded(QList<MapObject*>)),
                this, SLOT(objectsAdded(QList<MapObject*>)));
        connect(mMapDocument, SIGNAL(objectsRemoved(QList<MapObject*>)),
                this, SLOT(objectsRemoved(QList<MapObject*>)));
        connect(mMapDocument, SIGNAL(objectsChanged(QList<MapObject*>)),
                this, SLOT(objectsChanged(QList<MapObject*>)));
        connect(mMapDocument, SIGNAL(selectedObjectsChanged()),
                this, SLOT(updateSelectedObjectItems()));
#ifdef ZOMBOID
        connect(mMapDocument, SIGNAL(pathsAdded(QList<Path*>)),
                SLOT(pathsAdded(QList<Path*>)));
        connect(mMapDocument, SIGNAL(pathsRemoved(QList<Path*>)),
                SLOT(pathsRemoved(QList<Path*>)));
        connect(mMapDocument, SIGNAL(pathsChanged(QList<Path*>)),
                SLOT(pathsChanged(QList<Path*>)));
        connect(mMapDocument, SIGNAL(selectedPathsChanged()),
                SLOT(updateSelectedPathItems()));
#endif
    }
}

void MapScene::setSelectedObjectItems(const QSet<MapObjectItem *> &items)
{
    // Inform the map document about the newly selected objects
    QList<MapObject*> selectedObjects;
#if QT_VERSION >= 0x040700
    selectedObjects.reserve(items.size());
#endif
    foreach (const MapObjectItem *item, items)
        selectedObjects.append(item->mapObject());
    mMapDocument->setSelectedObjects(selectedObjects);
}

#ifdef ZOMBOID
void MapScene::setSelectedPathItems(const QSet<PathItem *> &items)
{
    // Inform the map document about the newly selected paths
    QList<Path*> selectedPaths;
    selectedPaths.reserve(items.size());

    foreach (const PathItem *item, items)
        selectedPaths.append(item->path());
    mMapDocument->setSelectedPaths(selectedPaths);
}
#endif

void MapScene::setSelectedTool(AbstractTool *tool)
{
    mSelectedTool = tool;
}

void MapScene::refreshScene()
{
    mLayerItems.clear();
    mObjectItems.clear();
#ifdef ZOMBOID
    mPathItems.clear();
#endif

#ifdef ZOMBOID
    removeItem(mGridItem);
#endif
    removeItem(mDarkRectangle);
    clear();
    addItem(mDarkRectangle);
#ifdef ZOMBOID
    addItem(mGridItem);
    mGridItem->setZValue(20000);
#endif

    if (!mMapDocument) {
        setSceneRect(QRectF());
        return;
    }

#ifdef ZOMBOID
    // This stops tall tiles being cut off near the 0,0 tile at the top of the window
    // by including the map's drawMargins.
    // It also includes lot bounds.
    QRectF sceneRect = mMapDocument->mapComposite()->boundingRect(mMapDocument->renderer());
    setSceneRect(sceneRect);
    mDarkRectangle->setRect(sceneRect);
#else
    const QSize mapSize = mMapDocument->renderer()->mapSize();
    setSceneRect(0, 0, mapSize.width(), mapSize.height());
    mDarkRectangle->setRect(0, 0, mapSize.width(), mapSize.height());
#endif

    const Map *map = mMapDocument->map();
    mLayerItems.resize(map->layerCount());

    int layerIndex = 0;
    foreach (Layer *layer, map->layers()) {
        QGraphicsItem *layerItem = createLayerItem(layer);
        layerItem->setZValue(layerIndex);
        addItem(layerItem);
        mLayerItems[layerIndex] = layerItem;
        ++layerIndex;
    }

    TileSelectionItem *selectionItem = new TileSelectionItem(mMapDocument);
    selectionItem->setZValue(10000 - 1);
    addItem(selectionItem);

    updateCurrentLayerHighlight();
}

QGraphicsItem *MapScene::createLayerItem(Layer *layer)
{
    QGraphicsItem *layerItem = 0;

    if (TileLayer *tl = layer->asTileLayer()) {
        layerItem = new TileLayerItem(tl, mMapDocument->renderer());
    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        ObjectGroupItem *ogItem = new ObjectGroupItem(og);
        foreach (MapObject *object, og->objects()) {
            MapObjectItem *item = new MapObjectItem(object, mMapDocument,
                                                    ogItem);
            mObjectItems.insert(object, item);
        }
        layerItem = ogItem;
#ifdef ZOMBOID
    } else if (PathLayer *pl = layer->asPathLayer()) {
        PathLayerItem *plItem = new PathLayerItem(pl);
        foreach (Path *path, pl->paths()) {
            PathItem *item = new PathItem(path, mMapDocument, plItem);
            mPathItems.insert(path, item);
        }
        layerItem = plItem;
#endif
    } else if (ImageLayer *il = layer->asImageLayer()) {
        layerItem = new ImageLayerItem(il, mMapDocument->renderer());
    }

    Q_ASSERT(layerItem);

    layerItem->setVisible(layer->isVisible());
    return layerItem;
}

void MapScene::updateCurrentLayerHighlight()
{
    if (!mMapDocument)
        return;

    const int currentLayerIndex = mMapDocument->currentLayerIndex();

    if (!mHighlightCurrentLayer || currentLayerIndex == -1) {
        mDarkRectangle->setVisible(false);

        // Restore opacity for all layers
        for (int i = 0; i < mLayerItems.size(); ++i) {
            const Layer *layer = mMapDocument->map()->layerAt(i);
            mLayerItems.at(i)->setOpacity(layer->opacity());
        }

        return;
    }

    // Darken layers below the current layer
    mDarkRectangle->setZValue(currentLayerIndex - 0.5);
    mDarkRectangle->setVisible(true);

    // Set layers above the current layer to half opacity
    for (int i = 1; i < mLayerItems.size(); ++i) {
        const Layer *layer = mMapDocument->map()->layerAt(i);
        const qreal multiplier = (currentLayerIndex < i) ? opacityFactor : 1;
        mLayerItems.at(i)->setOpacity(layer->opacity() * multiplier);
    }
}

#ifdef ZOMBOID
void MapScene::regionChanged(const QRegion &region, Layer *layer)
{
    const MapRenderer *renderer = mMapDocument->renderer();
    const QMargins margins = mMapDocument->map()->drawMargins();

    foreach (const QRect &r, region.rects()) {
        update(renderer->boundingRect(r, layer->level()).adjusted(-margins.left(),
                                                                  -margins.top(),
                                                                  margins.right(),
                                                                  margins.bottom()));
    }
}
#else
void MapScene::repaintRegion(const QRegion &region)
{
    const MapRenderer *renderer = mMapDocument->renderer();
    const QMargins margins = mMapDocument->map()->drawMargins();

    foreach (const QRect &r, region.rects()) {
        update(renderer->boundingRect(r).adjusted(-margins.left(),
                                                  -margins.top(),
                                                  margins.right(),
                                                  margins.bottom()));
    }
}
#endif

void MapScene::enableSelectedTool()
{
    if (!mSelectedTool || !mMapDocument)
        return;

    mActiveTool = mSelectedTool;
    mActiveTool->activate(this);

    mCurrentModifiers = QApplication::keyboardModifiers();
    if (mCurrentModifiers != Qt::NoModifier)
        mActiveTool->modifiersChanged(mCurrentModifiers);

    if (mUnderMouse) {
        mActiveTool->mouseEntered();
        mActiveTool->mouseMoved(mLastMousePos, Qt::KeyboardModifiers());
    }
}

void MapScene::disableSelectedTool()
{
    if (!mActiveTool)
        return;

    if (mUnderMouse)
        mActiveTool->mouseLeft();
    mActiveTool->deactivate(this);
    mActiveTool = 0;
}

void MapScene::currentLayerIndexChanged()
{
    updateCurrentLayerHighlight();
#ifdef ZOMBOID
    // LevelIsometric orientation may move the grid
    mGridItem->currentLayerIndexChanged();
#endif
}

/**
 * Adapts the scene rect and layers to the new map size.
 */
void MapScene::mapChanged()
{
#ifdef ZOMBOID
    // This stops tall tiles being cut off near the 0,0 tile at the top of the window
    // by including the map's drawMargins.
    // It also includes lot bounds.
    QRectF sceneRect = mMapDocument->mapComposite()->boundingRect(mMapDocument->renderer());
    setSceneRect(sceneRect);
    mDarkRectangle->setRect(sceneRect);
    mGridItem->currentLayerIndexChanged(); // index didn't change, just updating the bounds
#else
    const QSize mapSize = mMapDocument->renderer()->mapSize();
    setSceneRect(0, 0, mapSize.width(), mapSize.height());
    mDarkRectangle->setRect(0, 0, mapSize.width(), mapSize.height());
#endif

    foreach (QGraphicsItem *item, mLayerItems) {
        if (TileLayerItem *tli = dynamic_cast<TileLayerItem*>(item))
            tli->syncWithTileLayer();
    }
#ifdef ZOMBOID
    // BUG: create object layer, add items, resize map much larger, try to select the objects
    foreach (MapObjectItem *item, mObjectItems)
        item->syncWithMapObject();
#endif
}

void MapScene::tilesetChanged(Tileset *tileset)
{
    if (!mMapDocument)
        return;

    if (mMapDocument->map()->tilesets().contains(tileset))
        update();
}

void MapScene::layerAdded(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = createLayerItem(layer);
    addItem(layerItem);
    mLayerItems.insert(index, layerItem);

#ifndef ZOMBOID
    int z = 0;
    foreach (QGraphicsItem *item, mLayerItems)
        item->setZValue(z++);
#endif
}

#ifdef ZOMBOID
void MapScene::layerAboutToBeRemoved(int index)
{
    Layer *layer = mMapDocument->map()->layerAt(index);
    if (ObjectGroup *og = layer->asObjectGroup()) {
        foreach (MapObject *o, og->objects()) {
            mObjectItems.remove(o);
        }
    }
    if (PathLayer *pl = layer->asPathLayer()) {
        foreach (Path *p, pl->paths())
            mPathItems.remove(p);
    }
}
#endif

void MapScene::layerRemoved(int index)
{
    delete mLayerItems.at(index);
    mLayerItems.remove(index);
}

/**
 * A layer has changed. This can mean that the layer visibility or opacity has
 * changed.
 */
void MapScene::layerChanged(int index)
{
    const Layer *layer = mMapDocument->map()->layerAt(index);
    QGraphicsItem *layerItem = mLayerItems.at(index);

    layerItem->setVisible(layer->isVisible());

    qreal multiplier = 1;
#if !defined(ZOMBOID)
    if (mHighlightCurrentLayer && mMapDocument->currentLayerIndex() < index)
        multiplier = opacityFactor;
#endif

    layerItem->setOpacity(layer->opacity() * multiplier);
}

#ifdef ZOMBOID
void MapScene::layerRenamed(int index)
{
    Q_UNUSED(index)
}
#endif

/**
 * Inserts map object items for the given objects.
 */
void MapScene::objectsAdded(const QList<MapObject*> &objects)
{
    foreach (MapObject *object, objects) {
        ObjectGroup *og = object->objectGroup();
        ObjectGroupItem *ogItem = 0;

        // Find the object group item for the map object's object group
        foreach (QGraphicsItem *item, mLayerItems) {
            if (ObjectGroupItem *ogi = dynamic_cast<ObjectGroupItem*>(item)) {
                if (ogi->objectGroup() == og) {
                    ogItem = ogi;
                    break;
                }
            }
        }

        Q_ASSERT(ogItem);
        MapObjectItem *item = new MapObjectItem(object, mMapDocument, ogItem);
        mObjectItems.insert(object, item);
    }
}

/**
 * Removes the map object items related to the given objects.
 */
void MapScene::objectsRemoved(const QList<MapObject*> &objects)
{
    foreach (MapObject *o, objects) {
        ObjectItems::iterator i = mObjectItems.find(o);
        Q_ASSERT(i != mObjectItems.end());

        mSelectedObjectItems.remove(i.value());
        delete i.value();
        mObjectItems.erase(i);
    }
}

/**
 * Updates the map object items related to the given objects.
 */
void MapScene::objectsChanged(const QList<MapObject*> &objects)
{
    foreach (MapObject *object, objects) {
        MapObjectItem *item = itemForObject(object);
        Q_ASSERT(item);

        item->syncWithMapObject();
    }
}

void MapScene::updateSelectedObjectItems()
{
    const QList<MapObject *> &objects = mMapDocument->selectedObjects();

    QSet<MapObjectItem*> items;
    foreach (MapObject *object, objects) {
        MapObjectItem *item = itemForObject(object);
        Q_ASSERT(item);

        items.insert(item);
    }

    // Update the editable state of the items
    foreach (MapObjectItem *item, mSelectedObjectItems - items)
        item->setEditable(false);
    foreach (MapObjectItem *item, items - mSelectedObjectItems)
        item->setEditable(true);

    mSelectedObjectItems = items;
    emit selectedObjectItemsChanged();
}

void MapScene::syncAllObjectItems()
{
    foreach (MapObjectItem *item, mObjectItems)
        item->syncWithMapObject();
}

#ifdef ZOMBOID
void MapScene::pathsAdded(const QList<Path *> &paths)
{
    foreach (Path *path, paths) {
        PathLayer *pathLayer = path->pathLayer();
        PathLayerItem *pathLayerItem = 0;

        // Find the object group item for the map object's object group
        foreach (QGraphicsItem *item, mLayerItems) {
            if (PathLayerItem *pli = dynamic_cast<PathLayerItem*>(item)) {
                if (pli->pathLayer() == pathLayer) {
                    pathLayerItem = pli;
                    break;
                }
            }
        }

        Q_ASSERT(pathLayerItem);
        PathItem *item = new PathItem(path, mMapDocument, pathLayerItem);
        mPathItems.insert(path, item);
    }
}

void MapScene::pathsRemoved(const QList<Path *> &paths)
{
    foreach (Path *path, paths) {
        PathItems::iterator i = mPathItems.find(path);
        Q_ASSERT(i != mPathItems.end());

        mSelectedPathItems.remove(i.value());
        delete i.value();
        mPathItems.erase(i);
    }
}

void MapScene::pathsChanged(const QList<Path *> &paths)
{
    foreach (Path *path, paths) {
        PathItem *pathItem = itemForPath(path);
        Q_ASSERT(pathItem);
        pathItem->syncWithPath();
    }
}

void MapScene::updateSelectedPathItems()
{
    const QList<Path*> &paths = mMapDocument->selectedPaths();

    QSet<PathItem*> items;
    foreach (Path *path, paths) {
        PathItem *item = itemForPath(path);
        Q_ASSERT(item);

        items.insert(item);
    }

    // Update the editable state of the items
    foreach (PathItem *item, mSelectedPathItems - items)
        item->setEditable(false);
    foreach (PathItem *item, items - mSelectedPathItems)
        item->setEditable(true);

    mSelectedPathItems = items;
    emit selectedPathItemsChanged();
}
#endif

void MapScene::setGridVisible(bool visible)
{
    if (mGridVisible == visible)
        return;

#ifdef ZOMBOID
    mGridVisible = visible;
    mGridItem->setVisible(mGridVisible);
#else
    mGridVisible = visible;
    update();
#endif
}

void MapScene::setHighlightCurrentLayer(bool highlightCurrentLayer)
{
    if (mHighlightCurrentLayer == highlightCurrentLayer)
        return;

    mHighlightCurrentLayer = highlightCurrentLayer;
    updateCurrentLayerHighlight();
}

void MapScene::drawForeground(QPainter *painter, const QRectF &rect)
{
#ifdef ZOMBOID
    // There is a GridItem that draws the grid for us.
    Q_UNUSED(painter)
    Q_UNUSED(rect)
#else
    if (!mMapDocument || !mGridVisible)
        return;

    Preferences *prefs = Preferences::instance();
    mMapDocument->renderer()->drawGrid(painter, rect, prefs->gridColor());
#endif
}

bool MapScene::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter:
        mUnderMouse = true;
        if (mActiveTool)
            mActiveTool->mouseEntered();
        break;
    case QEvent::Leave:
        mUnderMouse = false;
        if (mActiveTool)
            mActiveTool->mouseLeft();
        break;
    default:
        break;
    }

    return QGraphicsScene::event(event);
}

void MapScene::mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    mLastMousePos = mouseEvent->scenePos();

    if (!mMapDocument)
        return;

    QGraphicsScene::mouseMoveEvent(mouseEvent);
    if (mouseEvent->isAccepted())
        return;

    if (mActiveTool) {
        mActiveTool->mouseMoved(mouseEvent->scenePos(),
                                mouseEvent->modifiers());
        mouseEvent->accept();
    }
}

void MapScene::mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mousePressEvent(mouseEvent);
    if (mouseEvent->isAccepted())
        return;

    if (mActiveTool) {
        mouseEvent->accept();
        mActiveTool->mousePressed(mouseEvent);
    }
}

void MapScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent)
{
    QGraphicsScene::mouseReleaseEvent(mouseEvent);
    if (mouseEvent->isAccepted())
        return;

    if (mActiveTool) {
        mouseEvent->accept();
        mActiveTool->mouseReleased(mouseEvent);
    }
}

/**
 * Override to ignore drag enter events.
 */
void MapScene::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    event->ignore();
}

bool MapScene::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            Qt::KeyboardModifiers newModifiers = keyEvent->modifiers();

            if (mActiveTool && newModifiers != mCurrentModifiers) {
                mActiveTool->modifiersChanged(newModifiers);
                mCurrentModifiers = newModifiers;
            }
        }
        break;
    default:
        break;
    }

    return false;
}
