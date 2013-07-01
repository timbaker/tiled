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

#ifndef PICKTILETOOL_H
#define PICKTILETOOL_H

#include "abstracttiletool.h"

#include "BuildingEditor/singleton.h"

namespace Tiled {
class Tile;

namespace Internal {

class PickTileTool : public AbstractTileTool, public Singleton<PickTileTool>
{
    Q_OBJECT
public:
    PickTileTool(QObject *parent = 0);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *) {}

    void languageChanged() {}

protected:
    void tilePositionChanged(const QPoint &tilePos);

    typedef Tiled::Tile Tile;
signals:
    void tilePicked(Tile *tile);
};

} // namespace Internal
} // namespace Tiled


#endif // PICKTILETOOL_H
