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

#ifndef TILECATEGORYVIEW_H
#define TILECATEGORYVIEW_H

#include <QAbstractListModel>
#include <QTableView>

namespace Tiled {
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingTileCategory;
class BuildingTileEntry;

class BuildingEntryDelegate;

class TileCategoryModel : public QAbstractListModel
{
    Q_OBJECT
public:
    TileCategoryModel(QObject *parent = 0);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(BuildingTileEntry *entry, int e) const;

    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void setCategory(BuildingTileCategory *category);

    BuildingTileEntry *entryAt(const QModelIndex &index) const;
    int enumAt(const QModelIndex &index) const;

    void scaleChanged(qreal scale);

public:
    int itemsPerEntry() const
    { return shadowImageColumns() * shadowImageRows(); }
    int shadowImageColumns() const;
    int shadowImageRows() const;

signals:
    void tileDropped(BuildingTileEntry *entry, int e, const QString &tileName);

private:
    BuildingTileCategory *mCategory;
    static QString mMimeType;
};

class TileCategoryView : public QTableView
{
    Q_OBJECT
public:
    explicit TileCategoryView(QWidget *parent = 0);
    explicit TileCategoryView(Tiled::Internal::Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    TileCategoryModel *model() const
    { return mModel; }

    BuildingEntryDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

signals:

public slots:
    void scaleChanged(qreal scale);

private:
    void init();

private:
    TileCategoryModel *mModel;
    BuildingEntryDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
};

} // namespace BuildingEditor

#endif // TILECATEGORYVIEW_H
