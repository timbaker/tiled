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

#include "containeroverlaydialog.h"
#include "ui_containeroverlaydialog.h"

#include "containeroverlayfile.h"
#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include "tile.h"
#include "tileset.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

/////

namespace {

class BaseOverlayCommand : public QUndoCommand
{
public:
    BaseOverlayCommand(ContainerOverlayDialog *d, const char *text) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", text)),
        mDialog(d)
    {
    }

    ContainerOverlayDialog *mDialog;
};

class SetBaseTile : public BaseOverlayCommand
{
public:
    SetBaseTile(ContainerOverlayDialog *d, ContainerOverlay *overlay, const QString &tileName) :
        BaseOverlayCommand(d, "Set Overlay Base Tile"),
        mOverlay(overlay),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mTileName = mDialog->setBaseTile(mOverlay, mTileName);
    }

    void redo()
    {
        mTileName = mDialog->setBaseTile(mOverlay, mTileName);
    }

    ContainerOverlay *mOverlay;
    QString mTileName;
};

class SetEntryTile : public BaseOverlayCommand
{
public:
    SetEntryTile(ContainerOverlayDialog *d, ContainerOverlayEntry *entry, int index, const QString &tileName) :
        BaseOverlayCommand(d, "Set Overlay Tile"),
        mEntry(entry),
        mIndex(index),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mTileName = mDialog->setEntryTile(mEntry, mIndex, mTileName);
    }

    void redo()
    {
        mTileName = mDialog->setEntryTile(mEntry, mIndex, mTileName);
    }

    ContainerOverlayEntry *mEntry;
    int mIndex;
    QString mTileName;
};

class SetEntryRoomName : public BaseOverlayCommand
{
public:
    SetEntryRoomName(ContainerOverlayDialog *d, ContainerOverlayEntry *entry, const QString &roomName) :
        BaseOverlayCommand(d, "Set Room Name"),
        mEntry(entry),
        mRoomName(roomName)
    {
    }

    void undo()
    {
        mRoomName = mDialog->setEntryRoomName(mEntry, mRoomName);
    }

    void redo()
    {
        mRoomName = mDialog->setEntryRoomName(mEntry, mRoomName);
    }

    ContainerOverlayEntry *mEntry;
    QString mRoomName;
};

class InsertOverlay : public BaseOverlayCommand
{
public:
    InsertOverlay(ContainerOverlayDialog *d, int index, ContainerOverlay *overlay) :
        BaseOverlayCommand(d, "Insert Overlay"),
        mOverlay(overlay),
        mIndex(index)
    {
    }

    void undo()
    {
        mDialog->removeOverlay(mIndex);
    }

    void redo()
    {
        mDialog->insertOverlay(mIndex, mOverlay);
    }

    int mIndex;
    ContainerOverlay *mOverlay;
};

class RemoveOverlay : public BaseOverlayCommand
{
public:
    RemoveOverlay(ContainerOverlayDialog *d, int index, ContainerOverlay *overlay) :
        BaseOverlayCommand(d, "Remove Overlay"),
        mOverlay(overlay),
        mIndex(index)
    {
    }

    void undo()
    {
        mDialog->insertOverlay(mIndex, mOverlay);
    }

    void redo()
    {
        mDialog->removeOverlay(mIndex);
    }

    int mIndex;
    ContainerOverlay *mOverlay;
};

class InsertEntry : public BaseOverlayCommand
{
public:
    InsertEntry(ContainerOverlayDialog *d, ContainerOverlay *overlay, int index, ContainerOverlayEntry *entry) :
        BaseOverlayCommand(d, "Add Room"),
        mOverlay(overlay),
        mIndex(index),
        mEntry(entry)
    {
    }

    void undo()
    {
        mDialog->removeEntry(mOverlay, mIndex);
    }

    void redo()
    {
        mDialog->insertEntry(mOverlay, mIndex, mEntry);
    }

    ContainerOverlay *mOverlay;
    int mIndex;
    ContainerOverlayEntry *mEntry;
};

class RemoveEntry : public BaseOverlayCommand
{
public:
    RemoveEntry(ContainerOverlayDialog *d, int index, ContainerOverlayEntry *entry) :
        BaseOverlayCommand(d, "Remove Room"),
        mOverlay(entry->mParent),
        mIndex(index),
        mEntry(entry)
    {
    }

