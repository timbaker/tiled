/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#include "containeroverlayview.h"

#include "BuildingEditor/buildingtiles.h"

#include "containeroverlayfile.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QApplication>
#include <QDebug>
#include <QDragMoveEvent>
#include <QHeaderView>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOption>

using namespace Tiled;
using namespace Internal;

/////

class ContainerOverlayDelegate : public QStyledItemDelegate
{
public:
    ContainerOverlayDelegate(ContainerOverlayView *view, QObject *parent = 0)
        : QStyledItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    int dropCoords(const QPoint &dragPos, const QModelIndex &index);

    qreal scale() const
    { return mView->zoomable()->scale(); }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;
    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;

private:
    ContainerOverlayView *mView;
};

void ContainerOverlayDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());

    ContainerOverlay *overlay = m->overlayAt(index);
    ContainerOverlayEntry *entry = m->entryAt(index);
    if (!overlay && !entry)
        return;

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    qreal scale = this->scale();
    int extra = 2;

    qreal tileWidth = 64 * scale;
//    qreal tileHeight = 128 * scale;
    int fontHgt = option.fontMetrics.lineSpacing();

    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }

    QPen pen = painter->pen();

    if (overlay) {
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(overlay->mTileName)) {
            QRect r = option.rect.adjusted(extra, extra, -extra, -extra);
            r.setRight(r.left() + tileWidth);
            QMargins margins = tile->drawMargins(scale);
            r.adjust(margins.left(), margins.top(), -margins.right(), -margins.bottom());
            painter->drawImage(r, tile->image());
        }
    }
    if (entry) {
        painter->setPen(QColor(Qt::lightGray));
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        painter->setPen(pen);
        Tile *baseTile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(entry->mParent->mTileName);
        QRect r = option.rect.adjusted(extra, extra + fontHgt, -extra, -extra);
        r.setRight(r.left() + tileWidth);
        QMargins margins = baseTile->drawMargins(scale);
        for (int i = 0; i < entry->mTiles.size(); i++) {
            QString tileName = entry->mTiles[i];
            if (baseTile) {
                painter->drawImage(r.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), baseTile->image());
            }
            if (!tileName.isEmpty()) {
                Tile *tile = (tileName == QLatin1Literal("none")) ?
                            BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile() :
                            BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName);
                if (tile) {
                    QMargins margins2 = tile->drawMargins(scale);
                    painter->drawImage(r.adjusted(margins2.left(), margins2.top(), -margins2.right(), -margins2.bottom()), tile->image());
                }
            }
            if (m->dropEntryIndex() == i && m->dropIndex() == index)
                painter->drawRect(r);
            r.translate(tileWidth + extra, 0);
        }
    }

    if (entry) {
        // Draw the room name
        QRectF textRect;
        painter->drawText(option.rect.adjusted(extra, extra, 0, 0),
                          Qt::AlignTop | Qt::AlignLeft,
                          entry->mRoomName,
                          &textRect);
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

QSize ContainerOverlayDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    const qreal scale = this->scale();
    const int extra = 2;
    ContainerOverlay *overlay = m->overlayAt(index);
    ContainerOverlayEntry *entry = m->entryAt(index);
    int tileWidth = 64 * scale, tileHeight = 128 * scale;
    int fontHgt = overlay ? 0 : option.fontMetrics.lineSpacing();
    QSize size(extra + tileWidth + extra, extra + fontHgt + tileHeight + extra);

    if (entry) {
        size.setWidth((extra + tileWidth) * entry->mTiles.size() + extra);
    }

    return size;
}

int ContainerOverlayDelegate::dropCoords(const QPoint &dragPos,
                                         const QModelIndex &index)
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    QRect r = mView->visualRect(index);
    ContainerOverlay *overlay = m->overlayAt(index);
    ContainerOverlayEntry *entry = m->entryAt(index);
    const int extra = 2;
    int tileWid = 64 * scale();
    if (overlay)
        return 0;
    if (entry) {
        int index = (dragPos.x() - r.x()) / (extra + tileWid);
        if (index < 0 || index >= entry->mTiles.size())
            return -1;
        return index;
    }
    return -1;
}

QWidget *ContainerOverlayDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &/*index*/) const
{
    QLineEdit *editor = new QLineEdit(parent);
    editor->installEventFilter(const_cast<ContainerOverlayDelegate*>(this));
    return editor;
}

void ContainerOverlayDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    ContainerOverlayEntry *entry = m->entryAt(index);
    ((QLineEdit *)editor)->setText(entry ? entry->mRoomName : QLatin1String(""));
}

void ContainerOverlayDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    ContainerOverlayModel *m = static_cast<ContainerOverlayModel*>(model);
    ContainerOverlayEntry *entry = m->entryAt(index);
    if (entry) {
        QString text = ((QLineEdit*)editor)->text();
        if (!text.isEmpty() && !text.contains(QLatin1String(" ")) && text != entry->mRoomName)
            emit m->entryRoomNameEdited(entry, text);
    }
}

void ContainerOverlayDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    QRect rect = option.rect;
    rect.setHeight(editor->sizeHint().height());
    editor->setGeometry(rect);
}

/////

#define COLUMN_COUNT 1

ContainerOverlayModel::ContainerOverlayModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

ContainerOverlayModel::~ContainerOverlayModel()
{
    qDeleteAll(mItems);
}

int ContainerOverlayModel::rowCount(const QModelIndex &parent) const
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

int ContainerOverlayModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : COLUMN_COUNT;
}

Qt::ItemFlags ContainerOverlayModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    Item *item = toItem(index);
    if (!item)
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    else {
        flags |= Qt::ItemIsDropEnabled;
        if (item->mEntry)
            flags |= Qt::ItemIsEditable;
    }
    return flags;
}

QVariant ContainerOverlayModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
    return QVariant();
}

QVariant ContainerOverlayModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex ContainerOverlayModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    int itemIndex = row * columnCount() + column;
    if (itemIndex >= mItems.count())
        return QModelIndex();

    return createIndex(row, column, mItems.at(itemIndex));
}

QModelIndex ContainerOverlayModel::index(ContainerOverlay *overlay)
{
    int itemIndex = mItems.indexOf(toItem(overlay));
    if (itemIndex != -1)
        return index(itemIndex / columnCount(), itemIndex % columnCount());
    return QModelIndex();
}

QModelIndex ContainerOverlayModel::index(ContainerOverlayEntry *entry)
{
    int itemIndex = mItems.indexOf(toItem(entry));
    if (itemIndex != -1)
        return index(itemIndex / columnCount(), itemIndex % columnCount());
    return QModelIndex();
}

QString ContainerOverlayModel::mMimeType(QLatin1String("application/x-tilezed-tile"));

QStringList ContainerOverlayModel::mimeTypes() const
{
    return QStringList() << mMimeType;
}

bool ContainerOverlayModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                  int row, int column, const QModelIndex &parent)
 {
    Q_UNUSED(row)
    Q_UNUSED(column)

    if (action == Qt::IgnoreAction)
         return true;

     if (!data->hasFormat(mMimeType))
         return false;

     QModelIndex index = parent; // this->index(row, column, parent);
     Item *item = toItem(index);
     if (!item)
         return false;

     if (item->mEntry && mDropEntryIndex == -1)
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tilesetName, tileId);
         if (item->mOverlay)
            emit tileDropped(item->mOverlay, tileName);
         if (item->mEntry)
             emit tileDropped(item->mEntry, mDropEntryIndex, tileName);
     }

     return true;
}

void ContainerOverlayModel::clear()
{
    setOverlays(QList<ContainerOverlay*>());
}

void ContainerOverlayModel::setOverlays(const QList<ContainerOverlay *> &overlays)
{
    beginResetModel();

    qDeleteAll(mItems);
    mItems.clear();

    foreach (ContainerOverlay *overlay, overlays) {
        mItems += new Item(overlay);
        foreach (ContainerOverlayEntry *entry, overlay->mEntries) {
            mItems += new Item(entry);
        }
    }

    endResetModel();
}

ContainerOverlay *ContainerOverlayModel::overlayAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mOverlay;
    return 0;
}

ContainerOverlayEntry *ContainerOverlayModel::entryAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mEntry;
    return 0;
}

void ContainerOverlayModel::insertOverlay(int index, ContainerOverlay *overlay)
{
    int overlayIndex = 0, row = 0;
    foreach (Item *item, mItems) {
        if (item->mOverlay) {
            if (overlayIndex == index)
                break;
            overlayIndex++;
        }
        row++;
    }

    Item *item = new Item(overlay);
    beginInsertRows(QModelIndex(), row, row + overlay->mEntries.size());
    mItems.insert(row, item);
    foreach (ContainerOverlayEntry *entry, overlay->mEntries)
        mItems.insert(++row, new Item(entry));
    endInsertRows();
}

void ContainerOverlayModel::removeOverlay(ContainerOverlay *overlay)
{
    Item *item = toItem(overlay);
    int row = mItems.indexOf(item) / columnCount();
    beginRemoveRows(QModelIndex(), row, row + overlay->mEntries.size());
    int i = mItems.indexOf(item);
    delete mItems.takeAt(i);
    for (int j = 0; j < overlay->mEntries.size(); j++)
        delete mItems.takeAt(i);
    endRemoveRows();
}

