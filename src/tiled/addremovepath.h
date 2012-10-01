/*
 * addremovemapobject.h
 * Copyright 2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#ifndef ADDREMOVEPATH_H
#define ADDREMOVEPATH_H

#include <QUndoCommand>

namespace Tiled {

class Path;
class PathLayer;

namespace Internal {

class MapDocument;

/**
 * Abstract base class for AddMapObject and RemoveMapObject.
 */
class AddRemovePath : public QUndoCommand
{
public:
    AddRemovePath(MapDocument *mapDocument,
                       ObjectGroup *objectGroup,
                       MapObject *mapObject,
                       bool ownObject,
                       QUndoCommand *parent = 0);
    ~AddRemovePath();

protected:
    void addPath();
    void removePath();

private:
    MapDocument *mMapDocument;
    MapObject *mPath;
    ObjectGroup *mPathLayer;
    int mIndex;
    bool mOwnsPath;
};

/**
 * Undo command that adds a path to a map.
 */
class AddPath : public AddRemovePath
{
public:
    AddPath(MapDocument *mapDocument, PathLayer *pathLayer,
            Path *path, QUndoCommand *parent = 0);

    void undo()
    { removePath(); }

    void redo()
    { addPath(); }
};

/**
 * Undo command that removes a path from a map.
 */
class RemovePath : public AddRemovePath
{
public:
    RemovePath(MapDocument *mapDocument, Path *path,
               QUndoCommand *parent = 0);

    void undo()
    { addPath(); }

    void redo()
    { removePath(); }
};

} // namespace Internal
} // namespace Tiled

#endif // ADDREMOVEPATH_H
