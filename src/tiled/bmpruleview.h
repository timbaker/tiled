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

#ifndef BMPRULEVIEW_H
#define BMPRULEVIEW_H

#include <QAbstractListModel>
#include <QGraphicsView>
#include <QTableView>

namespace Tiled {
class BmpRule;
class Map;

namespace Internal {
class BmpRuleDelegate;
class Zoomable;

class BmpRuleModel : public QAbstractListModel
{
    Q_OBJECT
public:
    BmpRuleModel(QObject *parent = 0);
    ~BmpRuleModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(BmpRule *rule) const;

    void setRules(const Map *map);
    void clear();

    BmpRule *ruleAt(const QModelIndex &index) const;
    int ruleIndex(BmpRule *rule) const;

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
        Item() :
            mRule(0),
            mOriginalRule(0)
        {
        }

        Item(BmpRule *rule);
        ~Item();

        BmpRule *mRule;
        BmpRule *mOriginalRule;
        int mIndex;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(BmpRule *rule) const;

    QList<Item*> mItems;
    QMap<QString,QStringList> mAliasTiles;
};

class BmpRuleView : public QTableView
{
    Q_OBJECT
public:
    explicit BmpRuleView(QWidget *parent = 0);

    QSize sizeHint() const;

    bool viewportEvent(QEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    BmpRuleModel *model() const
    { return mModel; }

    void setZoomable(Zoomable *zoomable);

    Zoomable *zoomable() const
    { return mZoomable; }

    void contextMenuEvent(QContextMenuEvent *event);
    void setContextMenu(QMenu *menu)
    { mContextMenu = menu; }

    void clear();
    void setRules(const Map *map);

    int maxHeaderWidth() const
    { return mMaxHeaderWidth; }

    void setExpanded(bool expanded);
    bool isExpanded() const
    { return mExpanded; }

    void setShowObsolete(bool show);

signals:
    void layerNameClicked(int layerIndex);

public slots:
    void scaleChanged(qreal scale);
    void scrollToCurrentItem();

private:
    BmpRuleModel *mModel;
    BmpRuleDelegate *mDelegate;
    Zoomable *mZoomable;
    QMenu *mContextMenu;
    int mMaxHeaderWidth;
    bool mIgnoreMouse;
    bool mExpanded;
    bool mShowObsolete = false;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPRULEVIEW_H