    void undo()
    {
        mDialog->insertEntry(mOverlay, mIndex, mEntry);
    }

    void redo()
    {
        mDialog->removeEntry(mOverlay, mIndex);
    }

    ContainerOverlay *mOverlay;
    int mIndex;
    ContainerOverlayEntry *mEntry;
};

} // namespace anon

/////

ContainerOverlayDialog::ContainerOverlayDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ContainerOverlayDialog),
    mZoomable(new Zoomable),
    mCurrentTileset(0),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    undoAction->setIcon(undoIcon);
    redoAction->setIcon(redoIcon);
    ui->toolBar->addActions(QList<QAction*>() << undoAction << redoAction);

    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(syncUI()));

    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(fileOpen()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(fileSaveAs()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

    connect(ui->actionAddOverlay, SIGNAL(triggered()), SLOT(addOverlay()));
    connect(ui->actionAddRoom, SIGNAL(triggered()), SLOT(addEntry()));
    connect(ui->actionSetToNone, SIGNAL(triggered()), SLOT(setToNone()));
    connect(ui->actionRemove, SIGNAL(triggered()), SLOT(remove()));

    connect(ui->overlayView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(syncUI()));
    connect(ui->overlayView, SIGNAL(activated(QModelIndex)),
           SLOT(overlayActivated(QModelIndex)));
    connect(ui->overlayView->model(), SIGNAL(tileDropped(ContainerOverlay*,QString)),
            SLOT(tileDropped(ContainerOverlay*,QString)));
    connect(ui->overlayView->model(), SIGNAL(tileDropped(ContainerOverlayEntry*,int,QString)),
            SLOT(tileDropped(ContainerOverlayEntry*,int,QString)));
    connect(ui->overlayView->model(), SIGNAL(entryRoomNameEdited(ContainerOverlayEntry*,QString)),
            SLOT(entryRoomNameEdited(ContainerOverlayEntry*,QString)));

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(ui->tilesetList, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetRemoved(Tiled::Tileset*)),
            SLOT(tilesetRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    ui->tilesetTilesView->setZoomable(mZoomable);
    ui->tilesetTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tilesetTilesView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(syncUI()));
    connect(ui->tilesetTilesView, SIGNAL(activated(QModelIndex)),
            SLOT(tileActivated(QModelIndex)));

    ui->tilesetTilesView->setDragEnabled(true);

//    ContainerOverlayFile cof;
//    cof.read(QLatin1String("D:/pz/mpspmerge/workdir/media/lua/server/Items/WorldFiller.lua"));
/*
    ContainerOverlay *overlay = new ContainerOverlay;
    overlay->mTileName = QLatin1String("location_shop_generic_01_40");
    if (ContainerOverlayEntry *entry = new ContainerOverlayEntry) {
        entry->mParent = overlay;
        entry->mRoomName = QLatin1String("clothesstore");
        entry->mTiles += QLatin1String("clothing_01_8");
        entry->mTiles += QLatin1String("clothing_01_16");
        overlay->mEntries += entry;
    }
    if (ContainerOverlayEntry *entry = new ContainerOverlayEntry) {
        entry->mParent = overlay;
        entry->mRoomName = QLatin1String("generalstore");
        entry->mTiles += QLatin1String("clothing_01_8");
        entry->mTiles += QLatin1String("clothing_01_16");
        overlay->mEntries += entry;
    }
    if (ContainerOverlayEntry *entry = new ContainerOverlayEntry) {
        entry->mParent = overlay;
        entry->mRoomName = QLatin1String("departmentstore");
        entry->mTiles += QLatin1String("clothing_01_8");
        entry->mTiles += QLatin1String("clothing_01_16");
        overlay->mEntries += entry;
    }
*/

    setTilesetList();

    syncUI();
}

ContainerOverlayDialog::~ContainerOverlayDialog()
{
    delete ui;
}

QString ContainerOverlayDialog::setBaseTile(ContainerOverlay *overlay,
                                            const QString &tileName)
{
    QString old = overlay->mTileName;
    overlay->mTileName = tileName;
    ui->overlayView->redisplay(overlay);
    updateUsedTiles();
    return old;
}

QString ContainerOverlayDialog::setEntryTile(ContainerOverlayEntry *entry,
                                             int index, const QString &tileName)
{
    QString old = entry->mTiles[index];
    entry->mTiles[index] = tileName;
    ui->overlayView->redisplay(entry);
    updateUsedTiles();
    return old;
}

QString ContainerOverlayDialog::setEntryRoomName(ContainerOverlayEntry *entry, const QString &roomName)
{
    QString old = entry->mRoomName;
    entry->mRoomName = roomName;
    ui->overlayView->redisplay(entry);
    return old;
}

void ContainerOverlayDialog::insertOverlay(int index, ContainerOverlay *overlay)
{
    mOverlays.insert(index, overlay);
    ui->overlayView->model()->insertOverlay(index, overlay);
    ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(overlay));
    updateUsedTiles();
}

