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

#include "tilerotatedvisualview.h"

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
    TileRotateDelegate(TileRotatedVisualView *view, QObject *parent = nullptr)
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
    TileRotatedVisualView *mView;
};

void TileRotateDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const TileRotatedVisualModel *m = static_cast<const TileRotatedVisualModel*>(index.model());

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

    MapRotation mapRotation;
    QSharedPointer<TileRotatedVisual> visual = m->visualAt(index, mapRotation);
    if (!visual)
        return;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scale = this->scale();
    int extra = 2;

    qreal tileWidth = 64 * scale;
    qreal tileHeight = 32 * scale;
    qreal imageHeight = 128 * scale;
    QPointF tileMargins(0, imageHeight - tileHeight);

    // Draw the tile images.
    TileRotatedVisualData& direction = visual->mData[int(mapRotation)];
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

    // Draw the "floor"
    if (true) {
        QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
        QPointF p1(r.left() + r.width() / 2, r.bottom() - tileHeight);
        QPointF p2(r.right(), r.bottom() - tileHeight / 2);
        QPointF p3(r.left() + r.width() / 2, r.bottom());
        QPointF p4(r.left(), r.bottom() - tileHeight / 2);
        painter->drawLine(p1, p2);
        painter->drawLine(p2, p3);
        painter->drawLine(p3, p4);
        painter->drawLine(p4, p1);
        QPen penOld = painter->pen();
        QPen pen = painter->pen();
        pen.setWidth(3);
        painter->setPen(pen);
        switch (mapRotation) {
        case MapRotation::NotRotated:
            painter->drawLine(p1, p2);
            break;
        case MapRotation::Clockwise90:
            painter->drawLine(p2, p3);
            break;
        case MapRotation::Clockwise180:
            painter->drawLine(p3, p4);
            break;
        case MapRotation::Clockwise270:
            painter->drawLine(p4, p1);
            break;
        }
        painter->setPen(penOld);
    }

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
    const TileRotatedVisualModel *m = static_cast<const TileRotatedVisualModel*>(index.model());
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
    const TileRotatedVisualModel *m = static_cast<const TileRotatedVisualModel*>(index.model());
    MapRotation mapRotation;
    QSharedPointer<TileRotatedVisual> visual = m->visualAt(index, mapRotation);
    QRect r = mView->visualRect(index);
    const int extra = 2;
    qreal x = dragPos.x() - r.left() - extra;
    qreal y = dragPos.y() - r.top() - extra;
    if (x < 0 || y < 0)
        return QPoint(-1, -1);
    switch (mapRotation) {
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
TileRotatedVisualView::TileRotatedVisualView(QWidget *parent) :
    QTableView(parent),
    mModel(new TileRotatedVisualModel(this)),
    mDelegate(new TileRotateDelegate(this, this)),
    mZoomable(new Zoomable(this)),
    mContextMenu(nullptr)
{
    init();
}

TileRotatedVisualView::TileRotatedVisualView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new TileRotatedVisualModel(this)),
    mDelegate(new TileRotateDelegate(this, this)),
    mZoomable(zoomable),
    mContextMenu(nullptr)
{
    init();
}

QSize TileRotatedVisualView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void TileRotatedVisualView::wheelEvent(QWheelEvent *event)
{
    QPoint numDegrees = event->angleDelta() / 8;
    if ((event->modifiers() & Qt::ControlModifier) && (numDegrees.y() != 0))
    {
        QPoint numSteps = numDegrees / 15;
        mZoomable->handleWheelDelta(numSteps.y() * 120);
        return;
    }

    QTableView::wheelEvent(event);
}

void TileRotatedVisualView::dragMoveEvent(QDragMoveEvent *event)
{
    QAbstractItemView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        MapRotation mapRotation;
        if (QSharedPointer<TileRotatedVisual> visual = model()->visualAt(index, mapRotation)) {
            //
        } else {
            event->ignore();
            return;
        }
    }
}

void TileRotatedVisualView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QAbstractItemView::dragLeaveEvent(event);
}

void TileRotatedVisualView::dropEvent(QDropEvent *event)
{
    QAbstractItemView::dropEvent(event);
}

