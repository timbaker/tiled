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

#ifndef CONTAINEROVERLAYDIALOG_H
#define CONTAINEROVERLAYDIALOG_H

#include <QMainWindow>

namespace Ui {
class ContainerOverlayDialog;
}

class ContainerOverlay;
class ContainerOverlayEntry;

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

class QUndoGroup;
class QUndoStack;

class ContainerOverlayDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit ContainerOverlayDialog(QWidget *parent = 0);
    ~ContainerOverlayDialog();

    QString setBaseTile(ContainerOverlay *overlay, const QString &tileName);
    QString setEntryTile(ContainerOverlayEntry *entry, int index, const QString &tileName);
    QString setEntryRoomName(ContainerOverlayEntry *entry, const QString &roomName);

    void insertOverlay(int index, ContainerOverlay *overlay);
    void removeOverlay(int index);
    void insertEntry(ContainerOverlay *overlay, int index, ContainerOverlayEntry *entry);
    void removeEntry(ContainerOverlay *overlay, int index);

protected:
    void setTilesetList();
    bool isOverlayTileUsed(const QString &tileName);
    bool isEntryTileUsed(const QString &_tileName);
    void updateUsedTiles();

    void closeEvent(QCloseEvent *event);
    bool confirmSave();
    QString getSaveLocation();
    void fileOpen(const QString &fileName);
    bool fileSave(const QString &fileName);

    typedef Tiled::Tileset Tileset;
private slots:
    void addOverlay();
    void addEntry();
    void setToNone();
    void overlayActivated(const QModelIndex &index);
    void scrollToNow(const QModelIndex &index);
    void tileActivated(const QModelIndex &index);
    void tilesetSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tileset *tileset);

    void tileDropped(ContainerOverlay *overlay, const QString &tileName);
    void tileDropped(ContainerOverlayEntry *entry, int index, const QString &tileName);
    void entryRoomNameEdited(ContainerOverlayEntry *entry, const QString &roomName);

    void fileOpen();
    bool fileSave();
    bool fileSaveAs();

    void remove();

    void updateWindowTitle();
    void syncUI();

private:
    Ui::ContainerOverlayDialog *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    QList<ContainerOverlay*> mOverlays;
    Tileset *mCurrentTileset;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
};

#endif // CONTAINEROVERLAYDIALOG_H