void ContainerOverlayModel::insertEntry(ContainerOverlay *overlay, int index,
                                        ContainerOverlayEntry *entry)
{
    int row = this->index(overlay).row();
    beginInsertRows(QModelIndex(), row + index + 1, row + index + 1);
    mItems.insert(row + index + 1, new Item(entry));
    endInsertRows();
}

void ContainerOverlayModel::removeEntry(ContainerOverlay *overlay, int index)
{
    Item *item = toItem(overlay);
    int row = mItems.indexOf(item) / columnCount();
    beginRemoveRows(QModelIndex(), row + index + 1, row + index + 1);
    int i = mItems.indexOf(item) + index + 1;
    delete mItems.takeAt(i);
    endRemoveRows();
}

void ContainerOverlayModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void ContainerOverlayModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

void ContainerOverlayModel::redisplay(ContainerOverlay *overlay)
{
    int row = index(overlay).row();
    emit dataChanged(index(row, 0), index(row + overlay->mEntries.size(), 0));
}

void ContainerOverlayModel::redisplay(ContainerOverlayEntry *entry)
{
    emit dataChanged(index(entry), index(entry));
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(ContainerOverlay *overlay) const
{
    foreach (Item *item, mItems)
        if (item->mOverlay == overlay)
            return item;
    return 0;
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(ContainerOverlayEntry *entry) const
{
    foreach (Item *item, mItems)
        if (item->mEntry == entry)
            return item;
    return 0;
}

/////

// This constructor is for the benefit of QtDesigner
ContainerOverlayView::ContainerOverlayView(QWidget *parent) :
    QTableView(parent),
    mModel(new ContainerOverlayModel(this)),
    mDelegate(new ContainerOverlayDelegate(this, this)),
    mZoomable(new Zoomable(this))
{
    init();
}

ContainerOverlayView::ContainerOverlayView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new ContainerOverlayModel(this)),
    mDelegate(new ContainerOverlayDelegate(this, this)),
    mZoomable(zoomable)
{
    init();
}

QSize ContainerOverlayView::sizeHint() const
{
    return QSize(64 * 4, 128);
}

void ContainerOverlayView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    if (model()->entryAt(index)) {
        if (event->pos().y() < visualRect(index).y() + fontMetrics().lineSpacing()) {
            edit(index);
            return;
        }
    }

    QTableView::mouseDoubleClickEvent(event);
}

void ContainerOverlayView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void ContainerOverlayView::dragMoveEvent(QDragMoveEvent *event)
{
    QAbstractItemView::dragMoveEvent(event);

    if (event->isAccepted()) {
        QModelIndex index = indexAt(event->pos());
        int entryIndex = mDelegate->dropCoords(event->pos(), index);
        if (entryIndex == -1) {
            event->ignore();
            return;
        }
        model()->setDropCoords(entryIndex, index);
    } else {
        model()->setDropCoords(-1, QModelIndex());
        event->ignore();
        return;
    }
}

void ContainerOverlayView::dragLeaveEvent(QDragLeaveEvent *event)
{
    model()->setDropCoords(-1, QModelIndex());
    QAbstractItemView::dragLeaveEvent(event);
}

void ContainerOverlayView::dropEvent(QDropEvent *event)
{
    QAbstractItemView::dropEvent(event);
    model()->setDropCoords(-1, QModelIndex());
}

void ContainerOverlayView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void ContainerOverlayView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void ContainerOverlayView::setOverlays(const QList<ContainerOverlay *> &overlays)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setOverlays(overlays);
}

void ContainerOverlayView::redisplay()
{
    model()->redisplay();
}

void ContainerOverlayView::redisplay(ContainerOverlay *overlay)
{
    model()->redisplay(overlay);
}

void ContainerOverlayView::redisplay(ContainerOverlayEntry *entry)
{
    model()->redisplay(entry);
}

void ContainerOverlayView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void ContainerOverlayView::tilesetChanged(Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay(); // FIXME: only if it is a TileMetaInfoMgr tileset
}

void ContainerOverlayView::tilesetAdded(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void ContainerOverlayView::tilesetRemoved(Tiled::Tileset *tileset)
{
    Q_UNUSED(tileset)
    redisplay();
}

void ContainerOverlayView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setItemDelegate(mDelegate);
    setShowGrid(false);

    setSelectionMode(SingleSelection);

    setAcceptDrops(true);

    setEditTriggers(QAbstractItemView::NoEditTriggers);

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
