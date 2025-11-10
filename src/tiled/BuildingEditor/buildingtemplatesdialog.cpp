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

#include "buildingtemplatesdialog.h"
#include "ui_buildingtemplatesdialog.h"

#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"
#include "choosetemplatesdialog.h"
#include "templatedocument.h"
#include "templateroomsdialog.h"

#include "preferences.h"
#include "tile.h"
#include "utils.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QRandomGenerator>
#include <QToolBar>
#include <QUndoGroup>
#include <QUndoStack>

using namespace BuildingEditor;

BuildingTemplatesDialog::BuildingTemplatesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTemplatesDialog),
    mTemplate(0),
    mTileRow(-1),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    {
        mUndoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
        mRedoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
        mUndoAction->setShortcuts(QKeySequence::Undo);
        mRedoAction->setShortcuts(QKeySequence::Redo);
        QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
        undoIcon.addFile(QLatin1String(":images/24x24/edit-undo.png"));
        QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
        redoIcon.addFile(QLatin1String(":images/24x24/edit-redo.png"));
        mUndoAction->setIcon(undoIcon);
        mRedoAction->setIcon(redoIcon);
        Tiled::Utils::setThemeIcon(mUndoAction, "edit-undo");
        Tiled::Utils::setThemeIcon(mRedoAction, "edit-redo");
    }

    ui->tilesList->clear();
    ui->tilesList->addItems(BuildingTemplate::enumTileNames());

    QToolBar *toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    toolBar->addAction(ui->actionAdd);
    toolBar->addAction(ui->actionDuplicate);
    toolBar->addAction(ui->actionRemove);
#if 1
    toolBar->addSeparator();
#else
    QWidget *spacerWidget = new QWidget(this);
    spacerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    toolBar->addWidget(spacerWidget);
#endif
    toolBar->addAction(ui->actionMoveUp);
    toolBar->addAction(ui->actionMoveDown);
    toolBar->addAction(ui->actionImport);
    toolBar->addAction(ui->actionExport);
    ui->toolBarLayout->addWidget(toolBar);

    for (BuildingTemplate *btemplate : mgr()->templates()) {
        BuildingTemplate *clone = new BuildingTemplate(btemplate);
        mTemplates += clone;
        ui->templatesList->addItem(btemplate->name());
    }

    connect(ui->templatesList, &QListWidget::itemSelectionChanged,
            this, &BuildingTemplatesDialog::templateSelectionChanged);
    connect(ui->actionAdd, &QAction::triggered, this, &BuildingTemplatesDialog::addTemplate);
    connect(ui->actionRemove, &QAction::triggered, this, &BuildingTemplatesDialog::removeTemplate);
    connect(ui->actionDuplicate, &QAction::triggered, this, &BuildingTemplatesDialog::duplicateTemplate);
    connect(ui->actionMoveUp, &QAction::triggered, this, &BuildingTemplatesDialog::moveUp);
    connect(ui->actionMoveDown, &QAction::triggered, this, &BuildingTemplatesDialog::moveDown);
    connect(ui->actionImport, &QAction::triggered, this, &BuildingTemplatesDialog::importTemplates);
    connect(ui->actionExport, &QAction::triggered, this, &BuildingTemplatesDialog::exportTemplates);
    connect(ui->name, &QLineEdit::textEdited, this, &BuildingTemplatesDialog::nameEdited);
    connect(ui->tilesList, &QListWidget::itemSelectionChanged,
            this, &BuildingTemplatesDialog::tileSelectionChanged);
    connect(ui->editRooms, &QAbstractButton::clicked, this, &BuildingTemplatesDialog::editRooms);
    connect(ui->tilesList, &QAbstractItemView::activated, this, &BuildingTemplatesDialog::chooseTile);
    connect(ui->clearTile, &QAbstractButton::clicked, this, &BuildingTemplatesDialog::clearTile);
    connect(ui->randomTile, &QAbstractButton::clicked, this, &BuildingTemplatesDialog::randomTile);
    connect(ui->chooseTile, &QAbstractButton::clicked, this, &BuildingTemplatesDialog::chooseTile);

    ui->templatesList->setCurrentRow(0);
    ui->tilesList->setCurrentRow(0);

//    synchUI();

    readSettings();
}

BuildingTemplatesDialog::~BuildingTemplatesDialog()
{
    delete ui;
    qDeleteAll(mTemplates);
}

void BuildingTemplatesDialog::templateSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->templatesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item == 0) {
        mTemplate = 0;
        synchUI();
        return;
    }

    int row = ui->templatesList->row(item);
    mTemplate = mTemplates.at(row);
    synchUI();
}

