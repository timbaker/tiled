/*
 * tilesetdock.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 * Copyright 2012, Stefan Beller <stefanbeller@googlemail.com>
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

#ifndef ZOMBOID

#include "tilesetdock.h"

#include "addremovemapobject.h"
#include "addremovetileset.h"
#include "documentmanager.h"
#include "erasetiles.h"
#include "map.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "movetileset.h"
#include "objectgroup.h"
#include "propertiesdialog.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tilesetmodel.h"
#include "tilesetview.h"
#include "tilesetmanager.h"
#include "tmxmapwriter.h"
#include "utils.h"
#include "zoomable.h"

#include <QAction>
#include <QDropEvent>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QSignalMapper>
#include <QStackedWidget>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

/**
 * Used for exporting/importing tilesets.
 *
 * @warning Does not work for tilesets that are shared by multiple maps!
 */
class SetTilesetFileName : public QUndoCommand
{
public:
    SetTilesetFileName(MapDocument *mapDocument,
                       Tileset *tileset,
                       const QString &fileName)
        : mMapDocument(mapDocument)
        , mTileset(tileset)
        , mFileName(fileName)
    {
        if (fileName.isEmpty())
            setText(QCoreApplication::translate("Undo Commands",
                                                "Import Tileset"));
        else
            setText(QCoreApplication::translate("Undo Commands",
                                                "Export Tileset"));
    }

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap()
    {
        QString previousFileName = mTileset->fileName();
        mMapDocument->setTilesetFileName(mTileset, mFileName);
        mFileName = previousFileName;
    }

    MapDocument *mMapDocument;
    Tileset *mTileset;
    QString mFileName;
};

class RenameTileset : public QUndoCommand
{
public:
    RenameTileset(MapDocument *mapDocument,
                  Tileset *tileset,
                  const QString &oldName,
                  const QString &newName)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Tileset name"))
        , mMapDocument(mapDocument)
        , mTileset(tileset)
        , mOldName(oldName)
        , mNewName(newName)
    {
        redo();
    }

    void undo()
    {
        mMapDocument->setTilesetName(mTileset, mOldName);
    }

    void redo()
    {
        mMapDocument->setTilesetName(mTileset, mNewName);
    }

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
    QString mOldName;
    QString mNewName;
};

} // anonymous namespace

TilesetDock::TilesetDock(QWidget *parent):
    QDockWidget(parent),
    mMapDocument(0),
    mTabBar(new QTabBar),
    mViewStack(new QStackedWidget),
    mToolBar(new QToolBar),
    mCurrentTile(0),
    mCurrentTiles(0),
    mImportTileset(new QAction(this)),
    mExportTileset(new QAction(this)),
    mPropertiesTileset(new QAction(this)),
    mDeleteTileset(new QAction(this)),
    mRenameTileset(new QAction(this)),
    mTilesetMenuButton(new QToolButton(this)),
    mTilesetMenu(new QMenu(this)),
    mTilesetMenuMapper(0)
{
    setObjectName(QLatin1String("TilesetDock"));

    mTabBar->setMovable(true);
    mTabBar->setUsesScrollButtons(true);

    connect(mTabBar, SIGNAL(currentChanged(int)),
            SLOT(updateActions()));
    connect(mTabBar, SIGNAL(currentChanged(int)),
            mViewStack, SLOT(setCurrentIndex(int)));
    connect(mTabBar, SIGNAL(tabMoved(int,int)),
            this, SLOT(moveTileset(int,int)));

    QWidget *w = new QWidget(this);

    QHBoxLayout *horizontal = new QHBoxLayout();
    horizontal->setSpacing(5);
    horizontal->addWidget(mTabBar);
    horizontal->addWidget(mTilesetMenuButton);

    QVBoxLayout *vertical = new QVBoxLayout(w);
    vertical->setSpacing(5);
    vertical->setMargin(5);
    vertical->addLayout(horizontal);
    vertical->addWidget(mViewStack);
    horizontal = new QHBoxLayout();
    horizontal->setSpacing(5);
    horizontal->addWidget(mToolBar, 1);
    vertical->addLayout(horizontal);

    mImportTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-import.png")));
    mExportTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-export.png")));
    mPropertiesTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-properties.png")));
    mDeleteTileset->setIcon(QIcon(QLatin1String(":images/16x16/edit-delete.png")));
    mRenameTileset->setIcon(QIcon(QLatin1String(":images/16x16/edit-rename.png")));

    Utils::setThemeIcon(mImportTileset, "document-import");
    Utils::setThemeIcon(mExportTileset, "document-export");
    Utils::setThemeIcon(mPropertiesTileset, "document-properties");
    Utils::setThemeIcon(mDeleteTileset, "edit-delete");
    Utils::setThemeIcon(mRenameTileset, "edit-rename");

    connect(mImportTileset, SIGNAL(triggered()),
            SLOT(importTileset()));
    connect(mExportTileset, SIGNAL(triggered()),
            SLOT(exportTileset()));
    connect(mPropertiesTileset, SIGNAL(triggered()),
            SLOT(editTilesetProperties()));
    connect(mDeleteTileset, SIGNAL(triggered()),
            SLOT(removeTileset()));
    connect(mRenameTileset, SIGNAL(triggered()),
            SLOT(renameTileset()));

    mToolBar->setIconSize(QSize(16, 16));
    mToolBar->addAction(mImportTileset);
    mToolBar->addAction(mExportTileset);
    mToolBar->addAction(mPropertiesTileset);
    mToolBar->addAction(mDeleteTileset);
    mToolBar->addAction(mRenameTileset);

    mZoomable = new Zoomable(this);
    mZoomable->setZoomFactors(QVector<qreal>() << 0.25 << 0.5 << 0.75 << 1.0 << 1.25 << 1.5 << 1.75 << 2.0);
    mToolBar->addSeparator();
    mZoomComboBox = new QComboBox;
    mZoomable->connectToComboBox(mZoomComboBox);
    horizontal->addWidget(mZoomComboBox);

    connect(mViewStack, SIGNAL(currentChanged(int)),
            this, SLOT(updateCurrentTiles()));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            this, SLOT(tilesetChanged(Tileset*)));

    connect(DocumentManager::instance(), SIGNAL(documentCloseRequested(int)),
            SLOT(documentCloseRequested(int)));

    mTilesetMenuButton->setMenu(mTilesetMenu);
    mTilesetMenuButton->setPopupMode(QToolButton::InstantPopup);
    mTilesetMenuButton->setAutoRaise(true);
    connect(mTilesetMenu, SIGNAL(aboutToShow()), SLOT(refreshTilesetMenu()));

    setWidget(w);

    retranslateUi();
    setAcceptDrops(true);
    updateActions();
}

