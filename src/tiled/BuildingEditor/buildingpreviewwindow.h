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

class BaseMapObject;
class BuildingFloor;
class BuildingDocument;

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

class BuildingPreviewScene : public QGraphicsScene
{
    Q_OBJECT
public:
    BuildingPreviewScene(QWidget *parent = 0);
    ~BuildingPreviewScene();

    void setDocument(BuildingDocument *doc);

    void setTilesets(const QMap<QString,Tiled::Tileset*> &tilesets)
    { mTilesetByName = tilesets; }


    QString errorString() const
    { return mError; }

    enum LayerType
    {
        Floor,
        Wall,
        Frame,
        Door,
        Furniture
    };

private:
    void BuildingToMap();
    void BuildingFloorToTileLayers(BuildingFloor *floor, const QVector<Tiled::TileLayer *> &layers);
    void floorEdited(BuildingFloor *floor);

private slots:
    void currentFloorChanged();
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);
    void roomDefinitionChanged();
    void floorAdded(BuildingFloor *floor);
    void objectAdded(BaseMapObject *object);
    void objectRemoved(BuildingFloor *floor, int index);
    void objectMoved(BaseMapObject *object);

private:
    BuildingDocument *mDocument;
    MapComposite *mMapComposite;
    Tiled::Map *mMap;
    Tiled::MapRenderer *mRenderer;
    QMap<int,CompositeLayerGroupItem*> mLayerGroupItems;
    QMap<QString,Tiled::Tileset*> mTilesetByName;
    QString mError;
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

    void setDocument(BuildingDocument *doc);

    BuildingPreviewScene *scene() const
    { return mScene; }

private:
    Ui::BuildingPreviewWindow *ui;
    BuildingDocument *mDocument;
    BuildingPreviewScene *mScene;
};

} // namespace BuildingEditor

#endif // BUILDINGPREVIEWWINDOW_H
