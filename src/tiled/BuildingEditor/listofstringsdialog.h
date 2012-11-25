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

#ifndef LISTOFSTRINGSDIALOG_H
#define LISTOFSTRINGSDIALOG_H

#include <QDialog>

namespace Ui {
class ListOfStringsDialog;
}

namespace BuildingEditor {

class ListOfStringsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ListOfStringsDialog(const QString &message, const QStringList &strings,
                                 QWidget *parent = 0);
    ~ListOfStringsDialog();
    
private:
    Ui::ListOfStringsDialog *ui;
};

} // namespace BuildingEditor

#endif // LISTOFSTRINGSDIALOG_H
