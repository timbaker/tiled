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

#include "bmpblendview.h"

#include "tilemetainfomgr.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "tile.h"
#include "tileset.h"

#include <qmath.h>
#include <QHeaderView>
#include <QMenu>
#include <QWheelEvent>

using namespace Tiled;
using namespace Tiled::Internal;
using namespace BuildingEditor;

/////

namespace Tiled {
namespace Internal {

class BmpBlendDelegate : public QAbstractItemDelegate
{
public:
    BmpBlendDelegate(BmpBlendView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
        , mMouseOverDir(-1)
        , mLabelFontMetrics(mLabelFont)
    {
        if (true/*mFont != mView->font()*/) {
            mFont = mView->font();
            mLabelFont = mFont;
            mLabelFont.setBold(true);
            if (mLabelFont.pixelSize() != -1)
                mLabelFont.setPixelSize(mLabelFont.pixelSize() + 4);
            else if (mLabelFont.pointSize() != -1)
                mLabelFont.setPointSize(mLabelFont.pointSize() + 1);
            mLabelFontMetrics = QFontMetrics(mLabelFont);
        }
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    int blendAt(const QPoint &p, const QRect &viewRect);
    void setMouseOver(const QModelIndex &index, int dir);

    QModelIndex mouseOverIndex() const
    { return mMouseOverIndex; }
    int mouseOverDir() const { return mMouseOverDir; }

private:
    BmpBlendView *mView;
    QModelIndex mMouseOverIndex;
    int mMouseOverDir;
    QFont mFont;
    QFont mLabelFont;
    QFontMetrics mLabelFontMetrics;
};

} // namepace Internal
} // namespace Tiled

void BmpBlendDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const BmpBlendModel *m = static_cast<const BmpBlendModel*>(index.model());

    QBrush brush(QColor(255, 255, 255));
    painter->fillRect(option.rect, brush);

    if (index.row() > 0 && !(option.state & QStyle::State_Selected)) {
        painter->setPen(Qt::darkGray);
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        painter->setPen(Qt::black);
    }

    QVector<BmpBlend*> blends = m->blendsAt(index);
    if (blends.isEmpty()) return;

    const int extra = 2;
#if 0
    // Overlay with highlight color when selected
    if ((option.state & QStyle::State_Selected) && (option.state & QStyle::State_Active)) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;
        painter->fillRect(option.rect/*.adjusted(extra, extra, -extra, -extra)*/,
                          option.palette.brush(cg, QPalette::Highlight));
        painter->setOpacity(opacity);
    } else if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.15);
        painter->fillRect(option.rect, option.palette.highlight());
        painter->setOpacity(opacity);
    }
#endif
    // This requires the view's setMouseTracking(true)
    if (option.state & QStyle::State_MouseOver) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.15);
        painter->fillRect(option.rect, option.palette.highlight());
        painter->setOpacity(opacity);
    }

    const QFontMetrics fm = painter->fontMetrics();
    const int labelHeight = fm.lineSpacing();
    qreal scale = mView->zoomable()->scale();
    const int tileWidth = qCeil(64 * scale);
    const int tileHeight = qCeil(128 * scale);
