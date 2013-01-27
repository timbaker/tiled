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

#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "choosebuildingtiledialog.h"
#include "choosetemplatesdialog.h"
#include "roomsdialog.h"

#include "tile.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QToolBar>

using namespace BuildingEditor;

BuildingTemplatesDialog::BuildingTemplatesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BuildingTemplatesDialog),
    mTemplate(0),
    mTileRow(-1)
{
    ui->setupUi(this);

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

    foreach (BuildingTemplate *btemplate, mgr()->templates()) {
        BuildingTemplate *clone = new BuildingTemplate(btemplate);
        mTemplates += clone;
        ui->templatesList->addItem(btemplate->name());
    }

    connect(ui->templatesList, SIGNAL(itemSelectionChanged()),
            SLOT(templateSelectionChanged()));
    connect(ui->actionAdd, SIGNAL(triggered()), SLOT(addTemplate()));
    connect(ui->actionRemove, SIGNAL(triggered()), SLOT(removeTemplate()));
    connect(ui->actionDuplicate, SIGNAL(triggered()), SLOT(duplicateTemplate()));
    connect(ui->actionMoveUp, SIGNAL(triggered()), SLOT(moveUp()));
    connect(ui->actionMoveDown, SIGNAL(triggered()), SLOT(moveDown()));
    connect(ui->actionImport, SIGNAL(triggered()), SLOT(importTemplates()));
    connect(ui->actionExport, SIGNAL(triggered()), SLOT(exportTemplates()));
    connect(ui->name, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));
    connect(ui->tilesList, SIGNAL(itemSelectionChanged()),
            SLOT(tileSelectionChanged()));
    connect(ui->editRooms, SIGNAL(clicked()), SLOT(editRooms()));
    connect(ui->tilesList, SIGNAL(activated(QModelIndex)), SLOT(chooseTile()));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));

    ui->templatesList->setCurrentRow(0);
    ui->tilesList->setCurrentRow(0);

//    synchUI();
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
    RoomsDialog dialog(mTemplate->rooms(), this);
    dialog.setWindowTitle(tr("Rooms in '%1'").arg(mTemplate->name()));
    if (dialog.exec() == QDialog::Accepted) {
        mTemplate->clearRooms();
        foreach (Room *dialogRoom, dialog.rooms())
            mTemplate->addRoom(new Room(dialogRoom));
    }
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
            setTilePixmap();
        }
    }
}

void BuildingTemplatesDialog::synchUI()
{
    ui->name->setEnabled(mTemplate != 0);
    ui->actionRemove->setEnabled(mTemplate != 0);
    ui->actionDuplicate->setEnabled(mTemplate != 0);
    ui->actionMoveUp->setEnabled(mTemplate != 0 &&
            mTemplates.indexOf(mTemplate) > 0);
    ui->actionMoveDown->setEnabled(mTemplate != 0 &&
            mTemplates.indexOf(mTemplate) < mTemplates.count() - 1);
    ui->actionExport->setEnabled(mTemplates.size() > 0);
    ui->tilesList->setEnabled(mTemplate != 0);
    ui->chooseTile->setEnabled(mTemplate != 0 && mTileRow != -1);
    ui->editRooms->setEnabled(mTemplate != 0);

    if (mTemplate != 0) {
        ui->name->setText(mTemplate->name());
    } else {
        ui->name->clear();
        ui->tilesList->clearSelection();
    }
    setTilePixmap();
}

void BuildingTemplatesDialog::setTilePixmap()
{
    if (BuildingTileEntry *entry = selectedTile()) {
        Tiled::Tile *tile = BuildingTilesMgr::instance()->tileFor(entry->displayTile());
        ui->tileLabel->setPixmap(tile->image());
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
