/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "buildingisoview.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingmap.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"
#include "buildingtools.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "zlevelrenderer.h"

#include <qmath.h>
#include <QApplication>
#include <QDebug>
#include <QGLFormat>
#include <QOpenGLWidget>
#include <QKeyEvent>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////

CompositeLayerGroupItem::CompositeLayerGroupItem(CompositeLayerGroup *layerGroup,
                                                 MapRenderer *renderer,
                                                 QGraphicsItem *parent)
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
    if (mLayerGroup->needsSynch() /*mBoundingRect != mLayerGroup->boundingRect(mRenderer)*/)
        return;

    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#if 1 && !defined(QT_NO_DEBUG)
    QPen pen(Qt::white);
    pen.setCosmetic(true);
    p->setPen(pen);
    p->drawRect(mBoundingRect);
#endif
}

void CompositeLayerGroupItem::synchWithTileLayers()
{
//    if (layerGroup()->needsSynch())
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

/////

TileModeGridItem::TileModeGridItem(BuildingDocument *doc, MapRenderer *renderer) :
    QGraphicsItem(),
    mDocument(doc),
    mRenderer(renderer),
    mEditingTiles(false)
{
    setVisible(BuildingPreferences::instance()->showGrid());
    connect(BuildingPreferences::instance(), SIGNAL(gridColorChanged(QColor)),
            SLOT(gridColorChanged()));
    connect(BuildingPreferences::instance(), SIGNAL(showGridChanged(bool)),
            SLOT(showGridChanged(bool)));

    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    synchWithBuilding();
}

void TileModeGridItem::synchWithBuilding()
{
    mTileBounds = mDocument->building()->bounds();
    if (mEditingTiles)
        mTileBounds.adjust(0, 0, 1, 1);

    QRectF bounds = mRenderer->boundingRect(mTileBounds, mDocument->currentLevel());
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF TileModeGridItem::boundingRect() const
{
    return mBoundingRect;
}

void TileModeGridItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    mRenderer->drawGrid(p, option->exposedRect, BuildingPreferences::instance()->gridColor(),
                        mDocument->currentLevel(), mTileBounds);
}

void TileModeGridItem::setEditingTiles(bool editing)
{
    if (mEditingTiles != editing) {
        mEditingTiles = editing;
        synchWithBuilding();
    }
}

void TileModeGridItem::showGridChanged(bool show)
{
    setVisible(show);
}

/////

TileModeSelectionItem::TileModeSelectionItem(BuildingIsoScene *scene) :
    mScene(scene)
{
    setZValue(1000);

    connect(document(), SIGNAL(tileSelectionChanged(QRegion)),
            SLOT(tileSelectionChanged(QRegion)));
    connect(document(), SIGNAL(currentFloorChanged()),
            SLOT(currentLevelChanged()));

    updateBoundingRect();
}

QRectF TileModeSelectionItem::boundingRect() const
{
    return mBoundingRect;
}

void TileModeSelectionItem::paint(QPainter *p,
                                  const QStyleOptionGraphicsItem *option,
                                  QWidget *)
{
    const QRegion &selection = document()->tileSelection();
    QColor highlight = QApplication::palette().highlight().color();
    highlight.setAlpha(128);

    MapRenderer *renderer = mScene->mapRenderer();
    renderer->drawTileSelection(p, selection, highlight, option->exposedRect,
                                mScene->currentLevel());
}

BuildingDocument *TileModeSelectionItem::document() const
{
    return mScene->document();
}

void TileModeSelectionItem::tileSelectionChanged(const QRegion &oldSelection)
{
    prepareGeometryChange();
    updateBoundingRect();

    // Make sure changes within the bounding rect are updated
    QRegion newSelection = document()->tileSelection();
    const QRect changedArea = newSelection.xored(oldSelection).boundingRect();
    update(mScene->mapRenderer()->boundingRect(changedArea, document()->currentLevel()));
}

void TileModeSelectionItem::currentLevelChanged()
{
    prepareGeometryChange();
    updateBoundingRect();
}

void TileModeSelectionItem::updateBoundingRect()
{
    const QRect r = document()->tileSelection().boundingRect();
    mBoundingRect = mScene->mapRenderer()->boundingRect(r, document()->currentLevel());
}

/////

BuildingIsoScene::BuildingIsoScene(QObject *parent) :
    BuildingBaseScene(parent),
    mBuildingMap(nullptr),
    mGridItem(nullptr),
    mTileSelectionItem(nullptr),
    mDarkRectangle(new QGraphicsRectItem),
    mCurrentTool(nullptr),
    mLayerGroupWithToolTiles(nullptr),
    mToolTiles(QString(), 0, 0, 1, 1),
    mNonEmptyLayerGroupItem(nullptr),
    mShowBuildingTiles(true),
    mShowUserTiles(true),
    mCurrentLevel(0),
    mHighlightRoomLock(false)
{
    ZVALUE_CURSOR = 1000;
    ZVALUE_GRID = 1001;

    mRenderer = new IsoBuildingRenderer();

    setBackgroundBrush(Qt::darkGray);

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(0.6);
    mDarkRectangle->setVisible(false);
    addItem(mDarkRectangle);

    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(BuildingTilesMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(prefs(), SIGNAL(highlightFloorChanged(bool)),
            SLOT(highlightFloorChanged(bool)));
    connect(prefs(), SIGNAL(highlightRoomChanged(bool)),
            SLOT(highlightRoomChanged(bool)));
    connect(prefs(), SIGNAL(showLowerFloorsChanged(bool)),
            SLOT(showLowerFloorsChanged(bool)));

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));
}

