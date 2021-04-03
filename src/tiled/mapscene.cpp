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
#include "maplevel.h"
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
#include "abstractobjecttool.h"
#include "bmpselectionitem.h"
#include "mapcomposite.h"
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
    mMapDocument(nullptr),
    mSelectedTool(nullptr),
    mActiveTool(nullptr),
    mGridVisible(true),
    mUnderMouse(false),
    mCurrentModifiers(Qt::NoModifier),
    mDarkRectangle(new QGraphicsRectItem)
#ifdef ZOMBOID
    ,
    mGridItem(new ZGridItem)
#ifdef SEPARATE_BMP_SELECTION
    , mBmpSelectionItem(0)
#endif
#endif
{
#ifndef ZOMBOID
    setBackgroundBrush(Qt::darkGray);
#endif

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
    setBackgroundBrush(prefs->backgroundColor());
    connect(prefs, SIGNAL(backgroundColorChanged(QColor)),
            SLOT(bgColorChanged(QColor)));
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
        connect(mMapDocument, &MapDocument::mapChanged,
                this, &MapScene::mapChanged);
#ifdef ZOMBOID
        connect(mMapDocument, &MapDocument::regionChanged,
                this, &MapScene::regionChanged);
#else
        connect(mMapDocument, SIGNAL(regionChanged(QRegion)),
                this, SLOT(repaintRegion(QRegion)));
#endif
        connect(mMapDocument, &MapDocument::layerAdded,
                this, &MapScene::layerAdded);
#ifdef ZOMBOID
        connect(mMapDocument, &MapDocument::layerAboutToBeRemoved,
                this, &MapScene::layerAboutToBeRemoved);
        connect(mMapDocument, &MapDocument::layerRenamed,
                this, &MapScene::layerRenamed);
#endif
        connect(mMapDocument, &MapDocument::layerRemoved,
                this, &MapScene::layerRemoved);
        connect(mMapDocument, &MapDocument::layerChanged,
                this, &MapScene::layerChanged);
        connect(mMapDocument, &MapDocument::currentLayerIndexChanged,
                this, &MapScene::currentLayerIndexChanged);
        connect(mMapDocument, &MapDocument::objectsAdded,
                this, &MapScene::objectsAdded);
        connect(mMapDocument, &MapDocument::objectsRemoved,
                this, &MapScene::objectsRemoved);
        connect(mMapDocument, &MapDocument::objectsChanged,
                this, &MapScene::objectsChanged);
        connect(mMapDocument, &MapDocument::selectedObjectsChanged,
                this, &MapScene::updateSelectedObjectItems);
#ifdef ZOMBOID
        // The tooltip on lot objects contains the relative path to the lot.
        connect(mMapDocument, &MapDocument::fileNameChanged,
                this, &MapScene::syncAllObjectItems);
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
    for (const MapObjectItem *item : items) {
        selectedObjects.append(item->mapObject());
    }
    mMapDocument->setSelectedObjects(selectedObjects);
}

MapObjectItem *MapScene::itemForObject(MapObject *object) const
{
    if (object == nullptr) {
        return nullptr;
    }
    // FIXME: after removing a MapObject that is a Lot, onLotRemoved() is called which calls this.
    // In that case, object has already been deleted.
    for (const LevelData &levelData : mLevelData) {
        if (MapObjectItem *item = levelData.mObjectItems.value(object)) {
            return item;
        }
    }
//    const LevelData &levelData = mLevelData[object->objectGroup()->level()];
//    return levelData.mObjectItems.value(object);
    return nullptr;
}

void MapScene::setSelectedTool(AbstractTool *tool)
{
    mSelectedTool = tool;
}

void MapScene::refreshScene()
{
    for (int z = 0; z < mLevelData.size(); z++) {
        LevelData &levelData = mLevelData[z];
        levelData.mLayerItems.clear();
        levelData.mObjectItems.clear();
    }

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
    for (int z = 0; z < map->levelCount(); z++) {
        LevelData &levelData = mLevelData[z];
        MapLevel *mapLevel = map->levelAt(z);
        levelData.mLayerItems.resize(mapLevel->layerCount());

        int layerIndex = 0;
        for (Layer *layer : mapLevel->layers()) {
            QGraphicsItem *layerItem = createLayerItem(layer);
            layerItem->setZValue(layerIndex);
            addItem(layerItem);
            levelData.mLayerItems[layerIndex] = layerItem;
            ++layerIndex;
        }
    }

    TileSelectionItem *selectionItem = new TileSelectionItem(mMapDocument);
    selectionItem->setZValue(10000 - 1);
    addItem(selectionItem);

#ifdef ZOMBOID
#ifdef SEPARATE_BMP_SELECTION
    mBmpSelectionItem = new BmpSelectionItem(mMapDocument);
    mBmpSelectionItem->setZValue(10000 - 1);
    addItem(mBmpSelectionItem);
#else
    mTileSelectionItem = selectionItem;
#endif
#endif

    updateCurrentLayerHighlight();
}