TilesetDock::~TilesetDock()
{
    delete mCurrentTiles;
}

void TilesetDock::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    setCurrentTiles(0);

    if (mMapDocument)
        mCurrentTilesets.insert(mMapDocument,
                                mTabBar->tabText(mTabBar->currentIndex()));
    // Clear previous content

    while (mTabBar->count())
        mTabBar->removeTab(0);
    while (mViewStack->currentWidget())
        delete mViewStack->currentWidget();

    // Clear all connections to the previous document
    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;

    if (mMapDocument) {
        Map *map = mMapDocument->map();
        foreach (Tileset *tileset, map->tilesets())
            insertTilesetView(mTabBar->count(), tileset);

        connect(mMapDocument, SIGNAL(tilesetAdded(int,Tileset*)),
                SLOT(insertTilesetView(int,Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetRemoved(Tileset*)),
                SLOT(tilesetRemoved(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetMoved(int,int)),
                SLOT(tilesetMoved(int,int)));
        connect(mMapDocument, SIGNAL(tilesetNameChanged(Tileset*)),
                SLOT(tilesetNameChanged(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetFileNameChanged(Tileset*)),
                SLOT(updateActions()));

        QString cacheName = mCurrentTilesets.take(mMapDocument);
        for (int i = 0; i < mTabBar->count(); ++i) {
            if (mTabBar->tabText(i) == cacheName) {
                mTabBar->setCurrentIndex(i);
                break;
            }
        }
    }
    updateActions();
}

void TilesetDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TilesetDock::dragEnterEvent(QDragEnterEvent *e)
{
    const QList<QUrl> urls = e->mimeData()->urls();
    if (!urls.isEmpty() && !urls.at(0).toLocalFile().isEmpty())
        e->accept();
}

void TilesetDock::dropEvent(QDropEvent *e)
{
    QStringList paths;
    foreach (const QUrl &url, e->mimeData()->urls()) {
        const QString localFile = url.toLocalFile();
        if (!localFile.isEmpty())
            paths.append(localFile);
    }
    if (!paths.isEmpty()) {
        emit tilesetsDropped(paths);
        e->accept();
    }
}

void TilesetDock::insertTilesetView(int index, Tileset *tileset)
{
    TilesetView *view = new TilesetView(mMapDocument, mZoomable);
    view->setModel(new TilesetModel(tileset, view));

    connect(view->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateCurrentTiles()));

    mTabBar->insertTab(index, tileset->name());
    mViewStack->insertWidget(index, view);
    updateActions();
}

void TilesetDock::updateActions()
{
    bool external = false;
    TilesetView *view = 0;
    const int index = mTabBar->currentIndex();
    if (index > -1) {
        view = tilesetViewAt(index);
        if (view) {
            mViewStack->setCurrentWidget(view);
            external = view->tilesetModel()->tileset()->isExternal();
        }
    }

    mRenameTileset->setEnabled(view && !external);
    mImportTileset->setEnabled(view && external);
    mExportTileset->setEnabled(view && !external);
    mPropertiesTileset->setEnabled(view && !external);
    mDeleteTileset->setEnabled(view);
}

void TilesetDock::updateCurrentTiles()
{
    const int viewIndex = mViewStack->currentIndex();
    if (viewIndex == -1)
        return;

    const QItemSelectionModel *s = tilesetViewAt(viewIndex)->selectionModel();
    const QModelIndexList indexes = s->selection().indexes();

    if (indexes.isEmpty())
        return;

    const QModelIndex &first = indexes.first();
    int minX = first.column();
    int maxX = first.column();
    int minY = first.row();
    int maxY = first.row();

    foreach (const QModelIndex &index, indexes) {
        if (minX > index.column()) minX = index.column();
        if (maxX < index.column()) maxX = index.column();
        if (minY > index.row()) minY = index.row();
        if (maxY < index.row()) maxY = index.row();
    }

    // Create a tile layer from the current selection
    TileLayer *tileLayer = new TileLayer(QString(), 0, 0,
                                         maxX - minX + 1,
                                         maxY - minY + 1);

    const TilesetModel *model = static_cast<const TilesetModel*>(s->model());
    foreach (const QModelIndex &index, indexes) {
        tileLayer->setCell(index.column() - minX,
                           index.row() - minY,
                           Cell(model->tileAt(index)));
    }

    setCurrentTiles(tileLayer);
    setCurrentTile(model->tileAt(s->currentIndex()));
}

void TilesetDock::tilesetChanged(Tileset *tileset)
{
    // Update the affected tileset model
    for (int i = 0; i < mViewStack->count(); ++i) {
        TilesetModel *model = tilesetViewAt(i)->tilesetModel();
        if (model->tileset() == tileset) {
            model->tilesetChanged();
            break;
        }
    }
}

void TilesetDock::tilesetRemoved(Tileset *tileset)
{
    // Delete the related tileset view
    for (int i = 0; i < mViewStack->count(); ++i) {
        TilesetView *view = tilesetViewAt(i);
        if (view->tilesetModel()->tileset() == tileset) {
            mTabBar->removeTab(i);
            delete view;
            break;
        }
    }


    // Make sure we don't reference this tileset anymore
    if (mCurrentTiles) {
        // TODO: Don't clean unnecessarily (but first the concept of
        //       "current brush" would need to be introduced)
        TileLayer *cleaned = static_cast<TileLayer *>(mCurrentTiles->clone());
        cleaned->removeReferencesToTileset(tileset);
        setCurrentTiles(cleaned);
    }
    if (mCurrentTile && mCurrentTile->tileset() == tileset)
        setCurrentTile(0);
}

void TilesetDock::tilesetMoved(int from, int to)
{
    // Move the related tileset views
    QWidget *widget = mViewStack->widget(from);
    mViewStack->removeWidget(widget);
    mViewStack->insertWidget(to, widget);
    mViewStack->setCurrentIndex(mTabBar->currentIndex());

    // Update the titles of the affected tabs
    const int start = qMin(from, to);
    const int end = qMax(from, to);
    for (int i = start; i <= end; ++i) {
        const Tileset *tileset = tilesetViewAt(i)->tilesetModel()->tileset();
        if (mTabBar->tabText(i) != tileset->name())
            mTabBar->setTabText(i, tileset->name());
    }
}

/**
 * Removes the currently selected tileset.
 */
void TilesetDock::removeTileset()
{
    const int currentIndex = mViewStack->currentIndex();
    if (currentIndex != -1)
        removeTileset(mViewStack->currentIndex());
}

/**
 * Removes the tileset at the given index. Prompting the user when the tileset
 * is in use by the map.
 */
void TilesetDock::removeTileset(int index)
{
    Tileset *tileset = tilesetViewAt(index)->tilesetModel()->tileset();
    const bool inUse = mMapDocument->map()->isTilesetUsed(tileset);

    // If the tileset is in use, warn the user and confirm removal
    if (inUse) {
        QMessageBox warning(QMessageBox::Warning,
                            tr("Remove Tileset"),
                            tr("The tileset \"%1\" is still in use by the "
                               "map!").arg(tileset->name()),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
        warning.setDefaultButton(QMessageBox::Yes);
        warning.setInformativeText(tr("Remove this tileset and all references "
                                      "to the tiles in this tileset?"));

        if (warning.exec() != QMessageBox::Yes)
            return;
    }

    QUndoCommand *remove = new RemoveTileset(mMapDocument, index, tileset);
    QUndoStack *undoStack = mMapDocument->undoStack();

    if (inUse) {
        // Remove references to tiles in this tileset from the current map
        undoStack->beginMacro(remove->text());
        foreach (Layer *layer, mMapDocument->map()->layers()) {
            if (TileLayer *tileLayer = layer->asTileLayer()) {
                const QRegion refs = tileLayer->tilesetReferences(tileset);
                if (!refs.isEmpty()) {
                    undoStack->push(new EraseTiles(mMapDocument,
                                                   tileLayer, refs));
                }
            } else if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
                foreach (MapObject *object, objectGroup->objects()) {
                    const Tile *tile = object->tile();
                    if (tile && tile->tileset() == tileset) {
                        undoStack->push(new RemoveMapObject(mMapDocument,
                                                            object));
                    }
                }
            }
        }
    }
    undoStack->push(remove);
    if (inUse)
        undoStack->endMacro();
}

void TilesetDock::moveTileset(int from, int to)
{
    QUndoCommand *command = new MoveTileset(mMapDocument, from, to);
    mMapDocument->undoStack()->push(command);
}

void TilesetDock::setCurrentTiles(TileLayer *tiles)
{
    if (mCurrentTiles == tiles)
        return;

    delete mCurrentTiles;
    mCurrentTiles = tiles;

    emit currentTilesChanged(mCurrentTiles);
}

void TilesetDock::setCurrentTile(Tile *tile)
{
    if (mCurrentTile == tile)
        return;

    mCurrentTile = tile;
    emit currentTileChanged(mCurrentTile);
}

void TilesetDock::retranslateUi()
{
    setWindowTitle(tr("Tilesets"));
    mImportTileset->setText(tr("&Import Tileset"));
    mExportTileset->setText(tr("&Export Tileset As..."));
    mPropertiesTileset->setText(tr("Tile&set Properties"));
    mDeleteTileset->setText(tr("&Remove Tileset"));
    mRenameTileset->setText(tr("Rena&me Tileset"));
}

Tileset *TilesetDock::currentTileset() const
{
    if (QWidget *widget = mViewStack->currentWidget())
        return static_cast<TilesetView *>(widget)->tilesetModel()->tileset();

    return 0;
}

TilesetView *TilesetDock::tilesetViewAt(int index) const
{
    return static_cast<TilesetView *>(mViewStack->widget(index));
}

void TilesetDock::editTilesetProperties()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    PropertiesDialog propertiesDialog(tr("Tileset"),
                                      tileset,
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void TilesetDock::exportTileset()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    const QLatin1String extension(".tsx");
    QString suggestedFileName = QFileInfo(mMapDocument->fileName()).path();
    suggestedFileName += QLatin1Char('/');
    suggestedFileName += tileset->name();
    if (!suggestedFileName.endsWith(extension))
        suggestedFileName.append(extension);

    const QString fileName =
            QFileDialog::getSaveFileName(this, tr("Export Tileset"),
                                         suggestedFileName,
                                         tr("Tiled tileset files (*.tsx)"));
    if (fileName.isEmpty())
        return;

    TmxMapWriter writer;

    if (writer.writeTileset(tileset, fileName)) {
        QUndoCommand *command = new SetTilesetFileName(mMapDocument,
                                                       tileset, fileName);
        mMapDocument->undoStack()->push(command);
    }
}

void TilesetDock::importTileset()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    QUndoCommand *command = new SetTilesetFileName(mMapDocument,
                                                   tileset, QString());
    mMapDocument->undoStack()->push(command);
}