void TileRotatedVisualView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void TileRotatedVisualView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void TileRotatedVisualView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void TileRotatedVisualView::setVisuals(const QList<QSharedPointer<TileRotatedVisual>> &visuals)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setVisuals(visuals);
}

void TileRotatedVisualView::redisplay()
{
    model()->redisplay();
}

void TileRotatedVisualView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void TileRotatedVisualView::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay(); // FIXME: only if it is a TileMetaInfoMgr tileset
}

void TileRotatedVisualView::tilesetAdded(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void TileRotatedVisualView::tilesetRemoved(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void TileRotatedVisualView::init()
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

TileRotatedVisualModel::TileRotatedVisualModel(QObject *parent) :
    QAbstractListModel(parent),
    mShowHeaders(true)
{
}

TileRotatedVisualModel::~TileRotatedVisualModel()
{
    qDeleteAll(mItems);
}

int TileRotatedVisualModel::rowCount(const QModelIndex &parent) const
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

int TileRotatedVisualModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

Qt::ItemFlags TileRotatedVisualModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    MapRotation mapRotation;
    if (visualAt(index, mapRotation) == nullptr)
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else
        flags |= Qt::ItemIsDropEnabled;
    return flags;
}

QVariant TileRotatedVisualModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return QVariant();
}

QVariant TileRotatedVisualModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex TileRotatedVisualModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    int tileIndex = row * columnCount() + column;
    if (tileIndex >= mItems.count())
        return QModelIndex();

    return createIndex(row, column, mItems.at(tileIndex));
}

QModelIndex TileRotatedVisualModel::index(QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation)
{
    int tileIndex = mItems.indexOf(toItem(visual, mapRotation));
    if (tileIndex != -1)
        return index(tileIndex / columnCount(), tileIndex % columnCount());
    return QModelIndex();
}

QString TileRotatedVisualModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList TileRotatedVisualModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

bool TileRotatedVisualModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                   int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     MapRotation mapRotation;
     QSharedPointer<TileRotatedVisual> visual = visualAt(index, mapRotation);
     if (visual == nullptr)
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingTilesMgr::nameForTile(tilesetName, tileId);
         emit tileDropped(visual, mapRotation, tileName);
     }

     return true;
}

void TileRotatedVisualModel::clear()
{
    setVisuals(QList<QSharedPointer<TileRotatedVisual>>());
}

void TileRotatedVisualModel::setVisuals(const QList<QSharedPointer<TileRotatedVisual>> &visuals)
{
    beginResetModel();

    qDeleteAll(mItems);
    mItems.clear();
    mVisuals.clear();

    QString header;
    for (const QSharedPointer<TileRotatedVisual>& visual : visuals) {
        QString label = QLatin1String("<TODO: Label>");
        if (mShowHeaders && (label != header)) {
            while (mItems.count() % columnCount())
                mItems += new Item(); // filler after previous tile
            header = label;
            for (int i = 0; i < columnCount(); i++)
                mItems += new Item(header);
        }
        mItems += new Item(visual, MapRotation::NotRotated);
        mItems += new Item(visual, MapRotation::Clockwise90);
        mItems += new Item(visual, MapRotation::Clockwise180);
        mItems += new Item(visual, MapRotation::Clockwise270);
    }

    endResetModel();
}

QSharedPointer<TileRotatedVisual> TileRotatedVisualModel::visualAt(const QModelIndex &index, MapRotation& mapRotation) const
{
    if (Item *item = toItem(index)) {
        mapRotation = item->mMapRotation;
        return item->mVisual;
    }
    return nullptr;
}

QString TileRotatedVisualModel::headerAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mHeading;
    return QString();
}

void TileRotatedVisualModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void TileRotatedVisualModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

TileRotatedVisualModel::Item *TileRotatedVisualModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

TileRotatedVisualModel::Item *TileRotatedVisualModel::toItem(QSharedPointer<TileRotatedVisual> visual, MapRotation mapRotation) const
{
    for (Item *item : mItems) {
        if ((item->mVisual == visual) && (item->mMapRotation == mapRotation)) {
            return item;
        }
    }
    return nullptr;
}
