/*
 * createpathtool.h
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef CREATEPATHTOOL_H
#define CREATEPATHTOOL_H

#include "abstractpathtool.h"

namespace Tiled {

class Path;
class PathLayer;

namespace Internal {

class PathItem;

class CreatePathTool : public AbstractPathTool
{
    Q_OBJECT

public:
    enum CreationMode {
        CreateArea,
        CreatePolygon,
        CreatePolyline
    };

    CreatePathTool(CreationMode mode, QObject *parent = 0);
    ~CreatePathTool();

    void deactivate(MapScene *scene);

    void mouseEntered();
    void mouseMoved(const QPointF &pos,
                    Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

private:
    void startNewPath(const QPointF &pos, PathLayer *pathLayer);
    Path *clearNewPathItem();
    void cancelNewPath();
    void finishNewPath();

    PathItem *mNewPathItem;
    PathLayer *mOverlayPathLayer;
    Path *mOverlayPath;
    PathItem *mOverlayItem;
    CreationMode mMode;
};

} // namespace Internal
} // namespace Tiled

#endif // CREATEPATHTOOL_H
