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

namespace {

class TileDelegate : public QAbstractItemDelegate
{
public:
    TileDelegate(ZTilesetThumbView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    ZTilesetThumbView *mView;
};

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
	const QFontMetrics fm = painter->fontMetrics();
//	const ZTilesetThumbModel *model = static_cast<const ZTilesetThumbModel*>(index.model());

	// Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();

	const int dx1 = (option.rect.width() - tileImage.width()) / 2;
	const int dx2 = option.rect.width() - tileImage.width() - dx1;
	painter->drawPixmap(option.rect.adjusted(dx1, 0, -dx2, 0 - fm.lineSpacing()), tileImage);

    const QVariant decoration = index.model()->data(index, Qt::DecorationRole);
	QString tilesetName = decoration.toString();

    QString name = tilesetName; // fm.elidedText(tilesetName, Qt::ElideRight, option.rect.width());
	painter->drawText(option.rect.left(), option.rect.bottom() - fm.lineSpacing(), option.rect.width(), fm.lineSpacing(), Qt::AlignHCenter, name);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(0, 0, 0, 0),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const ZTilesetThumbModel *m = static_cast<const ZTilesetThumbModel*>(index.model());
    const Tileset *tileset = m->tilesetAt(index);
	const QFontMetrics fm(option.font);
	const int labelWidth = mView->contentWidth(); // fm.boundingRect(tileset->name()).width();
	const int labelHeight = fm.lineSpacing();
    return QSize(qMax(tileset->tileWidth(), labelWidth),
                 tileset->tileHeight() + labelHeight);
}

} // anonymous namespace

ZTilesetThumbView::ZTilesetThumbView(QWidget *parent)
    : QTableView(parent)
	, mMapDocument(0)
{
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new TileDelegate(this, this));
    setShowGrid(false);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

	setModel(mModel = new ZTilesetThumbModel(this));

    QHeaderView *header = horizontalHeader();
    header->hide();
    header->setResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(1);

    header = verticalHeader();
    header->hide();
    header->setResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(1);

    // Hardcode this view on 'left to right' since it doesn't work properly
    // for 'right to left' languages.
    setLayoutDirection(Qt::LeftToRight);
}

QSize ZTilesetThumbView::sizeHint() const
{
	if (mMapDocument) {
		int sbw = verticalScrollBar()->sizeHint().width();
		int fudge = 4; // borders???
		return QSize(contentWidth()+sbw+fudge, 100);
	}
    return QSize(130, 100);
}

/**
 * Allow changing tile properties through a context menu.
 */
void ZTilesetThumbView::contextMenuEvent(QContextMenuEvent *event)
{
#if 0
    const QModelIndex index = indexAt(event->pos());
    const TilesetModel *m = tilesetModel();
    Tile *tile = m->tileAt(index);

    const bool isExternal = m->tileset()->isExternal();
    QMenu menu;

    QIcon propIcon(QLatin1String(":images/16x16/document-properties.png"));

    if (tile) {
        // Select this tile to make sure it is clear that only the properties
        // of a single tile are being edited.
        selectionModel()->setCurrentIndex(index,
                                          QItemSelectionModel::SelectCurrent |
                                          QItemSelectionModel::Clear);

        QAction *tileProperties = menu.addAction(propIcon,
                                                 tr("Tile &Properties..."));
        tileProperties->setEnabled(!isExternal);
        Utils::setThemeIcon(tileProperties, "document-properties");
        menu.addSeparator();

        connect(tileProperties, SIGNAL(triggered()),
                SLOT(editTileProperties()));
    }


    menu.addSeparator();
    QAction *toggleGrid = menu.addAction(tr("Show &Grid"));
    toggleGrid->setCheckable(true);
    toggleGrid->setChecked(mDrawGrid);

    Preferences *prefs = Preferences::instance();
    connect(toggleGrid, SIGNAL(toggled(bool)),
            prefs, SLOT(setShowTilesetGrid(bool)));

    menu.exec(event->globalPos());
#endif
}

void ZTilesetThumbView::setMapDocument(MapDocument *mapDoc)
{
	if (mMapDocument == mapDoc)
		return;
	mMapDocument = mapDoc;
	if (mMapDocument) {
		QFontMetrics fm = fontMetrics(); // FIXME: same font used by QPainter?
		int width = 64;
		foreach (Tileset *ts, mMapDocument->map()->tilesets())
			width = qMax(width, fm.width(ts->name()));
		mContentWidth = width;
	}
	model()->setMapDocument(mapDoc);
	updateGeometry();
}