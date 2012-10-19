/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "mixedtilesetview.h"

#include "tile.h"
#include "tileset.h"

#include <QHeaderView>
#include <QPainter>

using namespace Tiled;
using namespace Internal;

/////

namespace Tiled {
namespace Internal {

class TileDelegate : public QAbstractItemDelegate
{
public:
    TileDelegate(MixedTilesetView *tilesetView, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(tilesetView)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    MixedTilesetView *mView;
};

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
    if (!m->tileAt(index)) {
        painter->drawLine(option.rect.topLeft(), option.rect.bottomRight());
        return;
    }

    // Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int extra = 0; // mView->drawGrid() ? 1 : 0;

#if 0
    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
#endif

    painter->drawPixmap(option.rect.adjusted(0, 0, -extra, -extra), tileImage);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(0, 0, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem & option,
                             const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
    if (!m->tileAt(index))
        return QSize(64,128);
    const Tileset *tileset = m->tileAt(index)->tileset();
    const qreal zoom = 1.0; //mView->zoomable()->scale();
    const int extra = 0; // mView->drawGrid() ? 1 : 0;
    return QSize(tileset->tileWidth() * zoom + extra,
                 tileset->tileHeight() * zoom + extra);
}

} // namepace Internal
} // namespace Tiled

/////

MixedTilesetView::MixedTilesetView(QWidget *parent) :
    QTableView(parent),
    mModel(new MixedTilesetModel(this))
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(new TileDelegate(this, this));
    setShowGrid(false);

    setSelectionMode(SingleSelection);

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

    setModel(mModel);
}

QSize MixedTilesetView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

/////

#define COLUMN_COUNT 8 // same as recent PZ tilesets

MixedTilesetModel::MixedTilesetModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int MixedTilesetModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    const int tiles = mTiles.count();
    const int columns = COLUMN_COUNT;

    int rows = 1;
    if (columns > 0) {
        rows = tiles / columns;
        if (tiles % columns > 0)
            ++rows;
    }

    return rows;
}

int MixedTilesetModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

QVariant MixedTilesetModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (Tile *tile = tileAt(index))
            return tile->image();
    }

    return QVariant();
}

QVariant MixedTilesetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

void MixedTilesetModel::setTiles(const QList<Tile *> &tiles)
{
    mTiles = tiles;
    reset();
}

Tile *MixedTilesetModel::tileAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;

    const int i = index.column() + index.row() * COLUMN_COUNT;
    return (i < mTiles.count()) ?  mTiles.at(i) : 0;
}