void TilesetDock::renameTileset()
{
    bool ok;
    const QString oldText = mTabBar->tabText(mTabBar->currentIndex());
    QString newText = QInputDialog::getText(this, tr("Rename Tileset"),
                                         tr("New name:"), QLineEdit::Normal,
                                         oldText, &ok);

    if (!ok || newText == oldText)
        return;

    int index = mViewStack->currentIndex();
    TilesetView *view = tilesetViewAt(index);

    RenameTileset *name = new RenameTileset(mMapDocument,
                                            view->tilesetModel()->tileset(),
                                            oldText, newText);
    mMapDocument->undoStack()->push(name);
}

void TilesetDock::tilesetNameChanged(Tileset *tileset)
{
    for (int i = 0; i < mTabBar->count(); i++) {
        TilesetView *view = tilesetViewAt(i);
        if (tileset == view->tilesetModel()->tileset()) {
            mTabBar->setTabText(i, tileset->name());
            return;
        }
    }
}

void TilesetDock::documentCloseRequested(int index)
{
    DocumentManager *documentManager = DocumentManager::instance();
    mCurrentTilesets.remove(documentManager->documents().at(index));
}

void TilesetDock::refreshTilesetMenu()
{
    mTilesetMenu->clear();

    if (mTilesetMenuMapper) {
        mTabBar->disconnect(mTilesetMenuMapper);
        delete mTilesetMenuMapper;
    }

    mTilesetMenuMapper = new QSignalMapper(this);
    connect(mTilesetMenuMapper, SIGNAL(mapped(int)),
            mTabBar, SLOT(setCurrentIndex(int)));

    for (int i = 0; i < mTabBar->count(); ++i) {
        const QString name = mTabBar->tabText(i);
        QAction *action = new QAction(name, this);
        mTilesetMenu->addAction(action);
        connect(action, SIGNAL(triggered()), mTilesetMenuMapper, SLOT(map()));
        mTilesetMenuMapper->setMapping(action, i);
    }
}

