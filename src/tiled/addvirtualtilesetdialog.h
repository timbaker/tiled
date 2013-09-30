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

#ifndef ADDVIRTUALTILESETDIALOG_H
#define ADDVIRTUALTILESETDIALOG_H

#include <QDialog>

namespace Ui {
class AddVirtualTilesetDialog;
}

class AddVirtualTilesetDialog : public QDialog
{
    Q_OBJECT

public:
    AddVirtualTilesetDialog(QWidget *parent = 0);
    AddVirtualTilesetDialog(const QString &name, int columnCount, int rowCount,
                            QWidget *parent = 0);
    ~AddVirtualTilesetDialog();

    QString name() const;
    int columnCount() const;
    int rowCount() const;

private slots:
    void updateActions();

private:
    void init(const QString &name, int columnCount, int rowCount);

private:
    Ui::AddVirtualTilesetDialog *ui;
};

#endif // ADDVIRTUALTILESETDIALOG_H
