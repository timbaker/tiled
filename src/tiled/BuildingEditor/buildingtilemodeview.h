#ifndef BUILDINGTILEMODEVIEW_H
#define BUILDINGTILEMODEVIEW_H

#include <QGraphicsScene>
#include <QGraphicsView>

#include <QMap>

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

class BuildingObject;
class Building;
class BuildingFloor;
class BuildingDocument;
class CompositeLayerGroupItem;
class PreviewGridItem;
class Room;

class BuildingTileModeScene : public QGraphicsScene
{
    Q_OBJECT
public:
    BuildingTileModeScene(QWidget *parent = 0);
    ~BuildingTileModeScene();

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    MapComposite *mapComposite() const
    { return mMapComposite; }

    Tiled::Map *map() const
    { return mMap; }

private:
    void BuildingToMap();
    void BuildingFloorToTileLayers(BuildingFloor *floor, const QVector<Tiled::TileLayer *> &layers);

    CompositeLayerGroupItem *itemForFloor(BuildingFloor *floor);

private slots:
    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    void roomDefinitionChanged();

    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomChanged(Room *room);

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);
    void floorEdited(BuildingFloor *floor);

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

private:
    BuildingDocument *mDocument;
    MapComposite *mMapComposite;
    Tiled::Map *mMap;
    Tiled::MapRenderer *mRenderer;
    PreviewGridItem *mGridItem;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    bool mLoading;
    QGraphicsRectItem *mDarkRectangle;
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

    void hideEvent(QHideEvent *event);

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void setHandScrolling(bool handScrolling);

private slots:
    void adjustScale(qreal scale);

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    bool mHandScrolling;
};

} // namespace BuildingEditor

#endif // BUILDINGTILEMODEVIEW_H
