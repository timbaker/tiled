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

#include <QContextMenuEvent>

TileOverlayDialog::TileOverlayDialog(QWidget* parent)
    : AbstractOverlayDialog(parent)
{
    ui->overlayView->setMoreThan2Tiles(true);
    setWindowTitle(QLatin1String("Tile Overlays"));

    connect(ui->overlayView, &ContainerOverlayView::showContextMenu, this, &TileOverlayDialog::showContextMenu);
}

void TileOverlayDialog::showContextMenu(const QModelIndex &index, int entryIndex, QContextMenuEvent *event)
{
    ContainerOverlayModel *m = ui->overlayView->model();
    if (!m->moreThan2Tiles()) {
        return;
    }
    AbstractOverlayEntry *entry = m->entryAt(index);

    QMenu menu;
    QAction *actionEditRoom = menu.addAction(tr("Edit Room"));
    QAction *actionEditChance = menu.addAction(tr("Edit Chance"));
    QAction *actionEditUsage = menu.addAction(tr("Edit Usage"));
    QAction *actionRemove = (entryIndex > 0 && entryIndex < entry->tiles().size()) ? menu.addAction(tr("Remove Tile")) : nullptr;
    QAction *action = menu.exec(event->globalPos());
    if (action == actionEditRoom) {
        ui->overlayView->edit(index, EditAbstractOverlay::RoomName);
    }
    if (action == actionEditUsage) {
        ui->overlayView->edit(index, EditAbstractOverlay::Usage);
    }
    if (action == actionEditChance) {
        ui->overlayView->edit(index, EditAbstractOverlay::Chance);
    }
    if ((actionRemove != nullptr) && (action == actionRemove)) {
        removeTile(entry, entryIndex);
    }
}

bool TileOverlayDialog::fileOpen(const QString &fileName, QList<AbstractOverlay *> &overlays)
{
    TileOverlayFile tof;
    if (!tof.read(fileName)) {
        mError = tof.errorString();
        return false;
    }

    overlays.clear();
    const QList<TileOverlay*> tileOverlays = tof.takeOverlays();
    for (TileOverlay* overlay : tileOverlays) {
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
