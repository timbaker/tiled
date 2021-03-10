/*
 * tilerotateview.cpp
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

#include "tilerotateview.h"

#include "BuildingEditor/buildingtiles.h"

#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QApplication>
#include <QDebug>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOption>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Internal;

/////

namespace Tiled {

class TileRotateDelegate : public QAbstractItemDelegate
{
public:
    TileRotateDelegate(TileRotateView *view, QObject *parent = nullptr)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    QPoint dropCoords(const QPoint &dragPos, const QModelIndex &index);

    qreal scale() const
    { return mView->zoomable()->scale(); }

    void itemResized(const QModelIndex &index);

private:
    TileRotateView *mView;
};

void TileRotateDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const TileRotateModel *m = static_cast<const TileRotateModel*>(index.model());

    QString header = m->headerAt(index);
    if (!header.isEmpty()) {
        if (index.row() > 0) {
            painter->setPen(Qt::darkGray);
            painter->drawLine(option.rect.topLeft(), option.rect.topRight());
            painter->setPen(Qt::black);
        }
        // One slice of the tileset name is drawn in each column.
        if (index.column() == 0)
            painter->drawText(option.rect.adjusted(2, 2, 0, 0), Qt::AlignLeft, header);
        else {
            QModelIndex indexColumn0 = m->index(index.row(), 0);
            int left = mView->visualRect(indexColumn0).left();
            QRect r = option.rect;
            r.setLeft(left);
            painter->save();
            painter->setClipRect(option.rect);
            painter->drawText(r.adjusted(2, 2, 0, 0), Qt::AlignLeft, header);
            painter->restore();
        }
        return;
    }

    TileRotated *ftile = m->tileAt(index);
    if (!ftile)
        return;

    MapRotation mapRotation = m->mapRotationAt(index);

    TileRotated *original = ftile;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scale = this->scale();
    int extra = 2;

    qreal tileWidth = 64 * scale;
    qreal tileHeight = 32 * scale;
    qreal imageHeight = 128 * scale;
    QPointF tileMargins(0, imageHeight - tileHeight);

    // Draw the tile images.
    /*if (mapRotation == MapRotation::NotRotated)*/ {
        TileRotatedVisual& direction = ftile->mVisual;
        for (int i = 0; i < direction.mTileNames.size(); i++) {
            const QString& tileName = direction.mTileNames[i];
            const QPoint tileOffset = direction.pixelOffset(i);
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) { // FIXME: calc this elsewhere
                QRect r1(r.topLeft() + tileOffset * scale, QSize(tileWidth, imageHeight));
                if (tile->image().isNull())
                    tile = TilesetManager::instance()->missingTile();
                const QMargins margins = tile->drawMargins(scale);
                painter->drawImage(r1.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile->image());
            }
        }
    }
#if 0
    if (mapRotation == MapRotation::Clockwise90) {
        TileRotatedVisual& direction = ftile->r90;
        for (int i = 0; i < direction.mTileNames.size(); i++) {
            const QString& tileName = direction.mTileNames[i];
            const QPoint tileOffset = direction.pixelOffset(i);
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) { // FIXME: calc this elsewhere
                QRect r1(r.topLeft() + tileOffset * scale, QSize(tileWidth, imageHeight));
                if (tile->image().isNull())
                    tile = TilesetManager::instance()->missingTile();
                const QMargins margins = tile->drawMargins(scale);
                painter->drawImage(r1.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile->image());
            }
        }
    }
    if (mapRotation == MapRotation::Clockwise180) {
        TileRotatedVisual& direction = ftile->r180;
        for (int i = 0; i < direction.mTileNames.size(); i++) {
            const QString& tileName = direction.mTileNames[i];
            const QPoint tileOffset = direction.pixelOffset(i);
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) { // FIXME: calc this elsewhere
                QRect r1(r.topLeft() + tileOffset * scale, QSize(tileWidth, imageHeight));
                if (tile->image().isNull())
                    tile = TilesetManager::instance()->missingTile();
                const QMargins margins = tile->drawMargins(scale);
                painter->drawImage(r1.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile->image());
            }
        }
    }
    if (mapRotation == MapRotation::Clockwise270) {
        TileRotatedVisual& direction = ftile->r270;
        for (int i = 0; i < direction.mTileNames.size(); i++) {
            const QString& tileName = direction.mTileNames[i];
            const QPoint tileOffset = direction.pixelOffset(i);
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) { // FIXME: calc this elsewhere
                QRect r1(r.topLeft() + tileOffset * scale, QSize(tileWidth, imageHeight));
                if (tile->image().isNull())
                    tile = TilesetManager::instance()->missingTile();
                const QMargins margins = tile->drawMargins(scale);
                painter->drawImage(r1.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile->image());
            }
        }
    }
