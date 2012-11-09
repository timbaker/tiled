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
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingtmx.h"

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
#include <QSettings>
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

    BuildingPreferences *prefs = BuildingPreferences::instance();

    QSettings settings;
    ui->actionShowWalls->setChecked(prefs->showWalls());
    connect(ui->actionShowWalls, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowWalls(bool)));
    connect(prefs, SIGNAL(showWallsChanged(bool)),
            mScene, SLOT(showWallsChanged(bool)));

    ui->actionHighlightFloor->setChecked(prefs->highlightFloor());
    connect(ui->actionHighlightFloor, SIGNAL(toggled(bool)),
            prefs, SLOT(setHighlightFloor(bool)));
    connect(prefs, SIGNAL(highlightFloorChanged(bool)),
            mScene, SLOT(highlightFloorChanged(bool)));

    QList<QKeySequence> keys = QKeySequence::keyBindings(QKeySequence::ZoomIn);
    keys += QKeySequence(tr("+"));
    ui->actionZoomIn->setShortcuts(keys);
    mView->addAction(ui->actionZoomIn);
    ui->actionZoomIn->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys = QKeySequence::keyBindings(QKeySequence::ZoomOut);
    keys += QKeySequence(tr("-"));
    ui->actionZoomOut->setShortcuts(keys);
    mView->addAction(ui->actionZoomOut);
    ui->actionZoomOut->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    keys.clear();
    keys += QKeySequence(tr("Ctrl+0"));
    keys += QKeySequence(tr("0"));
    ui->actionNormalSize->setShortcuts(keys);
    mView->addAction(ui->actionNormalSize);
    ui->actionNormalSize->setShortcutContext(Qt::WidgetWithChildrenShortcut);

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
    updateActions();
    mScene->setDocument(doc);
}

void BuildingPreviewWindow::clearDocument()
{
    mDocument = 0;
    updateActions();
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
    if (!BuildingTMX::instance()->exportTMX(mDocument->building(),
                                           mScene->mapComposite(),
                                           fileName)) {
        QMessageBox::critical(this, tr("Error Saving Map"),
                              BuildingTMX::instance()->errorString());
    }
    return true;
}

void BuildingPreviewWindow::updateActions()
{
    ui->actionShowWalls->setEnabled(mDocument != 0);
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
    mGridItem(0),
    mLoading(false),
    mDarkRectangle(new QGraphicsRectItem)
{
    setBackgroundBrush(Qt::darkGray);

    mShowWalls = BuildingPreferences::instance()->showWalls();

    mDarkRectangle->setPen(Qt::NoPen);
    mDarkRectangle->setBrush(Qt::black);
    mDarkRectangle->setOpacity(0.6);
    mDarkRectangle->setVisible(false);
    addItem(mDarkRectangle);
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

    BuildingToMap();

    mLoading = true;
    currentFloorChanged();
    mLoading = false;

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());

    connect(mDocument, SIGNAL(currentFloorChanged()),
            SLOT(currentFloorChanged()));

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

    connect(mDocument, SIGNAL(objectAdded(BuildingObject*)),
            SLOT(objectAdded(BuildingObject*)));
    connect(mDocument, SIGNAL(objectRemoved(BuildingObject*)),
            SLOT(objectRemoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectMoved(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));
    connect(mDocument, SIGNAL(objectTileChanged(BuildingObject*)),
            SLOT(objectTileChanged(BuildingObject*)));
    connect(mDocument, SIGNAL(objectChanged(BuildingObject*)),
            SLOT(objectMoved(BuildingObject*)));

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
    if (mMapComposite) {
        delete mMapComposite->mapInfo();
        delete mMapComposite;
        TilesetManager::instance()->removeReferences(mMap->tilesets());
        delete mMap;
        delete mRenderer;
        qDeleteAll(mLayerGroupItems);
        mLayerGroupItems.clear();
        delete mGridItem;
    }

    mMap = new Map(Map::Isometric,
                   mDocument->building()->width(),
                   mDocument->building()->height(),
                   64, 32);

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

    // The order must match the LayerIndexXXX constants.
    const char *layerNames[] = {
        "Floor",
        "Walls",
        "Frames",
        "Doors",
        "Furniture",
        "Furniture2",
        "RoofCap",
        "RoofCap2",
        "Roof",
#ifdef ROOF_TOP
        "RoofTop"
#endif
        0
    };

    int maxLevel =  mDocument->building()->floorCount() - 1;
    int extraForWalls = 1;
    QSize mapSize(mDocument->building()->width() + maxLevel * 3 + extraForWalls,
                  mDocument->building()->height() + maxLevel * 3 + extraForWalls);
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        for (int i = 0; layerNames[i]; i++) {
            QString name = QLatin1String(layerNames[i]);
            QString layerName = tr("%1_%2").arg(floor->level()).arg(name);
            TileLayer *tl = new TileLayer(layerName,
                                          0, 0, mapSize.width(), mapSize.height());
            mMap->addLayer(tl);
        }
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);
    mMapComposite = new MapComposite(mapInfo);

    foreach (CompositeLayerGroup *layerGroup, mMapComposite->layerGroups()) {
        BuildingFloor *floor = mDocument->building()->floor(layerGroup->level());
        floor->LayoutToSquares();
        BuildingFloorToTileLayers(floor, layerGroup->layers());

        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(layerGroup, mRenderer);
        item->setZValue(layerGroup->level());
        item->synchWithTileLayers();
        item->updateBounds();
        addItem(item);
        mLayerGroupItems[layerGroup->level()] = item;
    }

    mGridItem = new PreviewGridItem(mDocument->building(), mRenderer);
    mGridItem->synchWithBuilding();
    addItem(mGridItem);

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());
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
                if ((index == LayerIndexWall) /*&& (mShowWalls || floor != mDocument->currentFloor())*/) {
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
                if (index == LayerIndexFurniture2) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionFurniture2];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionFurniture2])));
                }
                if (index == LayerIndexRoofCap) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionRoofCap];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionRoofCap])));
                }
                if (index == LayerIndexRoofCap2) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionRoofCap2];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionRoofCap2])));
                }
                if (index == LayerIndexRoof) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionRoof];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionRoof])));
                }
