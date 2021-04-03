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

// FIXME: shouldn't know anything about this class
#include "tileoverlayfile.h"

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
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyleOption>

using namespace Tiled;
using namespace Internal;

/////

class ContainerOverlayDelegate : public QStyledItemDelegate
{
public:
    ContainerOverlayDelegate(ContainerOverlayView *view, QObject *parent = nullptr)
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

    void setEditOrdinal(EditAbstractOverlay edit) { mEditOrdinal = edit; }

private:
    float parseAlpha(AbstractOverlayEntry* entry) const
    {
        const QStringList ss = entry->usage().split(QLatin1Char(';'));
        for (const QString &s1 : ss) {
            QString s = s1.trimmed();
            if (s.startsWith(QLatin1String("alpha="))) {
                bool ok;
                float alpha = s.remove(0, 6).toFloat(&ok);
                return ok ? alpha : 1.0f;
            }
        }
        return 1.0f;
    }

private:
    ContainerOverlayView *mView;
    EditAbstractOverlay mEditOrdinal = EditAbstractOverlay::RoomName;
};

void ContainerOverlayDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());

    AbstractOverlay *overlay = m->overlayAt(index);
    AbstractOverlayEntry *entry = m->entryAt(index);
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
        if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(overlay->tileName())) {
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
        Tile *baseTile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(entry->parent()->tileName());
        QRect r = option.rect.adjusted(extra, extra + fontHgt * 2, -extra, -extra);
        r.setRight(r.left() + tileWidth);
        QMargins margins = baseTile->drawMargins(scale);
        for (int i = 0; i < entry->tiles().size(); i++) {
            QString tileName = entry->tiles()[i];
            if (baseTile) {
                painter->drawImage(r.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), baseTile->image());
            }
            if (!tileName.isEmpty()) {
                Tile *tile = (tileName == QLatin1String("none")) ?
                            BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile() :
                            BuildingEditor::BuildingTilesMgr::instance()->tileFor(tileName);
                if (tile) {
                    QMargins margins2 = tile->drawMargins(scale);
                    painter->setOpacity(qreal(parseAlpha(entry)));
                    painter->drawImage(r.adjusted(margins2.left(), margins2.top(), -margins2.right(), -margins2.bottom()), tile->image());
                    painter->setOpacity(qreal(1.0));
                }
            }
            if ((m->dropEntryIndex() == i) && (m->dropIndex() == index)) {
                painter->drawRect(r);
            }
            if ((m->dropEntryIndex() == -1) && (option.state & QStyle::State_MouseOver)) {
                if (mView->getHoverIndex() == index && mView->getHoverEntryIndex() == i) {
                    painter->drawRect(r);
                }
            }
            r.translate(tileWidth + extra, 0);
        }
        if (m->moreThan2Tiles()) {
            if (baseTile) {
                painter->setOpacity(qreal(0.25));
                painter->drawImage(r.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), baseTile->image());
                painter->setOpacity(qreal(1.0));
            }
            if ((m->dropEntryIndex() == entry->tiles().size()) && (m->dropIndex() == index)) {
                painter->drawRect(r);
            }
        }
    }

    if (entry) {
        // Draw the room name
        QRectF textRect;
        painter->drawText(option.rect.adjusted(extra, extra, 0, 0),
                          Qt::AlignTop | Qt::AlignLeft,
                          entry->roomName(),
                          &textRect);

        painter->drawText(option.rect.adjusted(extra, extra + fontHgt, 0, 0),
                          Qt::AlignTop | Qt::AlignLeft,
                          QString::fromUtf8("usage: %1").arg(entry->usage()),
                          &textRect);

        // FIXME: this base class shouldn't know anything about TileOverlayEntry
        if (TileOverlayEntry* toe = dynamic_cast<TileOverlayEntry*>(entry)) {
            painter->drawText(option.rect.adjusted(extra + 150, extra, 0, 0),
                              Qt::AlignTop | Qt::AlignLeft,
                              tr("chance: 1 in %1").arg(toe->mChance),
                              &textRect);
        }
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

QSize ContainerOverlayDelegate::sizeHint(const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    const qreal scale = this->scale();
    const int extra = 2;
    AbstractOverlay *overlay = m->overlayAt(index);
    AbstractOverlayEntry *entry = m->entryAt(index);
    int tileWidth = 64 * scale, tileHeight = 128 * scale;
    int fontHgt = overlay ? 0 : option.fontMetrics.lineSpacing();
    QSize size(extra + tileWidth + extra, extra + fontHgt * 2 + tileHeight + extra);

    if (entry) {
        int nTiles = entry->tiles().size();
        if (m->moreThan2Tiles()) {
            nTiles++;
        }
        size.setWidth((extra + tileWidth) * nTiles + extra);
        if (m->moreThan2Tiles()) {
            // Leave room for "chance = 1 in 10"
            size.setWidth(qMax(size.width(), 150 + 150));
        }
    }

    return size;
}

int ContainerOverlayDelegate::dropCoords(const QPoint &dragPos,
                                         const QModelIndex &index)
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    QRect r = mView->visualRect(index);
    AbstractOverlay *overlay = m->overlayAt(index);
    AbstractOverlayEntry *entry = m->entryAt(index);
    const int extra = 2;
    int tileWid = 64 * scale();
    if (overlay)
        return 0;
    if (entry) {
        int nTiles = entry->tiles().size();
        if (m->moreThan2Tiles()) {
            nTiles++;
        }
        int index = (dragPos.x() - r.x()) / (extra + tileWid);
        if (index < 0 || index >= nTiles)
            return -1;
        return index;
    }
    return -1;
}

