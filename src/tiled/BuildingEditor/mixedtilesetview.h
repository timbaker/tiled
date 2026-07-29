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

class QMenu;

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
    ~MixedTilesetModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    enum {
        HeaderRole = Qt::UserRole,
        CategoryBgRole,
        ItemHeightRole,
        SurfaceRole,
    };
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

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

    void clear();
    void setTiles(const QList<Tile*> &tiles,
                  const QList<void*> &userData = QList<void*>(),
                  const QStringList &headers = QStringList());
    void setTileset(Tileset *tileset,
                    const QList<void *> &userData = QList<void*>(),
                    const QStringList &labels = QStringList());

    Tile *tileAt(const QModelIndex &index) const;
    QString headerAt(const QModelIndex &index) const;
    void *userDataAt(const QModelIndex &index) const;

    void setCategoryBounds(const QModelIndex &index, const QRect &bounds);
    void setCategoryBounds(Tile *tile, const QRect &bounds);
    void setCategoryBounds(int tileIndex, const QRect &bounds);
    void clearCategoryBounds(const QModelIndex &index);
    QRect categoryBounds(const QModelIndex &index) const;
    QRect categoryBounds(Tile *tile) const;

    void setOverlayTile(const QModelIndex &index, Tiled::Tile *tile);
    Tiled::Tile *overlayTile(const QModelIndex &index) const;
    Tiled::Tile *overlayTile(Tiled::Tile *tile) const;

    void scaleChanged(qreal scale);

    void redisplay();

    void setShowHeaders(bool show);

    void setShowLabels(bool show);
    bool showLabels() const { return mShowLabels; }
    void setLabel(Tile *tile, const QString &label);

    void setHighlightLabelledItems(bool highlight) { mHighlightLabelledItems = highlight; }
    bool highlightLabelledItems() const { return mHighlightLabelledItems; }

    void setShowEmptyTilesAsMissig(bool show);
    bool showEmptyTilesAsMissing() const { return mShowEmptyTilesAsMissing; }

    void setToolTip(int tileIndex, const QString &text);

    void setColumnCount(int count);

    void setDropCoords(const QModelIndex &dropIndex)
    { mDropIndex = dropIndex; }

    QModelIndex dropIndex() const
    { return mDropIndex; }

signals:
    void tileDropped(const QString &tilesetName, int tileId);
    void tileDroppedAt(const QString &tilesetName, int tileId, int row, int column, const QModelIndex &parent);

private:
    class Item
    {
    public:
        Item() :
            mTile(nullptr),
            mOverlayTile(nullptr),
            mUserData(nullptr),
            mItemHeightProperty(-1),
            mSurfaceProperty(-1)
        {
        }

        Item(Tiled::Tile *tile, void *userData = nullptr) :
            mTile(tile),
            mOverlayTile(nullptr),
            mUserData(userData),
            mItemHeightProperty(-1),
            mSurfaceProperty(-1)
        {

        }
        Item(const QString &tilesetName) :
            mTile(nullptr),
            mOverlayTile(nullptr),
            mUserData(nullptr),
            mTilesetName(tilesetName),
            mItemHeightProperty(-1),
            mSurfaceProperty(-1)
        {

        }

        int mIndex;
        Tiled::Tile *mTile;
        Tiled::Tile *mOverlayTile;
        void *mUserData;
        QString mTilesetName;
        QString mLabel;
        QString mToolTip;
        QBrush mBackground;
        QRect mCategoryBounds;
        QColor mCategoryColor;
        int mItemHeightProperty;
        int mSurfaceProperty;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(Tiled::Tile *tile) const;
    Item *toItem(void *userData) const;
    Item *toItem(int tileIndex) const;

    int indexOf(Item *item) { return item->mIndex; }

    bool hasMultipleTilesets(const QList<Tiled::Tile*>& tiles) const;

    QList<Item*> mItems;
    QList<Tiled::Tile*> mTiles;
    QList<void *> mUserData;
    Tiled::Tileset *mTileset;
    QMap<int,Item*> mTileItemsByIndex;
    QMap<Tiled::Tile*,Item*> mTileToItem;
    QMap<void*,Item*> mUserDataToItem;
    static QString mMimeType;
    static QString mGridMimeType;
    bool mShowHeaders;
    bool mShowLabels;
    bool mHighlightLabelledItems;
    bool mShowEmptyTilesAsMissing;
    int mColumnCount;
    QModelIndex mDropIndex;
};

class MixedTilesetView : public QTableView
{
    Q_OBJECT
public:
    explicit MixedTilesetView(QWidget *parent = 0);
    explicit MixedTilesetView(Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    bool viewportEvent(QEvent *event) override;

    MixedTilesetModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    bool mouseDown() const
    { return mMousePressed; }

    void contextMenuEvent(QContextMenuEvent *event) override;
    void setContextMenu(QMenu *menu)
    { mContextMenu = menu; }

    virtual void clear();
    void setTiles(const QList<Tile*> &tiles,
                  const QList<void*> &userData = QList<void*>(),
                  const QStringList &headers = QStringList());
    void setTileset(Tileset *tileset,
                    const QList<void *> &userData = QList<void*>(),
                    const QStringList &labels = QStringList());

    int maxHeaderWidth() const
    { return mMaxHeaderWidth; }

signals:
    void mousePressed();
    void mouseReleased();

    void tileEntered(const QModelIndex &index);
    void tileLeft(const QModelIndex &index);

public slots:
    void scaleChanged(qreal scale);
    void tilesetBackgroundColorChanged(const QColor& color);

private:
    void init();

private:
    MixedTilesetModel *mModel;
    Zoomable *mZoomable;
    bool mMousePressed;
    QMenu *mContextMenu;
    QModelIndex mToolTipIndex;
    int mMaxHeaderWidth;
};

} // namespace Internal
} // namespace Tiled

#endif // MIXEDTILESETVIEW_H
