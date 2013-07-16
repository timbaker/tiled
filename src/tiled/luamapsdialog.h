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

#ifndef LUAMAPSDIALOG_H
#define LUAMAPSDIALOG_H

#include <QDialog>

namespace Ui {
class LuaMapsDialog;
}

class LuaMapsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LuaMapsDialog(QWidget *parent = 0);
    ~LuaMapsDialog();

private:
    void accept();
    bool processMap(const QString &mapFilePath);

private slots:
    void setList();
    void dirBrowse();
    void selectAll();
    void selectNone();

    void backupsBrowse();

    void scriptBrowse();

private:
    Ui::LuaMapsDialog *ui;
    struct ScopedPointerCustomDeleter;
};

#endif // LUAMAPSDIALOG_H
