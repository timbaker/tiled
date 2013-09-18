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

#include "tilelayerspanel.h"

#include "bmpblender.h"
//#include "layermodel.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "preferences.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "tile.h"
#include "tileset.h"

#include <qmath.h>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QWheelEvent>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace Tiled {
namespace Internal {

class LayersPanelDelegate : public QAbstractItemDelegate
{
public:
    LayersPanelDelegate(LayersPanelView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

    void setMouseOver(const QModelIndex &index);

    QModelIndex mouseOverIndex() const
    { return mMouseOverIndex; }

private:
    LayersPanelView *mView;
    QModelIndex mMouseOverIndex;
};

} // namepace Internal
} // namespace Tiled

void LayersPanelDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const LayersPanelModel *m = static_cast<const LayersPanelModel*>(index.model());

    QBrush brush = qvariant_cast<QBrush>(m->data(index, Qt::BackgroundRole));
    painter->fillRect(option.rect, brush);

    if (index.row() > 0 && !(option.state & QStyle::State_Selected)) {
        painter->setPen(Qt::darkGray);
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        painter->setPen(Qt::black);
    }

    // Note: BuildingTilesMgr::instance()->noneTiledTile() is used for valid
    // model indices with no tile in the layer.
    Tile *tile = m->tileAt(index);
    if (!tile)
        return;
    if (tile == BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile())
        tile = 0;

    const int extra = 2;

    const QFontMetrics fm = painter->fontMetrics();
    const int labelHeight = fm.lineSpacing();

    // Draw the tile image
    if (tile != 0) {
        const QVariant display = index.model()->data(index, Qt::DisplayRole);
        const QPixmap tileImage = display.value<QPixmap>();
        const int tileWidth = qCeil(tile->tileset()->tileWidth() * mView->zoomable()->scale());

        if (mView->zoomable()->smoothTransform())
            painter->setRenderHint(QPainter::SmoothPixmapTransform);

        const int dw = option.rect.width() - tileWidth;
        painter->drawPixmap(option.rect.adjusted(dw/2, extra + labelHeight + extra,
                                                 -(dw - dw/2), -extra), tileImage);
    }
#if 0
    // Overlay with highlight color when selected
    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          option.palette.brush(cg, QPalette::Highlight));
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

    const QPen oldPen = painter->pen();

    // Rect around current layer
    if (option.state & QStyle::State_Selected) {
        QPen pen;
        pen.setWidth(2);
        painter->setPen(pen);
        painter->drawRect(option.rect.adjusted(1,1,-1,-1));
        painter->setPen(oldPen);
    }

    // Draw the layer name.  Underline it if the mouse is over it.
    QString label = index.data(Qt::DecorationRole).toString();
    QString name = fm.elidedText(label, Qt::ElideRight, option.rect.width());
    const QFont oldFont = painter->font();
    if (mMouseOverIndex == index) {
        QFont newFont = oldFont;
        newFont.setUnderline(true);
        painter->setFont(newFont);
    }
    if (!tile)
        painter->setPen(Qt::gray);
    painter->drawText(option.rect.left(), option.rect.top() + 2,
                      option.rect.width(), labelHeight, Qt::AlignHCenter, name);
    if (!tile)
        painter->setPen(oldPen);
    if (mMouseOverIndex == index)
        painter->setFont(oldFont);

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

QSize LayersPanelDelegate::sizeHint(const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    const LayersPanelModel *m = static_cast<const LayersPanelModel*>(index.model());
    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;
    const QFontMetrics &fm = option.fontMetrics;
    const int labelHeight = fm.lineSpacing();
    if (!m->tileAt(index))
        return QSize(64 * zoom + extra, 2 + labelHeight + 128 * zoom + extra);
    const Tileset *tileset = m->tileAt(index)->tileset();
    const int tileWidth = qCeil(tileset->tileWidth() * zoom);
    const int viewWidth = mView->viewport()->width();
    return QSize(qMax(qMax(tileWidth, mView->maxHeaderWidth()) + extra, viewWidth),
                 2 + labelHeight + qCeil(tileset->tileHeight() * zoom) + extra);
}

void LayersPanelDelegate::setMouseOver(const QModelIndex &index)
{
    mMouseOverIndex = index;
}

/////

LayersPanelModel::LayersPanelModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

LayersPanelModel::~LayersPanelModel()
{
    qDeleteAll(mItems);
}

int LayersPanelModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mItems.size();
}

int LayersPanelModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

Qt::ItemFlags LayersPanelModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    return flags;
}

