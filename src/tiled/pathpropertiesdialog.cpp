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

#include "addremovetileset.h"
#include "mapdocument.h"
#include "pathgeneratorsdialog.h"
#include "pathgeneratormgr.h"
#include "pathgeneratortilesdialog.h"
#include "utils.h"

#include "map.h"
#include "pathgenerator.h"
#include "pathlayer.h"
#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QUndoCommand>

using namespace Tiled;
using namespace Internal;

namespace PathUndoRedo
{

enum {
    Cmd_ChangePropertyValue = 1000
};

class AddGenerator : public QUndoCommand
{
public:
    AddGenerator(MapDocument *mapDoc, Path *path, int index, PathGenerator *pgen) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Generator to Path")),
        mMapDocument(mapDoc),
        mPath(path),
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
        mGenerator = mMapDocument->removePathGenerator(mPath, mIndex);
    }

    void redo()
    {
        mMapDocument->addPathGenerator(mPath, mIndex, mGenerator);
        mGenerator = 0;
    }

    MapDocument *mMapDocument;
    Path *mPath;
    int mIndex;
    PathGenerator *mGenerator;
};

class RemoveGenerator : public QUndoCommand
{
public:
    RemoveGenerator(MapDocument *mapDoc, Path *path, int index) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Generator from Path")),
        mMapDocument(mapDoc),
        mPath(path),
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
        mMapDocument->addPathGenerator(mPath, mIndex, mGenerator);
        mGenerator = 0;
    }

    void redo()
    {
        mGenerator = mMapDocument->removePathGenerator(mPath, mIndex);
    }

    MapDocument *mMapDocument;
    Path *mPath;
    int mIndex;
    PathGenerator *mGenerator;
};

class ReorderGenerator : public QUndoCommand
{
public:
    ReorderGenerator(MapDocument *mapDoc, Path *path, int oldIndex, int newIndex) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Reorder Generator in Path")),
        mMapDocument(mapDoc),
        mPath(path),
        mOldIndex(oldIndex),
        mNewIndex(newIndex)
    {
    }

    void undo()
    {
        mMapDocument->reorderPathGenerator(mPath, mNewIndex, mOldIndex);
    }

    void redo()
    {
        mMapDocument->reorderPathGenerator(mPath ,mOldIndex, mNewIndex);
    }

    MapDocument *mMapDocument;
    Path *mPath;
    int mOldIndex;
    int mNewIndex;
};

class ChangePropertyValue : public QUndoCommand
{
public:
    ChangePropertyValue(MapDocument *mapDoc, Path *path, PathGeneratorProperty *prop,
                        const QString &newValue, bool mergeable = false) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Property Value")),
        mMapDocument(mapDoc),
        mPath(path),
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
        mValue = mMapDocument->changePathGeneratorPropertyValue(mPath, mProperty, mValue);
    }

    MapDocument *mMapDocument;
    Path *mPath;
    PathGeneratorProperty *mProperty;
    QString mValue;
    bool mMergeable;
};

class ChangePathClosed : public QUndoCommand
{
public:
    ChangePathClosed(MapDocument *mapDoc, Path *path, bool closed) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Path is Closed")),
        mMapDocument(mapDoc),
        mPath(path),
        mClosed(closed)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mClosed = mMapDocument->changePathIsClosed(mPath, mClosed);
    }

    MapDocument *mMapDocument;
    Path *mPath;
    bool mClosed;
};

class ChangePathCenters : public QUndoCommand
{
public:
    ChangePathCenters(MapDocument *mapDoc, Path *path, bool centers) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Change Path Centers")),
        mMapDocument(mapDoc),
        mPath(path),
        mCenters(centers)
    {
    }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap()
    {
        mCenters = mMapDocument->changePathCenters(mPath, mCenters);
    }

    MapDocument *mMapDocument;
    Path *mPath;
    bool mCenters;
};

} // namespace PathUndoRedo

using namespace PathUndoRedo;

