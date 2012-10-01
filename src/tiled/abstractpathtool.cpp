/*
 * abstractpathtool.cpp
 * Copyright 2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "abstractpathtool.h"

#include "addremovepath.h"
#include "map.h"
#include "mapdocument.h"
#include "pathlayer.h"
#include "pathlayeritem.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "movepathtolayer.h"
//#include "objectpropertiesdialog.h"
#include "utils.h"

#include <QMenu>
#include <QUndoStack>

#include <qmath.h>

using namespace Tiled;
using namespace Tiled::Internal;

AbstractPathTool::AbstractPathTool(const QString &name,
                                   const QIcon &icon,
                                   const QKeySequence &shortcut,
                                   QObject *parent)
    : AbstractTool(name, icon, shortcut, parent)
    , mMapScene(0)
{
}

void AbstractPathTool::activate(MapScene *scene)
{
    mMapScene = scene;
}

void AbstractPathTool::deactivate(MapScene *)
{
    mMapScene = 0;
}

void AbstractPathTool::mouseLeft()
{
    setStatusInfo(QString());
}

void AbstractPathTool::mouseMoved(const QPointF &pos,
                                  Qt::KeyboardModifiers)
{
    Layer *layer = currentPathLayer();
    const QPointF tilePosF = mapDocument()->renderer()
            ->pixelToTileCoords(pos, layer ? layer->level() : 0);

    const int x = qFloor(tilePosF.x());
    const int y = qFloor(tilePosF.y());
    setStatusInfo(QString(QLatin1String("%1, %2")).arg(x).arg(y));
}

void AbstractPathTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        showContextMenu(topMostPathItemAt(event->scenePos()),
                        event->screenPos(), event->widget());
    }
}

void AbstractPathTool::updateEnabledState()
{
    setEnabled(currentPathLayer() != 0);
}

ObjectGroup *AbstractPathTool::currentPathLayer() const
{
    if (!mapDocument())
        return 0;

    return dynamic_cast<PathLayer*>(mapDocument()->currentLayer());
}

PathItem *AbstractPathTool::topMostPathItemAt(QPointF pos) const
{
    foreach (QGraphicsItem *item, mMapScene->items(pos)) {
        if (PathItem *pathItem = dynamic_cast<PathItem*>(item))
            return pathItem;
    }
    return 0;
}

/**
 * Shows the context menu for map objects. The menu allows you to duplicate and
 * remove the map objects, or to edit their properties.
 */
void AbstractPathTool::showContextMenu(PathItem *clickedPathItem,
                                       QPoint screenPos, QWidget *parent)
{
    QSet<PathItem *> selection = mMapScene->selectedPathItems();
    if (clickedPathItem && !selection.contains(clickedPathItem)) {
        selection.clear();
        selection.insert(clickedPathItem);
        mMapScene->setSelectedPathItems(selection);
    }
    if (selection.isEmpty())
        return;

    const QList<Path*> selectedPaths = mapDocument()->selectedPaths();

    QList<PathLayer*> pathLayers;
    foreach (Layer *layer, mapDocument()->map()->layers()) {
        if (PathLayer *pathLayer = layer->asPathLayer())
            pathLayers.append(pathLayer);
    }

    QMenu menu;
    QIcon dupIcon(QLatin1String(":images/16x16/stock-duplicate-16.png"));
    QIcon delIcon(QLatin1String(":images/16x16/edit-delete.png"));
    QIcon propIcon(QLatin1String(":images/16x16/document-properties.png"));
    QString dupText = tr("Duplicate %n Object(s)", "", selectedPaths.size());
    QString removeText = tr("Remove %n Object(s)", "", selectedPaths.size());
    QAction *dupAction = menu.addAction(dupIcon, dupText);
    QAction *removeAction = menu.addAction(delIcon, removeText);

    typedef QMap<QAction*, PathLayer*> MoveToLayerActionMap;
    MoveToLayerActionMap moveToLayerActions;

    if (pathLayers.size() > 1) {
        menu.addSeparator();
        QMenu *moveToLayerMenu = menu.addMenu(tr("Move %n Path(s) to Layer",
                                                 "", selectedPaths.size()));
        foreach (PathLayer *pathLayer, pathLayers) {
            QAction *action = moveToLayerMenu->addAction(pathLayer->name());
            moveToLayerActions.insert(action, pathLayer);
        }
    }

    menu.addSeparator();
    QAction *propertiesAction = menu.addAction(propIcon,
                                               tr("Path &Properties..."));
    // TODO: Implement editing of properties for multiple paths
    propertiesAction->setEnabled(selectedPaths.size() == 1);

    Utils::setThemeIcon(removeAction, "edit-delete");
    Utils::setThemeIcon(propertiesAction, "document-properties");

    QAction *selectedAction = menu.exec(screenPos);

    if (selectedAction == dupAction) {
        duplicatePaths(selectedPaths);
    }
    else if (selectedAction == removeAction) {
        removePaths(selectedPaths);
    }
#if 0
    else if (selectedAction == propertiesAction) {
        Path *path = selectedPaths.first();
        PathPropertiesDialog propertiesDialog(mapDocument(), path, parent);
        propertiesDialog.exec();
    }
#endif

    MoveToLayerActionMap::const_iterator i =
            moveToLayerActions.find(selectedAction);

    if (i != moveToLayerActions.end()) {
        PathLayer *pathLayer = i.value();
        movePathsToLayer(selectedPaths, pathLayer);
    }
}

void AbstractPathTool::duplicatePaths(const QList<Path *> &paths)
{
    QUndoStack *undoStack = mapDocument()->undoStack();
    undoStack->beginMacro(tr("Duplicate %n Path(s)", "", objects.size()));

    QList<Path*> clones;
    foreach (const Path *path, paths) {
        Path *clone = path->clone();
        clones.append(clone);
        undoStack->push(new AddPath(mapDocument(),
                                    path->pathLayer(),
                                    clone));
    }

    undoStack->endMacro();
    mapDocument()->setSelectedPaths(clones);
}

void AbstractPathTool::removePaths(const QList<Path *> &paths)
{
    QUndoStack *undoStack = mapDocument()->undoStack();
    undoStack->beginMacro(tr("Remove %n Path(s)", "", paths.size()));
    foreach (Path *path, paths)
        undoStack->push(new RemovePath(mapDocument(), path));
    undoStack->endMacro();
}

void AbstractPathTool::movePathsToLayer(const QList<Path *> &paths,
                                        PathLayer *pathLayer)
{
    QUndoStack *undoStack = mapDocument()->undoStack();
    undoStack->beginMacro(tr("Move %n Path(s) to Layer", "", paths.size()));
    foreach (Path *path, paths) {
        if (path->pathLayer() == pathLayer)
            continue;
        undoStack->push(new MovePathToLayer(mapDocument(), path, pathLayer));
    }
    undoStack->endMacro();
}
