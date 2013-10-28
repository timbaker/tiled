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

#include "addtexturesdialog.h"
#include "addvirtualtilesetdialog.h"
#include "preferences.h"
#include "texturemanager.h"
#include "texturepropertiesdialog.h"
#include "tileshapeeditor.h"
#include "tileshapegroupdialog.h"
#include "undoredobuttons.h"
#include "virtualtileset.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QClipboard>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
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
    ChangeVTile(VirtualTilesetDialog *d, VirtualTile *vtile, const VirtualTile *other) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Virtual Tile")),
        mDialog(d),
        mTile(vtile),
        mTile2(other)
    {
    }

    void undo()
    {
        VirtualTilesetMgr::instance().changeVTile(mTile, mTile2);
    }

    void redo()
    {
        VirtualTilesetMgr::instance().changeVTile(mTile, mTile2);
    }

    VirtualTilesetDialog *mDialog;
    VirtualTile *mTile;
    VirtualTile mTile2;
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
                                                               vtile->shape());
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

class AddTexture : public QUndoCommand
{
public:
    AddTexture(VirtualTilesetDialog *d, TextureInfo *tex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Texture")),
        mDialog(d),
        mTexture(tex)
    {
    }

    void undo()
    {
        mDialog->removeTexture(mTexture);
    }

    void redo()
    {
        mDialog->addTexture(mTexture);
    }

    VirtualTilesetDialog *mDialog;
    TextureInfo *mTexture;
};

class RemoveTexture : public QUndoCommand
{
public:
    RemoveTexture(VirtualTilesetDialog *d, TextureInfo *tex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Texture")),
        mDialog(d),
        mTexture(tex)
    {
    }

    void undo()
    {
        mDialog->addTexture(mTexture);
    }

    void redo()
    {
        mDialog->removeTexture(mTexture);
    }

    VirtualTilesetDialog *mDialog;
    TextureInfo *mTexture;
};

class ChangeTexture : public QUndoCommand
{
public:
    ChangeTexture(VirtualTilesetDialog *d, TextureInfo *tex, int tileWidth, int tileHeight) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Texture")),
        mDialog(d),
        mTexture(tex),
        mTileWidth(tileWidth),
        mTileHeight(tileHeight)
    {
    }

    void undo()
    {
        mDialog->changeTexture(mTexture, mTileWidth, mTileHeight);
    }

    void redo()
    {
        mDialog->changeTexture(mTexture, mTileWidth, mTileHeight);
    }

    VirtualTilesetDialog *mDialog;
    TextureInfo *mTexture;
    int mTileWidth;
    int mTileHeight;
};

class EditShape : public QUndoCommand
{
public:
    EditShape(VirtualTilesetDialog *d, TileShape *shape, const TileShape *other) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Edit Shape")),
        mDialog(d),
        mShape(shape),
        mShape2(other)
    {
    }

    void undo()
    {
        mDialog->editShape(mShape, mShape2);
    }

    void redo()
    {
        mDialog->editShape(mShape, mShape2);
    }

    VirtualTilesetDialog *mDialog;
    TileShape *mShape;
    TileShape mShape2;
};

} // namespace

SINGLETON_IMPL(VirtualTilesetDialog)

VirtualTilesetDialog::VirtualTilesetDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::VirtualTilesetDialog),
    mCurrentVirtualTileset(0),
    mCurrentTexture(0),
    mTextureTileset(0),
    mIsoTileset(0),
    mShapeGroup(0),
    mUngroupedGroup(0),
    mShowDiskImage(false),
    mFile(0),
    mShapesEdited(false),
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
    toolBar->addAction(ui->actionTilesetImageToClipboard);
    ui->vTileToolbarLayout->insertWidget(0, toolBar, 1);

    toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddTexture);
    toolBar->addAction(ui->actionRemoveTexture);
    toolBar->addAction(ui->actionTextureProperties);
    ui->textureToolbarLayout->insertWidget(0, toolBar);

    UndoRedoButtons *urb = new UndoRedoButtons(mUndoStack, this);
    ui->buttonsLayout->insertWidget(0, urb->redoButton());
    ui->buttonsLayout->insertWidget(0, urb->undoButton());