#else // ZOMBOID

#include "tilesetdock.h"

#include "addremovemapobject.h"
#include "addremovetileset.h"
#include "documentmanager.h"
#include "erasetiles.h"
#include "map.h"
#include "mapcomposite.h"
#include "mapdocument.h"
#include "mapobject.h"
#include "movetileset.h"
#include "objectgroup.h"
#include "preferences.h"
#include "propertiesdialog.h"
#include "tile.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tilesetmodel.h"
#include "tilesetview.h"
#include "tilesetmanager.h"
#include "tmxmapwriter.h"
#include "utils.h"
#include "zoomable.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

using namespace Tiled;
using namespace Tiled::Internal;

namespace {

/**
 * Used for exporting/importing tilesets.
 *
 * @warning Does not work for tilesets that are shared by multiple maps!
 */
class SetTilesetFileName : public QUndoCommand
{
public:
    SetTilesetFileName(MapDocument *mapDocument,
                       Tileset *tileset,
                       const QString &fileName)
        : mMapDocument(mapDocument)
        , mTileset(tileset)
        , mFileName(fileName)
    {
        if (fileName.isEmpty())
            setText(QCoreApplication::translate("Undo Commands",
                                                "Import Tileset"));
        else
            setText(QCoreApplication::translate("Undo Commands",
                                                "Export Tileset"));
    }

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap()
    {
        QString previousFileName = mTileset->fileName();
        mMapDocument->setTilesetFileName(mTileset, mFileName);
        mFileName = previousFileName;
    }

    MapDocument *mMapDocument;
    Tileset *mTileset;
    QString mFileName;
};

class RenameTileset : public QUndoCommand
{
public:
    RenameTileset(MapDocument *mapDocument,
                  Tileset *tileset,
                  const QString &oldName,
                  const QString &newName)
        : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                                   "Change Tileset name"))
        , mMapDocument(mapDocument)
        , mTileset(tileset)
        , mOldName(oldName)
        , mNewName(newName)
    {
        redo();
    }

    void undo()
    {
        mMapDocument->setTilesetName(mTileset, mOldName);
    }

    void redo()
    {
        mMapDocument->setTilesetName(mTileset, mNewName);
    }

private:
    MapDocument *mMapDocument;
    Tileset *mTileset;
    QString mOldName;
    QString mNewName;
};

} // anonymous namespace

