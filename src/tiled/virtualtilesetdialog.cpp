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

#include "virtualtilesetdialog.h"
#include "ui_virtualtilesetdialog.h"

#include "addvirtualtilesetdialog.h"
#include "undoredobuttons.h"
#include "virtualtileset.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

/////

namespace {

class AddTileset : public QUndoCommand
{
public:
    AddTileset(VirtualTilesetDialog *d, VirtualTileset *vts) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tileset")),
        mDialog(d),
        mTileset(vts)
    {
    }

    void undo()
    {
        mDialog->removeTileset(mTileset);
    }

    void redo()
    {
        mDialog->addTileset(mTileset);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTileset *mTileset;
};

class RemoveTileset : public QUndoCommand
{
public:
    RemoveTileset(VirtualTilesetDialog *d, VirtualTileset *vts) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tileset")),
        mDialog(d),
        mTileset(vts)
    {
    }

    void undo()
    {
        mDialog->addTileset(mTileset);
    }

    void redo()
    {
        mDialog->removeTileset(mTileset);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTileset *mTileset;
};

class ChangeVTile : public QUndoCommand
{
public:
    ChangeVTile(VirtualTilesetDialog *d, VirtualTile *vtile, const QString &imageSource,
                int srcX, int srcY, int isoType) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Virtual Tile")),
        mDialog(d),
        mTile(vtile),
        mImageSource(imageSource),
        mSrcX(srcX),
        mSrcY(srcY),
        mIsoType(isoType)
    {
    }

    void undo()
    {
        mDialog->changeVTile(mTile, mImageSource, mSrcX, mSrcY, mIsoType);
    }

    void redo()
    {
        mDialog->changeVTile(mTile, mImageSource, mSrcX, mSrcY, mIsoType);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTile *mTile;
    QString mImageSource;
    int mSrcX;
    int mSrcY;
    int mIsoType;
};

class RenameVTileset : public QUndoCommand
{
public:
    RenameVTileset(VirtualTilesetDialog *d, VirtualTileset *vts, const QString &name) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Rename Tileset")),
        mDialog(d),
        mTileset(vts),
        mName(name)
    {
    }

    void undo()
    {
        mName = mDialog->renameTileset(mTileset, mName);
    }

    void redo()
    {
        mName = mDialog->renameTileset(mTileset, mName);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTileset *mTileset;
    QString mName;
};

class ResizeVTileset : public QUndoCommand
{
public:
    ResizeVTileset(VirtualTilesetDialog *d, VirtualTileset *vts, const QSize &size) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Resize Tileset")),
        mDialog(d),
        mTileset(vts),
        mSize(size),
        mTiles(size.width() * size.height())
    {
        QRect a(0, 0, mTileset->columnCount(), mTileset->rowCount());
        QRect b(0, 0, size.width(), size.height());
        QRect c = a & b;
        for (int y = 0; y <= c.bottom(); y++) {
            for (int x = 0; x <= c.right(); x++) {
                VirtualTile *vtile = mTileset->tileAt(x, y);
                mTiles[y * size.width() + x] = new VirtualTile(mTileset, x, y, vtile->imageSource(),
                                                               vtile->srcX(), vtile->srcY(),
                                                               vtile->type());
            }
        }
        for (int i = 0; i < mTiles.size(); i++)
            if (!mTiles[i])
                mTiles[i] = new VirtualTile(mTileset, i % size.width(), i / size.width());
    }

    ~ResizeVTileset()
    {
        qDeleteAll(mTiles);
    }

    void undo()
    {
        mDialog->resizeTileset(mTileset, mSize, mTiles);
    }

    void redo()
    {
        mDialog->resizeTileset(mTileset, mSize, mTiles);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTileset *mTileset;
    QSize mSize;
    QVector<VirtualTile*> mTiles;
};

} // namespace

VirtualTilesetDialog::VirtualTilesetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VirtualTilesetDialog),
    mCurrentVirtualTileset(0),
    mOrthoTileset(0),
    mIsoTileset(0),
    mIsoCategory(CategoryWall),
    mShowDiskImage(false),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    QToolBar *toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddTileset);
    toolBar->addAction(ui->actionRemoveTileset);
    ui->vTilesetToolbarLayout->insertWidget(0, toolBar);

    toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionClearVTiles);
    toolBar->addAction(ui->actionShowDiskImage);
    ui->vTileToolbarLayout->insertWidget(0, toolBar, 1);

