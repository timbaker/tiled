/*
 * mapscene.h
 * Copyright 2008-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2008, Roderic Morris <roderic@ccs.neu.edu>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
 * Copyright 2010, Jeff Bland <jksb@member.fsf.org>
 *
 * This file is part of Tiled.
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

#ifndef MAPSCENE_H
#define MAPSCENE_H

#include <QGraphicsScene>
#include <QMap>
#include <QSet>

#include <array>

namespace Tiled {

class Layer;
class MapObject;
class ObjectGroup;
class Tileset;

namespace Internal {

class AbstractTool;
class MapDocument;
class MapObjectItem;
class MapScene;
class ObjectGroupItem;
#ifdef ZOMBOID
#ifdef SEPARATE_BMP_SELECTION
class BmpSelectionItem;
#endif
class TileSelectionItem;
class ZGridItem;
#endif

/**
 * A graphics scene that represents the contents of a map.
 */
class MapScene : public QGraphicsScene
{
    Q_OBJECT

public:
    /**
     * Constructor.
     */
    MapScene(QObject *parent);

    /**
     * Destructor.
     */
    ~MapScene();

    /**
     * Returns the map document this scene is displaying.
     */
    MapDocument *mapDocument() const { return mMapDocument; }

    /**
     * Sets the map this scene displays.
     */
#ifdef ZOMBOID
    virtual void setMapDocument(MapDocument *map);
#else
    void setMapDocument(MapDocument *map);
#endif

    /**
     * Returns whether the tile grid is visible.
     */
    bool isGridVisible() const { return mGridVisible; }

    /**
     * Returns the set of selected map object items.
     */
    const QSet<MapObjectItem*> &selectedObjectItems() const
    { return mSelectedObjectItems; }

    /**
     * Sets the set of selected map object items. This translates to a call to
     * MapDocument::setSelectedObjects.
     */
    void setSelectedObjectItems(const QSet<MapObjectItem*> &items);

    /**
     * Returns the MapObjectItem associated with the given \a mapObject.
     */
    MapObjectItem *itemForObject(MapObject *object) const;

    /**
     * Enables the selected tool at this map scene.
     * Therefore it tells that tool, that this is the active map scene.
     */
    void enableSelectedTool();
    void disableSelectedTool();

    /**
     * Sets the currently selected tool.
     */
    void setSelectedTool(AbstractTool *tool);

#ifdef ZOMBOID
#ifdef SEPARATE_BMP_SELECTION
    BmpSelectionItem *bmpSelectionItem() const
    { return mBmpSelectionItem; }
#else
    TileSelectionItem *bmpSelectionItem() const
    { return mTileSelectionItem; }
#endif
#endif // ZOMBOID

signals:
    void selectedObjectItemsChanged();

public slots:
    /**
     * Sets whether the tile grid is visible.
     */
    void setGridVisible(bool visible);

    /**
     * Sets whether the current layer should be highlighted.
     */
    void setHighlightCurrentLayer(bool highlightCurrentLayer);

protected:
    /**
     * QGraphicsScene::drawForeground override that draws the tile grid.
     */
    void drawForeground(QPainter *painter, const QRectF &rect);

    /**
     * Override for handling enter and leave events.
     */
    bool event(QEvent *event);

    void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
    void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);

    void dragEnterEvent(QGraphicsSceneDragDropEvent *event);

#ifdef ZOMBOID
protected slots:
#else
private slots:
#endif
    /**
     * Refreshes the map scene.
     */
#ifdef ZOMBOID
    virtual void refreshScene();
#else
    void refreshScene();
#endif

    /**
     * Repaints the specified region. The region is in tile coordinates.
     */
#ifdef ZOMBOID
    virtual void regionChanged(const QRegion &region, Layer *layer);
#else
    void repaintRegion(const QRegion &region);
#endif

    void currentLayerIndexChanged();

#ifdef ZOMBOID
    virtual void mapChanged();
#else
    void mapChanged();
#endif
    void tilesetChanged(Tileset *tileset);

#ifdef ZOMBOID
    virtual void layerAdded(int z, int index);
    virtual void layerAboutToBeRemoved(int z, int index);
    virtual void layerRemoved(int z, int index);
    virtual void layerChanged(int z, int index);
    virtual void layerRenamed(int z, int index);
#else
    void layerAdded(int index);
    void layerRemoved(int index);
    void layerChanged(int index);
#endif

    void objectsAdded(const QList<MapObject*> &objects);
    void objectsRemoved(ObjectGroup *objectGroup, const QList<MapObject*> &objects);
    void objectsChanged(const QList<MapObject*> &objects);

    void updateSelectedObjectItems();
    void syncAllObjectItems();

#ifdef ZOMBOID
    void bgColorChanged(const QColor &color);

protected:
    virtual QGraphicsItem *createLayerItem(Layer *layer);

    virtual void updateCurrentLayerHighlight();
#else
private:
    QGraphicsItem *createLayerItem(Layer *layer);

    void updateCurrentLayerHighlight();
#endif

    bool eventFilter(QObject *object, QEvent *event);

    typedef QMap<MapObject*, MapObjectItem*> ObjectItems;

    class LevelData
    {
    public:
        LevelData()
        {

        }
        LevelData(const LevelData& other) = delete;
        LevelData& operator=(const LevelData& other) = delete;

        QVector<QGraphicsItem*> mLayerItems;
        ObjectItems mObjectItems;
    };

    MapDocument *mMapDocument;
    AbstractTool *mSelectedTool;
    AbstractTool *mActiveTool;
    bool mGridVisible;
    bool mHighlightCurrentLayer;
    bool mUnderMouse;
    Qt::KeyboardModifiers mCurrentModifiers;
    QPointF mLastMousePos;
    std::array<LevelData, 32> mLevelData;
    QGraphicsRectItem *mDarkRectangle;
#ifdef ZOMBOID
    ZGridItem *mGridItem;
    TileSelectionItem *mTileSelectionItem;
#ifdef SEPARATE_BMP_SELECTION
    BmpSelectionItem *mBmpSelectionItem;
#endif
#endif
    QSet<MapObjectItem*> mSelectedObjectItems;
};

} // namespace Internal
} // namespace Tiled

#endif // MAPSCENE_H
