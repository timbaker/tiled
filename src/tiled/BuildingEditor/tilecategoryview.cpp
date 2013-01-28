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

#include "tilecategoryview.h"

#include "buildingtiles.h"
#include "furnituregroups.h"

#include "tile.h"
#include "tileset.h"
#include "zoomable.h"

#include <QApplication>
#include <QDebug>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMimeData>
#include <QPainter>
#include <QStyleOption>

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

class BuildingEntryDelegate : public QAbstractItemDelegate
{
public:
    BuildingEntryDelegate(TileCategoryView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    QPointF pixelToTileCoords(qreal x, qreal y) const;
    QPoint dropCoords(const QPoint &dragPos, const QModelIndex &index);
    QPointF tileToPixelCoords(qreal x, qreal y) const;

    qreal scale() const
    { return mView->zoomable()->scale(); }

private:
    TileCategoryView *mView;
};

void BuildingEntryDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const TileCategoryModel *m = static_cast<const TileCategoryModel*>(index.model());

    BuildingTileEntry *entry = m->entryAt(index);
    if (!entry)
        return;
    int e = m->enumAt(index);

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scale = this->scale();
    int extra = 2;

    qreal tileWidth = 64 * scale;
    qreal tileHeight = 32 * scale;
    qreal imageHeight = 128 * scale;
    QPointF tileMargins(0, imageHeight - tileHeight);

    QRect r = option.rect.adjusted(extra, extra, -extra, -extra);

    // Draw the shadow image.
    {
        QPointF offset = entry->offset(e);
        QPointF p1 = tileToPixelCoords(0, 0) + tileMargins + r.topLeft();
        QRect target((p1 - QPointF(tileWidth/2, imageHeight - tileHeight)).toPoint(),
                QSize(tileWidth, imageHeight));
        int row = index.row() % m->shadowImageRows();
        QRect source(index.column() * 64, row * 128,
                     64, 128);
        painter->drawImage(target, entry->category()->shadowImage(), source);
    }

    // Draw the tile image.
    if (BuildingTile *btile = entry->tile(e)) {
        if (!btile->isNone()) {
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(btile)) { // FIXME: calc this elsewhere
                QPointF offset = entry->offset(e);
                QPointF p1 = tileToPixelCoords(offset.x(), offset.y()) + tileMargins + r.topLeft();
                QRect target((p1 - QPointF(tileWidth/2, imageHeight - tileHeight)).toPoint(),
                        QSize(tileWidth, imageHeight));
                QRegion clipRgn = painter->clipRegion();
                bool hasClipping = painter->hasClipping();
                painter->setClipRect(r);
                painter->drawImage(target, tile->image());
                painter->setClipRegion(clipRgn, hasClipping ? Qt::ReplaceClip : Qt::NoClip);
            }
        }
    }

    // Divider line between entries
    if (index.row() % m->shadowImageRows() == m->shadowImageRows() - 1) {
        painter->setPen(Qt::gray);
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    }

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }

    // Focus rect around 'current' item
    if (option.state & QStyle::State_HasFocus) {
        QStyleOptionFocusRect o;
        o.QStyleOption::operator=(option);
        o.rect = option.rect.adjusted(1,1,-1,-1);
        o.state |= QStyle::State_KeyboardFocusChange;
        o.state |= QStyle::State_Item;
        QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
                                  ? QPalette::Normal : QPalette::Disabled;
        o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
                                                 ? QPalette::Highlight : QPalette::Window);
        const QWidget *widget = 0/*d->widget(option)*/;
        QStyle *style = /*widget ? widget->style() :*/ QApplication::style();
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, widget);
    }
}

QSize BuildingEntryDelegate::sizeHint(const QStyleOptionViewItem & option,
                             const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
//    const TileCategoryModel *m = static_cast<const TileCategoryModel*>(index.model());
    const qreal zoom = scale();
    const int extra = 2;
//    FurnitureTile *tile = m->tileAt(index);
    int tileWidth = 64, tileHeight = 32;
    return isometricSize(1, 1, tileWidth, tileHeight) * zoom + QSize(extra * 2, extra * 2);
}

QPointF BuildingEntryDelegate::pixelToTileCoords(qreal x, qreal y) const
{
    const int tileWidth = 64 * scale();
    const int tileHeight = 32 * scale();
    const qreal ratio = (qreal) tileWidth / tileHeight;

    const int mapHeight = 1;
    x -= mapHeight * tileWidth / 2;
    const qreal mx = y + (x / ratio);
    const qreal my = y - (x / ratio);

    return QPointF(mx / tileHeight,
                   my / tileHeight);
}

