/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "reorderpath.h"

#include "mapdocument.h"
#include "pathlayer.h"
#include "pathmodel.h"
#include <QCoreApplication>

using namespace Tiled;
using namespace Tiled::Internal;

ReorderPath::ReorderPath(MapDocument *mapDocument, Path *path, int newIndex) :
    mMapDocument(mapDocument),
    mPath(path),
    mLayer(path->pathLayer()),
    mOldIndex(path->index()),
    mNewIndex(newIndex)
{
    setText(QCoreApplication::translate("Undo Commands",
                                        "Reorder Path"));
}

void ReorderPath::undo()
{
    mMapDocument->pathModel()->removePath(mLayer, mPath);
    mMapDocument->pathModel()->insertPath(mLayer, mOldIndex, mPath);
    mMapDocument->setSelectedPaths(mMapDocument->selectedPaths() + (QList<Path*>() << mPath));
}

void ReorderPath::redo()
{
    mMapDocument->pathModel()->removePath(mLayer, mPath);
    mMapDocument->pathModel()->insertPath(mLayer, mNewIndex, mPath);
    mMapDocument->setSelectedPaths(mMapDocument->selectedPaths() + (QList<Path*>() << mPath));
}