BuildingIsoScene::~BuildingIsoScene()
{
    delete mBuildingMap;
}

MapRenderer *BuildingIsoScene::mapRenderer() const
{
    return mBuildingMap->mapRenderer();
}

void BuildingIsoScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void BuildingIsoScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void BuildingIsoScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void BuildingIsoScene::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    // Delete before clearing mDocument.
    delete mTileSelectionItem;
    mTileSelectionItem = nullptr;

    mDocument = doc;

    qDeleteAll(mFloorItems);
    mFloorItems.clear();
    mSelectedObjectItems.clear();
    mMouseOverObject = nullptr;

    if (mBuildingMap) {
        delete mBuildingMap;
        mBuildingMap = nullptr;

        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;

        dynamic_cast<IsoBuildingRenderer*>(mRenderer)->mMapRenderer = nullptr;

        mLayerGroupWithToolTiles = nullptr;
        mNonEmptyLayerGroupItem = nullptr;
        mNonEmptyLayer.clear();
    }

    if (!mDocument)
        return;

    BuildingToMap();

    for (BuildingFloor *floor : mDocument->building()->floors())
        BuildingBaseScene::floorAdded(floor);

    mLoading = true;
    currentFloorChanged();
    mLoading = false;

    setSceneRect(mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer()));
    mDarkRectangle->setRect(sceneRect());

    connect(mDocument, SIGNAL(currentFloorChanged()),
            SLOT(currentFloorChanged()));
    connect(mDocument, SIGNAL(currentLayerChanged()),
            SLOT(currentLayerChanged()));

    connect(mDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));
    connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
            SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));

    connect(mDocument, SIGNAL(roomDefinitionChanged()),
            SLOT(roomDefinitionChanged()));

    connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(floorAdded(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorRemoved(BuildingFloor*)),
            SLOT(floorRemoved(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorEdited(BuildingFloor*)),
            SLOT(floorEdited(BuildingFloor*)));

    connect(mDocument, SIGNAL(floorTilesChanged(BuildingFloor*)),
            SLOT(floorTilesChanged(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorTilesChanged(BuildingFloor*,QString,QRect)),
            SLOT(floorTilesChanged(BuildingFloor*,QString,QRect)));

    connect(mDocument, SIGNAL(layerOpacityChanged(BuildingFloor*,QString)),
            SLOT(layerOpacityChanged(BuildingFloor*,QString)));
    connect(mDocument, SIGNAL(layerVisibilityChanged(BuildingFloor*,QString)),
            SLOT(layerVisibilityChanged(BuildingFloor*,QString)));

    connect(mDocument, SIGNAL(objectAdded(BuildingObject*)),
            SLOT(objectAdded(BuildingObject*)));
    connect(mDocument, SIGNAL(objectAboutToBeRemoved(BuildingObject*)),
            SLOT(objectAboutToBeRemoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectRemoved(BuildingObject*)),
            SLOT(objectRemoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectMoved(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectTileChanged(BuildingObject*)),
            SLOT(objectTileChanged(BuildingObject*)));
    connect(mDocument, SIGNAL(objectChanged(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));

    connect(mDocument, SIGNAL(selectedObjectsChanged()),
            SLOT(selectedObjectsChanged()));

    connect(mDocument, SIGNAL(buildingResized()), SLOT(buildingResized()));
    connect(mDocument, SIGNAL(buildingRotated()), SLOT(buildingRotated()));

    connect(mDocument, SIGNAL(roomAdded(Room*)), SLOT(roomAdded(Room*)));
    connect(mDocument, SIGNAL(roomRemoved(Room*)), SLOT(roomRemoved(Room*)));
    connect(mDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));

    emit documentChanged();
}

void BuildingIsoScene::clearDocument()
{
    setDocument(nullptr);
    emit documentChanged();
}

QStringList BuildingIsoScene::layerNames() const
{
    if (!mDocument)
        return QStringList();

    QStringList ret;
    int level = mDocument->currentLevel();
    if (CompositeLayerGroup *lg = mBuildingMap->mapComposite()->layerGroupForLevel(level)) {
        foreach (TileLayer *tl, lg->layers())
            ret += MapComposite::layerNameWithoutPrefix(tl);
    }
    return ret;
}

QPoint IsoBuildingRenderer::sceneToTile(const QPointF &scenePos, int level)
{
    QPointF p = mMapRenderer->pixelToTileCoords(scenePos, level);

    // x/y < 0 rounds up to zero
    qreal x = p.x(), y = p.y();
    if (x < 0)
        x = -qCeil(-x);
    if (y < 0)
        y = -qCeil(-y);
    return QPoint(x, y);
}

QPointF IsoBuildingRenderer::sceneToTileF(const QPointF &scenePos, int level)
{
    return mMapRenderer->pixelToTileCoords(scenePos, level);
}

QRect IsoBuildingRenderer::sceneToTileRect(const QRectF &sceneRect, int level)
{
    QPoint topLeft = sceneToTile(sceneRect.topLeft(), level);
    QPoint botRight = sceneToTile(sceneRect.bottomRight(), level);
    return QRect(topLeft, botRight);
}

QRectF IsoBuildingRenderer::sceneToTileRectF(const QRectF &sceneRect, int level)
{
    QPointF topLeft = sceneToTileF(sceneRect.topLeft(), level);
    QPointF botRight = sceneToTileF(sceneRect.bottomRight(), level);
    return QRectF(topLeft, botRight);
}

QPointF IsoBuildingRenderer::tileToScene(const QPoint &tilePos, int level)
{
    return mMapRenderer->tileToPixelCoords(tilePos, level);
}

QPointF IsoBuildingRenderer::tileToSceneF(const QPointF &tilePos, int level)
{
    return mMapRenderer->tileToPixelCoords(tilePos, level);
}

QPolygonF IsoBuildingRenderer::tileToScenePolygon(const QPoint &tilePos, int level)
{
    return mMapRenderer->tileToPixelCoords(QRect(tilePos, QSize(1,1)));
#if 0
    QPolygonF polygon;
    polygon += tilePos;
    polygon += tilePos + QPoint(1, 0);
    polygon += tilePos + QPoint(1, 1);
    polygon += tilePos + QPoint(0, 1);
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
#endif
}

QPolygonF IsoBuildingRenderer::tileToScenePolygon(const QRect &tileRect, int level)
{
    return mMapRenderer->tileToPixelCoords(tileRect, level);
#if 0
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight() + QPoint(1, 0);
    polygon += tileRect.bottomRight() + QPoint(1, 1);
    polygon += tileRect.bottomLeft() + QPoint(0, 1);
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
#endif
}

QPolygonF IsoBuildingRenderer::tileToScenePolygonF(const QRectF &tileRect, int level)
{
    return mMapRenderer->tileToPixelCoords(tileRect, level);
#if 0
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight();
    polygon += tileRect.bottomRight();
    polygon += tileRect.bottomLeft();
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
#endif
}

QPolygonF IsoBuildingRenderer::tileToScenePolygon(const QPolygonF &tilePolygon, int level)
{
    QPolygonF polygon(tilePolygon.size());
    for (int i = tilePolygon.size() - 1; i >= 0; --i)
        polygon[i] = tileToSceneF(tilePolygon[i], level);
    return polygon;
}

void IsoBuildingRenderer::drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level)
{
    painter->drawLine(tileToSceneF(QPointF(x1, y1), level), tileToSceneF(QPointF(x2, y2), level));
}

void IsoBuildingRenderer::setRotation(MapRotation rotation)
{
    mMapRenderer->setRotation(rotation);
}

MapRotation IsoBuildingRenderer::getRotation() const
{
    return mMapRenderer->rotation();
}

bool BuildingIsoScene::currentFloorContains(const QPoint &tilePos, int dw, int dh)
{
    return currentFloor() && currentFloor()->contains(tilePos, dw, dh);
}

void BuildingIsoScene::setToolTiles(const FloorTileGrid *tiles,
                                    const QPoint &pos,
                                    const QString &layerName)
{
    clearToolTiles();

    CompositeLayerGroupItem *item = itemForFloor(currentFloor());
    CompositeLayerGroup *layerGroup = item->layerGroup();

    TileLayer *layer = nullptr;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    QMap<QString,Tileset*> tilesetByName;
    foreach (Tileset *ts, mBuildingMap->map()->tilesets())
        tilesetByName[ts->name()] = ts;

    QSize tilesSize(tiles->width(), tiles->height());
    mToolTiles.resize(tilesSize, QPoint());

    for (int x = 0; x < tiles->width(); x++) {
        for (int y = 0; y < tiles->height(); y++) {
            QString tileName = tiles->at(x, y);
            Tile *tile = 0;
            if (!tileName.isEmpty()) {
                tile = TilesetManager::instance()->missingTile();
                QString tilesetName;
                int index;
                if (BuildingTilesMgr::parseTileName(tileName, tilesetName, index)) {
                    if (tilesetByName.contains(tilesetName))
                        tile = tilesetByName[tilesetName]->tileAt(index);
                }
            }
            mToolTiles.setCell(x, y, Cell(tile));
        }
    }

    layerGroup->setToolTiles(&mToolTiles, pos, QRect(pos, tilesSize), layer);
    if (layerGroup->bounds().isEmpty() && !mToolTiles.isEmpty()) {
        item->synchWithTileLayers();
        item->updateBounds();
    }
    mLayerGroupWithToolTiles = layerGroup;

    QRectF r = mBuildingMap->mapRenderer()->boundingRect(tiles->bounds().translated(pos), currentLevel())
            .adjusted(0, -(128-32)*2, 0, 0); // use mMap->drawMargins()
    update(r);
}

void BuildingIsoScene::clearToolTiles()
{
    if (mLayerGroupWithToolTiles) {
        mLayerGroupWithToolTiles->clearToolTiles();
        mLayerGroupWithToolTiles = 0;
    }
}

QString BuildingIsoScene::buildingTileAt(int x, int y)
{
    return mBuildingMap->buildingTileAt(x, y, currentLevel(), currentLayerName());
}

QString BuildingIsoScene::tileUnderPoint(int x, int y)
{
    QList<bool> visible;
    foreach (CompositeLayerGroupItem *item, mLayerGroupItems)
        visible += item->isVisible();
    return mBuildingMap->buildingTileAt(x, y, visible);
}

void BuildingIsoScene::drawTileSelection(QPainter *painter, const QRegion &region,
                                              const QColor &color, const QRectF &exposed,
                                              int level) const
{
    mapRenderer()->drawTileSelection(painter, region, color, exposed, level);
}

void BuildingIsoScene::setCursorObject(BuildingObject *object)
{
    mBuildingMap->setCursorObject(currentFloor(), object);
}

void BuildingIsoScene::dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset)
{
    mBuildingMap->dragObject(floor, object, offset);
}

