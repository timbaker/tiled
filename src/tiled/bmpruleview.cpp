#include "bmpruleview.h"

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

class BmpRuleDelegate : public QAbstractItemDelegate
{
public:
    BmpRuleDelegate(BmpRuleView *view, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(view)
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

    void setMouseOver(const QModelIndex &index);

    QModelIndex mouseOverIndex() const
    { return mMouseOverIndex; }

private:
    BmpRuleView *mView;
    QModelIndex mMouseOverIndex;
    QFont mFont;
    QFont mLabelFont;
    QFontMetrics mLabelFontMetrics;
};

} // namepace Internal
} // namespace Tiled

void BmpRuleDelegate::paint(QPainter *painter,
                                const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const BmpRuleModel *m = static_cast<const BmpRuleModel*>(index.model());

    QBrush brush(QColor(255, 255, 255));
    painter->fillRect(option.rect, brush);

    if (index.row() > 0 && !(option.state & QStyle::State_Selected)) {
        painter->setPen(Qt::darkGray);
        painter->drawLine(option.rect.topLeft(), option.rect.topRight());
        painter->setPen(Qt::black);
    }

    BmpRule *rule = m->ruleAt(index);
    if (!rule) return;

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

    // Rect around current rule
    if (option.state & QStyle::State_Selected) {
        QPen oldPen = painter->pen();
        QPen pen;
        pen.setWidth(2);
        painter->setPen(pen);
        painter->drawRect(option.rect.adjusted(1,1,-1,-1));
        painter->setPen(oldPen);
    }

    // Draw the tiles
    QStringList tileNames;
    foreach (QString tileName, rule->tileChoices) {
        if (tileName.isEmpty()) continue; // "null"
        QStringList aliasTiles = m->aliasTiles(tileName);
        if (!aliasTiles.isEmpty())
            tileNames += aliasTiles;
        else
            tileNames += tileName;
    }

    if (!mView->isExpanded())
        tileNames.clear();

    const QFontMetrics fm = painter->fontMetrics();
    const int labelHeight = fm.lineSpacing();
    qreal scale = mView->zoomable()->scale();
    const int tileWidth = qCeil(64 * scale);
    const int columns = qMax(8, option.rect.width() / tileWidth);

    int n = 0;
    foreach (QString tileName, tileNames) {
        if (Tile *tile = BuildingTilesMgr::instance()->tileFor(tileName)) {
            const int tileWidth = qCeil(tile->tileset()->tileWidth() * scale);
            const int tileHeight = qCeil(tile->tileset()->tileHeight() * scale);

            if (mView->zoomable()->smoothTransform())
                painter->setRenderHint(QPainter::SmoothPixmapTransform);

            if (tile != BuildingTilesMgr::instance()->noneTiledTile())
                painter->drawPixmap(QRect(option.rect.left() + extra + (n % columns) * tileWidth,
                                          option.rect.top() + extra + labelHeight + (n / columns) * tileHeight + extra,
                                          tileWidth, tileHeight),
                                    QPixmap::fromImage(tile->image()));
            n++;
        }
    }

    int labelWidth = 0;
    QFont font = painter->font();
    if (rule->label.size()) {
        QPen oldPen = painter->pen();
        painter->setPen(Qt::blue);
        painter->setFont(mLabelFont);
        labelWidth = mLabelFontMetrics.width(rule->label) + 6;
        painter->drawText(option.rect.left() + extra, option.rect.top() + extra,
                          option.rect.width() - extra * 2, labelHeight, Qt::AlignLeft, rule->label);
        painter->setFont(font);
        painter->setPen(oldPen);
    }

    // Rule header.
    QString label = tr("Rule #%1: index=%2 color=%3,%4,%5")
            .arg(m->ruleIndex(rule))
            .arg(rule->bitmapIndex)
            .arg(qRed(rule->color)).arg(qGreen(rule->color)).arg(qBlue(rule->color));
    if (rule->condition != qRgb(0, 0, 0))
        label += QString::fromLatin1(" condition=%1,%2,%3")
                .arg(qRed(rule->condition)).arg(qGreen(rule->condition)).arg(qBlue(rule->condition));
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

QSize BmpRuleDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const BmpRuleModel *m = static_cast<const BmpRuleModel*>(index.model());
    BmpRule *rule = m->ruleAt(index);
    if (!rule) return QSize();

    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;

    const QFontMetrics &fm = rule->label.isEmpty() ? option.fontMetrics : mLabelFontMetrics;
    const int labelHeight = fm.lineSpacing();

    QStringList tileNames;
    foreach (QString tileName, rule->tileChoices) {
        if (tileName.isEmpty()) continue; // "null"
        QStringList aliasTiles = m->aliasTiles(tileName);
        if (!aliasTiles.isEmpty())
            tileNames += aliasTiles;
        else
            tileNames += tileName;
    }

    const int tileWidth = qCeil(64 * zoom);
    const int tileHeight = mView->isExpanded() ? qCeil(128 * zoom) : 0;
    const int viewWidth = mView->viewport()->width();
    const int columns = qMax(8, viewWidth / tileWidth);
    return QSize(qMax(qMax(tileWidth, mView->maxHeaderWidth()) + extra, viewWidth),
                 2 + labelHeight + (tileHeight * ((tileNames.size() + columns - 1) / columns)) + extra);
}

void BmpRuleDelegate::setMouseOver(const QModelIndex &index)
{
    mMouseOverIndex = index;
}