void ContainerOverlayDialog::removeOverlay(int index)
{
    ContainerOverlay *overlay = mOverlays.takeAt(index);
    ui->overlayView->model()->removeOverlay(overlay);
    updateUsedTiles();
}

void ContainerOverlayDialog::insertEntry(ContainerOverlay *overlay, int index, ContainerOverlayEntry *entry)
{
    overlay->mEntries.insert(index, entry);
    ui->overlayView->model()->insertEntry(overlay, index, entry);
    ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(entry));
    updateUsedTiles();
}

void ContainerOverlayDialog::removeEntry(ContainerOverlay *overlay, int index)
{
    /*ContainerOverlayEntry *entry = */overlay->mEntries.takeAt(index);
    ui->overlayView->model()->removeEntry(overlay, index);
    updateUsedTiles();
}

void ContainerOverlayDialog::setTilesetList()
{
    ui->tilesetList->clear();
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing())
            item->setForeground(Qt::red);
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setFixedWidth(width + 16 + sbw);
}

bool ContainerOverlayDialog::isOverlayTileUsed(const QString &_tileName)
{
    QString tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(_tileName);
    foreach (ContainerOverlay *overlay, mOverlays)
        if (overlay->mTileName == tileName)
            return true;
    return false;
}

bool ContainerOverlayDialog::isEntryTileUsed(const QString &_tileName)
{
    QString tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(_tileName);
    foreach (ContainerOverlay *overlay, mOverlays) {
        foreach (ContainerOverlayEntry *entry, overlay->mEntries) {
            if (entry->mTiles.contains(tileName))
                return true;
        }
    }
    return false;
}

void ContainerOverlayDialog::addOverlay()
{
    QModelIndex index = ui->tilesetTilesView->currentIndex();
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (!tile)
        return;
    ContainerOverlay *overlay = new ContainerOverlay;
    overlay->mTileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
    ContainerOverlayEntry *entry = new ContainerOverlayEntry;
    entry->mParent = overlay;
    entry->mRoomName = QLatin1String("other");
    entry->mTiles += QLatin1String("none");
    entry->mTiles += QLatin1String("none");
    overlay->mEntries += entry;

    QMap<QString, ContainerOverlay*> map;
    foreach (ContainerOverlay *overlay, mOverlays)
        map[overlay->mTileName] = overlay;
    map[overlay->mTileName] = overlay;
    QList<ContainerOverlay*> sorted = map.values();
    int index2 = sorted.indexOf(overlay);

    mUndoStack->push(new InsertOverlay(this, index2, overlay));
}

void ContainerOverlayDialog::addEntry()
{
    QModelIndex index = ui->overlayView->currentIndex();
    ContainerOverlay *overlay = ui->overlayView->model()->overlayAt(index);
    if (!overlay)
        return;
    ContainerOverlayEntry *entry = new ContainerOverlayEntry;
    entry->mParent = overlay;
    entry->mRoomName = QLatin1String("other");
    entry->mTiles += QLatin1String("none");
    entry->mTiles += QLatin1String("none");
    mUndoStack->push(new InsertEntry(this, overlay, overlay->mEntries.size(), entry));
}

void ContainerOverlayDialog::setToNone()
{
    QModelIndex index = ui->overlayView->currentIndex();
    ContainerOverlayEntry *entry = ui->overlayView->model()->entryAt(index);
    if (!entry)
        return;
    QList<QUndoCommand*> commands;
    for (int i = 0; i < entry->mTiles.size(); i++)
        if (entry->mTiles[i] != QLatin1String("none"))
            commands += new SetEntryTile(this, entry, i, QLatin1String("none"));
    if (commands.size() > 1)
        mUndoStack->beginMacro(QLatin1String("Set To None"));
    foreach (QUndoCommand *uc, commands)
        mUndoStack->push(uc);
    if (commands.size() > 1)
        mUndoStack->endMacro();
}