PathPropertiesDialog::PathPropertiesDialog(QWidget *parent) :
    QDialog(parent),
    mMapDocument(0),
    mPath(0),
    mCurrentGenerator(0),
    mCurrentProperty(0),
    mCurrentGeneratorTemplate(0),
    mWasEditingString(false),
    ui(new Ui::PathPropertiesDialog)
{
    ui->setupUi(this);

    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));

    QToolButton *button = new QToolButton(this);
    button->setIcon(undoIcon);
    Utils::setThemeIcon(button, "edit-undo");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(tr("Undo"));
    button->setEnabled(false);
    button->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->undoRedoLayout->addWidget(button);

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(tr("Redo"));
    button->setEnabled(false);
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->undoRedoLayout->addWidget(button);

    connect(ui->generatorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorChanged(int)));
    connect(ui->propertyList, SIGNAL(currentRowChanged(int)),
            SLOT(currentPropertyChanged(int)));
    connect(ui->propertyList, SIGNAL(activated(QModelIndex)),
            SLOT(propertyActivated(QModelIndex)));
    connect(ui->allGeneratorsList, SIGNAL(currentRowChanged(int)),
            SLOT(currentGeneratorTemplateChanged(int)));
    connect(ui->allGeneratorsList, SIGNAL(activated(QModelIndex)),
            SLOT(templateActivated(QModelIndex)));

    connect(ui->checkBox, SIGNAL(toggled(bool)), SLOT(booleanToggled(bool)));
    connect(ui->integerSpinBox, SIGNAL(valueChanged(int)),
            SLOT(integerValueChanged(int)));
    connect(ui->chooseTile, SIGNAL(clicked()), SLOT(chooseTile()));
    connect(ui->clearTile, SIGNAL(clicked()), SLOT(clearTile()));
    connect(ui->stringEdit, SIGNAL(textEdited(QString)), SLOT(stringEdited(QString)));

    connect(ui->duplicate, SIGNAL(clicked()), SLOT(duplicate()));
    connect(ui->moveUp, SIGNAL(clicked()), SLOT(moveUp()));
    connect(ui->moveDown, SIGNAL(clicked()), SLOT(moveDown()));
    connect(ui->removeGenerator, SIGNAL(clicked()), SLOT(removeGenerator()));

    connect(ui->addGenerator, SIGNAL(clicked()), SLOT(addGenerator()));
    connect(ui->generatorsDialog, SIGNAL(clicked()), SLOT(generatorsDialog()));
    connect(ui->nameEdit, SIGNAL(textEdited(QString)), SLOT(nameEdited(QString)));

    connect(ui->closed, SIGNAL(toggled(bool)), SLOT(setClosed(bool)));
    connect(ui->centers, SIGNAL(toggled(bool)), SLOT(setCenters(bool)));

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

    connect(mMapDocument, SIGNAL(pathGeneratorAdded(Path*,int)),
            SLOT(pathGeneratorAdded(Path*,int)));
    connect(mMapDocument, SIGNAL(pathGeneratorRemoved(Path*,int)),
            SLOT(pathGeneratorRemoved(Path*,int)));
    connect(mMapDocument, SIGNAL(pathGeneratorReordered(Path*,int,int)),
            SLOT(pathGeneratorReordered(Path*,int,int)));
    connect(mMapDocument,
            SIGNAL(pathGeneratorPropertyChanged(Path*,PathGeneratorProperty*)),
            SLOT(pathGeneratorPropertyChanged(Path*,PathGeneratorProperty*)));

    mUndoIndex = mMapDocument->undoStack()->index();
    connect(mMapDocument->undoStack(), SIGNAL(indexChanged(int)),
            SLOT(synchUI()));

    connect(mUndoButton, SIGNAL(clicked()),
            mMapDocument->undoStack(), SLOT(undo()));
    connect(mRedoButton, SIGNAL(clicked()),
            mMapDocument->undoStack(), SLOT(redo()));

    mPath = path;
    setGeneratorsList();

    synchUI();

    PathGeneratorMgr::instance()->loadTilesets(); // FIXME: move this
}

void PathPropertiesDialog::currentGeneratorChanged(int row)
{
    mCurrentGenerator = 0;
    mCurrentProperty = 0;
    if (row >= 0) {
        mCurrentGenerator = mPath->generator(row);
        setPropertyList();
    }
    synchUI();
    mWasEditingString = false;
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
    mWasEditingString = false;
}

void PathPropertiesDialog::currentGeneratorTemplateChanged(int row)
{
    mCurrentGeneratorTemplate = 0;
    if (row >= 0) {
        mCurrentGeneratorTemplate = PathGeneratorMgr::instance()->generators().at(row);
    }
    synchUI();
}

void PathPropertiesDialog::propertyActivated(const QModelIndex &index)
{
    Q_UNUSED(index)
    if (!mCurrentProperty)
        return;
    if (PGP_Tile *prop = mCurrentProperty->asTile())
        chooseTile();
}

void PathPropertiesDialog::templateActivated(const QModelIndex &index)
{
    Q_UNUSED(index)
    addGenerator();
}

