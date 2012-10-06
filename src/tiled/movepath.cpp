/*
 * movepath.cpp
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

#include "movepath.h"

#include "mapdocument.h"
#include "pathmodel.h"

#include <QCoreApplication>

using namespace Tiled;
using namespace Internal;

MovePath::MovePath(MapDocument *mapDocument, Tiled::Path *path,
                   const QPoint &delta)
    : mMapDocument(mapDocument)
    , mPath(path)
    , mDelta(delta)
{
    setText(QCoreApplication::translate("Undo Commands", "Move Path"));
}

void MovePath::undo()
{
    mMapDocument->pathModel()->movePath(mPath, -mDelta);
}

void MovePath::redo()
{
    mMapDocument->pathModel()->movePath(mPath, mDelta);
}
