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

#ifndef BUILDINGLAYERSDOCK_H
#define BUILDINGLAYERSDOCK_H

#include <QDockWidget>

class QListWidgetItem;

namespace Ui {
class BuildingLayersDock;
}

namespace BuildingEditor {

class BuildingDocument;
class BuildingFloor;

class BuildingLayersDock : public QDockWidget
{
    Q_OBJECT
    
public:
    explicit BuildingLayersDock(QWidget *parent = 0);
    ~BuildingLayersDock();

private:
    void setLayersList();

private slots:
    void currentDocumentChanged(BuildingDocument *doc);
    void currentLayerChanged(int row);

    void visibilityChanged(int value);
    void opacityChanged(int value);
    void layerItemChanged(QListWidgetItem *item);

    void currentFloorChanged();
    void currentLayerChanged();
    void layerVisibilityChanged(BuildingFloor *floor, const QString &layerName);

    void updateActions();

private:
    Ui::BuildingLayersDock *ui;
    BuildingDocument *mDocument;
    bool mSynching;
};

} // namespace BuildingEditor

#endif // BUILDINGLAYERSDOCK_H
