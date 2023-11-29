/*
 * Copyright 2023, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGATTRIBUTESDOCK_H
#define BUILDINGATTRIBUTESDOCK_H

#include <QDockWidget>

class QListWidgetItem;

namespace Ui {
class BuildingAttributesDock;
}

namespace BuildingEditor {

class BuildingDocument;

class BuildingAttributesDock : public QDockWidget
{
    Q_OBJECT
public:
    BuildingAttributesDock(QWidget *parent = nullptr);
    ~BuildingAttributesDock();

private slots:
    void currentDocumentChanged(BuildingEditor::BuildingDocument *doc);
    void tileSelectionChanged(const QRegion& old);
    void itemChanged(QListWidgetItem *item);
    void updateActions();

private:
    void clearAttributeOnSelectedSquares(const QString& attrName);
    void setAttributeOnSelectedSquares(const QString& attrName);
    void syncListWithSelectedSquares();

private:
    Ui::BuildingAttributesDock *ui;
    BuildingDocument *mDocument;
    bool mSynching;
};

} // namespace BuildingEditor

#endif // BUILDINGATTRIBUTESDOCK_H
