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
    Q_OBJECT
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
    QModelIndex index(void *userData);

    Qt::DropActions supportedDropActions() const
    { return Qt::CopyAction; }

    QStringList mimeTypes() const;
    QMimeData *mimeData(const QModelIndexList &indexes) const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void setTiles(const QList<Tile*> &tiles,
                  const QList<void*> &userData = QList<void*>(),
                  const QStringList &headers = QStringList());
    void setTileset(Tileset *tileset, const QStringList &labels = QStringList());

    Tile *tileAt(const QModelIndex &index) const;
    QString headerAt(const QModelIndex &index) const;
    void *userDataAt(const QModelIndex &index) const;

    void setCategoryBounds(Tile *tile, const QRect &bounds);
    QRect categoryBounds(Tile *tile) const;

    void scaleChanged(qreal scale);

    void setShowHeaders(bool show);

    void setShowLabels(bool show) { mShowLabels = show; }
    bool showLabels() const { return mShowLabels; }
    void setLabel(Tile *tile, const QString &label);

signals:
    void tileDropped(const QString &tilesetName, int tileId);

private:
    class Item
    {
    public:
        Item() :
            mTile(0),
            mUserData(0)
        {
        }

        Item(Tiled::Tile *tile, void *userData = 0) :
            mTile(tile),
            mUserData(userData)
        {

        }
        Item(const QString &tilesetName) :
            mTile(0),
            mUserData(0),
            mTilesetName(tilesetName)
        {

        }

        Tiled::Tile *mTile;
        void *mUserData;
        QString mTilesetName;
        QString mLabel;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Tiled::Tile *tile) const;
    Item *toItem(void *userData) const;

    QList<Item*> mItems;
    QList<Tiled::Tile*> mTiles;
    QList<void *> mUserData;
    Tiled::Tileset *mTileset;
    QMap<Tiled::Tile*,QRect> mCategoryBounds;
    static QString mMimeType;
    bool mShowHeaders;
    bool mShowLabels;
};

class MixedTilesetView : public QTableView
{
    Q_OBJECT
public:
    explicit MixedTilesetView(QWidget *parent = 0);
    explicit MixedTilesetView(Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    MixedTilesetModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    bool mouseDown() const
    { return mMousePressed; }

signals:
    void mousePressed();
    void mouseReleased();
    
public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    MixedTilesetModel *mModel;
    Zoomable *mZoomable;
    bool mMousePressed;
};

} // namespace Internal
} // namespace Tiled

#endif // MIXEDTILESETVIEW_H