QPointF BuildingEntryDelegate::tileToPixelCoords(qreal x, qreal y) const
{
    const int tileWidth = 64 * scale();
    const int tileHeight = 32 * scale();
    const int mapHeight = 1;
    const int originX = mapHeight * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

} // namespace BuildingEditor

/////

// This constructor is for the benefit of QtDesigner
TileCategoryView::TileCategoryView(QWidget *parent) :
    QTableView(parent),
    mModel(new TileCategoryModel(this)),
    mDelegate(new BuildingEntryDelegate(this, this)),
    mZoomable(new Zoomable(this))
{
    init();
}

TileCategoryView::TileCategoryView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new TileCategoryModel(this)),
    mDelegate(new BuildingEntryDelegate(this, this)),
    mZoomable(zoomable)
{
    init();
}

QSize TileCategoryView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void TileCategoryView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void TileCategoryView::dragMoveEvent(QDragMoveEvent *event)
{
    QAbstractItemView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        if (model()->entryAt(index)) {
            // nothing
        } else {
            event->ignore();
            return;
        }
    }
}

void TileCategoryView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QAbstractItemView::dragLeaveEvent(event);
}

void TileCategoryView::dropEvent(QDropEvent *event)
{
    QAbstractItemView::dropEvent(event);
}

void TileCategoryView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void TileCategoryView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void TileCategoryView::setCategory(BuildingTileCategory *category)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setCategory(category);
}

void TileCategoryView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void TileCategoryView::init()
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

TileCategoryModel::TileCategoryModel(QObject *parent) :
    QAbstractListModel(parent),
    mCategory(0)
{
}

int TileCategoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !mCategory)
        return 0;

    const int tiles = mCategory->entryCount() * itemsPerEntry();
    const int columns = columnCount();

    int rows = 1;
    if (columns > 0) {
        rows = tiles / columns;
        if (tiles % columns > 0)
            ++rows;
    }

    return rows;
}

int TileCategoryModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : shadowImageColumns();
}

Qt::ItemFlags TileCategoryModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!entryAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

QVariant TileCategoryModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return QVariant();
}

QVariant TileCategoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex TileCategoryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid() || !mCategory)
        return QModelIndex();

    int itemIndex = row * columnCount() + column;
    int entryIndex = itemIndex / itemsPerEntry();
    int shadowIndex = itemIndex % itemsPerEntry();
    if (entryIndex >= mCategory->entryCount() || shadowIndex >= mCategory->shadowCount())
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex TileCategoryModel::index(BuildingTileEntry *entry, int e) const
{
    int entryIndex = mCategory->indexOf(entry);
    int shadowIndex = mCategory->enumToShadow(e);
    if (shadowIndex < 0 || shadowIndex >= mCategory->shadowCount())
        return QModelIndex();
    int itemIndex = entryIndex * itemsPerEntry() + shadowIndex;
    int column = itemIndex % columnCount();
    int row = itemIndex / columnCount();
    return createIndex(row, column);
}

QString TileCategoryModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList TileCategoryModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

bool TileCategoryModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     BuildingTileEntry *entry = entryAt(index);
     if (!entry)
         return false;
     int e = enumAt(index);

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingTilesMgr::nameForTile(tilesetName, tileId);
         emit tileDropped(entry, e, tileName);
     }

     return true;
}

void TileCategoryModel::clear()
{
    setCategory(0);
}

void TileCategoryModel::setCategory(BuildingTileCategory *category)
{
    mCategory = category;
    reset();
}

BuildingTileEntry *TileCategoryModel::entryAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    int itemIndex = index.column() + index.row() * columnCount();
    int entryIndex = itemIndex / itemsPerEntry();
    int shadowIndex = itemIndex % itemsPerEntry();
    if (entryIndex >= mCategory->entryCount() || shadowIndex >= mCategory->shadowCount())
        return 0;
    return mCategory->entry(entryIndex);
}

int TileCategoryModel::enumAt(const QModelIndex &index) const
{
    if (!index.isValid())
        return BuildingTileCategory::Invalid;
    int itemIndex = index.column() + index.row() * columnCount();
    int entryIndex = itemIndex / itemsPerEntry();
    int shadowIndex = itemIndex % itemsPerEntry();
    if (entryIndex >= mCategory->entryCount() || shadowIndex >= mCategory->shadowCount())
        return BuildingTileCategory::Invalid;
    return mCategory->shadowToEnum(shadowIndex);
}

void TileCategoryModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    reset();
}

int TileCategoryModel::shadowImageColumns() const
{
    if (!mCategory)
        return 0;
    return mCategory->shadowImage().width() / 64;
}

int TileCategoryModel::shadowImageRows() const
{
    if (!mCategory)
        return 0;
    return mCategory->shadowImage().height() / 128;
}
