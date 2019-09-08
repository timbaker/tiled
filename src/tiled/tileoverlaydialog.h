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

#ifndef TILEOVERLAYDIALOG_H
#define TILEOVERLAYDIALOG_H

#include "containeroverlaydialog.h"

class TileOverlayDialog : public AbstractOverlayDialog
{
    Q_OBJECT

public:
    explicit TileOverlayDialog(QWidget *parent = nullptr);

protected:
    bool fileOpen(const QString &fileName, QList<AbstractOverlay*> &overlays) override;
    bool fileSave(const QString &fileName, const QList<AbstractOverlay*> &overlays) override;
    QString defaultWindowTitle() const override
    { return tr("Tile Overlays"); }
    AbstractOverlay *createOverlay(Tiled::Tile *tile) override;
    AbstractOverlayEntry *createEntry(AbstractOverlay *overlay) override;
};

#endif // TILEOVERLAYDIALOG_H