    UndoRedoButtons *urb = new UndoRedoButtons(mUndoStack, this);
    ui->buttonsLayout->insertWidget(0, urb->redoButton());
    ui->buttonsLayout->insertWidget(0, urb->undoButton());

//    ui->orthoTiles->autoSwitchLayerChanged(false);
    ui->orthoTiles->setModel(new TilesetModel(0, ui->orthoTiles));

    ui->vTilesetTiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->vTilesetTiles->setDragEnabled(true);
    ui->vTilesetTiles->setDragDropMode(QAbstractItemView::DragDrop);

    Zoomable *zoomable = ui->vTilesetTiles->zoomable();
    delete ui->isoTiles->zoomable();
    ui->isoTiles->setZoomable(zoomable);
    ui->isoTiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->isoTiles->setDragEnabled(true);
    ui->isoTiles->setDragDropMode(QAbstractItemView::DragOnly);

    zoomable->setZoomFactors(zoomable->zoomFactors().mid(2));
    QSettings settings;
    qreal scale = settings.value(QLatin1String("VirtualTilesetDialog/TileScale"),
                                 0.5).toReal();
    zoomable->setScale(scale);
    zoomable->connectToComboBox(ui->scaleCombo);

    ui->comboBox->addItem(QLatin1String("Floor"));
    ui->comboBox->addItem(QLatin1String("Roofs"));
    ui->comboBox->addItem(QLatin1String("Walls"));
    ui->comboBox->setCurrentIndex(mIsoCategory);
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), SLOT(isoCategoryChanged(int)));

    connect(ui->vTilesetNames->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(virtualTilesetNameSelected()));
    connect(ui->vTilesetNames, SIGNAL(activated(QModelIndex)), SLOT(editVTileset(QModelIndex)));
    connect(ui->vTilesetTiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateActions()));
    connect(ui->orthoFiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(orthoFileSelectionChanged()));
    connect(ui->orthoTiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(orthoTileSelectionChanged()));

    connect(ui->vTilesetTiles->model(), SIGNAL(beginDropTiles()), SLOT(beginDropTiles()));
    connect(ui->vTilesetTiles->model(), SIGNAL(endDropTiles()), SLOT(endDropTiles()));
    connect(ui->vTilesetTiles->model(), SIGNAL(tileDropped(VirtualTile*,QString,int,int,int)),
            SLOT(tileDropped(VirtualTile*,QString,int,int,int)));

    connect(ui->isoTiles, SIGNAL(activated(QModelIndex)), SLOT(editShape(QModelIndex)));

    connect(ui->actionAddTileset, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionRemoveTileset, SIGNAL(triggered()), SLOT(removeTileset()));
    connect(ui->actionClearVTiles, SIGNAL(triggered()), SLOT(clearVTiles()));
    connect(ui->actionShowDiskImage, SIGNAL(toggled(bool)), SLOT(showDiskImage(bool)));

    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetAdded(VirtualTileset*)),
            SLOT(tilesetAdded(VirtualTileset*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetRemoved(VirtualTileset*)),
            SLOT(tilesetRemoved(VirtualTileset*)));

    setVirtualTilesetNamesList();
    setOrthoFilesList();

    settings.beginGroup(QLatin1String("VirtualTilesetDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    mShowDiskImage = settings.value(QLatin1String("ShowDiskImage"), mShowDiskImage).toBool();
    settings.endGroup();
    restoreSplitterSizes(ui->splitter);

    updateActions();
}

VirtualTilesetDialog::~VirtualTilesetDialog()
{
    delete ui;
}

void VirtualTilesetDialog::setVirtualTilesetNamesList()
{
    ui->vTilesetNames->clear();

    mTilesets = VirtualTilesetMgr::instance().tilesets();
    foreach (VirtualTileset *vts, mTilesets) {
        ui->vTilesetNames->addItem(vts->name());
    }
}

void VirtualTilesetDialog::setVirtualTilesetTilesList()
{
    ui->vTilesetTiles->setTileset(mCurrentVirtualTileset);

    QImage diskImage;
    if (mCurrentVirtualTileset) {
        QString fileName = VirtualTilesetMgr::instance().imageSource(mCurrentVirtualTileset);
        if (QImageReader(fileName).size().isValid())
            diskImage = QImage(fileName);
    }
    ui->vTilesetTiles->model()->setDiskImage(diskImage);
}

void VirtualTilesetDialog::setOrthoFilesList()
{
    ui->orthoFiles->clear();
    mOrthoFiles.clear();
    mOrthoFiles << QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\Tiles\\Textures\\tex_walls_exterior_house_01.png");
    foreach (QString file, mOrthoFiles)
        ui->orthoFiles->addItem(QFileInfo(file).fileName());
}

void VirtualTilesetDialog::setOrthoTilesList()
{
    ui->orthoTiles->tilesetModel()->setTileset(mOrthoTileset);
}

void VirtualTilesetDialog::addTileset(VirtualTileset *vts)
{
    VirtualTilesetMgr::instance().addTileset(vts);
}

void VirtualTilesetDialog::removeTileset(VirtualTileset *vts)
{
    VirtualTilesetMgr::instance().removeTileset(vts);
}

QString VirtualTilesetDialog::renameTileset(VirtualTileset *vts, const QString &name)
{
    QString old = vts->name();
    VirtualTilesetMgr::instance().renameTileset(vts, name);
    return old;
}

void VirtualTilesetDialog::resizeTileset(VirtualTileset *vts, QSize &size,
                                         QVector<VirtualTile*> &tiles)
{
    QSize oldSize(vts->columnCount(), vts->rowCount());
    VirtualTilesetMgr::instance().resizeTileset(vts, size, tiles);
    size = oldSize;
}

void VirtualTilesetDialog::changeVTile(VirtualTile *vtile, QString &imageSource,
                                       int &srcX, int &srcY, int &isoType)
{
    QString mOldImgSrc = vtile->imageSource();
    int oldSrcX = vtile->srcX();
    int oldSrcY = vtile->srcY();
    int oldType = vtile->type();

    vtile->setImageSource(imageSource, srcX, srcY);
    vtile->setType(static_cast<VirtualTile::IsoType>(isoType));
    vtile->setImage(QImage());

    vtile->tileset()->tileChanged();

    imageSource = mOldImgSrc;
    srcX = oldSrcX, srcY = oldSrcY;
    isoType = oldType;

    VirtualTilesetMgr::instance().emitTilesetChanged(vtile->tileset());

    ui->vTilesetTiles->model()->redisplay(vtile);
}

void VirtualTilesetDialog::addTileset()
{
    AddVirtualTilesetDialog d(this);
    if (d.exec() != QDialog::Accepted)
        return;

    if (VirtualTilesetMgr::instance().tileset(d.name()) != 0) {
        QMessageBox::information(this, tr("Add Tileset"),
                                 tr("There is already a tileset called '%1'.").arg(d.name()));
        return;
    }

    VirtualTileset *vts = new VirtualTileset(d.name(), d.columnCount(), d.rowCount());
    mUndoStack->push(new AddTileset(this, vts));
}

void VirtualTilesetDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->vTilesetNames->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->vTilesetNames->row(item);
        VirtualTileset *vts = mTilesets.at(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?")
                                  .arg(vts->name()),
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
            return;
        mUndoStack->push(new RemoveTileset(this, vts));
    }
}

