#include "rearrangetiles.h"
#include "ui_rearrangetiles.h"

#include "preferences.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"
#include "BuildingEditor/simplefile.h"

#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>

using namespace Tiled;
using namespace Tiled::Internal;

/////

#define TILE_WIDTH (64)
#define TILE_HEIGHT (128)

namespace {

class TileDelegate : public QAbstractItemDelegate
{
public:
    TileDelegate(RearrangeTilesView *tilesetView, QObject *parent = 0)
        : QAbstractItemDelegate(parent)
        , mView(tilesetView)
    { }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;

private:
    RearrangeTilesView *mView;
};

void TileDelegate::paint(QPainter *painter,
                         const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    const RearrangeTilesModel *m = static_cast<const RearrangeTilesModel*>(index.model());

    QBrush brush = qvariant_cast<QBrush>(m->data(index, Qt::BackgroundRole));
    painter->fillRect(option.rect, brush);

    Tile *tile1x = m->tileAt(index);
    if (!tile1x) {
        return;
    }
    Tile *tile2x = m->tile2xAt(index);

    // Draw the tile image
    const QVariant display = index.model()->data(index, Qt::DisplayRole);
    const QPixmap tileImage = display.value<QPixmap>();
    const int tileWidth = TILE_WIDTH * mView->zoomable()->scale();

    if (mView->zoomable()->smoothTransform())
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

    const int extra = 2;
    const int dw = option.rect.width() - (tileWidth + 4 + tileWidth);

    if (m->isChanged(index)) {
        painter->fillRect(option.rect.adjusted(extra, extra, -extra, -extra),
                          Qt::lightGray);
    }

    if (option.state & QStyle::State_Selected) {
        const qreal opacity = painter->opacity();
        painter->setOpacity(0.5);
        painter->fillRect(option.rect.adjusted(dw/2, extra, -(dw - dw/2) - tileWidth - 4, -extra),
                          option.palette.highlight());
        painter->fillRect(option.rect.adjusted(dw/2 + tileWidth + 4, extra, -(dw - dw/2), -extra),
                          option.palette.highlight());
        painter->setOpacity(opacity);
    }

    QMargins margins = tile1x->drawMargins(mView->zoomable()->scale());
    painter->drawPixmap(option.rect.adjusted(dw/2, extra, -(dw - dw/2) - tileWidth - 4, -extra)
                        .adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tileImage);
    margins = tile2x->drawMargins(mView->zoomable()->scale());
    painter->drawImage(option.rect.adjusted(dw/2 + tileWidth + 4, extra, -(dw - dw/2), -extra)
                       .adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom()), tile2x->image());

    painter->setPen(Qt::darkGray);
    painter->drawRect(option.rect.adjusted(1,1,-1,-1));

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
    Q_UNUSED(option)
//    const RearrangeTilesModel *m = static_cast<const RearrangeTilesModel*>(index.model());
    const qreal zoom = mView->zoomable()->scale();
    const int extra = 2 * 2;
    const int tileWidth = TILE_WIDTH + 4 + TILE_WIDTH;
    return QSize(tileWidth * zoom + extra, TILE_HEIGHT * zoom + extra);
}

} // namespace anonymous

/////

// This constructor is for the benefit of QtDesigner
RearrangeTilesView::RearrangeTilesView(QWidget *parent) :
    QTableView(parent),
    mModel(new RearrangeTilesModel(this)),
    mZoomable(new Zoomable(this))
{
    init();
}

RearrangeTilesView::RearrangeTilesView(Zoomable *zoomable, QWidget *parent) :
    QTableView(parent),
    mModel(new RearrangeTilesModel(this)),
    mZoomable(zoomable)
{
    init();
}

QSize RearrangeTilesView::sizeHint() const
{
    return QSize(TILE_WIDTH * 4, TILE_HEIGHT);
}

void RearrangeTilesView::wheelEvent(QWheelEvent *event)
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