//    const int columns = qMax(8, option.rect.width() / tileWidth);

    // Mouse-over tile highlight
    if (index == mMouseOverIndex && mMouseOverDir != -1) {
        QRect r(option.rect.left() + extra + mMouseOverDir * tileWidth,
                option.rect.top() + extra + labelHeight + extra,
                tileWidth, tileHeight);
        painter->fillRect(r, Qt::lightGray);
    }

    // Rect around current blend
    if (option.state & QStyle::State_Selected) {
        QPen oldPen = painter->pen();
        QPen pen;
        pen.setWidth(2);
        painter->setPen(pen);
        painter->drawRect(option.rect.adjusted(1,1,-1,-1));
        painter->setPen(oldPen);
    }

    // Draw the tiles
    for (int i = BmpBlend::Unknown; i <= BmpBlend::SE; i++) {
        if (BmpBlend *blend = blends[i]) {
            QString tileName = i ? blend->blendTile : blend->mainTile;
            QStringList aliasTiles = m->aliasTiles(tileName);
            if (!aliasTiles.isEmpty())
                tileName = aliasTiles.first();
            if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) {
                const int tileWidth = qCeil(tile->tileset()->tileWidth() * scale);
                const int tileHeight = qCeil(tile->tileset()->tileHeight() * scale);

                if (mView->zoomable()->smoothTransform())
                    painter->setRenderHint(QPainter::SmoothPixmapTransform);

                if (tile != BuildingTilesMgr::instance()->noneTiledTile()) {
                    QMargins margins = tile->drawMargins(scale);
                    painter->drawPixmap(QRect(option.rect.left() + extra + i * tileWidth,
                                              option.rect.top() + extra + labelHeight /*+ (n / columns) * tileHeight*/ + extra,
                                              tileWidth, tileHeight).adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()),
                                        QPixmap::fromImage(tile->image()));
                }
            }
        }
    }

    int labelWidth = 0;
    QFont font = painter->font();
    QString label = blends[0] ? blends[0]->mainTile : QString();
    if (label.length()) {
        QPen oldPen = painter->pen();
        painter->setPen(Qt::blue);
        painter->setFont(mLabelFont);
//        labelWidth = mLabelFontMetrics.horizontalAdvance(label) + 6;
        painter->drawText(option.rect.left() + extra, option.rect.top() + extra,
                          option.rect.width() - extra * 2, labelHeight, Qt::AlignLeft, label);
        painter->setFont(font);
        painter->setPen(oldPen);
    }

#if 0
    // Rule header.
    QString label = tr("Rule #%1: index=%2 color=%3,%4,%5")
            .arg(m->blendIndex(blend))
            .arg(blend->bitmapIndex)
            .arg(qRed(blend->color)).arg(qGreen(blend->color)).arg(qBlue(blend->color));
    if (blend->condition != qRgb(0, 0, 0))
        label += QString::fromLatin1(" condition=%1,%2,%3")
                .arg(qRed(blend->condition)).arg(qGreen(blend->condition)).arg(qBlue(blend->condition));
    QString name = fm.elidedText(label, Qt::ElideRight, option.rect.width());
    if (mMouseOverIndex == index) {
        QFont newFont = font;
        newFont.setUnderline(true);
        painter->setFont(newFont);
    }
    painter->drawText(option.rect.left() + extra + labelWidth, option.rect.top() + extra,
                      option.rect.width() - labelWidth - extra * 2, labelHeight, Qt::AlignLeft | Qt::AlignVCenter, name);
    if (mMouseOverIndex == index)
        painter->setFont(font);
#endif
#if 0
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
#endif
}

QSize BmpBlendDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    Q_UNUSED(option)
    const BmpBlendModel *m = static_cast<const BmpBlendModel*>(index.model());
    QVector<BmpBlend*> blends = m->blendsAt(index);
    if (blends.isEmpty()) return QSize();

    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;

    const QFontMetrics &fm = mLabelFontMetrics;
    const int labelHeight = fm.lineSpacing();

    const int tileWidth = qCeil(64 * zoom);
    const int tileHeight = mView->isExpanded() ? qCeil(128 * zoom) : 0;
    const int viewWidth = mView->viewport()->width();
    const int columns = 9;
    return QSize(qMax(qMax(tileWidth * columns, mView->maxHeaderWidth()) + extra, viewWidth),
                 2 + labelHeight + (tileHeight * 1) + extra);
}

int BmpBlendDelegate::blendAt(const QPoint &p, const QRect &viewRect)
{
    int extra = 2;
    const qreal zoom = mView->zoomable()->scale();
    const int tileWidth = qCeil(64 * zoom);
    int n = (p.x() - viewRect.x() - extra) / tileWidth;
    if (n < 0 || n > 8) return -1;
    return n;
}

void BmpBlendDelegate::setMouseOver(const QModelIndex &index, int dir)
{
    mMouseOverIndex = index;
    mMouseOverDir = dir;
}

/////

