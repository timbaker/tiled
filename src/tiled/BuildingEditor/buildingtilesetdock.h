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

#ifndef BUILDINGTILESETDOCK_H
#define BUILDINGTILESETDOCK_H

#include <QDockWidget>

namespace Ui {
class BuildingTilesetDock;
}

namespace Tiled {
class Tile;
class Tileset;

namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingTilesetDock : public QDockWidget
{
    Q_OBJECT
    
public:
    explicit BuildingTilesetDock(QWidget *parent = 0);
    ~BuildingTilesetDock();
    
    void firstTimeSetup();

private:
    void setTilesetList();
    void setTilesList();

    void switchLayerForTile(Tiled::Tile *tile);

    typedef Tiled::Tileset Tileset;

private slots:
    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tileset *tileset);

    void autoSwitchLayerChanged(bool autoSwitch);

    void tileScaleChanged(qreal scale);

private:
    Ui::BuildingTilesetDock *ui;
    Tiled::Tileset *mCurrentTileset;
    Tiled::Internal::Zoomable *mZoomable;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESETDOCK_H
