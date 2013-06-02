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

#ifndef WORLDEDDOCK_H
#define WORLDEDDOCK_H

#include <QAbstractItemModel>
#include <QDockWidget>
#include <QTreeView>

class WorldCell;
class WorldCellLot;

namespace Ui {
class WorldEdDock;
}

class WorldCellLevel
{
public:
    WorldCellLevel(WorldCell *cell, int level);

    int level() const
    { return mLevel; }

    void setVisible(bool visible)
    { mVisible = visible; }

    bool isVisible() const
    { return mVisible; }

private:
    WorldCell *mCell;
    int mLevel;
    QList<WorldCellLot*> mLots;
    bool mVisible;
};

namespace Tiled {
namespace Internal {
class MapDocument;

class WorldCellLotModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    WorldCellLotModel(QObject *parent = 0);
    ~WorldCellLotModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QModelIndex index(WorldCellLevel *level) const;
    QModelIndex index(WorldCellLot *lot) const;

    WorldCellLevel *toLevel(const QModelIndex &index) const;
    WorldCellLot *toLot(const QModelIndex &index) const;

    void setWorldCell(WorldCell *cell);

signals:
    void visibilityChanged(WorldCellLevel *level);
    void visibilityChanged(WorldCellLot *lot);

private:
    class Item
    {
    public:
        Item() :
            parent(0),
            level(0),
            lot(0)
        {

        }

        Item(Item *parent, int index, WorldCellLevel *level) :
            parent(parent),
            level(level),
            lot(0)
        {
            parent->children.insert(index, this);
        }

        Item(Item *parent, int index, WorldCellLot *lot) :
            parent(parent),
            level(0),
            lot(lot)
        {
            parent->children.insert(index, this);
        }

        Item *parent;
        QList<Item*> children;
        WorldCellLevel *level;
        WorldCellLot *lot;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(WorldCellLevel *level) const;
    Item *toItem(WorldCellLot *lot) const;

    Item *mRoot;
    QVector<WorldCellLevel*> mLevels;
    WorldCell *mCell;
};

class WorldEdView : public QTreeView
{
public:
    WorldEdView(QWidget *parent = 0);

    WorldCellLotModel *model()
    { return mModel; }

private:
    WorldCellLotModel *mModel;
};

class WorldEdDock : public QDockWidget
{
    Q_OBJECT
    
public:
    WorldEdDock(QWidget *parent = 0);
    ~WorldEdDock();

    void setMapDocument(MapDocument *mapDoc);
    
private slots:
    void selectionChanged();
    void visibilityChanged(WorldCellLevel *level);
    void visibilityChanged(WorldCellLot *lot);

private:
    MapDocument *mDocument;
    Ui::WorldEdDock *ui;
};

} // namespace Internal
} // namespace Tiled

#endif // WORLDEDDOCK_H