QVariant LayersPanelModel::data(const QModelIndex &index, int role) const
{
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

bool LayersPanelModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Item *item = toItem(index)) {
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
                emit dataChanged(index, index);
                return true;
            }
        }
        if (role == Qt::ToolTipRole) {
            if (value.canConvert<QString>()) {
                item->mToolTip = value.toString();
                return true;
            }
        }
    }
    return false;
}

QVariant LayersPanelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex LayersPanelModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    if (row * columnCount() + column >= mItems.count())
        return QModelIndex();

    Item *item = mItems.at(row * columnCount() + column);
    return createIndex(row, column, item);
}

QModelIndex LayersPanelModel::index(int layerIndex) const
{
    if (Item *item = toItem(layerIndex))
        return index(mItems.indexOf(item), 0);
    return QModelIndex();
}

void LayersPanelModel::clear()
{
    if (!mItems.size())
        return;
    beginRemoveRows(QModelIndex(), 0, mItems.size() - 1);
    qDeleteAll(mItems);
    mItems.clear();
    endRemoveRows();
}

void LayersPanelModel::prependLayer(const QString &layerName, Tile *tile, int layerIndex)
{
    beginInsertRows(QModelIndex(), 0, 0);
    Item *item = new Item(layerName, tile, layerIndex);
    mItems.prepend(item);
    endInsertRows();
}

int LayersPanelModel::layerAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mLayerIndex;
    return -1;
}

Tile *LayersPanelModel::tileAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mTile;
    return 0;
}

QString LayersPanelModel::labelAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mLabel;
    return QString();
}

void LayersPanelModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

LayersPanelModel::Item *LayersPanelModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

LayersPanelModel::Item *LayersPanelModel::toItem(int layerIndex) const
{
    foreach (Item *item, mItems) {
        if (item->mLayerIndex == layerIndex)
            return item;
    }
    return 0;
}

/////

LayersPanelView::LayersPanelView(QWidget *parent) :
    QTableView(parent),
    mModel(new LayersPanelModel(this)),
    mDelegate(new LayersPanelDelegate(this, this)),
    mZoomable(new Zoomable(this)),
    mContextMenu(0),
    mMaxHeaderWidth(0),
    mIgnoreMouse(false)
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

QSize LayersPanelView::sizeHint() const
{
    return QSize(64, 128);
}

bool LayersPanelView::viewportEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        QModelIndex mouseOver;
        QPoint pos = static_cast<QHoverEvent*>(event)->pos();
        QModelIndex index = indexAt(pos);
        int layerIndex = model()->layerAt(index);
        if (layerIndex >= 0) {
            QRect r = visualRect(index);
            if (pos.y() < r.top() + 2 + fontMetrics().lineSpacing())
                mouseOver = index;
        }
        if (mouseOver != mDelegate->mouseOverIndex()) {
            if (mDelegate->mouseOverIndex().isValid())
                update(mDelegate->mouseOverIndex());
            mDelegate->setMouseOver(mouseOver);
            update(mouseOver);
        }
        break;
    }
    case QEvent::HoverLeave: {
        QModelIndex index = mDelegate->mouseOverIndex();
        if (index.isValid()) {
            mDelegate->setMouseOver(QModelIndex());
            update(index);
        }
    }
    default:
            break;
    }

    return QTableView::viewportEvent(event);
}

void LayersPanelView::mouseMoveEvent(QMouseEvent *event)
{
    if (mIgnoreMouse)
        return;

    QTableView::mouseMoveEvent(event);
}

void LayersPanelView::mousePressEvent(QMouseEvent *event)
{
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

    QTableView::mousePressEvent(event);
}

void LayersPanelView::mouseReleaseEvent(QMouseEvent *event)
{
    if (mIgnoreMouse) {
        mIgnoreMouse = false;
        return;
    }
    QTableView::mouseReleaseEvent(event);
}

void LayersPanelView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QModelIndex index = indexAt(event->pos());
    int layerIndex = model()->layerAt(index);
    if (layerIndex >= 0) {
        QRect r = visualRect(index);
        if (event->pos().y() < r.top() + 2 + fontMetrics().lineSpacing()) {
            emit layerNameClicked(layerIndex);
            mIgnoreMouse = true;
            return; // don't activated() the item
        }
    }

    QTableView::mouseDoubleClickEvent(event);
}

void LayersPanelView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void LayersPanelView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void LayersPanelView::clear()
{
    mDelegate->setMouseOver(QModelIndex());
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
    mMaxHeaderWidth = 0;
}

