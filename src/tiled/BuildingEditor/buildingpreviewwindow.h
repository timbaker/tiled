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

#ifndef BUILDINGPREVIEWWINDOW_H
#define BUILDINGPREVIEWWINDOW_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QSettings>

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

namespace Ui {
class BuildingPreviewWindow;
}

namespace BuildingEditor {

class BuildingObject;
class Building;
class BuildingFloor;
class BuildingDocument;
class Door;
class Room;

class CompositeLayerGroupItem : public QGraphicsItem
{
public:
    CompositeLayerGroupItem(CompositeLayerGroup *layerGroup,
                            Tiled::MapRenderer *renderer,
                            QGraphicsItem *parent = 0);

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

class PreviewGridItem : public QGraphicsItem
{
public:
    PreviewGridItem(Building *building, Tiled::MapRenderer *renderer);

    void synchWithBuilding();

    QRectF boundingRect() const;
    void paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *);

private:
    Building *mBuilding;
    Tiled::MapRenderer *mRenderer;
    QRect mTileBounds;
    QRectF mBoundingRect;
};

class BuildingPreviewScene : public QGraphicsScene
{
    Q_OBJECT
public:
    BuildingPreviewScene(QWidget *parent = 0);
    ~BuildingPreviewScene();

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    MapComposite *mapComposite() const
    { return mMapComposite; }

    Tiled::Map *map() const
    { return mMap; }

    QString errorString() const
    { return mError; }

    enum LayerType
    {
        LayerIndexFloor,
        LayerIndexWall,
        LayerIndexFrame,
        LayerIndexDoor,
        LayerIndexFurniture,
        LayerIndexFurniture2,
        LayerIndexRoof
    };

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
    void floorEdited(BuildingFloor *floor);

    void objectAdded(BuildingObject *object);
    void objectRemoved(BuildingFloor *floor, int index);
    void objectMoved(BuildingObject *object);
    void objectTileChanged(BuildingObject *object);

    void buildingResized();
    void buildingRotated();

    void showWalls(bool show);

private:
    BuildingDocument *mDocument;
    MapComposite *mMapComposite;
    Tiled::Map *mMap;
    Tiled::MapRenderer *mRenderer;
    PreviewGridItem *mGridItem;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    QString mError;
    bool mShowWalls;
    bool mLoading;
};

class BuildingPreviewView : public QGraphicsView
{
    Q_OBJECT
public:
    BuildingPreviewView(QWidget *parent = 0);

    BuildingPreviewScene *scene() const
    { return dynamic_cast<BuildingPreviewScene*>(QGraphicsView::scene()); }

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void mouseMoveEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

private slots:
    void adjustScale(qreal scale);

private:
    Tiled::Internal::Zoomable *mZoomable;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
};

class BuildingPreviewWindow : public QMainWindow
{
    Q_OBJECT
public:
    BuildingPreviewWindow(QWidget *parent = 0);

    void closeEvent(QCloseEvent *event);

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    BuildingPreviewScene *scene() const
    { return mScene; }

    void readSettings();
    void writeSettings();

    bool exportTMX(const QString &fileName);

private slots:
    void updateActions();

private:
    Ui::BuildingPreviewWindow *ui;
    BuildingDocument *mDocument;
    BuildingPreviewScene *mScene;
    BuildingPreviewView *mView;
    QSettings mSettings;
};

} // namespace BuildingEditor

#endif // BUILDINGPREVIEWWINDOW_H