TilesetDock::TilesetDock(QWidget *parent):
    QDockWidget(parent),
    mMapDocument(0),
    mToolBar(new QToolBar),
    mCurrentTileset(0),
    mCurrentTile(0),
    mCurrentTiles(0),
    mZoomable(new Zoomable(this)),
    mZoomComboBox(new QComboBox(this)),
    mActionTilesetUp(new QAction(this)),
    mActionTilesetDown(new QAction(this)),
    mActionSortByName(new QAction(this)),
    mActionNewTileset(new QAction(this)),
    mActionImportTileset(new QAction(this)),
    mActionExportTileset(new QAction(this)),
    mActionPropertiesTileset(new QAction(this)),
    mActionDeleteTileset(new QAction(this)),
    mActionRenameTileset(new QAction(this)),
    mActionSwitchLayer(new QAction(this)),
    mTilesetView(new TilesetView(mZoomable, this)),
    mTilesetNamesView(new QListWidget(this))
{
    setObjectName(QLatin1String("TilesetDock"));

    mTilesetView->setModel(new TilesetModel(0, mTilesetView));

    connect(mTilesetNamesView, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(mTilesetNamesView, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(tilesetItemChanged(QListWidgetItem*)));

    mZoomable->connectToComboBox(mZoomComboBox);

    QHBoxLayout *tilesetToolBarLayout = new QHBoxLayout();
    tilesetToolBarLayout->addWidget(mToolBar, 1);
    tilesetToolBarLayout->addWidget(mZoomComboBox);

    QVBoxLayout *tilesetLayout = new QVBoxLayout();
    tilesetLayout->setSpacing(5);
    tilesetLayout->setMargin(0);
    tilesetLayout->addWidget(mTilesetView);
    tilesetLayout->addLayout(tilesetToolBarLayout);

    mActionNewTileset->setIcon(QIcon(QLatin1String(":/images/16x16/document-new.png")));
    mActionImportTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-import.png")));
    mActionExportTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-export.png")));
    mActionPropertiesTileset->setIcon(QIcon(QLatin1String(":images/16x16/document-properties.png")));
    mActionDeleteTileset->setIcon(QIcon(QLatin1String(":images/16x16/edit-delete.png")));
    mActionRenameTileset->setIcon(QIcon(QLatin1String(":images/16x16/edit-rename.png")));

    Utils::setThemeIcon(mActionNewTileset, "document-new");
    Utils::setThemeIcon(mActionImportTileset, "document-import");
    Utils::setThemeIcon(mActionExportTileset, "document-export");
    Utils::setThemeIcon(mActionPropertiesTileset, "document-properties");
    Utils::setThemeIcon(mActionDeleteTileset, "edit-delete");
    Utils::setThemeIcon(mActionRenameTileset, "edit-rename");

    connect(mActionNewTileset, SIGNAL(triggered()), parent, SLOT(newTileset()));
    connect(mActionImportTileset, SIGNAL(triggered()), SLOT(importTileset()));
    connect(mActionExportTileset, SIGNAL(triggered()), SLOT(exportTileset()));
    connect(mActionPropertiesTileset, SIGNAL(triggered()), SLOT(editTilesetProperties()));
    connect(mActionDeleteTileset, SIGNAL(triggered()), SLOT(removeTileset()));
    connect(mActionRenameTileset, SIGNAL(triggered()), SLOT(renameTileset()));

    mIconTileLayer = QIcon(QLatin1String(":/images/16x16/layer-tile.png"));
    mIconTileLayerStop = QIcon(QLatin1String(":/images/16x16/layer-tile-stop.png"));
    mActionSwitchLayer->setCheckable(true);
    bool enabled = Preferences::instance()->autoSwitchLayer();
    mActionSwitchLayer->setChecked(enabled == false);
    mActionSwitchLayer->setIcon(enabled ? mIconTileLayer : mIconTileLayerStop);
    connect(mActionSwitchLayer, SIGNAL(toggled(bool)),
            SLOT(layerSwitchToggled()));
    connect(Preferences::instance(), SIGNAL(autoSwitchLayerChanged(bool)),
            SLOT(autoSwitchLayerChanged(bool)));

    mToolBar->setIconSize(QSize(16, 16));

    mToolBar->addAction(mActionNewTileset);
    mToolBar->addAction(mActionImportTileset);
    mToolBar->addAction(mActionExportTileset);
    mToolBar->addAction(mActionPropertiesTileset);
    mToolBar->addAction(mActionDeleteTileset);
    mToolBar->addAction(mActionRenameTileset);
    mToolBar->addAction(mActionSwitchLayer);

    connect(mTilesetView->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(updateCurrentTiles()));

    mZoomable->setZoomFactors(QVector<qreal>() << 0.25 << 0.5 << 0.75 << 1.0
                              << 1.25 << 1.5 << 1.75 << 2.0);
    mZoomable->blockSignals(true);
    mZoomable->setScale(Preferences::instance()->tilesetScale());
    mZoomable->blockSignals(false);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)), Preferences::instance(),
            SLOT(setTilesetScale(qreal)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    connect(DocumentManager::instance(), SIGNAL(documentAboutToClose(int,MapDocument*)),
            SLOT(documentAboutToClose(int,MapDocument*)));

    connect(this, SIGNAL(currentTileChanged(Tile*)), SLOT(switchLayerForTile(Tile*)));

    mActionTilesetUp->setIcon(QIcon(QLatin1String(":/images/16x16/go-up.png")));
    mActionTilesetDown->setIcon(QIcon(QLatin1String(":/images/16x16/go-down.png")));

    connect(mActionTilesetUp, SIGNAL(triggered()), SLOT(moveTilesetUp()));
    connect(mActionTilesetDown, SIGNAL(triggered()), SLOT(moveTilesetDown()));

    Utils::setThemeIcon(mActionTilesetUp, "go-up");
    Utils::setThemeIcon(mActionTilesetDown, "go-down");

    mActionSortByName->setIcon(QIcon(QLatin1String(":/images/16x16/sort-by-name.png")));
    mActionSortByName->setCheckable(true);
    mActionSortByName->setChecked(Preferences::instance()->sortTilesets());

    connect(mActionSortByName, SIGNAL(triggered()), SLOT(sortByName()));

    QVBoxLayout *tilesetNamesLayout = new QVBoxLayout();
    tilesetNamesLayout->setSpacing(5);
    tilesetNamesLayout->setMargin(0);
    QToolBar *toolbar = new QToolBar();
    toolbar->setIconSize(QSize(16, 16));
    toolbar->addAction(mActionTilesetUp);
    toolbar->addAction(mActionTilesetDown);
    toolbar->addAction(mActionSortByName);
    QToolButton *button;
    button = dynamic_cast<QToolButton*>(toolbar->widgetForAction(mActionTilesetUp));
    button->setAutoRepeat(true);
    button = dynamic_cast<QToolButton*>(toolbar->widgetForAction(mActionTilesetDown));
    button->setAutoRepeat(true);

    tilesetNamesLayout->addWidget(mTilesetNamesView);
    tilesetNamesLayout->addWidget(toolbar);

    QWidget *outer = new QWidget(this);
    QHBoxLayout *outerLayout = new QHBoxLayout(outer);
    outerLayout->setSpacing(5);
    outerLayout->setMargin(5);
    outerLayout->addLayout(tilesetNamesLayout);
    outerLayout->addLayout(tilesetLayout);
    setWidget(outer);

    retranslateUi();
    setAcceptDrops(true);
    updateActions();
}

