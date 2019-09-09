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

// FIXME: shouldn't know anything about this class
#include "tileoverlayfile.h"

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
    BaseOverlayCommand(AbstractOverlayDialog *d, const char *text) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", text)),
        mDialog(d)
    {
    }

    AbstractOverlayDialog *mDialog;
};

class SetBaseTile : public BaseOverlayCommand
{
public:
    SetBaseTile(AbstractOverlayDialog *d, AbstractOverlay *overlay, const QString &tileName) :
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

    AbstractOverlay *mOverlay;
    QString mTileName;
};

class AddEntryTile : public BaseOverlayCommand
{
public:
    AddEntryTile(AbstractOverlayDialog *d, AbstractOverlayEntry *entry, const QString &tileName) :
        BaseOverlayCommand(d, "Add Overlay Tile"),
        mEntry(entry),
        mIndex(entry->tiles().size()),
        mTileName(tileName)
    {
    }

    void undo()
    {
        mDialog->removeEntryTile(mEntry, mIndex);
    }

    void redo()
    {
        mDialog->addEntryTile(mEntry, mIndex, mTileName);
    }

    AbstractOverlayEntry *mEntry;
    int mIndex;
    QString mTileName;
};

class RemoveEntryTile : public BaseOverlayCommand
{
public:
    RemoveEntryTile(AbstractOverlayDialog *d, AbstractOverlayEntry *entry, int index) :
        BaseOverlayCommand(d, "Remove Overlay Tile"),
        mEntry(entry),
        mIndex(index),
        mTileName(entry->tiles()[index])
    {
    }

    void undo()
    {
        mDialog->addEntryTile(mEntry, mIndex, mTileName);
    }

    void redo()
    {
        mDialog->removeEntryTile(mEntry, mIndex);
    }

    AbstractOverlayEntry *mEntry;
    int mIndex;
    QString mTileName;
};

class SetEntryTile : public BaseOverlayCommand
{
public:
    SetEntryTile(AbstractOverlayDialog *d, AbstractOverlayEntry *entry, int index, const QString &tileName) :
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

    AbstractOverlayEntry *mEntry;
    int mIndex;
    QString mTileName;
};

class SetEntryRoomName : public BaseOverlayCommand
{
public:
    SetEntryRoomName(AbstractOverlayDialog *d, AbstractOverlayEntry *entry, const QString &roomName) :
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

    AbstractOverlayEntry *mEntry;
    QString mRoomName;
};

// FIXME: this base class shouldn't know anything about TileOverlayEntry
class SetEntryChance : public BaseOverlayCommand
{
public:
    SetEntryChance(AbstractOverlayDialog *d, AbstractOverlayEntry *entry, int chance) :
        BaseOverlayCommand(d, "Set Chance"),
        mEntry(entry),
        mChance(chance)
    {
    }

    void undo()
    {
        mChance = mDialog->setEntryChance(mEntry, mChance);
    }

    void redo()
    {
        mChance = mDialog->setEntryChance(mEntry, mChance);
    }

    AbstractOverlayEntry *mEntry;
    int mChance;
};

class InsertOverlay : public BaseOverlayCommand
{
public:
    InsertOverlay(AbstractOverlayDialog *d, int index, AbstractOverlay *overlay) :
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
    AbstractOverlay *mOverlay;
};

class RemoveOverlay : public BaseOverlayCommand
{
public:
    RemoveOverlay(AbstractOverlayDialog *d, int index, AbstractOverlay *overlay) :
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
    AbstractOverlay *mOverlay;
};

class InsertEntry : public BaseOverlayCommand
{
public:
    InsertEntry(AbstractOverlayDialog *d, AbstractOverlay *overlay, int index, AbstractOverlayEntry *entry) :
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

    AbstractOverlay *mOverlay;
    int mIndex;
    AbstractOverlayEntry *mEntry;
};

