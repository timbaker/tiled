/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#ifndef ADDTILESETSDIALOG_H
#define ADDTILESETSDIALOG_H

#include <QDialog>

namespace Ui {
class AddTilesetsDialog;
}

class AddTilesetsDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit AddTilesetsDialog(const QString &dir, const QStringList &ignore,
                               bool ignoreIsPaths = false,
                               QWidget *parent = nullptr);
    ~AddTilesetsDialog();

    void setAllowBrowse(bool browse);
    void setPrompt(const QString &prompt);
    
    QStringList fileNames();

public slots:
    int exec() override;

private:
    void setFilesList();

private slots:
    void browse();

    void checkAll();
    void uncheckAll();

    void accept();

private:
    Ui::AddTilesetsDialog *ui;
    QString mDirectory;
    QStringList mIgnore;
    bool mIgnoreIsPaths;
    bool mAllowBrowse;
};

#endif // ADDTILESETSDIALOG_H