void RearrangeTilesView::setZoomable(Zoomable *zoomable)
{
    mZoomable = zoomable;
    if (zoomable)
        connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(scaleChanged(qreal)));
}

void RearrangeTilesView::clear()
{
    selectionModel()->clear(); // because the model calls reset()
    model()->clear();
}

void RearrangeTilesView::setTiles(Tileset *ts1x, Tileset *ts2x, const RearrangeTileset *rts)
{
    selectionModel()->clear(); // because the model calls reset()
    model()->setTiles(ts1x, ts2x, rts);
}

void RearrangeTilesView::scaleChanged(qreal scale)
{
    model()->scaleChanged(scale);
}

void RearrangeTilesView::init()
{
    setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(32);
    setItemDelegate(new TileDelegate(this, this));
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
}

/////

#define COLUMN_COUNT 8 // same as recent PZ tilesets

RearrangeTilesModel::RearrangeTilesModel(QObject *parent) :
    QAbstractListModel(parent),
    mColumnCount(COLUMN_COUNT)
{
}

RearrangeTilesModel::~RearrangeTilesModel()
{
    qDeleteAll(mItems);
}

int RearrangeTilesModel::rowCount(const QModelIndex &parent) const
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

int RearrangeTilesModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : mColumnCount;
}

Qt::ItemFlags RearrangeTilesModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags flags = QAbstractListModel::flags(index);
    if (!tileAt(index))
        flags &= ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return flags;
}

QVariant RearrangeTilesModel::data(const QModelIndex &index, int role) const
{
//    if (role == Qt::BackgroundRole) {
//        if (Item *item = toItem(index))
//            return item->mBackground;
//    }
    if (role == Qt::DisplayRole) {
        if (Tile *tile = tileAt(index))
            return tile->image();
    }

    return QVariant();
}

bool RearrangeTilesModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Item *item = toItem(index)) {
//        if (role == Qt::BackgroundRole) {
//            if (value.canConvert<QBrush>()) {
//                item->mBackground = qvariant_cast<QBrush>(value);
//                emit dataChanged(index, index);
//                return true;
//            }
//        }
//        if (role == Qt::DisplayRole) {
//            if (item->mTile1x && value.canConvert<Tile*>()) {
//                item->mTile1x = qvariant_cast<Tile*>(value);
//                return true;
//            }
//        }
    }
    return false;
}

QVariant RearrangeTilesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    if (role == Qt::SizeHintRole)
        return QSize(1, 1);
    return QVariant();
}

QModelIndex RearrangeTilesModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent.isValid())
        return QModelIndex();

    if (row * columnCount() + column >= mItems.count())
        return QModelIndex();

    Item *item = mItems.at(row * columnCount() + column);
    if (!item->valid())
        return QModelIndex();
    return createIndex(row, column, item);
}

QModelIndex RearrangeTilesModel::index(Tile *tile)
{
    if (Item *item = toItem(tile)) {
        int tileIndex = indexOf(item);
        if (tileIndex != -1)
            return index(tileIndex / columnCount(), tileIndex % columnCount());
    }
    return QModelIndex();
}

void RearrangeTilesModel::clear()
{
    setTiles(0, 0, 0);
}

void RearrangeTilesModel::setTiles(Tileset *ts1x, Tileset *ts2x, const RearrangeTileset *rts)
{
    beginResetModel();

    mTileset1x = ts1x;
    mTileset2x = ts2x;
    mTileToItem.clear();
    mTileItemsByIndex.clear();

    qDeleteAll(mItems);
    mItems.clear();

    if (ts1x == 0) {
        endResetModel();
        return;
    }

    for (int i = 0; i < ts1x->tileCount(); i++) {
        if (i >= ts2x->tileCount())
            break;
        RearrangeIndex idx(i % ts1x->columnCount(), i / ts1x->columnCount());
        bool changed = rts && rts->mIndexMap.contains(idx);
        Item *item = new Item(i, changed);
        mItems += item;
        mTileItemsByIndex[i] = item;
        mTileToItem[ts1x->tileAt(i)] = item;
    }

    endResetModel();
}

