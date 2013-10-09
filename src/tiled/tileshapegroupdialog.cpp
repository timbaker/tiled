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

#include "tileshapegroupdialog.h"
#include "ui_tileshapegroupdialog.h"

#include "tileshapeeditor.h"
#include "tileshapegrouppropertiesdialog.h"
#include "tileshapepropertiesdialog.h"
#include "undoredobuttons.h"
#include "virtualtileset.h"

#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>
#include <QUndoCommand>
#include <QUndoStack>

using namespace Tiled::Internal;

namespace {

class AddGroup : public QUndoCommand
{
public:
    AddGroup(TileShapeGroupDialog *d, int index, TileShapeGroup *g) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Group")),
        mDialog(d),
        mIndex(index),
        mGroup(g)
    {

    }

    ~AddGroup()
    {
        delete mGroup;
    }

    void undo()
    {
        mGroup = mDialog->removeGroup(mIndex);
    }

    void redo()
    {
        mDialog->insertGroup(mIndex, mGroup);
        mGroup = 0;
    }

    TileShapeGroupDialog *mDialog;
    int mIndex;
    TileShapeGroup *mGroup;
};

class RemoveGroup : public QUndoCommand
{
public:
    RemoveGroup(TileShapeGroupDialog *d, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Group")),
        mDialog(d),
        mIndex(index),
        mGroup(0)
    {

    }

    ~RemoveGroup()
    {
        delete mGroup;
    }

    void undo()
    {
        mDialog->insertGroup(mIndex, mGroup);
        mGroup = 0;
    }

    void redo()
    {
        mGroup = mDialog->removeGroup(mIndex);
    }

    TileShapeGroupDialog *mDialog;
    int mIndex;
    TileShapeGroup *mGroup;
};

class EditGroup : public QUndoCommand
{
public:
    EditGroup(TileShapeGroupDialog *d, TileShapeGroup *g, const TileShapeGroup *other) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Edit Group")),
        mDialog(d),
        mGroup(g),
        mGroup2(other)
    {
    }

    void undo()
    {
        mDialog->editGroup(mGroup, mGroup2);
    }

    void redo()
    {
        mDialog->editGroup(mGroup, mGroup2);
    }

    TileShapeGroupDialog *mDialog;
    TileShapeGroup *mGroup;
    TileShapeGroup mGroup2;
};

class AddShape : public QUndoCommand
{
public:
    AddShape(TileShapeGroupDialog *d, TileShape *shape) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Shape")),
        mDialog(d),
        mShape(shape),
        mAdded(false)
    {
    }

    ~AddShape()
    {
        if (!mAdded)
            delete mShape;
    }

    void undo()
    {
        mDialog->removeShape(mShape);
        mAdded = false;
    }

    void redo()
    {
        mDialog->addShape(mShape);
        mAdded = true;
    }

    TileShapeGroupDialog *mDialog;
    TileShape *mShape;
    bool mAdded;
};

class RemoveShape : public QUndoCommand
{
public:
    RemoveShape(TileShapeGroupDialog *d, TileShape *shape) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Shape")),
        mDialog(d),
        mShape(shape),
        mAdded(true)
    {
    }

    ~RemoveShape()
    {
        if (!mAdded)
            delete mShape;
    }

    void undo()
    {
        mDialog->addShape(mShape);
        mAdded = true;
    }

    void redo()
    {
        mDialog->removeShape(mShape);
        mAdded = false;
    }

    TileShapeGroupDialog *mDialog;
    TileShape *mShape;
    bool mAdded;
};

class AssignShape : public QUndoCommand
{
public:
    AssignShape(TileShapeGroupDialog *d, TileShapeGroup *g, int col, int row, TileShape *shape) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Assign Shape To Group")),
        mDialog(d),
        mGroup(g),
        mColumn(col),
        mRow(row),
        mShape(shape)
    {
    }

    void undo()
    {
        mShape = mDialog->assignShape(mGroup, mColumn, mRow, mShape);
    }

    void redo()
    {
        mShape = mDialog->assignShape(mGroup, mColumn, mRow, mShape);
    }

