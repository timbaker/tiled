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

#ifndef BUILDINGTILEENTRYVIEW_H
#define BUILDINGTILEENTRYVIEW_H

#include "mixedtilesetview.h"

namespace BuildingEditor {
class BuildingTileEntry;

class BuildingTileEntryView : public Tiled::Internal::MixedTilesetView
{
    Q_OBJECT
public:
    BuildingTileEntryView(QWidget *parent = 0);

    void clear();
    void setEntries(const QList<BuildingTileEntry*> &entries, bool categoryLabels = false);

    QModelIndex index(BuildingTileEntry *entry);
    BuildingTileEntry *entry(const QModelIndex &index);

    typedef Tiled::Tileset Tileset;
private slots:
    void tilesetChanged(Tileset *tileset);
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

private:
    QList<BuildingTileEntry*> mEntries;
    bool mCategoryLabels;
};

} // namespace BuildingEditor

#endif // BUILDINGTILEENTRYVIEW_H
