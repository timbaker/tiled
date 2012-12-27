/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
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

#include "buildingfloorsdialog.h"
#include "ui_buildingfloorsdialog.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingundoredo.h"

#include "utils.h"

#include <QToolBar>
#include <QToolButton>

using namespace BuildingEditor;
using namespace Tiled;

BuildingFloorsDialog::BuildingFloorsDialog(BuildingDocument *doc, QWidget *parent) :
    QDialog(parent),
    mDocument(doc),
    mCurrentFloor(0),
    ui(new Ui::BuildingFloorsDialog)
{
    ui->setupUi(this);

    /////
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));

    QToolButton *button = new QToolButton(this);
    button->setIcon(undoIcon);
    Utils::setThemeIcon(button, "edit-undo");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(tr("Undo"));
//    button->setEnabled(mUndoGroup->canUndo());
    button->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->undoRedoLayout->insertWidget(0, button);
    connect(button, SIGNAL(clicked()), mDocument->undoStack(), SLOT(undo()));

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(tr("Redo"));
//    button->setEnabled(mUndoGroup->canRedo());
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->undoRedoLayout->insertWidget(1, button);
    connect(button, SIGNAL(clicked()), mDocument->undoStack(), SLOT(redo()));

    mUndoIndex = mDocument->undoStack()->index();
    connect(mDocument->undoStack(), SIGNAL(indexChanged(int)), SLOT(updateUI()));
    connect(mDocument->undoStack(), SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mDocument->undoStack(), SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));
    /////

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAdd);
    toolBar->addAction(ui->actionDuplicate);
    toolBar->addAction(ui->actionRemove);
    toolBar->addAction(ui->actionMoveUp);
    toolBar->addAction(ui->actionMoveDown);
    ui->toolbarLayout->addWidget(toolBar);

    connect(ui->actionAdd, SIGNAL(triggered()), SLOT(add()));
    connect(ui->actionRemove, SIGNAL(triggered()), SLOT(remove()));
    connect(ui->actionDuplicate, SIGNAL(triggered()), SLOT(duplicate()));
    connect(ui->actionMoveUp, SIGNAL(triggered()), SLOT(moveUp()));
    connect(ui->actionMoveDown, SIGNAL(triggered()), SLOT(moveDown()));

    connect(ui->floors, SIGNAL(currentRowChanged(int)),
            SLOT(currentFloorChanged(int)));

    ui->highlight->setChecked(BuildingPreferences::instance()->highlightFloor());
    connect(ui->highlight, SIGNAL(toggled(bool)),
            BuildingPreferences::instance(), SLOT(setHighlightFloor(bool)));

    connect(mDocument, SIGNAL(floorAdded(BuildingFloor*)),
            SLOT(floorAdded(BuildingFloor*)));
    connect(mDocument, SIGNAL(floorRemoved(BuildingFloor*)),
            SLOT(floorRemoved(BuildingFloor*)));

    setFloorsList();
    if (BuildingFloor *floor = mDocument->currentFloor())
        ui->floors->setCurrentRow(toRow(floor));
    updateUI();
}

BuildingFloorsDialog::~BuildingFloorsDialog()
{
    delete ui;
}

void BuildingFloorsDialog::add()
{
    int level = mCurrentFloor ? mCurrentFloor->level() + 1 : mDocument->building()->floorCount();
    BuildingFloor *floor = new BuildingFloor(mDocument->building(), level);
    mDocument->undoStack()->push(new InsertFloor(mDocument, level, floor));
}

void BuildingFloorsDialog::remove()
{
    if (mCurrentFloor && (mDocument->building()->floorCount() > 1))
        mDocument->undoStack()->push(new RemoveFloor(mDocument,
                                                     mCurrentFloor->level()));
}

void BuildingFloorsDialog::duplicate()
{
    if (mCurrentFloor) {
        BuildingFloor *floor = mCurrentFloor->clone();
        floor->setLevel(mCurrentFloor->level() + 1);
        mDocument->undoStack()->push(new InsertFloor(mDocument, floor->level(),
                                                     floor));
    }
}

void BuildingFloorsDialog::moveUp()
{
    if (mCurrentFloor && !mCurrentFloor->isTopFloor()) {
        int index = mCurrentFloor->level();
        mDocument->undoStack()->push(new ReorderFloor(mDocument, index, index + 1));
    }
}

void BuildingFloorsDialog::moveDown()
{
    if (mCurrentFloor && !mCurrentFloor->isBottomFloor()) {
        int index = mCurrentFloor->level();
        mDocument->undoStack()->push(new ReorderFloor(mDocument, index, index - 1));
    }
}

void BuildingFloorsDialog::floorAdded(BuildingFloor *floor)
{
    setFloorsList();
    ui->floors->setCurrentRow(toRow(floor));
}

void BuildingFloorsDialog::floorRemoved(BuildingFloor *floor)
{
    int row = ui->floors->currentRow();
    setFloorsList();
    ui->floors->setCurrentRow(qBound(0, row, ui->floors->count() - 1));
}

void BuildingFloorsDialog::currentFloorChanged(int row)
{
    if ((mCurrentFloor = floorAt(row)) && (mCurrentFloor != mDocument->currentFloor())) {
        mDocument->setSelectedObjects(QSet<BuildingObject*>());
        mDocument->setCurrentFloor(mCurrentFloor);
    }
    updateUI();
}

void BuildingFloorsDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void BuildingFloorsDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void BuildingFloorsDialog::updateUI()
{
    int row = ui->floors->currentRow();
    ui->actionRemove->setEnabled((row != -1) && (ui->floors->count() > 1));
    ui->actionDuplicate->setEnabled(row != -1);
    ui->actionMoveUp->setEnabled(row > 0);
    ui->actionMoveDown->setEnabled(row != -1 && row < ui->floors->count() - 1);

    mUndoButton->setEnabled(mDocument->undoStack()->index() > mUndoIndex);
    mRedoButton->setEnabled(mDocument->undoStack()->canRedo());
}

void BuildingFloorsDialog::setFloorsList()
{
    ui->floors->clear();
    foreach (BuildingFloor *floor, mDocument->building()->floors())
        ui->floors->insertItem(0, tr("Floor %1").arg(floor->level() + 1));
}

int BuildingFloorsDialog::toRow(BuildingFloor *floor)
{
    return mDocument->building()->floorCount() - floor->level() - 1;
}

BuildingFloor *BuildingFloorsDialog::floorAt(int row)
{
    if (row < 0 || row >= ui->floors->count())
        return 0;
    return mDocument->building()->floor(ui->floors->count() - row - 1);
}
