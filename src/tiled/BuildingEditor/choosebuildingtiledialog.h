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

class BuildingTile;

class ChooseBuildingTileDialog : public QDialog
{
    Q_OBJECT
    
public:
    explicit ChooseBuildingTileDialog(const QString &prompt,
                                      const QString &categoryName,
                                      BuildingTile *initialTile,
                                      QWidget *parent = 0);
    ~ChooseBuildingTileDialog();

    BuildingTile *selectedTile() const;

private:
    void setTilesList(const QString &categoryName, BuildingTile *initialTile = 0);

private slots:
    void tilesDialog();
    void accept();
    
private:
    Ui::ChooseBuildingTileDialog *ui;
    QString mCategoryName;
    QList<Tiled::Tile*> mTiles;
    QList<BuildingTile*> mBuildingTiles;
    Tiled::Internal::Zoomable *mZoomable;
};

} // namespace BuildingEditor

#endif // CHOOSEBUILDINGTILEDIALOG_H
