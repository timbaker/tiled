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

#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOption>
#include <QToolTip>

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

    QBrush brush = qvariant_cast<QBrush>(m->data(index, Qt::BackgroundRole));
    painter->fillRect(option.rect, brush);

    QString tilesetName = m->headerAt(index);
    if (!tilesetName.isEmpty()) {
        if (index.row() > 0) {
            painter->setPen(Qt::darkGray);
            painter->drawLine(option.rect.topLeft(), option.rect.topRight());
            painter->setPen(Qt::black);
        }
        // One slice of the tileset name is drawn in each column.
        if (index.column() == 0)
            painter->drawText(option.rect.adjusted(2, 2, 0, 0), Qt::AlignLeft,
                              tilesetName);
        else {
            QRect r = option.rect.adjusted(-index.column() * option.rect.width(),
                                           0, 0, 0);
            painter->save();
            painter->setClipRect(option.rect);
            painter->drawText(r.adjusted(2, 2, 0, 0), Qt::AlignLeft, tilesetName);
            painter->restore();
        }
        return;
    }

    Tile *tile;
    if (!(tile = m->tileAt(index))) {
#if 0
        painter->drawLine(option.rect.topLeft(), option.rect.bottomRight());
        painter->drawLine(option.rect.topRight(), option.rect.bottomLeft());
#endif
        return;
    }

    const int extra = 2;

    QString label = index.data(Qt::DecorationRole).toString();

    QRect r = m->categoryBounds(index);
    if (m->showLabels() && m->highlightLabelledItems() && label.length())
        r = QRect(index.column(),index.row(),1,1);
    if (r.isValid() && !(option.state & QStyle::State_Selected)) {
        int left = option.rect.left();
        int right = option.rect.right();
        int top = option.rect.top();
        int bottom = option.rect.bottom();
        if (index.column() == r.left())
            left += extra;
        if (index.column() == r.right())
            right -= extra;
        if (index.row() == r.top())
            top += extra;
        if (index.row() == r.bottom())
            bottom -= extra;

        QBrush brush = qvariant_cast<QBrush>(index.data(MixedTilesetModel::CategoryBgRole));
        painter->fillRect(left, top, right-left+1, bottom-top+1, brush);

        painter->setPen(Qt::darkGray);
        if (index.column() == r.left())
            painter->drawLine(left, top, left, bottom);
        if (index.column() == r.right())
            painter->drawLine(right, top, right, bottom);
        if (index.row() == r.top())
            painter->drawLine(left, top, right, top);
        if (index.row() == r.bottom())
            painter->drawLine(left, bottom, right, bottom);
        painter->setPen(Qt::black);
    }

    // Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int tileWidth = tile->tileset()->tileWidth() * mView->zoomable()->scale();

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const QFontMetrics fm = painter->fontMetrics();
    const int labelHeight = m->showLabels() ? fm.lineSpacing() : 0;
    const int dw = option.rect.width() - tileWidth;
    const QMargins margins = tile->drawMargins(mView->zoomable()->scale());
    painter->drawPixmap(option.rect.adjusted(dw/2 + margins.left(), extra + margins.top(), -(dw - dw/2) - margins.right(), -extra - labelHeight - margins.bottom()), tileImage);

    if (m->showLabels()) {
        QString name = fm.elidedText(label, Qt::ElideRight, option.rect.width());
        painter->drawText(option.rect.left(), option.rect.bottom() - labelHeight,
                          option.rect.width(), labelHeight, Qt::AlignHCenter, name);
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

QSize TileDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    const MixedTilesetModel *m = static_cast<const MixedTilesetModel*>(index.model());
    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;
    if (m->headerAt(index).length()) {
        if (m->columnCount() == 1)
            return QSize(mView->maxHeaderWidth() + extra,
                         option.fontMetrics.lineSpacing() + 2);
        return QSize(64 * zoom + extra, option.fontMetrics.lineSpacing() + 2);
    }
    if (!m->tileAt(index))
        return QSize(64 * zoom + extra, 128 * zoom + extra);
    const Tileset *tileset = m->tileAt(index)->tileset();
    const int tileWidth = tileset->tileWidth() + (m->showLabels() ? 16 : 0);
    const QFontMetrics &fm = option.fontMetrics;
    const int labelHeight = m->showLabels() ? fm.lineSpacing() : 0;
    return QSize(tileWidth * zoom + extra,
                 tileset->tileHeight() * zoom + extra + labelHeight);
}

} // namepace Internal
} // namespace Tiled