void BuildingIsoScene::resetDrag(BuildingFloor *floor, BuildingObject *object)
{
    mBuildingMap->resetDrag(floor, object);
}

void BuildingIsoScene::changeFloorGrid(BuildingFloor *floor,
                                            const QVector<QVector<Room*> > &grid)
{
    mBuildingMap->changeFloorGrid(floor, grid);
}

void BuildingIsoScene::resetFloorGrid(BuildingFloor *floor)
{
    mBuildingMap->resetFloorGrid(floor);
}

void BuildingIsoScene::changeUserTiles(BuildingFloor *floor,
                                       const QMap<QString,FloorTileGrid*> &tiles)
{
    mBuildingMap->changeUserTiles(floor, tiles);
}

void BuildingIsoScene::resetUserTiles(BuildingFloor *floor)
{
    mBuildingMap->resetUserTiles(floor);
}

bool BuildingIsoScene::shouldShowFloorItem(BuildingFloor *floor) const
{
    return !mEditingTiles
            && (currentLevel() == floor->level());
}

bool BuildingIsoScene::shouldShowObjectItem(BuildingObject *object) const
{
    // Cursor items are always visible.
    if (!object->floor())
        return true;

    return !mEditingTiles
            && prefs()->showObjects()
            && (currentLevel() == object->floor()->level());
}

