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

#include "choosetemplatesdialog.h"
#include "ui_choosetemplatesdialog.h"

#include "buildingtemplates.h"

using namespace BuildingEditor;

ChooseTemplatesDialog::ChooseTemplatesDialog(const QList<BuildingTemplate *> &templates,
                                             const QString &prompt, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChooseTemplatesDialog),
    mTemplates(templates)
{
    ui->setupUi(this);

    ui->prompt->setText(prompt);

    connect(ui->checkAll, SIGNAL(clicked()), SLOT(checkAll()));
    connect(ui->uncheckAll, SIGNAL(clicked()), SLOT(uncheckAll()));

    setList();
}

ChooseTemplatesDialog::~ChooseTemplatesDialog()
{
    delete ui;
}

void ChooseTemplatesDialog::setButtons(QDialogButtonBox::StandardButtons buttons)
{
    ui->buttonBox->setStandardButtons(buttons);
}

QList<BuildingTemplate *> ChooseTemplatesDialog::chosenTemplates() const
{
    QList<BuildingTemplate *> ret;
    for (int i = 0; i < ui->list->count(); i++) {
        QListWidgetItem *item = ui->list->item(i);
        if (item->checkState() == Qt::Checked)
            ret += mTemplates.at(i);
    }
    return ret;
}

void ChooseTemplatesDialog::checkAll()
{
    for (int i = 0; i < ui->list->count(); i++)
        ui->list->item(i)->setCheckState(Qt::Checked);
}

void ChooseTemplatesDialog::uncheckAll()
{
    for (int i = 0; i < ui->list->count(); i++)
        ui->list->item(i)->setCheckState(Qt::Unchecked);
}

void ChooseTemplatesDialog::setList()
{
    ui->list->clear();

    foreach (BuildingTemplate *btemplate, mTemplates) {
        QListWidgetItem *item = new QListWidgetItem;
        item->setText(btemplate->name());
        item->setCheckState(Qt::Checked);
        ui->list->addItem(item);
    }
}
