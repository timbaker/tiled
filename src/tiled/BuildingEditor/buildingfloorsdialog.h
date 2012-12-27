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

#ifndef BUILDINGFLOORSDIALOG_H
#define BUILDINGFLOORSDIALOG_H

#include <QDialog>

class QToolButton;

namespace Ui {
class BuildingFloorsDialog;
}

namespace BuildingEditor {

class BuildingDocument;
class BuildingFloor;

class BuildingFloorsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingFloorsDialog(BuildingDocument *doc, QWidget *parent = 0);
    ~BuildingFloorsDialog();
    
private slots:
    void add();
    void remove();

    void duplicate();

    void moveUp();
    void moveDown();

    void floorAdded(BuildingFloor *floor);
    void floorRemoved(BuildingFloor *floor);

    void currentFloorChanged(int row);

    void undoTextChanged(const QString &text);
    void redoTextChanged(const QString &text);

    void updateUI();

private:
    void setFloorsList();
    int toRow(BuildingFloor *floor);
    BuildingFloor *floorAt(int row);

private:
    Ui::BuildingFloorsDialog *ui;
    BuildingDocument *mDocument;
    BuildingFloor *mCurrentFloor;

    QToolButton *mUndoButton;
    QToolButton *mRedoButton;
    int mUndoIndex;
};

} // BuildingEditor

#endif // BUILDINGFLOORSDIALOG_H
