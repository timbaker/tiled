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

#include "worldeddock.h"
#include "ui_worldeddock.h"

#include "documentmanager.h"
#include "mainwindow.h"
#include "mapdocument.h"
#include "ZomboidScene.h"

#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"

#include "maprenderer.h"

#include <QFileInfo>

using namespace Tiled::Internal;

/////

WorldCellLotModel::WorldCellLotModel(QObject *parent) :
    QAbstractItemModel(parent),
    mRoot(0),
    mCell(0)
{
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(levelVisibilityChanged(WorldCellLevel*)),
            SLOT(levelVisibilityChanged(WorldCellLevel*)));
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(lotVisibilityChanged(WorldCellLot*)),
            SLOT(lotVisibilityChanged(WorldCellLot*)));
}

WorldCellLotModel::~WorldCellLotModel()
{
    delete mRoot;
}

QModelIndex WorldCellLotModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!mRoot)
        return QModelIndex();

    if (!parent.isValid()) {
        if (row < mRoot->children.size())
            return createIndex(row, column, mRoot->children.at(row));
        return QModelIndex();
    }

    if (Item *item = toItem(parent)) {
        if (row < item->children.size())
            return createIndex(row, column, item->children.at(row));
        return QModelIndex();
    }

    return QModelIndex();
}

QModelIndex WorldCellLotModel::parent(const QModelIndex &index) const
{
    if (Item *item = toItem(index)) {
        if (item->parent != mRoot) {
            Item *grandParent = item->parent->parent;
            return createIndex(grandParent->children.indexOf(item->parent), 0, item->parent);
        }
    }
    return QModelIndex();
}

int WorldCellLotModel::rowCount(const QModelIndex &parent) const
{
    if (!mRoot)
        return 0;
    if (!parent.isValid())
        return mRoot->children.size();
    if (Item *item = toItem(parent))
        return item->children.size();
    return 0;
}

int WorldCellLotModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant WorldCellLotModel::data(const QModelIndex &index, int role) const
{
    Item *item = toItem(index);
    if (!item)
        return QVariant();

    if (WorldCellLevel *level = item->level) {
        switch (role) {
        case Qt::DisplayRole:
            return QString::fromLatin1("Level %1").arg(level->z());
        case Qt::EditRole:
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            return level->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    if (WorldCellLot *lot = item->lot) {
        switch (role) {
        case Qt::DisplayRole:
            return QFileInfo(lot->mapName()).fileName();
        case Qt::EditRole:
        case Qt::DecorationRole:
            return QVariant();
        case Qt::CheckStateRole:
            return lot->isVisible() ? Qt::Checked : Qt::Unchecked;
        default:
            return QVariant();
        }
    }
    return QVariant();
}

bool WorldCellLotModel::setData(const QModelIndex &index, const QVariant &value,
                                int role)
{
    Item *item = toItem(index);
    if (!item)
        return false;

    if (WorldCellLevel *level = item->level) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            WorldEd::WorldEdMgr::instance()->setLevelVisible(level, c == Qt::Checked);
            return true;
        }
        }
        return false;
    }
    if (WorldCellLot *lot = item->lot) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            WorldEd::WorldEdMgr::instance()->setLotVisible(lot, c == Qt::Checked);
            return true;
        }
        }
        return false;
    }
    return false;
}

Qt::ItemFlags WorldCellLotModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags rc = QAbstractItemModel::flags(index);
    rc |= Qt::ItemIsUserCheckable;
    return rc;
}

QVariant WorldCellLotModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        switch (section) {
        case 0: return tr("Name");
        }
    }
    return QVariant();
}

QModelIndex WorldCellLotModel::index(WorldCellLevel *level) const
{
    Item *item = toItem(level);
    return index(item);
}

QModelIndex WorldCellLotModel::index(WorldCellLot *lot) const
{
    Item *item = toItem(lot);
    return index(item);
}

WorldCellLevel *WorldCellLotModel::toLevel(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->level;
    return 0;
}

WorldCellLot *WorldCellLotModel::toLot(const QModelIndex &index) const
{
    if (Item *item = toItem(index))
        return item->lot;
    return 0;
}

void WorldCellLotModel::setWorldCell(WorldCell *cell)
{
    beginResetModel();

    delete mRoot;
    mRoot = 0;

    mCell = cell;

    if (mCell) {
        mRoot = new Item;

        for (int z = 0; z < mCell->levelCount(); z++) {
            Item *levelItem = new Item(mRoot, 0, cell->levelAt(z));
            foreach (WorldCellLot *lot, mCell->lots()) {
                if (lot->level() == z)
                    new Item(levelItem, 0, lot);
            }
        }
    }

    endResetModel();
}

void WorldCellLotModel::levelVisibilityChanged(WorldCellLevel *level)
{
    if (Item *item = toItem(level))
        emit dataChanged(index(item), index(item));
}

void WorldCellLotModel::lotVisibilityChanged(WorldCellLot *lot)
{
    if (Item *item = toItem(lot))
        emit dataChanged(index(item), index(item));
}

WorldCellLotModel::Item *WorldCellLotModel::toItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<Item*>(index.internalPointer());
    return 0;
}

WorldCellLotModel::Item *WorldCellLotModel::toItem(WorldCellLevel *level) const
{
    if (!mRoot)
        return 0;
    foreach (Item *item, mRoot->children)
        if (item->level == level)
            return item;
    return 0;
}

WorldCellLotModel::Item *WorldCellLotModel::toItem(WorldCellLot *lot) const
{
    if (!mRoot)
        return 0;
    Item *parent = toItem(lot->cell()->levelAt(lot->level()));
    foreach (Item *item, parent->children)
        if (item->lot == lot)
            return item;
    return 0;
}