    TileShapeGroupDialog *mDialog;
    TileShapeGroup *mGroup;
    int mColumn;
    int mRow;
    TileShape *mShape;
};

class ChangeShape : public QUndoCommand
{
public:
    ChangeShape(TileShapeGroupDialog *d, TileShape *shape, const TileShape *other) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Shape")),
        mDialog(d),
        mShape(shape),
        mShape2(other)
    {
    }

    void undo()
    {
        VirtualTilesetMgr::instance().changeShape(mShape, mShape2);
    }

    void redo()
    {
        VirtualTilesetMgr::instance().changeShape(mShape, mShape2);
    }

    TileShapeGroupDialog *mDialog;
    TileShape *mShape;
    TileShape mShape2;
};

class ChangeVTile : public QUndoCommand
{
public:
    ChangeVTile(TileShapeGroupDialog *d, VirtualTile *vtile, const VirtualTile *other) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change VTile")),
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

    TileShapeGroupDialog *mDialog;
    VirtualTile *mTile;
    VirtualTile mTile2;
};

class ReorderGroup : public QUndoCommand
{
public:
    ReorderGroup(TileShapeGroupDialog *d, int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Group")),
        mDialog(d),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        TileShapeGroup *g = VirtualTilesetMgr::instance().removeShapeGroup(mNewIndex);
        VirtualTilesetMgr::instance().insertShapeGroup(mOldIndex, g);
    }

    void redo()
    {
        TileShapeGroup *g = VirtualTilesetMgr::instance().removeShapeGroup(mOldIndex);
        VirtualTilesetMgr::instance().insertShapeGroup(mNewIndex, g);
    }