void PathPropertiesDialog::nameEdited(const QString &text)
{
    if (mSynching || !mCurrentGenerator)
        return;
    mCurrentGenerator->setLabel(text);
    int row = mPath->indexOf(mCurrentGenerator);
    ui->generatorsList->item(row)->setText(text);
}

void PathPropertiesDialog::booleanToggled(bool newValue)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (PGP_Boolean *p = mCurrentProperty->asBoolean()) {
        mMapDocument->undoStack()->push(new ChangePropertyValue(mMapDocument,
                                                                mPath,
                                                                mCurrentProperty,
                                                                QLatin1String(newValue ? "true" : "false")));
    }
}

void PathPropertiesDialog::integerValueChanged(int newValue)
{
    if (mSynching || !mMapDocument || !mCurrentProperty)
        return;
    if (PGP_Integer *p = mCurrentProperty->asInteger()) {
        mMapDocument->undoStack()->push(new ChangePropertyValue(mMapDocument,
                                                                mPath,
                                                                mCurrentProperty,
                                                                QString::number(newValue)));
    }
}

void PathPropertiesDialog::chooseTile()
{
    PGP_Tile *prop = mCurrentProperty->asTile();

    PathGeneratorTilesDialog *dialog = PathGeneratorTilesDialog::instance();

    QWidget *saveParent = dialog->parentWidget();
    dialog->reparent(this);

    dialog->setInitialTile(prop->mTilesetName, prop->mTileID);
    if (dialog->exec() == QDialog::Accepted) {
        if (Tile *tile = dialog->selectedTile()) {
            mMapDocument->undoStack()->beginMacro(tr("Change Generator Tile"));
            addTilesetIfNeeded(tile->tileset()->name());
            mMapDocument->undoStack()->push(
                        new ChangePropertyValue(mMapDocument, mPath, prop,
                                                prop->tileName(
                                                    tile->tileset()->name(),
                                                    tile->id())));
            mMapDocument->undoStack()->endMacro();
        }
    }
    dialog->reparent(saveParent);
}

void PathPropertiesDialog::clearTile()
{
    PGP_Tile *prop = mCurrentProperty->asTile();
    mMapDocument->undoStack()->push(new ChangePropertyValue(mMapDocument, mPath,
                                                            prop, QString()));
}

void PathPropertiesDialog::stringEdited(const QString &text)
{
    if (mSynching || !mCurrentProperty)
        return;
    if (mCurrentProperty->asString() || mCurrentProperty->asLayer()) {
        mMapDocument->undoStack()->push(new ChangePropertyValue(mMapDocument,
                                                                mPath,
                                                                mCurrentProperty,
                                                                text,
                                                                !mWasEditingString));
        mWasEditingString = true;
    }
}

void PathPropertiesDialog::duplicate()
{
    if (mCurrentGenerator) {
        int index = ui->generatorsList->currentRow() + 1;
        mMapDocument->undoStack()->push(new AddGenerator(mMapDocument, mPath, index,
                                                         mCurrentGenerator->clone()));
    }
}

void PathPropertiesDialog::removeGenerator()
{
    if (mCurrentGenerator) {
        int index = mPath->generators().indexOf(mCurrentGenerator);
        mMapDocument->undoStack()->push(new RemoveGenerator(mMapDocument,
                                                            mPath, index));
#if 0
        mPath->removeGenerator(index);
        Q_ASSERT(mCurrentGenerator->refCount() == 0);
        delete mCurrentGenerator;
        setGeneratorsList();
        index = qMin(index, ui->generatorsList->count() - 1);
        ui->generatorsList->setCurrentRow(index);
#endif
    }
}

void PathPropertiesDialog::moveUp()
{
    if (!mCurrentGenerator)
         return;
    int index = mPath->indexOf(mCurrentGenerator);
    if (index == 0)
        return;
    mMapDocument->undoStack()->push(new ReorderGenerator(mMapDocument, mPath,
                                                         index, index - 1));
}

void PathPropertiesDialog::moveDown()
{
    if (!mCurrentGenerator)
         return;
    int index = mPath->indexOf(mCurrentGenerator);
    if (index == mPath->generators().size() - 1)
        return;
    mMapDocument->undoStack()->push(new ReorderGenerator(mMapDocument, mPath,
                                                         index, index + 1));
}

