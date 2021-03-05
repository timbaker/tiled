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

#ifndef BUILDINGISOVIEW_H
#define BUILDINGISOVIEW_H

#include "buildingorthoview.h"

#include "tilelayer.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <QMap>

class CompositeLayerGroup;
class MapComposite;

namespace Tiled {
class Map;
class MapRenderer;
class TileLayer;
class Tileset;

namespace Internal {
class Zoomable;
}

} // namespace Tiled

namespace BuildingEditor {

class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingMap;
class BuildingObject;
class BuildingPreferences;
class CompositeLayerGroupItem;
class FloorTileGrid;
class Room;

class BuildingIsoScene;

class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CompositeLayerGroup *layerGroup,
                            Tiled::MapRenderer *renderer,
                            QGraphicsItem *parent = nullptr);

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

class TileModeGridItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    TileModeGridItem(BuildingDocument *doc, Tiled::MapRenderer *renderer);

    void synchWithBuilding();

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

    void setEditingTiles(bool editing);

private slots:
    void gridColorChanged() { update(); }
    void showGridChanged(bool show);

private:
    BuildingDocument *mDocument;
    Tiled::MapRenderer *mRenderer;
    QRect mTileBounds;
    QRectF mBoundingRect;
    bool mEditingTiles;
};

class TileModeSelectionItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    TileModeSelectionItem(BuildingIsoScene *scene);

    QRectF boundingRect() const;

    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget);

    BuildingDocument *document() const;

private slots:
    void tileSelectionChanged(const QRegion &oldSelection);
    void currentLevelChanged();

private:
    void updateBoundingRect();

private:
    BuildingIsoScene *mScene;
    QRectF mBoundingRect;
};

class IsoBuildingRenderer : public BuildingRenderer
{
public:
    QPoint sceneToTile(const QPointF &scenePos, int level) override;
    QPointF sceneToTileF(const QPointF &scenePos, int level) override;
    QRect sceneToTileRect(const QRectF &sceneRect, int level) override;
    QRectF sceneToTileRectF(const QRectF &sceneRect, int level) override;
    QPointF tileToScene(const QPoint &tilePos, int level) override;
    QPointF tileToSceneF(const QPointF &tilePos, int level) override;
    QPolygonF tileToScenePolygon(const QPoint &tilePos, int level) override;
    QPolygonF tileToScenePolygon(const QRect &tileRect, int level) override;
    QPolygonF tileToScenePolygonF(const QRectF &tileRect, int level) override;
    QPolygonF tileToScenePolygon(const QPolygonF &tilePolygon, int level) override;

    void drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level) override;

    IsoBuildingRenderer *asIso() override { return this; }

    void setRotation(Tiled::MapRotation rotation) override;
    Tiled::MapRotation getRotation() const override;

    Tiled::MapRenderer *mMapRenderer;
};

class BuildingIsoScene : public BuildingBaseScene
{
    Q_OBJECT
public:
    BuildingIsoScene(QObject *parent = nullptr);
    ~BuildingIsoScene();

    Tiled::MapRenderer *mapRenderer() const;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
    { mousePressEvent(event); }

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    QStringList layerNames() const;

    bool currentFloorContains(const QPoint &tilePos, int dw = 0, int dh = 0);

    void setToolTiles(const FloorTileGrid *tiles,
                      const QPoint &pos, const QString &layerName);
    void clearToolTiles();

    QString buildingTileAt(int x, int y);
    QString tileUnderPoint(int x, int y);

    void drawTileSelection(QPainter *painter, const QRegion &region,
                           const QColor &color, const QRectF &exposed,
                           int level = 0) const;

    void setHighlightRoomLock(bool lock)
    { mHighlightRoomLock = lock; }

    void setCursorObject(BuildingObject *object);
    void dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset);
    void resetDrag(BuildingFloor *floor, BuildingObject *object);
    void changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room*> > &grid);
    void resetFloorGrid(BuildingFloor *floor);
    void changeUserTiles(BuildingFloor *floor, const QMap<QString,FloorTileGrid*> &tiles);
    void resetUserTiles(BuildingFloor *floor);

    bool shouldShowFloorItem(BuildingFloor *floor) const;
    bool shouldShowObjectItem(BuildingObject *object) const;

    void setShowBuildingTiles(bool show);
    void setShowUserTiles(bool show);
    void setEditingTiles(bool editing);
    bool editingTiles() const { return mEditingTiles; }

    void setCursorPosition(const QPoint &pos);

protected:
    void setRotationDerive()/* override*/;

private:
    void BuildingToMap();
    CompositeLayerGroupItem *itemForFloor(BuildingFloor *floor);

    BuildingPreferences *prefs() const;

    typedef Tiled::Tileset Tileset;
private slots:
    void currentFloorChanged();
    void currentLayerChanged();

    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    void roomDefinitionChanged();

    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomChanged(Room *room);

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

    void floorTilesChanged(BuildingFloor *floor);
    void floorTilesChanged(BuildingFloor *floor, const QString &layerName,
                           const QRect &bounds);

    void layerOpacityChanged(BuildingFloor *floor, const QString &layerName);
    void layerVisibilityChanged(BuildingFloor *floor, const QString &layerName);

    void objectAdded(BuildingObject *object);
    void objectAboutToBeRemoved(BuildingObject *object);
    void objectRemoved(BuildingObject *object);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);

    void buildingResized();
    void buildingRotated();

    void highlightFloorChanged(bool highlight);
    void highlightRoomChanged(bool highlight);
    void showLowerFloorsChanged(bool show);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void tilesetChanged(Tileset *tileset);

    void currentToolChanged(BaseTool *tool);

    // BuildingMap signals
    void aboutToRecreateLayers();
    void layersRecreated();
    void mapResized();
    void layersUpdated(int level, const QRegion &rgn);

private:
    BuildingMap *mBuildingMap;
    TileModeGridItem *mGridItem;
    TileModeSelectionItem *mTileSelectionItem;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    bool mLoading;
    QGraphicsRectItem *mDarkRectangle;
    BaseTool *mCurrentTool;
    CompositeLayerGroup *mLayerGroupWithToolTiles;
    Tiled::TileLayer mToolTiles;
    QString mNonEmptyLayer;
    CompositeLayerGroupItem *mNonEmptyLayerGroupItem;
    bool mShowBuildingTiles;
    bool mShowUserTiles;
    int mCurrentLevel;
    QPoint mHighlightRoomPos;
    bool mHighlightRoomLock;
};

class BuildingIsoView : public QGraphicsView
{
    Q_OBJECT
public:
    BuildingIsoView(QWidget *parent = nullptr);
    ~BuildingIsoView();

    BuildingIsoScene *scene() const
    { return dynamic_cast<BuildingIsoScene*>(QGraphicsView::scene()); }

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    bool event(QEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

    void hideEvent(QHideEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    void setHandScrolling(bool handScrolling);

signals:
    void mouseCoordinateChanged(const QPoint &tilePos);

private slots:
    void adjustScale(qreal scale);
    void setUseOpenGL(bool useOpenGL);

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    QPoint mLastMouseTilePos;
    bool mHandScrolling;
};

} // namespace BuildingEditor

#endif // BUILDINGISOVIEW_H
