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

#include "mixedtilesetview.h"

#include <QDockWidget>
#include <QIcon>

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

class BuildingDocument;

class BuildingTilesetView : public Tiled::Internal::MixedTilesetView
{
public:
    BuildingTilesetView(QWidget *parent = 0);
    void contextMenuEvent(QContextMenuEvent *event);
};

class BuildingTilesetDock : public QDockWidget
{
    Q_OBJECT
    
public:
    explicit BuildingTilesetDock(QWidget *parent = 0);
    ~BuildingTilesetDock();
    
    void firstTimeSetup();

    void setDocument(BuildingDocument *document);
    void clearDocument();

private:
    void changeEvent(QEvent *event);
    void retranslateUi();

    void setTilesetList();
    void setTilesList();

    void switchLayerForTile(Tiled::Tile *tile);

    typedef Tiled::Tileset Tileset;
    typedef Tiled::Tile Tile;

private slots:
    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tileset *tileset);
    void tileLayerNameChanged(Tile *tile);

    void layerSwitchToggled(bool checked);
    void autoSwitchLayerChanged(bool enabled);

    void tileScaleChanged(qreal scale);

private:
    Ui::BuildingTilesetDock *ui;
    BuildingDocument *mDocument;
    Tiled::Tileset *mCurrentTileset;
    Tiled::Internal::Zoomable *mZoomable;
    QIcon mIconTileLayer;
    QIcon mIconTileLayerStop;
    QAction *mActionSwitchLayer;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESETDOCK_H