/////

// This constructor is for the benefit of QtDesigner
MixedTilesetView::MixedTilesetView(QWidget *parent) :
    QTableView(parent),
    mModel(new MixedTilesetModel(this)),
    mZoomable(new Zoomable(this)),
    mContextMenu(0)
{
    init();
}

MixedTilesetView::MixedTilesetView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new MixedTilesetModel(this)),
    mZoomable(zoomable),
    mContextMenu(0),
    mMaxHeaderWidth(0)
{
    init();
}

QSize MixedTilesetView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void MixedTilesetView::mousePressEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && !mMousePressed) {
        mMousePressed = true;
        emit mousePressed();
    }

    QTableView::mousePressEvent(event);
}

void MixedTilesetView::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::NoButton) {
        QModelIndex index = indexAt(event->pos());
        if (index.isValid() && index != mToolTipIndex) {
            if (model()->tileAt(mToolTipIndex))
                emit tileLeft(mToolTipIndex);
            if (model()->tileAt(index))
                emit tileEntered(index);
            mToolTipIndex = index;
            QVariant tooltip = index.data(Qt::ToolTipRole);
            if (tooltip.canConvert<QString>())
                QToolTip::showText(event->globalPos(), tooltip.toString(), this,
                                   visualRect(index));
            return;
        } else if (!index.isValid() && mToolTipIndex.isValid()) {
            if (model()->tileAt(mToolTipIndex)) {
                emit tileLeft(mToolTipIndex);
                mToolTipIndex = QModelIndex();
            }
        }
    }

    QTableView::mouseMoveEvent(event);
}

void MixedTilesetView::mouseReleaseEvent(QMouseEvent *event)
{
    if ((event->button() == Qt::LeftButton) && mMousePressed) {
        mMousePressed = false;
        emit mouseReleased();
    }

    QTableView::mouseReleaseEvent(event);
}

void MixedTilesetView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

bool MixedTilesetView::viewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::ToolTip:
        return true;
    case QEvent::Leave:
        if (model()->tileAt(mToolTipIndex)) {
            emit tileLeft(mToolTipIndex);
            mToolTipIndex = QModelIndex();
        }
        break;
    default:
        break;
    }
    return QTableView::viewportEvent(event);
}

void MixedTilesetView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void MixedTilesetView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void MixedTilesetView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void MixedTilesetView::setTiles(const QList<Tile *> &tiles,
                                const QList<void *> &userData,
                                const QStringList &headers)
{
    mMaxHeaderWidth = 0;
    if (model()->columnCount() == 1) {
        foreach (QString header, headers) {
            int width = fontMetrics().width(header);
            mMaxHeaderWidth = qMax(mMaxHeaderWidth, width);
        }
    }

    selectionModel()->clear(); // because the model calls reset()
    model()->setTiles(tiles, userData, headers);
}

void MixedTilesetView::setTileset(Tileset *tileset,
                                  const QList<void *> &userData,
                                  const QStringList &labels)
{
    mMaxHeaderWidth = 0;

    selectionModel()->clear(); // because the model calls reset()
    model()->setTileset(tileset, userData, labels);
}

void MixedTilesetView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void MixedTilesetView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(32);
    setItemDelegate(new TileDelegate(this, this));
    setShowGrid(false);

    setSelectionMode(SingleSelection);

    setMouseTracking(true); // for fast tooltips

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

    mMousePressed = false;
}

/////

#define COLUMN_COUNT 8 // same as recent PZ tilesets

MixedTilesetModel::MixedTilesetModel(QObject *parent) :
    QAbstractListModel(parent),
    mTileset(0),
    mShowHeaders(true),
    mShowLabels(false),
    mHighlightLabelledItems(false),
    mColumnCount(COLUMN_COUNT)
{
}

MixedTilesetModel::~MixedTilesetModel()
{
    qDeleteAll(mItems);
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
    return parent.isValid() ? 0 : (mTileset ? mTileset->columnCount() : mColumnCount);
}

Qt::ItemFlags MixedTilesetModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDragEnabled;
    if (!index.isValid())
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

