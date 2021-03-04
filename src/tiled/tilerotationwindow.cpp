/*
 * tilerotationwindow.cpp
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

#include "tilerotationwindow.h"
#include "ui_tilerotationwindow.h"

#include "tilemetainfodialog.h"
#include "tilemetainfomgr.h"
#include "tilerotation.h"
#include "tilerotationfile.h"

#include "tileset.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "BuildingEditor/buildingtiles.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QUndoGroup>
#include <QUndoStack>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

// // // // //

class AddFurnitureTiles : public QUndoCommand
{
public:
    AddFurnitureTiles(TileRotationWindow *d, FurnitureGroup *group, int index, FurnitureTiles *ftiles) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Furniture Tiles")),
        mDialog(d),
        mGroup(group),
        mIndex(index),
        mTiles(ftiles)
    {
    }

    ~AddFurnitureTiles() override
    {
        delete mTiles;
    }

    void undo() override
    {
        mTiles = mDialog->removeFurnitureTiles(mGroup, mIndex);
    }

    void redo() override
    {
        mDialog->insertFurnitureTiles(mGroup, mIndex, mTiles);
        mTiles = nullptr;
    }

    TileRotationWindow *mDialog;
    FurnitureGroup *mGroup;
    int mIndex;
    FurnitureTiles *mTiles;
};

class ChangeFurnitureTile : public QUndoCommand
{
public:
    ChangeFurnitureTile(TileRotationWindow *d, FurnitureTile *ftile, int x, int y, const QString &tileName = QString()) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Furniture Tile")),
        mDialog(d),
        mTile(ftile),
        mX(x),
        mY(y),
        mTileName(tileName)
    {
    }

    void undo() override
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    void redo() override
    {
        mTileName = mDialog->changeFurnitureTile(mTile, mX, mY, mTileName);
    }

    TileRotationWindow *mDialog;
    FurnitureTile *mTile;
    int mX;
    int mY;
    QString mTileName;
};

class RemoveFurnitureTiles : public QUndoCommand
{
public:
    RemoveFurnitureTiles(TileRotationWindow *d, FurnitureGroup *group, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Furniture Tiles")),
        mDialog(d),
        mGroup(group),
        mIndex(index),
        mTiles(nullptr)
    {
    }

    void undo() override
    {
        mDialog->insertFurnitureTiles(mGroup, mIndex, mTiles);
        mTiles = nullptr;
    }

    void redo() override
    {
        mTiles = mDialog->removeFurnitureTiles(mGroup, mIndex);
    }

    TileRotationWindow *mDialog;
    FurnitureGroup *mGroup;
    int mIndex;
    FurnitureTiles *mTiles;
};

// // // // //

TileRotationWindow::TileRotationWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileRotationWindow),
    mZoomable(new Zoomable),
    mCurrentTileset(nullptr),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this)),
    mFurnitureGroup(new FurnitureGroup())
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

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

    connect(ui->actionNew, &QAction::triggered, this, &TileRotationWindow::fileNew);
    connect(ui->actionOpen, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileOpen));
    connect(ui->actionSave, &QAction::triggered, this, QOverload<>::of(&TileRotationWindow::fileSave));
    connect(ui->actionSaveAs, &QAction::triggered, this, &TileRotationWindow::fileSaveAs);
    connect(ui->actionClose, &QAction::triggered, this, &TileRotationWindow::close);
    connect(ui->actionAddTiles, &QAction::triggered, this, &TileRotationWindow::addTiles);
    connect(ui->actionClearTiles, &QAction::triggered, this, &TileRotationWindow::clearTiles);
    connect(ui->actionRemove, &QAction::triggered, this, &TileRotationWindow::removeTiles);

    ui->tilesetList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    connect(ui->tilesetList, &QListWidget::itemSelectionChanged, this, &TileRotationWindow::tilesetSelectionChanged);
    connect(ui->tilesetMgr, &QToolButton::clicked, this,&TileRotationWindow::manageTilesets);

    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAdded, this, &TileRotationWindow::tilesetAdded);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetAboutToBeRemoved, this, &TileRotationWindow::tilesetAboutToBeRemoved);
    connect(TileMetaInfoMgr::instance(), &TileMetaInfoMgr::tilesetRemoved, this, &TileRotationWindow::tilesetRemoved);

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged, this, &TileRotationWindow::tilesetChanged);

    ui->tilesetFilter->setClearButtonEnabled(true);
    ui->tilesetFilter->setEnabled(false);
    connect(ui->tilesetFilter, &QLineEdit::textEdited, this, &TileRotationWindow::filterEdited);

//    ui->tilesetTilesView->setZoomable(mZoomable);
    ui->tilesetTilesView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tilesetTilesView->setDragEnabled(true);
//    connect(ui->tilesetTilesView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->tilesetTilesView, &MixedTilesetView::activated, this, &TileRotationWindow::tileActivated);

    ui->furnitureView->setZoomable(mZoomable);
    ui->furnitureView->setAcceptDrops(true);
    ui->furnitureView->model()->setShowResolved(false); // show the grid
    connect(ui->furnitureView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TileRotationWindow::syncUI);
    connect(ui->furnitureView->model(), &FurnitureModel::furnitureTileDropped, this, &TileRotationWindow::furnitureTileDropped);
    connect(ui->furnitureView, &FurnitureView::activated, this, &TileRotationWindow::furnitureActivated);

    setTilesetList();
    syncUI();

    // FIXME: load from file
    mUndoStack->beginMacro(QLatin1Literal("TEST"));
    auto& furnitureTiles = TileRotation::instance()->furnitureTiles();
    for (auto* furnitureTiles : furnitureTiles) {
        FurnitureTiles* furnitureTiles1 = new FurnitureTiles(false);
        furnitureTiles1->setGroup(mFurnitureGroup);
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            FurnitureTile* furnitureTile1 = new FurnitureTile(furnitureTiles1, furnitureTile->orient());
            for (int dy = 0; dy < furnitureTile->height(); dy++) {
                for (int dx = 0; dx < furnitureTile->width(); dx++) {
                    if (BuildingTile* buildingTile = furnitureTile->tile(dx, dy)) {
                        furnitureTile1->setTile(dx, dy, buildingTile);
                    }
                }
            }
            furnitureTiles1->setTile(furnitureTile1);
        }
        mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup, mFurnitureGroup->mTiles.size(), furnitureTiles1));
//        mFurnitureGroup->mTiles += furnitureTiles1;
    }
    mUndoStack->endMacro();
    mFileName = QLatin1Literal("D:/pz/TileRotation.txt");
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);

}

TileRotationWindow::~TileRotationWindow()
{
    delete ui;
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    delete mFurnitureGroup;
}

void TileRotationWindow::fileNew()
{
    if (!confirmSave())
        return;

    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return;

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);

    syncUI();
}

void TileRotationWindow::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .txt file"),
                                                    lastPath,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    fileOpen(fileName);

    syncUI();
}

void TileRotationWindow::fileOpen(const QString &fileName)
{
    QList<FurnitureTiles*> furnitureTiles;
    QStringList noRotateFileNames;
    if (!fileOpen(fileName, furnitureTiles, noRotateFileNames)) {
        QMessageBox::warning(this, tr("Error reading file"), mError);
        return;
    }

    mUndoStack->clear();
    mFileName = fileName;
    qDeleteAll(mFurnitureGroup->mTiles);
    mFurnitureGroup->mTiles.clear();
    mFurnitureGroup->mTiles += furnitureTiles;
    mNoRotateTileNames = noRotateFileNames;
    ui->furnitureView->setTiles(mFurnitureGroup->mTiles);
    syncUI();
}

void TileRotationWindow::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        mFileName.clear();
        ui->furnitureView->clear();
        qDeleteAll(mFurnitureGroup->mTiles);
        mFurnitureGroup->mTiles.clear();
        mUndoStack->clear();
        syncUI();
        event->accept();
    } else {
        event->ignore();
    }
}

bool TileRotationWindow::confirmSave()
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

QString TileRotationWindow::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("ContainerOverlay/LastOpenPath");
    QString suggestedFileName = QLatin1String("TileRotation.txt");
    if (mFileName.isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty()) {
            suggestedFileName = lastPath + QLatin1String("/TileRotation.txt");
        }
    } else {
        suggestedFileName = mFileName;
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Text files (*.txt)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absoluteFilePath());

    return fileName;
}

bool TileRotationWindow::fileSave()
{
    if (mFileName.length())
        return fileSave(mFileName);
    return fileSaveAs();
}

bool TileRotationWindow::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

void TileRotationWindow::addTiles()
{
    int index = mFurnitureGroup->mTiles.size();
//    if (mCurrentFurniture)
//        index = mFurnitureGroup->mTiles.indexOf(mCurrentFurniture->owner()) + 1;
    FurnitureTiles *tiles = new FurnitureTiles(false);
    mUndoStack->push(new AddFurnitureTiles(this, mFurnitureGroup, index, tiles));
}

void TileRotationWindow::clearTiles()
{
    FurnitureView *v = ui->furnitureView;
    QList<FurnitureTile*> clear;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection) {
        FurnitureTile *tile = v->model()->tileAt(index);
        if (!tile->isEmpty())
            clear += tile;
    }
    if (clear.isEmpty())
        return;
    mUndoStack->beginMacro(tr("Clear Furniture Tiles"));
    for (FurnitureTile* ftile : clear) {
        for (int x = 0; x < ftile->width(); x++) {
            for (int y = 0; y < ftile->height(); y++) {
                if (ftile->tile(x, y)) {
                    mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y));
                }
            }
        }
    }
    mUndoStack->endMacro();
}

void TileRotationWindow::removeTiles()
{
    FurnitureView *v = ui->furnitureView;
    QList<FurnitureTiles*> remove;
    QModelIndexList selection = v->selectionModel()->selectedIndexes();
    for (QModelIndex& index : selection) {
        FurnitureTile *tile = v->model()->tileAt(index);
        if (!remove.contains(tile->owner())) {
            remove += tile->owner();
        }
    }
    if (remove.count() > 1) {
        mUndoStack->beginMacro(tr("Remove Furniture Tiles"));
    }
    for (FurnitureTiles *tiles : remove) {
        mUndoStack->push(new RemoveFurnitureTiles(this, tiles->group(), tiles->indexInGroup()));
    }
    if (remove.count() > 1) {
        mUndoStack->endMacro();
    }
}

bool TileRotationWindow::fileSave(const QString &fileName)
{
    if (!fileSave(fileName, mFurnitureGroup->mTiles, mNoRotateTileNames)) {
        QMessageBox::warning(this, tr("Error writing file"), mError);
        return false;
    }
    mFileName = fileName;
    mUndoStack->setClean();
    syncUI();
    return true;
}

bool TileRotationWindow::fileOpen(const QString &fileName, QList<FurnitureTiles *> &tiles, QStringList& noRotateTileNames)
{
    TileRotationFile file;
    if (!file.read(fileName)) {
        mError = file.errorString();
        return false;
    }
    tiles = file.takeTiles();
    noRotateTileNames = file.takeNoRotateTileNames();
    return true;
}

bool TileRotationWindow::fileSave(const QString &fileName, const QList<FurnitureTiles *> &tiles, const QStringList &noRotateTileNames)
{
    TileRotationFile file;
    if (!file.write(fileName, tiles, noRotateTileNames)) {
        mError = file.errorString();
        return false;
    }
    return true;
}

void TileRotationWindow::updateWindowTitle()
{
    if (mFileName.length()) {
        QString fileName = QDir::toNativeSeparators(mFileName);
        setWindowTitle(tr("[*]%1 - Tile Rotation").arg(fileName));
    } else {
        setWindowTitle(QLatin1Literal("Tile Rotation"));
    }
    setWindowModified(!mUndoStack->isClean());
}

void TileRotationWindow::syncUI()
{
    ui->actionSave->setEnabled(!mFileName.isEmpty() && !mUndoStack->isClean());
    ui->actionSaveAs->setEnabled(!mFileName.isEmpty());

    QModelIndexList selected = ui->furnitureView->selectionModel()->selectedIndexes();
    ui->actionClearTiles->setEnabled(selected.size() > 0);
    ui->actionRemove->setEnabled(selected.size() > 0);

    updateWindowTitle();
}

void TileRotationWindow::tileActivated(const QModelIndex &index)
{
    Tile *tile = ui->tilesetTilesView->model()->tileAt(index);
    if (tile == nullptr)
        return;
    QString tileName = BuildingTilesMgr::nameForTile(tile);
    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(tileName);
    for (FurnitureTiles* furnitureTiles : mFurnitureGroup->mTiles) {
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            if (furnitureTile->tiles().contains(buildingTile)) {
                ui->furnitureView->setCurrentIndex(ui->furnitureView->model()->index(furnitureTile));
                return;
            }
        }
    }
}

void TileRotationWindow::tilesetSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesetList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : nullptr;
    mCurrentTileset = nullptr;
    if (item) {
        int row = ui->tilesetList->row(item);
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
        if (mCurrentTileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(mCurrentTileset);
            updateUsedTiles();
        }
    } else {
        ui->tilesetTilesView->clear();
    }
    syncUI();
}

void TileRotationWindow::furnitureActivated(const QModelIndex &index)
{
    FurnitureModel *m = ui->furnitureView->model();
    if (FurnitureTile *ftile = m->tileAt(index)) {
        for (BuildingTile *btile : ftile->tiles()) {
            if (btile != nullptr) {
                displayTileInTileset(btile);
                break;
            }
        }
    }
}

void TileRotationWindow::displayTileInTileset(Tiled::Tile *tile)
{
    if (tile == nullptr)
        return;
    int row = TileMetaInfoMgr::instance()->indexOf(tile->tileset());
    if (row >= 0) {
        ui->tilesetList->setCurrentRow(row);
        ui->tilesetTilesView->setCurrentIndex(ui->tilesetTilesView->model()->index(tile));
    }
}

void TileRotationWindow::displayTileInTileset(BuildingTile *tile)
{
    displayTileInTileset(BuildingTilesMgr::instance()->tileFor(tile));
}

void TileRotationWindow::updateUsedTiles()
{
    if (mCurrentTileset == nullptr)
        return;

    for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
        QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(mCurrentTileset->name(), i);
        if (mHoverTileName.isEmpty() == false) {
            if (mHoverTileName == tileName) {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
            } else {
                ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
            }
            continue;
        }
        if (isTileUsed(tileName)) {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect(0, 0, 1, 1));
        } else {
            ui->tilesetTilesView->model()->setCategoryBounds(i, QRect());
        }
    }
    ui->tilesetTilesView->model()->redisplay();
}

void TileRotationWindow::setTilesetList()
{
    ui->tilesetList->clear();
    ui->tilesetFilter->setEnabled(!TileMetaInfoMgr::instance()->tilesets().isEmpty());
    // Add the list of tilesets, and resize it to fit
    int width = 64;
    QFontMetrics fm = ui->tilesetList->fontMetrics();
    for (Tileset *tileset : TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing()) {
            item->setForeground(Qt::red);
        }
        ui->tilesetList->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesetList->verticalScrollBar()->sizeHint().width();
    ui->tilesetList->setFixedWidth(width + 16 + sbw);
    ui->tilesetFilter->setFixedWidth(ui->tilesetList->width());
}

void TileRotationWindow::filterEdited(const QString &text)
{
    QListWidget* mTilesetNamesView = ui->tilesetList;

    for (int row = 0; row < mTilesetNamesView->count(); row++) {
        QListWidgetItem* item = mTilesetNamesView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = mTilesetNamesView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = mTilesetNamesView->row(current) - 1;
        while (row >= 0 && mTilesetNamesView->item(row)->isHidden()) {
            row--;
        }
        if (row >= 0) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = mTilesetNamesView->row(current) + 1;
        while (row < mTilesetNamesView->count() && mTilesetNamesView->item(row)->isHidden()) {
            row++;
        }
        if (row < mTilesetNamesView->count()) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // All items hidden
        mTilesetNamesView->setCurrentItem(nullptr);
    }

    current = mTilesetNamesView->currentItem();
    if (current != nullptr) {
        mTilesetNamesView->scrollToItem(current);
    }
}

bool TileRotationWindow::isTileUsed(const QString &_tileName)
{
    BuildingTile* buildingTile = BuildingTilesMgr::instance()->get(_tileName);
    if (buildingTile == nullptr)
        return false;
    for (FurnitureTiles* furnitureTiles : mFurnitureGroup->mTiles) {
        for (FurnitureTile* furnitureTile : furnitureTiles->tiles()) {
            if (furnitureTile->tiles().contains(buildingTile)) {
                return true;
            }
        }
    }
    return false;
}

void TileRotationWindow::manageTilesets()
{
    TileMetaInfoDialog dialog(this);
    dialog.exec();

    TileMetaInfoMgr *mgr = TileMetaInfoMgr::instance();
    if (!mgr->writeTxt()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"), mgr->errorString());
    }
}

void TileRotationWindow::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesetList->setCurrentRow(row);
}

void TileRotationWindow::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesetList->takeItem(row);
}

void TileRotationWindow::tilesetRemoved(Tileset *tileset)
{
    Q_UNUSED(tileset)
}

// Called when a tileset image changes or a missing tileset was found.
void TileRotationWindow::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        if (tileset->isMissing()) {
            ui->tilesetTilesView->clear();
        } else {
            ui->tilesetTilesView->setTileset(tileset);
        }
    }

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesetList->item(row)) {
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
    }
}

void TileRotationWindow::furnitureTileDropped(FurnitureTile *ftile, int x, int y, const QString &tileName)
{
    mUndoStack->push(new ChangeFurnitureTile(this, ftile, x, y, tileName));
}

void TileRotationWindow::insertFurnitureTiles(FurnitureGroup *group, int index, FurnitureTiles *ftiles)
{
    group->mTiles.insert(index, ftiles);
    ftiles->setGroup(group);
    ui->furnitureView->setTiles(group->mTiles);
    updateUsedTiles();
}

FurnitureTiles *TileRotationWindow::removeFurnitureTiles(FurnitureGroup *group, int index)
{
    FurnitureTiles *ftiles = group->mTiles.takeAt(index);
    ftiles->setGroup(nullptr);
    ui->furnitureView->model()->removeTiles(ftiles);
    updateUsedTiles();
    return ftiles;
}

QString TileRotationWindow::changeFurnitureTile(FurnitureTile *ftile, int x, int y, const QString &tileName)
{
    QString old = ftile->tile(x, y) ? ftile->tile(x, y)->name() : QString();
    QSize oldSize = ftile->size();
    BuildingTile *btile = tileName.isEmpty() ? nullptr : BuildingTilesMgr::instance()->get(tileName);
    if (btile && BuildingTilesMgr::instance()->tileFor(btile) && BuildingTilesMgr::instance()->tileFor(btile)->image().isNull()) {
        btile = nullptr;
    }
    ftile->setTile(x, y, btile);
    if (ftile->size() != oldSize) {
        ui->furnitureView->furnitureTileResized(ftile);
    }
    ui->furnitureView->update(ui->furnitureView->model()->index(ftile));
    updateUsedTiles();
    return old;
}
