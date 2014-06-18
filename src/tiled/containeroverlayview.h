/*
 * Copyright 2014, Tim Baker <treectrl@users.sf.net>
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

#ifndef CONTAINEROVERLAYVIEW_H
#define CONTAINEROVERLAYVIEW_H

#include <QAbstractListModel>
#include <QTableView>

class QMenu;

namespace Tiled {
class Tileset;
namespace Internal {
class Zoomable;
}
}

class ContainerOverlay;
class ContainerOverlayEntry;

class ContainerOverlayDelegate;

class ContainerOverlayModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ContainerOverlayModel(QObject *parent = 0);
    ~ContainerOverlayModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(ContainerOverlay *overlay);
    QModelIndex index(ContainerOverlayEntry *entry);

    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void clear();
    void setOverlays(const QList<ContainerOverlay *> &overlays);

    ContainerOverlay *overlayAt(const QModelIndex &index) const;
    ContainerOverlayEntry *entryAt(const QModelIndex &index) const;

    void insertOverlay(int index, ContainerOverlay *overlay);
    void removeOverlay(ContainerOverlay *overlay);
    void insertEntry(ContainerOverlay *overlay, int index, ContainerOverlayEntry *entry);
    void removeEntry(ContainerOverlay *overlay, int index);

    void scaleChanged(qreal scale);

    void setDropCoords(int entryIndex, const QModelIndex &dropIndex)
    { mDropEntryIndex = entryIndex, mDropIndex = dropIndex; }

    int dropEntryIndex() const
    { return mDropEntryIndex; }

    QModelIndex dropIndex() const
    { return mDropIndex; }

    void redisplay();
    void redisplay(ContainerOverlay *overlay);
    void redisplay(ContainerOverlayEntry *entry);

signals:
    void tileDropped(ContainerOverlay *overlay, const QString &tileName);
    void tileDropped(ContainerOverlayEntry *entry, int index, const QString &tileName);
    void entryRoomNameEdited(ContainerOverlayEntry *entry, const QString &roomName);

private:
    class Item
    {
    public:
        Item() :
            mOverlay(0),
            mEntry(0)
        {
        }

        Item(ContainerOverlay *overlay) :
            mOverlay(overlay),
            mEntry(0)
        {
        }

        Item(ContainerOverlayEntry *entry) :
            mOverlay(0),
            mEntry(entry)
        {
        }

        ContainerOverlay *mOverlay;
        ContainerOverlayEntry *mEntry;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(ContainerOverlay *overlay) const;
    Item *toItem(ContainerOverlayEntry *entry) const;

    static QString mMimeType;
    QList<Item*> mItems;
    int mDropEntryIndex;
    QModelIndex mDropIndex;
};

class ContainerOverlayView : public QTableView
{
    Q_OBJECT
public:
    explicit ContainerOverlayView(QWidget *parent = 0);
    explicit ContainerOverlayView(Tiled::Internal::Zoomable *zoomable, QWidget *parent = 0);

    QSize sizeHint() const;

    void mouseDoubleClickEvent(QMouseEvent *event);

    void wheelEvent(QWheelEvent *event);

    void dragMoveEvent(QDragMoveEvent *event);
    void dragLeaveEvent(QDragLeaveEvent *event);
    void dropEvent(QDropEvent *event);

    ContainerOverlayModel *model() const
    { return mModel; }

    ContainerOverlayDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void clear();
    void setOverlays(const QList<ContainerOverlay*> &overlays);

    void redisplay();
    void redisplay(ContainerOverlay *overlay);
    void redisplay(ContainerOverlayEntry *entry);

    typedef Tiled::Tileset Tileset;
public slots:
    void scaleChanged(qreal scale);

    void tilesetChanged(Tileset *tileset);
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

private:
    void init();

private:
    ContainerOverlayModel *mModel;
    ContainerOverlayDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
};

#endif // CONTAINEROVERLAYVIEW_H
