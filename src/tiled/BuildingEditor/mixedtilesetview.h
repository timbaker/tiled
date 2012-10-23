/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#ifndef MIXEDTILESETVIEW_H
#define MIXEDTILESETVIEW_H

#include <QAbstractListModel>
#include <QTableView>

namespace Tiled {

class Tile;
class Tileset;

namespace Internal {

class Zoomable;

class MixedTilesetModel : public QAbstractListModel
{
public:
    MixedTilesetModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(Tiled::Tile *tile);

    void setTiles(const QList<Tile*> &tiles);
    void setTileset(Tileset *tileset);

    Tile *tileAt(const QModelIndex &index) const;
    QString headerAt(const QModelIndex &index) const;

    void setCategoryBounds(Tile *tile, const QRect &bounds);
    QRect categoryBounds(Tile *tile) const;

    void scaleChanged(qreal scale);


private:
    class Item
    {
    public:
        Item() :
            mTile(0)
        {
        }

        Item(Tiled::Tile *tile) :
            mTile(tile)
        {

        }
        Item(const QString &tilesetName) :
            mTile(0),
            mTilesetName(tilesetName)
        {

        }

        Tiled::Tile *mTile;
        QString mTilesetName;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Tiled::Tile *tile) const;

    QList<Item*> mItems;
    QList<Tiled::Tile*> mTiles;
    Tiled::Tileset *mTileset;
    QMap<Tiled::Tile*,QRect> mCategoryBounds;
};

class MixedTilesetView : public QTableView
{
    Q_OBJECT
public:
    explicit MixedTilesetView(QWidget *parent = 0);
    explicit MixedTilesetView(Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    MixedTilesetModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

signals:
    
public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    MixedTilesetModel *mModel;
    Zoomable *mZoomable;
};

} // namespace Internal
} // namespace Tiled

#endif // MIXEDTILESETVIEW_H
