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

#ifndef CHOOSEBUILDINGTILEDIALOG_H
#define CHOOSEBUILDINGTILEDIALOG_H

#include <QDialog>

namespace Ui {
class ChooseBuildingTileDialog;
}

namespace Tiled {
class Tile;
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingTileCategory;
class BuildingTileEntry;

class ChooseBuildingTileDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ChooseBuildingTileDialog(const QString &prompt,
                                      BuildingTileCategory *category,
                                      BuildingTileEntry *initialTile,
                                      QWidget *parent = 0);
    ~ChooseBuildingTileDialog();

    BuildingTileEntry *selectedTile() const;

private:
    void setTilesList(BuildingTileCategory *category,
                      BuildingTileEntry *initialTile = 0);

    void saveSettings();

private slots:
    void tilesDialog();
    void accept();
    void reject();
    
private:
    Ui::ChooseBuildingTileDialog *ui;
    BuildingTileCategory *mCategory;
    QList<Tiled::Tile*> mTiles;
    QList<BuildingTileEntry*> mBuildingTiles;
    Tiled::Internal::Zoomable *mZoomable;
};

} // namespace BuildingEditor

#endif // CHOOSEBUILDINGTILEDIALOG_H
