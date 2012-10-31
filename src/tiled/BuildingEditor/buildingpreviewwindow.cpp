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

#include "buildingpreviewwindow.h"
#include "ui_buildingpreviewwindow.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "objectgroup.h"
#include "tmxmapwriter.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "zlevelrenderer.h"

#include <QMessageBox>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingPreviewWindow::BuildingPreviewWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingPreviewWindow),
    mDocument(0),
    mScene(new BuildingPreviewScene(this))
{
    ui->setupUi(this);

    mView = ui->graphicsView;

    ui->graphicsView->setScene(mScene);

    mView->zoomable()->connectToComboBox(ui->zoomComboBox);
    connect(mView->zoomable(), SIGNAL(scaleChanged(qreal)),
            SLOT(updateActions()));

    QSettings settings;
    bool showWalls = settings.value(QLatin1String("BuildingEditor/PreviewWindow/ShowWalls"),
                                    true).toBool();
    ui->actionShowWalls->setChecked(showWalls);
    connect(ui->actionShowWalls, SIGNAL(toggled(bool)),
            mScene, SLOT(showWalls(bool)));

    connect(ui->actionZoomIn, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomIn()));
    connect(ui->actionZoomOut, SIGNAL(triggered()),
            mView->zoomable(), SLOT(zoomOut()));
    connect(ui->actionNormalSize, SIGNAL(triggered()),
            mView->zoomable(), SLOT(resetZoom()));

    setWindowFlags(Qt::Tool);

    readSettings();

    updateActions();
}

void BuildingPreviewWindow::closeEvent(QCloseEvent *event)
{
#if 0
    writeSettings();
    if (confirmAllSave())
        event->accept();
    else
#endif
        event->ignore();
}

void BuildingPreviewWindow::setDocument(BuildingDocument *doc)
{
    mDocument = doc;

    mScene->setDocument(doc);
}

void BuildingPreviewWindow::clearDocument()
{
    mScene->clearDocument();
}

void BuildingPreviewWindow::readSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/PreviewWindow"));
    QByteArray geom = mSettings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    else
        resize(400, 300);
    restoreState(mSettings.value(QLatin1String("state"),
                                 QByteArray()).toByteArray());
    qreal scale = mSettings.value(QLatin1String("scale"), 0.5f).toReal();
    ui->graphicsView->zoomable()->setScale(scale);
    mSettings.endGroup();
}

void BuildingPreviewWindow::writeSettings()
{
    mSettings.beginGroup(QLatin1String("BuildingEditor/PreviewWindow"));
    mSettings.setValue(QLatin1String("geometry"), saveGeometry());
    mSettings.setValue(QLatin1String("state"), saveState());
    mSettings.setValue(QLatin1String("scale"), ui->graphicsView->zoomable()->scale());
    mSettings.endGroup();
}

bool BuildingPreviewWindow::exportTMX(const QString &fileName)
{
    TmxMapWriter writer;

    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        int n = mScene->map()->indexOfLayer(tr("%1_RoomDefs").arg(floor->level()),
                                            Layer::ObjectGroupType);
        ObjectGroup *objectGroup = mScene->map()->layerAt(n)->asObjectGroup();
        while (objectGroup->objectCount()) {
            MapObject *mapObject = objectGroup->objects().at(0);
            objectGroup->removeObjectAt(0);
            delete mapObject;
        }
        int delta = (mScene->mapComposite()->maxLevel() - floor->level()) * 3;
        QPoint offset(delta, delta); // FIXME: not for LevelIsometric
        foreach (Room *room, mDocument->building()->rooms()) {
            QRegion roomRegion = floor->roomRegion(room);
            foreach (QRect rect, roomRegion.rects()) {
                MapObject *mapObject = new MapObject(room->internalName, tr("room"),
                                                     rect.topLeft() + offset,
                                                     rect.size());
                objectGroup->addObject(mapObject);
            }
        }
    }

    if (!writer.write(mScene->map(), fileName)) {
        QMessageBox::critical(this, tr("Error Saving Map"),
                              writer.errorString());
        return false;
    }
    return true;
}

void BuildingPreviewWindow::updateActions()
{
    ui->actionZoomIn->setEnabled(mView->zoomable()->canZoomIn());
    ui->actionZoomOut->setEnabled(mView->zoomable()->canZoomOut());
    ui->actionNormalSize->setEnabled(mView->zoomable()->scale() != 1.0);
}

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
#if 0 && defined(_DEBUG)
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

PreviewGridItem::PreviewGridItem(Building *building, MapRenderer *renderer) :
    QGraphicsItem(),
    mBuilding(building),
    mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);
    synchWithBuilding();
}

void PreviewGridItem::synchWithBuilding()
{
    int offset = (mBuilding->floorCount() - 1) * 3;
    mTileBounds = QRect(offset, offset, mBuilding->width(), mBuilding->height());

    QRectF bounds = mRenderer->boundingRect(mTileBounds);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

QRectF PreviewGridItem::boundingRect() const
{
    return mBoundingRect;
}

void PreviewGridItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    mRenderer->drawGrid(p, option->exposedRect, Qt::black, 0, mTileBounds);
}

