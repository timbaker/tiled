/*
 * movepathtolayer.h
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
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

#ifndef MOVEPATHTOLAYER_H
#define MOVEPATHTOLAYER_H

#include <QUndoCommand>

namespace Tiled {

class Path;
class PathLayer;

namespace Internal {

class MapDocument;

class MovePathToLayer : public QUndoCommand
{
public:
    MovePathToLayer(MapDocument *mapDocument,
                    Path *path,
                    PathLayer *pathLayer);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    Path *mPath;
    PathLayer *mOldLayer;
    PathLayer *mNewLayer;
};

} // namespace Internal
} // namespace Tiled

#endif // MOVEMAPOBJECTTOGROUP_H
