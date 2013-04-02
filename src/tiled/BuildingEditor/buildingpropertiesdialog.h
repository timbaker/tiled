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

#ifndef BUILDINGDIALOG_H
#define BUILDINGDIALOG_H

#include <QDialog>

class QAbstractButton;

namespace Ui {
class BuildingPropertiesDialog;
}

namespace BuildingEditor {
class Building;
class BuildingDocument;
class BuildingTileEntry;

class BuildingPropertiesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingPropertiesDialog(BuildingDocument *doc, QWidget *parent = 0);
    ~BuildingPropertiesDialog();

    const QVector<BuildingTileEntry*> &tiles() const
    { return mTiles; }
    
private slots:
    void tileSelectionChanged();
    void chooseTile();
    void bbclicked(QAbstractButton *button);

private:
    void synchUI();
    void setTilePixmap();
    BuildingTileEntry *selectedTile();
    void accept();
    void apply();

private:
    Ui::BuildingPropertiesDialog *ui;

    BuildingDocument *mDocument;
    int mTileRow;
    QVector<BuildingTileEntry*> mTiles;
};

} // namespace BuildingEditor

#endif // BUILDINGDIALOG_H