#ifdef ROOF_TOPS
                if (index == LayerIndexRoofTop) {
                    BuildingTile *tile = square.mTiles[BuildingFloor::Square::SectionRoofTop];
                    if (tile)
                        tl->setCell(x + offset, y + offset, Cell(tilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + square.mTileOffset[BuildingFloor::Square::SectionRoofTop])));
                }
#endif
            }
        }
        index++;
    }
}

CompositeLayerGroupItem *BuildingPreviewScene::itemForFloor(BuildingFloor *floor)
{
    return mLayerGroupItems[floor->level()];
}

void BuildingPreviewScene::synchWithShowWalls()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        bool visible = mShowWalls || floor->level() < mDocument->currentFloor()->level();
        CompositeLayerGroup *layerGroup = mMapComposite->layerGroupForLevel(floor->level());
        layerGroup->setLayerVisibility(layerGroup->layers()[LayerIndexWall], visible);
        layerGroup->setLayerVisibility(layerGroup->layers()[LayerIndexRoofCap], visible);
        layerGroup->setLayerVisibility(layerGroup->layers()[LayerIndexRoofCap2], visible);
        layerGroup->setLayerVisibility(layerGroup->layers()[LayerIndexRoof], visible);
        itemForFloor(floor)->synchWithTileLayers();
        itemForFloor(floor)->updateBounds();
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
    itemForFloor(floor)->synchWithTileLayers();
    itemForFloor(floor)->updateBounds();

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());
}

void BuildingPreviewScene::currentFloorChanged()
{
    highlightFloorChanged(BuildingPreferences::instance()->highlightFloor());

    if (!mShowWalls || mLoading)
        synchWithShowWalls();
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
    BuildingToMap();
}

void BuildingPreviewScene::floorRemoved(BuildingFloor *floor)
{
    BuildingToMap();
}

void BuildingPreviewScene::objectAdded(BuildingObject *object)
{
    if (mLoading)
        return;

    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingPreviewScene::objectRemoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingPreviewScene::objectMoved(BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    // Stairs affect the floor tiles on the floor above.
    // Roofs sometimes affect the floor tiles on the floor above.
    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
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

void BuildingPreviewScene::showWallsChanged(bool show)
{
    if (show == mShowWalls)
        return;

    mShowWalls = show;

    synchWithShowWalls();

    update();
}

void BuildingPreviewScene::highlightFloorChanged(bool highlight)
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
