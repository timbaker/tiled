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
#include "pathgeneratortilesdialog.h"
#include "utils.h"

#include "map.h"
#include "pathgenerator.h"
#include "tile.h"
#include "tileset.h"

#include <QMessageBox>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Internal;

namespace PathGeneratorUndoRedo
{

enum {
    Cmd_ChangePropertyValue = 1000
};

class AddGenerator : public QUndoCommand
{
public:
    AddGenerator(PathGeneratorsDialog *d, int index, PathGenerator *pgen) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Generator")),
        mDialog(d),
        mIndex(index),
        mGenerator(pgen)
    {
    }

    ~AddGenerator()
    {
        // FIXME: delete mGenerator
    }

    void undo()
    {
        mGenerator = mDialog->removeGenerator(mIndex);
    }

    void redo()
    {
        mDialog->addGenerator(mIndex, mGenerator);
        mGenerator = 0;
    }

    PathGeneratorsDialog *mDialog;
    int mIndex;
    PathGenerator *mGenerator;
};

class RemoveGenerator : public QUndoCommand
{
public:
    RemoveGenerator(PathGeneratorsDialog *d, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Generator")),
        mDialog(d),
        mIndex(index),
        mGenerator(0)
    {
    }

    ~RemoveGenerator()
    {
        // FIXME: delete mGenerator
    }

    void undo()
    {
        mDialog->addGenerator(mIndex, mGenerator);
        mGenerator = 0;
    }

    void redo()
    {
        mGenerator = mDialog->removeGenerator(mIndex);
    }

    PathGeneratorsDialog *mDialog;
    int mIndex;
    PathGenerator *mGenerator;
};

class ReorderGenerator : public QUndoCommand
{
public:
    ReorderGenerator(PathGeneratorsDialog *d, int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Generator")),
        mDialog(d),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        mDialog->reorderGenerator(mNewIndex, mOldIndex);
    }

    void redo()
    {
        mDialog->reorderGenerator(mOldIndex, mNewIndex);
    }

    PathGeneratorsDialog *mDialog;
    int mOldIndex;
    int mNewIndex;
};

class ChangePropertyValue : public QUndoCommand
{
public:
    ChangePropertyValue(PathGeneratorsDialog *d, PathGeneratorProperty *prop,
                        const QString &newValue, bool mergeable = false) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Property Value")),
        mDialog(d),
        mProperty(prop),
        mValue(newValue),
        mMergeable(mergeable)
    {
    }

    int id() const { return Cmd_ChangePropertyValue; }

    bool mergeWith(const QUndoCommand *other)
    {
        const ChangePropertyValue *o = static_cast<const ChangePropertyValue*>(other);
        if (!(o && mMergeable && o->mProperty == mProperty))
            return false;
        return true;
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mValue = mDialog->changePropertyValue(mProperty, mValue);
    }

