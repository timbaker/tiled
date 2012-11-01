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

#ifndef BUILDINGPREFERENCESDIALOG_H
#define BUILDINGPREFERENCESDIALOG_H

#include <QDialog>
#include <QSettings>

namespace Ui {
class BuildingPreferencesDialog;
}

namespace BuildingEditor {

class BuildingPreferencesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingPreferencesDialog(QWidget *parent = 0);
    ~BuildingPreferencesDialog();

public slots:
    void browseTiles();
    void accept();
    
private:
    Ui::BuildingPreferencesDialog *ui;
    QSettings mSettings;
};

} // BuildingEditor

#endif // BUILDINGPREFERENCESDIALOG_H
