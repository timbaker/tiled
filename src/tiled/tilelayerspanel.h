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

#ifndef TILELAYERSPANEL_H
#define TILELAYERSPANEL_H

#include <QAbstractListModel>
#include <QWidget>
#include <QTableView>

class QMenu;

namespace Tiled {
class Layer;
class Tile;

namespace Internal {

class LayersPanelDelegate;
class MapDocument;
class MixedTilesetView;
class Zoomable;

class LayersPanelModel : public QAbstractListModel
{
    Q_OBJECT
public:
    LayersPanelModel(QObject *parent = 0);
    ~LayersPanelModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(int layerIndex) const;

    void clear();
    void prependLayer(const QString &layerName, Tile *tile, int layerIndex);
    void insertLayer(int row, const QString &layerName, Tile *tile, int layerIndex);

    int layerAt(const QModelIndex &index) const;
    Tile *tileAt(const QModelIndex &index) const;
    QString labelAt(const QModelIndex &index) const;

    void scaleChanged(qreal scale);

private:
    class Item
    {
    public:
        Item() :
            mTile(0),
            mLayerIndex(-1)
        {
        }

        Item(const QString &label, Tiled::Tile *tile, int layerIndex) :
            mLabel(label),
            mTile(tile),
            mLayerIndex(layerIndex)
        {

        }

        QString mLabel;
        Tiled::Tile *mTile;
        int mLayerIndex;
        QString mToolTip;
        QBrush mBackground;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(int layerIndex) const;

    QList<Item*> mItems;
};

class LayersPanelView : public QTableView
{
    Q_OBJECT
public:
    explicit LayersPanelView(QWidget *parent = 0);

    QSize sizeHint() const;

    bool viewportEvent(QEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    LayersPanelModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    void contextMenuEvent(QContextMenuEvent *event);
    void setContextMenu(QMenu *menu)
    { mContextMenu = menu; }

    virtual void clear();

    void prependLayer(const QString &layerName, Tile *tile, int layerIndex);

    int maxHeaderWidth() const
    { return mMaxHeaderWidth; }

signals:
    void layerNameClicked(int layerIndex);

public slots:
    void scaleChanged(qreal scale);

private:
    LayersPanelModel *mModel;
    LayersPanelDelegate *mDelegate;
    Zoomable *mZoomable;
    QMenu *mContextMenu;
    int mMaxHeaderWidth;
    bool mIgnoreMouse;
};

class TileLayersPanel : public QWidget
{
    Q_OBJECT
public:
    TileLayersPanel(QWidget *parent = 0);

    void setDocument(MapDocument *doc);

    void setScale(qreal scale);
    qreal scale() const;

signals:
    void tilePicked(Tile *tile);

public slots:
    void setTilePosition(const QPoint &tilePos);

private slots:
    void setList();
    void activated(const QModelIndex &index);
    void layerNameClicked(int layerIndex);
    void currentChanged();
    void layerIndexChanged(int index);
    void layerChanged(int index);
    void regionAltered(const QRegion &region, Layer *layer);
    void showTileLayersPanelChanged(bool show);

private:
    MapDocument *mDocument;
    LayersPanelView *mView;
    int mCurrentLayerIndex;
    QPoint mTilePos;
};

}
}

#endif // TILELAYERSPANEL_H
