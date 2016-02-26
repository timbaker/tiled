/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "tiledefdialog.h"
#include "ui_tiledefdialog.h"

#include "addtilesetsdialog.h"
#include "preferences.h"
#include "tiledeffile.h"
#include "tilesetmanager.h"
#include "zoomable.h"
#include "utils.h"
#include "virtualtileset.h"
#include "zprogress.h"

#include "BuildingEditor/listofstringsdialog.h"
#include "BuildingEditor/buildingtiles.h"

#include "tile.h"
#include "tileset.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>
#include <QUrl>

#ifdef QT_NO_DEBUG
inline QNoDebug noise() { return QNoDebug(); }
#else
inline QDebug noise() { return QDebug(QtDebugMsg); }
#endif

using namespace Tiled;
using namespace Tiled::Internal;

/////

class Tiled::Internal::TilePropertyClipboard
{
public:
    ~TilePropertyClipboard();

    struct Entry
    {
        Entry()
        {}
        UIProperties mProperties;
    };

    void clear();
    void resize(int width, int height);
    void setEntry(int x, int y, UIProperties &properties);
    UIProperties *entry(int tx, int ty, int x, int y);

    int mWidth, mHeight;
    QVector<Entry*> mEntries;
    QRegion mValidRgn;
};

TilePropertyClipboard::~TilePropertyClipboard()
{
    qDeleteAll(mEntries);
}

void TilePropertyClipboard::clear()
{
    qDeleteAll(mEntries);
    mEntries.resize(0);
    mValidRgn = QRegion();
    mWidth = mHeight = 0;
}

void TilePropertyClipboard::resize(int width, int height)
{
    clear();

    mWidth = width, mHeight = height;
    mEntries.resize(mWidth * mHeight);
    for (int i = 0; i < mEntries.size(); i++) {
        if (!mEntries[i])
            mEntries[i] = new Entry;
    }
    mValidRgn = QRegion();
}

void TilePropertyClipboard::setEntry(int x, int y, UIProperties &properties)
{
    mEntries[x + y * mWidth]->mProperties.copy(properties);
    mValidRgn |= QRect(x, y, 1, 1);
}

UIProperties *TilePropertyClipboard::entry(int tx, int ty, int x, int y)
{
    // Support tiling the clipboard contents over the area we are pasting to.
    while (x - tx >= mWidth)
        tx += mWidth;
    while (y - ty >= mHeight)
        ty += mHeight;

    if (QRect(tx, ty, mWidth, mHeight).contains(x, y)) {
        x -= tx, y -= ty;
        if (mValidRgn.contains(QPoint(x, y)))
            return &mEntries[x + y * mWidth]->mProperties;
    }
    return 0;
}

/////

namespace TileDefUndoRedo {

class AddTileset : public QUndoCommand
{
public:
    AddTileset(TileDefDialog *d, int index, Tileset *tileset, TileDefTileset *defTileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tileset")),
        mDialog(d),
        mIndex(index),
        mTileset(tileset),
        mDefTileset(defTileset)
    {
    }

    void undo()
    {
        mDialog->removeTileset(mIndex, &mTileset, &mDefTileset);
    }

    void redo()
    {
        mDialog->insertTileset(mIndex, mTileset, mDefTileset);
        mTileset = 0;
        mDefTileset = 0;
    }

    TileDefDialog *mDialog;
    int mIndex;
    Tileset *mTileset;
    TileDefTileset *mDefTileset;
};

class RemoveTileset : public QUndoCommand
{
public:
    RemoveTileset(TileDefDialog *d, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tileset")),
        mDialog(d),
        mIndex(index),
        mTileset(0),
        mDefTileset(0)
    {
    }

    void undo()
    {
        mDialog->insertTileset(mIndex, mTileset, mDefTileset);
        mTileset = 0;
        mDefTileset = 0;
    }

    void redo()
    {
        mDialog->removeTileset(mIndex, &mTileset, &mDefTileset);
    }

    TileDefDialog *mDialog;
    int mIndex;
    Tileset *mTileset;
    TileDefTileset *mDefTileset;
};

class ChangePropertyValue : public QUndoCommand
{
public:
    ChangePropertyValue(TileDefDialog *d, TileDefTile *defTile,
                        UIProperties::UIProperty *property,
                        const QVariant &value) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Property Value")),
        mDialog(d),
        mTile(defTile),
        mProperty(property),
        mValue(value)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mValue = mDialog->changePropertyValue(mTile, mProperty->mName, mValue);
    }

