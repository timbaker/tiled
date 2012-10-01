/*
 * abstractobjecttool.h
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

#ifndef ABSTRACTPATHTOOL_H
#define ABSTRACTPATHTOOL_H

#include "abstracttool.h"

namespace Tiled {

class Path;
class PathLayer;

namespace Internal {

class PathItem;

/**
 * A convenient base class for tools that work on path layers. Implements
 * the standard context menu.
 */
class AbstractPathTool : public AbstractTool
{
    Q_OBJECT

public:
    /**
     * Constructs an path object tool with the given \a name and \a icon.
     */
    AbstractPathTool(const QString &name,
                       const QIcon &icon,
                       const QKeySequence &shortcut,
                       QObject *parent = 0);

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseLeft();
    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);

protected:
    /**
     * Overridden to only enable this tool when the currently selected layer is
     * an path layer.
     */
    void updateEnabledState();

    MapScene *mapScene() const { return mMapScene; }
    PathLayer *currentPathLayer() const;
    PathItem *topMostPathItemAt(QPointF pos) const;

private:
    void showContextMenu(PathItem *clickedPath,
                         QPoint screenPos, QWidget *parent);

    void duplicatePaths(const QList<Path*> &paths);
    void removePaths(const QList<Path*> &paths);
    void movePathsToLayer(const QList<Path*> &paths,
                          PathLayer *pathLayer);

    MapScene *mMapScene;
};

} // namespace Internal
} // namespace Tiled

#endif // ABSTRACTPATHTOOL_H
