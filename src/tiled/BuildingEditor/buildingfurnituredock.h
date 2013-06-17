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

#ifndef BUILDINGFURNITUREDOCK_H
#define BUILDINGFURNITUREDOCK_H

#include <QDockWidget>

class QListWidget;
class QSettings;
class QSplitter;

namespace BuildingEditor {

class FurnitureGroup;
class FurnitureTile;
class FurnitureView;

class BuildingFurnitureDock : public QDockWidget
{
    Q_OBJECT

public:
    BuildingFurnitureDock(QWidget *parent = 0);

    void readSettings(QSettings &settings);
    void writeSettings(QSettings &settings);

    void switchTo();

protected:
    void changeEvent(QEvent *event);

private:
    void retranslateUi();

    void setGroupsList();
    void setFurnitureList();

private slots:
    void currentGroupChanged(int row);
    void currentFurnitureChanged();
    void tileScaleChanged(qreal scale);
    void tilesDialogEdited();

private:
    QListWidget *mGroupList;
    QSplitter *mSplitter;
    FurnitureView *mFurnitureView;
    FurnitureGroup *mCurrentGroup;
    FurnitureTile *mCurrentTile;
};

} // namespace BuildingEditor

#endif // BUILDINGFURNITUREDOCK_H
