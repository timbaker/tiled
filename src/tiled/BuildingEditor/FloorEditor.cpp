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

#include "FloorEditor.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingtools.h"
#include "buildingeditorwindow.h"

#include <QAction>
#include <QDebug>
#include <QPainter>

using namespace BuildingEditor;

GraphicsFloorItem::GraphicsFloorItem(BuildingFloor *floor) :
    QGraphicsItem(),
    mFloor(floor),
    mBmp(new QImage(mFloor->width(), mFloor->height(), QImage::Format_RGB32))
{
    mBmp->fill(Qt::black);
}

QRectF GraphicsFloorItem::boundingRect() const
{
    return QRectF(0, 0, mFloor->width() * 30, mFloor->height() * 30);
}

void GraphicsFloorItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                              QWidget *)
{
    for (int x = 0; x < mFloor->width(); x++) {
        for (int y = 0; y < mFloor->height(); y++) {
            QRgb c = mBmp->pixel(x, y);
            if (c == qRgb(0, 0, 0))
                continue;
            painter->fillRect(x * 30, y * 30, 30, 30, c);
        }
    }
}

/////

GraphicsGridItem::GraphicsGridItem(int width, int height) :
    QGraphicsItem(),
    mWidth(width),
    mHeight(height)
{
}

QRectF GraphicsGridItem::boundingRect() const
{
    return QRectF(0, 0, mWidth * 30, mHeight * 30);
}

void GraphicsGridItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *,
                             QWidget *)
{
    QPen pen(QColor(128, 128, 220, 80));
    pen.setWidth(2);
    pen.setStyle(Qt::DotLine);
    painter->setPen(pen);

    for (int x = 0; x <= mWidth; x++)
        painter->drawLine(x * 30, 0, x * 30, mHeight * 30);

    for (int y = 0; y <= mHeight; y++)
        painter->drawLine(0, y * 30, mWidth * 30, y * 30);
}

/////

FloorEditor::FloorEditor(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mCurrentTool(0)
{
    setBackgroundBrush(Qt::black);
}

void FloorEditor::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mousePressEvent(event);
}

void FloorEditor::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseMoveEvent(event);
}

void FloorEditor::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (mCurrentTool)
        mCurrentTool->mouseReleaseEvent(event);
}

void FloorEditor::UpdateMetaBuilding()
{
#if 0
    QString wallName = RoomDefinitionManager::instance->ExteriorWall;
    WallType *exteriorWall = WallTypes::instance->getEWallFromName(wallName);

    QList<Layout*> layouts;
    foreach (EditorFloor *fl, floors) {
        fl->UpdateLayout(exteriorWall);
        layouts += fl->layout;
    }

    building()->recreate(layouts, exteriorWall);

    if (Form1.instance.preview == null)
        return;

    Form1.instance.preview.panel1.building = building;
    Form1.instance.preview.panel1.Invalidate();
    Invalidate();
#endif
}

void FloorEditor::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    if (mDocument) {
        connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
                SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));
    }
}

Building *FloorEditor::building() const
{
    return mDocument ? mDocument->building() : 0;
}

void FloorEditor::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
    }

    mCurrentTool = tool;

    if (mCurrentTool)
        mCurrentTool->action()->setChecked(true);
}

QPoint FloorEditor::sceneToTile(const QPointF &scenePos)
{
    return QPoint(scenePos.x() / 30, scenePos.y() / 30);
}

QRectF FloorEditor::tileToSceneRect(const QPoint &tilePos)
{
    return QRectF(tilePos.x() * 30, tilePos.y() * 30, 30, 30);
}

bool FloorEditor::currentFloorContains(const QPoint &tilePos)
{
    int x = tilePos.x(), y = tilePos.y();
    if (x < 0 || y < 0 || x >= currentFloor->width() || y >= currentFloor->height())
        return false;
    return true;
}

void FloorEditor::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    int index = floor->building()->floors().indexOf(floor);
    Room *room = floor->layout()->roomAt(pos);
    qDebug() << floor << pos << room;
    mFloorItems[index]->bmp()->setPixel(pos, room ? room->Color : qRgb(0, 0, 0));
    mFloorItems[index]->update();
}

/////
