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

#ifndef PATHGENERATORTILESDIALOG_H
#define PATHGENERATORTILESDIALOG_H

#include <QDialog>
#include <QModelIndex>

namespace Ui {
class PathGeneratorTilesDialog;
}

namespace Tiled {

class Tile;
class Tileset;

namespace Internal {

class Zoomable;

class PathGeneratorTilesDialog : public QDialog
{
    Q_OBJECT
    
public:
    static PathGeneratorTilesDialog *instance();
    static void deleteInstance();

    void reparent(QWidget *parent);
    void setInitialTile(const QString &tilesetName, int tileID);

    Tile *selectedTile();

private:
    void setTilesetList();
    void setTilesList();

private slots:
    void addTileset();
    void removeTileset();

    void currentTilesetChanged(int row);
    void currentTileChanged(const QModelIndex &index);
    void tileActivated(const QModelIndex &index);

    void updateUI();

    void accept();

private:
    static PathGeneratorTilesDialog *mInstance;
    explicit PathGeneratorTilesDialog(QWidget *parent = 0);
    ~PathGeneratorTilesDialog();

    Ui::PathGeneratorTilesDialog *ui;
    Zoomable *mZoomable;
    Tileset *mCurrentTileset;
    Tile *mCurrentTile;
};

} // namespace Internal;
} // namespace Tiled

#endif // PATHGENERATORTILESDIALOG_H