    TileShapeGroupDialog *mDialog;
    int mOldIndex;
    int mNewIndex;
};

} // namespace anon

TileShapeGroupDialog::TileShapeGroupDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileShapeGroupDialog),
    mUndoStack(new QUndoStack(this)),
    mCurrentGroup(0),
    mCurrentGroupTileset(0),
    mCurrentSrcGroup(0),
    mCurrentSrcGroupTileset(0),
    mTextureImage(VirtualTilesetMgr::instance().checkerboard()),
    mTextureX(-1),
    mTextureY(-1)
{
    ui->setupUi(this);

    QToolBar *toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddGroup);
    toolBar->addAction(ui->actionEditGroup);
    toolBar->addAction(ui->actionMoveGroupUp);
    toolBar->addAction(ui->actionMoveGroupDown);
    toolBar->addAction(ui->actionRemove_Group);
    ui->groupsToolBarLayout->addWidget(toolBar, 1);

    toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddShape);
    toolBar->addAction(ui->actionShapeProperties);
    toolBar->addAction(ui->actionClearShape);
    toolBar->addAction(ui->actionRemoveShape);
    ui->shapeToolBarLayout->addWidget(toolBar, 1);

    UndoRedoButtons *urb = new UndoRedoButtons(mUndoStack, this);
    ui->buttonsLayout->insertWidget(0, urb->undoButton());
    ui->buttonsLayout->insertWidget(1, urb->redoButton());

    // Drag within self to rearrange
    ui->shapesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->shapesList->setDragEnabled(true);
    ui->shapesList->setDragDropMode(QAbstractItemView::DragDrop);

    connect(ui->shapesList->model(), SIGNAL(beginDropTiles()), SLOT(beginDropTiles()));
    connect(ui->shapesList->model(), SIGNAL(endDropTiles()), SLOT(endDropTiles()));
    connect(ui->shapesList->model(), SIGNAL(tileDropped(VirtualTile*,QString,int,int,TileShape*)),
            SLOT(tileDropped(VirtualTile*,QString,int,int,TileShape*)));

    // Drag to shapesList to assign
    ui->shapeSrcList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->shapeSrcList->setDragEnabled(true);
    ui->shapeSrcList->setDragDropMode(QAbstractItemView::DragOnly);

    connect(ui->actionAddGroup, SIGNAL(triggered()), SLOT(addGroup()));
    connect(ui->actionEditGroup, SIGNAL(triggered()), SLOT(editGroup()));
    connect(ui->actionRemove_Group, SIGNAL(triggered()), SLOT(removeGroup()));
    connect(ui->actionMoveGroupUp, SIGNAL(triggered()), SLOT(moveGroupUp()));
    connect(ui->actionMoveGroupDown, SIGNAL(triggered()), SLOT(moveGroupDown()));

    connect(ui->actionAddShape, SIGNAL(triggered()), SLOT(addShape()));
    connect(ui->actionClearShape, SIGNAL(triggered()), SLOT(clearShape()));
    connect(ui->actionShapeProperties, SIGNAL(triggered()), SLOT(shapeProperties()));
    connect(ui->actionRemoveShape, SIGNAL(triggered()), SLOT(removeShape()));

    connect(ui->groupsList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectedGroupChanged()));
    connect(ui->shapesList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateActions()));
    connect(ui->shapesList, SIGNAL(activated(QModelIndex)), SLOT(shapeActivated(QModelIndex)));
    connect(ui->groupCombo, SIGNAL(currentIndexChanged(int)), SLOT(srcGroupSelected(int)));

    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupAdded(int,TileShapeGroup*)),
            SLOT(shapeGroupAdded(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupRemoved(int,TileShapeGroup*)),
            SLOT(shapeGroupRemoved(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupChanged(TileShapeGroup*)),
            SLOT(shapeGroupChanged(TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeAssigned(TileShapeGroup*,int,int)),
            SLOT(shapeAssigned(TileShapeGroup*,int,int)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeChanged(TileShape*)),
            SLOT(shapeChanged(TileShape*)));

    setGroupList();

    updateActions();
}

TileShapeGroupDialog::~TileShapeGroupDialog()
{
    delete ui;
}

void TileShapeGroupDialog::selectGroupAndShape(TileShapeGroup *g, int col, int row)
{
    int index = (g == mUngroupedGroup) ? 0 : VirtualTilesetMgr::instance().shapeGroups().indexOf(g) + 1;
    ui->groupsList->setCurrentRow(index);
    ui->shapesList->setCurrentIndex(ui->shapesList->model()->index(row, col));
    ui->groupCombo->setCurrentIndex(index);
}

bool TileShapeGroupDialog::needsSaving() const
{
    return !mUndoStack->isClean();
}

void TileShapeGroupDialog::insertGroup(int index, TileShapeGroup *g)
{
    VirtualTilesetMgr::instance().insertShapeGroup(index, g);
}

TileShapeGroup *TileShapeGroupDialog::removeGroup(int index)
{
    return VirtualTilesetMgr::instance().removeShapeGroup(index);
}

void TileShapeGroupDialog::editGroup(TileShapeGroup *g, TileShapeGroup &other)
{
    VirtualTilesetMgr::instance().editShapeGroup(g, other);
}

TileShape *TileShapeGroupDialog::assignShape(TileShapeGroup *g, int col, int row, TileShape *shape)
{
    TileShape *old = g->shapeAt(col, row);
    VirtualTilesetMgr::instance().assignShape(g, col, row, shape);
    return old;
}

void TileShapeGroupDialog::addShape(TileShape *shape)
{
    VirtualTilesetMgr::instance().addShape(shape);
}

void TileShapeGroupDialog::removeShape(TileShape *shape)
{
    VirtualTilesetMgr::instance().removeShape(shape);
}

void TileShapeGroupDialog::addGroup()
{
    TileShapeGroup _g(tr("New Group"), 8, 8);
    TileShapeGroupPropertiesDialog d(&_g, this);
    d.setWindowTitle(tr("New Group"));
    if (d.exec() != QDialog::Accepted)
        return;
    TileShapeGroup *g = new TileShapeGroup(d.label(), d.columnCount(), d.rowCount());
    mUndoStack->push(new AddGroup(this, VirtualTilesetMgr::instance().shapeGroups().size(), g));
}

void TileShapeGroupDialog::removeGroup()
{
    int q = QMessageBox::question(this, tr("Remove Group"), tr("The shapes in this group will not be destroyed.\nReally remove this group?"),
                                  QMessageBox::Yes, QMessageBox::No);
    if (q == QMessageBox::No)
        return;
    mUndoStack->push(new RemoveGroup(this, ui->groupsList->currentRow() - 1));
}

void TileShapeGroupDialog::editGroup()
{
    TileShapeGroupPropertiesDialog d(mCurrentGroup, this);
    d.setWindowTitle(tr("Edit Group"));
    if (d.exec() != QDialog::Accepted)
        return;
    TileShapeGroup changed(mCurrentGroup);
    changed.setLabel(d.label());
    changed.resize(d.columnCount(), d.rowCount());
    mUndoStack->push(new EditGroup(this, mCurrentGroup, &changed));
}

void TileShapeGroupDialog::moveGroupUp()
{
    int row = ui->groupsList->currentRow();
    if (row < 2) return;
    row -= 1; // mUngroupedGroup
    mUndoStack->push(new ReorderGroup(this, row, row - 1));
}

void TileShapeGroupDialog::moveGroupDown()
{
    int row = ui->groupsList->currentRow();
    if (row == -1 || row == ui->groupsList->count() -  1) return;
    row -= 1; // mUngroupedGroup
    mUndoStack->push(new ReorderGroup(this, row, row + 1));
}

void TileShapeGroupDialog::selectedGroupChanged()
{
    QModelIndexList selected = ui->groupsList->selectionModel()->selectedIndexes();
    int row = selected.size() ? selected.first().row() : -1;
    if (row == 0)
        mCurrentGroup = mUngroupedGroup;
    else
        mCurrentGroup = (row != -1) ? VirtualTilesetMgr::instance().shapeGroupAt(row - 1) : 0;
    setShapeList();
    updateActions();
}

void TileShapeGroupDialog::shapeGroupAdded(int index, TileShapeGroup *g)
{
    ui->groupsList->insertItem(index + 1, g->label());
    ui->groupsList->setCurrentRow(index + 1);
    ui->groupCombo->insertItem(index + 1, g->label());
}

void TileShapeGroupDialog::shapeGroupRemoved(int index, TileShapeGroup *g)
{
    Q_UNUSED(g)
    ui->groupsList->takeItem(index + 1);
    ui->groupCombo->removeItem(index + 1);
}

void TileShapeGroupDialog::shapeGroupChanged(TileShapeGroup *g)
{
    int row = VirtualTilesetMgr::instance().shapeGroups().indexOf(g) + 1;
    ui->groupsList->item(row)->setText(g->label());
    if (g == mCurrentGroup)
        setShapeList();

    ui->groupCombo->setItemText(row, g->label());
    if (g == mCurrentSrcGroup)
        setShapeSrcList();
}

void TileShapeGroupDialog::shapeAssigned(TileShapeGroup *g, int col, int row)
{
    TileShape *shape = g->shapeAt(col, row);
    if (g == mCurrentGroup) {
        VirtualTile *vtile = mCurrentGroupTileset->tileAt(col, row);
        vtile->setShape(shape);
        vtile->setImage(QImage());
        ui->shapesList->model()->redisplay(vtile);
    }
    if (g == mCurrentSrcGroup) {
        VirtualTile *vtile = mCurrentSrcGroupTileset->tileAt(col, row);
        vtile->setShape(shape);
        vtile->setImage(QImage());
        ui->shapeSrcList->model()->redisplay(vtile);
    }
    if (mCurrentGroup == mUngroupedGroup)
        setShapeList();
    if (mCurrentSrcGroup == mUngroupedGroup)
        setShapeSrcList();
}

void TileShapeGroupDialog::shapeChanged(TileShape *shape)
{
    if (mCurrentGroup && mCurrentGroup->hasShape(shape))
        ui->shapesList->model()->redisplay(shape);
    if (mCurrentSrcGroup && mCurrentSrcGroup->hasShape(shape))
        ui->shapeSrcList->model()->redisplay(shape);
}

void TileShapeGroupDialog::addShape()
{
    TileShape _shape(tr("my_new_shape"));
    TileShapePropertiesDialog d(&_shape, this);
    d.setWindowTitle(tr("New Shape"));
    if (d.exec() != QDialog::Accepted)
        return;

    mUndoStack->beginMacro(tr("Add Shape"));

    TileShape *shape = new TileShape(d.name());
    mUndoStack->push(new AddShape(this, shape));

    // Add it to the current group
    if (mCurrentGroup != mUngroupedGroup) {
        QModelIndexList selection = ui->shapesList->selectionModel()->selectedIndexes();
        Q_ASSERT(selection.size() == 1);
        QModelIndex index = selection.first();
        Q_ASSERT(mCurrentGroup->shapeAt(index.column(), index.row()) == 0);
        mUndoStack->push(new AssignShape(this, mCurrentGroup, index.column(), index.row(), shape));
    }

    mUndoStack->endMacro();

    TileShapeEditor ed(shape, mTextureImage, this);
    if (ed.exec() != QDialog::Accepted)
        return;
    mUndoStack->push(new ChangeShape(this, shape, ed.tileShape()));
#if 0
    shape->copy(ed.tileShape());
    if (mCurrentGroup == mUngroupedGroup)
        setShapeList();
    if (mCurrentSrcGroup == mUngroupedGroup)
        setShapeSrcList();
#endif
}

void TileShapeGroupDialog::shapeProperties()
{
    QModelIndexList selected = ui->shapesList->selectionModel()->selectedIndexes();
    QModelIndex index = selected.first();
    TileShape *shape = mCurrentGroup->shapeAt(index.column(), index.row());
    TileShapePropertiesDialog d(shape, this);
    d.setWindowTitle(tr("Edit Shape"));
    if (d.exec() != QDialog::Accepted)
        return;
    TileShape changed(shape);
    changed.mName = d.name();
    mUndoStack->push(new ChangeShape(this, shape, &changed));
}

void TileShapeGroupDialog::clearShape()
{
    QModelIndexList selected = ui->shapesList->selectionModel()->selectedIndexes();
    mUndoStack->beginMacro(tr("Clear Shapes"));
    foreach (QModelIndex index, selected)
        mUndoStack->push(new AssignShape(this, mCurrentGroup, index.column(), index.row(), 0));
    mUndoStack->endMacro();
}

void TileShapeGroupDialog::removeShape()
{
    QModelIndexList selected = ui->shapesList->selectionModel()->selectedIndexes();
    int q = QMessageBox::question(this, tr("Destroy Shape"), tr("Any tiles the using removed shapes will lose their images!\nReally destroy the selected shapes?"),
                                  QMessageBox::Yes, QMessageBox::No);
    if (q == QMessageBox::No)
        return;

    mUndoStack->beginMacro(tr("Destroy Shapes"));
    foreach (QModelIndex index, selected) {
        TileShape *shape = mCurrentGroup->shapeAt(index.column(), index.row());

        // Remove from 'sameAs'
        foreach (TileShape *shape2, VirtualTilesetMgr::instance().tileShapes()) {
            if (shape2->mSameAs == shape) {
                TileShape changed(shape2);
                changed.mSameAs = 0;
                changed.mXform.clear();
                mUndoStack->push(new ChangeShape(this, shape2, &changed));
            }
        }

        // Remove shape from groups
        foreach (TileShapeGroup *group, VirtualTilesetMgr::instance().shapeGroups()) {
            for (int i = 0; i < group->count(); i++) {
                if (group->shapeAt(i) == shape)
                    mUndoStack->push(new AssignShape(this, group,
                                                     i % group->columnCount(),
                                                     i / group->columnCount(),
                                                     0));
            }
        }

        // Remove shape from virtual tileset tiles
        foreach (VirtualTileset *vts, VirtualTilesetMgr::instance().tilesets()) {
            for (int i = 0; i < vts->tileCount(); i++) {
                VirtualTile *vtile = vts->tileAt(i);
                if (vtile->shape() == shape) {
                    VirtualTile changed(vtile);
                    changed.setShape(0);
                    mUndoStack->push(new ChangeVTile(this, vtile, &changed));
                }
            }
        }

        mUndoStack->push(new RemoveShape(this, shape));
    }
    mUndoStack->endMacro();
}

void TileShapeGroupDialog::shapeActivated(const QModelIndex &index)
{
    TileShape *shape = mCurrentGroup->shapeAt(index.column(), index.row());
    if (!shape) {
        addShape();
        return;
    }
    TileShapeEditor d(shape, mTextureImage, this);
    if (d.exec() != QDialog::Accepted)
        return;
    mUndoStack->push(new ChangeShape(this, shape, d.tileShape()));
}

void TileShapeGroupDialog::beginDropTiles()
{
    mUndoStack->beginMacro(tr("Assign Shapes"));
}

void TileShapeGroupDialog::endDropTiles()
{
    mUndoStack->endMacro();
}

void TileShapeGroupDialog::tileDropped(VirtualTile *vtile, const QString &imageSource,
                                       int srcX, int srcY, TileShape *shape)
{
    Q_UNUSED(imageSource)
    Q_UNUSED(srcX)
    Q_UNUSED(srcY)
    mUndoStack->push(new AssignShape(this, mCurrentGroup, vtile->x(), vtile->y(), shape));
}

void TileShapeGroupDialog::srcGroupSelected(int index)
{
    if (index == 0)
        mCurrentSrcGroup = mUngroupedGroup;
    else
        mCurrentSrcGroup = (index != -1) ? VirtualTilesetMgr::instance().shapeGroupAt(index - 1) : 0;
    setShapeSrcList();
}

void TileShapeGroupDialog::updateActions()
{
    ui->actionEditGroup->setEnabled(mCurrentGroup != 0 && mCurrentGroup != mUngroupedGroup);
    ui->actionRemove_Group->setEnabled(mCurrentGroup != 0 && mCurrentGroup != mUngroupedGroup);
    ui->actionMoveGroupUp->setEnabled(mCurrentGroup != 0 && mCurrentGroup != mUngroupedGroup
            && mCurrentGroup != VirtualTilesetMgr::instance().shapeGroups().first());
    ui->actionMoveGroupDown->setEnabled(mCurrentGroup != 0 && mCurrentGroup != mUngroupedGroup
            && mCurrentGroup != VirtualTilesetMgr::instance().shapeGroups().last());

    QModelIndexList selection = ui->shapesList->selectionModel()->selectedIndexes();
    bool emptyTile = mCurrentGroup == mUngroupedGroup;
    if ((mCurrentGroup != mUngroupedGroup) && (selection.size() == 1)) {
        QModelIndex index = selection.first();
        TileShape *shape = mCurrentGroup->shapeAt(index.column(), index.row());
        emptyTile = (shape == 0);
    }
    ui->actionAddShape->setEnabled((mCurrentGroup == mUngroupedGroup) || emptyTile);
    ui->actionShapeProperties->setEnabled(selection.size() == 1);
    ui->actionClearShape->setEnabled(selection.size() != 0 && mCurrentGroup != mUngroupedGroup);
    ui->actionRemoveShape->setEnabled(selection.size() != 0);
}

void TileShapeGroupDialog::setGroupList()
{
    mUngroupedGroup = VirtualTilesetMgr::instance().ungroupedGroup();

    ui->groupsList->clear();
    ui->groupsList->addItem(mUngroupedGroup->label());
    ui->groupsList->addItems(VirtualTilesetMgr::instance().shapeGroupLabels());

    ui->groupCombo->clear();
    ui->groupCombo->addItem(mUngroupedGroup->label());
    ui->groupCombo->addItems(VirtualTilesetMgr::instance().shapeGroupLabels());
}

void TileShapeGroupDialog::setShapeList()
{
    VirtualTileset *vts = mCurrentGroupTileset;
    mCurrentGroupTileset = 0;
    if (mCurrentGroup) {
        mCurrentGroupTileset = new VirtualTileset(mCurrentGroup->label(),
                                                  mCurrentGroup->columnCount(),
                                                  mCurrentGroup->rowCount());
        for (int i = 0; i < mCurrentGroup->count(); i++) {
            mCurrentGroupTileset->tileAt(i)->setImageSource(mTextureName, mTextureX, mTextureY);
            mCurrentGroupTileset->tileAt(i)->setShape(mCurrentGroup->shapeAt(i));
        }
    }
    ui->shapesList->setTileset(mCurrentGroupTileset);
    delete vts;
}

void TileShapeGroupDialog::setShapeSrcList()
{
    VirtualTileset *vts = mCurrentSrcGroupTileset;
    mCurrentSrcGroupTileset = 0;
    if (mCurrentSrcGroup) {
        mCurrentSrcGroupTileset = new VirtualTileset(mCurrentSrcGroup->label(),
                                                  mCurrentSrcGroup->columnCount(),
                                                  mCurrentSrcGroup->rowCount());
        for (int i = 0; i < mCurrentSrcGroup->count(); i++) {
            mCurrentSrcGroupTileset->tileAt(i)->setImageSource(mTextureName, mTextureX, mTextureY);
            mCurrentSrcGroupTileset->tileAt(i)->setShape(mCurrentSrcGroup->shapeAt(i));
        }
    }
    ui->shapeSrcList->setTileset(mCurrentSrcGroupTileset);
    delete vts;
}