void BuildingIsoScene::setShowBuildingTiles(bool show)
{
    Q_UNUSED(show)
}

void BuildingIsoScene::setShowUserTiles(bool show)
{
    Q_UNUSED(show)
}

void BuildingIsoScene::setEditingTiles(bool editing)
{
    if (editing != mEditingTiles) {
        mEditingTiles = editing;
        synchObjectItemVisibility();
        if (mGridItem)
            mGridItem->setEditingTiles(editing);
    }
}

bool isRectAdjacent(const QRect &r, const QRect &r2)
{
    return r.adjusted(-1, -1, 1, 1).intersects(r2);
}

QVector<QRect> adjacentRects(const QVector<QRect> &rects, const QPoint &pos)
{
    QRect startRect;
    foreach (QRect r, rects) {
        if (r.contains(pos)) {
            startRect = r;
            break;
        }
    }
    QVector<QRect> ret;
    if (startRect.isEmpty())
        return ret;

    QVector<QRect> remaining = rects;
    remaining.remove(rects.indexOf(startRect));

    ret += startRect;
    while (1) {
        int count = remaining.size();
        for (int i = 0; i < remaining.size(); i++) {
            QRect r = remaining[i];
            foreach (QRect r2, ret) {
                if (isRectAdjacent(r, r2)) {
                    ret += r;
                    remaining.remove(i);
                    i--;
                    break;
                }
            }
        }
        if (count == remaining.size())
            break;
    }

    return ret;
}

