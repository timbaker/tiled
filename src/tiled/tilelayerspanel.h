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

#ifndef TILELAYERSPANEL_H
#define TILELAYERSPANEL_H

#include <QModelIndex>
#include <QWidget>

namespace Tiled {
class Layer;
class Tile;

namespace Internal {

class MapDocument;
class MixedTilesetView;
class Zoomable;

class TileLayersPanel : public QWidget
{
    Q_OBJECT
public:
    TileLayersPanel(QWidget *parent = 0);

    void setDocument(MapDocument *doc);

    void setScale(qreal scale);
    qreal scale() const;

signals:
    void tilePicked(Tile *tile);

public slots:
    void setTilePosition(const QPoint &tilePos);

private slots:
    void setList();
    void activated(const QModelIndex &index);
    void currentChanged();
    void layerIndexChanged(int index);
    void regionAltered(const QRegion &region, Layer *layer);
    void showTileLayersPanelChanged(bool show);

private:
    MapDocument *mDocument;
    MixedTilesetView *mView;
    int mCurrentLayerIndex;
    QPoint mTilePos;
};

}
}

#endif // TILELAYERSPANEL_H
