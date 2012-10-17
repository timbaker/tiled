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

#ifndef FLOOREDITOR_H
#define FLOOREDITOR_H

#include <QGraphicsItem>
#include <QGraphicsScene>

namespace BuildingEditor {

class BaseMapObject;
class BaseTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class Layout;
class WallType;

/////

class GraphicsFloorItem : public QGraphicsItem
{
public:
    GraphicsFloorItem(BuildingFloor *floor);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    QImage *bmp() const
    { return mBmp; }

private:
    BuildingFloor *mFloor;
    QImage *mBmp;
};

class GraphicsGridItem : public QGraphicsItem
{
public:
    GraphicsGridItem(int width, int height);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSize(int width, int height);

private:
    int mWidth, mHeight;
};

class FloorEditor : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit FloorEditor(QWidget *parent = 0);

    BuildingFloor *currentFloor;
    int Floor;

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void UpdateMetaBuilding();

    void setDocument(BuildingDocument *doc);

    BuildingDocument *document() const
    { return mDocument; }

    Building *building() const;

    void UpFloor();
    void DownFloor();

#if 0
    int posY, posX;
    int wallSnapX, wallSnapY;
    bool vertWall;
    BaseMapObject::Direction dir;
#endif

    void ProcessMove(int x, int y);

    void activateTool(BaseTool *tool);
    QPoint sceneToTile(const QPointF &scenePos);
    QRectF tileToSceneRect(const QPoint &tilePos);
    bool currentFloorContains(const QPoint &tilePos);

private slots:
    void roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos);

public:
    QList<GraphicsFloorItem*> mFloorItems;

private:
    BuildingDocument *mDocument;
    BaseTool *mCurrentTool;
};

} // namespace BuildingEditor

#endif // FLOOREDITOR_H