    TileDefDialog *mDialog;
    TileDefTile *mTile;
    UIProperties::UIProperty *mProperty;
    QVariant mValue;
};

} // namespace

using namespace TileDefUndoRedo;

TileDefDialog *TileDefDialog::mInstance = 0;

TileDefDialog *TileDefDialog::instance()
{
    if (!mInstance)
        mInstance = new TileDefDialog;
    return mInstance;
}

void TileDefDialog::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

bool TileDefDialog::closeYerself()
{
    if (mInstance) {
        return mInstance->close();
    }
    return true;
}

TileDefDialog::TileDefDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileDefDialog),
    mCurrentTileset(0),
    mCurrentDefTileset(0),
    mZoomable(new Zoomable(this)),
    mSynching(false),
    mTilesetsUpdatePending(false),
    mPropertyPageUpdatePending(false),
    mTileDefFile(0),
    mTileDefProperties(&TilePropertyMgr::instance()->properties()),
    mClipboard(new TilePropertyClipboard),
    mTilesetHistoryIndex(0),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    ui->statusbar->hide();

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
    undoAction->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->buttonLayout->insertWidget(0, button);
    connect(mUndoGroup, SIGNAL(canUndoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), undoAction, SIGNAL(triggered()));

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(redoAction->text());
    button->setEnabled(mUndoGroup->canRedo());
    redoAction->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->buttonLayout->insertWidget(1, button);
    connect(mUndoGroup, SIGNAL(canRedoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), redoAction, SIGNAL(triggered()));

    connect(mUndoGroup, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(cleanChanged(bool)), SLOT(updateUI()));

    /////

    QToolBar *toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionCopyProperties);
    toolBar->addAction(ui->actionPasteProperties);
    toolBar->addAction(ui->actionReset);
    ui->tileToolsLayout->insertWidget(0, toolBar);

    ui->actionCopyProperties->setShortcut(QKeySequence::Copy);
    ui->actionPasteProperties->setShortcut(QKeySequence::Paste);
    ui->actionReset->setShortcut(QKeySequence::Delete);

    connect(ui->actionCopyProperties, SIGNAL(triggered()), SLOT(copyProperties()));
    connect(ui->actionPasteProperties, SIGNAL(triggered()), SLOT(pasteProperties()));
    connect(ui->actionReset, SIGNAL(triggered()), SLOT(resetDefaults()));

    /////

    toolBar = new QToolBar;
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionGoBack);
    toolBar->addAction(ui->actionGoForward);
    toolBar->addSeparator();
    toolBar->addAction(ui->actionAddTileset);
    toolBar->addAction(ui->actionRemoveTileset);
    ui->toolBarLayout->insertWidget(0, toolBar);

    connect(ui->actionGoBack, SIGNAL(triggered()), SLOT(goBack()));
    connect(ui->actionGoForward, SIGNAL(triggered()), SLOT(goForward()));
    connect(ui->actionRemoveTileset, SIGNAL(triggered()), SLOT(removeTileset()));

    /////

    QAction *a = ui->menuEdit->actions().first();
    ui->menuEdit->insertAction(a, undoAction);
    ui->menuEdit->insertAction(a, redoAction);

    ui->splitter->setStretchFactor(0, 1);

    mZoomable->setScale(0.5);
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);

    ui->tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tiles->model()->setShowHeaders(false);
    ui->tiles->model()->setShowLabels(true);
    ui->tiles->model()->setHighlightLabelledItems(true);

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(ui->tiles, SIGNAL(tileEntered(QModelIndex)), SLOT(tileEntered(QModelIndex)));
    connect(ui->tiles, SIGNAL(tileLeft(QModelIndex)), SLOT(tileLeft(QModelIndex)));

    ui->actionNew->setShortcut(QKeySequence::New);
    ui->actionOpen->setShortcut(QKeySequence::Open);
    ui->actionSave->setShortcut(QKeySequence::Save);
    ui->actionSaveAs->setShortcut(QKeySequence::SaveAs);

    connect(ui->actionNew, SIGNAL(triggered()), SLOT(fileNew()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(fileOpen()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(fileSaveAs()));
    connect(ui->actionAddTileset, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionTilesDirectory, SIGNAL(triggered()), SLOT(chooseTilesDirectory()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

    connect(ui->actionUserGuide, SIGNAL(triggered()), SLOT(help()));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    foreach (QObject *o, ui->propertySheet->children())
        if (o->isWidgetType())
            delete o;
    delete ui->propertySheet->layout();
    QFormLayout *form = new QFormLayout(ui->propertySheet);
    ui->propertySheet->setLayout(form);
    form->setRowWrapPolicy(QFormLayout::DontWrapRows);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    foreach (TileDefProperty *prop, mTileDefProperties->mProperties) {
        if (mTileDefProperties->mSeparators.contains(form->rowCount())) {
            QFrame *w = new QFrame(ui->propertySheet);
            w->setObjectName(QString::fromUtf8("line"));
            w->setFrameShape(QFrame::HLine);
            w->setFrameShadow(QFrame::Sunken);
            form->setWidget(form->rowCount(), QFormLayout::SpanningRole, w);
        }
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            QCheckBox *w = new QCheckBox(p->mName, ui->propertySheet);
            w->setObjectName(p->mName);
            connect(w, SIGNAL(toggled(bool)), SLOT(checkboxToggled(bool)));
            mCheckBoxes[p->mName] = w;
            form->setWidget(form->rowCount(), QFormLayout::SpanningRole, w);
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            QSpinBox *w = new QSpinBox(ui->propertySheet);
            w->setObjectName(p->mName);
            w->setRange(p->mMin, p->mMax);
            w->setMinimumWidth(96);
            w->installEventFilter(this); // to disable mousewheel
            connect(w, SIGNAL(valueChanged(int)), SLOT(spinBoxValueChanged(int)));
            mSpinBoxes[p->mName] = w;
            form->addRow(p->mName, w);
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            QComboBox *w = new QComboBox(ui->propertySheet);
            w->setObjectName(p->mName);
            w->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            w->setEditable(true);
            w->setInsertPolicy(QComboBox::InsertAlphabetically);
            w->installEventFilter(this); // to disable mousewheel
            connect(w->lineEdit(), SIGNAL(editingFinished()), SLOT(stringEdited()));
            mComboBoxes[p->mName] = w;
            form->addRow(p->mName, w);
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            QComboBox *w = new QComboBox(ui->propertySheet);
            w->setObjectName(p->mName);
            w->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            w->addItems(p->mEnums);
            w->installEventFilter(this); // to disable mousewheel
            connect(w, SIGNAL(activated(int)), SLOT(comboBoxActivated(int)));
            mComboBoxes[p->mName] = w;
            form->addRow(p->mName, w);
            continue;
        }
    }

#if 0
    // Hack - force the tileset-names-list font to be updated now, because
    // setTilesetList() uses its font metrics to determine the maximum item
    // width.
    ui->tilesets->setFont(QFont());
    setTilesetList();
#endif

    QLabel label;
    mLabelFont = label.font();
    mBoldLabelFont = mLabelFont;
    mBoldLabelFont.setBold(true);

    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    qreal scale = settings.value(QLatin1String("TileScale"), 0.5f).toReal();
    mZoomable->setScale(scale);
    mCurrentTilesetName = settings.value(QLatin1String("CurrentTileset")).toString();
    settings.endGroup();

    restoreSplitterSizes(ui->splitter);

    ui->propertySheet->setEnabled(false);
#if 0
    fileOpen(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\maptools\\tiledefinitions.tiles"));
    initStringComboBoxValues();
    updateTilesetListLater();
#endif
    updateUI();
}

TileDefDialog::~TileDefDialog()
{
    delete ui;

    TilesetManager::instance()->removeReferences(mTilesets);
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
    qDeleteAll(mRemovedDefTilesets);
    delete mTileDefFile;
    delete mClipboard;
    //delete mTileDefProperties;

    mInstance = 0;
}

void TileDefDialog::addTileset()
{ 
    AddTilesetsDialog dialog(tilesDir(),
                             mTileDefFile->tilesetNames(),
                             false,
                             this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    mUndoStack->beginMacro(tr("Add Tilesets"));

    foreach (QString fileName, dialog.fileNames()) {
        if (Tiled::Tileset *ts = loadTileset(fileName)) {
            TileDefTileset *defTileset = new TileDefTileset(ts);
            defTileset->mID = uniqueTilesetID();
            mUndoStack->push(new AddTileset(this, mTilesets.size(), ts, defTileset));
        } else {
            QMessageBox::warning(this, tr("It's no good, Jim!"), mError);
        }
    }

    mUndoStack->endMacro();
}

void TileDefDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->tilesets->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->tilesets->row(item);
        Tileset *tileset = this->tileset(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?\nYou will lose all the properties for this tileset!")
                                  .arg(tileset->name()),
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        mUndoStack->push(new RemoveTileset(this, mTilesets.indexOf(tileset)));
    }
}

void TileDefDialog::chooseTilesDirectory()
{
    if (!mTileDefFile)
        return;

    QString f = QFileDialog::getExistingDirectory(this,
                                                  tr("Choose Tiles Directory"),
                                                  tilesDir());
    if (f.isEmpty())
        return;

    setTilesDir(f);
    tilesDirChanged();
    updateTilesetListLater();
}

void TileDefDialog::insertTileset(int index, Tileset *ts, TileDefTileset *defTileset)
{
    Q_ASSERT(!mTilesets.contains(ts));
    Q_ASSERT(!mTilesetByName.contains(ts->name()));
    Q_ASSERT(ts->name() == defTileset->mName);

    mTileDefFile->insertTileset(index, defTileset);
    mRemovedDefTilesets.removeOne(defTileset);

    mTilesets.insert(index, ts);
    mTilesetByName[ts->name()] = ts;
    if (!mRemovedTilesets.contains(ts))
        TilesetManager::instance()->addReference(ts);
    mRemovedTilesets.removeOne(ts);

    mCurrentTilesetName = ts->name(); // to be highlighted later
    updateTilesetListLater();
}

void TileDefDialog::removeTileset(int index, Tileset **tsPtr, TileDefTileset **defTilesetPtr)
{
    TileDefTileset *defTileset = mTileDefFile->removeTileset(index);
    mRemovedDefTilesets += defTileset;

    int row = rowOf(defTileset->mName);

    Tileset *ts = mTilesets.takeAt(index);
    mTilesetByName.remove(ts->name());
    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += ts;

    Q_ASSERT(ts->name() == defTileset->mName);

    if (tsPtr) *tsPtr = ts;
    if (defTilesetPtr) *defTilesetPtr = defTileset;

    mTilesetHistory.removeAll(ts->name());
    mTilesetHistoryIndex = qBound(0, mTilesetHistoryIndex, mTilesetHistory.size() - 1);

    row = qBound(0, row, mTilesets.size() - 1);
    if (row < mTilesets.size())
        mCurrentTilesetName = tilesetNames().at(row); // to be highlighted later
    else
        mCurrentTilesetName.clear(); // No more tilesets

    updateTilesetListLater();
}

QVariant TileDefDialog::changePropertyValue(TileDefTile *defTile, const QString &name,
                                            const QVariant &value)
{
    QVariant old = defTile->mPropertyUI.mProperties[name]->value();
    defTile->mPropertyUI.ChangePropertiesV(name, value);
    if (mCurrentDefTileset == defTile->tileset()) {
        setToolTipEtc(defTile->id());
        ui->tiles->update(ui->tiles->model()->index((void*)defTile));
        updatePropertyPageLater();
    }
    return old;
}

void TileDefDialog::fileNew()
{
    if (!confirmSave())
        return;

    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return;

    clearDocument();

    mTileDefFile = new TileDefFile;
    mTileDefFile->setFileName(fileName);

    initStringComboBoxValues();
    setTilesetList();
    updateUI();
}

void TileDefDialog::fileOpen()
{
    if (!confirmSave())
        return;

    QSettings settings;
    QString key = QLatin1String("TileDefDialog/LastOpenPath");
    QString lastPath = settings.value(key).toString();

    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .tiles file"),
                                                    lastPath,
                                                    QLatin1String("Tile properties files (*.tiles)"));
    if (fileName.isEmpty())
        return;

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    clearDocument();

    fileOpen(fileName);

    initStringComboBoxValues();
    updateTilesetListLater();
    updateUI();
}

bool TileDefDialog::fileSave()
{    
    if (mTileDefFile->fileName().length())
        return fileSave(mTileDefFile->fileName());
    else
        return fileSaveAs();
}

bool TileDefDialog::fileSaveAs()
{
    QString fileName = getSaveLocation();
    if (fileName.isEmpty())
        return false;
    return fileSave(fileName);
}

static void debugHistory(QStringList &history, int index)
{
#ifdef QT_NO_DEBUG
    Q_UNUSED(history)
    Q_UNUSED(index)
#else
    QStringList items;
    for (int i = 0; i < history.size(); i++) {
        if (i == index)
            items += QLatin1String("(") + history[i] + QLatin1String(")");
        else
            items += history[i];
    }
    noise() << items.join(QLatin1String(" "));
#endif
}

void TileDefDialog::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    mCurrentDefTileset = 0;
    mCurrentTilesetName.clear();

    if (row >= 0) {
        mCurrentTileset = tileset(row);
        mCurrentTilesetName = mCurrentTileset->name();
        mCurrentDefTileset = mTileDefFile->tileset(mCurrentTileset->name());

        if (!mSynching) {
            // Remove entries after the current one.
            if (mTilesetHistory.size())
                mTilesetHistory = mTilesetHistory.mid(0, mTilesetHistoryIndex + 1);
            mTilesetHistory.append(mCurrentTileset->name());
            // Don't let the history get too big.
            if (mTilesetHistory.size() > 30)
                mTilesetHistory = mTilesetHistory.mid(mTilesetHistory.size() - 30);
            mTilesetHistoryIndex = mTilesetHistory.size() - 1;
            debugHistory(mTilesetHistory, mTilesetHistoryIndex);
        }
    }

    setTilesList();
    updateUI();
}

void TileDefDialog::goBack()
{
    if (mTilesetHistoryIndex > 0) {
        mTilesetHistoryIndex--;
        int row = rowOf(mTilesetHistory[mTilesetHistoryIndex]);
        bool synch = mSynching;
        mSynching = true;
        ui->tilesets->setCurrentRow(row);
        mSynching = synch;
        debugHistory(mTilesetHistory, mTilesetHistoryIndex);
    }
}

void TileDefDialog::goForward()
{
    if (mTilesetHistoryIndex < mTilesetHistory.size() - 1) {
        mTilesetHistoryIndex++;
        int row = rowOf(mTilesetHistory[mTilesetHistoryIndex]);
        bool synch = mSynching;
        mSynching = true;
        ui->tilesets->setCurrentRow(row);
        mSynching = synch;
        debugHistory(mTilesetHistory, mTilesetHistoryIndex);
    }
}

void TileDefDialog::tileSelectionChanged()
{
    mSelectedTiles.clear();

    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        if (Tile *tile = ui->tiles->model()->tileAt(index)) {
            if (TileDefTileset *ts = mTileDefFile->tileset(tile->tileset()->name()))
                mSelectedTiles += ts->mTiles[tile->id()];
        }
    }

    setPropertiesPage();
    updateUI();
}

