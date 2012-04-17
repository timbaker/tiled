/*
 * zlot.cpp
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

#include "zlot.hpp"

#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "maprenderer.h"
#include "mapscene.h"
#include "tilelayer.h"
#include "tileset.h"
#include "ztilelayergroup.h"
#include "ztilelayergroupitem.h"

namespace Tiled {

///// ///// ///// ///// /////

ZLotTileLayerGroup::ZLotTileLayerGroup(int level, ZLot *owner)
	: ZTileLayerGroup(level)
	, mLot(owner)
{
}

bool ZLotTileLayerGroup::orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const
{
	bool cleared = false;
	foreach (TileLayer *tl, mLayers) {
		if (!tl->isVisible())
			continue;
#if 0 // DO NOT USE - VERY SLOW
		if (tl->isEmpty())
			continue;
#endif
		QPoint pos = point - tl->position();
		if (tl->contains(pos)) {
			const Cell *cell = &tl->cellAt(pos);
			if (!cell->isEmpty()) {
				if (!cleared) {
					cells.clear();
					cleared = true;
				}
				cells.append(cell);
			}
		}
	}
	return !cells.empty();
}

///// ///// ///// ///// /////

// from map.cpp
static void maxMargins(const QMargins &a,
                           const QMargins &b,
						   QMargins &out)
{
    out.setLeft(qMax(a.left(), b.left()));
    out.setTop(qMax(a.top(), b.top()));
    out.setRight(qMax(a.right(), b.right()));
    out.setBottom(qMax(a.bottom(), b.bottom()));
}

ZLot::ZLot(Map *map, Map::Orientation orient)
	: mMap(map)
	, mOrientation(orient)
{
	int index = 0;
	foreach (Layer *layer, mMap->layers()) {
		if (TileLayer *tl = layer->asTileLayer()) {
			if (tl->name().contains(QLatin1String("NoRender")))
				continue;
			if (tl->isEmpty()) // SLOW - checks every cell
				continue;
			uint level;
			if (groupForTileLayer(tl, &level)) {
				tl->setLevel(level);
				if (!mLevelToTileLayers.contains(level))
					mLevelToTileLayers[level] = new ZLotTileLayerGroup(level, this);
				ZLotTileLayerGroup *layerGroup = mLevelToTileLayers[level];
				layerGroup->addTileLayer(tl, index);
				mLayersByName[layer->name()].append(layer);
				++index;
			}
		}
	}

	mMinLevel = 10000;
	foreach (ZLotTileLayerGroup *layerGroup, mLevelToTileLayers) {
		int level = layerGroup->level();
		layerGroup->mBounds = layerGroup->_bounds();
		maxMargins(layerGroup->mMargins, layerGroup->_drawMargins(), layerGroup->mMargins);
		if (level > map->maxLevel())
			map->setMaxLevel(level);
		if (level < mMinLevel)
			mMinLevel = level;
	}
	if (mMinLevel == 10000)
		mMinLevel = 0;
}

ZLot::~ZLot()
{
	qDeleteAll(mLevelToTileLayers);
#if 0 // tilesets are shared, see ZLotManager
	qDeleteAll(mMap->tilesets()); // FIXME: share these
#endif
	delete mMap;
}

// FIXME: duplicated in ZomboidScene
bool ZLot::groupForTileLayer(TileLayer *tl, uint *group)
{
	// See if the layer name matches "0_foo" or "1_bar" etc.
	const QString& name = tl->name();
	QStringList sl = name.trimmed().split(QLatin1Char('_'));
	if (sl.count() > 1 && !sl[1].isEmpty()) {
		bool conversionOK;
		(*group) = sl[0].toUInt(&conversionOK);
		return conversionOK;
	}
	return false;
}

bool ZLot::orderedCellsAt(int level, const QPoint &point, QVector<const Cell*>& cells) const
{
	if (mLevelToTileLayers.contains(level) == false)
		return false;
	return mLevelToTileLayers[level]->orderedCellsAt(point, cells);
}

const ZTileLayerGroup *ZLot::tileLayersForLevel(int level) const
{
	if (mLevelToTileLayers.contains(level))
		return mLevelToTileLayers[level];
	return 0;
}

void ZLot::setLayerVisibility(const QString &name, bool visible) const
{
	if (!mLayersByName.contains(name))
		return;
	foreach (Layer *layer, mLayersByName[name])
		layer->setVisible(visible);
}

} // namespace Tiled