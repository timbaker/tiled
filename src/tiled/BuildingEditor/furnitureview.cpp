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

#include "furnitureview.h"

#include "buildingtiles.h"
#include "furnituregroups.h"

#include "tile.h"
#include "tileset.h"
#include "zoomable.h"

#include <QDebug>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMimeData>
#include <QPainter>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Internal;

/////

namespace BuildingEditor {

static QSize isometricSize(int numX, int numY, int tileWidth, int tileHeight)
{
    // Map width and height contribute equally in both directions
    const int side = numX + numY;
    return QSize(side * tileWidth / 2, side * tileHeight / 2)
            + QSize(0, 128 - tileHeight);
}

class FurnitureTileDelegate : public QAbstractItemDelegate
{
public:
    FurnitureTileDelegate(FurnitureView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    QPointF pixelToTileCoords(int mapWidth, int mapHeight, qreal x, qreal y) const;
    QPoint dropCoords(const QPoint &dragPos, const QModelIndex &index);
    QPointF tileToPixelCoords(int mapWidth, int mapHeight, qreal x, qreal y) const;

    qreal scale() const
    { return mView->zoomable()->scale(); }

    void itemResized(const QModelIndex &index);

private:
    FurnitureView *mView;
};

void FurnitureTileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const FurnitureModel *m = static_cast<const FurnitureModel*>(index.model());

    FurnitureTile *ftile = m->tileAt(index);
    if (!ftile)
        return;

    FurnitureTile *original = ftile;
    if (!mView->acceptDrops())
        ftile = ftile->resolved();

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scale = this->scale();
    int extra = 2;

    qreal tileWidth = 64 * scale;
    qreal tileHeight = 32 * scale;
    qreal imageHeight = 128 * scale;
    QPointF tileMargins(0, imageHeight - tileHeight);
    int mapWidth = ftile->width();
    int mapHeight = ftile->height();
    if (mView->acceptDrops())
        mapWidth += 1, mapHeight += 1;

    // Draw the tile images.
    for (int y = 0; y < ftile->height(); y++) {
        for (int x = 0; x < ftile->width(); x++) {
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (BuildingTile *btile = ftile->tile(x, y)) {
                if (Tile *tile = BuildingTilesMgr::instance()->tileFor(btile)) { // FIXME: calc this elsewhere
                    QPointF p1 = tileToPixelCoords(mapWidth, mapHeight, x, y) + tileMargins + r.topLeft();
                    QRect r((p1 - QPointF(tileWidth/2, imageHeight - tileHeight)).toPoint(),
                            QSize(tileWidth, imageHeight));
                    painter->drawPixmap(r, tile->image());
                }
            }
        }
    }

    if (mView->acceptDrops()) {
        // Draw the tile grid.
        for (int y = 0; y < mapHeight; y++) {
            for (int x = 0; x < mapWidth; x++) {
                QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
                QPointF p1 = tileToPixelCoords(mapWidth, mapHeight, x, y) + tileMargins + r.topLeft();
                QPointF p2 = tileToPixelCoords(mapWidth, mapHeight, x+1, y) + tileMargins + r.topLeft();
                QPointF p3 = tileToPixelCoords(mapWidth, mapHeight, x, y+1) + tileMargins + r.topLeft();
                QPointF p4 = tileToPixelCoords(mapWidth, mapHeight, x+1, y+1) + tileMargins + r.topLeft();

                if (QPoint(x,y) == m->dropCoords() && index == m->dropIndex()) {
                    QBrush brush(Qt::gray, Qt::Dense4Pattern);
                    QPolygonF poly;
                    poly << p1 << p2 << p4 << p3 << p1;
                    QPainterPath path;
                    path.addPolygon(poly);
                    painter->fillPath(path, brush);
                    QPen pen;
                    pen.setWidth(3);
                    painter->setPen(pen);
                    painter->drawPath(path);
                    painter->setPen(QPen());
                }

                painter->drawLine(p1, p2); painter->drawLine(p3, p4);
                painter->drawLine(p1, p3); painter->drawLine(p2, p4);
            }
        }
    }

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }

    QRectF textRect;
    painter->drawText(option.rect.adjusted(extra, extra, 0, 0),
                      Qt::AlignTop | Qt::AlignLeft,
                      ftile->orientToString(),
                      &textRect);

    // Draw resolved-tiles indicator
    if (!mView->acceptDrops() && (ftile != original))
        painter->fillRect(textRect.right() + 3, textRect.center().y()-2, 4, 4, Qt::gray);
}