    PathGeneratorsDialog *mDialog;
    PathGeneratorProperty *mProperty;
    QString mValue;
    bool mMergeable;
};

} // namespace PathGeneratorUndoRedo

using namespace PathGeneratorUndoRedo;

PathGeneratorsDialog::PathGeneratorsDialog(QWidget *parent) :
    QDialog(parent),
    mGenerators(PathGeneratorMgr::instance()->generators()),
    mCurrentGenerator(0),
    mCurrentProperty(0),
    mCurrentGeneratorTemplate(0),
    mSynching(false),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this)),
    mWasEditingString(false),
    ui(new Ui::PathGeneratorsDialog)
{
    ui->setupUi(this);

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
    connect(mUndoGroup, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));
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

    /////

    connect(ui->generatorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorChanged(int)));
    connect(ui->propertyList, SIGNAL(currentRowChanged(int)),
            SLOT(currentPropertyChanged(int)));
    connect(ui->propertyList, SIGNAL(activated(QModelIndex)),
            SLOT(propertyActivated(QModelIndex)));
    connect(ui->generatorTypesList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorTemplateChanged(int)));
    connect(ui->generatorTypesList, SIGNAL(activated(QModelIndex)),
            SLOT(addGenerator()));

    connect(ui->removeGenerator, SIGNAL(clicked()), SLOT(removeGenerator()));
    connect(ui->duplicate, SIGNAL(clicked()), SLOT(duplicate()));
    connect(ui->moveUp, SIGNAL(clicked()), SLOT(moveUp()));
    connect(ui->moveDown, SIGNAL(clicked()), SLOT(moveDown()));

    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));
    connect(ui->checkBox, SIGNAL(toggled(bool)), SLOT(booleanToggled(bool)));
    connect(ui->integerSpinBox, SIGNAL(valueChanged(int)),
            SLOT(integerValueChanged(int)));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));
    connect(ui->clearTile, SIGNAL(clicked()), SLOT(clearTile()));
    connect(ui->stringEdit, SIGNAL(textEdited(QString)), SLOT(stringEdited(QString)));

    connect(ui->addGenerator, SIGNAL(clicked()), SLOT(addGenerator()));

    setGeneratorsList();

    ui->generatorTypesList->clear();
    foreach (PathGenerator *pgen, PathGeneratorMgr::instance()->generatorTypes())
        ui->generatorTypesList->addItem(pgen->type());

    ui->generatorsList->setCurrentRow(0);
    ui->propertyList->setCurrentRow(0);

    PathGeneratorMgr::instance()->loadTilesets(); // FIXME: move this
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
    mWasEditingString = false;
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
    mWasEditingString = false;
}

void PathGeneratorsDialog::currentGeneratorTemplateChanged(int row)
{
    mCurrentGeneratorTemplate = 0;
    if (row >= 0) {
        mCurrentGeneratorTemplate = PathGeneratorMgr::instance()->generatorTypes().at(row);
    }
    synchUI();
}

void PathGeneratorsDialog::removeGenerator()
{
    if (!mCurrentGenerator)
         return;
    int index = mGenerators.indexOf(mCurrentGenerator);
    mUndoStack->push(new RemoveGenerator(this, index));
}

void PathGeneratorsDialog::duplicate()
{
    if (!mCurrentGenerator)
         return;
    PathGenerator *clone = mCurrentGenerator->clone();
    int index = mGenerators.indexOf(mCurrentGenerator);
    mUndoStack->push(new AddGenerator(this, index + 1, clone));
}

void PathGeneratorsDialog::moveUp()
{
    if (!mCurrentGenerator)
         return;
    int index = mGenerators.indexOf(mCurrentGenerator);
    if (index == 0)
        return;
    mUndoStack->push(new ReorderGenerator(this, index, index - 1));
}

void PathGeneratorsDialog::moveDown()
{
    if (!mCurrentGenerator)
         return;
    int index = mGenerators.indexOf(mCurrentGenerator);
    if (index == mGenerators.size() - 1)
        return;
    mUndoStack->push(new ReorderGenerator(this, index, index + 1));
}

void PathGeneratorsDialog::propertyActivated(const QModelIndex &index)
{
    Q_UNUSED(index)
    if (!mCurrentProperty)
        return;
    if (PGP_Tile *prop = mCurrentProperty->asTile())
        chooseTile();
}

void PathGeneratorsDialog::nameEdited(const QString &text)
{
    if (mSynching || !mCurrentGenerator)
        return;
    mCurrentGenerator->setLabel(text);
    int row = mGenerators.indexOf(mCurrentGenerator);
    ui->generatorsList->item(row)->setText(text);
}

void PathGeneratorsDialog::booleanToggled(bool newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        mUndoStack->push(new ChangePropertyValue(this, p,
                                                 QLatin1String(newValue
                                                               ? "true"
                                                               : "false")));
    }
}

void PathGeneratorsDialog::integerValueChanged(int newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Integer *p = mCurrentProperty->asInteger()) {
        mUndoStack->push(new ChangePropertyValue(this, p, QString::number(newValue)));
    }
}