#include "buildingroomdef.h"
void BuildingIsoScene::setCursorPosition(const QPoint &pos)
{
    if (mHighlightRoomLock)
        return;
    mHighlightRoomPos = pos;
    if (!currentFloor())
        return;
    Room *room = prefs()->highlightRoom() ? currentFloor()->GetRoomAt(pos) : 0;
    if (room) {
        QRegion roomRegion;
#if 1
        BuildingRoomDefecator rd(currentFloor(), room);
        rd.defecate();
        QVector<QRect> rects;
        foreach (QRegion rgn, rd.mRegions) {
            if (rgn.contains(pos)) {
                rects += rgn.rects();
                break;
            }
        }
#else
        QVector<QRect> rects = currentFloor()->roomRegion(room);
#endif
        foreach (QRect r, adjacentRects(rects, pos))
            roomRegion |= r;
        mBuildingMap->suppressTiles(currentFloor(), QRegion(currentFloor()->bounds(1, 1)) - roomRegion);
    } else {
        mBuildingMap->suppressTiles(currentFloor(), QRegion());
    }
}

void BuildingIsoScene::BuildingToMap()
{
    if (mBuildingMap) {
        delete mBuildingMap;

        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;
        delete mTileSelectionItem;

        mLayerGroupWithToolTiles = nullptr;
        mNonEmptyLayerGroupItem = nullptr;
        mNonEmptyLayer.clear();
    }

    mBuildingMap = new BuildingMap(building());
    connect(mBuildingMap, SIGNAL(aboutToRecreateLayers()), SLOT(aboutToRecreateLayers()));
    connect(mBuildingMap, SIGNAL(layersRecreated()), SLOT(layersRecreated()));
    connect(mBuildingMap, SIGNAL(mapResized()), SLOT(mapResized()));
    connect(mBuildingMap, SIGNAL(layersUpdated(int,QRegion)),
            SLOT(layersUpdated(int,QRegion)));

    mRenderer->asIso()->mMapRenderer = mBuildingMap->mapRenderer();

    foreach (CompositeLayerGroup *layerGroup, mBuildingMap->mapComposite()->layerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mBuildingMap->mapRenderer());
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }

    mGridItem = new TileModeGridItem(mDocument, mBuildingMap->mapRenderer());
    mGridItem->setEditingTiles(editingTiles());
    mGridItem->synchWithBuilding();
    mGridItem->setZValue(ZVALUE_GRID);
    addItem(mGridItem);

    mTileSelectionItem = new TileModeSelectionItem(this);
    addItem(mTileSelectionItem);

    mRoomSelectionItem = new RoomSelectionItem(this);
    addItem(mRoomSelectionItem);

    setSceneRect(mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer()));
    mDarkRectangle->setRect(sceneRect());
}

CompositeLayerGroupItem *BuildingIsoScene::itemForFloor(BuildingFloor *floor)
{
    if (mLayerGroupItems.contains(floor->level()))
        return mLayerGroupItems[floor->level()];
    return nullptr;
}

BuildingPreferences *BuildingIsoScene::prefs() const
{
    return BuildingPreferences::instance();
}

void BuildingIsoScene::floorEdited(BuildingFloor *floor)
{
    BuildingBaseScene::floorEdited(floor);

    mBuildingMap->floorEdited(floor);
}

void BuildingIsoScene::floorTilesChanged(BuildingFloor *floor)
{
    mBuildingMap->floorTilesChanged(floor);
}

void BuildingIsoScene::floorTilesChanged(BuildingFloor *floor,
                                              const QString &layerName,
                                              const QRect &bounds)
{
    mBuildingMap->floorTilesChanged(floor, layerName, bounds);
}

void BuildingIsoScene::layerOpacityChanged(BuildingFloor *floor,
                                                const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerOpacity(layerName, floor->layerOpacity(layerName)))
            item->update();
    }
}

void BuildingIsoScene::layerVisibilityChanged(BuildingFloor *floor, const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerVisibility(layerName, floor->layerVisibility(layerName))) {
            item->synchWithTileLayers();
            item->updateBounds();
        }
    }
}

