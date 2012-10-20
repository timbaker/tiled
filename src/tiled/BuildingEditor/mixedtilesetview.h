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

namespace Internal {

class Zoomable;

class MixedTilesetModel : public QAbstractListModel
{
public:
    MixedTilesetModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    void setTiles(const QList<Tile*> &tiles);

    Tile *tileAt(const QModelIndex &index) const;

    void scaleChanged(qreal scale);

private:
    QList<Tiled::Tile*> mTiles;
};

class MixedTilesetView : public QTableView
{
    Q_OBJECT
public:
    explicit MixedTilesetView(Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    MixedTilesetModel *model() const
    { return mModel; }

    Zoomable *zoomable() const
    { return mZoomable; }

signals:
    
public slots:
    void scaleChanged(qreal scale);

private:
    MixedTilesetModel *mModel;
    Zoomable *mZoomable;
};

} // namespace Internal
} // namespace Tiled

#endif // MIXEDTILESETVIEW_H
