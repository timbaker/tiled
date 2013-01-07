#ifndef BUILDINGTILEMODEVIEW_H
#define BUILDINGTILEMODEVIEW_H

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

class BuildingTileModeScene : public QGraphicsScene
{
    Q_OBJECT
public:
    static const int ZVALUE_CURSOR;
    static const int ZVALUE_GRID;

    BuildingTileModeScene(QWidget *parent = 0);
    ~BuildingTileModeScene();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    BuildingDocument *document() const
    { return mDocument; }

    MapComposite *mapComposite() const
    { return mMapComposite; }

    Tiled::Map *map() const
    { return mMap; }

    Tiled::MapRenderer *renderer() const
    { return mRenderer; }

    int currentLevel();
    BuildingFloor *currentFloor();

    QStringList layerNames() const;
    QString currentLayerName() const;

    QPoint sceneToTile(const QPointF &scenePos);
    QPointF sceneToTileF(const QPointF &scenePos);
    QRect sceneToTileRect(const QRectF &sceneRect);
    QPointF tileToScene(const QPoint &tilePos);
    QPolygonF tileToScenePolygon(const QPoint &tilePos);
    QPolygonF tileToScenePolygon(const QRect &tileRect);
    QPolygonF tileToScenePolygonF(const QRectF &tileRect);
    bool currentFloorContains(const QPoint &tilePos, int dw = 0, int dh = 0);

    void setToolTiles(const FloorTileGrid *tiles,
                      const QPoint &pos, const QString &layerName);
    void clearToolTiles();

    QString buildingTileAt(int x, int y);

private:
    void BuildingToMap();
    void BuildingFloorToTileLayers(BuildingFloor *floor, const QVector<Tiled::TileLayer *> &layers);

    void floorTilesToLayer(BuildingFloor *floor, const QString &layerName,
                           const QRect &bounds);

    CompositeLayerGroupItem *itemForFloor(BuildingFloor *floor);

signals:
    void documentChanged();

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

private:
    BuildingDocument *mDocument;
    MapComposite *mBlendMapComposite;
    Tiled::Map *mBlendMap;
    MapComposite *mMapComposite;
    Tiled::Map *mMap;
    Tiled::MapRenderer *mRenderer;
    TileModeGridItem *mGridItem;
    TileModeSelectionItem *mTileSelectionItem;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    bool mLoading;
    QGraphicsRectItem *mDarkRectangle;
    BaseTileTool *mCurrentTool;
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