QSize FurnitureTileDelegate::sizeHint(const QStyleOptionViewItem & option,
                             const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    int width = 2, height = 2;
    const FurnitureModel *m = static_cast<const FurnitureModel*>(index.model());
    FurnitureTile *ftile = m->tileAt(index);
    int d = 0;
    if (ftile) {
        if (mView->acceptDrops()) {
            d = 1;
        } else {
            ftile = ftile->resolved();
        }
        width = ftile->width() + d;
        height = ftile->height() + d;
    }
    const qreal zoom = scale();
    const int extra = 2;
    int tileWidth = 64, tileHeight = 32;
    QSize size = isometricSize(width, height, tileWidth, tileHeight)
            * zoom + QSize(extra * 2, extra * 2);

    if (ftile) {
        width = mView->model()->maxTileSize(ftile->orient()).width() + d;
        height = mView->model()->maxTileSize(ftile->orient()).height() + d;
        QSize sizeMax = isometricSize(width, height, tileWidth, tileHeight)
                * zoom + QSize(extra * 2, extra * 2);
        size.setWidth(qMax(size.width(), sizeMax.width()));
    }

    return size;
}

QPoint FurnitureTileDelegate::dropCoords(const QPoint &dragPos,
                                        const QModelIndex &index)
{
    const FurnitureModel *m = static_cast<const FurnitureModel*>(index.model());
    FurnitureTile *ftile = m->tileAt(index);
    if (!mView->acceptDrops())
        ftile = ftile->resolved();
    QRect r = mView->visualRect(index);
    const int extra = 2;
    qreal x = dragPos.x() - r.left() - extra;
    qreal y = dragPos.y() - r.top() - extra - (128 - 32) * scale();
    QPointF tileCoords = pixelToTileCoords(ftile->width() + 1, ftile->height() + 1, x, y);
    if (tileCoords.x() < 0 || tileCoords.y() < 0)
        return QPoint(-1, -1);
    return QPoint(tileCoords.x(), tileCoords.y()); // don't use toPoint, it rounds up
}

QPointF FurnitureTileDelegate::pixelToTileCoords(int mapWidth, int mapHeight, qreal x, qreal y) const
{
    Q_UNUSED(mapWidth)
    const int tileWidth = 64 * scale();
    const int tileHeight = 32 * scale();
    const qreal ratio = (qreal) tileWidth / tileHeight;

    x -= mapHeight * tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QPointF FurnitureTileDelegate::tileToPixelCoords(int mapWidth, int mapHeight, qreal x, qreal y) const
{
    Q_UNUSED(mapWidth)
    const int tileWidth = 64 * scale();
    const int tileHeight = 32 * scale();
    const int originX = mapHeight * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

void FurnitureTileDelegate::itemResized(const QModelIndex &index)
{
    emit sizeHintChanged(index);
}

} // namespace BuildingEditor

/////

// This constructor is for the benefit of QtDesigner
FurnitureView::FurnitureView(QWidget *parent) :
    QTableView(parent),
    mModel(new FurnitureModel(this)),
    mDelegate(new FurnitureTileDelegate(this, this)),
    mZoomable(new Zoomable(this))
{
    init();
}

FurnitureView::FurnitureView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new FurnitureModel(this)),
    mDelegate(new FurnitureTileDelegate(this, this)),
    mZoomable(zoomable)
{
    init();
}

QSize FurnitureView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void FurnitureView::dragMoveEvent(QDragMoveEvent *event)
{
    QAbstractItemView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        if (FurnitureTile *ftile = model()->tileAt(index)) {
            QPoint dropCoords = mDelegate->dropCoords(event->pos(), index);
            if (!QRect(0, 0, ftile->width() + 1, ftile->height() + 1).contains(dropCoords)) {
                model()->setDropCoords(QPoint(-1,-1), QModelIndex());
//                update(index);
                event->ignore();
                return;
            }
            model()->setDropCoords(dropCoords, index);
        } else {
            model()->setDropCoords(QPoint(-1,-1), QModelIndex());
            event->ignore();
            return;
        }
    }
}

void FurnitureView::dragLeaveEvent(QDragLeaveEvent *event)
{
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
    QAbstractItemView::dragLeaveEvent(event);
}

void FurnitureView::dropEvent(QDropEvent *event)
{
    QAbstractItemView::dropEvent(event);
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
}

void FurnitureView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void FurnitureView::furnitureTileResized(FurnitureTile *ftile)
{
    model()->calcMaxTileSize();
    mDelegate->itemResized(model()->index(ftile));
}

void FurnitureView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void FurnitureView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setItemDelegate(mDelegate);
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

#define COLUMN_COUNT 4 // W, N, E, S

FurnitureModel::FurnitureModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

int FurnitureModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    const int tiles = mTiles.count();
    const int columns = columnCount();

    int rows = 1;
    if (columns > 0) {
        rows = tiles / columns;
        if (tiles % columns > 0)
            ++rows;
    }

    return rows;
}

int FurnitureModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

Qt::ItemFlags FurnitureModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

QVariant FurnitureModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return QVariant();
}

QVariant FurnitureModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex FurnitureModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    int tileIndex = row * columnCount() + column;
    if (tileIndex >= mTiles.count())
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex FurnitureModel::index(FurnitureTile *tile)
{
    int tileIndex = mTiles.indexOf(tile);
    if (tileIndex != -1)
        return index(tileIndex / columnCount(), tileIndex % columnCount());
    return QModelIndex();
}

QString FurnitureModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList FurnitureModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

bool FurnitureModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     FurnitureTile *tile = tileAt(index);
     if (!tile)
         return false;

     if (!QRect(0, 0, tile->width() + 1, tile->height() + 1).contains(mDropCoords))
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingTilesMgr::nameForTile(tilesetName, tileId);
         emit furnitureTileDropped(tile, mDropCoords.x(), mDropCoords.y(), tileName);
     }

     return true;
}

void FurnitureModel::setTiles(const QList<FurnitureTiles *> &tilesList)
{
    mTiles.clear();
    foreach (FurnitureTiles *ftiles, tilesList) {
        mTiles += ftiles->tile(FurnitureTile::FurnitureW);
        mTiles += ftiles->tile(FurnitureTile::FurnitureN);
        mTiles += ftiles->tile(FurnitureTile::FurnitureE);
        mTiles += ftiles->tile(FurnitureTile::FurnitureS);
        if (ftiles->hasCorners()) {
            mTiles += ftiles->tile(FurnitureTile::FurnitureSW);
            mTiles += ftiles->tile(FurnitureTile::FurnitureNW);
            mTiles += ftiles->tile(FurnitureTile::FurnitureNE);
            mTiles += ftiles->tile(FurnitureTile::FurnitureSE);
        }
    }
    calcMaxTileSize();
    reset();
}

FurnitureTile *FurnitureModel::tileAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    int i = index.column() + index.row() * columnCount();
    return mTiles.at(i);
}

void FurnitureModel::toggleCorners(FurnitureTiles *ftiles)
{
    int n = mTiles.indexOf(ftiles->tile(FurnitureTile::FurnitureW));
    int row = n / columnCount() + 1;
    if (ftiles->hasCorners()) {
        beginInsertRows(QModelIndex(), row, row);
        mTiles.insert(n + 4, ftiles->tile(FurnitureTile::FurnitureSW));
        mTiles.insert(n + 5, ftiles->tile(FurnitureTile::FurnitureNW));
        mTiles.insert(n + 6, ftiles->tile(FurnitureTile::FurnitureNE));
        mTiles.insert(n + 7, ftiles->tile(FurnitureTile::FurnitureSE));
        calcMaxTileSize();
        endInsertRows();
    } else {
        beginRemoveRows(QModelIndex(), row, row);
        mTiles.takeAt(n + 4);
        mTiles.takeAt(n + 4);
        mTiles.takeAt(n + 4);
        mTiles.takeAt(n + 4);
        calcMaxTileSize();
        endRemoveRows();
    }
}

void FurnitureModel::removeTiles(FurnitureTiles *ftiles)
{
    QModelIndex index = this->index(ftiles->tile(FurnitureTile::FurnitureW));
    int row = index.row();
    beginRemoveRows(QModelIndex(), row, row + (ftiles->hasCorners() ? 1 : 0));
    int i = mTiles.indexOf(ftiles->tile(FurnitureTile::FurnitureW));
    mTiles.takeAt(i);
    mTiles.takeAt(i);
    mTiles.takeAt(i);
    mTiles.takeAt(i);
    if (ftiles->hasCorners()) {
        mTiles.takeAt(i);
        mTiles.takeAt(i);
        mTiles.takeAt(i);
        mTiles.takeAt(i);
    }
    calcMaxTileSize();
    endRemoveRows();
    return;
}

void FurnitureModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    reset();
}

void FurnitureModel::calcMaxTileSize()
{
    mMaxTileSize.fill(QSize(1, 1), FurnitureTile::OrientCount);
    foreach (FurnitureTile *ftile, mTiles) {
        int n = ftile->orient();
        mMaxTileSize[n].setWidth( qMax(mMaxTileSize[n].width(), ftile->width()) );
        mMaxTileSize[n].setHeight( qMax(mMaxTileSize[n].height(), ftile->height()) );
    }
}