QGraphicsItem *MapScene::createLayerItem(Layer *layer)
{
    QGraphicsItem *layerItem = nullptr;

    LevelData &levelData = mLevelData[layer->level()];

    if (TileLayer *tl = layer->asTileLayer()) {
        layerItem = new TileLayerItem(tl, mMapDocument->renderer());
    } else if (ObjectGroup *og = layer->asObjectGroup()) {
        ObjectGroupItem *ogItem = new ObjectGroupItem(og);
        for (MapObject *object : og->objects()) {
            MapObjectItem *item = new MapObjectItem(object, mMapDocument,
                                                    ogItem);
            levelData.mObjectItems.insert(object, item);
        }
        layerItem = ogItem;
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
        for (int z = 0; z < mLevelData.size(); z++) {
            LevelData &levelData = mLevelData[z];
            const QList<Layer*> layers = mMapDocument->map()->levelAt(z)->layers();
            for (int i = 0; i < levelData.mLayerItems.size(); ++i) {
                const Layer *layer = layers[i];
                levelData.mLayerItems.at(i)->setOpacity(layer->opacity());
            }
        }
        return;
    }

    // Darken layers below the current layer
    mDarkRectangle->setZValue(currentLayerIndex - 0.5);
    mDarkRectangle->setVisible(true);

    // Set layers above the current layer to half opacity
    for (int z = 0; z < mLevelData.size(); z++) {
        LevelData &levelData = mLevelData[z];
        const QList<Layer*> layers = mMapDocument->map()->levelAt(z)->layers();
        for (int i = 1; i < levelData.mLayerItems.size(); ++i) {
            const Layer *layer = layers[i];
            const qreal multiplier = (currentLayerIndex < i) ? opacityFactor : 1;
            levelData.mLayerItems.at(i)->setOpacity(layer->opacity() * multiplier);
        }
    }
}