TilesetDock::~TilesetDock()
{
    delete mCurrentTiles;
}

void TilesetDock::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    setCurrentTiles(0);
    setCurrentTile(0);

    if (mMapDocument) {
        if (mCurrentTileset)
            mCurrentTilesets.insert(mMapDocument, mCurrentTileset->name());
        mMapDocument->disconnect(this);
    }

    mMapDocument = mapDocument;
    mTilesetView->setMapDocument(mapDocument);

    setTilesetNamesList();

    if (mMapDocument) {
        connect(mMapDocument, SIGNAL(tilesetAdded(int,Tileset*)),
                SLOT(tilesetAdded(int,Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetRemoved(Tileset*)),
                SLOT(tilesetRemoved(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetMoved(int,int)),
                SLOT(tilesetMoved(int,int)));
        connect(mMapDocument, SIGNAL(tilesetNameChanged(Tileset*)),
                SLOT(tilesetNameChanged(Tileset*)));
        connect(mMapDocument, SIGNAL(tilesetFileNameChanged(Tileset*)),
                SLOT(updateActions()));

        QString cacheName = mCurrentTilesets.take(mMapDocument);
        if (mTilesetByName.contains(cacheName)) {
            int row = mTilesets.indexOf(mTilesetByName[cacheName]);
            mTilesetNamesView->setCurrentRow(row);
        }
    }

    updateActions();
}

void TilesetDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TilesetDock::dragEnterEvent(QDragEnterEvent *e)
{
    const QList<QUrl> urls = e->mimeData()->urls();
    if (!urls.isEmpty() && !urls.at(0).toLocalFile().isEmpty())
        e->accept();
}

void TilesetDock::dropEvent(QDropEvent *e)
{
    QStringList paths;
    foreach (const QUrl &url, e->mimeData()->urls()) {
        const QString localFile = url.toLocalFile();
        if (!localFile.isEmpty())
            paths.append(localFile);
    }
    if (!paths.isEmpty()) {
        emit tilesetsDropped(paths);
        e->accept();
    }
}

void TilesetDock::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (mMapDocument && (row >= 0))
        mCurrentTileset = mTilesets[row];

    setTilesetList();
    updateCurrentTiles();
    updateActions();
}

void TilesetDock::tilesetItemChanged(QListWidgetItem *item)
{
    int row = mTilesetNamesView->row(item);
    Tileset *tileset = mTilesets.at(row);
    if (item->text() != tileset->name()) {
        mMapDocument->undoStack()->push(new RenameTileset(mMapDocument, tileset,
                                                          tileset->name(),
                                                          item->text()));
    }
}

void TilesetDock::tilesetAdded(int index, Tileset *tileset)
{
    Q_UNUSED(index)
    setTilesetNamesList();
    int row = mTilesets.indexOf(tileset);
    mTilesetNamesView->setCurrentRow(row);
    currentTilesetChanged(row); // in case the row didn't change
}

void TilesetDock::updateActions()
{
    bool curTileset = mCurrentTileset != 0;
    bool external = curTileset && mCurrentTileset->isExternal();

    mActionNewTileset->setEnabled(mMapDocument != 0);
    mActionRenameTileset->setEnabled(curTileset && !external);
    mActionImportTileset->setEnabled(curTileset && external);
    mActionExportTileset->setEnabled(curTileset && !external);
    mActionPropertiesTileset->setEnabled(curTileset && !external);
    mActionDeleteTileset->setEnabled(curTileset);

    bool sort = Preferences::instance()->sortTilesets();
    mActionTilesetUp->setEnabled(!sort && curTileset && (mCurrentTileset != mTilesets.first()));
    mActionTilesetDown->setEnabled(!sort && curTileset && (mCurrentTileset != mTilesets.last()));

    mZoomComboBox->setEnabled(curTileset != 0);
}

void TilesetDock::updateCurrentTiles()
{
    if (!mCurrentTileset)
        return;

    const QItemSelectionModel *s = mTilesetView->selectionModel();
    const QModelIndexList indexes = s->selection().indexes();

    if (indexes.isEmpty())
        return;

    const QModelIndex &first = indexes.first();
    int minX = first.column();
    int maxX = first.column();
    int minY = first.row();
    int maxY = first.row();

    foreach (const QModelIndex &index, indexes) {
        if (minX > index.column()) minX = index.column();
        if (maxX < index.column()) maxX = index.column();
        if (minY > index.row()) minY = index.row();
        if (maxY < index.row()) maxY = index.row();
    }

    // Create a tile layer from the current selection
    TileLayer *tileLayer = new TileLayer(QString(), 0, 0,
                                         maxX - minX + 1,
                                         maxY - minY + 1);

    const TilesetModel *model = static_cast<const TilesetModel*>(s->model());
    foreach (const QModelIndex &index, indexes) {
        tileLayer->setCell(index.column() - minX,
                           index.row() - minY,
                           Cell(model->tileAt(index)));
    }

    setCurrentTiles(tileLayer);
    setCurrentTile(model->tileAt(s->currentIndex()));

    // Hack - copy tile name to the clipboard
    if (mCurrentTile) {
        QString tileName = QFileInfo(mCurrentTile->tileset()->imageSource()).baseName() +
                QLatin1Char('_') + QString::number(mCurrentTile->id());
        qApp->clipboard()->setText(tileName);
    }
}