void BuildingIsoScene::currentFloorChanged()
{
    synchObjectItemVisibility();

    highlightFloorChanged(prefs()->highlightFloor());

    mGridItem->synchWithBuilding();

    if (!mNonEmptyLayer.isEmpty()) {
        mNonEmptyLayerGroupItem->layerGroup()->setLayerNonEmpty(mNonEmptyLayer, false);
        mNonEmptyLayerGroupItem->layerGroup()->setHighlightLayer(QString());
        mNonEmptyLayerGroupItem->update();
        mNonEmptyLayer.clear();
        mNonEmptyLayerGroupItem = 0;
    }

    if (BuildingFloor *floor = building()->floor(mCurrentLevel))
        mBuildingMap->suppressTiles(floor, QRegion());
    mCurrentLevel = currentLevel();
}

void BuildingIsoScene::currentLayerChanged()
{
    if (CompositeLayerGroupItem *item = itemForFloor(currentFloor())) {
        QRectF bounds = item->boundingRect();

        if (!mNonEmptyLayer.isEmpty()) {
            mNonEmptyLayerGroupItem->layerGroup()->setLayerNonEmpty(mNonEmptyLayer, false);
            mNonEmptyLayerGroupItem->layerGroup()->setHighlightLayer(QString());
        }
        QString layerName = currentLayerName();
        if (!layerName.isEmpty())
            item->layerGroup()->setLayerNonEmpty(layerName, true);
        mNonEmptyLayer = layerName;
        mNonEmptyLayerGroupItem = item;

        item->layerGroup()->setHighlightLayer(tr("%1_%2").arg(currentLevel()).arg(mNonEmptyLayer));
        item->update();

        if (bounds.isEmpty()) {
            item->synchWithTileLayers();
            item->updateBounds();
        }
        QRectF sceneRect = mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer());
        if (sceneRect != this->sceneRect()) {
            setSceneRect(sceneRect);
            mDarkRectangle->setRect(sceneRect);
        }
    }
}

void BuildingIsoScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    Q_UNUSED(pos);
    mBuildingMap->floorEdited(floor);
}

void BuildingIsoScene::roomDefinitionChanged()
{
    // Also called when the building's exterior wall tile changes.

    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BuildingBaseScene::floorEdited(floor);
    }
}

void BuildingIsoScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BuildingBaseScene::floorEdited(floor);
    }

    mBuildingMap->roomAdded(room);
}

void BuildingIsoScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BuildingBaseScene::floorEdited(floor);
    }

    mBuildingMap->roomRemoved(room);
}

void BuildingIsoScene::roomChanged(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BuildingBaseScene::floorEdited(floor);
    }
}

void BuildingIsoScene::floorAdded(BuildingFloor *floor)
{
    BuildingBaseScene::floorAdded(floor);
    mBuildingMap->floorAdded(floor);
}

void BuildingIsoScene::floorRemoved(BuildingFloor *floor)
{
    BuildingBaseScene::floorRemoved(floor);
    mBuildingMap->floorRemoved(floor);
}

void BuildingIsoScene::objectAdded(BuildingObject *object)
{
    if (mLoading)
        return;

    BuildingBaseScene::objectAdded(object);

    mBuildingMap->objectAdded(object);
}

void BuildingIsoScene::objectAboutToBeRemoved(BuildingObject *object)
{
    BuildingBaseScene::objectAboutToBeRemoved(object);

    mBuildingMap->objectAboutToBeRemoved(object);
}

void BuildingIsoScene::objectRemoved(BuildingObject *object)
{
    mBuildingMap->objectRemoved(object);
}

void BuildingIsoScene::objectMoved(BuildingObject *object)
{
    BuildingBaseScene::objectMoved(object);

    mBuildingMap->objectMoved(object);
}

void BuildingIsoScene::objectTileChanged(BuildingObject *object)
{
    BuildingBaseScene::objectTileChanged(object);

    mBuildingMap->objectTileChanged(object);
}

void BuildingIsoScene::buildingResized()
{
    BuildingBaseScene::buildingResized();
    mBuildingMap->buildingResized();
    mGridItem->synchWithBuilding();
}

// Called when the building is flipped or rotated.
void BuildingIsoScene::buildingRotated()
{
    BuildingBaseScene::buildingRotated();
    mBuildingMap->buildingRotated();
    mGridItem->synchWithBuilding();
}

void BuildingIsoScene::highlightFloorChanged(bool highlight)
{
    qreal z = 0;

    if (!mDocument) {
        mDarkRectangle->setVisible(false);
        return;
    }

    int currentLevel = mDocument->currentLevel();
    foreach (CompositeLayerGroupItem *item, mLayerGroupItems) {
        item->setVisible(!highlight ||
                         (item->layerGroup()->level() == currentLevel) ||
                         (item->layerGroup()->level() < currentLevel && prefs()->showLowerFloors()));
        if (item->layerGroup()->level() == currentLevel)
            z = item->zValue() - 0.5;
    }

    mDarkRectangle->setVisible(highlight);
    mDarkRectangle->setZValue(z);
}