QVariant MixedTilesetModel::data(const QModelIndex &index, int role) const
{
    if (role == CategoryBgRole) {
        if (Item *item = toItem(index))
            return QBrush(item->mCategoryColor.isValid()
                    ? item->mCategoryColor
                    : QColor(220, 220, 220));
    }
    if (role == Qt::BackgroundRole) {
        if (Item *item = toItem(index))
            return item->mBackground;
    }
    if (role == Qt::DisplayRole) {
        if (Tile *tile = tileAt(index))
            return tile->image();
    }
    if (role == Qt::DecorationRole) {
        if (Item *item = toItem(index))
            return item->mLabel;
    }
    if (role == Qt::ToolTipRole) {
        if (Item *item = toItem(index))
            return item->mToolTip;
    }

    return QVariant();
}

bool MixedTilesetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Item *item = toItem(index)) {
        if (role == CategoryBgRole) {
            if (value.canConvert<QBrush>()) {
                item->mCategoryColor = qvariant_cast<QBrush>(value).color();
                // SLOW in TileProperties editor...
//                emit dataChanged(index, index);
                return true;
            }
        }
        if (role == Qt::BackgroundRole) {
            if (value.canConvert<QBrush>()) {
                item->mBackground = qvariant_cast<QBrush>(value);
                emit dataChanged(index, index);
                return true;
            }
        }
        if (role == Qt::DecorationRole) {
            if (value.canConvert<QString>()) {
                item->mLabel = value.toString();
                return true;
            }
        }
        if (role == Qt::DisplayRole) {
            if (item->mTile && value.canConvert<Tile*>()) {
                item->mTile = qvariant_cast<Tile*>(value);
                return true;
            }
        }
        if (role == Qt::ToolTipRole) {
            if (value.canConvert<QString>()) {
                item->mToolTip = value.toString();
                return true;
            }
        }
        if (role == HeaderRole) {
            if (item->mTilesetName.length() && value.canConvert<QString>()) {
                item->mTilesetName = value.toString();
                emit dataChanged(index, index);
                return true;
            }
        }
    }
    return false;
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
    if (Item *item = toItem(tile)) {
        int tileIndex = indexOf(item);
        if (tileIndex != -1)
            return index(tileIndex / columnCount(), tileIndex % columnCount());
    }
    return QModelIndex();
}

QModelIndex MixedTilesetModel::index(void *userData)
{
    if (Item *item = toItem(userData)) {
        int tileIndex = indexOf(item);
        if (tileIndex != -1)
            return index(tileIndex / columnCount(), tileIndex % columnCount());
    }
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

bool MixedTilesetModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)
    Q_UNUSED(parent)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         emit tileDropped(tilesetName, tileId);
     }

     return true;
}

void MixedTilesetModel::clear()
{
    setTiles(QList<Tile*>());
}

void MixedTilesetModel::setTiles(const QList<Tile *> &tiles,
                                 const QList<void *> &userData,
                                 const QStringList &headers)
{
    beginResetModel();

    mTiles = tiles;
    mUserData = userData;
    mTileset = 0;
    mTileToItem.clear();
    mUserDataToItem.clear();
    mTileItemsByIndex.clear();

    qDeleteAll(mItems);
    mItems.clear();
    QString header;
    int index = 0;

    foreach (Tile *tile, mTiles) {
        if (mShowHeaders) {
            QString name = tile->tileset()->name();
            if (!headers.isEmpty())
                name = (index < headers.size()) ? headers[index] : headers.last();
            if (name != header) {
                while (mItems.count() % columnCount())
                    mItems += new Item(); // filler after previous tile
                header = name;
                for (int i = 0; i < columnCount(); i++)
                    mItems += new Item(header);
            }
        }
        void *data = (index < userData.size()) ? userData[index] : 0;
        Item *item = new Item(tile, data);
        mItems += item;
        mTileItemsByIndex[index] = item;
        if (!mTileToItem.contains(tile))
            mTileToItem[tile] = item; // may not be unique!
        if (!mUserDataToItem.contains(item->mUserData))
            mUserDataToItem[item->mUserData] = item; // may not be unique!
        index++;
    }

    index = 0;
    foreach (Item *item, mItems)
        item->mIndex = index++;

    endResetModel();
}