void BmpBlendModel::Item::addBlend(BmpBlend *blend)
{
    mBlend[blend->dir] = new BmpBlend(blend);
    mBlend[Unknown] = new BmpBlend(blend); // for mainTile
    mOriginalBlend[blend->dir] = blend;
}

BmpBlendModel::Item::~Item()
{
    qDeleteAll(mBlend);
}

/////

BmpBlendModel::BmpBlendModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

BmpBlendModel::~BmpBlendModel()
{
    qDeleteAll(mItems);
}

int BmpBlendModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mItems.size();
}

int BmpBlendModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

Qt::ItemFlags BmpBlendModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    return flags;
}

QVariant BmpBlendModel::data(const QModelIndex &index, int role) const
{
    Q_UNUSED(index)
    Q_UNUSED(role)
#if 0
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
#endif
    return QVariant();
}

bool BmpBlendModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(index)
    Q_UNUSED(value)
    Q_UNUSED(role)
#if 0
    if (Item *item = toItem(index)) {
        if (role == Qt::DisplayRole) {
            if (item->mTile && value.canConvert<Tile*>()) {
                item->mTile = qvariant_cast<Tile*>(value);
                emit dataChanged(index, index);
                return true;
            }
        }
    }
#endif
    return false;
}

QVariant BmpBlendModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex BmpBlendModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    if (row * columnCount() + column >= mItems.count())
        return QModelIndex();

    Item *item = mItems.at(row * columnCount() + column);
    return createIndex(row, column, item);
}

QModelIndex BmpBlendModel::index(BmpBlend *blend) const
{
    if (Item *item = toItem(blend))
        return index(mItems.indexOf(item), 0);
    return QModelIndex();
}

void BmpBlendModel::setBlends(const Map *map)
{
    if (map->bmpSettings()->blends().isEmpty()) {
        mAliasTiles.clear();
        clear();
        return;
    }

    // Groups blends by mainTile
    QMap<QString,Item*> itemMap;
    QList<Item*> items;
    foreach (BmpBlend *blend, map->bmpSettings()->blends()) {
        Item *item = itemMap[blend->mainTile];
        if (!item) {
            item = new Item();
            items += item;
            itemMap[blend->mainTile] = item;
        }
        item->addBlend(blend);
    }

    beginInsertRows(QModelIndex(), 0, items.size() - 1);

    mItems = items;

    mAliasTiles.clear();
    foreach (BmpAlias *alias, map->bmpSettings()->aliases()) {
        mAliasTiles[alias->name] = alias->tiles;
    }

    endInsertRows();
}

void BmpBlendModel::clear()
{
    if (!mItems.size())
        return;
    beginRemoveRows(QModelIndex(), 0, mItems.size() - 1);
    qDeleteAll(mItems);
    mItems.clear();
    endRemoveRows();
}

QVector<BmpBlend*> BmpBlendModel::blendsAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        return item->mBlend;
    }
    return QVector<BmpBlend*>();
}

int BmpBlendModel::blendIndex(BmpBlend *blend) const
{
    if (Item *item = toItem(blend))
        return item->mIndex;
    return -1;
}

void BmpBlendModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

BmpBlendModel::Item *BmpBlendModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

BmpBlendModel::Item *BmpBlendModel::toItem(BmpBlend *blend) const
{
    foreach (Item *item, mItems) {
        if (item->mOriginalBlend.contains(blend))
            return item;
    }
    return 0;
}

/////

BmpBlendView::BmpBlendView(QWidget *parent) :
    QTableView(parent),
    mModel(new BmpBlendModel(this)),
    mDelegate(new BmpBlendDelegate(this, this)),
    mZoomable(new Zoomable(this)),
    mContextMenu(0),
    mMaxHeaderWidth(0),
    mIgnoreMouse(false),
    mExpanded(true)
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setItemDelegate(mDelegate);
    setShowGrid(false);

    setSelectionMode(SingleSelection);

    setMouseTracking(true); // for State_MouseOver
    viewport()->setAttribute(Qt::WA_Hover, true);

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

QSize BmpBlendView::sizeHint() const
{
    return QSize(64, 128);
}