QWidget *ContainerOverlayDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &/*option*/, const QModelIndex &/*index*/) const
{
    switch (mEditOrdinal) {
    case EditAbstractOverlay::RoomName: {
        QLineEdit *editor = new QLineEdit(parent);
        editor->installEventFilter(const_cast<ContainerOverlayDelegate*>(this));
        return editor;
    }
    case EditAbstractOverlay::Usage: {
        QLineEdit *editor = new QLineEdit(parent);
        editor->installEventFilter(const_cast<ContainerOverlayDelegate*>(this));
        return editor;
    }
    case EditAbstractOverlay::Chance: {
        // FIXME: this base class shouldn't know anything about TileOverlayEntry
        QSpinBox *editor = new QSpinBox(parent);
        editor->installEventFilter(const_cast<ContainerOverlayDelegate*>(this));
        return editor;
    }
    default:
        return nullptr;
    }
}

void ContainerOverlayDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    const ContainerOverlayModel *m = static_cast<const ContainerOverlayModel*>(index.model());
    AbstractOverlayEntry *entry = m->entryAt(index);
    switch (mEditOrdinal) {
    case EditAbstractOverlay::RoomName: {
        static_cast<QLineEdit*>(editor)->setText(entry ? entry->roomName() : QLatin1String(""));
        break;
    }
    case EditAbstractOverlay::Usage: {
        static_cast<QLineEdit*>(editor)->setText(entry ? entry->usage() : QLatin1String(""));
        break;
    }
    case EditAbstractOverlay::Chance: {
        // FIXME: this base class shouldn't know anything about TileOverlayEntry
        TileOverlayEntry* toe = static_cast<TileOverlayEntry*>(entry);
        static_cast<QSpinBox*>(editor)->setValue(toe->mChance);
        static_cast<QSpinBox*>(editor)->setRange(1, 100000);
        break;
    }
    }
}

void ContainerOverlayDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    ContainerOverlayModel *m = static_cast<ContainerOverlayModel*>(model);
    AbstractOverlayEntry *entry = m->entryAt(index);
    if (entry) {
        switch (mEditOrdinal) {
        case EditAbstractOverlay::RoomName: {
            QString text = static_cast<QLineEdit*>(editor)->text().trimmed();
            if (!text.isEmpty() && !text.contains(QLatin1String(" ")) && text != entry->roomName()) {
                emit m->entryRoomNameEdited(entry, text);
            }
            break;
        }
        case EditAbstractOverlay::Usage: {
            QString text = static_cast<QLineEdit*>(editor)->text().trimmed();
            if (text != entry->usage()) {
                emit m->entryUsageEdited(entry, text);
            }
            break;
        }
        case EditAbstractOverlay::Chance: {
            // FIXME: this base class shouldn't know anything about TileOverlayEntry
            QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
            if (spinBox->value() != static_cast<TileOverlayEntry*>(entry)->mChance) {
                emit m->entryChanceEdited(entry, spinBox->value());
            }
            break;
        }
        }
    }
}

void ContainerOverlayDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const
{
    QRect rect = option.rect;
    switch (mEditOrdinal) {
    case EditAbstractOverlay::RoomName:
        rect.setWidth(150);
        break;
    case EditAbstractOverlay::Usage:
        rect.adjust(0, option.fontMetrics.lineSpacing(), 0, 0);
        rect.setWidth(150);
        break;
    case EditAbstractOverlay::Chance:
        // FIXME: this base class shouldn't know anything about TileOverlayEntry
        rect.adjust(150, 0, 0, 0);
        rect.setWidth(100);
        break;
    }
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

QModelIndex ContainerOverlayModel::index(AbstractOverlay *overlay)
{
    int itemIndex = mItems.indexOf(toItem(overlay));
    if (itemIndex != -1)
        return index(itemIndex / columnCount(), itemIndex % columnCount());
    return QModelIndex();
}

QModelIndex ContainerOverlayModel::index(AbstractOverlayEntry *entry)
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

     if (item->mEntry && (mDropEntryIndex == -1))
         return false;

     QByteArray encodedData = data->data(mMimeType);
     QDataStream stream(&encodedData, QIODevice::ReadOnly);

     QStringList tileNames;
     while (!stream.atEnd()) {
         QString tilesetName;
         stream >> tilesetName;
         int tileId;
         stream >> tileId;
         QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tilesetName, tileId);
         tileNames += tileName;
     }

     if (item->mOverlay) {
        emit tileDropped(item->mOverlay, tileNames);
     }
     if (item->mEntry) {
         emit tileDropped(item->mEntry, mDropEntryIndex, tileNames);
     }

     return true;
}

void ContainerOverlayModel::clear()
{
    setOverlays(QList<AbstractOverlay*>());
}

void ContainerOverlayModel::setOverlays(const QList<AbstractOverlay *> &overlays)
{
    beginResetModel();

    qDeleteAll(mItems);
    mItems.clear();

    for (AbstractOverlay *overlay : overlays) {
        mItems += new Item(overlay);
        for (int i = 0; i < overlay->entryCount(); i++) {
            AbstractOverlayEntry *entry = overlay->entry(i);
            mItems += new Item(entry);
        }
    }

    endResetModel();
}

AbstractOverlay *ContainerOverlayModel::overlayAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mOverlay;
    return nullptr;
}

AbstractOverlayEntry *ContainerOverlayModel::entryAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mEntry;
    return nullptr;
}

void ContainerOverlayModel::insertOverlay(int index, AbstractOverlay *overlay)
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
    beginInsertRows(QModelIndex(), row, row + overlay->entryCount());
    mItems.insert(row, item);
    for (int i = 0; i < overlay->entryCount(); i++) {
        AbstractOverlayEntry *entry = overlay->entry(i);
        mItems.insert(++row, new Item(entry));
    }
    endInsertRows();
}

void ContainerOverlayModel::removeOverlay(AbstractOverlay *overlay)
{
    Item *item = toItem(overlay);
    int row = mItems.indexOf(item) / columnCount();
    beginRemoveRows(QModelIndex(), row, row + overlay->entryCount());
    int i = mItems.indexOf(item);
    delete mItems.takeAt(i);
    for (int j = 0; j < overlay->entryCount(); j++) {
        delete mItems.takeAt(i);
    }
    endRemoveRows();
}

void ContainerOverlayModel::insertEntry(AbstractOverlay *overlay, int index,
                                        AbstractOverlayEntry *entry)
{
    int row = this->index(overlay).row();
    beginInsertRows(QModelIndex(), row + index + 1, row + index + 1);
    mItems.insert(row + index + 1, new Item(entry));
    endInsertRows();
}

void ContainerOverlayModel::removeEntry(AbstractOverlay *overlay, int index)
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

void ContainerOverlayModel::redisplay(AbstractOverlay *overlay)
{
    int row = index(overlay).row();
    emit dataChanged(index(row, 0), index(row + overlay->entryCount(), 0));
}

void ContainerOverlayModel::redisplay(AbstractOverlayEntry *entry)
{
    emit dataChanged(index(entry), index(entry));
}

