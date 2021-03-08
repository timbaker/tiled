/*
 * tilerotateview.h
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

#ifndef TILEROTATEVIEW_H
#define TILEROTATEVIEW_H

#include <QAbstractListModel>
#include <QTableView>

#include "maprotation.h"

class QMenu;

namespace Tiled {
class Tileset;
namespace Internal {
class Zoomable;
}
}

namespace Tiled {

class TileRotated;
class TilesetRotated;

class TileRotateDelegate;

class TileRotateModel : public QAbstractListModel
{
    Q_OBJECT
public:
    TileRotateModel(QObject *parent = nullptr);
    ~TileRotateModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(TileRotated *tile);

    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void clear();
    void setTiles(const QList<TileRotated*> &tilesList);

    TileRotated *tileAt(const QModelIndex &index) const;
    MapRotation mapRotationAt(const QModelIndex &index) const;
    QString headerAt(const QModelIndex &index) const;

//    void removeTiles(FurnitureTiles *ftiles);

    void scaleChanged(qreal scale);

    void setDropCoords(const QPoint &dropCoord, const QModelIndex &dropIndex)
    {
        mDropCoords = dropCoord;
        mDropIndex = dropIndex;
    }

    QPoint dropCoords() const
    { return mDropCoords; }

    QModelIndex dropIndex() const
    { return mDropIndex; }

    void redisplay();

signals:
    void tileDropped(TileRotated *tile, int index, const QString &tileName);

private:
    class Item
    {
    public:
        Item() :
            mTile(nullptr),
            mMapRotation(MapRotation::NotRotated)
        {
        }

        Item(TileRotated *tile, MapRotation mapRotation) :
            mTile(tile),
            mMapRotation(mapRotation)
        {
        }
        Item(const QString &heading) :
            mTile(nullptr),
            mMapRotation(MapRotation::NotRotated),
            mHeading(heading)
        {
        }

        TileRotated *mTile;
        MapRotation mMapRotation;
        QString mHeading;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(TileRotated *tile) const;

    QList<Item*> mItems;
    QList<TileRotated*> mTiles;
    static QString mMimeType;
    QPoint mDropCoords;
    QModelIndex mDropIndex;
    bool mShowHeaders;
};

class TileRotateView : public QTableView
{
    Q_OBJECT
public:
    explicit TileRotateView(QWidget *parent = nullptr);
    explicit TileRotateView(Tiled::Internal::Zoomable *zoomable, QWidget *parent = nullptr);

    QSize sizeHint() const;

    void wheelEvent(QWheelEvent *event);

    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    TileRotateModel *model() const
    { return mModel; }

    TileRotateDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void furnitureTileResized(TileRotated *tile);

    void contextMenuEvent(QContextMenuEvent *event);
    void setContextMenu(QMenu *menu)
    { mContextMenu = menu; }

    void clear();
    void setTiles(const QList<TileRotated*> &tilesList);
    void redisplay();

    typedef Tiled::Tileset Tileset;
public slots:
    void scaleChanged(qreal scale);

    void tilesetChanged(Tileset *tileset);
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

private:
    void init();

private:
    TileRotateModel *mModel;
    TileRotateDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
    QMenu *mContextMenu;
};

} // namespace Tiled

#endif // TILEROTATEVIEW_H