void ContainerOverlayDialog::overlayActivated(const QModelIndex &index)
{
    ContainerOverlay *overlay = ui->overlayView->model()->overlayAt(index);
    ContainerOverlayEntry *entry = ui->overlayView->model()->entryAt(index);
    if (overlay) {
        if (Tiled::Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(overlay->mTileName)) {
            int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset()->name());
            ui->tilesetList->setCurrentRow(row);
            QModelIndex tileIndex = ui->tilesetTilesView->model()->index(tile);
            ui->tilesetTilesView->setCurrentIndex(tileIndex);
            QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                                      Q_ARG(QModelIndex, tileIndex));
        }
    }
    if (entry) {
        if (Tiled::Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(entry->mTiles.first())) {
            int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset()->name());
            ui->tilesetList->setCurrentRow(row);
            QModelIndex tileIndex = ui->tilesetTilesView->model()->index(tile);
            ui->tilesetTilesView->setCurrentIndex(tileIndex);
            QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                                      Q_ARG(QModelIndex, tileIndex));
        }
    }
}

void ContainerOverlayDialog::scrollToNow(const QModelIndex &index)
{
    ui->tilesetTilesView->scrollTo(index);
}

void ContainerOverlayDialog::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == 0)
        return;

    foreach (ContainerOverlay *overlay, mOverlays) {
        if (BuildingEditor::BuildingTilesMgr::normalizeTileName(overlay->mTileName) == BuildingEditor::BuildingTilesMgr::nameForTile(tile)) {
            ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(overlay));
            return;
        }

        foreach (ContainerOverlayEntry *entry, overlay->mEntries) {
            foreach (QString tileName, entry->mTiles)
            if (BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName) == BuildingEditor::BuildingTilesMgr::nameForTile(tile)) {
                ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(entry));
                return;
            }
        }
    }
}

void ContainerOverlayDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mCurrentTileset = 0;
    if (item) {
        int row = ui->tilesetList->row(item);
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
        if (mCurrentTileset->isMissing())
            ui->tilesetTilesView->clear();
        else {
            ui->tilesetTilesView->setTileset(mCurrentTileset);
            updateUsedTiles();
        }
    } else {
        ui->tilesetTilesView->clear();
    }
    syncUI();
}

void ContainerOverlayDialog::updateUsedTiles()
{
    if (!mCurrentTileset)
        return;

    for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
        QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(mCurrentTileset->name(), i);
        if (isOverlayTileUsed(tileName) || isEntryTileUsed(tileName))
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
        else
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
    }
    ui->tilesetTilesView->model()->redisplay();
}

void ContainerOverlayDialog::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt())
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
}

void ContainerOverlayDialog::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

void ContainerOverlayDialog::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void ContainerOverlayDialog::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

// Called when a tileset image changes or a missing tileset was found.
void ContainerOverlayDialog::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        if (tileset->isMissing())
            ui->tilesetTilesView->clear();
        else
            ui->tilesetTilesView->setTileset(tileset);
    }

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesetList->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
}

void ContainerOverlayDialog::tileDropped(ContainerOverlay *overlay, const QString &tileName)
{
    if (isOverlayTileUsed(tileName))
        return;
    mUndoStack->push(new SetBaseTile(this, overlay, tileName));
}

void ContainerOverlayDialog::tileDropped(ContainerOverlayEntry *entry,
                                         int index, const QString &tileName)
{
    mUndoStack->push(new SetEntryTile(this, entry, index, tileName));
}

void ContainerOverlayDialog::entryRoomNameEdited(ContainerOverlayEntry *entry, const QString &roomName)
{
    mUndoStack->push(new SetEntryRoomName(this, entry, roomName));
}

void ContainerOverlayDialog::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .lua file"),
                                                    lastPath,
                                                    QLatin1String("Lua files (*.lua)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    fileOpen(fileName);

    syncUI();
}

void ContainerOverlayDialog::fileOpen(const QString &fileName)
{
    ContainerOverlayFile cof;
    if (!cof.read(fileName)) {
        QMessageBox::warning(this, tr("Error reading file"), cof.errorString());
        return;
    }

    mFileName = fileName;
    mOverlays = cof.mOverlays;
    cof.mOverlays.clear();
    ui->overlayView->setOverlays(mOverlays);
    updateUsedTiles();

    syncUI();
}

