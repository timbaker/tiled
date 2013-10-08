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

#include "tileshapegrouppropertiesdialog.h"
#include "undoredobuttons.h"
#include "virtualtileset.h"

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
    EditGroup(TileShapeGroupDialog *d, TileShapeGroup *g, const QString &label,
              int columnCount, int rowCount) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Edit Group")),
        mDialog(d),
        mGroup(g),
        mLabel(label),
        mColumnCount(columnCount),
        mRowCount(rowCount)
    {
    }

    void undo()
    {
        mDialog->editGroup(mGroup, mLabel, mColumnCount, mRowCount);
    }

    void redo()
    {
        mDialog->editGroup(mGroup, mLabel, mColumnCount, mRowCount);
    }

    TileShapeGroupDialog *mDialog;
    TileShapeGroup *mGroup;
    QString mLabel;
    int mColumnCount;
    int mRowCount;
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

} // namespace anon

TileShapeGroupDialog::TileShapeGroupDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileShapeGroupDialog),
    mUndoStack(new QUndoStack(this)),
    mCurrentGroup(0),
    mCurrentGroupTileset(0),
    mCurrentSrcGroup(0),
    mCurrentSrcGroupTileset(0)
{
    ui->setupUi(this);

    QToolBar *toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAddGroup);
    toolBar->addAction(ui->actionEditGroup);
    toolBar->addAction(ui->actionRemove_Group);
    ui->groupsToolBarLayout->addWidget(toolBar, 1);

    toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionClearShape);
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
    connect(ui->actionAddShape, SIGNAL(triggered()), SLOT(addShape()));
    connect(ui->actionClearShape, SIGNAL(triggered()), SLOT(clearShape()));

    connect(ui->groupsList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(selectedGroupChanged()));
    connect(ui->shapesList->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateActions()));
    connect(ui->groupCombo, SIGNAL(currentIndexChanged(int)), SLOT(srcGroupSelected(int)));

    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupAdded(int,TileShapeGroup*)),
            SLOT(shapeGroupAdded(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupRemoved(int,TileShapeGroup*)),
            SLOT(shapeGroupRemoved(int,TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeGroupChanged(TileShapeGroup*)),
            SLOT(shapeGroupChanged(TileShapeGroup*)));
    connect(VirtualTilesetMgr::instancePtr(), SIGNAL(shapeAssigned(TileShapeGroup*,int,int)),
            SLOT(shapeAssigned(TileShapeGroup*,int,int)));

    setGroupList();
}

TileShapeGroupDialog::~TileShapeGroupDialog()
{
    delete ui;
}

void TileShapeGroupDialog::insertGroup(int index, TileShapeGroup *g)
{
    VirtualTilesetMgr::instance().insertShapeGroup(index, g);
}

TileShapeGroup *TileShapeGroupDialog::removeGroup(int index)
{
    return VirtualTilesetMgr::instance().removeShapeGroup(index);
}

void TileShapeGroupDialog::editGroup(TileShapeGroup *g, QString &label,
                                     int &columnCount, int &rowCount)
{
    QString oldLabel = g->label();
    int oldCC = g->columnCount();
    int oldRC = g->rowCount();
    VirtualTilesetMgr::instance().editShapeGroup(g, label, columnCount, rowCount);
    label = oldLabel;
    columnCount = oldCC;
    rowCount = oldRC;
}

TileShape *TileShapeGroupDialog::assignShape(TileShapeGroup *g, int col, int row, TileShape *shape)
{
    TileShape *old = g->shapeAt(col, row);
    VirtualTilesetMgr::instance().assignShape(g, col, row, shape);
    return old;
}

void TileShapeGroupDialog::addGroup()
{
    TileShapeGroup _g(tr("New Group"), 8, 8);
    TileShapeGroupPropertiesDialog d(&_g, this);
    if (d.exec() != QDialog::Accepted)
        return;
    TileShapeGroup *g = new TileShapeGroup(d.label(), d.columnCount(), d.rowCount());
    mUndoStack->push(new AddGroup(this, VirtualTilesetMgr::instance().shapeGroups().size(), g));
}

void TileShapeGroupDialog::removeGroup()
{

}

void TileShapeGroupDialog::editGroup()
{
    TileShapeGroupPropertiesDialog d(mCurrentGroup, this);
    if (d.exec() != QDialog::Accepted)
        return;
    mUndoStack->push(new EditGroup(this, mCurrentGroup, d.label(), d.columnCount(), d.rowCount()));
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

void TileShapeGroupDialog::addShape()
{

}

void TileShapeGroupDialog::clearShape()
{
    QModelIndexList selected = ui->shapesList->selectionModel()->selectedIndexes();
    mUndoStack->beginMacro(tr("Clear Shapes"));
    foreach (QModelIndex index, selected)
        mUndoStack->push(new AssignShape(this, mCurrentGroup, index.column(), index.row(), 0));
    mUndoStack->endMacro();
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
    ui->actionClearShape->setEnabled(ui->shapesList->selectionModel()->selectedIndexes().size() != 0 &&
            mCurrentGroup != mUngroupedGroup);
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
        for (int i = 0; i < mCurrentGroup->count(); i++)
            mCurrentGroupTileset->tileAt(i)->setShape(mCurrentGroup->shapeAt(i));
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
        for (int i = 0; i < mCurrentSrcGroup->count(); i++)
            mCurrentSrcGroupTileset->tileAt(i)->setShape(mCurrentSrcGroup->shapeAt(i));
    }
    ui->shapeSrcList->setTileset(mCurrentSrcGroupTileset);
    delete vts;
}
