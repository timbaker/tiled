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

#ifndef PATHGENERATORSDIALOG_H
#define PATHGENERATORSDIALOG_H

#include <QDialog>
#include <QModelIndex>

class QToolButton;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class PathGeneratorsDialog;
}

namespace Tiled {

class PathGenerator;
class PathGeneratorProperty;

namespace Internal {

class PathGeneratorsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit PathGeneratorsDialog(QWidget *parent);
    ~PathGeneratorsDialog();

private slots:
    void currentGeneratorChanged(int row);
    void currentPropertyChanged(int row);
    void currentGeneratorTemplateChanged(int row);

    void removeGenerator();
    void duplicate();
    void moveUp();
    void moveDown();

    void propertyActivated(const QModelIndex &index);

    void nameEdited(const QString &text);
    void booleanToggled(bool newValue);
    void integerValueChanged(int newValue);
    void chooseTile();
    void clearTile();
    void stringEdited(const QString &text);

    void addGenerator();

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void synchUI();

    void accept();

public:
    // +UNDO/REDO
    void addGenerator(int index, PathGenerator *pgen);
    PathGenerator *removeGenerator(int index);
    void reorderGenerator(int oldIndex, int newIndex);
    QString changePropertyValue(PathGeneratorProperty *prop, const QString &newValue);
    // -UNDO/REDO

private:
    void setGeneratorsList();
    void setPropertyList();
    void setPropertyPage();

private:
    Ui::PathGeneratorsDialog *ui;
    QList<PathGenerator*> mGenerators;
    PathGenerator *mCurrentGenerator;
    PathGeneratorProperty *mCurrentProperty;
    PathGenerator *mCurrentGeneratorTemplate;
    QString mPropertyName;
    bool mSynching;

    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
    bool mWasEditingString;
};

} // namespace Internal;
} // namespace Tiled

#endif // PATHGENERATORSDIALOG_H
