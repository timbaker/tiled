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
#include "zoomable.h"

#include <QHeaderView>
#include <QMimeData>
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

    QString tilesetName = m->headerAt(index);
    if (!tilesetName.isEmpty()) {
        if (index.row() > 0) {
            painter->setPen(Qt::darkGray);
            painter->drawLine(option.rect.topLeft(), option.rect.topRight());
            painter->setPen(Qt::black);
        }
        // One slice of the tileset name is drawn in each column.
        if (index.column() == 0)
            painter->drawText(option.rect.adjusted(2, 2, 0, 0), tilesetName);
        else {
            QRect r = option.rect.adjusted(-index.column() * option.rect.width(),
                                           0, 0, 0);
            painter->save();
            painter->setClipRect(option.rect);
            painter->drawText(r.adjusted(2, 2, 0, 0), tilesetName);
            painter->restore();
        }
        return;
    }

    if (!m->tileAt(index)) {
#if 0
        painter->drawLine(option.rect.topLeft(), option.rect.bottomRight());
        painter->drawLine(option.rect.topRight(), option.rect.bottomLeft());
#endif
        return;
    }

    QRect r = m->categoryBounds(m->tileAt(index));
    if (r.isValid()) {
        int left = option.rect.left();
        int right = option.rect.right();
        int top = option.rect.top();
        int bottom = option.rect.bottom();
        if (index.column() == r.left())
            ++left;
        if (index.column() == r.right())
            --right;
        if (index.row() == r.top())
            ++top;
        if (index.row() == r.bottom())
            --bottom;

        painter->fillRect(left, top, right-left+1, bottom-top+1,
                          qRgb(220, 220, 220));

        painter->setPen(Qt::darkGray);
        if (index.column() == r.left())
            painter->drawLine(left, top, left, bottom);
        if (index.column() == r.right())
            painter->drawLine(right, top, right, bottom);
        if (index.row() == r.top())
            painter->drawLine(left, top, right, top);
        if (index.row() == r.bottom())
            painter->drawLine(left, bottom, right, bottom);
    }

    // Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int extra = 2;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    painter->drawPixmap(option.rect.adjusted(extra, extra, -extra, -extra), tileImage);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize TileDelegate::sizeHint(const QStyleOptionViewItem & option,
                             const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
    const qreal zoom = mView->zoomable()->scale();
    const int extra = 4;
    if (m->headerAt(index).length())
        return QSize(64 * zoom + extra, option.fontMetrics.lineSpacing() + 2);
    if (!m->tileAt(index))
        return QSize(64 * zoom + extra, 128 * zoom + extra);
    const Tileset *tileset = m->tileAt(index)->tileset();
    return QSize(tileset->tileWidth() * zoom + extra,
                 tileset->tileHeight() * zoom + extra);
}

} // namepace Internal
} // namespace Tiled

/////

// This constructor is for the benefit of QtDesigner
MixedTilesetView::MixedTilesetView(QWidget *parent) :
    QTableView(parent),
    mModel(new MixedTilesetModel(this)),
    mZoomable(new Zoomable(this))
{
    init();
}

MixedTilesetView::MixedTilesetView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new MixedTilesetModel(this)),
    mZoomable(zoomable)
{
    init();
}

QSize MixedTilesetView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void MixedTilesetView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void MixedTilesetView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void MixedTilesetView::init()
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

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

/////

#define COLUMN_COUNT 8 // same as recent PZ tilesets

MixedTilesetModel::MixedTilesetModel(QObject *parent) :
    QAbstractListModel(parent),
    mTileset(0),
    mShowHeaders(true)
{
}

int MixedTilesetModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    const int tiles = mItems.count();
    const int columns = columnCount();

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
    return parent.isValid() ? 0 : (mTileset ? mTileset->columnCount() : COLUMN_COUNT);
}

Qt::ItemFlags MixedTilesetModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDragEnabled;
    return flags;
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
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex MixedTilesetModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    if (row * columnCount() + column >= mItems.count())
        return QModelIndex();

    Item *item = mItems.at(row * columnCount() + column);
    if (!item->mTile && item->mTilesetName.isEmpty())
        return QModelIndex();
    return createIndex(row, column, item);
}

QModelIndex MixedTilesetModel::index(Tile *tile)
{
    int tileIndex = mItems.indexOf(toItem(tile));
    if (tileIndex != -1)
        return index(tileIndex / columnCount(), tileIndex % columnCount());
    return QModelIndex();
}

QString MixedTilesetModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList MixedTilesetModel::mimeTypes() const
{
    QStringList types;
    types << mMimeType;
    return types;}

QMimeData *MixedTilesetModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    foreach (const QModelIndex &index, indexes) {
        if (Tile *tile = tileAt(index)) {
            stream << tile->tileset()->name();
            stream << tile->id();
        }
    }

    mimeData->setData(mMimeType, encodedData);
    return mimeData;
}

void MixedTilesetModel::setTiles(const QList<Tile *> &tiles)
{
    mTiles = tiles;
    mTileset = 0;
    mCategoryBounds.clear();

    qDeleteAll(mItems);
    mItems.clear();
    QString tilesetName;
    foreach (Tile *tile, mTiles) {
        if (mShowHeaders && tile->tileset()->name() != tilesetName) {
            while (mItems.count() % columnCount())
                mItems += new Item(); // filler after previous tile
            tilesetName = tile->tileset()->name();
            for (int i = 0; i < columnCount(); i++)
                mItems += new Item(tilesetName);
        }
        mItems += new Item(tile);
    }

    reset();
}

void MixedTilesetModel::setTileset(Tileset *tileset)
{
    mTiles.clear();
    mTileset = tileset;
    for (int i = 0; i < mTileset->tileCount(); i++)
        mTiles += mTileset->tileAt(i);
    mCategoryBounds.clear();

    qDeleteAll(mItems);
    mItems.clear();
    QString tilesetName;
    foreach (Tile *tile, mTiles) {
        if (tile->tileset()->name() != tilesetName) {
            while (mItems.count() % columnCount())
                mItems += new Item(); // filler after previous tile
            tilesetName = tile->tileset()->name();
            for (int i = 0; i < columnCount(); i++)
                mItems += new Item(tilesetName);
        }
        mItems += new Item(tile);
    }

    reset();
}

Tile *MixedTilesetModel::tileAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mTile;
    return 0;
}

QString MixedTilesetModel::headerAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mTilesetName;
    return QString();
}

void MixedTilesetModel::setCategoryBounds(Tile *tile, const QRect &bounds)
{
    QModelIndex index = this->index(tile);
    QRect viewBounds = bounds.translated(index.column(), index.row());
    for (int x = 0; x < bounds.width(); x++)
        for (int y = 0; y < bounds.height(); y++) {
            if (Tile *tile2 = tileAt(this->index(index.row() + y, index.column() + x)))
                mCategoryBounds[tile2] = viewBounds;
        }
}

QRect MixedTilesetModel::categoryBounds(Tile *tile) const
{
    if (mCategoryBounds.contains(tile))
        return mCategoryBounds[tile];
    return QRect();
}

void MixedTilesetModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    reset();
}

void MixedTilesetModel::setShowHeaders(bool show)
{
    mShowHeaders = show;
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(Tile *tile) const
{
    foreach (Item *item, mItems)
        if (item->mTile == tile)
            return item;
    return 0;
}