void TilesetDock::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        mTilesetView->tilesetModel()->tilesetChanged();

    if (mTilesets.contains(tileset)) {
        int row = mTilesets.indexOf(tileset);
        if (QListWidgetItem *item = mTilesetNamesView->item(row))
            item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
    }
}

void TilesetDock::tilesetRemoved(Tileset *tileset)
{
    // Make sure we don't reference this tileset anymore
    if (mCurrentTiles) {
        // TODO: Don't clean unnecessarily (but first the concept of
        //       "current brush" would need to be introduced)
        TileLayer *cleaned = static_cast<TileLayer *>(mCurrentTiles->clone());
        cleaned->removeReferencesToTileset(tileset);
        setCurrentTiles(cleaned);
    }
    if (mCurrentTile && mCurrentTile->tileset() == tileset)
        setCurrentTile(0);

    int row = mTilesetNamesView->currentRow();
    setTilesetNamesList();
    mTilesetNamesView->setCurrentRow(qBound(0, row, mTilesets.size() - 1));
    if (mTilesetNamesView->currentRow() == row)
        currentTilesetChanged(row);
}

void TilesetDock::tilesetMoved(int from, int to)
{
    Q_UNUSED(from)
    if (Preferences::instance()->sortTilesets())
        return;
    setTilesetNamesList();
    mTilesetNamesView->setCurrentRow(to);
}

/**
 * Removes the currently selected tileset.
 */
void TilesetDock::removeTileset()
{
    if (mCurrentTileset)
        removeTileset(mMapDocument->map()->tilesets().indexOf(mCurrentTileset));
}

/**
 * Removes the tileset at the given index. Prompting the user when the tileset
 * is in use by the map.
 */
void TilesetDock::removeTileset(int index)
{
    Tileset *tileset = mMapDocument->map()->tilesets()[index];
    const bool inUse = mMapDocument->map()->isTilesetUsed(tileset);

    // If the tileset is in use, warn the user and confirm removal
    if (inUse) {
        QMessageBox warning(QMessageBox::Warning,
                            tr("Remove Tileset"),
                            tr("The tileset \"%1\" is still in use by the "
                               "map!").arg(tileset->name()),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
        warning.setDefaultButton(QMessageBox::Yes);
        warning.setInformativeText(tr("Remove this tileset and all references "
                                      "to the tiles in this tileset?"));

        if (warning.exec() != QMessageBox::Yes)
            return;
    }

    QUndoCommand *remove = new RemoveTileset(mMapDocument, index, tileset);
    QUndoStack *undoStack = mMapDocument->undoStack();

    if (inUse) {
        // Remove references to tiles in this tileset from the current map
        undoStack->beginMacro(remove->text());
        foreach (Layer *layer, mMapDocument->map()->layers()) {
            if (TileLayer *tileLayer = layer->asTileLayer()) {
                const QRegion refs = tileLayer->tilesetReferences(tileset);
                if (!refs.isEmpty()) {
                    undoStack->push(new EraseTiles(mMapDocument,
                                                   tileLayer, refs));
                }
            } else if (ObjectGroup *objectGroup = layer->asObjectGroup()) {
                foreach (MapObject *object, objectGroup->objects()) {
                    const Tile *tile = object->tile();
                    if (tile && tile->tileset() == tileset) {
                        undoStack->push(new RemoveMapObject(mMapDocument,
                                                            object));
                    }
                }
            }
        }
    }
    undoStack->push(remove);
    if (inUse)
        undoStack->endMacro();
}

void TilesetDock::moveTileset(int from, int to)
{
    QUndoCommand *command = new MoveTileset(mMapDocument, from, to);
    mMapDocument->undoStack()->push(command);
}

void TilesetDock::setCurrentTiles(TileLayer *tiles)
{
    if (mCurrentTiles == tiles)
        return;

    delete mCurrentTiles;
    mCurrentTiles = tiles;

    emit currentTilesChanged(mCurrentTiles);
}

void TilesetDock::setCurrentTile(Tile *tile)
{
    if (mCurrentTile == tile)
        return;

    mCurrentTile = tile;
    emit currentTileChanged(mCurrentTile);
}

void TilesetDock::retranslateUi()
{
    setWindowTitle(tr("Tilesets"));

    mActionTilesetUp->setText(tr("Move Tileset Up"));
    mActionTilesetDown->setText(tr("Move Tileset Down"));
    mActionSortByName->setText(tr("Sort By Name"));

    mActionNewTileset->setText(tr("New Tileset"));
    mActionImportTileset->setText(tr("&Import Tileset"));
    mActionExportTileset->setText(tr("&Export Tileset As..."));
    mActionPropertiesTileset->setText(tr("Tile&set Properties"));
    mActionDeleteTileset->setText(tr("&Remove Tileset"));
    mActionRenameTileset->setText(tr("Rena&me Tileset"));

    bool enabled = Preferences::instance()->autoSwitchLayer();
    QString text = enabled ? tr("Layer Switch Enabled")
                           : tr("Layer Switch Disabled");
    mActionSwitchLayer->setText(text);
}

Tileset *TilesetDock::currentTileset() const
{
    return mCurrentTileset;
}

void TilesetDock::editTilesetProperties()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    PropertiesDialog propertiesDialog(tr("Tileset"),
                                      tileset,
                                      mMapDocument->undoStack(),
                                      this);
    propertiesDialog.exec();
}