void RearrangeTilesModel::setChanged(const QModelIndex &index, bool changed)
{
    if (Item *item = toItem(index)) {
        item->mChanged = changed;
        emit dataChanged(index, index);
    }
}

bool RearrangeTilesModel::isChanged(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        return item->mChanged;
    }
    return false;
}

Tile *RearrangeTilesModel::tileAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return mTileset1x->tileAt(item->mTileID);
    return 0;
}

Tile *RearrangeTilesModel::tile2xAt(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return mTileset2x->tileAt(item->mTileID);
    return 0;
}

void RearrangeTilesModel::scaleChanged(qreal scale)
{
    Q_UNUSED(scale)
    redisplay();
}

void RearrangeTilesModel::redisplay()
{
    int maxRow = rowCount() - 1;
    int maxColumn = columnCount() - 1;
    if (maxRow >= 0 && maxColumn >= 0)
        emit dataChanged(index(0, 0), index(maxRow, maxColumn));
}

void RearrangeTilesModel::redisplay(const QModelIndex &index)
{
    emit dataChanged(index, index);
}

void RearrangeTilesModel::setColumnCount(int count)
{
    beginResetModel();
    mColumnCount = count;
    endResetModel();
}

RearrangeTilesModel::Item *RearrangeTilesModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

RearrangeTilesModel::Item *RearrangeTilesModel::toItem(Tile *tile) const
{
    if (mTileToItem.contains(tile))
        return mTileToItem[tile];
    return 0;
}

/////

RearrangeTiles *RearrangeTiles::mInstance = 0;

RearrangeTiles *RearrangeTiles::instance()
{
    if (!mInstance)
        mInstance = new RearrangeTiles();
    return mInstance;
}

RearrangeTiles::RearrangeTiles(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::RearrangeTiles),
    mZoomable(new Zoomable(this)),
    bDirty(false)
{
    ui->setupUi(this);

    mZoomable->setScale(0.5); // FIXME
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles1x2x->setZoomable(mZoomable);

    connect(ui->actionSave, SIGNAL(triggered(bool)), SLOT(fileSave()));
    connect(ui->actionQuit, SIGNAL(triggered(bool)), SLOT(close()));

    connect(ui->browseTiles1x, SIGNAL(clicked()), SLOT(browse1x()));
    connect(ui->browseTiles2x, SIGNAL(clicked()), SLOT(browse2x()));

    ui->editTiles1x->setText(Preferences::instance()->tilesDirectory());
    ui->editTiles2x->setText(Preferences::instance()->tiles2xDirectory());

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles1x2x, SIGNAL(clicked(QModelIndex)), SLOT(tileClicked(QModelIndex)));

    setTilesetList();
}

RearrangeTiles::~RearrangeTiles()
{
    delete ui;
}

bool RearrangeTiles::isRearranged(BuildingEditor::BuildingTile *btile)
{
    if (Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(btile)) {
        return isRearranged(tile);
    }
    return false;
}

bool RearrangeTiles::isRearranged(Tile *tile)
{
    RearrangeTileset *rts = tileset(tile->tileset()->name());
    if (rts) {
        RearrangeIndex index(tile->id() % tile->tileset()->columnCount(), tile->id() / tile->tileset()->columnCount());
        return rts->mIndexMap.contains(index);
    }
    return false;
}

void RearrangeTiles::readTxtIfNeeded()
{
    if (mTilesets.isEmpty())
        readTxt();
}

void RearrangeTiles::browse1x()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Directory"),
                                                  ui->editTiles1x->text());
    if (!f.isEmpty()) {
        setTilesetList();
        updateUI();
    }
}

