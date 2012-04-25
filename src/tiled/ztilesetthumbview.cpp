/*
 * ztilesetthumbview.cpp
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

#include "ztilesetthumbview.hpp"

#include "map.h"
#include "mapdocument.h"
#include "preferences.h"
#include "propertiesdialog.h"
#include "tmxmapwriter.h"
#include "tile.h"
#include "tileset.h"
#include "tilesetmodel.h"
#include "utils.h"
#include "ztilesetthumbmodel.hpp"

#include <QAbstractItemDelegate>
#include <QCoreApplication>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QScrollbar>
#include <QUndoCommand>

using namespace Tiled;
using namespace Tiled::Internal;

ZTilesetThumbView::ZTilesetThumbView(QWidget *parent)
    : QListView(parent)
	, mMapDocument(0)
{
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
	setSelectionMode(QAbstractItemView::SingleSelection);

	setModel(mModel = new ZTilesetThumbModel(this));
}

QSize ZTilesetThumbView::sizeHint() const
{
	if (mMapDocument) {
		int sbw = verticalScrollBar()->sizeHint().width();
		int fudge = 16;
		return QSize(contentWidth() + sbw + fudge, 100);
	}
    return QSize(130, 100);
}

void ZTilesetThumbView::contextMenuEvent(QContextMenuEvent *event)
{
}

void ZTilesetThumbView::setMapDocument(MapDocument *mapDoc)
{
	if (mMapDocument == mapDoc)
		return;
	mMapDocument = mapDoc;
	model()->setMapDocument(mapDoc);

	if (mMapDocument) {
		QFontMetrics fm = fontMetrics(); // FIXME: same font used by QPainter?
		int width = 64;
		foreach (Tileset *ts, mMapDocument->map()->tilesets()) {
			width = qMax(width, fm.width(ts->name()));
		}
		mContentWidth = width;
	}

	updateGeometry();
}