#ifdef ZOMBOID
void MapScene::regionChanged(const QRegion &region, Layer *layer)
{
    const MapRenderer *renderer = mMapDocument->renderer();
    const QMargins margins = mMapDocument->map()->drawMargins();

    for (const QRect &r : region) {
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

#ifdef ZOMBOID
    // When an item accepts hover events, it stops the active tool getting
    // mouse move events.  For example, the Stamp brush won't update its
    // position when the mouse is hovering over a MapObjectItem.
    bool hover = dynamic_cast<AbstractObjectTool*>(mActiveTool) != 0;
    foreach (QGraphicsItem *item, items()) {
        if (MapObjectItem *mo = dynamic_cast<MapObjectItem*>(item)) {
            mo->setAcceptHoverEvents(hover);
            mo->labelItem()->setAcceptHoverEvents(hover);
        }
    }
#endif
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

    for (int z = 0; z < mLevelData.size(); z++) {
        LevelData &levelData = mLevelData[z];
        for (QGraphicsItem *item : qAsConst(levelData.mLayerItems)) {
            if (TileLayerItem *tli = dynamic_cast<TileLayerItem*>(item)) {
                tli->syncWithTileLayer();
            }
        }
#ifdef ZOMBOID
        // BUG: create object layer, add items, resize map much larger, try to select the objects
        for (MapObjectItem *item : qAsConst(levelData.mObjectItems)) {
            item->syncWithMapObject();
        }
#endif
    }
}

void MapScene::tilesetChanged(Tileset *tileset)
{
    if (!mMapDocument)
        return;

#ifdef ZOMBOID
    if (mMapDocument->mapComposite()->isTilesetUsed(tileset))
        update();
#else
    if (mMapDocument->map()->tilesets().contains(tileset))
        update();
#endif
}

void MapScene::layerAdded(int z, int index)
{
    MapLevel *mapLevel = mMapDocument->map()->levelAt(z);
    Layer *layer = mapLevel->layerAt(index);
    QGraphicsItem *layerItem = createLayerItem(layer);
    addItem(layerItem);
    mLevelData[z].mLayerItems.insert(index, layerItem);

#ifndef ZOMBOID
    int z = 0;
    foreach (QGraphicsItem *item, mLayerItems)
        item->setZValue(z++);
#endif
}

#ifdef ZOMBOID
void MapScene::layerAboutToBeRemoved(int z, int index)
{
    MapLevel *mapLevel = mMapDocument->map()->levelAt(z);
    Layer *layer = mapLevel->layerAt(index);
    if (ObjectGroup *og = layer->asObjectGroup()) {
        for (MapObject *o : og->objects()) {
            mLevelData[z].mObjectItems.remove(o);
        }
    }
}
#endif

void MapScene::layerRemoved(int z, int index)
{
    delete mLevelData[z].mLayerItems.at(index);
    mLevelData[z].mLayerItems.remove(index);
}

/**
 * A layer has changed. This can mean that the layer visibility or opacity has
 * changed.
 */
void MapScene::layerChanged(int z, int index)
{
    MapLevel *mapLevel = mMapDocument->map()->levelAt(z);
    const Layer *layer = mapLevel->layerAt(index);
    QGraphicsItem *layerItem = mLevelData[z].mLayerItems.at(index);

    layerItem->setVisible(layer->isVisible());

    qreal multiplier = 1;
#if !defined(ZOMBOID)
    if (mHighlightCurrentLayer && mMapDocument->currentLayerIndex() < index)
        multiplier = opacityFactor;
#endif

    layerItem->setOpacity(layer->opacity() * multiplier);
}

#ifdef ZOMBOID
void MapScene::layerRenamed(int z, int index)
{
    Q_UNUSED(z)
    Q_UNUSED(index)
}
#endif

/**
 * Inserts map object items for the given objects.
 */
void MapScene::objectsAdded(const QList<MapObject*> &objects)
{
    for (MapObject *object : objects) {
        ObjectGroup *og = object->objectGroup();
        ObjectGroupItem *ogItem = nullptr;

        // Find the object group item for the map object's object group
        LevelData &levelData = mLevelData[og->level()];
        for (QGraphicsItem *item : qAsConst(levelData.mLayerItems)) {
            if (ObjectGroupItem *ogi = dynamic_cast<ObjectGroupItem*>(item)) {
                if (ogi->objectGroup() == og) {
                    ogItem = ogi;
                    break;
                }
            }
        }

        Q_ASSERT(ogItem);
        MapObjectItem *item = new MapObjectItem(object, mMapDocument, ogItem);
        levelData.mObjectItems.insert(object, item);

#ifdef ZOMBOID
        // When an item accepts hover events, it stops the active tool getting
        // mouse move events.  For example, the Stamp brush won't update its
        // position when the mouse is hovering over a MapObjectItem.
        bool hover = dynamic_cast<AbstractObjectTool*>(mActiveTool) != nullptr;
        item->setAcceptHoverEvents(hover);
        item->labelItem()->setAcceptHoverEvents(hover);
#endif
    }
}

/**
 * Removes the map object items related to the given objects.
 */
void MapScene::objectsRemoved(ObjectGroup *objectGroup, const QList<MapObject*> &objects)
{
    for (MapObject *o : objects) {
        LevelData &levelData = mLevelData[objectGroup->level()];
        ObjectItems::iterator i = levelData.mObjectItems.find(o);
        Q_ASSERT(i != levelData.mObjectItems.end());

        mSelectedObjectItems.remove(i.value());
        delete i.value();
        levelData.mObjectItems.erase(i);
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
    const QSet<MapObjectItem*> items1 = mSelectedObjectItems - items;
    for (MapObjectItem *item : items1)
        item->setEditable(false);
    const QSet<MapObjectItem*> items2 = items - mSelectedObjectItems;
    for (MapObjectItem *item : items2)
        item->setEditable(true);

    mSelectedObjectItems = items;
    emit selectedObjectItemsChanged();
}

void MapScene::syncAllObjectItems()
{
    for (LevelData &levelData : mLevelData) {
        for (MapObjectItem *item : qAsConst(levelData.mObjectItems)) {
            item->syncWithMapObject();
        }
    }
}

#ifdef ZOMBOID
void MapScene::bgColorChanged(const QColor &color)
{
    setBackgroundBrush(color);
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
