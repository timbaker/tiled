/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#include "tilemetainfodialog.h"
#include "ui_tilemetainfodialog.h"

#include "preferences.h"
#include "tilemetainfomgr.h"
#include "utils.h"
#include "zoomable.h"

#include "tileset.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace TileMetaUndoRedo {

class AddTileset : public QUndoCommand
{
public:
    AddTileset(TileMetaInfoDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tileset")),
        mDialog(d),
        mTileset(tileset)
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

    TileMetaInfoDialog *mDialog;
    Tileset *mTileset;
};

class RemoveTileset : public QUndoCommand
{
public:
    RemoveTileset(TileMetaInfoDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tileset")),
        mDialog(d),
        mTileset(tileset)
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

    TileMetaInfoDialog *mDialog;
    Tileset *mTileset;
};

class SetTileMetaEnum : public QUndoCommand
{
public:
    SetTileMetaEnum(TileMetaInfoDialog *d, Tile *tile, const QString &enumName) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Tile Meta-Enum")),
        mDialog(d),
        mTile(tile),
        mEnumName(enumName)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mEnumName = mDialog->setTileEnum(mTile, mEnumName);
    }

    TileMetaInfoDialog *mDialog;
    Tile *mTile;
    QString mEnumName;
};

} // namespace TileMetaUndoRedo

using namespace TileMetaUndoRedo;

/////

TileMetaInfoDialog::TileMetaInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TileMetaInfoDialog),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this)),
    mSynching(false),
    mClosing(false),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    QToolBar *toolBar = new QToolBar();
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAdd);
    toolBar->addAction(ui->actionRemove);
    ui->toolBarLayout->addWidget(toolBar);

    /////

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QToolButton *button = new QToolButton(this);
    button->setIcon(undoIcon);
    Utils::setThemeIcon(button, "edit-undo");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(undoAction->text());
    button->setEnabled(mUndoGroup->canUndo());
    button->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->undoRedoLayout->addWidget(button);
    connect(mUndoGroup, SIGNAL(canUndoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), undoAction, SIGNAL(triggered()));

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(redoAction->text());
    button->setEnabled(mUndoGroup->canRedo());
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->undoRedoLayout->addWidget(button);
    connect(mUndoGroup, SIGNAL(canRedoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), redoAction, SIGNAL(triggered()));

    connect(mUndoGroup, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));

    /////

    mZoomable->setScale(0.5); // FIXME
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    ui->tiles->model()->setShowHeaders(false);

    ui->tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tiles->model()->setShowHeaders(false);
    ui->tiles->model()->setShowLabels(true);

    connect(ui->browseTiles, SIGNAL(clicked()), SLOT(browse()));
    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(ui->actionAdd, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionRemove, SIGNAL(triggered()), SLOT(removeTileset()));
    connect(ui->enums, SIGNAL(activated(int)),
            SLOT(enumChanged(int)));

    setTilesetList();

    mSynching = true;
    ui->enums->addItem(tr("<none>"));
    ui->enums->addItems(TileMetaInfoMgr::instance()->enumNames());
    mSynching = false;

    updateUI();
}

TileMetaInfoDialog::~TileMetaInfoDialog()
{
    delete ui;
}

QString TileMetaInfoDialog::setTileEnum(Tile *tile, const QString &enumName)
{
    QString old = TileMetaInfoMgr::instance()->tileEnum(tile);
    TileMetaInfoMgr::instance()->setTileEnum(tile, enumName);
    ui->tiles->model()->setLabel(tile, enumName);
    updateUI();
    return old;
}

void TileMetaInfoDialog::addTileset()
{
    const QString tilesDir = TileMetaInfoMgr::instance()->tilesDirectory();
    const QString filter = Utils::readableImageFormatsFilter();
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Tileset Image"),
                                                          tilesDir,
                                                          filter);

    mUndoStack->beginMacro(tr("Add Tilesets"));

    foreach (QString f, fileNames) {
        QFileInfo info(f);
        if (Tiled::Tileset *ts = TileMetaInfoMgr::instance()->loadTileset(info.canonicalFilePath())) {
            QString name = info.completeBaseName();
            // Replace any current tileset with the same name as an existing one.
            // This will NOT replace the meta-info for the old tileset, it will
            // be used by the new tileset as well.
            if (Tileset *old = TileMetaInfoMgr::instance()->tileset(name))
                mUndoStack->push(new RemoveTileset(this, old));
            mUndoStack->push(new AddTileset(this, ts));
        } else {
            QMessageBox::warning(this, tr("It's no good, Jim!"),
                                 TileMetaInfoMgr::instance()->errorString());
        }
    }

    mUndoStack->endMacro();
}

void TileMetaInfoDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->tilesets->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->tilesets->row(item);
        Tileset *tileset = TileMetaInfoMgr::instance()->tileset(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?\nYou will lose all the meta-info for this tileset!")
                                  .arg(tileset->name()),
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        mUndoStack->push(new RemoveTileset(this, tileset));
    }
}

void TileMetaInfoDialog::addTileset(Tileset *ts)
{
    TileMetaInfoMgr::instance()->addTileset(ts);
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(ts);
    ui->tilesets->setCurrentRow(row);
}

void TileMetaInfoDialog::removeTileset(Tileset *ts)
{
    int row = TileMetaInfoMgr::instance()->indexOf(ts);
    TileMetaInfoMgr::instance()->removeTileset(ts);
    setTilesetList();
    ui->tilesets->setCurrentRow(row);
}

void TileMetaInfoDialog::currentTilesetChanged(int row)
{
    if (mClosing)
        return;
    mCurrentTileset = 0;
    if (row >= 0) {
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    }
    setTilesList();
    updateUI();
}

void TileMetaInfoDialog::tileSelectionChanged()
{
    mSelectedTiles.clear();

    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        if (Tile *tile = ui->tiles->model()->tileAt(index))
            mSelectedTiles += tile;
    }

    updateUI();
}

void TileMetaInfoDialog::enumChanged(int index)
{
    if (mSynching)
        return;

    QString enumName;
    if (index > 0)
        enumName = TileMetaInfoMgr::instance()->enumNames().at(index - 1);

    QList<Tile*> tiles;
    foreach (Tile *tile, mSelectedTiles) {
        if (TileMetaInfoMgr::instance()->tileEnum(tile) != enumName)
            tiles += tile;
    }

    if (!tiles.size())
        return;

    mUndoStack->beginMacro(tr("Change Tile(s) Meta-Enum"));
    foreach (Tile *tile, tiles)
        mUndoStack->push(new SetTileMetaEnum(this, tile, enumName));
    mUndoStack->endMacro();
}

void TileMetaInfoDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void TileMetaInfoDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void TileMetaInfoDialog::browse()
{
    QString f = QFileDialog::getExistingDirectory(this, tr("Directory"),
                                                  ui->editTiles->text());
    if (!f.isEmpty()) {
        Preferences::instance()->setTilesDirectory(f);
        TileMetaInfoMgr::instance()->loadTilesets();
        setTilesetList();
        updateUI();
    }
}

void TileMetaInfoDialog::updateUI()
{
    mSynching = true;

    QString tilesDir = TileMetaInfoMgr::instance()->tilesDirectory();
    ui->editTiles->setText(QDir::toNativeSeparators(tilesDir));

    ui->actionRemove->setEnabled(mCurrentTileset != 0);
    ui->enums->setEnabled(mSelectedTiles.size() > 0);

    QSet<QString> enums;
    foreach (Tile *tile, mSelectedTiles)
         enums.insert(TileMetaInfoMgr::instance()->tileEnum(tile)); // could be nil

    if (enums.size() == 1) {
        QString enumName = enums.values()[0];
        int index = TileMetaInfoMgr::instance()->enumNames().indexOf(enumName);
        ui->enums->setCurrentIndex(index + 1);
    } else {
        ui->enums->setCurrentIndex(0);
    }

    mSynching = false;
}

void TileMetaInfoDialog::accept()
{
    mClosing = true; // getting a crash when TileMetaInfoMgr is deleted before this in MainWindow::tilesetMetaInfoDialog
    ui->tilesets->clear();
    ui->tiles->model()->setTiles(QList<Tile*>());
    QDialog::accept();
}

void TileMetaInfoDialog::reject()
{
    accept();
}

void TileMetaInfoDialog::setTilesetList()
{
    if (mClosing)
        return;

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 0;

    ui->tilesets->clear();
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name());
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.width(ts->name()));
    }
    ui->tilesets->setFixedWidth(maxWidth +
                                ui->tilesets->verticalScrollBar()->sizeHint().width()
                                + 16);
//    ui->horizontalLayout_XXX->setStretchFactor(ui->tilesetLayout, 0);
//    ui->horizontalLayout_XXX->setStretchFactor(ui->tilesLayout, 1);
}

void TileMetaInfoDialog::setTilesList()
{
    if (mCurrentTileset) {
        QStringList labels;
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            Tile *tile = mCurrentTileset->tileAt(i);
            labels += TileMetaInfoMgr::instance()->tileEnum(tile);
        }
        ui->tiles->model()->setTileset(mCurrentTileset, labels);
    } else {
        ui->tiles->model()->setTiles(QList<Tile*>());
    }
}


