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
#include "tiledeffile.h"
#include "tilesetmanager.h"
#include "zoomable.h"
#include "utils.h"
#include "zprogress.h"

#include "tile.h"
#include "tileset.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

/////

class Tiled::Internal::TilePropertyClipboard
{
public:
    struct Entry
    {
        Entry() :
            mProperties(mYuck, mYuckKeys)
        {}
        UIProperties mProperties;
        QList<QString> mYuckKeys;
        QMap<QString,QString> mYuck;
    };

    void clear();
    void resize(int width, int height);
    void setEntry(int x, int y, UIProperties &properties);
    UIProperties *entry(int tx, int ty, int x, int y);

    int mWidth, mHeight;
    QVector<Entry*> mEntries;
    QRegion mValidRgn;
};

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

TileDefDialog::TileDefDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileDefDialog),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this)),
    mSynching(false),
    mTilesetsUpdatePending(false),
    mPropertyPageUpdatePending(false),
    mTileDefFile(0),
    mTileDefProperties(&TilePropertyMgr::instance()->properties()),
    mClipboard(new TilePropertyClipboard),
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

    QAction *a = ui->menuEdit->actions().first();
    ui->menuEdit->insertAction(a, undoAction);
    ui->menuEdit->insertAction(a, redoAction);

    ui->splitter->setStretchFactor(0, 1);

    mZoomable->setScale(0.5); // FIXME
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
    connect(ui->tiles, SIGNAL(tileEntered(Tile*)), SLOT(tileEntered(Tile*)));
    connect(ui->tiles, SIGNAL(tileLeft(Tile*)), SLOT(tileLeft(Tile*)));

    ui->actionNew->setShortcut(QKeySequence::New);
    ui->actionOpen->setShortcut(QKeySequence::Open);
    ui->actionSave->setShortcut(QKeySequence::Save);
    ui->actionSaveAs->setShortcut(QKeySequence::SaveAs);

    connect(ui->actionNew, SIGNAL(triggered()), SLOT(fileNew()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(fileOpen()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->actionSaveAs, SIGNAL(triggered()), SLOT(fileSaveAs()));
    connect(ui->actionAddTileset, SIGNAL(triggered()), SLOT(addTileset()));
    connect(ui->actionClose, SIGNAL(triggered()), SLOT(close()));

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
    settings.endGroup();

    restoreSplitterSizes(ui->splitter);

    fileOpen(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\maptools\\tiledefinitions.tiles"));
    initStringComboBoxValues();
    updateTilesetListLater();

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
}

void TileDefDialog::addTileset()
{ 
    AddTilesetsDialog dialog(mTileDefFile->directory(),
                             mTileDefFile->tilesetNames(),
                             this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    mUndoStack->beginMacro(tr("Add Tilesets"));

    foreach (QString fileName, dialog.fileNames()) {
        QFileInfo info(fileName);
        if (Tiled::Tileset *ts = loadTileset(info.canonicalFilePath())) {
            TileDefTileset *defTileset = new TileDefTileset(ts);
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
        mUndoStack->push(new RemoveTileset(this, row));
    }
}

void TileDefDialog::insertTileset(int index, Tileset *ts, TileDefTileset *defTileset)
{
    Q_ASSERT(!mTilesets.contains(ts));
    Q_ASSERT(!mTilesetByName.contains(ts->name()));

    mTileDefFile->insertTileset(index, defTileset);
    mRemovedDefTilesets.removeOne(defTileset);

    mTilesets.insert(index, ts);
    mTilesetByName[ts->name()] = ts;
    if (!mRemovedTilesets.contains(ts))
        TilesetManager::instance()->addReference(ts);
    mRemovedTilesets.removeOne(ts);

    updateTilesetListLater();
}

void TileDefDialog::removeTileset(int index, Tileset **tsPtr, TileDefTileset **defTilesetPtr)
{
    TileDefTileset *defTileset = mTileDefFile->removeTileset(index);
    mRemovedDefTilesets += defTileset;

    Tileset *ts = mTilesets.takeAt(index);
    mTilesetByName.remove(ts->name());
    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += ts;

    updateTilesetListLater();

    if (tsPtr) *tsPtr = ts;
    if (defTilesetPtr) *defTilesetPtr = defTileset;
}

QVariant TileDefDialog::changePropertyValue(TileDefTile *defTile, const QString &name,
                                            const QVariant &value)
{
    QVariant old = defTile->mPropertyUI.mProperties[name]->value();
    defTile->mPropertyUI.ChangePropertiesV(name, value);
    setToolTipEtc(defTile->id());
    updatePropertyPageLater();
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

void TileDefDialog::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = tileset(row);

    setTilesList();
    updateUI();
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
    changePropertyValues(mSelectedTiles, prop->mName, index);
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
    qDebug() << "stringEdited";
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
    mTilesetsUpdatePending = false;
    loadTilesets();
    setTilesetList();
//    int row = indexOf(ts->name());
//    ui->tilesets->setCurrentRow(row);

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
    TileDefTileset *defTileset = mTileDefFile->tileset(mCurrentTileset->name());
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

void TileDefDialog::tileEntered(Tile *tile)
{
    if (mSelectedTiles.size() == 1) {
        TileDefTile *defTile1 = mSelectedTiles.first();
        TileDefTile *defTile2 = defTile1->tileset()->mTiles[tile->id()];
        int offset = defTile2->id() - defTile1->id();
        ui->tileOffset->setText(tr("Offset: %1").arg(offset));
        return;
    }
    ui->tileOffset->setText(tr("Offset: ?"));
}

void TileDefDialog::tileLeft(Tile *tile)
{
    ui->tileOffset->setText(tr("Offset: ?"));
}

void TileDefDialog::updateUI()
{
    mSynching = true;

    bool hasFile = mTileDefFile != 0;
    ui->actionSave->setEnabled(hasFile);
    ui->actionSaveAs->setEnabled(hasFile);
    ui->actionAddTileset->setEnabled(hasFile);

    ui->actionCopyProperties->setEnabled(mSelectedTiles.size());
    ui->actionPasteProperties->setEnabled(!mClipboard->mValidRgn.isEmpty() &&
                                          mSelectedTiles.size());
    ui->actionReset->setEnabled(mSelectedTiles.size());

    updateWindowTitle();

    mSynching = false;
}

bool TileDefDialog::eventFilter(QObject *object, QEvent *event)
{
    if ((event->type() == QEvent::Wheel) &&
            object->isWidgetType() &&
            ui->propertySheet->isAncestorOf(qobject_cast<QWidget*>(object))) {
        qDebug() << "Wheel event blocked";
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
        settings.endGroup();

        saveSplitterSizes(ui->splitter);

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
    QString suggestedFileName;
    if (mTileDefFile->fileName().isEmpty()) {
        suggestedFileName = settings.value(key).toString();
        if (!suggestedFileName.isEmpty())
            suggestedFileName += QLatin1String("/tiledefinitions.tiles");
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
        QMessageBox::warning(this, tr("Eror reading .tiles file"),
                             defFile->errorString());
        delete defFile;
        return;
    }

    mTileDefFile = defFile;
    QDir dir(mTileDefFile->directory());

    foreach (QString tilesetName, mTileDefFile->tilesetNames()) {
        TileDefTileset *tsDef = mTileDefFile->tileset(tilesetName);
        QString imageSource = dir.filePath(tsDef->mImageSource);
        if (QFileInfo(imageSource).exists())
            imageSource = QFileInfo(imageSource).canonicalFilePath();

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

        Tileset *tileset = new Tileset(tilesetName, 64, 128);
        int width = tsDef->mColumns * 64, height = tsDef->mRows * 128;
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, imageSource);
        Tile *missingTile = TilesetManager::instance()->missingTile();
        for (int i = 0; i < tileset->tileCount(); i++)
            tileset->tileAt(i)->setImage(missingTile->image());
        tileset->setMissing(true);

        mTilesets += tileset;
        mTilesetByName[tileset->name()] = tileset;
        TilesetManager::instance()->addReference(tileset);
    }
}

bool TileDefDialog::fileSave(const QString &fileName)
{
    if (!mTileDefFile)
        return false;

    if (!mTileDefFile->write(fileName)) {
        QMessageBox::warning(this, tr("Eror writing .tiles file"),
                             mTileDefFile->errorString());
        return false;
    }

    mTileDefFile->setFileName(fileName);
    mUndoStack->setClean();

    updateWindowTitle();

    return true;
}

void TileDefDialog::clearDocument()
{
    mUndoStack->clear();

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
    mSelectedTiles.clear();

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 128;

    ui->tilesets->clear();
    foreach (Tileset *ts, mTilesetByName.values()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name());
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.width(ts->name()));
    }
    ui->tilesets->setFixedWidth(maxWidth + 16 +
        ui->tilesets->verticalScrollBar()->sizeHint().width());
}

void TileDefDialog::setTilesList()
{
    mSelectedTiles.clear();

    if (mCurrentTileset) {
        ui->tiles->model()->setTileset(mCurrentTileset);

        // Tooltip shows properties with non-default value.
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            setToolTipEtc(i);
        }
    } else {
        ui->tiles->model()->setTiles(QList<Tile*>());
    }

    tileSelectionChanged(); // model calling reset() doesn't generate selectionChanged signal
}

void TileDefDialog::setToolTipEtc(int tileID)
{
    TileDefTileset *defTileset = mTileDefFile->tileset(mCurrentTileset->name());
    if (!defTileset)
        return;
    TileDefTile *defTile = defTileset->mTiles[tileID];
    QStringList tooltip;
    foreach (UIProperties::UIProperty *p, defTile->mPropertyUI.nonDefaultProperties())
        tooltip += tr("%1 = %2").arg(p->mName).arg(p->valueAsString());
    if (defTile->mProperties.size() && tooltip.isEmpty())
        qDebug() << defTile->mProperties;
    ui->tiles->model()->setToolTip(tileID, tooltip.join(QLatin1String("\n")));
    QRect r;
    if (tooltip.size())
        r = QRect(0,0,1,1);
    ui->tiles->model()->setCategoryBounds(tileID, r);
    ui->tiles->update(ui->tiles->model()->index(mCurrentTileset->tileAt(tileID)));
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

    const TileDefProperties *props = mTileDefProperties;
    TileDefTile *defTile = 0;
    if (mSelectedTiles.size()) {
        if (TileDefTileset *defTileset = mTileDefFile->tileset(mCurrentTileset->name())) {
            defTile = defTileset->mTiles[mSelectedTiles[0]->id()];
        }
    }
    foreach (TileDefProperty *prop, props->mProperties) {
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            if (QCheckBox *w = mCheckBoxes[p->mName]) {
                bool checked = p->mDefault;
                if (defTile) {
                    checked = defTile->getBoolean(p->mName);
                }
                w->setChecked(checked);
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
                int index = 0;
                if (defTile) {
                    index = defTile->getEnum(p->mName);
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
    return tilesetNames().indexOf(name);
}

void TileDefDialog::loadTilesets()
{
    bool anyMissing;
    foreach (Tileset *ts, mTilesets)
        if (ts->isMissing()) {
            anyMissing = true;
            break;
        }

    if (!anyMissing)
        return;

    PROGRESS progress(tr("Loading tilesets..."), this);

    foreach (Tileset *ts, mTilesets) {
        if (ts->isMissing()) {
            QString source = ts->imageSource();
            QString oldSource = source;
            if (QDir::isRelativePath(ts->imageSource()))
                source = QDir(mTileDefFile->directory()).filePath(ts->imageSource());
            QFileInfo info(source);
            if (info.exists()) {
                source = info.canonicalFilePath();
                if (loadTilesetImage(ts, source)) {
                    ts->setMissing(false); // Yay!
                    TilesetManager::instance()->tilesetSourceChanged(ts, oldSource, true);
                }
            }
        }
    }
}

Tileset *TileDefDialog::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
    if (!loadTilesetImage(ts, source)) {
        delete ts;
        return 0;
    }
    return ts;
}

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

/////

