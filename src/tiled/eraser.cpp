/*
 * eraser.cpp
 * Copyright 2009-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
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

#include "eraser.h"

#include "brushitem.h"
#include "erasetiles.h"
#include "map.h"
#include "mapdocument.h"
#include "mapscene.h"
#ifdef ZOMBOID
#include "preferences.h"
#endif
#include "tilelayer.h"

using namespace Tiled;
using namespace Tiled::Internal;

Eraser::Eraser(QObject *parent)
    : AbstractTileTool(tr("Eraser"),
                       QIcon(QLatin1String(
                               ":images/22x22/stock-tool-eraser.png")),
                       QKeySequence(tr("E")),
                       parent)
    , mErasing(false)
{
#ifdef ZOMBOID
    connect(Preferences::instance(), SIGNAL(eraserBrushSizeChanged(int)),
            SLOT(brushSizeChanged(int)));
#endif
}

void Eraser::tilePositionChanged(const QPoint &tilePos)
{
#ifdef ZOMBOID
    Q_UNUSED(tilePos)
    brushItem()->setTileRegion(brushRegion());
#else
    brushItem()->setTileRegion(QRect(tilePos, QSize(1, 1)));
#endif

    if (mErasing)
        doErase(true);
}

#ifdef ZOMBOID
QRegion Eraser::brushRegion()
{
    int brushSize = Preferences::instance()->eraserBrushSize();
    return QRect(tilePosition() - QPoint(brushSize/2, brushSize/2),
                 QSize(brushSize, brushSize));
}

void Eraser::brushSizeChanged(int newSize)
{
    Q_UNUSED(newSize);
    brushItem()->setTileRegion(brushRegion());
}
#endif

void Eraser::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        mErasing = true;
#ifdef ZOMBOID
        mMergeable = false;
#endif
        doErase(false);
    }
}

void Eraser::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mErasing = false;
}

void Eraser::languageChanged()
{
    setName(tr("Eraser"));
    setShortcut(QKeySequence(tr("E")));
}

void Eraser::doErase(bool mergeable)
{
    TileLayer *tileLayer = currentTileLayer();

#ifdef ZOMBOID
    Q_UNUSED(mergeable)
    QRegion eraseRegion = brushRegion() & tileLayer->bounds();
    if (eraseRegion.isEmpty())
        return;

    EraseTiles *erase = new EraseTiles(mapDocument(), tileLayer, eraseRegion);
    erase->setMergeable(mMergeable);
    mMergeable = true;
#else
    const QPoint tilePos = tilePosition();

    if (!tileLayer->bounds().contains(tilePos))
        return;

    QRegion eraseRegion(tilePos.x(), tilePos.y(), 1, 1);
    EraseTiles *erase = new EraseTiles(mapDocument(), tileLayer, eraseRegion);
    erase->setMergeable(mergeable);
#endif

    mapDocument()->undoStack()->push(erase);
    mapDocument()->emitRegionEdited(eraseRegion, tileLayer);
}