#endif
#if 1
    // Draw the "floor"
    if (true) {
        QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
        QPointF p1(r.left() + r.width() / 2, r.bottom() - tileHeight);
        QPointF p2(r.right(), r.bottom() - tileHeight / 2);
        QPointF p3(r.left() + r.width() / 2, r.bottom());
        QPointF p4(r.left(), r.bottom() - tileHeight / 2);
        painter->drawLine(p1, p2); painter->drawLine(p2, p3);
        painter->drawLine(p3, p4); painter->drawLine(p4, p1);
    }
#else
    if (false) {
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
#endif
    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }
#if 0
    QRectF textRect;
    painter->drawText(option.rect.adjusted(extra, extra, 0, 0),
                      Qt::AlignTop | Qt::AlignLeft,
                      ftile->mTileset->mName + QLatin1Literal("_") + QString::number(ftile->mID),
                      &textRect);
    // Draw resolved-tiles indicator
    if (false)
        painter->fillRect(textRect.right() + 3, textRect.center().y()-2, 4, 4, Qt::gray);
#endif

    // Grid lines
    if (true) {
        painter->fillRect(option.rect.right(), option.rect.top(), 1, option.rect.height(), Qt::lightGray);
        painter->fillRect(option.rect.left(), option.rect.bottom(), option.rect.width(), 1, Qt::lightGray);
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
        const QWidget *widget = nullptr/*d->widget(option)*/;
        QStyle *style = /*widget ? widget->style() :*/ QApplication::style();
        style->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter, widget);
    }
}

QSize TileRotateDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    int width = 2, height = 2;
    const TileRotateModel *m = static_cast<const TileRotateModel*>(index.model());
    const qreal zoom = scale();
    const int extra = 2 * 2;
    if (m->headerAt(index).length())
        return QSize(64 * zoom + extra, option.fontMetrics.lineSpacing() + 2);
    int tileWidth = 64, tileHeight = 128;
    QSize size = QSize(tileWidth, tileHeight) * zoom + QSize(extra, extra);
    return size;
}

QPoint TileRotateDelegate::dropCoords(const QPoint &dragPos, const QModelIndex &index)
{
    const TileRotateModel *m = static_cast<const TileRotateModel*>(index.model());
    TileRotated *ftile = m->tileAt(index);
    QRect r = mView->visualRect(index);
    const int extra = 2;
    qreal x = dragPos.x() - r.left() - extra;
    qreal y = dragPos.y() - r.top() - extra;
    int tileWidth = 64, tileHeight = 128;
    if (x < 0 || y < 0)
        return QPoint(-1, -1);
    switch (m->mapRotationAt(index)) {
    case MapRotation::NotRotated:
        return QPoint(0, 0);
    case MapRotation::Clockwise90:
        return QPoint(1, 0);
    case MapRotation::Clockwise180:
        return QPoint(2, 0);
    case MapRotation::Clockwise270:
        return QPoint(3, 0);
    }
    return QPoint(-1, -1);
}

void TileRotateDelegate::itemResized(const QModelIndex &index)
{
    emit sizeHintChanged(index);
}

} // namespace BuildingEditor

/////

// This constructor is for the benefit of QtDesigner
TileRotateView::TileRotateView(QWidget *parent) :
    QTableView(parent),
    mModel(new TileRotateModel(this)),
    mDelegate(new TileRotateDelegate(this, this)),
    mZoomable(new Zoomable(this)),
    mContextMenu(nullptr)
{
    init();
}

TileRotateView::TileRotateView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new TileRotateModel(this)),
    mDelegate(new TileRotateDelegate(this, this)),
    mZoomable(zoomable),
    mContextMenu(nullptr)
{
    init();
}