class RemoveEntry : public BaseOverlayCommand
{
public:
    RemoveEntry(AbstractOverlayDialog *d, int index, AbstractOverlayEntry *entry) :
        BaseOverlayCommand(d, "Remove Room"),
        mOverlay(entry->parent()),
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

    AbstractOverlay *mOverlay;
    int mIndex;
    AbstractOverlayEntry *mEntry;
};

} // namespace anon

/////

AbstractOverlayDialog::AbstractOverlayDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ContainerOverlayDialog),
    mZoomable(new Zoomable),
    mCurrentTileset(nullptr),
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
    connect(ui->overlayView->model(), QOverload<AbstractOverlay*,const QString&>::of(&ContainerOverlayModel::tileDropped),
            this, QOverload<AbstractOverlay*,const QString&>::of(&AbstractOverlayDialog::tileDropped));
    connect(ui->overlayView->model(), QOverload<AbstractOverlayEntry*,int,const QString&>::of(&ContainerOverlayModel::tileDropped),
            this, QOverload<AbstractOverlayEntry*,int,const QString&>::of(&AbstractOverlayDialog::tileDropped));
    connect(ui->overlayView->model(), QOverload<AbstractOverlayEntry*,const QString&>::of(&ContainerOverlayModel::entryRoomNameEdited),
            this, QOverload<AbstractOverlayEntry*,const QString&>::of(&AbstractOverlayDialog::entryRoomNameEdited));
    connect(ui->overlayView, &ContainerOverlayView::removeTile, this, &AbstractOverlayDialog::removeTile);

    // FIXME: this base class shouldn't know anything about TileOverlayEntry
    connect(ui->overlayView->model(), QOverload<AbstractOverlayEntry*,int>::of(&ContainerOverlayModel::entryChanceEdited),
            this, QOverload<AbstractOverlayEntry*,int>::of(&AbstractOverlayDialog::entryChanceEdited));

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
//    ui->listWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    connect(ui->tilesetList, SIGNAL(itemSelectionChanged()),
            SLOT(tilesetSelectionChanged()));
    connect(ui->tilesetMgr, SIGNAL(clicked(bool)), SLOT(manageTilesets()));

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

AbstractOverlayDialog::~AbstractOverlayDialog()
{
    delete ui;
}

QString AbstractOverlayDialog::setBaseTile(AbstractOverlay *overlay, const QString &tileName)
{
    QString old = overlay->tileName();
    overlay->setTileName(tileName);
    ui->overlayView->redisplay(overlay);
    updateUsedTiles();
    return old;
}

void AbstractOverlayDialog::addEntryTile(AbstractOverlayEntry *entry, int index, const QString &tileName)
{
    entry->tiles().insert(index, tileName);
    ui->overlayView->redisplay(entry);
    updateUsedTiles();
}

void AbstractOverlayDialog::removeEntryTile(AbstractOverlayEntry *entry, int index)
{
    entry->tiles().removeAt(index);
    ui->overlayView->redisplay(entry);
    updateUsedTiles();
}

QString AbstractOverlayDialog::setEntryTile(AbstractOverlayEntry *entry, int index, const QString &tileName)
{
    QString old = entry->tiles()[index];
    entry->tiles()[index] = tileName;
    ui->overlayView->redisplay(entry);
    updateUsedTiles();
    return old;
}

QString AbstractOverlayDialog::setEntryRoomName(AbstractOverlayEntry *entry, const QString &roomName)
{
    QString old = entry->roomName();
    entry->setRoomName(roomName);
    ui->overlayView->redisplay(entry);
    return old;
}

// FIXME: this base class shouldn't know anything about TileOverlayEntry
int AbstractOverlayDialog::setEntryChance(AbstractOverlayEntry *entry, int chance)
{
    int old = 1;
    if (TileOverlayEntry *toe = dynamic_cast<TileOverlayEntry*>(entry)) {
        old = toe->mChance;
        toe->mChance = chance;
        ui->overlayView->redisplay(entry);
    }
    return old;
}