void VirtualTilesetDialog::clearVTiles()
{
    QModelIndexList selected = ui->vTilesetTiles->selectionModel()->selectedIndexes();
    if (selected.size() > 1)
        mUndoStack->beginMacro(tr("Clear Virtual Tiles"));
    foreach (QModelIndex index, selected) {
        if (VirtualTile *vtile = ui->vTilesetTiles->model()->tileAt(index))
            mUndoStack->push(new ChangeVTile(this, vtile, QString(), -1, -1, VirtualTile::Invalid));
    }
    if (selected.size() > 1)
        mUndoStack->endMacro();
}

void VirtualTilesetDialog::showDiskImage(bool show)
{
    ui->vTilesetTiles->model()->setShowDiskImage(show);
}

void VirtualTilesetDialog::tilesetAdded(VirtualTileset *vts)
{
    setVirtualTilesetNamesList();
    int row = mTilesets.indexOf(vts);
    ui->vTilesetNames->setCurrentRow(row);
}

void VirtualTilesetDialog::tilesetRemoved(VirtualTileset *vts)
{
    Q_UNUSED(vts)
    setVirtualTilesetNamesList();
}

void VirtualTilesetDialog::virtualTilesetNameSelected()
{
    QModelIndexList selected = ui->vTilesetNames->selectionModel()->selectedIndexes();
    int row = selected.size() ? selected.first().row() : -1;
    mCurrentVirtualTileset = (row >= 0) ? mTilesets.at(row) : 0;
    setVirtualTilesetTilesList();
    updateActions();
}

