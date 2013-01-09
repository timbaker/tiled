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

#include "buildingtilemodeview.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingmap.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreviewwindow.h" // for CompositeLayerGroupItem;
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
#include <QKeyEvent>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

/////

TileModeGridItem::TileModeGridItem(BuildingDocument *doc, MapRenderer *renderer) :
    QGraphicsItem(),
    mDocument(doc),
    mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    synchWithBuilding();
}

void TileModeGridItem::synchWithBuilding()
{
    mTileBounds = QRect(0, 0,
                        mDocument->building()->width() + 1,
                        mDocument->building()->height() + 1);

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
    mRenderer->drawGrid(p, option->exposedRect, Qt::black,
                        mDocument->currentLevel(), mTileBounds);
}

/////

TileModeSelectionItem::TileModeSelectionItem(BuildingTileModeScene *scene) :
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

BuildingTileModeScene::BuildingTileModeScene(QObject *parent) :
    BaseFloorEditor(parent),
    mBuildingMap(0),
    mGridItem(0),
    mTileSelectionItem(0),
    mDarkRectangle(new QGraphicsRectItem),
    mCurrentTool(0),
    mLayerGroupWithToolTiles(0),
    mNonEmptyLayerGroupItem(0)
{
    ZVALUE_CURSOR = 1000;

    BaseFloorEditor::mRenderer = new IsoBuildingRenderer;

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

    connect(BuildingPreferences::instance(), SIGNAL(highlightFloorChanged(bool)),
            SLOT(highlightFloorChanged(bool)));

    connect(ToolManager::instance(), SIGNAL(currentToolChanged(BaseTool*)),
            SLOT(currentToolChanged(BaseTool*)));
}

BuildingTileModeScene::~BuildingTileModeScene()
{
    delete mBuildingMap;
}

MapRenderer *BuildingTileModeScene::mapRenderer() const
{
    return mBuildingMap->mapRenderer();
}

void BuildingTileModeScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void BuildingTileModeScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void BuildingTileModeScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void BuildingTileModeScene::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    // Delete before clearing mDocument.
    delete mTileSelectionItem;
    mTileSelectionItem = 0;

    mDocument = doc;

    qDeleteAll(mFloorItems);
    mFloorItems.clear();
    mSelectedObjectItems.clear();

    if (mBuildingMap) {
        delete mBuildingMap;
        mBuildingMap = 0;

        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;

        dynamic_cast<IsoBuildingRenderer*>(mRenderer)->mMapRenderer = 0;

        mLayerGroupWithToolTiles = 0;
        mNonEmptyLayerGroupItem = 0;
        mNonEmptyLayer.clear();
    }

    if (!mDocument)
        return;

    BuildingToMap();

    foreach (BuildingFloor *floor, mDocument->building()->floors())
        BaseFloorEditor::floorAdded(floor);

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

void BuildingTileModeScene::clearDocument()
{
    setDocument(0);
    emit documentChanged();
}

QStringList BuildingTileModeScene::layerNames() const
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
    QPolygonF polygon;
    polygon += tilePos;
    polygon += tilePos + QPoint(1, 0);
    polygon += tilePos + QPoint(1, 1);
    polygon += tilePos + QPoint(0, 1);
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
}

QPolygonF IsoBuildingRenderer::tileToScenePolygon(const QRect &tileRect, int level)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight() + QPoint(1, 0);
    polygon += tileRect.bottomRight() + QPoint(1, 1);
    polygon += tileRect.bottomLeft() + QPoint(0, 1);
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
}

