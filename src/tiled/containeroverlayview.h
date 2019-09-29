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

class AbstractOverlay;
class AbstractOverlayEntry;

class ContainerOverlayDelegate;

enum struct EditAbstractOverlay
{
    RoomName,
    Usage,
    Chance,
};


class ContainerOverlayModel : public QAbstractListModel
{
    Q_OBJECT
public:
    ContainerOverlayModel(QObject *parent = nullptr);
    ~ContainerOverlayModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex index(AbstractOverlay *overlay);
    QModelIndex index(AbstractOverlayEntry *entry);

    QStringList mimeTypes() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    void clear();
    void setOverlays(const QList<AbstractOverlay *> &overlays);

    AbstractOverlay *overlayAt(const QModelIndex &index) const;
    AbstractOverlayEntry *entryAt(const QModelIndex &index) const;

    void insertOverlay(int index, AbstractOverlay *overlay);
    void removeOverlay(AbstractOverlay *overlay);
    void insertEntry(AbstractOverlay *overlay, int index, AbstractOverlayEntry *entry);
    void removeEntry(AbstractOverlay *overlay, int index);

    void scaleChanged(qreal scale);

    void setDropCoords(int entryIndex, const QModelIndex &dropIndex)
    { mDropEntryIndex = entryIndex; mDropIndex = dropIndex; }

    int dropEntryIndex() const
    { return mDropEntryIndex; }

    QModelIndex dropIndex() const
    { return mDropIndex; }

    void redisplay();
    void redisplay(AbstractOverlay *overlay);
    void redisplay(AbstractOverlayEntry *entry);

    void setMoreThan2Tiles(bool moreThan2);
    bool moreThan2Tiles() const { return mMoreThan2Tiles; }

signals:
    void tileDropped(AbstractOverlay *overlay, const QStringList &tileName);
    void tileDropped(AbstractOverlayEntry *entry, int index, const QStringList &tileName);
    void entryRoomNameEdited(AbstractOverlayEntry *entry, const QString &roomName);
    void entryUsageEdited(AbstractOverlayEntry *entry, const QString &usage);
    void entryChanceEdited(AbstractOverlayEntry *entry, int chance);

private:
    class Item
    {
    public:
        Item() :
            mOverlay(nullptr),
            mEntry(nullptr)
        {
        }

        Item(AbstractOverlay *overlay) :
            mOverlay(overlay),
            mEntry(nullptr)
        {
        }

        Item(AbstractOverlayEntry *entry) :
            mOverlay(nullptr),
            mEntry(entry)
        {
        }

        AbstractOverlay *mOverlay;
        AbstractOverlayEntry *mEntry;
    };

    Item *toItem(const QModelIndex &index) const;
    Item *toItem(AbstractOverlay *overlay) const;
    Item *toItem(AbstractOverlayEntry *entry) const;

    static QString mMimeType;
    QList<Item*> mItems;
    int mDropEntryIndex = -1;
    QModelIndex mDropIndex;
    bool mMoreThan2Tiles = false;
};

class ContainerOverlayView : public QTableView
{
    Q_OBJECT
public:
    explicit ContainerOverlayView(QWidget *parent = nullptr);
    explicit ContainerOverlayView(Tiled::Internal::Zoomable *zoomable, QWidget *parent = nullptr);

    QSize sizeHint() const override;

    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void wheelEvent(QWheelEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void edit(const QModelIndex& index, EditAbstractOverlay edit);

    ContainerOverlayModel *model() const
    { return mModel; }

    ContainerOverlayDelegate *itemDelegate() const
    { return mDelegate; }

    void setZoomable(Tiled::Internal::Zoomable *zoomable);

    Tiled::Internal::Zoomable *zoomable() const
    { return mZoomable; }

    void setMoreThan2Tiles(bool moreThan2);

    QModelIndex getHoverIndex() { return mHoverIndex; }
    int getHoverEntryIndex() { return mHoverEntryIndex; }

    void clear();
    void setOverlays(const QList<AbstractOverlay*> &overlays);

    void redisplay();
    void redisplay(AbstractOverlay *overlay);
    void redisplay(AbstractOverlayEntry *entry);

    typedef Tiled::Tileset Tileset;

signals:
    void removeTile(AbstractOverlayEntry *entry, int index);
    void overlayEntryHover(const QModelIndex &index, int entryIndex);
    void showContextMenu(const QModelIndex &index, int entryIndex, QContextMenuEvent *event);

public slots:
    void scaleChanged(qreal scale);

    void tilesetChanged(Tileset *tileset);
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

protected:
    void contextMenuEvent(QContextMenuEvent *event);

private:
    void init();

private:
    ContainerOverlayModel *mModel;
    ContainerOverlayDelegate *mDelegate;
    Tiled::Internal::Zoomable *mZoomable;
    QModelIndex mHoverIndex;
    int mHoverEntryIndex = -1;
};

#endif // CONTAINEROVERLAYVIEW_H