void BuildingTemplatesDialog::tileSelectionChanged()
{
    QList<QListWidgetItem*> selection = ui->tilesList->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    mTileRow = item ? ui->tilesList->row(item) : -1;
    synchUI();
}

void BuildingTemplatesDialog::addTemplate()
{
    BuildingTemplate *btemplate = new BuildingTemplate;
    btemplate->setName(QLatin1String("New Template"));
    for (int i = 0; i < BuildingTemplate::TileCount; i++)
        btemplate->setTile(i, BuildingTilesMgr::instance()->defaultCategoryTile(btemplate->categoryEnum(i)));

    mTemplates += btemplate;
    ui->templatesList->addItem(btemplate->name());
    ui->templatesList->setCurrentRow(ui->templatesList->count() - 1);
}

void BuildingTemplatesDialog::removeTemplate()
{
    if (!mTemplate)
        return;

    if (QMessageBox::question(this, tr("Remove Template"),
                              tr("Really remove the template '%1'?").arg(mTemplate->name()),
                              QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
        return;

    int index = mTemplates.indexOf(mTemplate);
    // Order is important here. templateSelectionChanged() will get called.
    delete ui->templatesList->takeItem(index);
    delete mTemplates.takeAt(index);
}

void BuildingTemplatesDialog::duplicateTemplate()
{
    if (!mTemplate)
        return;

    BuildingTemplate *btemplate = new BuildingTemplate(mTemplate);
    mTemplates += btemplate;
    ui->templatesList->addItem(btemplate->name());
    ui->templatesList->setCurrentRow(ui->templatesList->count() - 1);
}

void BuildingTemplatesDialog::moveUp()
{
    if (!mTemplate)
        return;

    int index = mTemplates.indexOf(mTemplate);
    if (index > 0) {
        mTemplates.takeAt(index);
        mTemplates.insert(index - 1, mTemplate);
        QListWidgetItem *item = ui->templatesList->takeItem(index);
        ui->templatesList->insertItem(index - 1, item);
        ui->templatesList->setCurrentItem(item);
    }
}

void BuildingTemplatesDialog::moveDown()
{
    if (!mTemplate)
        return;

    int index = mTemplates.indexOf(mTemplate);
    if (index < mTemplates.size() - 1) {
        mTemplates.takeAt(index);
        mTemplates.insert(index + 1, mTemplate);
        QListWidgetItem *item = ui->templatesList->takeItem(index);
        ui->templatesList->insertItem(index + 1, item);
        ui->templatesList->setCurrentItem(item);
    }
}

void BuildingTemplatesDialog::importTemplates()
{
    QString filter = tr("Text files (*.txt)");
    filter += QLatin1String(";;");
    filter += tr("All Files (*)");

    QString f = QFileDialog::getOpenFileName(this, tr("Import Templates"),
                                             QString(), filter);
    if (f.isEmpty())
        return;

    QList<BuildingTemplate*> templates;
    if (!mgr()->importTemplates(f, templates)) {
        QMessageBox::warning(this, tr("Import Templates"), mgr()->errorString());
        return;
    }

    ChooseTemplatesDialog dialog(templates, tr("Choose templates to import:"),
                                 this);
    if (dialog.exec() == QDialog::Accepted) {
        foreach (BuildingTemplate *btemplate, dialog.chosenTemplates()) {
            mTemplates += new BuildingTemplate(btemplate);
            ui->templatesList->addItem(btemplate->name());
            ui->templatesList->setCurrentRow(ui->templatesList->count() - 1);
        }
    }
    qDeleteAll(templates);
}

void BuildingTemplatesDialog::exportTemplates()
{
    ChooseTemplatesDialog dialog(mTemplates, tr("Choose templates to export:"),
                                 this);
    dialog.setButtons(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // FIXME: don't hide ChooseTemplatesDialog yet

    QString filter = tr("Text files (*.txt)");
    filter += QLatin1String(";;");
    filter += tr("All Files (*)");

    QString f = QFileDialog::getSaveFileName(this, tr("Export Templates"),
                                             QLatin1String("templates.txt"),
                                             filter);
    if (f.isEmpty())
        return;

    bool ok = mgr()->exportTemplates(f, dialog.chosenTemplates());
    if (ok) {
        f = QDir::toNativeSeparators(f);
        QMessageBox::information(this, tr("Export Templates"),
                                 tr("The templates were successfully exported to:\n%1")
                                 .arg(f));
    } else {
        QMessageBox::warning(this, tr("Export Templates"), mgr()->errorString());
    }
}

void BuildingTemplatesDialog::nameEdited(const QString &name)
{
    if (!mTemplate)
        return;

    int index = mTemplates.indexOf(mTemplate);
    mTemplate->setName(name);
    ui->templatesList->item(index)->setText(name);
}

void BuildingTemplatesDialog::editRooms()
{
    /// TODO: Full undo-redo for edits made by this dialog.
    /// Currently, only TemplateRoomsDialog uses undo/redo.
    mUndoStack->clear();

    TemplateDocument document(mTemplate);
    TemplateRoomsDialog dialog(&document, nullptr, this);
    dialog.setWindowTitle(tr("Rooms in '%1'").arg(mTemplate->name()));
    if (dialog.exec() == QDialog::Accepted) {
//        mTemplate->clearRooms();
//        foreach (Room *dialogRoom, dialog.rooms())
//            mTemplate->addRoom(new Room(dialogRoom));
    }
}

void BuildingTemplatesDialog::clearTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mTemplate->categoryEnum(mTileRow));
    if (category->canAssignNone()) {
        mTemplate->setTile(mTileRow, category->noneTileEntry());
        synchUI();
    }
}

void BuildingTemplatesDialog::randomTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mTemplate->categoryEnum(mTileRow));
    QList<BuildingTileEntry*> entries = category->entries();
    if (category->canAssignNone()) {
        entries += category->noneTileEntry();
    }
    QRandomGenerator *rand = QRandomGenerator::global();
    mTemplate->setTile(mTileRow, entries.at(rand->bounded(entries.size())));
    synchUI();
}