/////

BmpRuleModel::Item::Item(BmpRule *rule) :
    mRule(new BmpRule(rule)),
    mOriginalRule(rule)
{

}

BmpRuleModel::Item::~Item()
{
    delete mRule;
}

/////

BmpRuleModel::BmpRuleModel(QObject *parent) :
    QAbstractListModel(parent)
{
}

BmpRuleModel::~BmpRuleModel()
{
    qDeleteAll(mItems);
}

int BmpRuleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return mItems.size();
}

int BmpRuleModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : 1;
}

Qt::ItemFlags BmpRuleModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    return flags;
}

QVariant BmpRuleModel::data(const QModelIndex &index, int role) const
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

bool BmpRuleModel::setData(const QModelIndex &index, const QVariant &value, int role)
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

QVariant BmpRuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex BmpRuleModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    if (row * columnCount() + column >= mItems.count())
        return QModelIndex();

    Item *item = mItems.at(row * columnCount() + column);
    return createIndex(row, column, item);
}

QModelIndex BmpRuleModel::index(BmpRule *rule) const
{
    if (Item *item = toItem(rule))
        return index(mItems.indexOf(item), 0);
    return QModelIndex();
}

void BmpRuleModel::setRules(const Map *map)
{
    if (map->bmpSettings()->rules().isEmpty()) {
        mAliasTiles.clear();
        clear();
        return;
    }

    beginInsertRows(QModelIndex(), 0, map->bmpSettings()->rules().size() - 1);

    int index = 0;
    foreach (BmpRule *rule, map->bmpSettings()->rules()) {
        Item *item = new Item(rule);
        item->mIndex = index++;
        mItems += item;
    }

    mAliasTiles.clear();
    foreach (BmpAlias *alias, map->bmpSettings()->aliases()) {
        mAliasTiles[alias->name] = alias->tiles;
    }

    endInsertRows();
}

void BmpRuleModel::clear()
{
    if (!mItems.size())
        return;
    beginRemoveRows(QModelIndex(), 0, mItems.size() - 1);
    qDeleteAll(mItems);
    mItems.clear();
    endRemoveRows();
}

BmpRule *BmpRuleModel::ruleAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->mOriginalRule;
    return 0;
}

int BmpRuleModel::ruleIndex(BmpRule *rule) const
{
    if (Item *item = toItem(rule))
        return item->mIndex;
    return -1;
}

void BmpRuleModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

BmpRuleModel::Item *BmpRuleModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

BmpRuleModel::Item *BmpRuleModel::toItem(BmpRule *rule) const
{
    foreach (Item *item, mItems) {
        if (item->mOriginalRule == rule)
            return item;
    }
    return 0;
}

/////

BmpRuleView::BmpRuleView(QWidget *parent) :
    QTableView(parent),
    mModel(new BmpRuleModel(this)),
    mDelegate(new BmpRuleDelegate(this, this)),
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

QSize BmpRuleView::sizeHint() const
{
    return QSize(64, 128);
}

bool BmpRuleView::viewportEvent(QEvent *event)
{
#if 0
    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove: {
        QModelIndex mouseOver;
        QPoint pos = static_cast<QHoverEvent*>(event)->pos();
        QModelIndex index = indexAt(pos);
        BmpRule *rule = model()->ruleAt(index);
        if (rule) {
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
#endif
    return QTableView::viewportEvent(event);
}

void BmpRuleView::mouseMoveEvent(QMouseEvent *event)
{
    if (mIgnoreMouse)
        return;

    QTableView::mouseMoveEvent(event);
}

void BmpRuleView::mousePressEvent(QMouseEvent *event)
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

void BmpRuleView::mouseReleaseEvent(QMouseEvent *event)
{
    if (mIgnoreMouse) {
        mIgnoreMouse = false;
        return;
    }
    QTableView::mouseReleaseEvent(event);
}

void BmpRuleView::mouseDoubleClickEvent(QMouseEvent *event)
{
#if 0
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
#endif
    QTableView::mouseDoubleClickEvent(event);
}

void BmpRuleView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        mZoomable->handleWheelDelta(event->delta());
        return;
    }

    QTableView::wheelEvent(event);
}

void BmpRuleView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mContextMenu)
        mContextMenu->exec(event->globalPos());
}

void BmpRuleView::clear()
{
    mDelegate->setMouseOver(QModelIndex());
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void BmpRuleView::setRules(const Map *map)
{
    QSet<Tileset*> tilesets;
    QMap<QString,BmpAlias*> aliasByName;
    foreach (BmpAlias *alias, map->bmpSettings()->aliases())
        aliasByName[alias->name] = alias;
    QList<BmpRule*> rules = map->bmpSettings()->rules();
    foreach (BmpRule *rule, rules) {
        QStringList tileChoices;
        foreach (QString tileName, rule->tileChoices) {
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

    model()->setRules(map);
}

void BmpRuleView::setExpanded(bool expanded)
{
    if (mExpanded != expanded) {
        mExpanded = expanded;
        model()->scaleChanged(mZoomable->scale());
        visualRect(currentIndex());
        QMetaObject::invokeMethod(this, "scrollToCurrentItem", Qt::QueuedConnection);
    }
}

void BmpRuleView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void BmpRuleView::scrollToCurrentItem()
{
    scrollTo(currentIndex(), QTableView::EnsureVisible);
}
