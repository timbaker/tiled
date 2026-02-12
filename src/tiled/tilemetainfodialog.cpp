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

#include "addremovetileset.h"
#include "addtilesetsdialog.h"
#include "documentmanager.h"
#include "mainwindow.h"
#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "utils.h"
#include "zoomable.h"

#include "map.h"
#include "tileset.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QSplitter>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace TileMetaUndoRedo {

class AddGlobalTileset : public QUndoCommand
{
public:
    AddGlobalTileset(TileMetaInfoDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Global Tileset")),
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

class RemoveGlobalTileset : public QUndoCommand
{
public:
    RemoveGlobalTileset(TileMetaInfoDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Global Tileset")),
        mDialog(d),
        mTileset(tileset)
    {
        mTxtTileset.fromTileset(tileset);
    }

    void undo()
    {
        mDialog->addTileset(mTileset);
        mTxtTileset = mDialog->setTilesetEnums(mTileset, mTxtTileset);
    }

    void redo()
    {
        mTxtTileset = mDialog->setTilesetEnums(mTileset, mTxtTileset);
        mDialog->removeTileset(mTileset);
    }

    TileMetaInfoDialog *mDialog;
    Tileset *mTileset;
    TilesetsTxtFile::Tileset mTxtTileset;
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

class SetTilesetEnums : public QUndoCommand
{
public:
    SetTilesetEnums(TileMetaInfoDialog *d, Tileset *tileset, TilesetsTxtFile::Tileset *txtTileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Set Tileset Meta-Enums")),
        mDialog(d),
        mTileset(tileset),
        mTxtTileset(*txtTileset)
    {
    }

    void undo() override { swap(); }
    void redo() override { swap(); }

    void swap()
    {
        mTxtTileset = mDialog->setTilesetEnums(mTileset, mTxtTileset);
    }

    TileMetaInfoDialog *mDialog;
    Tileset *mTileset;
    TilesetsTxtFile::Tileset mTxtTileset;
};

class ReplaceTileMetaMgrEnums : public QUndoCommand
{
public:
    ReplaceTileMetaMgrEnums(TileMetaInfoDialog *d, const QMap<QString,int> &enums, const QStringList &enumNames) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Set Meta-Enums")),
        mDialog(d),
        mEnums(enums),
        mEnumNames(enumNames)
    {
    }

    void undo() override { swap(); }
    void redo() override { swap(); }

    void swap()
    {
        mDialog->replaceTileMetaMgrEnums(mEnums, mEnumNames);
    }

    TileMetaInfoDialog *mDialog;
    QMap<QString,int> mEnums;
    QStringList mEnumNames;
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
    toolBar->addAction(ui->actionAddToMap);
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
    connect(mUndoGroup, &QUndoGroup::canUndoChanged, button, &QWidget::setEnabled);
    connect(button, &QAbstractButton::clicked, undoAction, &QAction::triggered);

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(redoAction->text());
    button->setEnabled(mUndoGroup->canRedo());
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->undoRedoLayout->addWidget(button);
    connect(mUndoGroup, &QUndoGroup::canRedoChanged, button, &QWidget::setEnabled);
    connect(button, &QAbstractButton::clicked, redoAction, &QAction::triggered);

    connect(mUndoGroup, &QUndoGroup::undoTextChanged, this, &TileMetaInfoDialog::undoTextChanged);
    connect(mUndoGroup, &QUndoGroup::redoTextChanged, this, &TileMetaInfoDialog::redoTextChanged);

    /////

    connect(ui->buttonExport, &QToolButton::clicked, this, &TileMetaInfoDialog::exportFile);
    connect(ui->buttonImport, &QToolButton::clicked, this, &TileMetaInfoDialog::importFile);
    connect(ui->buttonReload, &QToolButton::clicked, this, &TileMetaInfoDialog::reloadFile);

    /////

    mZoomable->setScale(0.5); // FIXME
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    ui->tiles->model()->setShowHeaders(false);

    ui->tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tiles->model()->setShowHeaders(false);
    ui->tiles->model()->setShowLabels(true);
    ui->tiles->model()->setHighlightLabelledItems(true);

    ui->filterEdit->setClearButtonEnabled(true);
    ui->filterEdit->setEnabled(false);
    connect(ui->filterEdit, &QLineEdit::textEdited, this, &TileMetaInfoDialog::tilesetFilterEdited);

    connect(ui->browseTiles, &QAbstractButton::clicked, this, &TileMetaInfoDialog::browse);
    connect(ui->tilesets, &QListWidget::currentRowChanged,
            this, &TileMetaInfoDialog::currentTilesetChanged);
    connect(ui->tiles->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &TileMetaInfoDialog::tileSelectionChanged);
    connect(ui->actionAdd, &QAction::triggered, this, qOverload<>(&TileMetaInfoDialog::addTileset));
    connect(ui->actionRemove, &QAction::triggered, this, qOverload<>(&TileMetaInfoDialog::removeTileset));
    connect(ui->actionAddToMap, &QAction::triggered, this, &TileMetaInfoDialog::addToMap);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    connect(ui->enums, qOverload<int>(&QComboBox::activated),
            this, &TileMetaInfoDialog::enumChanged);
#else
    connect(ui->enums, &QComboBox::activated,
            this, &TileMetaInfoDialog::enumChanged);
#endif

    connect(TilesetManager::instance(), &TilesetManager::tilesetChanged,
            this, &TileMetaInfoDialog::tilesetChanged);

    // Hack - force the tileset-names-list font to be updated now, because
    // setTilesetList() uses its font metrics to determine the maximum item
    // width.
    ui->tilesets->setFont(QFont());
    setTilesetList();

    mSynching = true;
    ui->enums->addItem(tr("<none>"));
    ui->enums->addItems(TileMetaInfoMgr::instance()->enumNames());
    mSynching = false;

    updateUI();

    restoreSettings();
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

TilesetsTxtFile::Tileset TileMetaInfoDialog::setTilesetEnums(Tileset *tileset, const TilesetsTxtFile::Tileset &txtTileset)
{
    TilesetsTxtFile::Tileset ret;
    ret.fromTileset(tileset);
    txtTileset.toTileset(tileset);
    return ret;
}

void TileMetaInfoDialog::addTileset()
{
    const QString tilesDir = TileMetaInfoMgr::instance()->tilesDirectory();

    AddTilesetsDialog dialog(tilesDir,
                             TileMetaInfoMgr::instance()->tilesetNames(),
                             false,
                             this);
    dialog.setAllowBrowse(true);
    if (dialog.exec() != QDialog::Accepted)
        return;

    mUndoStack->beginMacro(tr("Add Tilesets"));

    foreach (QString f, dialog.fileNames()) {
        QFileInfo info(f);
        if (Tiled::Tileset *ts = TileMetaInfoMgr::instance()->loadTileset(f/*info.canonicalFilePath()*/)) {
            QString name = info.completeBaseName();
            // Replace any current tileset with the same name as an existing one.
            // This will NOT replace the meta-info for the old tileset, it will
            // be used by the new tileset as well.
            if (Tileset *old = TileMetaInfoMgr::instance()->tileset(name))
                mUndoStack->push(new RemoveGlobalTileset(this, old));
            mUndoStack->push(new AddGlobalTileset(this, ts));
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
        mUndoStack->push(new RemoveGlobalTileset(this, tileset));
    }
}

void TileMetaInfoDialog::addToMap()
{
    MapDocument *mapDocument = DocumentManager::instance()->currentDocument();
    if (!mapDocument)
        return;

    QString text = tr("Really add all these tilesets to the current map?\nDuplicate tilesets will not be added.");
    if (QMessageBox::question(this, tr("Add Tilesets To Map"), text,
                              QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;

    QList<Tileset*> tilesets;
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        if (tileset->isMissing())
            continue;
        if (tileset->findSimilarTileset(mapDocument->map()->tilesets()))
            continue;
        tilesets += tileset->clone();
    }

    if (tilesets.size() > 0) {
        mapDocument->undoStack()->beginMacro(tr("Add Tilesets to Map"));
        foreach (Tileset *tileset, tilesets)
            mapDocument->undoStack()->push(new AddTileset(mapDocument, tileset));
        mapDocument->undoStack()->endMacro();
    }

    QMessageBox::information(this, tr("Add Tilesets to Map"),
                             tr("%1 tilesets were added to %2.")
                             .arg(tilesets.size())
                             .arg(mapDocument->displayName()));
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

void TileMetaInfoDialog::replaceTileMetaMgrEnums(QMap<QString, int> &metaEnums, QStringList& enumNames)
{
    TileMetaInfoMgr::instance()->replaceEnums(metaEnums, enumNames);
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

void TileMetaInfoDialog::tilesetFilterEdited(const QString &text)
{
    QListWidget* mTilesetNamesView = ui->tilesets;

    for (int row = 0; row < mTilesetNamesView->count(); row++) {
        QListWidgetItem* item = mTilesetNamesView->item(row);
        item->setHidden(text.trimmed().isEmpty() ? false : !item->text().contains(text));
    }

    QListWidgetItem* current = mTilesetNamesView->currentItem();
    if (current != nullptr && current->isHidden()) {
        // Select previous visible row.
        int row = mTilesetNamesView->row(current) - 1;
        while (row >= 0 && mTilesetNamesView->item(row)->isHidden())
            row--;
        if (row >= 0) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // Select next visible row.
        row = mTilesetNamesView->row(current) + 1;
        while (row < mTilesetNamesView->count() && mTilesetNamesView->item(row)->isHidden())
            row++;
        if (row < mTilesetNamesView->count()) {
            current = mTilesetNamesView->item(row);
            mTilesetNamesView->setCurrentItem(current);
            mTilesetNamesView->scrollToItem(current);
            return;
        }

        // All items hidden
        mTilesetNamesView->setCurrentItem(nullptr);
    }

    current = mTilesetNamesView->currentItem();
    if (current != nullptr)
        mTilesetNamesView->scrollToItem(current);
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
        TileMetaInfoMgr::instance()->changeTilesDirectory(f);
        setTilesetList();
        updateUI();
    }
}

void TileMetaInfoDialog::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset) {
        setTilesList();
        updateUI();
    }
}

static const QString SETTINGS_KEY_FILENAME = QStringLiteral("TilesetsDialog/ExportFileName");

void TileMetaInfoDialog::exportFile()
{
    QSettings &settings = *Preferences::instance()->settings();
    QString suggestedFileName = TileMetaInfoMgr::instance()->txtPath();
    suggestedFileName = settings.value(SETTINGS_KEY_FILENAME, suggestedFileName).toString();
    QString caption = tr("Export Tilesets");
    QString fileName = QFileDialog::getSaveFileName(this, caption, suggestedFileName, QStringLiteral("Tilesets.txt files (*.txt)"));
    if (fileName.isEmpty()) {
        return;
    }
    settings.setValue(SETTINGS_KEY_FILENAME, QFileInfo(fileName).absoluteFilePath());
    fileName = QDir::toNativeSeparators(fileName);
    if (!exportTxt(fileName)) {
        return;
    }
    QMessageBox::information(this, tr("Export Tiless"), tr("Saved.\n%1").arg(fileName));
}

void TileMetaInfoDialog::importFile()
{
    QSettings &settings = *Preferences::instance()->settings();
    QString suggestedFileName = TileMetaInfoMgr::instance()->txtPath();
    suggestedFileName = settings.value(SETTINGS_KEY_FILENAME, suggestedFileName).toString();
    QString caption = tr("Import Tilesets");
    QString fileName = QFileDialog::getOpenFileName(this, caption, suggestedFileName, QStringLiteral("Tilesets.txt files (*.txt)"));
    if (fileName.isEmpty()) {
        return;
    }
    settings.setValue(SETTINGS_KEY_FILENAME, QFileInfo(fileName).absoluteFilePath());
    fileName = QDir::toNativeSeparators(fileName);
    QMessageBox::StandardButton result = QMessageBox::question(this, tr("Import Tilesets"), tr("This will replace the current tilesets with the following file:\n\n%1\n\nChoose Yes to continue or No to cancel.").arg(fileName));
    if (result == QMessageBox::StandardButton::No) {
        return;
    }
    reloadTxt(fileName);
}

void TileMetaInfoDialog::reloadFile()
{
    QSettings &settings = *Preferences::instance()->settings();
    QString fileName = settings.value(SETTINGS_KEY_FILENAME, QString()).toString();
    if (fileName.isEmpty()) {
        importFile();
        return;
    }
    if (!QFileInfo::exists(fileName) || QFileInfo(fileName).isDir()) {
        importFile();
        return;
    }
    fileName = QDir::toNativeSeparators(fileName);
    QMessageBox::StandardButton result = QMessageBox::question(this, tr("Reload Tilesets"), tr("This will replace the current tilesets with the following file:\n\n%1\n\nChoose Yes to continue or No to cancel.").arg(fileName));
    if (result == QMessageBox::StandardButton::No) {
        return;
    }
    reloadTxt(fileName);
}

void TileMetaInfoDialog::updateUI()
{
    mSynching = true;

    QString tilesDir = TileMetaInfoMgr::instance()->tilesDirectory();
    ui->editTiles->setText(QDir::toNativeSeparators(tilesDir));

    ui->actionRemove->setEnabled(mCurrentTileset != 0);
    ui->actionAddToMap->setEnabled((parent() == MainWindow::instance()) &&
                                   (DocumentManager::instance()->currentDocument() != 0));
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
    saveSettings();
    ui->tilesets->clear();
    ui->tiles->clear();
    QDialog::accept();
}

void TileMetaInfoDialog::reject()
{
    accept();
}

void TileMetaInfoDialog::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TilesetsDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("TileScale"), mZoomable->scale());
    settings.endGroup();

//    saveSplitterSizes(ui->splitter);
}

void TileMetaInfoDialog::restoreSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("TilesetsDialog"));
    QByteArray geom = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    qreal scale = settings.value(QStringLiteral("TileScale"), 0.5f).toReal();
    mZoomable->setScale(scale);
    settings.endGroup();

//    restoreSplitterSizes(ui->splitter);
}

void TileMetaInfoDialog::saveSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TilesetsDialog"));
    QVariantList v;
    foreach (int size, splitter->sizes()) {
        v += size;
    }
    settings.setValue(tr("%1/sizes").arg(splitter->objectName()), v);
    settings.endGroup();
}

void TileMetaInfoDialog::restoreSplitterSizes(QSplitter *splitter)
{
    QSettings settings;
    settings.beginGroup(QLatin1String("TilesetsDialog"));
    QVariant v = settings.value(tr("%1/sizes").arg(splitter->objectName()));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (v.canConvert(QVariant::List)) {
        QList<int> sizes;
        foreach (QVariant v2, v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
#else
    if (v.canConvert<QList<QVariant>>()) {
        QList<int> sizes;
        for (const QVariant &v2 : v.toList()) {
            sizes += v2.toInt();
        }
        splitter->setSizes(sizes);
    }
#endif
    settings.endGroup();
}

void TileMetaInfoDialog::setTilesetList()
{
    if (mClosing)
        return;

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 64;

    ui->tilesets->clear();
    foreach (Tileset *ts, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name());
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.horizontalAdvance(ts->name()));
    }
    ui->tilesets->setFixedWidth(maxWidth + 16 +
        ui->tilesets->verticalScrollBar()->sizeHint().width());

    ui->filterEdit->setFixedWidth(ui->tilesets->width());
    ui->filterEdit->setEnabled(ui->tilesets->count() > 0);
    tilesetFilterEdited(ui->filterEdit->text());
}

void TileMetaInfoDialog::setTilesList()
{
    if (mCurrentTileset) {
        QStringList labels;
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            Tile *tile = mCurrentTileset->tileAt(i);
            labels += TileMetaInfoMgr::instance()->tileEnum(tile);
        }
        ui->tiles->setTileset(mCurrentTileset, QList<void*>(), labels);
    } else {
        ui->tiles->clear();
    }
}

bool TileMetaInfoDialog::exportTxt(const QString &fileName)
{
    int revision = TileMetaInfoMgr::instance()->revision();
    int sourceRevision = TileMetaInfoMgr::instance()->sourceRevision();
    if (TileMetaInfoMgr::instance()->writeTxt(fileName, revision, sourceRevision)) {
        return true;
    }
    QMessageBox::warning(this, tr("Export Tilesets Failed"), TileMetaInfoMgr::instance()->errorString());
    return false;
}

bool TileMetaInfoDialog::reloadTxt(const QString &fileName)
{
    TilesetsTxtFile file;
    if (!file.read(fileName)) {
        QMessageBox::warning(this, tr("Reading Tilesets Failed"), file.errorString());
        return false;
    }
    mUndoStack->beginMacro(tr("Import Tilesets"));

    QMap<QString,int> enums;
    QStringList enumNames;
    file.toMgrEnums(enums, enumNames);
    mUndoStack->push(new ReplaceTileMetaMgrEnums(this, enums, enumNames));

    QList<Tileset*> newTilesets;
    QSet<QString> loadedTilesetNames;
    for (TilesetsTxtFile::Tileset *txtTileset : std::as_const(file.mTilesets)) {
        if (Tiled::Tileset* existingTileset = TileMetaInfoMgr::instance()->tileset(txtTileset->mName)) {
            mUndoStack->push(new SetTilesetEnums(this, existingTileset, txtTileset));
        } else {
            if (Tiled::Tileset *newTileset = TileMetaInfoMgr::instance()->createTilesetFromTxtFile(txtTileset)) {
                mUndoStack->push(new AddGlobalTileset(this, newTileset));
                mUndoStack->push(new SetTilesetEnums(this, newTileset, txtTileset));
                newTilesets += newTileset;
            }
        }
        loadedTilesetNames += txtTileset->mName;
    }

    const QStringList tilesetNames = TileMetaInfoMgr::instance()->tilesetNames();
    const QSet<QString> deletedTilesets = QSet<QString>(tilesetNames.constBegin(), tilesetNames.constEnd()) - loadedTilesetNames;
    for (const QString &tilesetName : deletedTilesets) {
        if (Tileset *existingTileset = TileMetaInfoMgr::instance()->tileset(tilesetName)) {
            mUndoStack->push(new RemoveGlobalTileset(this, existingTileset));
        }
    }
    mUndoStack->endMacro();
    TileMetaInfoMgr::instance()->loadTilesets(newTilesets);
    return true;
}
