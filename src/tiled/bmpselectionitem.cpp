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

#include "bmpselectionitem.h"

#include "map.h"
#include "mapdocument.h"
#include "maprenderer.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;
using namespace Tiled::Internal;

BmpSelectionItem::BmpSelectionItem(MapDocument *mapDocument)
    : mMapDocument(mapDocument)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    connect(mMapDocument, SIGNAL(bmpSelectionChanged(QRegion,QRegion)),
            SLOT(selectionChanged(QRegion,QRegion)));

    updateBoundingRect();
}

QRectF BmpSelectionItem::boundingRect() const
{
    return mBoundingRect;
}

void BmpSelectionItem::paint(QPainter *painter,
                              const QStyleOptionGraphicsItem *option,
                              QWidget *)
{
    const QRegion &selection = mMapDocument->bmpSelection().translated(mDragOffset);
    QColor highlight = QApplication::palette().highlight().color();
    highlight.setAlpha(128);

    MapRenderer *renderer = mMapDocument->renderer();
    renderer->drawTileSelection(painter, selection, highlight,
                                option->exposedRect, mMapDocument->currentLevel());
}

void BmpSelectionItem::setDragOffset(const QPoint &offset)
{
    mDragOffset = offset;
    prepareGeometryChange();
    updateBoundingRect();
}

void BmpSelectionItem::selectionChanged(const QRegion &newSelection,
                                        const QRegion &oldSelection)
{
    prepareGeometryChange();
    updateBoundingRect();

    // Make sure changes within the bounding rect are updated
    const QRect changedArea = newSelection.xored(oldSelection).boundingRect();
    update(mMapDocument->renderer()->boundingRect(changedArea, mMapDocument->currentLevel()));
}

void BmpSelectionItem::updateBoundingRect()
{
    const QRect b = mMapDocument->bmpSelection().translated(mDragOffset).boundingRect();
    mBoundingRect = mMapDocument->renderer()->boundingRect(b, mMapDocument->currentLevel());
}
