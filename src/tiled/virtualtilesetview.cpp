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

#include "virtualtilesetview.h"

#include "tilesetmanager.h"
#include "tileshapeeditor.h"
#include "virtualtileset.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QApplication>
#include <QDebug>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMimeData>
#include <QPainter>
#include <QStyleOption>

using namespace Tiled;
using namespace Internal;

/////

namespace Tiled {
namespace Internal {

class VirtualTilesetDelegate : public QAbstractItemDelegate
{
public:
    VirtualTilesetDelegate(VirtualTilesetView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    qreal scale() const
    { return mView->zoomable()->scale(); }

private:
    VirtualTilesetView *mView;
};

}
}

void VirtualTilesetDelegate::paint(QPainter *painter,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QImage tileImage = display.value<QImage>();
    const int extra = 1;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    if (tileImage.isNull())
        painter->fillRect(option.rect.adjusted(0, 0, -extra, -extra),
                          QColor(240, 240, 240));
    else
        painter->drawImage(option.rect.adjusted(0, 0, -extra, -extra), tileImage);

    // Grid
    painter->fillRect(option.rect.right(), option.rect.top(), 1, option.rect.height(), Qt::lightGray);
    painter->fillRect(option.rect.left(), option.rect.bottom(), option.rect.width(), 1, Qt::lightGray);

    const VirtualTilesetModel *m = static_cast<const VirtualTilesetModel*>(index.model());
    VirtualTile *vtile = m->tileAt(index);
    if (vtile && vtile->shape() && vtile->shape()->mSameAs)
        painter->fillRect(option.rect.right() - 4, option.rect.top() + 2, 2, 2, Qt::gray);

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(0, 0, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
}

QSize VirtualTilesetDelegate::sizeHint(const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    Q_UNUSED(option)
    const VirtualTilesetModel *m = static_cast<const VirtualTilesetModel*>(index.model());
    if (!m->tileAt(index))
        return QSize();
    const qreal scale = this->scale();
    const int extra = 1;
    return QSize(64 * scale + extra, 128 * scale + extra);

}

/////

VirtualTilesetModel::VirtualTilesetModel(QObject *parent) :
    QAbstractTableModel(parent),
    mTileset(0),
    mShowDiskImage(false)
{
}

VirtualTilesetModel::~VirtualTilesetModel()
{
    qDeleteAll(mItems);
}

int VirtualTilesetModel::rowCount(const QModelIndex &parent) const
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

int VirtualTilesetModel::columnCount(const QModelIndex &parent) const
{
    return (parent.isValid() || !mTileset) ? 0 : mTileset->columnCount();
}

Qt::ItemFlags VirtualTilesetModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractTableModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else {
        flags |= Qt::ItemIsDropEnabled;
        flags |= Qt::ItemIsDragEnabled;
    }
    return flags;
}

QVariant VirtualTilesetModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (Item *item = toItem(index)) {
            if (mShowDiskImage) {
                if (!mDiskImage.isNull() && item->mDiskImage.isNull())
                    item->mDiskImage = mDiskImage.copy(item->mTile->x() * 64,
                                                       item->mTile->y() * 128,
                                                       64, 128);
                return item->mDiskImage;
            }
            return item->mTile->image();
        }
    }
    if (role == Qt::ToolTipRole) {
        if (VirtualTile *vtile = tileAt(index))
            return vtile->shape() ? vtile->shape()->name() : QString();
    }
    return QVariant();
}

QVariant VirtualTilesetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex VirtualTilesetModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    int tileIndex = row * columnCount() + column;
    if (tileIndex >= mItems.count())
        return QModelIndex();

    return createIndex(row, column, mItems.at(tileIndex));
}

QModelIndex VirtualTilesetModel::index(VirtualTile *vtile) const
{
    int tileIndex = mItems.indexOf(toItem(vtile));
    if (tileIndex != -1)
        return index(tileIndex / columnCount(), tileIndex % columnCount());
    return QModelIndex();
}

QString VirtualTilesetModel::mMimeType(QLatin1String("application/x-tilezed-vtiles"));

QStringList VirtualTilesetModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

QMimeData *VirtualTilesetModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);
#if 0
    QRect r(indexes.first().column(), indexes.first().row(), 1, 1);
    foreach (const QModelIndex &index, indexes)
        r |= QRect(index.column(), index.row(), 1, 1);
#endif
    foreach (const QModelIndex &index, indexes) {
        if (VirtualTile *vtile = tileAt(index)) {
            stream << (vtile->x() - mClickedIndex.column()) << (vtile->y() - mClickedIndex.row());
            stream << vtile->imageSource();
            stream << vtile->srcX() << vtile->srcY();
            stream << (vtile->shape() ? vtile->shape()->name() : QString());
        }
    }

    mimeData->setData(mMimeType, encodedData);
    return mimeData;
}

bool VirtualTilesetModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                       int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     VirtualTile *vtile = tileAt(index);
     if (!vtile)
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     emit beginDropTiles();

     foreach (VirtualTile *vtile, mMovedTiles)
         emit tileDropped(vtile, QString(), -1, -1, 0);

     while (!stream.atEnd()) {
         int col, row;
         stream >> col >> row;
         QString imageSource;
         stream >> imageSource;
         int srcX, srcY;
         stream >> srcX >> srcY;
         QString shapeName;
         stream >> shapeName;
         TileShape *shape = VirtualTilesetMgr::instance().tileShape(shapeName);
         if (VirtualTile *vtile = tileAt(this->index(index.row() + row,
                                                     index.column() + col)))
            emit tileDropped(vtile, imageSource, srcX, srcY, shape);
     }

     emit endDropTiles();

     return true;
}

