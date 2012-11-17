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

#ifndef FURNITUREVIEW_H
#define FURNITUREVIEW_H

#include <QAbstractListModel>
#include <QTableView>

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class FurnitureTile;
class FurnitureTiles;

class FurnitureTileDelegate;

class FurnitureModel : public QAbstractListModel
{
    Q_OBJECT
public:
    FurnitureModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(FurnitureTile *tile);

    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void setTiles(const QList<FurnitureTiles*> &tilesList);

    FurnitureTile *tileAt(const QModelIndex &index) const;

    void toggleCorners(FurnitureTiles *ftiles);
    void removeTiles(FurnitureTiles *ftiles);

    void scaleChanged(qreal scale);

    void setDropCoords(const QPoint &dropCoord, const QModelIndex &dropIndex)
    { mDropCoords = dropCoord, mDropIndex = dropIndex; }

    QPoint dropCoords() const
    { return mDropCoords; }

    QModelIndex dropIndex() const
    { return mDropIndex; }

    void calcMaxTileSize();

    QSize maxTileSize(int n) const
    { return mMaxTileSize[n]; }

signals:
    void furnitureTileDropped(FurnitureTile *ftile, int x, int y,
                              const QString &tileName);

private:
    QList<FurnitureTile*> mTiles;
    static QString mMimeType;
    QPoint mDropCoords;
    QModelIndex mDropIndex;
    QVector<QSize> mMaxTileSize;
};

class FurnitureView : public QTableView
{
    Q_OBJECT
public:
    explicit FurnitureView(QWidget *parent = 0);
    explicit FurnitureView(Tiled::Internal::Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    FurnitureModel *model() const
    { return mModel; }

    FurnitureTileDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void furnitureTileResized(FurnitureTile *ftile);

signals:

public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    FurnitureModel *mModel;
    FurnitureTileDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
};

} // namespace BuildingEditor

#endif // FURNITUREVIEW_H