QModelIndex WorldCellLotModel::index(WorldCellLotModel::Item *item) const
{
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

/////

WorldEdView::WorldEdView(QWidget *parent) :
    QTreeView(parent),
    mModel(new WorldCellLotModel(this))
{
    setRootIsDecorated(true);
    setHeaderHidden(true);
    setItemsExpandable(true);
    setUniformRowHeights(true);

    setSelectionBehavior(QAbstractItemView::SelectRows);

    setModel(mModel);
}

/////

WorldEdDock::WorldEdDock(QWidget *parent) :
    QDockWidget(parent),
    mDocument(0),
    mSynching(false),
    ui(new Ui::WorldEdDock)
{
    ui->setupUi(this);

    connect(ui->view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));
    connect(ui->view, SIGNAL(activated(QModelIndex)), SLOT(activated(QModelIndex)));

    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(lotVisibilityChanged(WorldCellLot*)),
            SLOT(visibilityChanged(WorldCellLot*)));
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(levelVisibilityChanged(WorldCellLevel*)),
            SLOT(visibilityChanged(WorldCellLevel*)));

    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(beforeWorldChanged(QString)),
           SLOT(beforeWorldChanged()));
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(afterWorldChanged(QString)),
            SLOT(afterWorldChanged()));
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(selectedLotsChanged()),
            SLOT(selectedLotsChanged()));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,MapDocument*)),
            SLOT(documentAboutToClose(int,MapDocument*)));
}

WorldEdDock::~WorldEdDock()
{
    delete ui;
}

void WorldEdDock::setMapDocument(MapDocument *mapDoc)
{
    if (mDocument) {
        saveExpandedLevels(mDocument);
        mDocument->disconnect(this);
    }

    mDocument = mapDoc;

    if (mDocument) {
        if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mDocument->fileName()))
            ui->view->model()->setWorldCell(cell);
        else
            ui->view->model()->setWorldCell(0);
        restoreExpandedLevels(mDocument);
    } else
        ui->view->model()->setWorldCell(0);
}

void WorldEdDock::selectionChanged()
{
    if (!mDocument || mSynching)
        return;

    QModelIndexList selectedRows = ui->view->selectionModel()->selectedRows();
    int count = selectedRows.count();

    if (count == 1) {
        QModelIndex index = selectedRows.first();
        if (WorldCellLot *lot = ui->view->model()->toLot(index)) {
            mSynching = true;
            WorldEd::WorldEdMgr::instance()->setSelectedLots(QSet<WorldCellLot*>() << lot);
            mSynching = false;
            DocumentManager::instance()->ensureRectVisible(
                        mDocument->renderer()->boundingRect(lot->bounds()));
        }
    }
}

void WorldEdDock::activated(const QModelIndex &index)
{
    if (WorldCellLot *lot = ui->view->model()->toLot(index)) {
        if (QFileInfo::exists(lot->mapName()))
            MainWindow::instance()->openFile(lot->mapName());
    }
}

void WorldEdDock::visibilityChanged(WorldCellLevel *level)
{
    ((ZomboidScene*)DocumentManager::instance()->currentMapScene())->lotManager()
            .worldCellLevelChanged(level->z(), level->isVisible());
}

void WorldEdDock::visibilityChanged(WorldCellLot *lot)
{
    ((ZomboidScene*)DocumentManager::instance()->currentMapScene())->lotManager()
            .worldCellLotChanged(lot);
}

void WorldEdDock::beforeWorldChanged()
{
    if (mDocument) {
        saveExpandedLevels(mDocument);
        ui->view->model()->setWorldCell(0);
    }
}

void WorldEdDock::afterWorldChanged()
{
    if (mDocument) {
        if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mDocument->fileName())) {
            ui->view->model()->setWorldCell(cell);
            restoreExpandedLevels(mDocument);
        }
    }
}

void WorldEdDock::selectedLotsChanged()
{
    const QSet<WorldCellLot*> &selected = WorldEd::WorldEdMgr::instance()->selectedLots();
    if (selected.size() == 1) {
        QModelIndex index = ui->view->model()->index(*selected.begin());
        if (index.isValid()) {
            mSynching = true;
            ui->view->setCurrentIndex(index);
            mSynching = false;
        }
    } else {
        ui->view->clearSelection();
    }
}

void WorldEdDock::saveExpandedLevels(MapDocument *mapDoc)
{
    mExpandedLevels[mapDoc].clear();
    if (WorldCell *cell = ui->view->model()->cell()) {
        foreach (WorldCellLevel *level, cell->levels()) {
            if (ui->view->isExpanded(ui->view->model()->index(level)))
                mExpandedLevels[mapDoc].append(level->z());
        }
    }
}

void WorldEdDock::restoreExpandedLevels(MapDocument *mapDoc)
{
    if (mExpandedLevels.contains(mapDoc)) {
        if (WorldCell *cell = ui->view->model()->cell()) {
            foreach (int z, mExpandedLevels[mapDoc]) {
                if (WorldCellLevel *level = cell->levelAt(z))
                    ui->view->setExpanded(ui->view->model()->index(level), true);
            }
        }
        mExpandedLevels[mapDoc].clear();
    } else
        ui->view->expandAll();

    // Also restore the selection
//    if (Layer *layer = mapDoc->currentLayer())
//        mView->setCurrentIndex(mView->model()->index(layer));
}

void WorldEdDock::documentAboutToClose(int index, MapDocument *mapDocument)
{
    Q_UNUSED(index)
    mExpandedLevels.remove(mapDocument);
}