void ContainerOverlayDialog::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        mFileName.clear();
        ui->overlayView->clear();// clearDocument();
        mOverlays.clear();
        mUndoStack->clear();
        updateUsedTiles();
        syncUI();
/*
        QSettings settings;
        settings.beginGroup(QLatin1String("ContainerOverlay"));
        settings.setValue(QLatin1String("geometry"), saveGeometry());
        settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
        settings.setValue(QLatin1String("CurrentTileset"), mCurrentTilesetName);
        settings.endGroup();

        saveSplitterSizes(ui->splitter);
*/
        event->accept();
    } else {
        event->ignore();
    }

//    QMainWindow::closeEvent(event);
}

bool ContainerOverlayDialog::confirmSave()
{
    if (mFileName.isEmpty() || mUndoStack->isClean())
        return true;

    int ret = QMessageBox::warning(
            this, tr("Unsaved Changes"),
            tr("There are unsaved changes. Do you want to save now?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (ret) {
    case QMessageBox::Save:    return fileSave();
    case QMessageBox::Discard: return true;
    case QMessageBox::Cancel:
    default:
        return false;
    }
}

QString ContainerOverlayDialog::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString suggestedFileName = QLatin1String("WorldFiller.lua");
    if (mFileName.isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty())
            suggestedFileName = lastPath + QLatin1String("/WorldFiller.lua");
    } else {
        suggestedFileName = mFileName;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Lua files (*.lua)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absoluteFilePath());

    return fileName;
}

bool ContainerOverlayDialog::fileSave()
{
    if (mFileName.length())
        return fileSave(mFileName);
    else
        return fileSaveAs();
}

bool ContainerOverlayDialog::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

bool ContainerOverlayDialog::fileSave(const QString &fileName)
{
    ContainerOverlayFile cof;
    cof.mOverlays = mOverlays;

    if (!cof.write(fileName)) {
        cof.mOverlays.clear();
        QMessageBox::warning(this, tr("Error writing file"), cof.errorString());
        return false;
    }
    cof.mOverlays.clear();

    mFileName = fileName;
    mUndoStack->setClean();

    syncUI();

    return true;
}

void ContainerOverlayDialog::updateWindowTitle()
{
    if (mFileName.length()) {
        QString fileName = QDir::toNativeSeparators(mFileName);
        setWindowTitle(tr("[*]%1").arg(fileName));
//    } else if (mTileDefFile) {
//        setWindowTitle(tr("[*]Untitled"));
    } else {
        setWindowTitle(tr("Container Overlays"));
    }
    setWindowModified(!mUndoStack->isClean());
}

void ContainerOverlayDialog::remove()
{
    QModelIndexList selected = ui->overlayView->selectionModel()->selectedIndexes();
    ContainerOverlay *overlay = ui->overlayView->model()->overlayAt(selected.first());
    ContainerOverlayEntry *entry = ui->overlayView->model()->entryAt(selected.first());
    if (overlay) {
        mUndoStack->push(new RemoveOverlay(this, mOverlays.indexOf(overlay), overlay));
    }
    if (entry) {
        mUndoStack->push(new RemoveEntry(this, entry->mParent->mEntries.indexOf(entry), entry));
    }
}

void ContainerOverlayDialog::syncUI()
{
    ui->actionSave->setEnabled(!mFileName.isEmpty() && !mUndoStack->isClean());
    ui->actionSaveAs->setEnabled(!mFileName.isEmpty());

    QString tileName;
    if (Tile *tile = ui->tilesetTilesView->model()->tileAt(ui->tilesetTilesView->currentIndex()))
        tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
    ui->actionAddOverlay->setEnabled(!mFileName.isEmpty() && !tileName.isEmpty() && !isOverlayTileUsed(tileName));

    QModelIndexList selected = ui->overlayView->selectionModel()->selectedIndexes();
    ui->actionAddRoom->setEnabled(selected.size() && ui->overlayView->model()->overlayAt(selected.first()));
    ui->actionSetToNone->setEnabled(selected.size() && ui->overlayView->model()->entryAt(selected.first()));
    ui->actionRemove->setEnabled(selected.size() > 0);

    updateWindowTitle();
}