/////

BuildingPreviewScene::BuildingPreviewScene(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMapComposite(0),
    mMap(0),
    mRenderer(0),
    mGridItem(0)
{
    setBackgroundBrush(Qt::darkGray);

    QSettings settings;
    mShowWalls = settings.value(QLatin1String("BuildingEditor/PreviewWindow/ShowWalls"),
                                true).toBool();
}

BuildingPreviewScene::~BuildingPreviewScene()
{
    delete mMapComposite;
    if (mMap)
        TilesetManager::instance()->removeReferences(mMap->tilesets());
    delete mMap;
    delete mRenderer;
}

void BuildingPreviewScene::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    if (mMap) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;
        delete mRenderer;
        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;

        mMapComposite = 0;
        mMap = 0;
        mRenderer = 0;
    }

    if (!mDocument)
        return;

    mMap = new Map(Map::Isometric, doc->building()->width(),
                   doc->building()->height(), 64, 32);

    // Add tilesets from MapBaseXMLLots.txt
    foreach (Tileset *ts, BuildingTiles::instance()->tilesets())
        mMap->addTileset(ts);
    TilesetManager::instance()->addReferences(mMap->tilesets());

    switch (mMap->orientation()) {
    case Map::Isometric:
        mRenderer = new IsometricRenderer(mMap);
        break;
    case Map::LevelIsometric:
        mRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return;
    }

    mGridItem = new PreviewGridItem(mDocument->building(), mRenderer);
    addItem(mGridItem);

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);

    mMapComposite = new MapComposite(mapInfo);

    foreach (BuildingFloor *floor, doc->building()->floors())
        floorAdded(floor);
    currentFloorChanged();

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));

    connect(mDocument, SIGNAL(currentFloorChanged()),
            SLOT(currentFloorChanged()));

    connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
            SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));

    connect(mDocument, SIGNAL(roomDefinitionChanged()),
            SLOT(roomDefinitionChanged()));

    connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(floorAdded(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorEdited(BuildingFloor*)),
            SLOT(floorEdited(BuildingFloor*)));

    connect(mDocument, SIGNAL(objectAdded(BuildingObject*)),
            SLOT(objectAdded(BuildingObject*)));
    connect(mDocument, SIGNAL(objectRemoved(BuildingFloor*,int)),
            SLOT(objectRemoved(BuildingFloor*,int)));
    connect(mDocument, SIGNAL(objectMoved(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectTileChanged(BuildingObject*)),
            SLOT(objectTileChanged(BuildingObject*)));

    connect(mDocument, SIGNAL(buildingResized()), SLOT(buildingResized()));
    connect(mDocument, SIGNAL(buildingRotated()), SLOT(buildingRotated()));

    connect(mDocument, SIGNAL(roomAdded(Room*)), SLOT(roomAdded(Room*)));
    connect(mDocument, SIGNAL(roomRemoved(Room*)), SLOT(roomRemoved(Room*)));
    connect(mDocument, SIGNAL(roomChanged(Room*)), SLOT(roomChanged(Room*)));
}

void BuildingPreviewScene::clearDocument()
{
    setDocument(0);
}

void BuildingPreviewScene::BuildingToMap()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        floor->LayoutToSquares();
        BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    }
}

void BuildingPreviewScene::BuildingFloorToTileLayers(BuildingFloor *floor,
                                                     const QVector<TileLayer *> &layers)
{
    const QMap<QString,Tiled::Tileset*> &tilesetByName =
            BuildingTiles::instance()->tilesetsMap();

    int offset = (mMapComposite->maxLevel() - floor->level()) * 3; // FIXME: not for LevelIsometric

    int index = 0;
    foreach (TileLayer *tl, layers) {
        tl->erase(QRegion(0, 0, tl->width(), tl->height()));
        for (int x = 0; x <= floor->width(); x++) {
            for (int y = 0; y <= floor->height(); y++) {
                const BuildingFloor::Square &square = floor->squares[x][y];
                if (index == LayerIndexFloor) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionFloor];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex)));
                }
                if ((index == LayerIndexWall) && mShowWalls) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionWall];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionWall])));
                }
                if (index == LayerIndexDoor) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionDoor];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionDoor])));
                }
                if (index == LayerIndexFrame) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionFrame];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionFrame])));
                }
                if (index == LayerIndexFurniture) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionFurniture];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionFurniture])));
                }
            }
        }
        index++;
    }
}

void BuildingPreviewScene::floorEdited(BuildingFloor *floor)
{
    // Existence check needed while loading a map.
    // floorAdded -> objectAdded -> floorEdited
    // With stairs, this may be the floor above the one the object is on, but
    // that floor hasn't been added yet.
    if (!mLayerGroupItems.contains(floor->level()))
        return;
    floor->LayoutToSquares();
    BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    mLayerGroupItems[floor->level()]->synchWithTileLayers();
    mLayerGroupItems[floor->level()]->updateBounds();

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
}

