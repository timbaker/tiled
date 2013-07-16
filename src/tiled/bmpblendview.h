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

#ifndef BMPBLENDVIEW_H
#define BMPBLENDVIEW_H

#include <QAbstractListModel>
#include <QGraphicsView>
#include <QTableView>

namespace Tiled {
class BmpBlend;
class Map;

namespace Internal {
class BmpBlendDelegate;
class Zoomable;

class BmpBlendModel : public QAbstractListModel
{
    Q_OBJECT
public:
    BmpBlendModel(QObject *parent = 0);
    ~BmpBlendModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(BmpBlend *blend) const;

    void setBlends(const Map *map);
    void clear();

    QVector<BmpBlend*> blendsAt(const QModelIndex &index) const;
    int blendIndex(BmpBlend *blend) const;

    QStringList aliasTiles(const QString &alias) const
    {
        if (mAliasTiles.contains(alias))
            return mAliasTiles[alias];
        return QStringList();
    }

    void scaleChanged(qreal scale);

private:
    class Item
    {
    public:
        enum Direction {
            Unknown,
            N,
            S,
            E,
            W,
            NW,
            NE,
            SW,
            SE,
            DirectionCount
        };

        Item() :
            mBlend(DirectionCount),
            mOriginalBlend(DirectionCount)
        {
        }

        void addBlend(BmpBlend *blend);

        ~Item();

        QVector<BmpBlend*> mBlend;
        QVector<BmpBlend*> mOriginalBlend;
        int mIndex;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(BmpBlend *blend) const;

    QList<Item*> mItems;
    QMap<QString,QStringList> mAliasTiles;
};

class BmpBlendView : public QTableView
{
    Q_OBJECT
public:
    explicit BmpBlendView(QWidget *parent = 0);

    QSize sizeHint() const;

    bool viewportEvent(QEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    BmpBlendModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    void contextMenuEvent(QContextMenuEvent *event);
    void setContextMenu(QMenu *menu)
    { mContextMenu = menu; }

    void clear();
    void setBlends(const Map *map);

    int maxHeaderWidth() const
    { return mMaxHeaderWidth; }

    void setExpanded(bool expanded);
    bool isExpanded() const
    { return mExpanded; }

signals:
    void blendHighlighted(BmpBlend *blend, int dir);

public slots:
    void scaleChanged(qreal scale);
    void scrollToCurrentItem();

private:
    BmpBlendModel *mModel;
    BmpBlendDelegate *mDelegate;
    Zoomable *mZoomable;
    QMenu *mContextMenu;
    int mMaxHeaderWidth;
    bool mIgnoreMouse;
    bool mExpanded;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPBLENDVIEW_H
