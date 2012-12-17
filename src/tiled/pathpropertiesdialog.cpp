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

#include "pathpropertiesdialog.h"
#include "ui_pathpropertiesdialog.h"

#include "mapdocument.h"
#include "pathgeneratorsdialog.h"
#include "pathgeneratormgr.h"

#include "map.h"
#include "pathgenerator.h"
#include "pathlayer.h"
#include "tile.h"
#include "tileset.h"

using namespace Tiled;
using namespace Internal;

PathPropertiesDialog::PathPropertiesDialog(QWidget *parent) :
    QDialog(parent),
    mMapDocument(0),
    mPath(0),
    mCurrentGenerator(0),
    mCurrentProperty(0),
    mCurrentGeneratorTemplate(0),
    ui(new Ui::PathPropertiesDialog)
{
    ui->setupUi(this);

    connect(ui->generatorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorChanged(int)));
    connect(ui->propertyList, SIGNAL(currentRowChanged(int)),
            SLOT(currentPropertyChanged(int)));
    connect(ui->allGeneratorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorTemplateChanged(int)));
    connect(ui->checkBox, SIGNAL(toggled(bool)), SLOT(booleanToggled(bool)));
    connect(ui->integerSpinBox, SIGNAL(valueChanged(int)),
            SLOT(integerValueChanged(int)));
    connect(ui->unlink, SIGNAL(clicked()), SLOT(unlink()));
    connect(ui->removeGenerator, SIGNAL(clicked()), SLOT(removeGenerator()));
    connect(ui->addGenerator, SIGNAL(clicked()), SLOT(addGenerator()));
    connect(ui->generatorsDialog, SIGNAL(clicked()), SLOT(generatorsDialog()));
    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));

    setGeneratorTypesList();

    ui->generatorsList->setCurrentRow(0);
    ui->propertyList->setCurrentRow(0);
}

PathPropertiesDialog::~PathPropertiesDialog()
{
    delete ui;
}

void PathPropertiesDialog::setPath(MapDocument *doc, Path *path)
{
    mMapDocument = doc;
    mPath = path;
    setGeneratorsList();
}

void PathPropertiesDialog::currentGeneratorChanged(int row)
{
    mCurrentGenerator = 0;
    mCurrentProperty = 0;
    if (row >= 0) {
        mCurrentGenerator = mPath->generators().at(row);
        setPropertyList();
    }
    synchUI();
}

void PathPropertiesDialog::currentPropertyChanged(int row)
{
    mCurrentProperty = 0;
    if (row >= 0 && mCurrentGenerator) {
        mCurrentProperty = mCurrentGenerator->properties().at(row);
        mPropertyName = mCurrentProperty->name();
    }
    setPropertyPage();
    synchUI();
}

void PathPropertiesDialog::currentGeneratorTemplateChanged(int row)
{
    mCurrentGeneratorTemplate = 0;
    if (row >= 0) {
        mCurrentGeneratorTemplate = PathGeneratorMgr::instance()->generators().at(row);
    }
    synchUI();
}

void PathPropertiesDialog::nameEdited(const QString &text)
{
    if (mSynching || !mCurrentGenerator)
        return;
    mCurrentGenerator->setLabel(text);
    int row = mPath->generators().indexOf(mCurrentGenerator);
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(row);
}

void PathPropertiesDialog::booleanToggled(bool newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        p->mValue = newValue;
    }
}

void PathPropertiesDialog::integerValueChanged(int newValue)
{
    if (mSynching || !mMapDocument || !mCurrentProperty)
        return;
    if (PGP_Integer *p = mCurrentProperty->asInteger()) {
        p->mValue = newValue;
    }
}

void PathPropertiesDialog::unlink()
{
    if (isCurrentGeneratorShared()) {
        PathGenerator *clone = mCurrentGenerator->clone();
        int index = mPath->generators().indexOf(mCurrentGenerator);
        mPath->removeGenerator(index);
        mPath->insertGenerator(index, clone);
        mCurrentGenerator = clone;
        synchUI();
    }
}

void PathPropertiesDialog::removeGenerator()
{
    if (mCurrentGenerator) {
        int index = mPath->generators().indexOf(mCurrentGenerator);
        mPath->removeGenerator(index);
        if (!isCurrentGeneratorShared())
            delete mCurrentGenerator;
        setGeneratorsList();
        index = qMin(index, ui->generatorsList->count() - 1);
        ui->generatorsList->setCurrentRow(index);
    }
}

void PathPropertiesDialog::addGenerator()
{
    int index = ui->generatorsList->currentRow() + 1;
    if (index == 0)
        index = mPath->generators().size();
    mPath->insertGenerator(index, mCurrentGeneratorTemplate);
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(index);
}

void PathPropertiesDialog::synchUI()
{
    ui->nameEdit->setText(mCurrentGenerator ? mCurrentGenerator->label() : QString());
    ui->nameEdit->setEnabled(!isCurrentGeneratorShared());

    ui->moveUp->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() > 0);
    ui->moveDown->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() < mPath->generators().size() - 1);
    ui->unlink->setEnabled(isCurrentGeneratorShared());
    ui->removeGenerator->setEnabled(mCurrentGenerator);
    ui->addGenerator->setEnabled(mCurrentGeneratorTemplate);

    ui->propertyStack->setEnabled(mCurrentProperty && !isCurrentGeneratorShared());
}

void PathPropertiesDialog::generatorsDialog()
{
    PathGeneratorsDialog dialog(this);
    dialog.exec();
    setGeneratorTypesList();
}

void PathPropertiesDialog::setGeneratorsList()
{
    ui->generatorsList->clear();
    foreach (PathGenerator *pgen, mPath->generators())
        ui->generatorsList->addItem(pgen->label());
}

void PathPropertiesDialog::setGeneratorTypesList()
{
    ui->allGeneratorsList->clear();
    foreach (PathGenerator *pgen, PathGeneratorMgr::instance()->generators())
        ui->allGeneratorsList->addItem(pgen->label());
}

void PathPropertiesDialog::setPropertyList()
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

void PathPropertiesDialog::setPropertyPage()
{
    mSynching = true;

    if (!mCurrentProperty) {
        if (!ui->nameEdit->text().isEmpty()) {
            ui->nameEdit->clear();
            ui->tileLabel->clear();
            ui->integerSpinBox->clear();
            ui->stringEdit->clear();
        }
    } else if (PGP_Tile *p = mCurrentProperty->asTile()) {
        ui->propertyStack->setCurrentWidget(ui->Tile);
        QPixmap pixmap;
        if (Tileset *ts = findTileset(p->mTilesetName, mPath->pathLayer()->map()->tilesets())) {
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

bool PathPropertiesDialog::isCurrentGeneratorShared()
{
    return mCurrentGenerator && (mCurrentGenerator->refCount() > 1);
            /*PathGeneratorMgr::instance()->generators().contains(mCurrentGenerator)*/;
}