void BuildingPreviewScene::currentFloorChanged()
{
    int level = mDocument->currentFloor()->level();
    for (int i = 0; i <= level; i++) {
        mLayerGroupItems[i]->setVisible(true);
    }
    for (int i = level + 1; i < mDocument->building()->floorCount(); i++)
        mLayerGroupItems[i]->setVisible(false);
}

void BuildingPreviewScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    Q_UNUSED(pos);
    floorEdited(floor);
}

void BuildingPreviewScene::roomDefinitionChanged()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingPreviewScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingPreviewScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingPreviewScene::roomChanged(Room *room)
{
    Q_UNUSED(room)
    roomDefinitionChanged();
}

void BuildingPreviewScene::floorAdded(BuildingFloor *floor)
{
    // Resize the map and all existing layers to accomodate the new level.
    int maxLevel = qMax(mMapComposite->maxLevel(), floor->level());
    QSize mapSize = QSize(floor->width() + 3 * maxLevel + 1,
                          floor->height() + 3 * maxLevel + 1);
    bool didResize = false; // always true
    if (mapSize != mMap->size()) {
        int delta = maxLevel - mMapComposite->maxLevel();
        foreach (Layer *layer, mMap->layers())
            layer->resize(mapSize, QPoint(delta * 3, delta * 3));
        mMap->setWidth(mapSize.width());
        mMap->setHeight(mapSize.height());
        didResize = true;
    }

    TileLayer *tl = new TileLayer(tr("%1_Floor").arg(floor->level()), 0, 0,
                                  mapSize.width(), mapSize.height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Walls").arg(floor->level()), 0, 0,
                       mapSize.width(), mapSize.height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Frames").arg(floor->level()), 0, 0,
                       mapSize.width(), mapSize.height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Doors").arg(floor->level()), 0, 0,
                       mapSize.width(), mapSize.height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Furniture").arg(floor->level()), 0, 0,
                       mapSize.width(), mapSize.height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    CompositeLayerGroup *lg = mMapComposite->tileLayersForLevel(floor->level());
    CompositeLayerGroupItem *item = new CompositeLayerGroupItem(lg, mRenderer);
    mLayerGroupItems[lg->level()] = item;
    floor->LayoutToSquares();
    BuildingFloorToTileLayers(floor, lg->layers());
    item->synchWithTileLayers();
    item->updateBounds();
    addItem(item);

    // Add an object layer for RoomDefs.  Objects are only added when
    // exporting to a TMX.
    ObjectGroup *objectGroup = new ObjectGroup(tr("%1_RoomDefs").arg(floor->level()),
                                               0, 0, mapSize.width(), mapSize.height());
    mMap->addLayer(objectGroup);

    foreach (BuildingObject *object, floor->objects())
        objectAdded(object);

    if (didResize) {
        foreach (CompositeLayerGroupItem *item, mLayerGroupItems) {
            int level = item->layerGroup()->level();
            if (level == floor->level())
                continue;
            BuildingFloorToTileLayers(mDocument->building()->floor(level),
                                      item->layerGroup()->layers());
            item->synchWithTileLayers();
            item->updateBounds();
        }
    }

    mGridItem->synchWithBuilding();

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
}

void BuildingPreviewScene::objectAdded(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above
    if (dynamic_cast<Stairs*>(object) && (floor = floor->floorAbove()))
        floorEdited(floor);
}

void BuildingPreviewScene::objectRemoved(BuildingFloor *floor, int index)
{
    Q_UNUSED(index)
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above
    if (floor = floor->floorAbove())
        floorEdited(floor);
}

void BuildingPreviewScene::objectMoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above
    if (dynamic_cast<Stairs*>(object) && (floor = floor->floorAbove()))
        floorEdited(floor);
}

void BuildingPreviewScene::objectTileChanged(BuildingEditor::BuildingObject *object)
{
    floorEdited(object->floor());
}

void BuildingPreviewScene::buildingResized()
{
    buildingRotated();
}

void BuildingPreviewScene::buildingRotated()
{
    int extra = mMapComposite->maxLevel() * 3 + 1;
    int width = mDocument->building()->width() + extra;
    int height = mDocument->building()->height() + extra;
    foreach (Layer *layer, mMap->layers())
        layer->resize(QSize(width, height), QPoint());
    mMap->setWidth(width);
    mMap->setHeight(height);

    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        floorEdited(floor);
    }
    mGridItem->synchWithBuilding();
}

void BuildingPreviewScene::showWalls(bool show)
{
    if (show == mShowWalls)
        return;

    mShowWalls = show;

    foreach (BuildingFloor *floor, mDocument->building()->floors())
        BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    update();
}

/////

BuildingPreviewView::BuildingPreviewView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this))
{
    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

void BuildingPreviewView::mouseMoveEvent(QMouseEvent *event)
{
    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void BuildingPreviewView::wheelEvent(QWheelEvent *event)
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

void BuildingPreviewView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}
