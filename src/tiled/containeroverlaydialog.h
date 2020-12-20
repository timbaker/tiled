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

class AbstractOverlay;
class AbstractOverlayEntry;

namespace Ui {
class ContainerOverlayDialog;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

class QUndoGroup;
class QUndoStack;

class AbstractOverlayDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit AbstractOverlayDialog(QWidget *parent = nullptr);
    ~AbstractOverlayDialog();

    QString setBaseTile(AbstractOverlay *overlay, const QString &tileName);
    void addEntryTile(AbstractOverlayEntry *entry, int index, const QString &tileName);
    void removeEntryTile(AbstractOverlayEntry *entry, int index);
    QString setEntryTile(AbstractOverlayEntry *entry, int index, const QString &tileName);
    QString setEntryRoomName(AbstractOverlayEntry *entry, const QString &roomName);
    QString setEntryUsage(AbstractOverlayEntry *entry, const QString &usage);
    int setEntryChance(AbstractOverlayEntry *entry, int chance);

    void insertOverlay(int index, AbstractOverlay *overlay);
    void removeOverlay(int index);
    void insertEntry(AbstractOverlay *overlay, int index, AbstractOverlayEntry *entry);
    void removeEntry(AbstractOverlay *overlay, int index);

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
    virtual bool fileOpen(const QString &fileName, QList<AbstractOverlay*> &overlays) = 0;
    virtual bool fileSave(const QString &fileName, const QList<AbstractOverlay*> &overlays) = 0;
    bool isEmptySprite(const QString& tileName);

    typedef Tiled::Tileset Tileset;
protected slots:
    void addOverlay();
    void addEntry();
    void setToNone();
    void overlayActivated(const QModelIndex &index);
    void overlayEntryHover(const QModelIndex &index, int entryIndex);
    void scrollToNow(const QModelIndex &index);
    void tileActivated(const QModelIndex &index);
    void tilesetSelectionChanged();
    void manageTilesets();
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tileset *tileset);

    virtual void tileDropped(AbstractOverlay *overlay, const QStringList &tileNames);
    void tileDropped(AbstractOverlayEntry *entry, int index, const QStringList &tileName);
    void entryRoomNameEdited(AbstractOverlayEntry *entry, const QString &roomName);
    void entryUsageEdited(AbstractOverlayEntry *entry, const QString &usage);
    void entryChanceEdited(AbstractOverlayEntry *entry, int chance);
    void removeTile(AbstractOverlayEntry *entry, int index);

    void fileNew();
    void fileOpen();
    bool fileSave();
    bool fileSaveAs();

    void remove();

    void updateWindowTitle();
    void syncUI();

protected:
    virtual QString defaultWindowTitle() const { return QLatin1Literal("Overlays"); }
    virtual AbstractOverlay *createOverlay(Tiled::Tile *tile) = 0;
    virtual AbstractOverlayEntry *createEntry(AbstractOverlay* parent) = 0;

protected:
    Ui::ContainerOverlayDialog *ui;
    QString mFileName;
    Tiled::Internal::Zoomable *mZoomable;
    QList<AbstractOverlay*> mOverlays;
    Tileset *mCurrentTileset;
    QString mHoverTileName;
    QString mError;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
};

class ContainerOverlayDialog : public AbstractOverlayDialog
{
    Q_OBJECT
public:
    explicit ContainerOverlayDialog(QWidget *parent = nullptr);

protected slots:
    void tileDropped(AbstractOverlay *overlay, const QStringList &tileNames);
    void showContextMenu(const QModelIndex &index, int entryIndex, QContextMenuEvent *event);

protected:
    bool fileOpen(const QString &fileName, QList<AbstractOverlay*> &overlays) override;
    bool fileSave(const QString &fileName, const QList<AbstractOverlay*> &overlays) override;
    QString defaultWindowTitle() const override
    { return tr("Container Overlays"); }
    AbstractOverlay *createOverlay(Tiled::Tile *tile) override;
    AbstractOverlayEntry *createEntry(AbstractOverlay* parent) override;
};

#endif // CONTAINEROVERLAYDIALOG_H