QSize TileRotateView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void TileRotateView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void TileRotateView::dragMoveEvent(QDragMoveEvent *event)
{
    QAbstractItemView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        if (TileRotated *ftile = model()->tileAt(index)) {
            QPoint dropCoords = mDelegate->dropCoords(event->pos(), index);
            if (!QRect(0, 0, 4, 1).contains(dropCoords)) {
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

void TileRotateView::dragLeaveEvent(QDragLeaveEvent *event)
{
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
    QAbstractItemView::dragLeaveEvent(event);
}

void TileRotateView::dropEvent(QDropEvent *event)
{
    QAbstractItemView::dropEvent(event);
    model()->setDropCoords(QPoint(-1,-1), QModelIndex());
}

void TileRotateView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void TileRotateView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void TileRotateView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void TileRotateView::setTiles(const QList<TileRotated *> &tilesList)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setTiles(tilesList);
}

void TileRotateView::redisplay()
{
    model()->redisplay();
}

void TileRotateView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void TileRotateView::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay(); // FIXME: only if it is a TileMetaInfoMgr tileset
}

void TileRotateView::tilesetAdded(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void TileRotateView::tilesetRemoved(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void TileRotateView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(32);
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

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));
}

/////

#define COLUMN_COUNT 4 // NotRotated, Clockwise90, Clockwise180, Clockwise270

TileRotateModel::TileRotateModel(QObject *parent) :
    QAbstractListModel(parent),
    mShowHeaders(true)
{
}

TileRotateModel::~TileRotateModel()
{
    qDeleteAll(mItems);
}

int TileRotateModel::rowCount(const QModelIndex &parent) const
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

int TileRotateModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

Qt::ItemFlags TileRotateModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

QVariant TileRotateModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return QVariant();
}

QVariant TileRotateModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex TileRotateModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    int tileIndex = row * columnCount() + column;
    if (tileIndex >= mItems.count())
        return QModelIndex();

    return createIndex(row, column, mItems.at(tileIndex));
}

QModelIndex TileRotateModel::index(TileRotated *tile)
{
    int tileIndex = mItems.indexOf(toItem(tile));
    if (tileIndex != -1)
        return index(tileIndex / columnCount(), tileIndex % columnCount());
    return QModelIndex();
}

QString TileRotateModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList TileRotateModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

bool TileRotateModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                   int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     TileRotated *tile = tileAt(index);
     if (!tile)
         return false;

     if (!QRect(0, 0, 4, 1).contains(mDropCoords))
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingTilesMgr::nameForTile(tilesetName, tileId);
         emit tileDropped(tile, mDropCoords.x(), tileName);
     }

     return true;
}

void TileRotateModel::clear()
{
    setTiles(QList<TileRotated*>());
}

void TileRotateModel::setTiles(const QList<TileRotated *> &tilesList)
{
    beginResetModel();

    qDeleteAll(mItems);
    mItems.clear();
    mTiles.clear();

    QString header;
    for (TileRotated *tile : tilesList) {
        QString label = QLatin1String("<TODO: Label>");
        if (mShowHeaders && (label != header)) {
            while (mItems.count() % columnCount())
                mItems += new Item(); // filler after previous tile
            header = label;
            for (int i = 0; i < columnCount(); i++)
                mItems += new Item(header);
        }
        mItems += new Item(tile, MapRotation::NotRotated);
        mItems += new Item(tile, MapRotation::Clockwise90);
        mItems += new Item(tile, MapRotation::Clockwise180);
        mItems += new Item(tile, MapRotation::Clockwise270);
    }

    endResetModel();
}

TileRotated *TileRotateModel::tileAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mTile;
    return nullptr;
}

MapRotation TileRotateModel::mapRotationAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mMapRotation;
    return MapRotation::NotRotated;
}

QString TileRotateModel::headerAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mHeading;
    return QString();
}

void TileRotateModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void TileRotateModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

TileRotateModel::Item *TileRotateModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

TileRotateModel::Item *TileRotateModel::toItem(TileRotated *ftile) const
{
    for (Item *item : mItems) {
        if (item->mTile == ftile)
            return item;
    }
    return nullptr;
}