void BuildingIsoScene::highlightRoomChanged(bool highlight)
{
    Q_UNUSED(highlight)
    setCursorPosition(mHighlightRoomPos);
}

void BuildingIsoScene::showLowerFloorsChanged(bool show)
{
    Q_UNUSED(show)
    highlightFloorChanged(prefs()->highlightFloor());
}

void BuildingIsoScene::tilesetAdded(Tileset *tileset)
{
    if (!mDocument)
        return;
    mBuildingMap->tilesetAdded(tileset);
}

void BuildingIsoScene::tilesetAboutToBeRemoved(Tileset *tileset)
{
    if (!mDocument)
        return;
    clearToolTiles();
    mBuildingMap->tilesetAboutToBeRemoved(tileset);
}

void BuildingIsoScene::tilesetRemoved(Tileset *tileset)
{
    if (!mDocument)
        return;
    mBuildingMap->tilesetRemoved(tileset);
}

void BuildingIsoScene::tilesetChanged(Tileset *tileset)
{
    if (!mDocument)
        return;
    if (mBuildingMap->isTilesetUsed(tileset))
        update();
}

void BuildingIsoScene::currentToolChanged(BaseTool *tool)
{
    mCurrentTool = tool;
}

void BuildingIsoScene::aboutToRecreateLayers()
{
    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();

    mLayerGroupWithToolTiles = nullptr;
    mNonEmptyLayerGroupItem = nullptr;
    mNonEmptyLayer.clear();

    delete mGridItem; // It uses the MapRenderer, which is being recreated.
    mGridItem = nullptr;

    mRenderer->asIso()->mMapRenderer = nullptr;
}

void BuildingIsoScene::layersRecreated()
{
    mRenderer->asIso()->mMapRenderer = mBuildingMap->mapRenderer();

    // Building object positions will change when the number of floors changes.
    BuildingBaseScene::mapResized();

    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();

    mLayerGroupWithToolTiles = nullptr;
    mNonEmptyLayerGroupItem = nullptr;
    mNonEmptyLayer.clear();

    foreach (CompositeLayerGroup *layerGroup, mBuildingMap->mapComposite()->layerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mBuildingMap->mapRenderer());
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }

    Q_ASSERT(mGridItem == nullptr);
    mGridItem = new TileModeGridItem(mDocument, mBuildingMap->mapRenderer());
    mGridItem->setEditingTiles(editingTiles());
    mGridItem->synchWithBuilding();
    mGridItem->setZValue(ZVALUE_GRID);
    addItem(mGridItem);

    highlightFloorChanged(prefs()->highlightFloor());
}

void BuildingIsoScene::mapResized()
{
    // Building object positions will change when the map size changes.
    BuildingBaseScene::mapResized();

    foreach (CompositeLayerGroup *layerGroup, mBuildingMap->mapComposite()->layerGroups()) {
        if (mLayerGroupItems.contains(layerGroup->level())) {
            CompositeLayerGroupItem *item = mLayerGroupItems[layerGroup->level()];
            item->synchWithTileLayers();
            item->updateBounds();
        }
    }

    QRectF sceneRect = mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer());
    if (sceneRect != this->sceneRect()) {
        setSceneRect(sceneRect);
        mDarkRectangle->setRect(sceneRect);
    }

    // TileModeGridItem::mBoundingRect needs updating.
    if (mGridItem)
        mGridItem->synchWithBuilding();

    // Strangely needed to fix graphics corruption when cropping a building.
    // Something to do with setSceneRect() causing the view to scroll (which
    // blits existing pixels?) and those pixels don't get redrawn, leaving
    // crud in the wrong location.  Simply calling update() here doesn't fix
    // the problem.
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void BuildingIsoScene::setRotationDerive()
{
    QRectF sceneRect = mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer());
    if (sceneRect != this->sceneRect()) {
        setSceneRect(sceneRect);
        mDarkRectangle->setRect(sceneRect);
    }
}