void AbstractOverlayDialog::insertOverlay(int index, AbstractOverlay *overlay)
{
    mOverlays.insert(index, overlay);
    ui->overlayView->model()->insertOverlay(index, overlay);
    ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(overlay));
    updateUsedTiles();
}

void AbstractOverlayDialog::removeOverlay(int index)
{
    AbstractOverlay *overlay = mOverlays.takeAt(index);
    ui->overlayView->model()->removeOverlay(overlay);
    updateUsedTiles();
}

void AbstractOverlayDialog::insertEntry(AbstractOverlay *overlay, int index, AbstractOverlayEntry *entry)
{
    overlay->insertEntry(index, entry);
    ui->overlayView->model()->insertEntry(overlay, index, entry);
    ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(entry));
    updateUsedTiles();
}

void AbstractOverlayDialog::removeEntry(AbstractOverlay *overlay, int index)
{
    overlay->removeEntry(index);
    ui->overlayView->model()->removeEntry(overlay, index);
    updateUsedTiles();
}

void AbstractOverlayDialog::setTilesetList()
{
    ui->tilesetList->clear();
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    for (Tileset *tileset : TileMetaInfoMgr::instance()->tilesets()) {
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

bool AbstractOverlayDialog::isOverlayTileUsed(const QString &_tileName)
{
    QString tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(_tileName);
    for (AbstractOverlay *overlay : mOverlays) {
        if (overlay->tileName() == tileName) {
            return true;
        }
    }
    return false;
}

bool AbstractOverlayDialog::isEntryTileUsed(const QString &_tileName)
{
    QString tileName = BuildingEditor::BuildingTilesMgr::normalizeTileName(_tileName);
    for (AbstractOverlay *overlay : mOverlays) {
        for (int i = 0; i < overlay->entryCount(); i++) {
            AbstractOverlayEntry *entry = overlay->entry(i);
            if (entry->tiles().contains(tileName)) {
                return true;
            }
        }
    }
    return false;
}

void AbstractOverlayDialog::addOverlay()
{
    QModelIndex index = ui->tilesetTilesView->currentIndex();
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (!tile)
        return;
    AbstractOverlay *overlay = createOverlay(tile);

    QMap<QString, AbstractOverlay*> map;
    for (AbstractOverlay *overlay : mOverlays) {
        map[overlay->tileName()] = overlay;
    }
    map[overlay->tileName()] = overlay;
    QList<AbstractOverlay*> sorted = map.values();
    int index2 = sorted.indexOf(overlay);

    mUndoStack->push(new InsertOverlay(this, index2, overlay));
}

void AbstractOverlayDialog::addEntry()
{
    QModelIndex index = ui->overlayView->currentIndex();
    AbstractOverlay *overlay = ui->overlayView->model()->overlayAt(index);
    if (!overlay)
        return;
    AbstractOverlayEntry *entry = createEntry(overlay);
    mUndoStack->push(new InsertEntry(this, overlay, overlay->entryCount(), entry));
}

void AbstractOverlayDialog::setToNone()
{
    QModelIndex index = ui->overlayView->currentIndex();
    AbstractOverlayEntry *entry = ui->overlayView->model()->entryAt(index);
    if (!entry)
        return;
    QList<QUndoCommand*> commands;
    for (int i = 0; i < entry->tiles().size(); i++) {
        if (entry->tiles()[i] != QLatin1String("none")) {
            commands += new SetEntryTile(this, entry, i, QLatin1String("none"));
        }
    }
    if (commands.size() > 1) {
        mUndoStack->beginMacro(QLatin1String("Set To None"));
    }
    for (QUndoCommand *uc : commands) {
        mUndoStack->push(uc);
    }
    if (commands.size() > 1) {
        mUndoStack->endMacro();
    }
}

void AbstractOverlayDialog::overlayActivated(const QModelIndex &index)
{
    AbstractOverlay *overlay = ui->overlayView->model()->overlayAt(index);
    AbstractOverlayEntry *entry = ui->overlayView->model()->entryAt(index);
    if (overlay) {
        if (Tiled::Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(overlay->tileName())) {
            int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset()->name());
            ui->tilesetList->setCurrentRow(row);
            QModelIndex tileIndex = ui->tilesetTilesView->model()->index(tile);
            ui->tilesetTilesView->setCurrentIndex(tileIndex);
            QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                                      Q_ARG(QModelIndex, tileIndex));
        }
    }
    if (entry) {
        if (Tiled::Tile *tile = BuildingEditor::BuildingTilesMgr::instance()->tileFor(entry->tiles().first())) {
            int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset()->name());
            ui->tilesetList->setCurrentRow(row);
            QModelIndex tileIndex = ui->tilesetTilesView->model()->index(tile);
            ui->tilesetTilesView->setCurrentIndex(tileIndex);
            QMetaObject::invokeMethod(this, "scrollToNow", Qt::QueuedConnection,
                                      Q_ARG(QModelIndex, tileIndex));
        }
    }
}