void TileDefDialog::comboBoxActivated(int index)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QComboBox *w = dynamic_cast<QComboBox*>(sender);
    if (!w) return;

    TileDefProperty *prop = mTileDefProperties->property(w->objectName());
    if (!prop || !prop->asEnum()) {
        Q_ASSERT(false);
        return;
    }
    changePropertyValues(mSelectedTiles, prop->mName, prop->asEnum()->mEnums[index]);
}

void TileDefDialog::checkboxToggled(bool value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QCheckBox *w = dynamic_cast<QCheckBox*>(sender);
    if (!w) return;

    TileDefProperty *prop = mTileDefProperties->property(w->objectName());
    if (!prop || !prop->asBoolean()) {
        Q_ASSERT(false);
        return;
    }
    changePropertyValues(mSelectedTiles, prop->mName, value);
}

void TileDefDialog::spinBoxValueChanged(int value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QSpinBox *w = dynamic_cast<QSpinBox*>(sender);
    if (!w) return;

    TileDefProperty *prop = mTileDefProperties->property(w->objectName());
    if (!prop || !prop->asInteger()) {
        Q_ASSERT(false);
        return;
    }
    changePropertyValues(mSelectedTiles, prop->mName, value);
}

void TileDefDialog::stringEdited()
{
    if (mSynching)
        return;

    QObject *sender = this->sender(); // QLineEdit
    if (!sender) return;
    QComboBox *w = dynamic_cast<QComboBox*>(sender->parent());
    if (!w) return;

    TileDefProperty *prop = mTileDefProperties->property(w->objectName());
    if (!prop || !prop->asString()) {
        Q_ASSERT(false);
        return;
    }
    noise() << "stringEdited";
    changePropertyValues(mSelectedTiles, prop->mName, w->lineEdit()->text());
}

void TileDefDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void TileDefDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void TileDefDialog::updateTilesetList()
{
    int row = rowOf(mCurrentTilesetName);

    mTilesetsUpdatePending = false;
    loadTilesets();
    setTilesetList();

    if (row == -1)
        row = 0;
    ui->tilesets->setCurrentRow(row);

    updateUI();
}

void TileDefDialog::updatePropertyPage()
{
    mPropertyPageUpdatePending = false;
    setPropertiesPage();
}

void TileDefDialog::copyProperties()
{
    QRegion selectedRgn;
    foreach (TileDefTile *defTile, mSelectedTiles) {
        int x = defTile->id() % defTile->tileset()->mColumns;
        int y = defTile->id() / defTile->tileset()->mColumns;
        selectedRgn |= QRect(x, y, 1, 1);
    }
    QRect selectedBounds = selectedRgn.boundingRect();

    mClipboard->resize(selectedBounds.width(), selectedBounds.height());
    foreach (TileDefTile *defTile, mSelectedTiles) {
        int x = defTile->id() % defTile->tileset()->mColumns;
        int y = defTile->id() / defTile->tileset()->mColumns;
        x -= selectedBounds.left(), y -= selectedBounds.top();
        mClipboard->setEntry(x, y, defTile->mPropertyUI);
    }

    updateUI();
}

void TileDefDialog::pasteProperties()
{
    QRegion selectedRgn;
    foreach (TileDefTile *defTile, mSelectedTiles) {
        int x = defTile->id() % defTile->tileset()->mColumns;
        int y = defTile->id() / defTile->tileset()->mColumns;
        selectedRgn |= QRect(x, y, 1, 1);
    }
    QRect selectedBounds = selectedRgn.boundingRect();

    QList<TileDefTile*> changed;
    TileDefTileset *defTileset = mCurrentDefTileset;
    int clipX = selectedBounds.left(), clipY = selectedBounds.top();
    for (int y = selectedBounds.top(); y <= selectedBounds.bottom(); y++) {
        for (int x = selectedBounds.left(); x <= selectedBounds.right(); x++) {
            if (!selectedRgn.contains(QPoint(x, y)))
                continue;
            if (UIProperties *props = mClipboard->entry(clipX, clipY, x, y)) {
                int tileID = x + y * defTileset->mColumns;
                TileDefTile *defTile = defTileset->mTiles[tileID];
                foreach (UIProperties::UIProperty *src, props->mProperties) {
                    if (src->value() != defTile->property(src->mName)->value()) {
                        changed += defTile;
                        break;
                    }
                }
            }
        }
    }

    if (changed.size()) {
        mUndoStack->beginMacro(tr("Paste Properties"));
        foreach (TileDefTile *defTile, changed) {
            int x = defTile->id() % defTile->tileset()->mColumns;
            int y = defTile->id() / defTile->tileset()->mColumns;
            UIProperties *props = mClipboard->entry(selectedBounds.left(),
                                                    selectedBounds.top(), x, y);
            foreach (UIProperties::UIProperty *src, props->mProperties) {
                if (src->value() != defTile->property(src->mName)->value()) {
                    mUndoStack->push(new ChangePropertyValue(this, defTile,
                                                             defTile->property(src->mName),
                                                             src->value()));
                }
            }
        }
        mUndoStack->endMacro();
    }
}

void TileDefDialog::resetDefaults()
{
    QList<TileDefTile*> defTiles;
    foreach (TileDefTile *defTile, mSelectedTiles) {
        if (defTile->mPropertyUI.nonDefaultProperties().size())
            defTiles += defTile;
    }
    if (defTiles.size()) {
        mUndoStack->beginMacro(tr("Reset Property Values"));
        foreach (TileDefTile *defTile, defTiles)
            resetDefaults(defTile);
        mUndoStack->endMacro();
    }
}