void VirtualTilesetModel::clear()
{
    setTileset(0);
}

void VirtualTilesetModel::setTileset(VirtualTileset *vts)
{
    beginResetModel();

    qDeleteAll(mItems);
    mItems.clear();
    mTileset = vts;

    if (mTileset) {
        foreach (VirtualTile *vtile, mTileset->tiles()) {
            mItems += new Item(vtile);
        }
    }

    endResetModel();
}

VirtualTile *VirtualTilesetModel::tileAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mTile;
    return 0;
}

void VirtualTilesetModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void VirtualTilesetModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

void VirtualTilesetModel::redisplay(VirtualTile *vtile)
{
    QModelIndex index = this->index(vtile);
    if (index.isValid())
        emit dataChanged(index, index);
}

void VirtualTilesetModel::setDiskImage(const QImage &image)
{
    mDiskImage = image;
    if (mShowDiskImage)
        redisplay();
}

void VirtualTilesetModel::setShowDiskImage(bool show)
{
    if (show != mShowDiskImage) {
        mShowDiskImage = show;
        redisplay();
    }
}

VirtualTilesetModel::Item *VirtualTilesetModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

VirtualTilesetModel::Item *VirtualTilesetModel::toItem(VirtualTile *vtile) const
{
    foreach (Item *item, mItems)
        if (item->mTile == vtile)
            return item;
    return 0;
}

/////

// This constructor is for the benefit of QtDesigner
VirtualTilesetView::VirtualTilesetView(QWidget *parent) :
    QTableView(parent),
    mModel(new VirtualTilesetModel(this)),
    mDelegate(new VirtualTilesetDelegate(this, this)),
    mZoomable(new Zoomable(this))
{
    init();
}

QSize VirtualTilesetView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void VirtualTilesetView::mousePressEvent(QMouseEvent *event)
{
    mClickedIndex = indexAt(event->pos());
    model()->setClickedIndex(mClickedIndex);

    // Only drag already-selected tiles.
    setDragEnabled(selectionModel()->isSelected(mClickedIndex));

    QTableView::mousePressEvent(event);
}

void VirtualTilesetView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void VirtualTilesetView::dragMoveEvent(QDragMoveEvent *event)
{
    QTableView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        if (VirtualTile *vtile = model()->tileAt(index)) {
#if 0
            QPoint dropCoords = mDelegate->dropCoords(event->pos(), index);
            if (!QRect(0, 0, ftile->width() + 1, ftile->height() + 1).contains(dropCoords)) {
                model()->setDropCoords(QPoint(-1,-1), QModelIndex());
//                update(index);
                event->ignore();
                return;
            }
            model()->setDropCoords(dropCoords, index);
#endif
        } else {
#if 0
            model()->setDropCoords(QPoint(-1,-1), QModelIndex());
            event->ignore();
            return;
#endif
        }
    }
}

void VirtualTilesetView::dragLeaveEvent(QDragLeaveEvent *event)
{
#if 0
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
#endif
    QTableView::dragLeaveEvent(event);
}

void VirtualTilesetView::dropEvent(QDropEvent *event)
{
    // When moving tiles within this widget, clear the dragged source tiles.
    QList<VirtualTile*> movedTiles;
    if (event->source() == this) {
        foreach (QModelIndex index, selectionModel()->selectedIndexes()) {
            if (VirtualTile *vtile = model()->tileAt(index))
                movedTiles += vtile;
        }
    }
    model()->setMovedTiles(movedTiles);

    QTableView::dropEvent(event);

    // Select the tiles in their new position.
    // FIXME: If this drop is undone, put the selection back how it was.
    if (event->source() == this) {
        QModelIndex dropIndex = indexAt(event->pos());
        QModelIndexList selected = selectionModel()->selectedIndexes();
        clearSelection();
        foreach (QModelIndex index, selected) {
            index = this->model()->index(index.row() + dropIndex.row() - mClickedIndex.row(),
                                         index.column() + dropIndex.column() - mClickedIndex.column());
            if (index.isValid())
                selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }

#if 0
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
#endif
}

void VirtualTilesetView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void VirtualTilesetView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void VirtualTilesetView::setTileset(VirtualTileset *vts)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setTileset(vts);
}

void VirtualTilesetView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void VirtualTilesetView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(mDelegate);
    setShowGrid(false);

    setSelectionMode(SingleSelection);

    QHeaderView *header = horizontalHeader();
    header->hide();
#if QT_VERSION >= 0x050000
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    header->setResizeMode(QHeaderView::ResizeToContents);
#endif
    header->setMinimumSectionSize(1);

    header = verticalHeader();
    header->hide();
#if QT_VERSION >= 0x050000
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    header->setResizeMode(QHeaderView::ResizeToContents);
#endif
    header->setMinimumSectionSize(1);

    // Hardcode this view on 'left to right' since it doesn't work properly
    // for 'right to left' languages.
    setLayoutDirection(Qt::LeftToRight);

    setModel(mModel);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}