void MixedTilesetModel::setTileset(Tileset *tileset,
                                   const QList<void*> &userData,
                                   const QStringList &labels)
{
    beginResetModel();

    mTiles.clear();
    mTileset = tileset;
    for (int i = 0; i < mTileset->tileCount(); i++)
        mTiles += mTileset->tileAt(i);
    mTileToItem.clear();
    mUserDataToItem.clear();
    mTileItemsByIndex.clear();

    qDeleteAll(mItems);
    mItems.clear();
    QString tilesetName;
    int index = 0;
    foreach (Tile *tile, mTiles) {
        if (mShowHeaders) {
            if (tile->tileset()->name() != tilesetName) {
                while (mItems.count() % columnCount())
                    mItems += new Item(); // filler after previous tile
                tilesetName = tile->tileset()->name();
                for (int i = 0; i < columnCount(); i++)
                    mItems += new Item(tilesetName);
            }
        }
        Item *item = new Item(tile);
        if (labels.size() > index)
            item->mLabel = labels[index];
        if (userData.size() > index)
            item->mUserData = userData[index];
        mItems += item;
        mTileItemsByIndex[index] = item;
        mTileToItem[tile] = item;
        mUserDataToItem[item->mUserData] = item;
        index++;
    }

    index = 0;
    foreach (Item *item, mItems)
        item->mIndex = index++;

    endResetModel();
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

void *MixedTilesetModel::userDataAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mUserData;
    return 0;
}

void MixedTilesetModel::setCategoryBounds(const QModelIndex &index, const QRect &bounds)
{
    QRect viewBounds = bounds.translated(index.column(), index.row());
    for (int x = 0; x < bounds.width(); x++)
        for (int y = 0; y < bounds.height(); y++) {
            if (Item *item = toItem(this->index(index.row() + y, index.column() + x))) {
                if (item->mTile)
                    item->mCategoryBounds = viewBounds;
            }
        }
}

void MixedTilesetModel::setCategoryBounds(Tile *tile, const QRect &bounds)
{
    setCategoryBounds(index(tile), bounds);
}

void MixedTilesetModel::setCategoryBounds(int tileIndex, const QRect &bounds)
{
    if (Item *item = toItem(tileIndex)) {
        if (item->mCategoryBounds == bounds)
            return;
        if (bounds.isEmpty()) {
            item->mCategoryBounds = QRect();
            // FIXME: clear bounds of all items that used to overlap.
            return;
        }
        int itemIndex = indexOf(item);
        QModelIndex index = createIndex(itemIndex / columnCount(), itemIndex % columnCount(), item);
        setCategoryBounds(index, bounds);
    }
}

QRect MixedTilesetModel::categoryBounds(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->mTile)
            return item->mCategoryBounds;
    }
    return QRect();
}

QRect MixedTilesetModel::categoryBounds(Tile *tile) const
{
    if (Item *item = toItem(tile)) {
        if (item->mTile)
            return item->mCategoryBounds;
    }
    return QRect();
}

void MixedTilesetModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void MixedTilesetModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

void MixedTilesetModel::setShowHeaders(bool show)
{
    mShowHeaders = show;
}

void MixedTilesetModel::setShowLabels(bool show)
{
    mShowLabels = show;
    redisplay();
}

void MixedTilesetModel::setLabel(Tile *tile, const QString &label)
{
    if (Item *item = toItem(tile)) {
        item->mLabel = label;
        QModelIndex index = this->index(tile);
        emit dataChanged(index, index);
    }
}

void MixedTilesetModel::setToolTip(int tileIndex, const QString &text)
{
    if (Item *item = toItem(tileIndex)) {
        item->mToolTip = text;
#if 0
        int itemIndex = indexOf(item);
        QModelIndex index = createIndex(itemIndex / columnCount(), itemIndex % columnCount(), item);
        emit toolTipChanged(index);
#endif
    }
}

void MixedTilesetModel::setColumnCount(int count)
{
    beginResetModel();
    mColumnCount = count;
    endResetModel();
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(Tile *tile) const
{
    if (mTileToItem.contains(tile))
        return mTileToItem[tile];
    return 0;
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(void *userData) const
{
    if (mUserDataToItem.contains(userData))
        return mUserDataToItem[userData];
    return 0;
}

MixedTilesetModel::Item *MixedTilesetModel::toItem(int tileIndex) const
{
    if (mTileItemsByIndex.contains(tileIndex))
        return mTileItemsByIndex[tileIndex];
    return 0;
}