QPolygonF IsoBuildingRenderer::tileToScenePolygonF(const QRectF &tileRect, int level)
{
    QPolygonF polygon;
    polygon += tileRect.topLeft();
    polygon += tileRect.topRight();
    polygon += tileRect.bottomRight();
    polygon += tileRect.bottomLeft();
    polygon += polygon.first();
    return mMapRenderer->tileToPixelCoords(polygon, level);
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

bool BuildingTileModeScene::currentFloorContains(const QPoint &tilePos, int dw, int dh)
{
    return currentFloor() && currentFloor()->contains(tilePos, dw, dh);
}

void BuildingTileModeScene::setToolTiles(const FloorTileGrid *tiles,
                                         const QPoint &pos,
                                         const QString &layerName)
{
    clearToolTiles();

    CompositeLayerGroupItem *item = itemForFloor(currentFloor());
    CompositeLayerGroup *layerGroup = item->layerGroup();

    TileLayer *layer = 0;
    foreach (TileLayer *tl, layerGroup->layers()) {
        if (layerName == MapComposite::layerNameWithoutPrefix(tl)) {
            layer = tl;
            break;
        }
    }

    QMap<QString,Tileset*> tilesetByName;
    foreach (Tileset *ts, mBuildingMap->map()->tilesets())
        tilesetByName[ts->name()] = ts;

    QVector<QVector<Tiled::Cell> > cells(tiles->width());
    for (int x = 0; x < tiles->width(); x++)
        cells[x].resize(tiles->height());

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
            cells[x][y] = Cell(tile);
        }
    }

    layerGroup->setToolTiles(cells, pos, layer);
    mLayerGroupWithToolTiles = layerGroup;

    QRectF r = mBuildingMap->mapRenderer()->boundingRect(tiles->bounds().translated(pos), currentLevel())
            .adjusted(0, -(128-32), 0, 0); // use mMap->drawMargins()
    update(r);
}

void BuildingTileModeScene::clearToolTiles()
{
    if (mLayerGroupWithToolTiles) {
        mLayerGroupWithToolTiles->clearToolTiles();
        mLayerGroupWithToolTiles = 0;
    }
}

QString BuildingTileModeScene::buildingTileAt(int x, int y)
{
    return mBuildingMap->buildingTileAt(x, y, currentLevel(), currentLayerName());
}

void BuildingTileModeScene::drawTileSelection(QPainter *painter, const QRegion &region,
                                              const QColor &color, const QRectF &exposed,
                                              int level) const
{
    mapRenderer()->drawTileSelection(painter, region, color, exposed, level);
}

void BuildingTileModeScene::setCursorObject(BuildingObject *object, const QRect &bounds)
{
    mBuildingMap->setCursorObject(currentFloor(), object, bounds);
}

void BuildingTileModeScene::BuildingToMap()
{
    if (mBuildingMap) {
        delete mBuildingMap;

        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;
        delete mTileSelectionItem;

        mLayerGroupWithToolTiles = 0;
        mNonEmptyLayerGroupItem = 0;
        mNonEmptyLayer.clear();
    }

    mBuildingMap = new BuildingMap(building());
    connect(mBuildingMap, SIGNAL(aboutToRecreateLayers()), SLOT(aboutToRecreateLayers()));
    connect(mBuildingMap, SIGNAL(layersRecreated()), SLOT(layersRecreated()));
    connect(mBuildingMap, SIGNAL(mapResized()), SLOT(mapResized()));
    connect(mBuildingMap, SIGNAL(layersUpdated(int)), SLOT(layersUpdated(int)));

    dynamic_cast<IsoBuildingRenderer*>(mRenderer)->mMapRenderer = mBuildingMap->mapRenderer();

    foreach (CompositeLayerGroup *layerGroup, mBuildingMap->mapComposite()->layerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mBuildingMap->mapRenderer());
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }

    mGridItem = new TileModeGridItem(mDocument, mBuildingMap->mapRenderer());
    mGridItem->synchWithBuilding();
    mGridItem->setZValue(1000);
    addItem(mGridItem);

    mTileSelectionItem = new TileModeSelectionItem(this);
    addItem(mTileSelectionItem);

    setSceneRect(mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer()));
    mDarkRectangle->setRect(sceneRect());
}

CompositeLayerGroupItem *BuildingTileModeScene::itemForFloor(BuildingFloor *floor)
{
    if (mLayerGroupItems.contains(floor->level()))
        return mLayerGroupItems[floor->level()];
    return 0;
}

void BuildingTileModeScene::floorEdited(BuildingFloor *floor)
{
    BaseFloorEditor::floorEdited(floor);

    mBuildingMap->floorEdited(floor);
}

void BuildingTileModeScene::floorTilesChanged(BuildingFloor *floor)
{
    mBuildingMap->floorTilesChanged(floor);
}