void AbstractOverlayDialog::scrollToNow(const QModelIndex &index)
{
    ui->tilesetTilesView->scrollTo(index);
}

void AbstractOverlayDialog::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == nullptr)
        return;

    for (AbstractOverlay *overlay : mOverlays) {
        if (BuildingEditor::BuildingTilesMgr::normalizeTileName(overlay->tileName()) == BuildingEditor::BuildingTilesMgr::nameForTile(tile)) {
            ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(overlay));
            return;
        }

        for (int i = 0; i < overlay->entryCount(); i++) {
            AbstractOverlayEntry *entry = overlay->entry(i);
            for (QString tileName : entry->tiles()) {
                if (BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName) == BuildingEditor::BuildingTilesMgr::nameForTile(tile)) {
                    ui->overlayView->setCurrentIndex(ui->overlayView->model()->index(entry));
                    return;
                }
            }
        }
    }
}

void AbstractOverlayDialog::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : nullptr;
    mCurrentTileset = nullptr;
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

void AbstractOverlayDialog::updateUsedTiles()
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

void AbstractOverlayDialog::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt())
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
}

void AbstractOverlayDialog::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

void AbstractOverlayDialog::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void AbstractOverlayDialog::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
#if 0
    categoryChanged(ui->categoryList->currentRow());
#endif
}

// Called when a tileset image changes or a missing tileset was found.
void AbstractOverlayDialog::tilesetChanged(Tileset *tileset)
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

void AbstractOverlayDialog::tileDropped(AbstractOverlay *overlay, const QString &tileName)
{
    if (isOverlayTileUsed(tileName))
        return;
    mUndoStack->push(new SetBaseTile(this, overlay, tileName));
}

void AbstractOverlayDialog::tileDropped(AbstractOverlayEntry *entry, int index, const QString &tileName)
{
    if (index >= entry->tiles().size()) {
        if (ui->overlayView->model()->moreThan2Tiles()) {
            mUndoStack->push(new AddEntryTile(this, entry, tileName));
        }
        return;
    }
    mUndoStack->push(new SetEntryTile(this, entry, index, tileName));
}

void AbstractOverlayDialog::entryRoomNameEdited(AbstractOverlayEntry *entry, const QString &roomName)
{
    mUndoStack->push(new SetEntryRoomName(this, entry, roomName));
}

// FIXME: this base class shouldn't know anything about TileOverlayEntry
void AbstractOverlayDialog::entryChanceEdited(AbstractOverlayEntry *entry, int chance)
{
    mUndoStack->push(new SetEntryChance(this, entry, chance));
}

void AbstractOverlayDialog::removeTile(AbstractOverlayEntry *entry, int index)
{
    mUndoStack->push(new RemoveEntryTile(this, entry, index));
}

