/*
 * changepathpolygon.h
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

#ifndef CHANGEPATHPOLYGON_H
#define CHANGEPATHPOLYGON_H

#include <QPolygon>
#include <QUndoCommand>

namespace Tiled {

class Path;

namespace Internal {

class MapDocument;

/**
 * Changes the polygon of a Path.
 *
 * This class expects the polygon to be already changed, and takes the previous
 * polygon in the constructor.
 */
class ChangePathPolygon : public QUndoCommand
{
public:
    ChangePathPolygon(MapDocument *mapDocument,
                      Path *path,
                      const QPolygon &oldPolygon);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    Path *mPath;

    QPolygon mOldPolygon;
    QPolygon mNewPolygon;
};

} // namespace Internal
} // namespace Tiled

#endif // CHANGEPOLYGON_H