void RearrangeTiles::browse2x()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Directory"),
                                                  ui->editTiles2x->text());
    if (!f.isEmpty()) {
        setTilesetList();
        updateUI();
    }
}

void RearrangeTiles::currentTilesetChanged(int row)
{
    if (row == -1)
        return;
    QListWidgetItem *item = ui->tilesets->item(row);

    if (mTilesets.isEmpty())
        readTxt();

    static Tileset *ts1x = new Tileset(QLatin1String("tileset1x"), 64, 128);
    ts1x->setName(item->text());
    QString fileName1x = QString::fromLatin1("%1/%2.png").arg(ui->editTiles1x->text()).arg(item->text());
    ts1x->loadFromImage(QImage(fileName1x), fileName1x);

    static Tileset *ts2x = new Tileset(QLatin1String("tileset2x"), 64, 128);
    ts2x->setName(item->text());
    QString fileName2x = QString::fromLatin1("%1/%2.png").arg(ui->editTiles2x->text()).arg(item->text());
    ts2x->setImageSource2x(fileName2x);
    ts2x->loadFromImage(QImage(fileName2x), fileName1x);

    QList<Tile*> tiles;
    for (int i = 0; i < ts1x->tileCount(); i++) {
        Tile *tile1x = ts1x->tileAt(i);
        Tile *tile2x = ts2x->tileAt(i);
        if (!tile2x)
            break;
        tiles += tile1x;
        tiles += tile2x;
    }
    RearrangeTileset *rts = tileset(item->text());
    ui->tiles1x2x->setTiles(ts1x, ts2x, rts);

    mCurrentTileset = item->text();
}

void RearrangeTiles::tileClicked(const QModelIndex &index)
{
    RearrangeTileset *rts = tileset(mCurrentTileset);
    if (rts == 0) {
        rts = new RearrangeTileset();
        rts->mName = mCurrentTileset;
        mTilesets += rts;
    }
    RearrangeIndex rindex(index.column(), index.row());
    if (rts->mIndexMap.contains(rindex)) {
        rts->mIndexMap.removeAll(rindex);
    } else {
        rts->mIndexMap += rindex;
    }
    ui->tiles1x2x->model()->setChanged(index, rts->mIndexMap.contains(rindex));
    bDirty = true;
}

bool RearrangeTiles::fileSave()
{
    writeTxt();
    bDirty = false;
    return true;
}

void RearrangeTiles::setTilesetList()
{
    QDir dir1x(ui->editTiles1x->text());
    QDir dir2x(ui->editTiles2x->text());

//    QFileInfoList entries1x = dir1x.entryInfoList();
    QFileInfoList entries2x = dir2x.entryInfoList();

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 64;

    ui->tilesets->clear();
    foreach (QFileInfo fileInfo, entries2x) {
        if (fileInfo.isDir())
            continue;
        if (fileInfo.suffix() != QLatin1String("png"))
            continue;
        if (!dir1x.exists(fileInfo.fileName()))
            continue;
        QListWidgetItem *item = new QListWidgetItem(fileInfo.baseName());
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(item->text()));
    }
    ui->tilesets->setFixedWidth(maxWidth + 16 +
                                ui->tilesets->verticalScrollBar()->sizeHint().width());
}

RearrangeTileset *RearrangeTiles::tileset(const QString &name)
{
    foreach (RearrangeTileset *rts, mTilesets) {
        if (rts->mName == name) {
            return rts;
        }
    }
    return 0;
}

void RearrangeTiles::rearrange(const QString &tilesetName,  const RearrangeIndex &index)
{
}

void RearrangeTiles::updateUI()
{

}