void PathGeneratorsDialog::chooseTile()
{
    if (PGP_TileEntry *prop = mCurrentProperty->asTileEntry()) {

    }

    if (PGP_Tile *prop = mCurrentProperty->asTile()) {
        PathGeneratorTilesDialog *dialog = PathGeneratorTilesDialog::instance();
        QWidget *saveParent = dialog->parentWidget();
        dialog->reparent(this);

        dialog->setInitialTile(prop->mTilesetName, prop->mTileID);
        if (dialog->exec() == QDialog::Accepted) {
            if (Tile *tile = dialog->selectedTile()) {
                mUndoStack->push(new ChangePropertyValue(this, prop,
                                                         prop->tileName(
                                                             tile->tileset()->name(),
                                                             tile->id())));
            }
        }

        dialog->reparent(saveParent);
    }
}

void PathGeneratorsDialog::clearTile()
{
    if (PGP_TileEntry *prop = mCurrentProperty->asTileEntry()) {

    }
    if (PGP_Tile *prop = mCurrentProperty->asTile())
        mUndoStack->push(new ChangePropertyValue(this, prop, QString()));
}

void PathGeneratorsDialog::stringEdited(const QString &text)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (mCurrentProperty->asString() || mCurrentProperty->asLayer()) {
        mUndoStack->push(new ChangePropertyValue(this, mCurrentProperty, text,
                                                 !mWasEditingString));
        mWasEditingString = true;
    }
}

void PathGeneratorsDialog::addGenerator()
{
    int index = ui->generatorsList->currentRow() + 1;
    if (index == 0)
        index = PathGeneratorMgr::instance()->generators().size();
    PathGenerator *clone = mCurrentGeneratorTemplate->clone();
    mUndoStack->push(new AddGenerator(this, index, clone));
}

void PathGeneratorsDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void PathGeneratorsDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void PathGeneratorsDialog::synchUI()
{
    ui->nameEdit->setText(mCurrentGenerator ? mCurrentGenerator->label() : QString());
    ui->duplicate->setEnabled(mCurrentGenerator != 0);
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
            ui->tileName->clear();
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
        ui->tileName->setText(p->valueToString());
    } else if (PGP_TileEntry *p0 = mCurrentProperty->asTileEntry()) {
        PGP_Tile *p = p0->properties()[p0->mDisplayIndex]->asTile();
        ui->propertyStack->setCurrentWidget(ui->Tile);
        QPixmap pixmap;
        if (Tileset *ts = findTileset(p->mTilesetName, PathGeneratorMgr::instance()->tilesets())) {
            if (Tile *tile = ts->tileAt(p->mTileID))
                pixmap = tile->image();
        }
        ui->tileLabel->setPixmap(pixmap);
        ui->tileName->setText(p->valueToString());
    } else if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        ui->propertyStack->setCurrentWidget(ui->Boolean);
        ui->checkBox->setChecked(p->mValue);
        ui->checkBox->setText(p->name());
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

void PathGeneratorsDialog::addGenerator(int index, PathGenerator *pgen)
{
    PathGeneratorMgr::instance()->insertGenerator(index, pgen);
    mGenerators = PathGeneratorMgr::instance()->generators();
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(index);
}

PathGenerator *PathGeneratorsDialog::removeGenerator(int index)
{
    PathGenerator *pgen = PathGeneratorMgr::instance()->removeGenerator(index);
    mGenerators = PathGeneratorMgr::instance()->generators();
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(index);
    return pgen;
}

void PathGeneratorsDialog::reorderGenerator(int oldIndex, int newIndex)
{
    PathGenerator *pgen = PathGeneratorMgr::instance()->removeGenerator(oldIndex);
    PathGeneratorMgr::instance()->insertGenerator(newIndex, pgen);
    mGenerators = PathGeneratorMgr::instance()->generators();
    setGeneratorsList();
    ui->generatorsList->setCurrentRow(newIndex);
}

QString PathGeneratorsDialog::changePropertyValue(PathGeneratorProperty *prop, const QString &newValue)
{
    QString old = prop->valueToString();
    prop->valueFromString(newValue);
    setPropertyPage();
    return old;
}