void BuildingTileModeScene::floorTilesChanged(BuildingFloor *floor,
                                              const QString &layerName,
                                              const QRect &bounds)
{
    mBuildingMap->floorTilesChanged(floor, layerName, bounds);
}

void BuildingTileModeScene::layerOpacityChanged(BuildingFloor *floor,
                                                const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerOpacity(layerName, floor->layerOpacity(layerName)))
            item->update();
    }
}

void BuildingTileModeScene::layerVisibilityChanged(BuildingFloor *floor, const QString &layerName)
{
    if (CompositeLayerGroupItem *item = itemForFloor(floor)) {
        if (item->layerGroup()->setLayerVisibility(layerName, floor->layerVisibility(layerName))) {
            item->synchWithTileLayers();
            item->updateBounds();
        }
    }
}

void BuildingTileModeScene::currentFloorChanged()
{
    for (int i = 0; i <= currentLevel(); i++) {
//        mFloorItems[i]->setOpacity((i == currentLevel()) ? 0.25 : 0.05);
        mFloorItems[i]->setVisible(i == currentLevel());
    }
    for (int i = currentLevel() + 1; i < mDocument->building()->floorCount(); i++)
        mFloorItems[i]->setVisible(false);


    highlightFloorChanged(BuildingPreferences::instance()->highlightFloor());
    mGridItem->synchWithBuilding();

    if (!mNonEmptyLayer.isEmpty()) {
        mNonEmptyLayerGroupItem->layerGroup()->setLayerNonEmpty(mNonEmptyLayer, false);
        mNonEmptyLayerGroupItem->layerGroup()->setHighlightLayer(QString());
        mNonEmptyLayerGroupItem->update();
        mNonEmptyLayer.clear();
        mNonEmptyLayerGroupItem = 0;
    }
}

void BuildingTileModeScene::currentLayerChanged()
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

void BuildingTileModeScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    Q_UNUSED(pos);
    mBuildingMap->floorEdited(floor);
}

void BuildingTileModeScene::roomDefinitionChanged()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BaseFloorEditor::floorEdited(floor);
    }
}

void BuildingTileModeScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BaseFloorEditor::floorEdited(floor);
    }
}

void BuildingTileModeScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BaseFloorEditor::floorEdited(floor);
    }
}

void BuildingTileModeScene::roomChanged(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        mBuildingMap->floorEdited(floor);
        BaseFloorEditor::floorEdited(floor);
    }
}

void BuildingTileModeScene::floorAdded(BuildingFloor *floor)
{
    BaseFloorEditor::floorAdded(floor);
#if 1
    mBuildingMap->floorAdded(floor);
#else
    BuildingToMap();
#endif
}

void BuildingTileModeScene::floorRemoved(BuildingFloor *floor)
{
    BaseFloorEditor::floorRemoved(floor);
#if 1
    mBuildingMap->floorRemoved(floor);
#else
    BuildingToMap();
#endif
}

void BuildingTileModeScene::objectAdded(BuildingObject *object)
{
    if (mLoading)
        return;

    BaseFloorEditor::objectAdded(object);

    mBuildingMap->objectAdded(object);
}

void BuildingTileModeScene::objectAboutToBeRemoved(BuildingObject *object)
{
    BaseFloorEditor::objectAboutToBeRemoved(object);

    mBuildingMap->objectAboutToBeRemoved(object);
}

void BuildingTileModeScene::objectRemoved(BuildingObject *object)
{
    mBuildingMap->objectRemoved(object);
}

void BuildingTileModeScene::objectMoved(BuildingObject *object)
{
    BaseFloorEditor::objectMoved(object);

    mBuildingMap->objectMoved(object);
}

void BuildingTileModeScene::objectTileChanged(BuildingObject *object)
{
    BaseFloorEditor::objectTileChanged(object);

    mBuildingMap->objectTileChanged(object);
}

void BuildingTileModeScene::buildingResized()
{
    BaseFloorEditor::buildingResized();
    mBuildingMap->buildingResized();
    mGridItem->synchWithBuilding();
}

// Called when the building is flipped or rotated.
void BuildingTileModeScene::buildingRotated()
{
    BaseFloorEditor::buildingRotated();
    mBuildingMap->buildingRotated();
    mGridItem->synchWithBuilding();
}