void TileDefDialog::tileEntered(const QModelIndex &index)
{
    TileDefTile *defTile = static_cast<TileDefTile*>(ui->tiles->model()->userDataAt(index)); // danger!
    highlightTilesWithMatchingProperties(defTile);

    if (mSelectedTiles.size() == 1) {
        TileDefTile *defTile1 = mSelectedTiles.first();
        TileDefTile *defTile2 = static_cast<TileDefTile*>(ui->tiles->model()->userDataAt(index)); // danger!
        int offset = defTile2->id() - defTile1->id();
        ui->tileOffset->setText(tr("Offset: %1").arg(offset));
        return;
    }
    ui->tileOffset->setText(tr("Offset: ?"));
}

void TileDefDialog::tileLeft(const QModelIndex &index)
{
    Q_UNUSED(index)
    highlightTilesWithMatchingProperties(0);
    ui->tileOffset->setText(tr("Offset: ?"));
}

void TileDefDialog::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        setTilesList();
}

void TileDefDialog::updateUI()
{
    mSynching = true;

    bool hasFile = mTileDefFile != 0;
    ui->actionSave->setEnabled(hasFile);
    ui->actionSaveAs->setEnabled(hasFile);

    ui->actionTilesDirectory->setEnabled(hasFile);

    ui->actionGoBack->setEnabled(mTilesetHistoryIndex > 0);
    ui->actionGoForward->setEnabled(mTilesetHistoryIndex < mTilesetHistory.size() - 1);
    ui->actionAddTileset->setEnabled(hasFile);
    ui->actionRemoveTileset->setEnabled(mCurrentTileset != 0);

    ui->actionCopyProperties->setEnabled(mSelectedTiles.size());
    ui->actionPasteProperties->setEnabled(!mClipboard->mValidRgn.isEmpty() &&
                                          mSelectedTiles.size());
    ui->actionReset->setEnabled(mSelectedTiles.size());

    updateWindowTitle();

    mSynching = false;
}

void TileDefDialog::help()
{
    QUrl url = QUrl::fromLocalFile(
            Preferences::instance()->docsPath(QLatin1String("TileProperties/index.html")));
    QDesktopServices::openUrl(url);
}

bool TileDefDialog::eventFilter(QObject *object, QEvent *event)
{
    if ((event->type() == QEvent::Wheel) &&
            object->isWidgetType() &&
            ui->propertySheet->isAncestorOf(qobject_cast<QWidget*>(object))) {
        noise() << "Wheel event blocked";
        QCoreApplication::sendEvent(ui->scrollArea, event);
        return true;
    }
    return false;
}

void TileDefDialog::closeEvent(QCloseEvent *event)
{
    if (confirmSave()) {
        clearDocument();

        QSettings settings;
        settings.beginGroup(QLatin1String("TileDefDialog"));
        settings.setValue(QLatin1String("geometry"), saveGeometry());
        settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
        settings.setValue(QLatin1String("CurrentTileset"), mCurrentTilesetName);
        settings.endGroup();

        saveSplitterSizes(ui->splitter);

        // Delete ourself and TilePropertyMgr so TileProperties.txt can be edited without having
        // to restart TileZed.
        deleteLater();
        TilePropertyMgr::instance()->deleteLater();

        event->accept();
    } else {
        event->ignore();
    }

//    QMainWindow::closeEvent(event);
}

bool TileDefDialog::confirmSave()
{
    if (!mTileDefFile || mUndoStack->isClean())
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

QString TileDefDialog::getSaveLocation()
{
    QSettings settings;
    QString key = QLatin1String("TileDefDialog/LastOpenPath");
    QString suggestedFileName = Preferences::instance()->tilesDirectory() +
            QLatin1String("/tiledefinitions.tiles");
    if (!mTileDefFile) {
        //
    } else if (mTileDefFile->fileName().isEmpty()) {
        QString lastPath = settings.value(key).toString();
        if (!lastPath.isEmpty())
            suggestedFileName = lastPath + QLatin1String("/tiledefinitions.tiles");
    } else {
        suggestedFileName = mTileDefFile->fileName();
    }
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    suggestedFileName,
                                                    QLatin1String("Tile properties files (*.tiles)"));
    if (fileName.isEmpty())
        return QString();

    settings.setValue(key, QFileInfo(fileName).absolutePath());

    return fileName;
}

void TileDefDialog::fileOpen(const QString &fileName)
{
    TileDefFile *defFile = new TileDefFile;
    if (!defFile->read(fileName)) {
        QMessageBox::warning(this, tr("Error reading .tiles file"),
                             defFile->errorString());
        delete defFile;
        return;
    }

    mTileDefFile = defFile;
    mTilesDirectory.clear();
    tilesDirChanged();
}

bool TileDefDialog::fileSave(const QString &fileName)
{
    if (!mTileDefFile)
        return false;

    if (!mTileDefFile->write(fileName)) {
        QMessageBox::warning(this, tr("Error writing .tiles file"),
                             mTileDefFile->errorString());
        return false;
    }

    mTileDefFile->setFileName(fileName);
    setTilesDir(tilesDir());
    mUndoStack->setClean();

    updateWindowTitle();

    return true;
}

void TileDefDialog::clearDocument()
{
    mUndoStack->clear();

    mTilesetHistory.clear();
    mTilesetHistoryIndex = 0;

    mRemovedTilesets += mTilesets;
    mTilesets.clear();
    mTilesetByName.clear();

    qDeleteAll(mRemovedDefTilesets);
    mRemovedDefTilesets.clear();

    delete mTileDefFile;
    mTileDefFile = 0;

    ui->tilesets->clear();
}

void TileDefDialog::changePropertyValues(const QList<TileDefTile *> &defTiles,
                                         const QString &name, const QVariant &value)
{
    QList<TileDefTile *> change;
    foreach (TileDefTile *defTile, defTiles) {
        if (defTile->mPropertyUI.mProperties[name]->value() != value)
            change += defTile;
    }
    if (change.size()) {
        mUndoStack->beginMacro(tr("Change Property Values"));
        foreach (TileDefTile *defTile, change)
            mUndoStack->push(new ChangePropertyValue(this, defTile,
                                                     defTile->mPropertyUI.mProperties[name],
                                                     value));
        mUndoStack->endMacro();
    }
}