void BuildingTemplatesDialog::chooseTile()
{
    BuildingTileCategory *category = BuildingTilesMgr::instance()->category(
                mTemplate->categoryEnum(mTileRow));
    ChooseBuildingTileDialog dialog(tr("Choose %1 tile for '%2'")
                                    .arg(category->label())
                                    .arg(mTemplate->name()),
                                    category,
                                    selectedTile(), this);
    if (dialog.exec() == QDialog::Accepted) {
        if (BuildingTileEntry *entry = dialog.selectedTile()) {
            mTemplate->setTile(mTileRow, entry);
            synchUI();
        }
    }
}

void BuildingTemplatesDialog::synchUI()
{
    const bool hasTemplate = mTemplate != nullptr;
    ui->name->setEnabled(hasTemplate);
    ui->actionRemove->setEnabled(hasTemplate);
    ui->actionDuplicate->setEnabled(hasTemplate);
    ui->actionMoveUp->setEnabled(hasTemplate &&mTemplates.indexOf(mTemplate) > 0);
    ui->actionMoveDown->setEnabled(hasTemplate && mTemplates.indexOf(mTemplate) < mTemplates.count() - 1);
    ui->actionExport->setEnabled(mTemplates.size() > 0);
    ui->tilesList->setEnabled(hasTemplate);
    bool enabled = false;
    if ((selectedTile() != nullptr) && !selectedTile()->isNone()) {
        BuildingTileCategory *category = BuildingTilesMgr::instance()->category(mTemplate->categoryEnum(mTileRow));
        enabled = category->canAssignNone();
    }
    ui->clearTile->setEnabled(enabled);
    ui->randomTile->setEnabled(hasTemplate && mTileRow != -1);
    ui->chooseTile->setEnabled(hasTemplate && mTileRow != -1);
    ui->editRooms->setEnabled(hasTemplate);

    if (hasTemplate) {
        ui->name->setText(mTemplate->name());
    } else {
        ui->name->clear();
        ui->tilesList->clearSelection();
    }
    setTilePixmap();
}

void BuildingTemplatesDialog::accept()
{
    saveSettings();
    QDialog::accept();
}

void BuildingTemplatesDialog::reject()
{
    saveSettings();
    QDialog::reject();
}

void BuildingTemplatesDialog::saveSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("BuildingTemplatesDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.endGroup();
}

void BuildingTemplatesDialog::readSettings()
{
    QSettings &settings = BuildingPreferences::instance()->settings();
    settings.beginGroup(QLatin1String("BuildingTemplatesDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    settings.endGroup();
}

void BuildingTemplatesDialog::setTilePixmap()
{
    if (BuildingTileEntry *entry = selectedTile()) {
        Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile());
        QPixmap pixmap(64, 128);
        pixmap.fill(Tiled::Internal::Preferences::instance()->tilesetBackgroundColor());
        QPainter painter(&pixmap);
        painter.drawImage(0, 0, tile->finalImage(64, 128));
        painter.end();
        ui->tileLabel->setPixmap(pixmap);
    } else {
        ui->tileLabel->clear();
    }
}

BuildingTileEntry *BuildingTemplatesDialog::selectedTile()
{
    if (mTemplate == 0 || mTileRow == -1)
        return 0;

    return mTemplate->tile(mTileRow);

    return 0;
}

BuildingTemplates *BuildingTemplatesDialog::mgr() const
{
    return BuildingTemplates::instance();
}
