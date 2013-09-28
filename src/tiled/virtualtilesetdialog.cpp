#include "virtualtilesetdialog.h"
#include "ui_virtualtilesetdialog.h"

#include "addvirtualtilesetdialog.h"
#include "undoredobuttons.h"
#include "virtualtileset.h"
#include "zoomable.h"

#include "tile.h"
#include "tileset.h"

#include <QFileInfo>
#include <QMessageBox>
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

} // namespace

VirtualTilesetDialog::VirtualTilesetDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VirtualTilesetDialog),
    mCurrentVirtualTileset(0),
    mOrthoTileset(0),
    mIsoTileset(0),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    UndoRedoButtons *urb = new UndoRedoButtons(mUndoStack, this);
    ui->buttonsLayout->insertWidget(0, urb->redoButton());
    ui->buttonsLayout->insertWidget(0, urb->undoButton());

//    ui->orthoTiles->autoSwitchLayerChanged(false);
    ui->orthoTiles->setModel(new TilesetModel(0, ui->orthoTiles));

    ui->vTilesetTiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->vTilesetTiles->setDragEnabled(true);
    ui->vTilesetTiles->setDragDropMode(QAbstractItemView::DragDrop);

    delete ui->isoTiles->zoomable();
    ui->isoTiles->setZoomable(ui->vTilesetTiles->zoomable());
    ui->isoTiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->isoTiles->setDragEnabled(true);
    ui->isoTiles->setDragDropMode(QAbstractItemView::DragOnly);

    ui->vTilesetTiles->zoomable()->connectToComboBox(ui->scaleCombo);

    connect(ui->vTilesetNames->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(virtualTilesetNameSelected()));
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

    connect(ui->addTileset, SIGNAL(clicked()), SLOT(addTileset()));
    connect(ui->removeTileset, SIGNAL(clicked()), SLOT(removeTileset()));
    connect(ui->clearVTiles, SIGNAL(clicked()), SLOT(clearVTiles()));

    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetAdded(VirtualTileset*)),
            SLOT(tilesetAdded(VirtualTileset*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(tilesetRemoved(VirtualTileset*)),
            SLOT(tilesetRemoved(VirtualTileset*)));

    setVirtualTilesetNamesList();
    setOrthoFilesList();

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

void VirtualTilesetDialog::changeVTile(VirtualTile *vtile, QString &imageSource,
                                       int &srcX, int &srcY, int &isoType)
{
    QString mOldImgSrc = vtile->imageSource();
    int oldSrcX = vtile->srcX();
    int oldSrcY = vtile->srcY();
    int oldType = vtile->type();

    vtile->setImageSource(imageSource, srcX, srcY);
    vtile->setType(static_cast<VirtualTile::IsoType>(isoType));
    vtile->setImage(VirtualTilesetMgr::instance().renderIsoTile(vtile));

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
                                 tr("There is already a tileset with that name"));
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
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
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
    if (!selected.isEmpty()) {
        Tile *tile = ui->orthoTiles->tilesetModel()->tileAt(selected.first());
        mIsoTileset = new VirtualTileset(QLatin1String("Dynamic"), 4, 2);
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

        for (int i = 0; i < mIsoTileset->tileCount(); i++) {
            VirtualTile *vtile = mIsoTileset->tileAt(i);
            if (vtile->type() != VirtualTile::Invalid)
                vtile->setImage(VirtualTilesetMgr::instance().renderIsoTile(vtile));
        }
    }
    ui->isoTiles->setTileset(mIsoTileset);
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

void VirtualTilesetDialog::updateActions()
{
    ui->removeTileset->setEnabled(ui->vTilesetNames->selectedItems().size() != 0);
    ui->clearVTiles->setEnabled(ui->vTilesetTiles->selectionModel()->selectedIndexes().size() != 0);
}
