/*
 * tileselectionitem.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "tileselectionitem.h"

#include "map.h"
#include "mapdocument.h"
#include "maprenderer.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;
using namespace Tiled::Internal;

TileSelectionItem::TileSelectionItem(MapDocument *mapDocument)
    : mMapDocument(mapDocument)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    connect(mMapDocument, &MapDocument::tileSelectionChanged,
            this, &TileSelectionItem::selectionChanged);
#ifdef ZOMBOID
    connect(mMapDocument, &MapDocument::currentLayerIndexChanged,
            this, &TileSelectionItem::currentLayerIndexChanged);
#endif

    updateBoundingRect();
}

QRectF TileSelectionItem::boundingRect() const
{
    return mBoundingRect;
}

void TileSelectionItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
{
#ifdef ZOMBOID
    const QRegion &selection = mMapDocument->tileSelection().translated(mDragOffset);
#else
    const QRegion &selection = mMapDocument->tileSelection();
#endif
    QColor highlight = QApplication::palette().highlight().color();
    highlight.setAlpha(128);

    MapRenderer *renderer = mMapDocument->renderer();
    renderer->drawTileSelection(painter, selection, highlight,
#ifdef ZOMBOID
        option->exposedRect, mMapDocument->currentLevelIndex());
#else
                                option->exposedRect);
#endif
}

void TileSelectionItem::selectionChanged(const QRegion &newSelection,
                                         const QRegion &oldSelection)
{
    prepareGeometryChange();
    updateBoundingRect();

    // Make sure changes within the bounding rect are updated
    const QRect changedArea = newSelection.xored(oldSelection).boundingRect();
#ifdef ZOMBOID
    update(mMapDocument->renderer()->boundingRect(changedArea, mMapDocument->currentLevelIndex()));
#else
    update(mMapDocument->renderer()->boundingRect(changedArea));
#endif
}

#ifdef ZOMBOID
void TileSelectionItem::currentLayerIndexChanged(int levelIndex, int layerIndex)
{
    Q_UNUSED(levelIndex)
    Q_UNUSED(layerIndex)
    prepareGeometryChange();
    updateBoundingRect();
}
#endif

void TileSelectionItem::updateBoundingRect()
{
#ifdef ZOMBOID
    const QRect b = mMapDocument->tileSelection().boundingRect().translated(mDragOffset);
    mBoundingRect = mMapDocument->renderer()->boundingRect(b, mMapDocument->currentLevelIndex());
#else
    const QRect b = mMapDocument->tileSelection().boundingRect();
    mBoundingRect = mMapDocument->renderer()->boundingRect(b);
#endif
}

#ifdef ZOMBOID
void TileSelectionItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    prepareGeometryChange();
    updateBoundingRect();
}
#endif
