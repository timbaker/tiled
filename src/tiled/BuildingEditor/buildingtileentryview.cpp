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

#include "buildingtileentryview.h"

#include "buildingtiles.h"

#include "tilemetainfomgr.h"
#include "tilesetmanager.h"

#include "tile.h"
#include "tileset.h"

using namespace BuildingEditor;
using namespace Tiled::Internal;

BuildingTileEntryView::BuildingTileEntryView(QWidget *parent) :
    MixedTilesetView(parent),
    mCategoryLabels(false)
{
    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    model()->setShowEmptyTilesAsMissig(true);
}

void BuildingTileEntryView::clear()
{
    mEntries.clear();
    MixedTilesetView::clear();
}

void BuildingTileEntryView::setEntries(const QList<BuildingTileEntry *> &entries,
                                       bool categoryLabels)
{
    QList<Tiled::Tile*> tiles;
    QList<void*> userData;
    QStringList headers;

    foreach (BuildingTileEntry *entry, entries) {
        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
            tiles += tile;
            userData += entry;
            if (categoryLabels)
                headers += entry->category()->label();
            else if (tile == TilesetManager::instance()->missingTile())
                headers += entry->displayTile()->mTilesetName;
            else
                headers += tile->tileset()->name();
        }
    }
    setTiles(tiles, userData, headers);

    mEntries = entries;
    mCategoryLabels = categoryLabels;
}

QModelIndex BuildingTileEntryView::index(BuildingTileEntry *entry)
{
    return model()->index((void*)entry);
}

BuildingTileEntry *BuildingTileEntryView::entry(const QModelIndex &index)
{
    if (model()->tileAt(index))
        return static_cast<BuildingTileEntry*>(model()->userDataAt(index));
    return 0;
}

void BuildingTileEntryView::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
    setEntries(mEntries, mCategoryLabels);
}

void BuildingTileEntryView::tilesetAdded(Tileset *tileset)
{
    Q_UNUSED(tileset)
    setEntries(mEntries, mCategoryLabels);
}

void BuildingTileEntryView::tilesetAboutToBeRemoved(Tileset *tileset)
{
    QList<Tiled::Tile*> tiles;
    QList<void*> userData;
    QStringList headers;

    foreach (BuildingTileEntry *entry, mEntries) {
        if (Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile())) {
            if (tile->tileset() == tileset)
                tile = TilesetManager::instance()->missingTile();
            tiles += tile;
            userData += entry;
            if (mCategoryLabels)
                headers += entry->category()->label();
            else if (tile == TilesetManager::instance()->missingTile())
                headers += entry->displayTile()->mTilesetName;
            else
                headers += tile->tileset()->name();
        }
    }
    setTiles(tiles, userData, headers);
}

void BuildingTileEntryView::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
//    setEntries(mEntries, mCategoryLabels);
}