void LayersPanelView::prependLayer(const QString &layerName, Tile *tile, int layerIndex)
{
    int width = fontMetrics().width(layerName);
    mMaxHeaderWidth = qMax(mMaxHeaderWidth, width);

    model()->prependLayer(layerName, tile, layerIndex);
}

void LayersPanelView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

/////

TileLayersPanel::TileLayersPanel(QWidget *parent) :
    QWidget(parent),
    mDocument(0),
    mView(new LayersPanelView(this)),
    mCurrentLevel(-1),
    mCurrentLayerIndex(-1)
{
    mView->zoomable()->setScale(0.25);

    QComboBox *scaleCombo = new QComboBox;
    mView->zoomable()->connectToComboBox(scaleCombo);

    connect(mView->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(currentChanged()));
    connect(mView, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));
    connect(mView, SIGNAL(layerNameClicked(int)), SLOT(layerNameClicked(int)));
    connect(Preferences::instance(), SIGNAL(showTileLayersPanelChanged(bool)),
            SLOT(showTileLayersPanelChanged(bool)));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);
    layout->addWidget(mView);
    layout->addWidget(scaleCombo);
    setLayout(layout);

    setVisible(Preferences::instance()->showTileLayersPanel());
}

void TileLayersPanel::setDocument(MapDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    mView->clear();

    if (mDocument) {
        connect(mDocument, SIGNAL(currentLayerIndexChanged(int)),
                SLOT(layerIndexChanged(int)));
        connect(mDocument, SIGNAL(layerAddedToGroup(int)), SLOT(setList()));
        connect(mDocument, SIGNAL(layerRemovedFromGroup(int,CompositeLayerGroup*)),
                SLOT(setList()));
        connect(mDocument, SIGNAL(layerChanged(int)), SLOT(layerChanged(int)));
        connect(mDocument, SIGNAL(regionAltered(QRegion,Layer*)),
                SLOT(regionAltered(QRegion,Layer*)));
        connect(mDocument, SIGNAL(noBlendPainted(MapNoBlend*,QRegion)),
                SLOT(noBlendPainted(MapNoBlend*,QRegion)));

        mCurrentLayerIndex = mDocument->currentLayerIndex();
        setList();
    }
}

void TileLayersPanel::setScale(qreal scale)
{
    mView->zoomable()->setScale(scale);
}

qreal TileLayersPanel::scale() const
{
    return mView->zoomable()->scale();
}

void TileLayersPanel::setTilePosition(const QPoint &tilePos)
{
    if (mTilePos == tilePos)
        return;

    mTilePos = tilePos;

#if 1
    setList();
#else
    // Not calling setList() only because it scrolls the view when setting
    // the current index.

    int level = mDocument->currentLevel();
    CompositeLayerGroup *lg = mDocument->mapComposite()->layerGroupForLevel(level);
    if (!lg) return;

    QStringList blendLayerNames = mDocument->mapComposite()->bmpBlender()->blendLayers();

    int index = 0;
    foreach (TileLayer *tl, lg->layers()) {
        Tile *tile = tl->contains(mTilePos) ? tl->cellAt(mTilePos).tile : 0;
        TileLayer *blendLayer = lg->bmpBlendLayers().at(index);
        if (blendLayer && blendLayer->contains(tilePos))
            if (!blendLayerNames.contains(tl->name()) ||
                    !mDocument->map()->noBlend(tl->name())->get(tilePos.x(), tilePos.y()))
                if (Tile *blendTile = blendLayer->cellAt(tilePos).tile)
                    tile = blendTile;
        if (!tile)
            tile = BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
        int layerIndex = mDocument->map()->layers().indexOf(tl);
        mView->model()->setData(mView->model()->index(layerIndex),
                                QVariant::fromValue(tile), Qt::DisplayRole);
        ++index;
    }
#endif
}

void TileLayersPanel::setList()
{
    mCurrentLevel = mDocument->currentLevel();

    mView->clear();

    CompositeLayerGroup *lg = mDocument->mapComposite()->layerGroupForLevel(mCurrentLevel);
    if (!lg) return;

    QStringList blendLayerNames = mDocument->mapComposite()->bmpBlender()->blendLayers();

    int index = 0;
    foreach (TileLayer *tl, lg->layers()) {
        QString layerName = MapComposite::layerNameWithoutPrefix(tl);
        Tile *tile = tl->contains(mTilePos) ? tl->cellAt(mTilePos).tile : 0;
        TileLayer *blendLayer = lg->bmpBlendLayers().at(index);
        if (blendLayer && blendLayer->contains(mTilePos))
            if (!blendLayerNames.contains(tl->name()) ||
                    !mDocument->map()->noBlend(tl->name())->get(mTilePos))
                if (Tile *blendTile = blendLayer->cellAt(mTilePos).tile)
                    tile = blendTile;
        if (!tile)
            tile = BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile();
        int layerIndex = mDocument->map()->layers().indexOf(tl);
        mView->prependLayer(layerName, tile, layerIndex);

        QBrush brush(tl->isVisible() ? Qt::white : Qt::lightGray);
        mView->model()->setData(mView->model()->index(layerIndex), brush, Qt::BackgroundRole);
        ++index;
    }

    mView->setAutoScroll(false);
    mView->setCurrentIndex(mView->model()->index(mCurrentLayerIndex));
    mView->setAutoScroll(true);
}