bool BmpBlendView::viewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        QModelIndex mouseOver;
        QPoint pos = static_cast<QHoverEvent*>(event)->pos();
        QModelIndex index = indexAt(pos);
        QVector<BmpBlend*> blends = model()->blendsAt(index);
        int blendDir = -1;
        if (blends.size()) {
            QRect r = visualRect(index);
            blendDir = mDelegate->blendAt(pos, r);
            if (blendDir != -1)
                mouseOver = index;
        }
        if (mouseOver != mDelegate->mouseOverIndex() || blendDir != mDelegate->mouseOverDir()) {
            if (mDelegate->mouseOverIndex().isValid())
                update(mDelegate->mouseOverIndex());
            mDelegate->setMouseOver(mouseOver, blendDir);
            update(mouseOver);
            if (blendDir != -1)
                emit blendHighlighted(blends[blendDir], blendDir);
            else
                emit blendHighlighted(0, -1);
        }
        break;
    }
    case QEvent::HoverLeave: {
        QModelIndex index = mDelegate->mouseOverIndex();
        if (index.isValid()) {
            mDelegate->setMouseOver(QModelIndex(), -1);
            update(index);
            emit blendHighlighted(0, -1);
        }
    }
    default:
            break;
    }

    return QTableView::viewportEvent(event);
}

void BmpBlendView::mouseMoveEvent(QMouseEvent *event)
{
    if (mIgnoreMouse)
        return;

    QTableView::mouseMoveEvent(event);
}

void BmpBlendView::mousePressEvent(QMouseEvent *event)
{
#if 0
    QModelIndex index = indexAt(event->pos());
    int layerIndex = model()->layerAt(index);
    if (layerIndex >= 0) {
        QRect r = visualRect(index);
        if (event->pos().y() < r.top() + 2 + fontMetrics().lineSpacing()) {
            emit layerNameClicked(layerIndex);
            mIgnoreMouse = true;
            return;
        }
    }
#endif
    QTableView::mousePressEvent(event);
}

void BmpBlendView::mouseReleaseEvent(QMouseEvent *event)
{
    if (mIgnoreMouse) {
        mIgnoreMouse = false;
        return;
    }
    QTableView::mouseReleaseEvent(event);
}

void BmpBlendView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QTableView::mouseDoubleClickEvent(event);
}

void BmpBlendView::wheelEvent(QWheelEvent *event)
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

void BmpBlendView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void BmpBlendView::clear()
{
    mDelegate->setMouseOver(QModelIndex(), -1);
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void BmpBlendView::setBlends(const Map *map)
{
#if 0
    QSet<Tileset*> tilesets;
    QMap<QString,BmpAlias*> aliasByName;
    foreach (BmpAlias *alias, map->bmpSettings()->aliases())
        aliasByName[alias->name] = alias;
    QList<BmpBlend*> blends = map->bmpSettings()->blends();
    foreach (BmpBlend *blend, blends) {
        QStringList tileChoices;
        foreach (QString tileName, blend->tileChoices) {
            if (aliasByName.contains(tileName))
                tileChoices += aliasByName[tileName]->tiles;
            else
                tileChoices += tileName;
        }
        foreach (QString tileName, tileChoices) {
            Tile *tile;
            if (tileName.isEmpty())
                tile = BuildingTilesMgr::instance()->noneTiledTile();
            else {
                tile = BuildingTilesMgr::instance()->tileFor(tileName);
                if (tile && TileMetaInfoMgr::instance()->indexOf(tile->tileset()) != -1)
                    tilesets += tile->tileset();
            }
        }
    }
    TileMetaInfoMgr::instance()->loadTilesets(tilesets.toList());
#endif
    model()->setBlends(map);
}

void BmpBlendView::setExpanded(bool expanded)
{
    if (mExpanded != expanded) {
        mExpanded = expanded;
        model()->scaleChanged(mZoomable->scale());
        visualRect(currentIndex());
        QMetaObject::invokeMethod(this, "scrollToCurrentItem", Qt::QueuedConnection);
    }
}

void BmpBlendView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void BmpBlendView::scrollToCurrentItem()
{
    scrollTo(currentIndex(), QTableView::EnsureVisible);
}