void TilesetDock::exportTileset()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    const QLatin1String extension(".tsx");
    QString suggestedFileName = QFileInfo(mMapDocument->fileName()).path();
    suggestedFileName += QLatin1Char('/');
    suggestedFileName += tileset->name();
    if (!suggestedFileName.endsWith(extension))
        suggestedFileName.append(extension);

    const QString fileName =
            QFileDialog::getSaveFileName(this, tr("Export Tileset"),
                                         suggestedFileName,
                                         tr("Tiled tileset files (*.tsx)"));
    if (fileName.isEmpty())
        return;

    TmxMapWriter writer;

    if (writer.writeTileset(tileset, fileName)) {
        QUndoCommand *command = new SetTilesetFileName(mMapDocument,
                                                       tileset, fileName);
        mMapDocument->undoStack()->push(command);
    }
}

void TilesetDock::importTileset()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    QUndoCommand *command = new SetTilesetFileName(mMapDocument,
                                                   tileset, QString());
    mMapDocument->undoStack()->push(command);
}

void TilesetDock::renameTileset()
{
    Tileset *tileset = currentTileset();
    if (!tileset)
        return;

    bool ok;
    const QString oldText = tileset->name();
    QString newText = QInputDialog::getText(this, tr("Rename Tileset"),
                                         tr("New name:"), QLineEdit::Normal,
                                         oldText, &ok);

    if (!ok || newText == oldText)
        return;

    mMapDocument->undoStack()->push(new RenameTileset(mMapDocument, tileset,
                                                      oldText, newText));
}

void TilesetDock::tilesetNameChanged(Tileset *tileset)
{
    int index = mTilesets.indexOf(tileset);
    if (index >= 0)
        mTilesetNamesView->item(index)->setText(tileset->name());
}

void TilesetDock::documentAboutToClose(int index, MapDocument *mapDocument)
{
    Q_UNUSED(index)
    mCurrentTilesets.remove(mapDocument);
}

void TilesetDock::layerSwitchToggled()
{
    bool checked = mActionSwitchLayer->isChecked();
    Preferences::instance()->setAutoSwitchLayer(checked == false);
}

void TilesetDock::autoSwitchLayerChanged(bool enabled)
{
    mActionSwitchLayer->setIcon(enabled ? mIconTileLayer : mIconTileLayerStop);
    QString text = enabled ? tr("Layer Switch Enabled") : tr("Layer Switch Disabled");
//    mActionSwitchLayer->setToolTip(text);
    mActionSwitchLayer->setText(text);
    mActionSwitchLayer->setChecked(enabled == false);
}

void TilesetDock::switchLayerForTile(Tile *tile)
{
    if (!tile)
        return;
    if (Preferences::instance()->autoSwitchLayer() == false)
        return;
    QString layerName = TilesetManager::instance()->layerName(tile);
    if (!layerName.isEmpty()) {
        Layer *currentLayer = mMapDocument->currentLayer();
        if (TileLayer *tl = currentLayer->asTileLayer()) { // FIXME: ObjectGroup too?
            if (tl->group()) {
                foreach (TileLayer *tl1, tl->group()->layers()) {
                    QString name = MapComposite::layerNameWithoutPrefix(tl1);
                    if (name == layerName) {
                        int layerIndex = mMapDocument->map()->layers().indexOf(tl1);
                        if (layerIndex != mMapDocument->currentLayerIndex())
                            mMapDocument->setCurrentLayerIndex(layerIndex);
                        return;
                    }
                }
            }
        }
    }
}

void TilesetDock::setTilesetNamesList()
{
    mCurrentTileset = 0;
    mTilesets.clear();
    mTilesetByName.clear();
    mTilesetNamesView->clear();

    if (mMapDocument && mMapDocument->map()->tilesets().size()) {
        if (Preferences::instance()->sortTilesets()) {
            foreach (Tileset *ts, mMapDocument->map()->tilesets())
                mTilesetByName.insertMulti(ts->name(), ts);
            mTilesets = mTilesetByName.values();
        } else {
            mTilesets = mMapDocument->map()->tilesets();
            foreach (Tileset *ts, mTilesets)
                mTilesetByName.insertMulti(ts->name(), ts);
        }

        int maxWidth = 64;
        QFontMetrics fm = mTilesetNamesView->fontMetrics(); // FIXME: same font used by QPainter?
        foreach (Tileset *ts, mTilesets) {
            QListWidgetItem *item = new QListWidgetItem(ts->name());
            if (ts->isMissing())
                item->setForeground(Qt::red);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            mTilesetNamesView->addItem(item);
            maxWidth = qMax(maxWidth, fm.width(ts->name()));
        }
        mTilesetNamesView->setFixedWidth(maxWidth + 16 +
            mTilesetNamesView->verticalScrollBar()->sizeHint().width());
    }
}

void TilesetDock::setTilesetList()
{
    mTilesetView->tilesetModel()->setTileset(mCurrentTileset);
}

void TilesetDock::moveTilesetUp()
{
    int from = mTilesetNamesView->currentRow();
    if (from > 0) {
        moveTileset(from, from-1);
    }
}

void TilesetDock::moveTilesetDown()
{
    int from = mTilesetNamesView->currentRow();
    if (from + 1 < mTilesetNamesView->count()) {
        moveTileset(from, from+1);
    }
}

void TilesetDock::sortByName()
{
    bool sort = mActionSortByName->isChecked();
    Preferences::instance()->setSortTilesets(sort);
    Tileset *ts = mCurrentTileset;
    setTilesetNamesList();
    int row = mTilesets.indexOf(ts);
    mTilesetNamesView->setCurrentRow(row);
    updateActions();
}

#endif // ZOMBOID
