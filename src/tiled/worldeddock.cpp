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
#include "mapdocument.h"

#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"

#include "maprenderer.h"

#include <QFileInfo>

using namespace Tiled::Internal;

/////

WorldCellLevel::WorldCellLevel(WorldCell *cell, int level) :
    mCell(cell),
    mLevel(level),
    mLots(cell->lots()),
    mVisible(true)
{
}

/////

WorldCellLotModel::WorldCellLotModel(QObject *parent) :
    QAbstractItemModel(parent),
    mRoot(0),
    mCell(0)
{
}

WorldCellLotModel::~WorldCellLotModel()
{
    delete mRoot;
    qDeleteAll(mLevels);
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
            return QString::fromLatin1("Level %1").arg(level->level());
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
            level->setVisible(c == Qt::Checked);
            emit visibilityChanged(level);
            return true;
        }
        }
        return false;
    }
    if (WorldCellLot *lot = item->lot) {
        switch (role) {
        case Qt::CheckStateRole: {
            Qt::CheckState c = static_cast<Qt::CheckState>(value.toInt());
            lot->setVisible(c == Qt::Checked);
            emit visibilityChanged(lot);
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
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
}

QModelIndex WorldCellLotModel::index(WorldCellLot *lot) const
{
    Item *item = toItem(lot);
    int row = item->parent->children.indexOf(item);
    return createIndex(row, 0, item);
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
    delete mRoot;
    mRoot = 0;
    qDeleteAll(mLevels);
    mLevels.clear();

    mCell = cell;

    if (mCell) {
        mRoot = new Item;

        for (int z = 0; z < 16; z++) {
            mLevels += new WorldCellLevel(cell, z);
            Item *levelItem = new Item(mRoot, 0, mLevels.last());
            foreach (WorldCellLot *lot, mCell->lots()) {
                if (lot->level() == z)
                    new Item(levelItem, 0, lot);
            }
        }
    }

    reset();
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
    Item *parent = toItem(mLevels[lot->level()]);
    foreach (Item *item, parent->children)
        if (item->lot == lot)
            return item;
    return 0;
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
    ui(new Ui::WorldEdDock)
{
    ui->setupUi(this);

    connect(ui->view->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectionChanged()));
    connect(ui->view->model(), SIGNAL(visibilityChanged(WorldCellLot*)),
            SLOT(visibilityChanged(WorldCellLot*)));
    connect(ui->view->model(), SIGNAL(visibilityChanged(WorldCellLevel*)),
            SLOT(visibilityChanged(WorldCellLevel*)));

    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(beforeWorldChanged(QString)),
           SLOT(beforeWorldChanged()));
    connect(WorldEd::WorldEdMgr::instance(), SIGNAL(afterWorldChanged(QString)),
            SLOT(afterWorldChanged()));
}

WorldEdDock::~WorldEdDock()
{
    delete ui;
}

void WorldEdDock::setMapDocument(MapDocument *mapDoc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = mapDoc;

    if (mDocument) {
        if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mDocument->fileName()))
            ui->view->model()->setWorldCell(cell);
        else
            ui->view->model()->setWorldCell(0);
    } else
        ui->view->model()->setWorldCell(0);
}


void WorldEdDock::selectionChanged()
{
    if (!mDocument /*|| mSynching*/)
        return;

    QModelIndexList selectedRows = ui->view->selectionModel()->selectedRows();
    int count = selectedRows.count();

    if (count == 1) {
        QModelIndex index = selectedRows.first();
        if (WorldCellLot *lot = ui->view->model()->toLot(index)) {
            DocumentManager::instance()->ensureRectVisible(
                        mDocument->renderer()->boundingRect(lot->bounds()));
        }
    }
}

#include "ZomboidScene.h"
void WorldEdDock::visibilityChanged(WorldCellLevel *level)
{
    ((ZomboidScene*)DocumentManager::instance()->currentMapScene())->lotManager()
            .worldCellLevelChanged(level->level(), level->isVisible());
}

void WorldEdDock::visibilityChanged(WorldCellLot *lot)
{
    ((ZomboidScene*)DocumentManager::instance()->currentMapScene())->lotManager()
            .worldCellLotChanged(lot);
}

void WorldEdDock::beforeWorldChanged()
{
    if (mDocument)
        ui->view->model()->setWorldCell(0);
}

void WorldEdDock::afterWorldChanged()
{
    if (mDocument) {
        if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mDocument->fileName()))
            ui->view->model()->setWorldCell(cell);
    }
}
