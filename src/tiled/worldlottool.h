/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef WORLDLOTTOOL_H
#define WORLDLOTTOOL_H

#include "abstracttool.h"

class WorldCell;
class WorldCellLot;

class QGraphicsPolygonItem;

namespace Tiled {
namespace Internal {

class WorldLotTool : public AbstractTool
{
    Q_OBJECT
public:
    static WorldLotTool *instance();
    static void deleteInstance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);
    void mouseEntered();
    void mouseLeft();
    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);
    void modifiersChanged(Qt::KeyboardModifiers) {}
    void languageChanged();

private:
    WorldCellLot *topmostLotAt(const QPointF &scenePos);
    WorldCell *adjacentCellAt(const QPointF &scenePos);
    void updateHoverItem(WorldCellLot *lot);

private slots:
    void beforeWorldChanged();
    void afterWorldChanged();
    void lotVisibilityChanged(WorldCellLot *lot);

private:
    Q_DISABLE_COPY(WorldLotTool)
    static WorldLotTool *mInstance;
    WorldLotTool(QObject *parent = 0);
    ~WorldLotTool();

    MapScene *mScene;
    WorldCell *mCell;
    QGraphicsPolygonItem *mHoverItem;
    WorldCellLot *mHoverLot;
    QPointF mLastScenePos;
    bool mShowingContextMenu;
};

} // namespace Internal
} // namespace Tiled

#endif // WORLDLOTTOOL_H
