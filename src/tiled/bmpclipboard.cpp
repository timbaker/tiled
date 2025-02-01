/*
 * Copyright 2022, Tim Baker <treectrl@users.sf.net>
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

#include "bmpclipboard.h"

#include "bmptool.h"
#include "mapdocument.h"
#include "toolmanager.h"

using namespace Tiled;
using namespace Tiled::Internal;

BmpClipboard::BmpClipboard(QObject *parent)
    : QObject(parent)
{

}

void BmpClipboard::copySelection(const MapDocument *mapDocument)
{
    mRegion = mapDocument->bmpSelection();
    mBmpMain = mapDocument->map()->bmpMain().image().copy(mRegion.boundingRect());
    mBmpVeg = mapDocument->map()->bmpVeg().image().copy(mRegion.boundingRect());
}

void BmpClipboard::pasteSelection(MapDocument *mapDocument)
{
    Q_UNUSED(mapDocument)
    BmpBrushTool::instance()->setUseBmpClipboard(true);
    ToolManager::instance()->selectTool(BmpBrushTool::instance());
}

void BmpClipboard::paint(MapDocument *mapDocument, const QPoint &topLeft, bool mergeable)
{
    QRegion region = mRegion.translated(topLeft - mRegion.boundingRect().topLeft());
    region &= QRect(QPoint(), mapDocument->map()->size());

    QRegion paintRgn0;
    QImage bmpImage = mapDocument->map()->bmpMain().image();
    for (QRect r : region) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (bmpImage.pixel(x, y) != mBmpMain.pixel(x - region.boundingRect().left(), y - region.boundingRect().top())) {
                    paintRgn0 += QRect(x, y, 1, 1);
                }
            }
        }
    }

    QRegion paintRgn1;
    bmpImage = mapDocument->map()->bmpVeg().image();
    for (QRect r : region) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (bmpImage.pixel(x, y) != mBmpVeg.pixel(x - region.boundingRect().left(), y - region.boundingRect().top())) {
                    paintRgn1 += QRect(x, y, 1, 1);
                }
            }
        }
    }
    PaintBMPx2 *cmd = new PaintBMPx2(mapDocument, topLeft.x(), topLeft.y(), mBmpMain, mBmpVeg,
                                     paintRgn0, paintRgn1, mergeable);
    mapDocument->undoStack()->push(cmd);
}

bool BmpClipboard::canPaste() const
{
    return mRegion.isEmpty() == false;
}

const QRegion &BmpClipboard::region() const
{
    return mRegion;
}
