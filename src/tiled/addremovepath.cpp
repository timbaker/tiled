/*
 * addremovepath.cpp
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

#include "addremovepath.h"

#include "mapdocument.h"
#include "pathlayer.h"
#include "pathmodel.h"

#include <QCoreApplication>

using namespace Tiled;
using namespace Tiled::Internal;

AddRemovePath::AddRemovePath(MapDocument *mapDocument,
                             PathLayer *pathLayer,
                             Path *path,
                             bool ownsPath,
                             QUndoCommand *parent)
    : QUndoCommand(parent)
    , mMapDocument(mapDocument)
    , mPath(path)
    , mPathLayer(pathLayer)
    , mIndex(-1)
    , mOwnsPath(ownsPath)
{
}

AddRemovePath::~AddRemovePath()
{
    if (mOwnsPath)
        delete mPath;
}

void AddRemovePath::addPath()
{
    mMapDocument->pathModel()->insertPath(mPathLayer, mIndex, mPath);
    mOwnsPath = false;
}

void AddRemovePath::removePath()
{
    mIndex = mMapDocument->pathModel()->removePath(mPathLayer, mPath);
    mOwnsPath = true;
}

/////

AddPath::AddPath(MapDocument *mapDocument, PathLayer *pathLayer,
                 Path *path, QUndoCommand *parent)
    : AddRemovePath(mapDocument,
                    pathLayer,
                    path,
                    true,
                    parent)
{
    setText(QCoreApplication::translate("Undo Commands", "Add Path"));
}

/////

RemovePath::RemovePath(MapDocument *mapDocument, Path *path,
                       QUndoCommand *parent)
    : AddRemovePath(mapDocument,
                    path->pathLayer(),
                    path,
                    false,
                    parent)
{
    setText(QCoreApplication::translate("Undo Commands", "Remove Path"));
}