void PathPropertiesDialog::addGenerator()
{
    mMapDocument->undoStack()->beginMacro(tr("Add Generator to Path"));

    // Add tilesets used by the generator to the map, if needed.
    foreach (PathGeneratorProperty *prop, mCurrentGeneratorTemplate->properties()) {
        if (PGP_Tile *tile = prop->asTile())
            addGeneratorTilesets(tile);
    }

    int index = ui->generatorsList->currentRow() + 1;
    if (index == 0)
        index = mPath->generators().size();
    mMapDocument->undoStack()->push(new AddGenerator(mMapDocument, mPath, index,
                                                     mCurrentGeneratorTemplate->clone()));

    mMapDocument->undoStack()->endMacro();
}

void PathPropertiesDialog::addGeneratorTilesets(PGP_Tile *prop)
{
    if (!prop->mTilesetName.isEmpty())
        addTilesetIfNeeded(prop->mTilesetName);
}

void PathPropertiesDialog::addTilesetIfNeeded(const QString &tilesetName)
{
    if (Tileset *ts = PathGeneratorMgr::instance()->tilesetFor(tilesetName)) {
        bool found = false;
        foreach (Tileset *mapTileset, mMapDocument->map()->tilesets()) {
            if (mapTileset->name() == tilesetName &&
                    mapTileset->imageSource() == ts->imageSource()) {
                found = true;
                break;
            }
        }
        if (!found) {
            qDebug() << "added generator tileset" << tilesetName;
            mMapDocument->undoStack()->push(new AddTileset(mMapDocument, ts));
        }
    }
}

void PathPropertiesDialog::setClosed(bool closed)
{
    if (mSynching || !mPath)
        return;

    mMapDocument->undoStack()->push(new ChangePathClosed(mMapDocument, mPath,
                                                         closed));
}

void PathPropertiesDialog::setCenters(bool centers)
{
    if (mSynching || !mPath)
        return;

    mMapDocument->undoStack()->push(new ChangePathCenters(mMapDocument, mPath,
                                                          centers));
}

void PathPropertiesDialog::synchUI()
{
    mSynching = true;

    ui->nameEdit->setText(mCurrentGenerator ? mCurrentGenerator->label() : QString());
    ui->nameEdit->setEnabled(mCurrentGenerator != 0);

    ui->duplicate->setEnabled(mCurrentGenerator != 0);
    ui->moveUp->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() > 0);
    ui->moveDown->setEnabled(mCurrentGenerator && ui->generatorsList->currentRow() < mPath->generators().size() - 1);
    ui->removeGenerator->setEnabled(mCurrentGenerator != 0);
    ui->addGenerator->setEnabled(mCurrentGeneratorTemplate);

    ui->propertyStack->setEnabled(mCurrentProperty);

    mUndoButton->setEnabled(mMapDocument &&
                            mMapDocument->undoStack()->index() > mUndoIndex);
    mRedoButton->setEnabled(mMapDocument &&
                            mMapDocument->undoStack()->index() <
                            mMapDocument->undoStack()->count());

    ui->closed->setChecked(mPath->isClosed());
    ui->centers->setChecked(mPath->centers());

    mSynching = false;
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
            ui->tileName->clear();
            ui->integerSpinBox->clear();
            ui->stringEdit->clear();
            ui->checkBox->setText(QLatin1String("CheckBox"));
        }
    } else if (PGP_Tile *p = mCurrentProperty->asTile()) {
        ui->propertyStack->setCurrentWidget(ui->Tile);
        QPixmap pixmap;
        if (Tileset *ts = findTileset(p->mTilesetName, mPath->pathLayer()->map()->tilesets())) {
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

void PathPropertiesDialog::pathGeneratorAdded(Path *path, int index)
{
    if (path != mPath)
        return;
    ui->generatorsList->insertItem(index, path->generator(index)->label());
    ui->generatorsList->setCurrentRow(index);
}

void PathPropertiesDialog::pathGeneratorRemoved(Path *path, int index)
{
    if (path != mPath)
        return;
    delete ui->generatorsList->takeItem(index);
    if (index == ui->generatorsList->currentRow())
        currentGeneratorChanged(index);
}

void PathPropertiesDialog::pathGeneratorReordered(Path *path, int oldIndex, int newIndex)
{
    if (path != mPath)
        return;
    ui->generatorsList->item(oldIndex)->setText(path->generator(oldIndex)->label());
    ui->generatorsList->item(newIndex)->setText(path->generator(newIndex)->label());
    ui->generatorsList->setCurrentRow(newIndex);
}

void PathPropertiesDialog::pathGeneratorPropertyChanged(Path *path,
                                                        PathGeneratorProperty *prop)
{
    if (path != mPath || prop != mCurrentProperty)
        return;
    setPropertyPage();
}