void BuildingIsoScene::layersUpdated(int level, const QRegion &rgn)
{
    if (mLayerGroupItems.contains(level)) {
        CompositeLayerGroupItem *item = mLayerGroupItems[level];
        // FIXME: BuildingMap will sometimes call layerGroup->synch() but we
        // won't know it, and won't update the bounds here.  That's why the
        // same code is in mapResized() above.
        if (item->layerGroup()->needsSynch()) {
            item->synchWithTileLayers();
            item->updateBounds();

            QRectF sceneRect = mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer());
            if (sceneRect != this->sceneRect()) {
                setSceneRect(sceneRect);
                mDarkRectangle->setRect(sceneRect);
            }
        }
        for (const QRect& r : rgn.rects())
            item->update(mapRenderer()->boundingRect(r, level).adjusted(0,-(128-32)*2,0,0));
    }
}

/////

BuildingIsoView::BuildingIsoView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this)),
    mHandScrolling(false)
{
    BuildingPreferences *prefs = BuildingPreferences::instance();
    setUseOpenGL(prefs->useOpenGL());
    connect(prefs, SIGNAL(useOpenGLChanged(bool)), SLOT(setUseOpenGL(bool)));

    QWidget *v = viewport();

    /* Since Qt 4.5, setting this attribute yields significant repaint
     * reduction when the view is being resized. */
    v->setAttribute(Qt::WA_StaticContents);

    /* Since Qt 4.6, mouse tracking is disabled when no graphics item uses
     * hover events. We need to set it since our scene wants the events. */
    v->setMouseTracking(true);

    // Adjustment for antialiasing is done by the items that need it
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

BuildingIsoView::~BuildingIsoView()
{
    setHandScrolling(false); // Just in case we didn't get a hide event
}

bool BuildingIsoView::event(QEvent *e)
{
    // Ignore space bar events since they're handled by the MainWindow
    if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease) {
        if (static_cast<QKeyEvent*>(e)->key() == Qt::Key_Space) {
            e->ignore();
            return false;
        }
    }

    return QGraphicsView::event(e);
}

bool BuildingIsoView::eventFilter(QObject *object, QEvent *event)
{
    Q_UNUSED(object)
    Q_UNUSED(event)
#if 0
    Q_UNUSED(object)
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
            TileToolManager::instance()->checkKeyboardModifiers(keyEvent->modifiers());
        }
        break;
    default:
        break;
    }
#endif
    return false;
}

void BuildingIsoView::hideEvent(QHideEvent *event)
{
    // Disable hand scrolling when the view gets hidden in any way
    setHandScrolling(false);
    QGraphicsView::hideEvent(event);
}

void BuildingIsoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void BuildingIsoView::mouseMoveEvent(QMouseEvent *event)
{
    if (mHandScrolling) {
        QScrollBar *hBar = horizontalScrollBar();
        QScrollBar *vBar = verticalScrollBar();
        const QPoint d = event->globalPos() - mLastMousePos;
        hBar->setValue(hBar->value() + (isRightToLeft() ? d.x() : -d.x()));
        vBar->setValue(vBar->value() - d.y());

        mLastMousePos = event->globalPos();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);

    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));

    if (!scene()->document())
        return;

    QPoint tilePos = scene()->sceneToTile(mLastMouseScenePos, scene()->currentLevel());
    if (tilePos != mLastMouseTilePos) {
        mLastMouseTilePos = tilePos;
        emit mouseCoordinateChanged(mLastMouseTilePos);

        scene()->setCursorPosition(tilePos);
    }
}

void BuildingIsoView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(false);
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void BuildingIsoView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(event->delta());

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMousePos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void BuildingIsoView::setDocument(BuildingDocument *doc)
{
    scene()->setDocument(doc);
    centerOn(scene()->sceneRect().center());
}

void BuildingIsoView::clearDocument()
{
    scene()->clearDocument();
}

void BuildingIsoView::setUseOpenGL(bool useOpenGL)
{
#ifndef QT_NO_OPENGL
    if (useOpenGL && QGLFormat::hasOpenGL()) {
        if (!qobject_cast<QOpenGLWidget*>(viewport())) {
//            QSurfaceFormat format = QSurfaceFormat::defaultFormat();
//            format.setDepth(false); // No need for a depth buffer
//            format.setSampleBuffers(true); // Enable anti-aliasing
            QOpenGLWidget *viewport = new QOpenGLWidget();
//            viewport->setFormat(format);
            setViewport(viewport);
        }
    } else {
        if (qobject_cast<QOpenGLWidget*>(viewport()))
            setViewport(0);
    }

    QWidget *v = viewport();
    v->setAttribute(Qt::WA_StaticContents);
    v->setMouseTracking(true);
#endif
}

void BuildingIsoView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
//    qDebug() << "setHandScrolling" << mHandScrolling;
    setInteractive(!mHandScrolling);

    if (mHandScrolling) {
        mLastMousePos = QCursor::pos();
        QApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
        viewport()->grabMouse();
    } else {
        viewport()->releaseMouse();
        QApplication::restoreOverrideCursor();
    }
}

void BuildingIsoView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}
