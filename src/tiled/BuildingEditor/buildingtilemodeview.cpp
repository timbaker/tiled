#include "buildingtilemodeview.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingpreferences.h"
#include "buildingpreviewwindow.h" // for CompositeLayerGroupItem;
#include "buildingtiles.h"

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

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QScrollBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTileModeScene::BuildingTileModeScene(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMapComposite(0),
    mMap(0),
    mRenderer(0),
    mGridItem(0),
    mDarkRectangle(new QGraphicsRectItem)
{
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
}

BuildingTileModeScene::~BuildingTileModeScene()
{
    delete mMapComposite;
    if (mMap)
        TilesetManager::instance()->removeReferences(mMap->tilesets());
    delete mMap;
    delete mRenderer;
}

void BuildingTileModeScene::setDocument(BuildingDocument *doc)
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

void BuildingTileModeScene::clearDocument()
{
    setDocument(0);
}

void BuildingTileModeScene::BuildingToMap()
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

    int maxLevel =  mDocument->building()->floorCount() - 1;
    int extraForWalls = 1;
    QSize mapSize(mDocument->building()->width() + maxLevel * 3 + extraForWalls,
                  mDocument->building()->height() + maxLevel * 3 + extraForWalls);

    mMap = new Map(Map::Isometric,
                   mapSize.width(), mapSize.height(),
                   64, 32);

    // Add tilesets from Tilesets.txt
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets())
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
        "FloorGrime",
        "FloorGrime2",
        "Walls",
        "Walls2",
        "RoofCap",
        "RoofCap2",
        "WallOverlay",
        "WallOverlay2",
        "WallGrime",
        "WallFurniture",
        "Frames",
        "Doors",
        "Curtains",
        "Furniture",
        "Furniture2",
        "Curtains2",
        "Roof",
        "Roof2",
        "RoofTop",
        0
    };
    Q_ASSERT(sizeof(layerNames)/sizeof(layerNames[0]) == BuildingFloor::Square::MaxSection + 1);

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
    mGridItem->setZValue(1000);
    addItem(mGridItem);

    mRenderer->setMaxLevel(mMapComposite->maxLevel());
    setSceneRect(mMapComposite->boundingRect(mRenderer));
    mDarkRectangle->setRect(sceneRect());
}

void BuildingTileModeScene::BuildingFloorToTileLayers(BuildingFloor *floor,
                                                     const QVector<TileLayer *> &layers)
{
    int offset = (mMapComposite->maxLevel() - floor->level()) * 3; // FIXME: not for LevelIsometric

    int section = 0;
    foreach (TileLayer *tl, layers) {
        tl->erase(QRegion(0, 0, tl->width(), tl->height()));
        for (int x = 0; x <= floor->width(); x++) {
            for (int y = 0; y <= floor->height(); y++) {
                const BuildingFloor::Square &square = floor->squares[x][y];
                if (BuildingTile *btile = square.mTiles[section]) {
                    if (!btile->isNone()) {
                        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(btile))
                            tl->setCell(x + offset, y + offset, Cell(tile));
                    }
                    continue;
                }
                if (BuildingTileEntry *entry = square.mEntries[section]) {
                    int tileOffset = square.mEntryEnum[section];
                    if (entry->isNone() || entry->tile(tileOffset)->isNone())
                        continue;
                    if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->tile(tileOffset)))
                        tl->setCell(x + offset, y + offset, Cell(tile));
                }

            }
        }
        section++;
    }
}

CompositeLayerGroupItem *BuildingTileModeScene::itemForFloor(BuildingFloor *floor)
{
    return mLayerGroupItems[floor->level()];
}

void BuildingTileModeScene::floorEdited(BuildingFloor *floor)
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

void BuildingTileModeScene::currentFloorChanged()
{
    highlightFloorChanged(BuildingPreferences::instance()->highlightFloor());
}

void BuildingTileModeScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    Q_UNUSED(pos);
    floorEdited(floor);
}

void BuildingTileModeScene::roomDefinitionChanged()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomAdded(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomRemoved(Room *room)
{
    Q_UNUSED(room)
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        floorEdited(floor);
}

void BuildingTileModeScene::roomChanged(Room *room)
{
    Q_UNUSED(room)
    roomDefinitionChanged();
}

void BuildingTileModeScene::floorAdded(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    BuildingToMap();
}

void BuildingTileModeScene::floorRemoved(BuildingFloor *floor)
{
    Q_UNUSED(floor)
    BuildingToMap();
}

void BuildingTileModeScene::objectAdded(BuildingObject *object)
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

void BuildingTileModeScene::objectRemoved(BuildingObject *object)
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

void BuildingTileModeScene::objectMoved(BuildingObject *object)
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

void BuildingTileModeScene::objectTileChanged(BuildingEditor::BuildingObject *object)
{
    BuildingFloor *floor = object->floor();
    floorEdited(floor);

    if (BuildingFloor *floorAbove = floor->floorAbove()) {
        if (object->affectsFloorAbove())
            floorEdited(floorAbove);
    }
}

void BuildingTileModeScene::buildingResized()
{
    buildingRotated();
}

void BuildingTileModeScene::buildingRotated()
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
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    BuildingToMap();
}

void BuildingTileModeScene::tilesetAboutToBeRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();
}

void BuildingTileModeScene::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
    if (!mDocument)
        return;
    BuildingToMap();
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

    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));
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