void VirtualTilesetDialog::editVTileset(const QModelIndex &index)
{
    VirtualTileset *vts = mTilesets.at(index.row());
    AddVirtualTilesetDialog dialog(vts->name(),
                                   vts->columnCount(),
                                   vts->rowCount());
    if (dialog.exec() != QDialog::Accepted)
        return;

    if (dialog.name() != vts->name()) {
        if (VirtualTilesetMgr::instance().tileset(dialog.name()) != 0) {
            QMessageBox::information(this, tr("Name Already Used"),
                                     tr("There is already a tileset called '%1'.")
                                     .arg(dialog.name()));
            return;
        }
        mUndoStack->push(new RenameVTileset(this, vts, dialog.name()));
    }

    if (dialog.columnCount() != vts->columnCount()) {
        QString prompt = tr("Changing the number of columns in a tileset will "
                            "break savegame compatibility and building tile "
                            "assignments.\n\nReally resize the tileset '%1'?")
                .arg(vts->name());
        if (QMessageBox::question(this, tr("Resize Tileset"), prompt,
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
            return;
    }
    if (dialog.columnCount() != vts->columnCount() ||
            dialog.rowCount() != vts->rowCount()) {
        mUndoStack->push(new ResizeVTileset(this, vts,
                                            QSize(dialog.columnCount(),
                                                  dialog.rowCount())));
    }
}

void VirtualTilesetDialog::orthoFileSelectionChanged()
{
    QModelIndexList selected = ui->orthoFiles->selectionModel()->selectedIndexes();
    if (mOrthoTileset) {
        delete mOrthoTileset;
        mOrthoTileset = 0;
    }
    if (!selected.isEmpty()) {
        int row = selected.first().row();
        mOrthoTileset = new Tileset(mOrthoFiles.at(row), 32, 96);
        mOrthoTileset->loadFromImage(QImage(mOrthoFiles.at(row)), mOrthoFiles.at(row));
    }
    setOrthoTilesList();
}

void VirtualTilesetDialog::orthoTileSelectionChanged()
{
    QModelIndexList selected = ui->orthoTiles->selectionModel()->selectedIndexes();
    if (mIsoTileset) {
        delete mIsoTileset;
        mIsoTileset = 0;
    }
    if (!selected.isEmpty() && mIsoCategory == CategoryFloor) {
        Tile *tile = ui->orthoTiles->tilesetModel()->tileAt(selected.first());
        mIsoTileset = new VirtualTileset(QLatin1String("Dynamic"), 1, 1);
        for (int i = 0; i < mIsoTileset->tileCount(); i++)
            mIsoTileset->tileAt(i)->setImageSource(tile->tileset()->imageSource(),
                                                   tile->id() % tile->tileset()->columnCount(),
                                                   tile->id() / tile->tileset()->columnCount());
        mIsoTileset->tileAt(0, 0)->setType(VirtualTile::Floor);
    }
    if (!selected.isEmpty() && mIsoCategory == CategoryRoof) {
        Tile *tile = ui->orthoTiles->tilesetModel()->tileAt(selected.first());
        mIsoTileset = new VirtualTileset(QLatin1String("Dynamic"), 8, 8);
        for (int i = 0; i < mIsoTileset->tileCount(); i++)
            mIsoTileset->tileAt(i)->setImageSource(tile->tileset()->imageSource(),
                                                   tile->id() % tile->tileset()->columnCount(),
                                                   tile->id() / tile->tileset()->columnCount());
        mIsoTileset->tileAt(0, 0)->setType(VirtualTile::SlopeS1);
        mIsoTileset->tileAt(1, 0)->setType(VirtualTile::SlopeS2);
        mIsoTileset->tileAt(2, 0)->setType(VirtualTile::SlopeS3);
        mIsoTileset->tileAt(3, 0)->setType(VirtualTile::SlopeE3);
        mIsoTileset->tileAt(4, 0)->setType(VirtualTile::SlopeE2);
        mIsoTileset->tileAt(5, 0)->setType(VirtualTile::SlopeE1);

        mIsoTileset->tileAt(0, 1)->setType(VirtualTile::SlopePt5S);
        mIsoTileset->tileAt(1, 1)->setType(VirtualTile::SlopeOnePt5S);
        mIsoTileset->tileAt(2, 1)->setType(VirtualTile::SlopeTwoPt5S);
        mIsoTileset->tileAt(3, 1)->setType(VirtualTile::SlopeTwoPt5E);
        mIsoTileset->tileAt(4, 1)->setType(VirtualTile::SlopeOnePt5E);
        mIsoTileset->tileAt(5, 1)->setType(VirtualTile::SlopePt5E);

        mIsoTileset->tileAt(0, 2)->setType(VirtualTile::Outer1);
        mIsoTileset->tileAt(1, 2)->setType(VirtualTile::Outer2);
        mIsoTileset->tileAt(2, 2)->setType(VirtualTile::Outer3);
        mIsoTileset->tileAt(3, 2)->setType(VirtualTile::Inner1);
        mIsoTileset->tileAt(4, 2)->setType(VirtualTile::Inner2);
        mIsoTileset->tileAt(5, 2)->setType(VirtualTile::Inner3);

        mIsoTileset->tileAt(0, 3)->setType(VirtualTile::OuterPt5);
        mIsoTileset->tileAt(1, 3)->setType(VirtualTile::OuterOnePt5);
        mIsoTileset->tileAt(2, 3)->setType(VirtualTile::OuterTwoPt5);
        mIsoTileset->tileAt(3, 3)->setType(VirtualTile::InnerPt5);
        mIsoTileset->tileAt(4, 3)->setType(VirtualTile::InnerOnePt5);
        mIsoTileset->tileAt(5, 3)->setType(VirtualTile::InnerTwoPt5);

        mIsoTileset->tileAt(0, 4)->setType(VirtualTile::CornerSW1);
        mIsoTileset->tileAt(1, 4)->setType(VirtualTile::CornerSW2);
        mIsoTileset->tileAt(2, 4)->setType(VirtualTile::CornerSW3);
        mIsoTileset->tileAt(3, 4)->setType(VirtualTile::CornerNE3);
        mIsoTileset->tileAt(4, 4)->setType(VirtualTile::CornerNE2);
        mIsoTileset->tileAt(5, 4)->setType(VirtualTile::CornerNE1);

        mIsoTileset->tileAt(0, 5)->setType(VirtualTile::RoofTopN1);
        mIsoTileset->tileAt(1, 5)->setType(VirtualTile::RoofTopW1);
        mIsoTileset->tileAt(2, 5)->setType(VirtualTile::RoofTopN2);
        mIsoTileset->tileAt(3, 5)->setType(VirtualTile::RoofTopW2);
        mIsoTileset->tileAt(4, 5)->setType(VirtualTile::RoofTopN3);
        mIsoTileset->tileAt(5, 5)->setType(VirtualTile::RoofTopW3);

        mIsoTileset->tileAt(0, 6)->setType(VirtualTile::ShallowSlopeE1);
        mIsoTileset->tileAt(1, 6)->setType(VirtualTile::ShallowSlopeE2);
        mIsoTileset->tileAt(2, 6)->setType(VirtualTile::ShallowSlopeW2);
        mIsoTileset->tileAt(3, 6)->setType(VirtualTile::ShallowSlopeW1);
        mIsoTileset->tileAt(4, 6)->setType(VirtualTile::ShallowSlopeN1);
        mIsoTileset->tileAt(5, 6)->setType(VirtualTile::ShallowSlopeN2);
        mIsoTileset->tileAt(6, 6)->setType(VirtualTile::ShallowSlopeS2);
        mIsoTileset->tileAt(7, 6)->setType(VirtualTile::ShallowSlopeS1);

        mIsoTileset->tileAt(0, 7)->setType(VirtualTile::TrimS);
        mIsoTileset->tileAt(1, 7)->setType(VirtualTile::TrimE);
        mIsoTileset->tileAt(2, 7)->setType(VirtualTile::TrimInner);
        mIsoTileset->tileAt(3, 7)->setType(VirtualTile::TrimOuter);
        mIsoTileset->tileAt(4, 7)->setType(VirtualTile::TrimCornerSW);
        mIsoTileset->tileAt(5, 7)->setType(VirtualTile::TrimCornerNE);
    }
    if (!selected.isEmpty() && mIsoCategory == CategoryWall) {
        Tile *tile = ui->orthoTiles->tilesetModel()->tileAt(selected.first());
        mIsoTileset = new VirtualTileset(QLatin1String("Dynamic"), 4, 3);
        for (int i = 0; i < mIsoTileset->tileCount(); i++)
            mIsoTileset->tileAt(i)->setImageSource(tile->tileset()->imageSource(),
                                                      tile->id() % tile->tileset()->columnCount(),
                                                      tile->id() / tile->tileset()->columnCount());
        mIsoTileset->tileAt(0, 0)->setType(VirtualTile::WallW);
        mIsoTileset->tileAt(1, 0)->setType(VirtualTile::WallN);
        mIsoTileset->tileAt(2, 0)->setType(VirtualTile::WallNW);
        mIsoTileset->tileAt(3, 0)->setType(VirtualTile::WallSE);
        mIsoTileset->tileAt(0, 1)->setType(VirtualTile::WallWindowW);
        mIsoTileset->tileAt(1, 1)->setType(VirtualTile::WallWindowN);
        mIsoTileset->tileAt(2, 1)->setType(VirtualTile::WallDoorW);
        mIsoTileset->tileAt(3, 1)->setType(VirtualTile::WallDoorN);
        mIsoTileset->tileAt(0, 2)->setType(VirtualTile::WallShortW);
        mIsoTileset->tileAt(1, 2)->setType(VirtualTile::WallShortN);
        mIsoTileset->tileAt(2, 2)->setType(VirtualTile::WallShortNW);
        mIsoTileset->tileAt(3, 2)->setType(VirtualTile::WallShortSE);
    }
    ui->isoTiles->setTileset(mIsoTileset);
}

void VirtualTilesetDialog::isoCategoryChanged(int row)
{
    mIsoCategory = static_cast<IsoCategory>(row);
    orthoTileSelectionChanged();
}

void VirtualTilesetDialog::beginDropTiles()
{
    mUndoStack->beginMacro(tr("Set Virtual Tiles"));
}

void VirtualTilesetDialog::endDropTiles()
{
    mUndoStack->endMacro();
}

void VirtualTilesetDialog::tileDropped(VirtualTile *vtile, const QString &imageSource,
                                       int srcX, int srcY, int isoType)
{
    mUndoStack->push(new ChangeVTile(this, vtile, imageSource, srcX, srcY, isoType));
}

#include "tileshapeeditor.h"
void VirtualTilesetDialog::editShape(const QModelIndex &index)
{
    if (VirtualTile *vtile = ui->isoTiles->model()->tileAt(index)) {
        if (TileShape *shape = VirtualTilesetMgr::instance().tileShape(vtile->type())) {
            TileShapeEditor dialog(shape, this);
            if (dialog.exec() == QDialog::Accepted) {
                TileShape *shape2 = dialog.tileShape();
                shape->mElements = shape2->mElements;
                vtile->setImage(QImage());
                vtile->tileset()->tileChanged();

                foreach (VirtualTileset *vts, VirtualTilesetMgr::instance().tilesets()) {
                    bool changed = false;
                    foreach (VirtualTile *vtile2, vts->tiles()) {
                        if (vtile2->type() == vtile->type()) {
                            vtile2->setImage(QImage());
                            changed = true;
                        }
                    }
                    if (changed) {
                        vts->tileChanged();
                        VirtualTilesetMgr::instance().emitTilesetChanged(vts);
                    }
                }
            }
        }
    }
}

void VirtualTilesetDialog::updateActions()
{
    ui->actionRemoveTileset->setEnabled(ui->vTilesetNames->selectedItems().size() != 0);
    ui->actionClearVTiles->setEnabled(ui->vTilesetTiles->selectionModel()->selectedIndexes().size() != 0);
//    ui->actionShowDiskImage->setEnabled(ui->vTilesetTiles->model()->diskImageValid());
}

void VirtualTilesetDialog::saveSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("VirtualTilesetDialog"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    settings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    settings.endGroup();
}

void VirtualTilesetDialog::restoreSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("VirtualTilesetDialog"));
    QVariant v = settings.value(tr("%1/sizes").arg(splitter->objectName()));
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
    settings.endGroup();
}

void VirtualTilesetDialog::done(int r)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("VirtualTilesetDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("TileScale"), ui->vTilesetTiles->zoomable()->scale());
    settings.endGroup();
    saveSplitterSizes(ui->splitter);

    if (!mUndoStack->isClean()) {
        VirtualTilesetMgr::instance().writeTxt();
        mUndoStack->setClean();
    }

    QDialog::done(r);
}
