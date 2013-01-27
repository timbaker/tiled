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

#ifndef CHOOSETEMPLATESDIALOG_H
#define CHOOSETEMPLATESDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>

namespace Ui {
class ChooseTemplatesDialog;
}

namespace BuildingEditor {

class BuildingTemplate;

class ChooseTemplatesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ChooseTemplatesDialog(const QList<BuildingTemplate*> &templates,
                                   const QString &prompt,
                                   QWidget *parent = 0);
    ~ChooseTemplatesDialog();

    void setButtons(QDialogButtonBox::StandardButtons buttons);

    QList<BuildingTemplate*> chosenTemplates() const;
    
private slots:
    void checkAll();
    void uncheckAll();

private:
    void setList();

private:
    Ui::ChooseTemplatesDialog *ui;

    QList<BuildingTemplate*> mTemplates;
};

} // namespace BuildingEditor

#endif // CHOOSETEMPLATESDIALOG_H