void BuildingTileModeScene::highlightFloorChanged(bool highlight)
{
    qreal z = 0;

    if (!mDocument) {
        mDarkRectangle->setVisible(false);
        return;
    }

    int currentLevel = mDocument->currentLevel();
    foreach (CompositeLayerGroupItem *item, mLayerGroupItems) {
        item->setVisible(!highlight ||
                         (item->layerGroup()->level() <= currentLevel));
        if (item->layerGroup()->level() == currentLevel)
            z = item->zValue() - 0.5;
    }

    mDarkRectangle->setVisible(highlight);
    mDarkRectangle->setZValue(z);
}

void BuildingTileModeScene::tilesetAdded(Tileset *tileset)
{
    if (!mDocument)
        return;
    mBuildingMap->tilesetAdded(tileset);
}

void BuildingTileModeScene::tilesetAboutToBeRemoved(Tileset *tileset)
{
    if (!mDocument)
        return;
    clearToolTiles();
    mBuildingMap->tilesetAboutToBeRemoved(tileset);
}

void BuildingTileModeScene::tilesetRemoved(Tileset *tileset)
{
    if (!mDocument)
        return;
    mBuildingMap->tilesetRemoved(tileset);
}

void BuildingTileModeScene::currentToolChanged(BaseTool *tool)
{
    mCurrentTool = tool;
}

void BuildingTileModeScene::aboutToRecreateLayers()
{
    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();

    mLayerGroupWithToolTiles = 0;
    mNonEmptyLayerGroupItem = 0;
    mNonEmptyLayer.clear();
}

void BuildingTileModeScene::layersRecreated()
{
    // Building object positions will change when the number of floors changes.
    BaseFloorEditor::mapResized();

    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();

    mLayerGroupWithToolTiles = 0;
    mNonEmptyLayerGroupItem = 0;
    mNonEmptyLayer.clear();

    foreach (CompositeLayerGroup *layerGroup, mBuildingMap->mapComposite()->layerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mBuildingMap->mapRenderer());
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }
}

void BuildingTileModeScene::mapResized()
{
    // Building object positions will change when the map size changes.
    BaseFloorEditor::mapResized();
}

void BuildingTileModeScene::layersUpdated(int level)
{
    if (mLayerGroupItems.contains(level)) {
        CompositeLayerGroupItem *item = mLayerGroupItems[level];
        if (item->layerGroup()->needsSynch()) {
            item->synchWithTileLayers();
            item->updateBounds();

            QRectF sceneRect = mBuildingMap->mapComposite()->boundingRect(mBuildingMap->mapRenderer());
            if (sceneRect != this->sceneRect()) {
                setSceneRect(sceneRect);
                mDarkRectangle->setRect(sceneRect);
            }
        }
        item->update();
    }
}

/////

BuildingTileModeView::BuildingTileModeView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this)),
    mHandScrolling(false)
{
    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));

    // Install an event filter so that we can get key events on behalf of the
    // active tool without having to have the current focus.
    qApp->installEventFilter(this);
}

BuildingTileModeView::~BuildingTileModeView()
{
    setHandScrolling(false); // Just in case we didn't get a hide event
}

bool BuildingTileModeView::event(QEvent *e)
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

bool BuildingTileModeView::eventFilter(QObject *object, QEvent *event)
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

void BuildingTileModeView::hideEvent(QHideEvent *event)
{
    // Disable hand scrolling when the view gets hidden in any way
    setHandScrolling(false);
    QGraphicsView::hideEvent(event);
}

void BuildingTileModeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton) {
        setHandScrolling(true);
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void BuildingTileModeView::mouseMoveEvent(QMouseEvent *event)
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
    }
}

void BuildingTileModeView::mouseReleaseEvent(QMouseEvent *event)
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
void BuildingTileModeView::wheelEvent(QWheelEvent *event)
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

void BuildingTileModeView::setHandScrolling(bool handScrolling)
{
    if (mHandScrolling == handScrolling)
        return;

    mHandScrolling = handScrolling;
    qDebug() << "setHandScrolling" << mHandScrolling;
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

void BuildingTileModeView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}
