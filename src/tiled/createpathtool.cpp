/*
 * createpathtool.cpp
 * Copyright 2010-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2012, Tim Baker
 *
 * This file is part of Tiled.
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

#include "createpathtool.h"

#include "addremovepath.h"
#include "map.h"
#include "mapdocument.h"
#include "pathitem.h"
#include "pathlayer.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "preferences.h"
#include "utils.h"

#include <QApplication>
#include <QPalette>

using namespace Tiled;
using namespace Tiled::Internal;

CreatePathTool::CreatePathTool(CreationMode mode, QObject *parent)
    : AbstractPathTool(QString(),
          QIcon(QLatin1String(":images/24x24/insert-object.png")),
          QKeySequence(tr("O")),
          parent)
    , mNewPathItem(0)
    , mOverlayPathLayer(0)
    , mOverlayPath(0)
    , mOverlayItem(0)
    , mMode(mode)
{    
    mOverlayPath = new Path;

    switch (mMode) {
    case CreateArea:
        mOverlayPath->setClosed(true);
        Utils::setThemeIcon(this, "insert-object");
        break;

    case CreatePolygon:
        mOverlayPath->setClosed(true);
        setIcon(QIcon(QLatin1String(":images/24x24/insert-polygon.png")));
        break;

    case CreatePolyline: {
        setIcon(QIcon(QLatin1String(":images/24x24/insert-polyline.png")));
        break;
    }
    }

    mOverlayPathLayer = new PathLayer;
    mOverlayPathLayer->addPath(mOverlayPath);

    QColor highlight = QApplication::palette().highlight().color();
    mOverlayPathLayer->setColor(highlight);

    languageChanged();
}

CreatePathTool::~CreatePathTool()
{
    delete mOverlayPathLayer;
}

void CreatePathTool::deactivate(MapScene *scene)
{
    if (mNewPathItem)
        cancelNewPath();

    AbstractPathTool::deactivate(scene);
}

void CreatePathTool::mouseEntered()
{
}

void CreatePathTool::mouseMoved(const QPointF &pos,
                                Qt::KeyboardModifiers modifiers)
{
    AbstractPathTool::mouseMoved(pos, modifiers);

    if (!mNewPathItem)
        return;

    const MapRenderer *renderer = mapDocument()->renderer();

    bool snapToGrid = Preferences::instance()->snapToGrid();
    if (modifiers & Qt::ControlModifier)
        snapToGrid = !snapToGrid;

    switch (mMode) {
    case CreateArea: {
        QPoint tileCoords = renderer->pixelToTileCoordsInt(pos,
                                                           currentPathLayer()->level());

        // Update the size of the new path
        const PathPoint topLeft = mNewPathItem->path()->points().first();
        QSize newSize(qMax(2, tileCoords.x() - topLeft.x() + 1),
                      qMax(2, tileCoords.y() - topLeft.y() + 1));

//        if (snapToGrid)
//            newSize = newSize.toSize();

        PathPoints points;
        points += topLeft;
        points += PathPoint(topLeft.x() + newSize.width() - 1,
                            topLeft.y());
        points += PathPoint(topLeft.x() + newSize.width() - 1,
                            topLeft.y() + newSize.height() - 1);
        points += PathPoint(topLeft.x(),
                            topLeft.y() + newSize.height() - 1);

        mNewPathItem->path()->setPoints(points); // FIXME: don't need both
        mOverlayItem->path()->setPoints(points); // FIXME: don't need both
        break;
    }
    case CreatePolygon:
    case CreatePolyline: {
        QPointF tileCoords = renderer->pixelToTileCoords(pos, currentPathLayer()->level());

        if (!mNewPathItem->path()->centers())
            tileCoords = QPointF(tileCoords.toPoint());

        PathPoints points = mOverlayPath->points();
        points.last() = PathPoint(tileCoords.x(), tileCoords.y());
        mOverlayItem->path()->setPoints(points);
        break;
    }
    }

    mNewPathItem->syncWithPath();
    mOverlayItem->syncWithPath();
}

void CreatePathTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    // Check if we are already creating a new path
    if (mNewPathItem) {
        switch (mMode) {
        case CreateArea:
            if (event->button() == Qt::RightButton)
                cancelNewPath();
            break;
        case CreatePolygon:
        case CreatePolyline:
            if (event->button() == Qt::RightButton) {
                // The polygon needs to have at least three points and a
                // polyline needs at least two.
                int min = mMode == CreatePolygon ? 3 : 2;
                if (mNewPathItem->path()->numPoints() >= min)
                    finishNewPath();
                else
                    cancelNewPath();
            } else if (event->button() == Qt::LeftButton) {
                PathPoints current = mNewPathItem->path()->points();
                PathPoints next = mOverlayPath->points();

                // If the last position is still the same, ignore the click
                if (next.last() == current.last())
                    return;

                // Assign current overlay polygon to the new path
                mNewPathItem->path()->setPoints(next);

                // Add a new editable point to the overlay
                next.append(next.last());
                mOverlayItem->path()->setPoints(next);
            }
            break;
        }
        return;
    }

    if (event->button() != Qt::LeftButton) {
        AbstractPathTool::mousePressed(event);
        return;
    }

    PathLayer *pathLayer = currentPathLayer();
    if (!pathLayer || !pathLayer->isVisible())
        return;

    const MapRenderer *renderer = mapDocument()->renderer();
    QPointF tileCoords = renderer->pixelToTileCoords(event->scenePos(),
                                                     pathLayer->level());

    bool centers = (event->modifiers() & Qt::ControlModifier) != 0;

    if (!centers && (mMode != CreateArea))
        tileCoords = tileCoords.toPoint(); // rounds to nearest integer

    startNewPath(tileCoords, pathLayer);

    mNewPathItem->path()->setCenters(centers);
    mOverlayPath->setCenters(centers);
}

void CreatePathTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mNewPathItem) {
        if (mMode == CreateArea) {
            finishNewPath();
        }
    }
}

void CreatePathTool::languageChanged()
{
    switch (mMode) {
    case CreateArea:
        setName(tr("Create Rectangle"));
        setShortcut(QKeySequence(tr("O")));
        break;
    case CreatePolygon:
        setName(tr("Create Polygon"));
        setShortcut(QKeySequence(tr("P")));
        break;
    case CreatePolyline:
        setName(tr("Create Polyline"));
        setShortcut(QKeySequence(tr("L")));
        break;
    }
}

void CreatePathTool::startNewPath(const QPointF &pos,
                                  PathLayer *pathLayer)
{
    Q_ASSERT(!mNewPathItem);

    Path *newPath = new Path;

    PathPoints points;

    switch (mMode) {
    case CreateArea:
        points += PathPoint(pos.x(), pos.y());
        points += PathPoint(pos.x() + 1, pos.y());
        points += PathPoint(pos.x() + 1, pos.y() + 1);
        points += PathPoint(pos.x(), pos.y() + 1);
        newPath->setClosed(true);
        newPath->setPoints(points);
        mOverlayPath->setPoints(points);
        break;
    case CreatePolygon:
        points += PathPoint(pos.x(), pos.y());
        newPath->setClosed(true);
        newPath->setPoints(points);
        points += points.first(); // The last point is connected to the mouse
        mOverlayPath->setPoints(points);
        break;
    case CreatePolyline:
        points += PathPoint(pos.x(), pos.y());
        newPath->setPoints(points);
        points += points.first(); // The last point is connected to the mouse
        mOverlayPath->setPoints(points);
        break;
    }

    mOverlayItem = new PathItem(mOverlayPath, mapDocument());
    mapScene()->addItem(mOverlayItem);

    pathLayer->addPath(newPath);

    mNewPathItem = new PathItem(newPath, mapDocument());
    mapScene()->addItem(mNewPathItem);
}

Path *CreatePathTool::clearNewPathItem()
{
    Q_ASSERT(mNewPathItem);

    Path *newPath = mNewPathItem->path();

    PathLayer *pathLayer = newPath->pathLayer();
    pathLayer->removePath(newPath);

    delete mNewPathItem;
    mNewPathItem = 0;

    delete mOverlayItem;
    mOverlayItem = 0;

    return newPath;
}

void CreatePathTool::cancelNewPath()
{
    Path *newPath = clearNewPathItem();
    delete newPath;
}

void CreatePathTool::finishNewPath()
{
    Q_ASSERT(mNewPathItem);
    Path *newPath = mNewPathItem->path();
    PathLayer *pathLayer = newPath->pathLayer();
    clearNewPathItem();

    mapDocument()->undoStack()->push(new AddPath(mapDocument(),
                                                 pathLayer,
                                                 newPath));
}