void TileLayersPanel::activated(const QModelIndex &index)
{
#if 1
    Tile *tile = mView->model()->tileAt(index);
    if (tile && tile != BuildingEditor::BuildingTilesMgr::instance()->noneTiledTile())
        emit tilePicked(tile);
#else
    int layerIndex = mView->model()->layerAt(index);
    if (layerIndex >= 0 && layerIndex < mDocument->map()->layerCount()) {
        Layer *layer = mDocument->map()->layerAt(layerIndex);
        if (!layer || !layer->asTileLayer())
            return;
        TileLayer *tl = layer->asTileLayer();
        Tile *tile = tl->contains(mTilePos) ? tl->cellAt(mTilePos).tile : 0;
        if (tl->contains(mTilePos) && )
        if (CompositeLayerGroup *lg = mDocument->mapComposite()->layerGroupForLevel(mCurrentLevel)) {
            int index = lg->layers().indexOf(layer->asTileLayer());
            TileLayer *blendLayer = lg->bmpBlendLayers().at(index);
            if (blendLayer && blendLayer->contains(mTilePos)) {
                if (!mDocument->map()->noBlend(layer->name())->get(mTilePos.x(), mTilePos.y())) {
                    Tile *tile = blendLayer->cellAt(mTilePos).tile;
                    emit tilePicked(tile);
                    return;
                }
            }
        }
        if (tile)
            emit tilePicked(tl->cellAt(mTilePos).tile);
    }
#endif
}

void TileLayersPanel::layerNameClicked(int layerIndex)
{
    Layer *layer = mDocument->map()->layerAt(layerIndex);
    mDocument->setLayerVisible(layerIndex, !layer->isVisible());
}

void TileLayersPanel::currentChanged()
{
    QModelIndex index = mView->currentIndex();
    int layerIndex = mView->model()->layerAt(index);
    if (layerIndex >= 0 && layerIndex < mDocument->map()->layerCount()) {
        if (layerIndex == mCurrentLayerIndex)
            return;
        mCurrentLayerIndex = layerIndex;
        mDocument->setCurrentLayerIndex(layerIndex);
    }
}

void TileLayersPanel::layerIndexChanged(int layerIndex)
{
    if (mCurrentLayerIndex == layerIndex)
        return;
    mCurrentLayerIndex = layerIndex;
    if (layerIndex >= 0 && layerIndex < mDocument->map()->layerCount()) {
        Layer *layer = mDocument->map()->layerAt(layerIndex);
        if ((layer->level() == mCurrentLevel) && !layer->isTileLayer()) {
            mView->setCurrentIndex(QModelIndex());
            return;
        }
    }
    setList();
}

void TileLayersPanel::layerChanged(int index)
{
    Layer *layer = mDocument->map()->layerAt(index);
    if (layer->asTileLayer() && (layer->level() == mDocument->currentLevel())) {
        QModelIndex mi = mView->model()->index(index);
        if (mi.isValid()) {
            QString name = MapComposite::layerNameWithoutPrefix(layer);
            mView->model()->setData(mi, name, Qt::DecorationRole);
            QBrush brush(layer->isVisible() ? Qt::white : Qt::lightGray);
            mView->model()->setData(mi, brush, Qt::BackgroundRole);
        }
    }
}

void TileLayersPanel::regionAltered(const QRegion &region, Layer *layer)
{
    if (layer->asTileLayer() && (layer->level() == mDocument->currentLevel())
            && region.contains(mTilePos))
        setList();
}

void TileLayersPanel::noBlendPainted(MapNoBlend *noBlend, const QRegion &rgn)
{
    Q_UNUSED(noBlend)
    Q_UNUSED(rgn)
    if (mDocument->currentLevel() == 0)
        setList();
}

void TileLayersPanel::showTileLayersPanelChanged(bool show)
{
    setVisible(show);
}
