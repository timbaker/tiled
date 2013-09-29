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

#ifndef VIRTUALTILESETVIEW_H
#define VIRTUALTILESETVIEW_H

#include <QAbstractTableModel>
#include <QTableView>

namespace Tiled {
namespace Internal {

class VirtualTile;
class VirtualTileset;
class VirtualTilesetDelegate;
class Zoomable;

class VirtualTilesetModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    VirtualTilesetModel(QObject *parent = 0);
    ~VirtualTilesetModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(VirtualTile *vtile) const;

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void clear();
    void setTileset(VirtualTileset *vts);
    VirtualTile *tileAt(const QModelIndex &index) const;
    void scaleChanged(qreal scale);
    void redisplay();
    void redisplay(VirtualTile *vtile);

    void setClickedIndex(const QModelIndex &index) { mClickedIndex = index; }
    void setMovedTiles(QList<VirtualTile*> &tiles) { mMovedTiles = tiles; }

    void setDiskImage(const QImage &image);
    void setShowDiskImage(bool show);
    bool diskImageValid() const { return !mDiskImage.isNull(); }

signals:
    void beginDropTiles();
    void tileDropped(VirtualTile *vtile, const QString &imageSource, int x, int y, int isoType);
    void endDropTiles();

private:
    class Item
    {
    public:
        Item() :
            mTile(0)
        {
        }

        Item(VirtualTile *vtile) :
            mTile(vtile)
        {
        }

        VirtualTile *mTile;
        QImage mDiskImage;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(VirtualTile *vtile) const;

    VirtualTileset *mTileset;
    QList<Item*> mItems;
    QModelIndex mClickedIndex;
    QList<VirtualTile*> mMovedTiles;
    QImage mDiskImage;
    bool mShowDiskImage;
    static QString mMimeType;
};

class VirtualTilesetView : public QTableView
{
    Q_OBJECT
public:
    explicit VirtualTilesetView(QWidget *parent = 0);

    QSize sizeHint() const;

    void mousePressEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    VirtualTilesetModel *model() const
    { return mModel; }

    VirtualTilesetDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void clear();
    void setTileset(VirtualTileset *vts);

public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    VirtualTilesetModel *mModel;
    VirtualTilesetDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
    QModelIndex mClickedIndex;
};

} // namespace Internal
} // namespace Tiled

#endif // VIRTUALTILESETVIEW_H
