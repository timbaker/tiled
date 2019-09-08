/*
 * Copyright 2019, Tim Baker <treectrl@users.sf.net>
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

#include "tileoverlaydialog.h"
#include "ui_containeroverlaydialog.h"

#include "tileoverlayfile.h"

#include "BuildingEditor/buildingtiles.h"

TileOverlayDialog::TileOverlayDialog(QWidget* parent)
    : AbstractOverlayDialog(parent)
{
    ui->overlayView->setMoreThan2Tiles(true);
    setWindowTitle(QLatin1Literal("Tile Overlays"));
}

bool TileOverlayDialog::fileOpen(const QString &fileName, QList<AbstractOverlay *> &overlays)
{
    TileOverlayFile tof;
    if (!tof.read(fileName)) {
        mError = tof.errorString();
        return false;
    }

    overlays.clear();
    for (TileOverlay* overlay : tof.takeOverlays()) {
        overlays << overlay;
    }
    return true;
}

bool TileOverlayDialog::fileSave(const QString &fileName, const QList<AbstractOverlay *> &overlays)
{
    TileOverlayFile tof;
    QList<TileOverlay*> overlays1;
    for (AbstractOverlay* overlay : overlays) {
        overlays1 << static_cast<TileOverlay*>(overlay);
    }
    if (!tof.write(fileName, overlays1)) {
        mError = tof.errorString();
        return false;
    }
    return true;
}

AbstractOverlay *TileOverlayDialog::createOverlay(Tiled::Tile *tile)
{
    TileOverlay *overlay = new TileOverlay;
    overlay->mTileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
    overlay->insertEntry(overlay->entryCount(), createEntry(overlay));
    return overlay;
}

AbstractOverlayEntry *TileOverlayDialog::createEntry(AbstractOverlay *overlay)
{
    TileOverlayEntry *entry = new TileOverlayEntry();
    entry->mParent = static_cast<TileOverlay*>(overlay);
    entry->mRoomName = QLatin1String("other");
//    entry->mTiles += QLatin1String("none");
//    entry->mTiles += QLatin1String("none");
    return entry;
}