void TileDefDialog::updateTilesetListLater()
{
    if (!mTilesetsUpdatePending) {
        QMetaObject::invokeMethod(this, "updateTilesetList", Qt::QueuedConnection);
        mTilesetsUpdatePending = true;
    }
}

void TileDefDialog::updatePropertyPageLater()
{
    if (!mPropertyPageUpdatePending) {
        QMetaObject::invokeMethod(this, "updatePropertyPage", Qt::QueuedConnection);
        mPropertyPageUpdatePending = true;
    }
}

void TileDefDialog::setTilesetList()
{
    if (mTilesetsUpdatePending)
        return;

    mCurrentTileset = 0;
    mCurrentDefTileset = 0;
    mSelectedTiles.clear();

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 128;

    ui->tilesets->clear();
    foreach (Tileset *ts, mTilesetByName.values()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name() + QString::fromLatin1(" (%1)").arg(mTileDefFile->tileset(ts->name())->mID));
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.width(item->text()));
    }
    ui->tilesets->setFixedWidth(maxWidth + 16 +
        ui->tilesets->verticalScrollBar()->sizeHint().width());
}

void TileDefDialog::setTilesList()
{
    mSelectedTiles.clear();
    mTilesWithMatchingProperties.clear();

    if (mCurrentTileset) {
        TileDefTileset *defTileset = mCurrentDefTileset;
        QList<void*> userData;
        foreach (TileDefTile *defTile, defTileset->mTiles)
            userData += defTile;
        ui->tiles->setTileset(mCurrentTileset, userData);

        // Tooltip shows properties with non-default value.
        for (int i = 0; i < defTileset->mTiles.size(); i++) {
            setToolTipEtc(i);
        }
    } else {
        ui->tiles->clear();
    }
}

void TileDefDialog::setToolTipEtc(int tileID)
{
    if (!mCurrentTileset || !mCurrentDefTileset)
        return;
    TileDefTile *defTile = mCurrentDefTileset->mTiles[tileID];
    QStringList tooltip;
    foreach (UIProperties::UIProperty *p, defTile->mPropertyUI.nonDefaultProperties())
        tooltip += tr("%1 = %2").arg(p->mName).arg(p->valueAsString());

    MixedTilesetModel *m = ui->tiles->model();

    // Show .tiles property/value pairs.
    QMap<QString,QString> properties;
    defTile->mPropertyUI.ToProperties(properties);
    if (properties.size()) {
        tooltip += QLatin1String("\nOutput:");
        foreach (QString name, properties.keys()) {
            tooltip += tr("%1 = %2").arg(name).arg(properties[name]);
        }
    }

    // Use a different background color for tiles that have unknown property names.
    QColor color; // invalid means use default color
    QSet<QString> known = defTile->mPropertyUI.knownPropertyNames().toSet(); // FIXME: same for every tile
    QStringList unknown;
    foreach (QString name, defTile->mProperties.keys()) {
        if (!known.contains(name))
            unknown += tr("%1 = %2").arg(name).arg(defTile->mProperties[name]);
    }
    if (properties.isEmpty()) color = Qt::white;
    if (unknown.size()) {
        if (tooltip.size())
            tooltip += QLatin1String("");
        tooltip += QLatin1String("Unknown:");
        tooltip += unknown;
        color = QColor(255, 128, 128);
    }
    m->setData(m->index((void*)defTile), QBrush(color), MixedTilesetModel::CategoryBgRole);

    if (tooltip.size())
        tooltip += QLatin1String("");
    tooltip += QString::fromLatin1("gid %1").arg(defTile->tileset()->mID * 1000 + defTile->id());

    m->setToolTip(tileID, tooltip.join(QLatin1String("\n")));

    QRect r;
    if (tooltip.size())
        r = QRect(0,0,1,1);
    m->setCategoryBounds(tileID, r);
}

void TileDefDialog::highlightTilesWithMatchingProperties(TileDefTile *defTile)
{
    MixedTilesetModel *m = ui->tiles->model();

    // Reset the appearance of currently-highlighted tiles.
    foreach (TileDefTile *defTile2, mTilesWithMatchingProperties) {
        setToolTipEtc(defTile2->id());
        ui->tiles->update(m->index((void*)defTile2));
    }
    mTilesWithMatchingProperties.clear();

    if (!defTile)
        return;

    // Get the properties on the given tile.  Exit if properties is nil.
    QMap<QString,QString> props = defTile->mProperties;
    defTile->mPropertyUI.ToProperties(props);
    if (props.isEmpty())
        return;
    mTilesWithMatchingProperties += defTile;

    // Find other tiles with the same properties.
    for (int i = 0; i < defTile->tileset()->mTiles.size(); i++) {
        TileDefTile *defTile2 = defTile->tileset()->mTiles[i];
        if (defTile == defTile2)
            continue;

        QMap<QString,QString> props2 = defTile2->mProperties;
        defTile2->mPropertyUI.ToProperties(props2);
        if (props == props2)
            mTilesWithMatchingProperties.insert(defTile2);
    }

    // Highlight the given tile and others with matching properties.
    foreach (TileDefTile *defTile2, mTilesWithMatchingProperties) {
        m->setCategoryBounds(defTile2->id(), QRect(0,0,1,1));
        m->setData(m->index((void*)defTile2), QBrush(QColor(196, 255, 255)),
                   MixedTilesetModel::CategoryBgRole);
        ui->tiles->update(m->index((void*)defTile2));
    }
}

void TileDefDialog::resetDefaults(TileDefTile *defTile)
{
    QList<UIProperties::UIProperty*> props = defTile->mPropertyUI.nonDefaultProperties();
    if (props.size() == 0)
        return;

    mUndoStack->beginMacro(tr("Reset Property Values"));
    foreach (UIProperties::UIProperty *prop, props) {
        mUndoStack->push(new ChangePropertyValue(this, defTile,
                                                 prop, prop->defaultValue()));
    }
    mUndoStack->endMacro();
}