void ContainerOverlayModel::setMoreThan2Tiles(bool moreThan2)
{
    mMoreThan2Tiles = moreThan2;
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return nullptr;
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(AbstractOverlay *overlay) const
{
    foreach (Item *item, mItems)
        if (item->mOverlay == overlay)
            return item;
    return nullptr;
}

ContainerOverlayModel::Item *ContainerOverlayModel::toItem(AbstractOverlayEntry *entry) const
{
    foreach (Item *item, mItems)
        if (item->mEntry == entry)
            return item;
    return nullptr;
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

void ContainerOverlayView::leaveEvent(QEvent *event)
{
    QTableView::leaveEvent(event);

    if (mHoverIndex.isValid()) {
        mHoverIndex = QModelIndex();
        mHoverEntryIndex = -1;
        redisplay();
        emit overlayEntryHover(mHoverIndex, mHoverEntryIndex);
    }
}

void ContainerOverlayView::mouseMoveEvent(QMouseEvent *event)
{
    QTableView::mouseMoveEvent(event);

    QModelIndex index = indexAt(event->pos());

     if (index.isValid() && (model()->entryAt(index) == nullptr)) {
        index = QModelIndex();
    }
    if (index == mHoverIndex) {
        if (index.isValid()) {
            int entryIndex = mDelegate->dropCoords(event->pos(), index);
            if (entryIndex != mHoverEntryIndex) {
                mHoverEntryIndex = entryIndex;
                redisplay();
                emit overlayEntryHover(mHoverIndex, mHoverEntryIndex);
            }
        }
    } else {
        mHoverIndex = index;
        mHoverEntryIndex = -1;
        if (index.isValid()) {
            mHoverEntryIndex = mDelegate->dropCoords(event->pos(), index);
        }
        redisplay();
        emit overlayEntryHover(mHoverIndex, mHoverEntryIndex);
    }
}

void ContainerOverlayView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    const int extra = 2;
    const int fontHgt = fontMetrics().lineSpacing();
    if (AbstractOverlayEntry *entry = model()->entryAt(index)) {
        if (event->pos().y() < visualRect(index).y() + extra + fontHgt) {
            // FIXME: this base class shouldn't know anything about TileOverlayEntry
            if (TileOverlayEntry* toe = dynamic_cast<TileOverlayEntry*>(entry)) {
                if (event->pos().x() > visualRect(index).x() + 150) {
                    edit(index, EditAbstractOverlay::Chance);
                    return;
                }
            }
            edit(index, EditAbstractOverlay::RoomName);
            return;
        }
        if (event->pos().y() < visualRect(index).y() + extra + fontHgt * 2) {
            edit(index, EditAbstractOverlay::Usage);
            return;
        }
    }

    QTableView::mouseDoubleClickEvent(event);
}

void ContainerOverlayView::wheelEvent(QWheelEvent *event)
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

void ContainerOverlayView::edit(const QModelIndex &index, EditAbstractOverlay which)
{
    mDelegate->setEditOrdinal(which);
    QTableView::edit(index);
}

void ContainerOverlayView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void ContainerOverlayView::setMoreThan2Tiles(bool moreThan2)
{
    model()->setMoreThan2Tiles(moreThan2);
}

void ContainerOverlayView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void ContainerOverlayView::setOverlays(const QList<AbstractOverlay *> &overlays)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setOverlays(overlays);
}

void ContainerOverlayView::redisplay()
{
    model()->redisplay();
}

void ContainerOverlayView::redisplay(AbstractOverlay *overlay)
{
    model()->redisplay(overlay);
}

void ContainerOverlayView::redisplay(AbstractOverlayEntry *entry)
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

void ContainerOverlayView::contextMenuEvent(QContextMenuEvent *event)
{
    const QModelIndex index = indexAt(event->pos());
    ContainerOverlayModel *m = model();
    if (AbstractOverlayEntry *entry = m->entryAt(index)) {
        int entryIndex = mDelegate->dropCoords(event->pos(), index);
        emit showContextMenu(index, entryIndex, event);
    }
}

void ContainerOverlayView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
    setItemDelegate(mDelegate);
    setShowGrid(false);

    setSelectionMode(QAbstractItemView::ExtendedSelection);

    setAcceptDrops(true);

    setMouseTracking(true);

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