void RearrangeTiles::readTxt()
{
    qDeleteAll(mTilesets);
    mTilesets.clear();

    QString fileName = Preferences::instance()->appConfigPath(QLatin1String("Rearrange.txt"));
    if (!QFileInfo(fileName).exists())
        return;
    RearrangeFile file;
    if (!file.read(fileName)) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while reading %1\n%2")
                              .arg(fileName)
                              .arg(file.errorString()));
        return;
    }
    mTilesets = file.takeTilesets();
}

void RearrangeTiles::writeTxt()
{
    QString fileName = Preferences::instance()->appConfigPath(QLatin1String("Rearrange.txt"));
    RearrangeFile file;
    if (!file.write(fileName, mTilesets)) {
        QMessageBox::critical(this, tr("It's no good, Jim!"),
                              tr("Error while writing %1\n%2")
                              .arg(fileName)
                              .arg(file.errorString()));
        return;
    }
}

bool RearrangeTiles::confirmSave()
{
    if (!bDirty)
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:
        return fileSave();
    case QMessageBox::Discard:
        readTxt();
        bDirty = false;
        ui->tiles1x2x->clear();
        return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

void RearrangeTiles::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
//        clearDocument();

//        QSettings settings;
//        settings.beginGroup(QLatin1String("TileDefDialog"));
//        settings.setValue(QLatin1String("geometry"), saveGeometry());
//        settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
//        settings.setValue(QLatin1String("CurrentTileset"), mCurrentTilesetName);
//        settings.endGroup();

//        saveSplitterSizes(ui->splitter);


        event->accept();
    } else {
        event->ignore();
    }
}

/////

// tileset
// {
//   name = fixtures_bathroom_02
//   tile = 0,0
//   tile = 0,2
// }

#define VERSION1 1
#define VERSION_LATEST VERSION1

bool RearrangeFile::read(const QString &fileName)
{
    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        mError = tr("%1\n(while reading %2)")
                .arg(simpleFile.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    if (simpleFile.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(fileName).arg(VERSION_LATEST).arg(simpleFile.version());
        return false;
    }

    mTilesets.clear();

    foreach (SimpleFileBlock tsBlock, simpleFile.blocks) {
        if (tsBlock.name == QLatin1String("tileset")) {
            if (!tsBlock.hasValue("name")) {
                mError = tr("Line %1: Missing 'name' attribute").arg(tsBlock.lineNumber);
                return false;
            }
            RearrangeTileset rts;
            rts.mName = tsBlock.value("name");
            foreach (SimpleFileKeyValue kv, tsBlock.values) {
                if (kv.name == QLatin1String("name"))
                    continue;
                if (kv.name == QLatin1String("tile")) {
                    RearrangeIndex index;
                    if (!parse2Ints(kv.value, &index.rx(), &index.ry()) ||
                            (index.x() < 0) || (index.y() < 0)) {
                        mError = tr("Line %1: Invalid %2 = %3").arg(kv.lineNumber).arg(kv.name).arg(kv.value);
                        return false;
                    }
                    rts.mIndexMap += index;
                } else {
                    mError = tr("Line %1: Unknown value name '%2'.").arg(kv.lineNumber).arg(kv.name);
                    return false;
                }
            }
            RearrangeTileset *prts = new RearrangeTileset();
            prts->mName = rts.mName;
            prts->mIndexMap = rts.mIndexMap;
            mTilesets += prts;
        } else {
            mError = tr("Unknown block name '%1'.").arg(tsBlock.name);
            return false;
        }
    }

    return true;
}

bool RearrangeFile::write(const QString &fileName, const QList<RearrangeTileset*> &tilesets)
{
    SimpleFile simpleFile;

    foreach (RearrangeTileset *rts, tilesets) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("name", rts->mName);

        foreach (RearrangeIndex index, rts->mIndexMap) {
            tilesetBlock.addValue("tile", QString::fromLatin1("%1,%2").arg(index.x()).arg(index.y()));
        }

        simpleFile.blocks += tilesetBlock;
    }

    qDebug() << "WRITE " << fileName;

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

bool RearrangeFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}