void TileDefDialog::setPropertiesPage()
{
    mSynching = true;

    ui->propertySheet->setEnabled(mSelectedTiles.size());

    const TileDefProperties *props = mTileDefProperties;
    TileDefTile *defTile = 0;
    if (mSelectedTiles.size())
        defTile = mSelectedTiles.first();

    foreach (TileDefProperty *prop, props->mProperties) {
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            if (QCheckBox *w = mCheckBoxes[p->mName]) {
                bool checked = p->mDefault;
                if (defTile) {
                    checked = defTile->getBoolean(p->mName);
                }
                w->setChecked(checked);
                if (p->mReverseLogic)
                    setBold(w, checked == p->mDefault);
                else
                    setBold(w, checked != p->mDefault);
            }
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            if (QSpinBox *w = mSpinBoxes[p->mName]) {
                int value = p->mDefault;
                if (defTile) {
                    value = defTile->getInteger(p->mName);
                }
                w->setValue(value);
                setBold(w, value != p->mDefault);
            }
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            if (QComboBox *w = mComboBoxes[p->mName]) {
                QString value = p->mDefault;
                if (defTile) {
                    value = defTile->getString(p->mName);
                }
                w->setEditText(value);
                setBold(w, value != p->mDefault);
            }
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            if (QComboBox *w = mComboBoxes[p->mName]) {
                int index = p->mEnums.indexOf(p->mDefault);
                if (defTile) {
                    index = p->mEnums.indexOf(defTile->getEnum(p->mName));
                }
                w->setCurrentIndex(index);
                setBold(w, index != 0 /*p->mDefault*/);
            }
            continue;
        }
    }

    mSynching = false;
}

void TileDefDialog::setBold(QWidget *w, bool bold)
{
    if (mLabelFont == w->font()) {
        if (!bold) return;
        w->setFont(mBoldLabelFont);
    } else {
        if (bold) return;
        w->setFont(mLabelFont);
    }

    foreach (QObject *o, w->parent()->children()) {
        if (QLabel *label = dynamic_cast<QLabel*>(o)) {
            if (label->buddy() == w) {
                label->setFont(w->font());
                break;
            }
        }
    }
}

void TileDefDialog::initStringComboBoxValues()
{
    mSynching = true;

    QMap<QString,QSet<QString> > values;

    foreach (TileDefTileset *ts, mTileDefFile->tilesets()) {
        foreach (TileDefTile *t, ts->mTiles) {
            foreach (UIProperties::UIProperty *p, t->mPropertyUI.mProperties) {
                if (p->getString().length())
                    values[p->mName].insert(p->getString());
            }
        }
    }

    foreach (TileDefProperty *prop, mTileDefProperties->mProperties) {
        if (StringTileDefProperty *p = prop->asString()) {
            if (values.contains(p->mName)) {
                QComboBox *w = mComboBoxes[p->mName];
                w->clear();
                QStringList names(values[p->mName].toList());
                names.sort();
                w->addItems(names);
                w->clearEditText();
            }
        }
    }

    mSynching = false;
}

Tileset *TileDefDialog::tileset(const QString &name) const
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

Tileset *TileDefDialog::tileset(int row) const
{
    if (row >= 0 && row < mTilesetByName.size())
        return mTilesetByName.values().at(row);
    return 0;
}

int TileDefDialog::indexOf(const QString &name) const
{
    if (mTilesetByName.contains(name))
        return mTilesets.indexOf(mTilesetByName[name]);
    return -1;
}

int TileDefDialog::rowOf(const QString &name) const
{
    return tilesetNames().indexOf(name);
}

extern bool resolveImageSource(QString &imageSource);

void TileDefDialog::loadTilesets()
{
    bool anyMissing = false;
    foreach (Tileset *ts, mTilesets) {
        if (ts->isMissing()) {
            anyMissing = true;
            break;
        }
    }
    if (!anyMissing)
        return;

    PROGRESS progress(tr("Loading tilesets..."), this);

    foreach (Tileset *ts, mTilesets) {
        if (ts->isMissing()) {
            QString source = ts->imageSource();
            if (QDir::isRelativePath(ts->imageSource()))
                source = QDir(tilesDir()).filePath(ts->imageSource());
            if (TilesetManager::instance()->useVirtualTilesets() &&
                    VirtualTilesetMgr::instance().resolveImageSource(source)) {
                TilesetManager::instance()->loadTileset(ts, source);
                continue;
            }
            QImageReader reader(source);
            if (reader.size().isValid()) {
                ts->loadFromNothing(reader.size(), ts->imageSource());
                QFileInfo info(source);
                TilesetManager::instance()->loadTileset(ts, info.canonicalFilePath());
            }
        }
    }
}

Tileset *TileDefDialog::loadTileset(const QString &source)
{
    QString canonical = source;
    if (TilesetManager::instance()->useVirtualTilesets() &&
            VirtualTilesetMgr::instance().resolveImageSource(canonical)) {
        QFileInfo info(source);
        Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
        TilesetManager::instance()->loadTileset(ts, canonical);
        return ts;
    }
    QImageReader reader(source);
    if (reader.size().isValid()) {
        QFileInfo info(source);
        Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
        ts->loadFromNothing(reader.size(), source/*info.canonicalFilePath()*/);
        ts->setMissing(true); // prevent FileSystemWatcher warning in TilesetManager::changeTilesetSource
        TilesetManager::instance()->loadTileset(ts, source/*info.canonicalFilePath()*/);
        return ts;
    }
    return 0;
}

#if 0
bool TileDefDialog::loadTilesetImage(Tileset *ts, const QString &source)
{
    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }

    return ts;
}
#endif

struct ResizedTileset {
    ResizedTileset(const QString &name, const QSize &oldSize, const QSize &newSize) :
        name(name), oldSize(oldSize), newSize(newSize)
    {}
    QString name;
    QSize oldSize;
    QSize newSize;
};

