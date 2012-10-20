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
#include "buildingeditorwindow.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
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
    mRenderer->drawGrid(p, option->exposedRect, Qt::black, mLayerGroup->level());
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

BuildingPreviewScene::BuildingPreviewScene(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMapComposite(0),
    mMap(0),
    mRenderer(0)
{
    setBackgroundBrush(Qt::darkGray);
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

        mMapComposite = 0;
        mMap = 0;
        mRenderer = 0;
    }

    if (!mDocument)
        return;

    mMap = new Map(Map::Isometric, doc->building()->width(),
                   doc->building()->height(), 64, 32);

    // Add tilesets from MapBaseXMLLots.txt
    foreach (Tileset *ts, mTilesetByName)
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

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);

    mMapComposite = new MapComposite(mapInfo);

    foreach (BuildingFloor *floor, doc->building()->floors()) {
        floorAdded(floor);
    }

    connect(mDocument, SIGNAL(currentFloorChanged()),
            SLOT(currentFloorChanged()));
    connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
            SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));
    connect(mDocument, SIGNAL(roomDefinitionChanged()),
            SLOT(roomDefinitionChanged()));
    connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(floorAdded(BuildingFloor*)));
    connect(mDocument, SIGNAL(objectAdded(BaseMapObject*)),
            SLOT(objectAdded(BaseMapObject*)));
    connect(mDocument, SIGNAL(objectRemoved(BuildingFloor*,int)),
            SLOT(objectRemoved(BuildingFloor*,int)));
    connect(mDocument, SIGNAL(objectMoved(BaseMapObject*)),
            SLOT(objectMoved(BaseMapObject*)));
    connect(mDocument, SIGNAL(objectTileChanged(BaseMapObject*)),
            SLOT(objectTileChanged(BaseMapObject*)));
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
    int index = 0;
    foreach (TileLayer *tl, layers) {
        tl->erase(QRegion(QRect(0, 0, tl->width(), tl->height())));
        for (int x = 0; x < floor->width(); x++) {
            for (int y = 0; y < floor->height(); y++) {
                if (index == LayerIndexFloor) {
                    bool bAboveStep = floor->level() ? floor->floorBelow()->IsTopStairAt(x, y) : false;
                    if (!bAboveStep) {
                        BuildingTile *tile = floor->squares[x][y].mTiles[BuildingFloor::Square::SectionFloor];
                        if (tile)
                            tl->setCell(x, y, Cell(mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex)));
                    }
                }
                if (index == LayerIndexWall) {
                    BuildingTile *tile = floor->squares[x][y].mTiles[BuildingFloor::Square::SectionWall];
                    if (tile)
                        tl->setCell(x, y, Cell(mTilesetByName[tile->mTilesetName]->tileAt(floor->squares[x][y].getTileIndexForWall())));
                }
                if (index == LayerIndexDoor) {
                    BuildingTile *tile = floor->squares[x][y].mTiles[BuildingFloor::Square::SectionDoor];
                    if (tile)
                        tl->setCell(x, y, Cell(mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex)));
                }
                if (index == LayerIndexFrame) {
                    BuildingTile *tile = floor->squares[x][y].mTiles[BuildingFloor::Square::SectionFrame];
                    if (tile)
                        tl->setCell(x, y, Cell(mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex)));
                }
                if (index == LayerIndexFurniture) {
                    BuildingTile *tile = floor->squares[x][y].mTiles[BuildingFloor::Square::SectionFurniture];
                    if (tile)
                        tl->setCell(x, y, Cell(mTilesetByName[tile->mTilesetName]->tileAt(tile->mIndex + floor->squares[x][y].mTileOffset[BuildingFloor::Square::SectionFurniture])));
                }
            }
        }
        index++;
    }
}

void BuildingPreviewScene::floorEdited(BuildingFloor *floor)
{
    floor->LayoutToSquares();
    BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    mLayerGroupItems[floor->level()]->synchWithTileLayers();
    mLayerGroupItems[floor->level()]->updateBounds();
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

void BuildingPreviewScene::floorAdded(BuildingFloor *floor)
{
    int x = floor->level() * -3, y = floor->level() * -3; // FIXME: not for LevelIsometric

    TileLayer *tl = new TileLayer(tr("%1_Floor").arg(floor->level()), x, y,
                                  floor->width(), floor->height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Walls").arg(floor->level()), x, y,
                       floor->width(), floor->height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Frames").arg(floor->level()), x, y,
                       floor->width(), floor->height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Doors").arg(floor->level()), x, y,
                       floor->width(), floor->height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    tl = new TileLayer(tr("%1_Furniture").arg(floor->level()), x, y,
                       floor->width(), floor->height());
    mMap->addLayer(tl);
    mMapComposite->layerAdded(mMap->layerCount() - 1);

    CompositeLayerGroup *lg = mMapComposite->tileLayersForLevel(floor->level());
    CompositeLayerGroupItem *item = new CompositeLayerGroupItem(lg, mRenderer);
    mLayerGroupItems[lg->level()] = item;
    item->synchWithTileLayers();
    addItem(item);
}

void BuildingPreviewScene::objectAdded(BaseMapObject *object)
{
    floorEdited(object->floor());
}

void BuildingPreviewScene::objectRemoved(BuildingFloor *floor, int index)
{
    Q_UNUSED(index)
    floorEdited(floor);
}

void BuildingPreviewScene::objectMoved(BaseMapObject *object)
{
    floorEdited(object->floor());
}

void BuildingPreviewScene::objectTileChanged(BuildingEditor::BaseMapObject *object)
{
    floorEdited(object->floor());
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
