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

#ifndef BUILDINGTEMPLATESDIALOG_H
#define BUILDINGTEMPLATESDIALOG_H

#include <QDialog>

class QAction;
class QUndoGroup;
class QUndoStack;

namespace Ui {
class BuildingTemplatesDialog;
}

namespace BuildingEditor {

class BuildingTemplate;
class BuildingTemplates;
class BuildingTileEntry;

class BuildingTemplatesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingTemplatesDialog(QWidget *parent = nullptr);
    ~BuildingTemplatesDialog();

    const QList<BuildingTemplate*> &templates() const
    { return mTemplates; }

    QUndoGroup *undoGroup() const
    { return mUndoGroup; }

    QUndoStack *undoStack() const
    { return mUndoStack; }

    QAction *undoAction() const
    { return mUndoAction; }

    QAction *redoAction() const
    { return mRedoAction; }

private slots:
    void templateSelectionChanged();
    void tileSelectionChanged();
    void addTemplate();
    void removeTemplate();
    void duplicateTemplate();
    void moveUp();
    void moveDown();
    void importTemplates();
    void exportTemplates();
    void nameEdited(const QString &name);
    void editRooms();
    void clearTile();
    void randomTile();
    void chooseTile();
    void synchUI();
    void accept() override;
    void reject() override;

private:
    void saveSettings();
    void readSettings();

private:
    void setTilePixmap();
    BuildingTileEntry *selectedTile();

    BuildingTemplates *mgr() const;

private:
    Ui::BuildingTemplatesDialog *ui;
    QList<BuildingTemplate*> mTemplates;
    BuildingTemplate *mTemplate;
    int mTileRow;
    QUndoGroup *mUndoGroup;
    QUndoStack *mUndoStack;
    QAction *mUndoAction;
    QAction *mRedoAction;
};

} // namespace BuildingEditor

#endif // BUILDINGTEMPLATESDIALOG_H
