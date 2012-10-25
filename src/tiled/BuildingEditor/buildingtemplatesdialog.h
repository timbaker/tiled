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

namespace Ui {
class BuildingTemplatesDialog;
}

namespace BuildingEditor {

class BuildingTemplate;
class BuildingTile;

class BuildingTemplatesDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit BuildingTemplatesDialog(QWidget *parent = 0);
    ~BuildingTemplatesDialog();

    const QList<BuildingTemplate*> &templates() const
    { return mTemplates; }
    
private slots:
    void templateSelectionChanged();
    void tileSelectionChanged();
    void addTemplate();
    void removeTemplate();
    void duplicateTemplate();
    void moveUp();
    void moveDown();
    void nameEdited(const QString &name);
    void editRooms();
    void chooseTile();
    void synchUI();

private:
    void setTilePixmap();
    BuildingTile *selectedTile();

private:
    Ui::BuildingTemplatesDialog *ui;
    QList<BuildingTemplate*> mTemplates;
    BuildingTemplate *mTemplate;
    int mTileRow;
};

} // namespace BuildingEditor

#endif // BUILDINGTEMPLATESDIALOG_H
