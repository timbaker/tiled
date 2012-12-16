/*
 * movemapobjecttogroup.cpp
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

#include "movepathtolayer.h"

#include "mapdocument.h"
#include "pathlayer.h"
#include "pathmodel.h"
#include <QCoreApplication>

using namespace Tiled;
using namespace Tiled::Internal;

MovePathToLayer::MovePathToLayer(MapDocument *mapDocument,
                                           Path *path,
                                 PathLayer *pathLayer)
    : mMapDocument(mapDocument)
    , mPath(path)
    , mOldLayer(path->pathLayer())
    , mNewLayer(pathLayer)
{
    setText(QCoreApplication::translate("Undo Commands",
                                        "Move Path to Layer"));
}

void MovePathToLayer::undo()
{
    mMapDocument->pathModel()->removePath(mNewLayer, mPath);
    mMapDocument->pathModel()->insertPath(mOldLayer, -1, mPath);
}

void MovePathToLayer::redo()
{
    mMapDocument->pathModel()->removePath(mOldLayer, mPath);
    mMapDocument->pathModel()->insertPath(mNewLayer, -1, mPath);
}
