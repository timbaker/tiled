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
class BuildingMap;
class BuildingObject;
class Building;
class BuildingFloor;
class BuildingDocument;
class CompositeLayerGroupItem;
class FloorTileGrid;
class Room;

class BuildingTileModeScene;

class TileModeGridItem : public QGraphicsItem
{
public:
    TileModeGridItem(BuildingDocument *doc, Tiled::MapRenderer *renderer);

    void synchWithBuilding();

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

private:
    BuildingDocument *mDocument;
    Tiled::MapRenderer *mRenderer;
    QRect mTileBounds;
    QRectF mBoundingRect;
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

    Tiled::MapRenderer *mMapRenderer;
};

class BuildingTileModeScene : public BaseFloorEditor
{
    Q_OBJECT
public:
    BuildingTileModeScene(QWidget *parent = 0);
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

private:
    void BuildingToMap();
    CompositeLayerGroupItem *itemForFloor(BuildingFloor *floor);

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

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void currentToolChanged(BaseTileTool *tool);
    void currentToolChanged(BaseTool *tool);

    // BuildingMap signals
    void aboutToRecreateLayers();
    void layersRecreated();
    void layersUpdated(int level);

private:
    BuildingMap *mBuildingMap;
    TileModeGridItem *mGridItem;
    TileModeSelectionItem *mTileSelectionItem;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    bool mLoading;
    QGraphicsRectItem *mDarkRectangle;
    BaseTileTool *mCurrentTileTool;
    BaseTool *mCurrentTool;
    CompositeLayerGroup *mLayerGroupWithToolTiles;
    QString mNonEmptyLayer;
    CompositeLayerGroupItem *mNonEmptyLayerGroupItem;
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

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    QPoint mLastMouseTilePos;
    bool mHandScrolling;
};

} // namespace BuildingEditor

#endif // BUILDINGTILEMODEVIEW_H
