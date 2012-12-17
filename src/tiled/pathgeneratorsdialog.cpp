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

#include "pathgeneratorsdialog.h"
#include "ui_pathgeneratorsdialog.h"

#include "pathgeneratormgr.h"

#include "map.h"
#include "pathgenerator.h"
#include "tile.h"
#include "tileset.h"

#include <QMessageBox>

using namespace Tiled;
using namespace Internal;

PathGeneratorsDialog::PathGeneratorsDialog(QWidget *parent) :
    QDialog(parent),
    mGenerators(PathGeneratorMgr::instance()->generators()),
    mCurrentGenerator(0),
    mCurrentProperty(0),
    mCurrentGeneratorTemplate(0),
    mSynching(false),
    ui(new Ui::PathGeneratorsDialog)
{
    ui->setupUi(this);

    connect(ui->generatorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorChanged(int)));
    connect(ui->propertyList, SIGNAL(currentRowChanged(int)),
            SLOT(currentPropertyChanged(int)));
    connect(ui->generatorTypesList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorTemplateChanged(int)));
    connect(ui->checkBox, SIGNAL(toggled(bool)), SLOT(booleanToggled(bool)));
    connect(ui->integerSpinBox, SIGNAL(valueChanged(int)),
            SLOT(integerValueChanged(int)));
    connect(ui->addGenerator, SIGNAL(clicked()), SLOT(addGenerator()));
    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));

    setGeneratorsList();

    ui->generatorTypesList->clear();
    foreach (PathGenerator *pgen, PathGeneratorMgr::instance()->generatorTypes())
        ui->generatorTypesList->addItem(pgen->type());

    ui->generatorsList->setCurrentRow(0);
    ui->propertyList->setCurrentRow(0);
}

PathGeneratorsDialog::~PathGeneratorsDialog()
{
    delete ui;
}

void PathGeneratorsDialog::currentGeneratorChanged(int row)
{
    mCurrentGenerator = 0;
    mCurrentProperty = 0;
    if (row >= 0) {
        mCurrentGenerator = mGenerators.at(row);
        setPropertyList();
    }
    synchUI();
}

void PathGeneratorsDialog::currentPropertyChanged(int row)
{
    mCurrentProperty = 0;
    if (row >= 0 && mCurrentGenerator) {
        mCurrentProperty = mCurrentGenerator->properties().at(row);
        mPropertyName = mCurrentProperty->name();
    }
    setPropertyPage();
    synchUI();
}

void PathGeneratorsDialog::currentGeneratorTemplateChanged(int row)
{
    mCurrentGeneratorTemplate = 0;
    if (row >= 0) {
        mCurrentGeneratorTemplate = PathGeneratorMgr::instance()->generatorTypes().at(row);
    }
    synchUI();
}

void PathGeneratorsDialog::nameEdited(const QString &text)
{
    if (mSynching || !mCurrentGenerator)
        return;
    mCurrentGenerator->setLabel(text);
    int row = mGenerators.indexOf(mCurrentGenerator);
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(row);
}

void PathGeneratorsDialog::booleanToggled(bool newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        p->mValue = newValue;
    }
}

void PathGeneratorsDialog::integerValueChanged(int newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Integer *p = mCurrentProperty->asInteger()) {
        p->mValue = newValue;
    }
}

void PathGeneratorsDialog::addGenerator()
{
    int index = ui->generatorsList->currentRow() + 1;
    if (index == 0)
        index = PathGeneratorMgr::instance()->generators().size();
    PathGenerator *clone = mCurrentGeneratorTemplate->clone();
    PathGeneratorMgr::instance()->insertGenerator(index, clone);
    mGenerators = PathGeneratorMgr::instance()->generators();
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(index);
}

void PathGeneratorsDialog::synchUI()
{
    ui->nameEdit->setText(mCurrentGenerator ? mCurrentGenerator->label() : QString());
    ui->moveUp->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() > 0);
    ui->moveDown->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() < mGenerators.size() - 1);
    ui->addGenerator->setEnabled(mCurrentGeneratorTemplate != 0);
}

void PathGeneratorsDialog::accept()
{
    if (!PathGeneratorMgr::instance()->writeTxt()) {
        QMessageBox::warning(this, tr("It's no good, Jim!"),
                             PathGeneratorMgr::instance()->errorString());
    }
    QDialog::accept();
}

void PathGeneratorsDialog::setGeneratorsList()
{
    ui->generatorsList->clear();
    foreach (PathGenerator *pgen, mGenerators)
        ui->generatorsList->addItem(pgen->label());
}

void PathGeneratorsDialog::setPropertyList()
{
    ui->propertyList->clear();
    if (mCurrentGenerator) {
        int row = 0;
        foreach (PathGeneratorProperty *prop, mCurrentGenerator->properties()) {
            ui->propertyList->addItem(prop->name());
            if (prop->name() == mPropertyName)
                ui->propertyList->setCurrentRow(row);
            ++row;
        }
    }
}

static Tileset *findTileset(const QString &name, const QList<Tileset*> &tilesets)
{
    foreach (Tileset *ts, tilesets) {
        if (ts->name() == name)
            return ts;
    }
    return 0;
}

void PathGeneratorsDialog::setPropertyPage()
{
    mSynching = true;

    ui->propertyStack->setEnabled(true);
    if (!mCurrentProperty) {
        ui->propertyStack->setEnabled(false);
        if (!ui->nameEdit->text().isEmpty()) {
            ui->nameEdit->clear();
            ui->tileLabel->clear();
            ui->integerSpinBox->clear();
            ui->stringEdit->clear();
        }
    } else if (PGP_Tile *p = mCurrentProperty->asTile()) {
        ui->propertyStack->setCurrentWidget(ui->Tile);
        QPixmap pixmap;
        if (Tileset *ts = findTileset(p->mTilesetName, PathGeneratorMgr::instance()->tilesets())) {
            if (Tile *tile = ts->tileAt(p->mTileID))
                pixmap = tile->image();
        }
        ui->tileLabel->setPixmap(pixmap);
    } else if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        ui->propertyStack->setCurrentWidget(ui->Boolean);
        ui->checkBox->setChecked(p->mValue);
    } else if (PGP_Integer *p = mCurrentProperty->asInteger()) {
        ui->propertyStack->setCurrentWidget(ui->Integer);
        ui->integerSpinBox->setRange(p->mMin, p->mMax);
        ui->integerSpinBox->setValue(p->mValue);
    } else if (PGP_String *p = mCurrentProperty->asString()) {
        ui->propertyStack->setCurrentWidget(ui->String);
        ui->stringEdit->setText(p->mValue);
    } else if (PGP_Layer *p = mCurrentProperty->asLayer()) {
        ui->propertyStack->setCurrentWidget(ui->String);
        ui->stringEdit->setText(p->mValue);
    }

    mSynching = false;
}