//    ui->orthoTiles->autoSwitchLayerChanged(false);
    ui->orthoTiles->setModel(new TilesetModel(0, ui->orthoTiles));

    ui->vTilesetTiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->vTilesetTiles->setDragEnabled(true);
    ui->vTilesetTiles->setDragDropMode(QAbstractItemView::DragDrop);

    ui->orthoTiles->setShowLayerNames(false);

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

    if (mUngroupedGroup = VirtualTilesetMgr::instance().ungroupedGroup())
        ui->comboBox->addItem(mUngroupedGroup->label());
    ui->comboBox->addItems(VirtualTilesetMgr::instance().shapeGroupLabels());
    if (ui->comboBox->count()) {
        ui->comboBox->setCurrentIndex(0);
        mShapeGroup = mUngroupedGroup ? mUngroupedGroup : VirtualTilesetMgr::instance().shapeGroupAt(0);
    }
    connect(ui->comboBox, SIGNAL(currentIndexChanged(int)), SLOT(shapeGroupChanged(int)));
    connect(ui->editGroups, SIGNAL(clicked()), SLOT(editGroups()));

    connect(ui->vTilesetNames->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(virtualTilesetNameSelected()));
    connect(ui->vTilesetNames, SIGNAL(activated(QModelIndex)), SLOT(editVTileset(QModelIndex)));
    connect(ui->vTilesetTiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateActions()));
    connect(ui->vTilesetTiles, SIGNAL(activated(QModelIndex)), SLOT(vTileActivated(QModelIndex)));
    connect(ui->orthoFiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(textureNameSelectionChanged()));
    connect(ui->orthoFiles, SIGNAL(activated(QModelIndex)), SLOT(textureProperties()));
    connect(ui->orthoTiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(textureTileSelectionChanged()));

    connect(ui->vTilesetTiles->model(), SIGNAL(beginDropTiles()), SLOT(beginDropTiles()));
    connect(ui->vTilesetTiles->model(), SIGNAL(endDropTiles()), SLOT(endDropTiles()));
    connect(ui->vTilesetTiles->model(), SIGNAL(tileDropped(VirtualTile*,QString,int,int,TileShape*)),
            SLOT(tileDropped(VirtualTile*,QString,int,int,TileShape*)));

    connect(ui->isoTiles, SIGNAL(activated(QModelIndex)), SLOT(editShape(QModelIndex)));

    connect(ui->actionAddTileset, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionRemoveTileset, SIGNAL(triggered()), SLOT(removeTileset()));
    connect(ui->actionClearVTiles, SIGNAL(triggered()), SLOT(clearVTiles()));
    connect(ui->actionShowDiskImage, SIGNAL(toggled(bool)), SLOT(showDiskImage(bool)));
    connect(ui->actionTilesetImageToClipboard, SIGNAL(triggered()), SLOT(copyToClipboard()));
    connect(ui->actionAddTexture, SIGNAL(triggered()), SLOT(addTexture()));
    connect(ui->actionRemoveTexture, SIGNAL(triggered()), SLOT(removeTexture()));
    connect(ui->actionTextureProperties, SIGNAL(triggered()), SLOT(textureProperties()));

    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetAdded(VirtualTileset*)),
            SLOT(tilesetAdded(VirtualTileset*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetRemoved(VirtualTileset*)),
            SLOT(tilesetRemoved(VirtualTileset*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetChanged(VirtualTileset*)),
            SLOT(tilesetChanged(VirtualTileset*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeChanged(TileShape*)),
            SLOT(shapeChanged(TileShape*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeAssigned(TileShapeGroup*,int,int)),
            SLOT(shapeAssigned(TileShapeGroup*,int,int)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupAdded(int,TileShapeGroup*)),
            SLOT(shapeGroupAdded(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupRemoved(int,TileShapeGroup*)),
            SLOT(shapeGroupRemoved(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupChanged(TileShapeGroup*)),
            SLOT(shapeGroupChanged(TileShapeGroup*)));

    connect(TextureMgr::instancePtr(), SIGNAL(textureAdded(TextureInfo*)),
            SLOT(textureAdded(TextureInfo*)));
    connect(TextureMgr::instancePtr(), SIGNAL(textureRemoved(TextureInfo*)),
            SLOT(textureRemoved(TextureInfo*)));
    connect(TextureMgr::instancePtr(), SIGNAL(textureChanged(TextureInfo*)),
            SLOT(textureChanged(TextureInfo*)));


    setVirtualTilesetNamesList();
    setTextureNamesList();

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

void VirtualTilesetDialog::setTextureNamesList()
{
    QString name;
    if (ui->orthoFiles->currentRow() != -1)
        name = ui->orthoFiles->item(ui->orthoFiles->currentRow())->text();

    ui->orthoFiles->clear();
    foreach (TextureInfo *tex, TextureMgr::instance().textures()) {
        ui->orthoFiles->addItem(tex->name());
        if (tex->name() == name)
            ui->orthoFiles->setCurrentRow(ui->orthoFiles->count() - 1);
    }
}

void VirtualTilesetDialog::setTextureTilesList()
{
    ui->orthoTiles->selectionModel()->clear();
    ui->orthoTiles->tilesetModel()->setTileset(mTextureTileset);
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

void VirtualTilesetDialog::addTexture(TextureInfo *tex)
{
    TextureMgr::instance().addTexture(tex);
}

void VirtualTilesetDialog::removeTexture(TextureInfo *tex)
{
    TextureMgr::instance().removeTexture(tex);
}

void VirtualTilesetDialog::changeTexture(TextureInfo *tex, int &tileWidth, int &tileHeight)
{
    int oldTileWidth = tex->tileWidth(), oldTileHeight = tex->tileHeight();
    TextureMgr::instance().changeTexture(tex, tileWidth, tileHeight);
    tileWidth = oldTileWidth;
    tileHeight = oldTileHeight;

    textureAdded(tex); // !!!

    setTextureTilesList();
}

void VirtualTilesetDialog::editShape(TileShape *shape, TileShape &other)
{
    VirtualTilesetMgr::instance().changeShape(shape, other);

    textureTileSelectionChanged();
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

void VirtualTilesetDialog::vTileActivated(const QModelIndex &index)
{
    if (VirtualTile *vtile = ui->vTilesetTiles->model()->tileAt(index)) {
        QString texName = vtile->imageSource();
        if (TextureInfo *tex = TextureMgr::instance().texture(texName)) {
            int row = TextureMgr::instance().textures().indexOf(tex);
            ui->orthoFiles->setCurrentRow(row);
            ui->orthoTiles->setCurrentIndex(ui->orthoTiles->model()->index(vtile->srcY(), vtile->srcX()));
        }
    }
}

void VirtualTilesetDialog::clearVTiles()
{
    QModelIndexList selected = ui->vTilesetTiles->selectionModel()->selectedIndexes();
    if (selected.size() > 1)
        mUndoStack->beginMacro(tr("Clear Virtual Tiles"));
    foreach (QModelIndex index, selected) {
        if (VirtualTile *vtile = ui->vTilesetTiles->model()->tileAt(index)) {
            VirtualTile changed(0, -1, -1);
            mUndoStack->push(new ChangeVTile(this, vtile, &changed));
        }
    }
    if (selected.size() > 1)
        mUndoStack->endMacro();
}

void VirtualTilesetDialog::showDiskImage(bool show)
{
    ui->vTilesetTiles->model()->setShowDiskImage(show);
}

void VirtualTilesetDialog::copyToClipboard()
{
    if (mCurrentVirtualTileset)
        qApp->clipboard()->setImage(mCurrentVirtualTileset->image());
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

void VirtualTilesetDialog::tilesetChanged(VirtualTileset *vts)
{
    if (mCurrentVirtualTileset == vts)
        ui->vTilesetTiles->model()->redisplay();
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

void VirtualTilesetDialog::textureNameSelectionChanged()
{
    QModelIndexList selected = ui->orthoFiles->selectionModel()->selectedIndexes();
    mCurrentTexture = 0;
    mTextureTileset = 0;
    if (!selected.isEmpty()) {
        int row = selected.first().row();
        QString name = ui->orthoFiles->item(row)->text();
        if (mCurrentTexture = TextureMgr::instance().texture(name))
            mTextureTileset = TextureMgr::instance().tileset(mCurrentTexture);
    }
    setTextureTilesList();
    updateActions();
}

void VirtualTilesetDialog::textureTileSelectionChanged()
{
    QModelIndexList selected = ui->orthoTiles->selectionModel()->selectedIndexes();
    VirtualTileset *oldTileset = mIsoTileset;
    if (mIsoTileset) {
        mIsoTileset = 0;
    }

    mTextureTileImage = QImage();
    if (!selected.isEmpty() && mShapeGroup != 0) {
        Tile *textureTile = ui->orthoTiles->tilesetModel()->tileAt(selected.first());
        mTextureTileImage = textureTile->image();
        // If there's only one tile in a group (such as Floors), then create a
        // virtual tile for each selected texture tile.
        if (mShapeGroup->count() == 1) {
            QRect r(selected.first().column(), selected.first().row(), 1, 1);
            foreach (QModelIndex index, selected)
                r |= QRect(index.column(), index.row(), 1, 1);
            mIsoTileset = new VirtualTileset(mShapeGroup->label(), r.width(), r.height());
            if (TileShape *shape = mShapeGroup->shapeAt(0)) {
                foreach (QModelIndex index, selected) {
                    Tile *tile = ui->orthoTiles->tilesetModel()->tileAt(index);
                    int x = index.column() - r.x(), y = index.row() - r.y();
                    mIsoTileset->tileAt(x, y)->setImageSource(tile->tileset()->name(),
                                                              tile->id() % tile->tileset()->columnCount(),
                                                              tile->id() / tile->tileset()->columnCount());
                    mIsoTileset->tileAt(x, y)->setShape(shape);
                }
            }
        } else {
            mIsoTileset = new VirtualTileset(mShapeGroup->label(),
                                             mShapeGroup->columnCount(),
                                             mShapeGroup->rowCount());
            for (int i = 0; i < mIsoTileset->tileCount(); i++) {
                if (TileShape *shape = mShapeGroup->shapeAt(i)) {
                    mIsoTileset->tileAt(i)->setImageSource(textureTile->tileset()->name(),
                                                           textureTile->id() % textureTile->tileset()->columnCount(),
                                                           textureTile->id() / textureTile->tileset()->columnCount());
                    mIsoTileset->tileAt(i)->setShape(shape);
                }
            }
        }
    }

    ui->isoTiles->setTileset(mIsoTileset);
    delete oldTileset;
}

void VirtualTilesetDialog::addTexture()
{
    QStringList ignore;
    foreach (TextureInfo *tex, TextureMgr::instance().textures()) {
        QString fileName = tex->fileName();
        if (QDir::isRelativePath(fileName))
            fileName = QDir(Preferences::instance()->texturesDirectory()).filePath(tex->fileName());
        ignore += fileName;
    }
    AddTexturesDialog dialog(Preferences::instance()->texturesDirectory(),
                             ignore, true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    int tileWidth = dialog.tileWidth();
    int tileHeight = dialog.tileHeight();

    mUndoStack->beginMacro(tr("Add Textures"));

    foreach (QString fileName, dialog.fileNames()) {
        QFileInfo info(fileName);
        QString name = info.completeBaseName();
        if (TextureInfo *old = TextureMgr::instance().texture(name))
            mUndoStack->push(new RemoveTexture(this, old));
        QImageReader reader(fileName);
        QSize imgSize = reader.size();
        if (imgSize.isValid() && !(imgSize.width() % tileWidth) && !(imgSize.height() % tileHeight)) {
            TextureInfo *tex = new TextureInfo(name, fileName,
                                               imgSize.width() / tileWidth,
                                               imgSize.height() / tileHeight,
                                               tileWidth, tileHeight);
            mUndoStack->push(new AddTexture(this, tex));
        } else {
            if (!imgSize.isValid()) {
                QMessageBox::warning(this, tr("Bad Texture"), tr("The texture file '%1' couldn't be read.")
                                     .arg(info.fileName()));
                break;
            } else {
                QMessageBox::warning(this, tr("Bad Texture"), tr("The texture file '%1' isn't an even number of tiles wide or tall.\nImage size: %2,%3\nTile size: %4,%5")
                                     .arg(info.fileName())
                                     .arg(imgSize.width()).arg(imgSize.height())
                                     .arg(tileWidth).arg(tileHeight));
                break;
            }
        }
    }

    mUndoStack->endMacro();
}

void VirtualTilesetDialog::removeTexture()
{
    if (mCurrentTexture) {
        if (QMessageBox::question(this, tr("Remove Texture"),
                                  tr("Really remove the texture '%1'?")
                                  .arg(mCurrentTexture->name()),
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
            return;
        mUndoStack->push(new RemoveTexture(this, mCurrentTexture));
    }
}

void VirtualTilesetDialog::textureProperties()
{
    TexturePropertiesDialog d(mCurrentTexture, this);
    if (d.exec() != QDialog::Accepted)
        return;

    mUndoStack->push(new ChangeTexture(this, mCurrentTexture, d.tileWidth(), d.tileHeight()));
}

void VirtualTilesetDialog::textureAdded(TextureInfo *tex)
{
#if 0
    foreach (VirtualTileset *vts, mTilesets) {
        bool changed = false;
        foreach (VirtualTile *vtile, vts->tiles()) {
            if (vtile->imageSource() == tex->name()) {
                vtile->setImage(QImage());
                // not calling vtile->setImageSource
                changed = true;
            }
        }
        if (changed) {
            vts->tileChanged();
            VirtualTilesetMgr::instance().emitTilesetChanged(vts);
            if (vts == mCurrentVirtualTileset)
                ui->vTilesetTiles->model()->redisplay();
        }
    }
#endif
    setTextureNamesList();
}

void VirtualTilesetDialog::textureRemoved(TextureInfo *tex)
{
#if 0
    foreach (VirtualTileset *vts, mTilesets) {
        bool changed = false;
        foreach (VirtualTile *vtile, vts->tiles()) {
            if (vtile->imageSource() == tex->name()) {
                vtile->setImage(QImage());
                // not calling vtile->setImageSource
                changed = true;
            }
        }
        if (changed) {
            vts->tileChanged();
            VirtualTilesetMgr::instance().emitTilesetChanged(vts);
            if (vts == mCurrentVirtualTileset)
                ui->vTilesetTiles->model()->redisplay();
        }
    }
#endif

    setTextureNamesList();
}

void VirtualTilesetDialog::textureChanged(TextureInfo *tex)
{
    if (tex == mCurrentTexture) {
        // Either the texture image changed, or the tile size changed causing
        // the tileset to be recreated.
        // FIXME: TextureMgr might have destroyed the tileset that is being displayed by us
        if (tex->tileset() != mTextureTileset) {
            mTextureTileset = tex->tileset();
            ui->orthoTiles->tilesetModel()->setTileset(tex->tileset());
        } else
            ui->orthoTiles->tilesetModel()->tilesetChanged();
    }
}

void VirtualTilesetDialog::shapeGroupAdded(int index, TileShapeGroup *g)
{
    ui->comboBox->insertItem(index + 1, g->label());
}

void VirtualTilesetDialog::shapeGroupRemoved(int index, TileShapeGroup *g)
{
    Q_UNUSED(g)
    ui->comboBox->removeItem(index + 1);
}

void VirtualTilesetDialog::shapeGroupChanged(int row)
{
    if (mUngroupedGroup && (row == 0))
        mShapeGroup = mUngroupedGroup;
    else
        mShapeGroup = VirtualTilesetMgr::instance().shapeGroupAt(row - 1);
    textureTileSelectionChanged();
}

void VirtualTilesetDialog::editGroups()
{
    TileShapeGroupDialog d(this);
    d.exec();
    if (d.needsSaving())
        mShapesEdited = true;
}

void VirtualTilesetDialog::shapeGroupChanged(TileShapeGroup *group)
{
    if (group == mShapeGroup)
        textureTileSelectionChanged();
}

void VirtualTilesetDialog::shapeAssigned(TileShapeGroup *g, int col, int row)
{
    if (mShapeGroup == g) {
        VirtualTile *vtile = mIsoTileset->tileAt(col, row);
        vtile->setShape(g->shapeAt(col, row));
        vtile->setImage(QImage());
        ui->isoTiles->model()->redisplay(vtile);
    }
}

void VirtualTilesetDialog::shapeChanged(TileShape *shape)
{
    if (mShapeGroup && mShapeGroup->hasShape(shape))
        textureTileSelectionChanged();
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
                                       int srcX, int srcY, TileShape *shape)
{
    VirtualTile changed(0, -1, -1, imageSource, srcX, srcY, shape);
    mUndoStack->push(new ChangeVTile(this, vtile, &changed));
}

void VirtualTilesetDialog::editShape(const QModelIndex &index)
{
    TileShapeGroupDialog d(this);
    d.setTexture(mTextureTileImage,
                 mIsoTileset->tileAt(index.column(), index.row())->imageSource(),
                 mIsoTileset->tileAt(index.column(), index.row())->srcX(),
                 mIsoTileset->tileAt(index.column(), index.row())->srcY());
    d.selectGroupAndShape(mShapeGroup, index.column(), index.row());
    d.exec();
    if (d.needsSaving())
        mShapesEdited = true;
    return; //// CONFLICTS WITH TileShapeGroupDialog UNDO STACK

    if (VirtualTile *vtile = ui->isoTiles->model()->tileAt(index)) {
        if (TileShape *shape = vtile->shape()) {
//            if (shape->mSameAs)
//                shape = shape->mSameAs;
            TileShapeEditor dialog(shape, mTextureTileImage, this);
            if (dialog.exec() == QDialog::Accepted) {
                TileShape *shape2 = dialog.tileShape();
                foreach (TileShapeFace e, shape2->mFaces) {
                    Q_ASSERT(e.mGeom.size() == e.mUV.size());
                }

                mUndoStack->push(new EditShape(this, shape, shape2));
#if 0
                shape->mElements = shape2->mElements;
                vtile->setImage(QImage());
                vtile->tileset()->tileChanged();

                foreach (VirtualTileset *vts, VirtualTilesetMgr::instance().tilesets()) {
                    bool changed = false;
                    foreach (VirtualTile *vtile2, vts->tiles()) {
                        if (vtile2->shape() == vtile->shape()) {
                            vtile2->setImage(QImage());
                            changed = true;
                        }
                    }
                    if (changed) {
                        vts->tileChanged();
                        VirtualTilesetMgr::instance().emitTilesetChanged(vts);
                    }
                }
#endif
            }
        }
    }
}

void VirtualTilesetDialog::writeShapes()
{
}

void VirtualTilesetDialog::updateActions()
{
    ui->actionRemoveTileset->setEnabled(ui->vTilesetNames->selectedItems().size() != 0);
    ui->actionClearVTiles->setEnabled(ui->vTilesetTiles->selectionModel()->selectedIndexes().size() != 0);
//    ui->actionShowDiskImage->setEnabled(ui->vTilesetTiles->model()->diskImageValid());
    ui->actionTilesetImageToClipboard->setEnabled(mCurrentVirtualTileset != 0);
    ui->actionRemoveTexture->setEnabled(ui->orthoFiles->selectionModel()->selectedIndexes().size() != 0);
    ui->actionTextureProperties->setEnabled(ui->orthoFiles->selectionModel()->selectedIndexes().size() != 0);
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

void VirtualTilesetDialog::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        QSettings settings;
        settings.beginGroup(QLatin1String("VirtualTilesetDialog"));
        settings.setValue(QLatin1String("geometry"), saveGeometry());
        settings.setValue(QLatin1String("TileScale"), ui->vTilesetTiles->zoomable()->scale());
        settings.endGroup();
        saveSplitterSizes(ui->splitter);

        if (!mUndoStack->isClean() || mShapesEdited) {
            if (!TextureMgr::instance().writeTxt())
                QMessageBox::warning(this, tr("Error!"), TextureMgr::instance().errorString()
                                     + tr("during TextureMgr::writeTxt()"));
            if (!VirtualTilesetMgr::instance().writeTxt())
                QMessageBox::warning(this, tr("Error!"), VirtualTilesetMgr::instance().errorString()
                                     + tr("during VirtualTilesetMgr::writeTxt()"));
            mUndoStack->setClean();
            mShapesEdited = false;
        }

        event->accept();
    } else
        event->ignore();
}

void VirtualTilesetDialog::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("VirtualTilesetDialog/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .vts file"),
                                                    lastPath,
                                                    QLatin1String("Virtual tileset files (*.vts)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    clearDocument();

    fileOpen(fileName);

//    initStringComboBoxValues();
//    updateTilesetListLater();
//    updateUI();
}

void VirtualTilesetDialog::fileOpen(const QString &fileName)
{
    VirtualTilesetsFile *vtsFile = new VirtualTilesetsFile;
    if (!vtsFile->read(fileName)) {
        QMessageBox::warning(this, tr("Error reading .vts file"),
                             vtsFile->errorString());
        delete vtsFile;
        return;
    }

    mFile = vtsFile;
}

bool VirtualTilesetDialog::confirmSave()
{
    if (!mFile || mUndoStack->isClean())
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

QString VirtualTilesetDialog::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("VirtualTilesetDialog/LastOpenPath");
    QString suggestedFileName = Preferences::instance()->tilesDirectory() +
            QLatin1String("/virtualtilesets.vts");
    if (!mFile) {
        //
    } else if (mFile->fileName().isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty())
            suggestedFileName = lastPath + QLatin1String("/virtualtilesets.vts");
    } else {
        suggestedFileName = mFile->fileName();
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Virtual tileset files (*.vts)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    return fileName;
}

bool VirtualTilesetDialog::fileSave()
{
    if (mFile->fileName().length())
        return fileSave(mFile->fileName());
    else
        return fileSaveAs();
}

bool VirtualTilesetDialog::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

TileShape *VirtualTilesetDialog::shape(const char *name)
{
    Q_ASSERT(VirtualTilesetMgr::instance().tileShape(QLatin1String(name)) != 0);
    return VirtualTilesetMgr::instance().tileShape(QLatin1String(name));
}

bool VirtualTilesetDialog::fileSave(const QString &fileName)
{
    if (!mFile)
        return false;

    if (!mFile->write(fileName)) {
        QMessageBox::warning(this, tr("Error writing .vts file"),
                             mFile->errorString());
        return false;
    }

    mFile->setFileName(fileName);
    mUndoStack->setClean();

//    updateWindowTitle();

    return true;
}

void VirtualTilesetDialog::clearDocument()
{
    mUndoStack->clear();

    mTilesets.clear();

    delete mFile;
    mFile = 0;

    ui->vTilesetNames->clear();
}