void AbstractOverlayDialog::fileOpen()
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

void AbstractOverlayDialog::fileOpen(const QString &fileName)
{
    if (!fileOpen(fileName, mOverlays)) {
        QMessageBox::warning(this, tr("Error reading file"), mError);
        return;
    }

    mUndoStack->clear();
    mFileName = fileName;
    ui->overlayView->setOverlays(mOverlays);
    updateUsedTiles();

    syncUI();
}

void AbstractOverlayDialog::closeEvent(QCloseEvent *event)
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

bool AbstractOverlayDialog::confirmSave()
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

QString AbstractOverlayDialog::getSaveLocation()
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

bool AbstractOverlayDialog::fileSave()
{
    if (mFileName.length())
        return fileSave(mFileName);
    else
        return fileSaveAs();
}

bool AbstractOverlayDialog::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

bool AbstractOverlayDialog::fileSave(const QString &fileName)
{
    if (!fileSave(fileName, mOverlays)) {
        QMessageBox::warning(this, tr("Error writing file"), mError);
        return false;
    }
    mFileName = fileName;
    mUndoStack->setClean();
    syncUI();
    return true;
}

void AbstractOverlayDialog::updateWindowTitle()
{
    if (mFileName.length()) {
        QString fileName = QDir::toNativeSeparators(mFileName);
        setWindowTitle(tr("[*]%1").arg(fileName));
    } else {
        setWindowTitle(defaultWindowTitle());
    }
    setWindowModified(!mUndoStack->isClean());
}

void AbstractOverlayDialog::remove()
{
    QModelIndexList selected = ui->overlayView->selectionModel()->selectedIndexes();
    AbstractOverlay *overlay = ui->overlayView->model()->overlayAt(selected.first());
    AbstractOverlayEntry *entry = ui->overlayView->model()->entryAt(selected.first());
    if (overlay) {
        mUndoStack->push(new RemoveOverlay(this, mOverlays.indexOf(overlay), overlay));
    }
    if (entry) {
        mUndoStack->push(new RemoveEntry(this, entry->indexOf(), entry));
    }
}

void AbstractOverlayDialog::syncUI()
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

// // // // //

ContainerOverlayDialog::ContainerOverlayDialog(QWidget *parent)
    : AbstractOverlayDialog(parent)
{

}

bool ContainerOverlayDialog::fileOpen(const QString &fileName, QList<AbstractOverlay *> &overlays)
{
    ContainerOverlayFile cof;
    if (!cof.read(fileName)) {
        mError = cof.errorString();
        return false;
    }

    overlays.clear();
    for (ContainerOverlay* overlay : cof.takeOverlays()) {
        overlays << overlay;
    }
    return true;
}

bool ContainerOverlayDialog::fileSave(const QString &fileName, const QList<AbstractOverlay *> &overlays)
{
    ContainerOverlayFile cof;
    QList<ContainerOverlay*> overlays1;
    for (AbstractOverlay* overlay : overlays) {
        overlays1 << static_cast<ContainerOverlay*>(overlay);
    }
    if (!cof.write(fileName, overlays1)) {
        mError = cof.errorString();
        return false;
    }
    return true;
}

AbstractOverlay *ContainerOverlayDialog::createOverlay(Tile *tile)
{
    ContainerOverlay *overlay = new ContainerOverlay;
    overlay->mTileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
    overlay->insertEntry(overlay->entryCount(), createEntry(overlay));
    return overlay;
}

AbstractOverlayEntry *ContainerOverlayDialog::createEntry(AbstractOverlay *overlay)
{
    ContainerOverlayEntry *entry = new ContainerOverlayEntry();
    entry->mParent = static_cast<ContainerOverlay*>(overlay);
    entry->mRoomName = QLatin1String("other");
    entry->mTiles += QLatin1String("none");
    entry->mTiles += QLatin1String("none");
    return entry;
}