void TileDefDialog::tilesDirChanged()
{
    mRemovedTilesets += mTilesets;
    mTilesets.clear();
    mTilesetByName.clear();

    QDir dir(tilesDir());

    QList<ResizedTileset> resized;

    foreach (TileDefTileset *tsDef, mTileDefFile->tilesets()) {
        QString imageSource = dir.filePath(tsDef->mImageSource);
        if (QFileInfo(imageSource).exists()) {
            imageSource = QFileInfo(imageSource).canonicalFilePath();
            QString imageSource2x = imageSource;
            if (resolveImageSource(imageSource2x)) {
                QImageReader ir(imageSource2x);
                if (ir.size().isValid()) {
                    int columns = ir.size().width() / (64 * 2);
                    int rows = ir.size().height() / (128 * 2);
                    if (QSize(columns, rows) != QSize(tsDef->mColumns, tsDef->mRows)) {
                        resized += ResizedTileset(tsDef->mName,
                                                  QSize(tsDef->mColumns, tsDef->mRows),
                                                  QSize(columns, rows));
                        tsDef->resize(columns, rows);
                    }
                }
            } else {
                QImageReader ir(imageSource);
                if (ir.size().isValid()) {
                    int columns = ir.size().width() / 64;
                    int rows = ir.size().height() / 128 ;
                    if (QSize(columns, rows) != QSize(tsDef->mColumns, tsDef->mRows)) {
                        resized += ResizedTileset(tsDef->mName,
                                                  QSize(tsDef->mColumns, tsDef->mRows),
                                                  QSize(columns, rows));
                        tsDef->resize(columns, rows);
                    }
                }
            }
        }

        // Try to reuse a tileset from our list of removed tilesets.
        bool reused = false;
        foreach (Tileset *ts, mRemovedTilesets) {
            if (ts->imageSource() == imageSource) {
                mTilesets += ts;
                mTilesetByName[ts->name()] = ts;
                // Don't addReferences().
                mRemovedTilesets.removeOne(ts);
                reused = true;
                break;
            }
        }
        if (reused)
            continue;

        Tileset *tileset = new Tileset(tsDef->mName, 64, 128);
        int width = tsDef->mColumns * 64, height = tsDef->mRows * 128;
        tileset->loadFromNothing(QSize(width, height), imageSource);
        Tile *missingTile = TilesetManager::instance()->missingTile();
        for (int i = 0; i < tileset->tileCount(); i++)
            tileset->tileAt(i)->setImage(missingTile->image());
        tileset->setMissing(true);

        mTilesets += tileset;
        mTilesetByName[tileset->name()] = tileset;
        TilesetManager::instance()->addReference(tileset);
    }

    if (resized.size()) {
        QStringList sl;
        foreach (ResizedTileset rt, resized) {
            bool smaller = rt.oldSize.width() > rt.newSize.width() ||
                    rt.oldSize.height() > rt.newSize.height();
            sl += QString::fromLatin1("%1 - was %2x%3, now %4x%5 - %6")
                    .arg(rt.name).arg(rt.oldSize.width()).arg(rt.oldSize.height())
                    .arg(rt.newSize.width()).arg(rt.newSize.height())
                    .arg(QLatin1String(smaller ? "SMALLER" : "LARGER"));
        }
        BuildingEditor::ListOfStringsDialog dialog(
                    tr("Some tileset images are not the same size they were before.\nIf a tileset is now smaller, you have lost properites for some tiles.\nIf you didn't want that to happen, close without saving now."),
                    sl, this);
        dialog.setWindowTitle(tr("Tileset Images Changed Size"));
        dialog.exec();
    }
}

QString TileDefDialog::tilesDir()
{
    // If the user never set the directory, it will be the same as the TileDefFile
    // directory.  If it was set, it is saved by QSettings, not in the file itself.
    if (mTilesDirectory.isEmpty()) {
        QFileInfo info(mTileDefFile->fileName());
        if (info.exists()) {
            QMap<QString,QString> kv;
            getTilesDirKeyValues(kv);
            foreach (QString file, kv.keys()) {
                if (file == info.canonicalFilePath()) {
                    if (!kv[file].isEmpty() && QFileInfo(kv[file]).exists())
                        return kv[file];
                    break;
                }
            }
        }
        return mTileDefFile->directory();
    }
    return mTilesDirectory;
}

void TileDefDialog::setTilesDir(const QString &path)
{
    QFileInfo info(mTileDefFile->fileName());
    if (info.exists()) {
        QSettings settings;
        QMap<QString,QString> kv;
        getTilesDirKeyValues(kv);
        settings.beginWriteArray(QLatin1String("TileDefDialog/TilesDir"));
        QString canonical = info.canonicalFilePath();
        int i = 0;
        foreach (QString file, kv.keys()) {
            if (file == canonical) continue;
            settings.setArrayIndex(i++);
            settings.setValue(QLatin1String("file"), file);
            settings.setValue(QLatin1String("dir"), kv[file]);
        }
        settings.setArrayIndex(i++);
        settings.setValue(QLatin1String("file"), canonical);
        settings.setValue(QLatin1String("dir"), path);
        settings.endArray();
        return;
    }

    // If the file hasn't been saved, we can't store the canonical file path
    // in QSettings. So just remember the path until we save the file.
    mTilesDirectory = path;
}

void TileDefDialog::getTilesDirKeyValues(QMap<QString, QString> &map)
{
    QSettings settings;
    int size = settings.beginReadArray(QLatin1String("TileDefDialog/TilesDir"));
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        QString file = settings.value(QLatin1String("file")).toString();
        QString dir = settings.value(QLatin1String("dir")).toString();
        if (QFile(file).exists())
            map[file] = dir;
    }
    settings.endArray();
}

int TileDefDialog::uniqueTilesetID()
{
    int id = 1;
    foreach (TileDefTileset *tsDef, mTileDefFile->tilesets()) {
        if (tsDef->mID >= id)
            id = tsDef->mID + 1;
    }
    return id;
}

void TileDefDialog::saveSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefDialog"));
    QVariantList v;
    foreach (int size, splitter->sizes())
        v += size;
    settings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    settings.endGroup();
}

void TileDefDialog::restoreSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TileDefDialog"));
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

void TileDefDialog::updateWindowTitle()
{
    if (mTileDefFile && mTileDefFile->fileName().length()) {
        QString fileName = QDir::toNativeSeparators(mTileDefFile->fileName());
        setWindowTitle(tr("[*]%1").arg(fileName));
    } else if (mTileDefFile) {
        setWindowTitle(tr("[*]Untitled"));
    } else {
        setWindowTitle(tr("Tile Properties"));
    }
    setWindowModified(!mUndoStack->isClean());
}

void TileDefDialog::displayTile(const QString &tileName)
{
    QString tilesetName;
    int tileID;
    if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
        if (mTilesetByName.contains(tilesetName)) {
            int row = rowOf(tilesetName);
            ui->tilesets->setCurrentRow(row);
            ui->tiles->setCurrentIndex(ui->tiles->model()->index(mTilesetByName[tilesetName]->tileAt(tileID)));
        }
    }
}

/////

