/*
 * ZomboidScene.h
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

#ifndef ZOMBOIDSCENE_H
#define ZOMBOIDSCENE_H

#include "mapscene.h"
#include "zlotmanager.h"

#include <QGraphicsItem>
#include <QMap>

class CompositeLayerGroup;
class DnDItem;
class MapBuildings;

namespace Tiled {
class MapRenderer;
}

class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CompositeLayerGroup *layerGroup, Tiled::MapRenderer *renderer, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

    void synchWithTileLayers();
    void updateBounds();

    CompositeLayerGroup *layerGroup() const { return mLayerGroup; }

private:
    CompositeLayerGroup *mLayerGroup;
    Tiled::MapRenderer *mRenderer;
    QRectF mBoundingRect;
};

namespace Tiled {

class Layer;
class MapObject;
class TileLayer;
class Tileset;

namespace Internal {

///// ///// ///// ///// /////
/**
 * A graphics scene that represents the contents of a map.
 */
class ZomboidScene : public MapScene
{
    Q_OBJECT

public:
    ZomboidScene(QObject *parent);
    ~ZomboidScene();

    // MapScene
    virtual void setMapDocument(MapDocument *mapDoc);

    ZLotManager &lotManager() { return mLotManager; }

private slots:
    virtual void refreshScene();

    virtual void regionAltered(const QRegion &region, Layer *layer);

    virtual void mapChanged();

    void layerAdded(int index);
    void layerRemoved(int index);
    void layerChanged(int index);

    void layerGroupAdded(int level);
    void layerGroupVisibilityChanged(CompositeLayerGroup *g);

    void layerAddedToGroup(int index);
    void layerRemovedFromGroup(int index, CompositeLayerGroup *oldGroup);

    void layerLevelChanged(int index, int oldLevel);

    void onLotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void onLotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void onLotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void onLotUpdated(MapComposite *mc, WorldCellLot *lot);

    void invalidateMapBuildings();

    void mapCompositeChanged();

    void bmpBlenderLayersRecreated();
    void bmpPainted(int bmpIndex, const QRegion &region);
    void bmpXXXChanged();

    void highlightRoomUnderPointerChanged(bool highlight);

    void handlePendingUpdates();

public:
    enum Pending {
        None = 0,
        AllGroups = 0x01,
        Bounds = 0x02,
        Synch = 0x04,
        ZOrder = 0x08,
        Paint = 0x10,
        Highlight = 0x20
    };
    Q_DECLARE_FLAGS(PendingFlags, Pending)

protected:

    // QGraphicsScene
    void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragMoveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);

    // MapScene
    virtual QGraphicsItem *createLayerItem(Layer *layer);
    virtual void updateCurrentLayerHighlight();

    void updateLayerGroupsLater(PendingFlags flags);
    void updateLayerGroupLater(int level, PendingFlags flags);

    void setGraphicsSceneZOrder();

    QRegion getBuildingRegion(const QPoint &tilePos, QRegion &roomRgn);
    void setHighlightRoomPosition(const QPoint &tilePos);

private:
    QMap<MapObject*,MapComposite*> mMapObjectToLot;

    void doLater(PendingFlags flags);
    PendingFlags mPendingFlags;
    bool mPendingActive;
    QList<CompositeLayerGroupItem*> mPendingGroupItems;

    QMap<int,CompositeLayerGroupItem*> mTileLayerGroupItems;
    ZLotManager mLotManager;
    DnDItem *mDnDItem;
    bool mWasHighlightCurrentLayer;
    QGraphicsPolygonItem *mMapBordersItem;
    QGraphicsPolygonItem *mMapBordersItem2;

    QPoint mHighlightRoomPosition;
    MapBuildings *mMapBuildings;
    bool mMapBuildingsInvalid;
};

} // namespace Internal
} // namespace Tiled

Q_DECLARE_OPERATORS_FOR_FLAGS(Tiled::Internal::ZomboidScene::PendingFlags)

#endif // ZOMBOIDSCENE_H
