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

#ifndef BUILDINGTILEMODEVIEW_H
#define BUILDINGTILEMODEVIEW_H

#include "FloorEditor.h"

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

class BaseTileTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingMap;
class BuildingObject;
class BuildingPreferences;
class CompositeLayerGroupItem;
class FloorTileGrid;
class Room;

class BuildingTileModeScene;

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
    TileModeSelectionItem(BuildingTileModeScene *scene);

    QRectF boundingRect() const;

    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *widget);

    BuildingDocument *document() const;

private slots:
    void tileSelectionChanged(const QRegion &oldSelection);
    void currentLevelChanged();

private:
    void updateBoundingRect();

private:
    BuildingTileModeScene *mScene;
    QRectF mBoundingRect;
};

class IsoBuildingRenderer : public BuildingRenderer
{
public:
    QPoint sceneToTile(const QPointF &scenePos, int level);
    QPointF sceneToTileF(const QPointF &scenePos, int level);
    QRect sceneToTileRect(const QRectF &sceneRect, int level);
    QRectF sceneToTileRectF(const QRectF &sceneRect, int level);
    QPointF tileToScene(const QPoint &tilePos, int level);
    QPointF tileToSceneF(const QPointF &tilePos, int level);
    QPolygonF tileToScenePolygon(const QPoint &tilePos, int level);
    QPolygonF tileToScenePolygon(const QRect &tileRect, int level);
    QPolygonF tileToScenePolygonF(const QRectF &tileRect, int level);
    QPolygonF tileToScenePolygon(const QPolygonF &tilePolygon, int level);

    void drawLine(QPainter *painter, qreal x1, qreal y1, qreal x2, qreal y2, int level);

    IsoBuildingRenderer *asIso() { return this; }

    Tiled::MapRenderer *mMapRenderer;
};

class BuildingTileModeScene : public BaseFloorEditor
{
    Q_OBJECT
public:
    BuildingTileModeScene(QObject *parent = 0);
    ~BuildingTileModeScene();

    Tiled::MapRenderer *mapRenderer() const;

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

    void drawTileSelection(QPainter *painter, const QRegion &region,
                           const QColor &color, const QRectF &exposed,
                           int level = 0) const;

    void setCursorObject(BuildingObject *object);
    void dragObject(BuildingFloor *floor, BuildingObject *object, const QPoint &offset);
    void resetDrag(BuildingFloor *floor, BuildingObject *object);
    void changeFloorGrid(BuildingFloor *floor, const QVector<QVector<Room*> > &grid);
    void resetFloorGrid(BuildingFloor *floor);

    bool shouldShowFloorItem(BuildingFloor *floor) const;
    bool shouldShowObjectItem(BuildingObject *object) const;

    void setShowBuildingTiles(bool show);
    void setShowUserTiles(bool show);
    void setEditingTiles(bool editing);

    void setCursorPosition(const QPoint &pos);

private:
    void BuildingToMap();
    CompositeLayerGroupItem *itemForFloor(BuildingFloor *floor);

    BuildingPreferences *prefs() const;

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

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

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
    QString mNonEmptyLayer;
    CompositeLayerGroupItem *mNonEmptyLayerGroupItem;
    bool mShowBuildingTiles;
    bool mShowUserTiles;
    int mCurrentLevel;
    QPoint mHighlightRoomPos;
};

class BuildingTileModeView : public QGraphicsView
{
    Q_OBJECT
public:
    BuildingTileModeView(QWidget *parent = 0);
    ~BuildingTileModeView();

    BuildingTileModeScene *scene() const
    { return dynamic_cast<BuildingTileModeScene*>(QGraphicsView::scene()); }

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    bool event(QEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

    void hideEvent(QHideEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

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

#endif // BUILDINGTILEMODEVIEW_H
