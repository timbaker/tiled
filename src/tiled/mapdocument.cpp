/*
 * mapdocument.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Jeff Bland <jeff@teamphobic.com>
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

#include "mapdocument.h"

#include "addremovelayer.h"
#include "addremovemapobject.h"
#include "addremovetileset.h"
#include "changeproperties.h"
#include "changetileselection.h"
#include "imagelayer.h"
#include "isometricrenderer.h"
#include "layermodel.h"
#include "mapobjectmodel.h"
#ifdef ZOMBOID
#include "bmpblender.h"
#include "bmptool.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "preferences.h"
#include "zlevelrenderer.h"
#include "worlded/world.h"
#include "worlded/worldcell.h"
#include "worlded/worldedmgr.h"
#endif
#include "map.h"
#include "maplevel.h"
#include "mapobject.h"
#include "movelayer.h"
#include "objectgroup.h"
#include "offsetlayer.h"
#include "orthogonalrenderer.h"
#include "painttilelayer.h"
#include "resizelayer.h"
#include "resizemap.h"
#include "staggeredrenderer.h"
#include "tile.h"
#include "tilelayer.h"
#include "tilesetmanager.h"
#include "tileset.h"
#include "tmxmapwriter.h"

#include <QFileInfo>
#include <QRect>
#include <QUndoStack>
#ifdef ZOMBOID
#include <QDir>
#endif

using namespace Tiled;
using namespace Tiled::Internal;

MapDocument::MapDocument(Map *map, const QString &fileName):
    mFileName(fileName),
    mMap(map),
    mLayerModel(new LayerModel(this)),
    mMapObjectModel(new MapObjectModel(this)),
#ifdef ZOMBOID
    mMapComposite(nullptr),
    mWorldCell(nullptr),
#endif
    mUndoStack(new QUndoStack(this))
{
#ifdef ZOMBOID
    mMapComposite = new MapComposite(MapManager::instance()->newFromMap(map, fileName));
    connect(mMapComposite->bmpBlender(), &BmpBlender::regionAltered,
            this, &MapDocument::bmpBlenderRegionAltered);
    connect(this, &MapDocument::layerAdded,
            mMapComposite->bmpBlender(), &BmpBlender::updateWarnings);
    connect(this, &MapDocument::layerRenamed,
            mMapComposite->bmpBlender(), &BmpBlender::updateWarnings);
    connect(this, &MapDocument::layerRemoved,
            mMapComposite->bmpBlender(), &BmpBlender::updateWarnings);
    connect(MapManager::instance(), &MapManager::mapAboutToChange,
            this, &MapDocument::onMapAboutToChange);
    connect(MapManager::instance(), &MapManager::mapChanged,
            this, &MapDocument::onMapChanged);

    if (!mFileName.isEmpty() && Preferences::instance()->showAdjacentMaps()) {
        connect(MapManager::instance(), &MapManager::mapLoaded,
                this, &MapDocument::mapLoaded);
        connect(MapManager::instance(), &MapManager::mapFailedToLoad,
                this, &MapDocument::mapFailedToLoad);
        connect(WorldEd::WorldEdMgr::instance(), &WorldEd::WorldEdMgr::afterWorldChanged,
                this, &MapDocument::initAdjacentMaps);
        initAdjacentMaps();
    }
#endif
    switch (map->orientation()) {
    case Map::Isometric:
        mRenderer = new IsometricRenderer(map);
        break;
    case Map::Staggered:
        mRenderer = new StaggeredRenderer(map);
        break;
#ifdef ZOMBOID
    case Map::LevelIsometric:
        mRenderer = new ZLevelRenderer(map);
        break;
#endif
    default:
        mRenderer = new OrthogonalRenderer(map);
        break;
    }

#ifdef ZOMBOID
    mRenderer->setMaxLevel(mMapComposite->maxLevel());
#endif

    mCurrentLevelIndex = 0;
    mCurrentLayerIndex = (map->layerCount() == 0) ? -1 : 0;
    mLayerModel->setMapDocument(this);

    // Forward signals emitted from the layer model
    connect(mLayerModel, &LayerModel::layerAdded, this, &MapDocument::onLayerAdded);
    connect(mLayerModel, &LayerModel::layerAboutToBeRemoved,
            this, &MapDocument::onLayerAboutToBeRemoved);
    connect(mLayerModel, &LayerModel::layerRemoved, this, &MapDocument::onLayerRemoved);
    connect(mLayerModel, &LayerModel::layerChanged, this, &MapDocument::layerChanged);
#ifdef ZOMBOID
    connect(mLayerModel, &LayerModel::layerRenamed, this, &MapDocument::onLayerRenamed);
    mMaxVisibleLayer = map->layerCount();

    connect(mMapComposite, &MapComposite::layerGroupAdded,
            this, &MapDocument::layerGroupAdded);
    connect(mMapComposite, &MapComposite::layerAddedToGroup,
            this, &MapDocument::layerAddedToGroup);
    connect(mMapComposite, &MapComposite::layerAboutToBeRemovedFromGroup,
            this, &MapDocument::layerAboutToBeRemovedFromGroup);
    connect(mMapComposite, &MapComposite::layerRemovedFromGroup,
            this, &MapDocument::layerRemovedFromGroup);
    connect(mMapComposite, &MapComposite::layerLevelChanged,
            this, &MapDocument::layerLevelChanged);
#endif

    // Forward signals emitted from the map object model
    mMapObjectModel->setMapDocument(this);
    connect(mMapObjectModel, &MapObjectModel::objectsAdded,
            this, &MapDocument::objectsAdded);
    connect(mMapObjectModel, &MapObjectModel::objectsChanged,
            this, &MapDocument::objectsChanged);
    connect(mMapObjectModel, &MapObjectModel::objectsAboutToBeRemoved,
            this, &MapDocument::objectsAboutToBeRemoved);
    connect(mMapObjectModel, &MapObjectModel::objectsRemoved,
            this, &MapDocument::onObjectsRemoved);

    connect(mUndoStack, &QUndoStack::cleanChanged, this, &MapDocument::modifiedChanged);

    // Register tileset references
    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->addReferences(mMap->tilesets());

#ifdef ZOMBOID
    connect(tilesetManager, &TilesetManager::tileLayerNameChanged,
            this, &MapDocument::tileLayerNameChanged);

    mMapComposite->setShowLotFloorsOnly(Preferences::instance()->showLotFloorsOnly());
#endif
}

MapDocument::~MapDocument()
{
    // Unregister tileset references
    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->removeReferences(mMap->tilesets());

#ifdef ZOMBOID
    // Paranoia
    mMapObjectModel->setMapDocument(nullptr);
    delete mMapComposite;
#endif

    delete mRenderer;
    delete mMap;
}

bool MapDocument::save(QString *error)
{
    return save(fileName(), error);
}

bool MapDocument::save(const QString &fileName, QString *error)
{
    TmxMapWriter mapWriter;

    if (!mapWriter.write(map(), fileName)) {
        if (error)
            *error = mapWriter.errorString();
        return false;
    }

    undoStack()->setClean();
    setFileName(fileName);

    return true;
}

void MapDocument::setFileName(const QString &fileName)
{
    if (mFileName == fileName)
        return;

    mFileName = fileName;
    emit fileNameChanged();
}

/**
 * Returns the name with which to display this map. It is the file name without
 * its path, or 'untitled.tmx' when the map has no file name.
 */
QString MapDocument::displayName() const
{
    QString displayName = QFileInfo(mFileName).fileName();
    if (displayName.isEmpty())
        displayName = tr("untitled.tmx");

    return displayName;
}

/**
 * Returns whether the map has unsaved changes.
 */
bool MapDocument::isModified() const
{
    return !mUndoStack->isClean();
}

void MapDocument::setCurrentLayerIndex(int levelIndex, int layerIndex)
{
    MapLevel *level = mMap->levelAt(levelIndex);
    if (level == nullptr) {
        levelIndex = 0;
        layerIndex = -1;
    } else {
//        Q_ASSERT(layerIndex >= -1 && layerIndex < level->layerCount());
        if (layerIndex == -1) {
            // Selected an empty level in the Layers view.
        } else if ((layerIndex < 0) || (layerIndex >= level->layerCount())) {
            Q_ASSERT(false);
            layerIndex = -1;
        }
    }

    mCurrentLevelIndex = levelIndex;
    mCurrentLayerIndex = layerIndex;

    /* This function always sends the following signal, even if the index
     * didn't actually change. This is because the selected index in the layer
     * table view might be out of date anyway, and would otherwise not be
     * properly updated.
     *
     * This problem happens due to the selection model not sending signals
     * about changes to its current index when it is due to insertion/removal
     * of other items. The selected item doesn't change in that case, but our
     * layer index does.
     */
    emit currentLayerIndexChanged(mCurrentLevelIndex, mCurrentLayerIndex);
}

Layer *MapDocument::currentLayer() const
{
    MapLevel *mapLevel = currentMapLevel();
    if (mapLevel == nullptr)
        return nullptr;

    return mapLevel->layerAt(mCurrentLayerIndex);
}

MapLevel *MapDocument::currentMapLevel() const
{
    return map()->levelAt(mCurrentLevelIndex);
}

void MapDocument::setCurrentLevelAndLayer(int levelIndex, int layerIndex)
{
    setCurrentLayerIndex(levelIndex, layerIndex);
}

void MapDocument::resizeMap(const QSize &size, const QPoint &offset)
{
    const QRegion movedSelection = mTileSelection.translated(offset);
    const QRectF newArea = QRectF(-offset, size);

    // Resize the map and each layer
    mUndoStack->beginMacro(tr("Resize Map"));
#ifdef ZOMBOID
    mUndoStack->push(new ResizeMap(this, size, true));
#endif
    for (int z = 0; z < mMap->levelCount(); ++z) {
        MapLevel *mapLevel = mMap->levelAt(z);
        for (int i = 0; i < mapLevel->layerCount(); i++) {
            if (ObjectGroup *objectGroup = mapLevel->layerAt(i)->asObjectGroup()) {
                // Remove objects that will fall outside of the map
                for (MapObject *o : objectGroup->objects()) {
                    if (!(newArea.contains(o->position())
                          || newArea.intersects(o->bounds()))) {
                        mUndoStack->push(new RemoveMapObject(this, o));
                    }
                }
            }
            mUndoStack->push(new ResizeLayer(this, z, i, size, offset));
        }
    }
#ifdef ZOMBOID
    mUndoStack->push(new ResizeBmpImage(this, 0, size, offset));
    mUndoStack->push(new ResizeBmpImage(this, 1, size, offset));
    mUndoStack->push(new ResizeBmpRands(this, 0, size));
    mUndoStack->push(new ResizeBmpRands(this, 1, size));
    foreach (MapNoBlend *noBlend, mMap->noBlends())
        mUndoStack->push(new ResizeNoBlend(this, noBlend, size, offset));
    mUndoStack->push(new ResizeMap(this, size, false));
#else
    mUndoStack->push(new ResizeMap(this, size));
#endif
    mUndoStack->push(new ChangeTileSelection(this, movedSelection));
#ifdef ZOMBOID
#ifdef SEPARATE_BMP_SELECTION
    QRegion bmpSelection = mBmpSelection.translated(offset);
    mUndoStack->push(new ChangeBmpSelection(this, bmpSelection));
#endif
#endif
    mUndoStack->endMacro();

    // TODO: Handle layers that don't match the map size correctly
}

void MapDocument::offsetMap(const QList<int> &layerIndexes,
                            const QPoint &offset,
                            const QRect &bounds,
                            bool wrapX, bool wrapY)
{
    if (layerIndexes.empty())
        return;

    if (layerIndexes.size() == 2) {
        mUndoStack->push(new OffsetLayer(this, layerIndexes.first(), layerIndexes.last(), offset,
                                         bounds, wrapX, wrapY));
    } else {
        mUndoStack->beginMacro(tr("Offset Map"));
        for (int i = 0; i < layerIndexes.size(); i += 2) {
            int levelIndex = layerIndexes[i];
            int layerIndex = layerIndexes[i + 1];
            mUndoStack->push(new OffsetLayer(this, levelIndex, layerIndex, offset,
                                             bounds, wrapX, wrapY));
        }

#ifdef ZOMBOID
        // Offset the BMP images and MapNoBlends only if every rule+blend layer
        // is being offset.
        // Don't offset the MapRands.
        bool allBMPLayers = true;
        MapLevel *mapLevel = map()->levelAt(0);
        const QStringList layerNames = mapComposite()->bmpBlender()->tileLayerNames();
        for (const QString &layerName : layerNames) {
            int layerIndex = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
            if (layerIndex >= 0 && !layerIndexes.contains(layerIndex)) {
                allBMPLayers = false;
                break;
            }
        }
        if (allBMPLayers) {
            mUndoStack->push(new OffsetBmpImage(this, 0, offset, bounds, wrapX, wrapY));
            mUndoStack->push(new OffsetBmpImage(this, 1, offset, bounds, wrapX, wrapY));
            const QList<MapNoBlend*> noBlends = map()->noBlends();
            for (MapNoBlend *noBlend : noBlends) {
                mUndoStack->push(new OffsetNoBlend(this, noBlend, offset, bounds, wrapX, wrapY));
            }
        }
#endif

        mUndoStack->endMacro();
    }
}

/**
 * Adds a layer of the given type to the top of the layer stack. After adding
 * the new layer, emits editLayerNameRequested().
 */
void MapDocument::addLayer(Layer::Type layerType)
{
    Layer *layer = nullptr;
    QString name;

#if 1
    // Create the new layer in the same level as the current layer.
    // Stack it with other layers of the same type in level-order.
    int level = currentLevelIndex();
    MapLevel *mapLevel = mMap->levelAt(level);
    int index = mapLevel->layers(layerType).size();

    switch (layerType) {
    case Layer::TileLayerType:
        name = tr("Tile Layer %2").arg(mapLevel->tileLayerCount() + 1);
        layer = new TileLayer(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::ObjectGroupType:
        name = tr("Object Layer %2").arg(mapLevel->objectGroupCount() + 1);
        layer = new ObjectGroup(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::ImageLayerType:
        name = tr("Image Layer %2").arg(mapLevel->imageLayerCount() + 1);
        layer = new ImageLayer(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::AnyLayerType:
        break; // Q_ASSERT below will fail.
    }
    Q_ASSERT(layer);
#else
    switch (layerType) {
    case Layer::TileLayerType:
        name = tr("Tile Layer %1").arg(mMap->tileLayerCount() + 1);
        layer = new TileLayer(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::ObjectGroupType:
        name = tr("Object Layer %1").arg(mMap->objectGroupCount() + 1);
        layer = new ObjectGroup(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::ImageLayerType:
        name = tr("Image Layer %1").arg(mMap->imageLayerCount() + 1);
        layer = new ImageLayer(name, 0, 0, mMap->width(), mMap->height());
        break;
    case Layer::AnyLayerType:
        break; // Q_ASSERT below will fail.
    }
    Q_ASSERT(layer);

    const int index = mMap->layerCount();
#endif
    layer->setLevel(level);
    mUndoStack->push(new AddLayer(this, level, index, layer));
    setCurrentLayerIndex(level, index);

    emit editLayerNameRequested();
}

/**
 * Duplicates the currently selected layer.
 */
void MapDocument::duplicateLayer()
{
    if (mCurrentLayerIndex == -1)
        return;

    Layer *duplicate = mMap->levelAt(mCurrentLevelIndex)->layerAt(mCurrentLayerIndex)->clone();
#ifdef ZOMBOID
    // Duplicate the layer into the same level by preserving the N_ prefix.
    duplicate->setName(tr("%1 copy").arg(duplicate->name()));
#else
    duplicate->setName(tr("Copy of %1").arg(duplicate->name()));
#endif

    const int index = mCurrentLayerIndex + 1;
    QUndoCommand *cmd = new AddLayer(this, mCurrentLevelIndex, index, duplicate);
    cmd->setText(tr("Duplicate Layer"));
    mUndoStack->push(cmd);
    setCurrentLayerIndex(duplicate->level(), index);
}

/**
 * Merges the currently selected layer with the layer below. This only works
 * when the layers can be merged.
 *
 * \see Layer::canMergeWith
 */
void MapDocument::mergeLayerDown()
{
    MapLevel *mapLevel = currentMapLevel();
    if (mapLevel == nullptr)
        return;

    if (mCurrentLayerIndex < 1)
        return;

    Layer *upperLayer = mapLevel->layerAt(mCurrentLayerIndex);
    Layer *lowerLayer = mapLevel->layerAt(mCurrentLayerIndex - 1);

    if (!lowerLayer->canMergeWith(upperLayer))
        return;

    Layer *merged = lowerLayer->mergedWith(upperLayer);

    mUndoStack->beginMacro(tr("Merge Layer Down"));
    mUndoStack->push(new AddLayer(this, merged->level(), mCurrentLayerIndex - 1, merged));
    mUndoStack->push(new RemoveLayer(this, merged->level(), mCurrentLayerIndex));
    mUndoStack->push(new RemoveLayer(this, merged->level(), mCurrentLayerIndex));
    mUndoStack->endMacro();
}

/**
 * Moves the given layer up. Does nothing when no valid layer index is
 * given.
 */
void MapDocument::moveLayerUp(int levelIndex, int layerIndex)
{
    MapLevel *mapLevel = mMap->levelAt(levelIndex);
    if (mapLevel == nullptr)
        return;

    if ((layerIndex < 0) || (layerIndex >= mapLevel->layerCount()))
        return;

    if ((layerIndex == mapLevel->layerCount() - 1) && (levelIndex == mMap->levelCount() - 1))
        return;

    mUndoStack->push(new MoveLayer(this, levelIndex, layerIndex, MoveLayer::Up));
}

/**
 * Moves the given layer down. Does nothing when no valid layer index is
 * given.
 */
void MapDocument::moveLayerDown(int levelIndex, int layerIndex)
{
    MapLevel *mapLevel = mMap->levelAt(levelIndex);
    if (mapLevel == nullptr)
        return;

    if ((layerIndex < 0) || (layerIndex >= mapLevel->layerCount()))
        return;

    if ((layerIndex == 0) && (levelIndex == 0))
        return;

    mUndoStack->push(new MoveLayer(this, levelIndex, layerIndex, MoveLayer::Down));
}

/**
 * Removes the given layer.
 */
void MapDocument::removeLayer(int levelIndex, int layerIndex)
{
    MapLevel *mapLevel = mMap->levelAt(levelIndex);
    if (mapLevel == nullptr)
        return;

    if (layerIndex < 0 || layerIndex >= mMap->layerCount())
        return;

    mUndoStack->push(new RemoveLayer(this, levelIndex, layerIndex));
}

/**
  * Show or hide all other layers except the layer at the given index.
  * If any other layer is visible then all layers will be hidden, otherwise
  * the layers will be shown.
  */
void MapDocument::toggleOtherLayers(int levelIndex, int layerIndex)
{
    mLayerModel->toggleOtherLayers(levelIndex, layerIndex);
}

#ifdef ZOMBOID
void MapDocument::setLayerVisible(int levelIndex, int layerIndex, bool visible)
{
    MapLevel *mapLevel = mMap->levelAt(levelIndex);
    if (mapLevel == nullptr)
        return;
    if (mapLevel->layerAt(layerIndex) == nullptr)
        return;
    mLayerModel->setData(mLayerModel->toIndex(levelIndex, layerIndex),
                         visible ? Qt::Checked : Qt::Unchecked,
                         Qt::CheckStateRole);
}
#endif // ZOMBOID

/**
 * Adds a tileset to this map at the given \a index. Emits the appropriate
 * signal.
 */
void MapDocument::insertTileset(int index, Tileset *tileset)
{
    mMap->insertTileset(index, tileset);
    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->addReference(tileset);
#ifdef ZOMBOID
    mMapComposite->bmpBlender()->tilesetAdded(tileset);
#endif
    emit tilesetAdded(index, tileset);
}

/**
 * Removes the tileset at the given \a index from this map. Emits the
 * appropriate signal.
 *
 * \warning Does not make sure that any references to tiles in the removed
 *          tileset are cleared.
 */
void MapDocument::removeTilesetAt(int index)
{
    Tileset *tileset = mMap->tilesets().at(index);
    mMap->removeTilesetAt(index);
#ifdef ZOMBOID
    mMapComposite->bmpBlender()->tilesetRemoved(tileset->name());
#endif
    emit tilesetRemoved(tileset);
    TilesetManager *tilesetManager = TilesetManager::instance();
    tilesetManager->removeReference(tileset);
}

void MapDocument::moveTileset(int from, int to)
{
    if (from == to)
        return;

    Tileset *tileset = mMap->tilesets().at(from);
    mMap->removeTilesetAt(from);
    mMap->insertTileset(to, tileset);
    emit tilesetMoved(from, to);
}

void MapDocument::setTileSelection(const QRegion &selection)
{
    if (mTileSelection != selection) {
        const QRegion oldTileSelection = mTileSelection;
        mTileSelection = selection;
        emit tileSelectionChanged(mTileSelection, oldTileSelection);
    }
}

#ifdef ZOMBOID
const QRegion &MapDocument::bmpSelection() const
{
#ifdef SEPARATE_BMP_SELECTION
    return mBmpSelection;
#else
    return tileSelection();
#endif
}

void MapDocument::setBmpSelection(const QRegion &selection)
{
#ifdef SEPARATE_BMP_SELECTION
    if (mBmpSelection != selection) {
        const QRegion oldSelection = mBmpSelection;
        mBmpSelection = selection;
        emit bmpSelectionChanged(mBmpSelection, oldSelection);
    }
#else
    setTileSelection(selection);
#endif
}

void MapDocument::paintBmp(int bmpIndex, int px, int py, const QImage &source,
                           const QRegion &paintRgn)
{
    MapBmp &bmp = mMap->rbmp(bmpIndex);
    QRegion region = paintRgn & QRect(0, 0, bmp.width(), bmp.height());

    for (const QRect &r : region) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                bmp.setPixel(x, y, source.pixel(x - px, y - py));
            }
        }
    }

    mapComposite()->bmpBlender()->markDirty(region);

    emit bmpPainted(bmpIndex, region);
}

QImage MapDocument::swapBmpImage(int bmpIndex, const QImage &image)
{
    QImage old = mMap->bmp(bmpIndex).image();
    mMap->rbmp(bmpIndex).rimage() = image;
    return old;
}

MapRands MapDocument::swapBmpRands(int bmpIndex, const MapRands &rands)
{
    MapRands old = mMap->bmp(bmpIndex).rands();
    mMap->rbmp(bmpIndex).rrands() = rands;
    return old;
}

void MapDocument::setBmpAliases(const QList<BmpAlias *> &aliases)
{
    mMap->rbmpSettings()->setAliases(aliases);

    mapComposite()->bmpBlender()->fromMap();
    mapComposite()->bmpBlender()->recreate();

    emit bmpAliasesChanged();
}

void MapDocument::setBmpRules(const QString &fileName,
                                      const QList<BmpRule *> &rules)
{
    mMap->rbmpSettings()->setRulesFile(fileName);
    mMap->rbmpSettings()->setRules(rules);

    mapComposite()->bmpBlender()->fromMap();
    mapComposite()->bmpBlender()->recreate();

    emit bmpRulesChanged();
}

void MapDocument::setBmpBlends(const QString &fileName,
                               const QList<BmpBlend *> &blends)
{
    mMap->rbmpSettings()->setBlendsFile(fileName);
    mMap->rbmpSettings()->setBlends(blends);

    mapComposite()->bmpBlender()->fromMap();
    mapComposite()->bmpBlender()->recreate();

    emit bmpBlendsChanged();
}

MapNoBlend MapDocument::paintNoBlend(MapNoBlend *noBlend, const MapNoBlend &other, const QRegion &rgn)
{
    MapNoBlend old = noBlend->copy(rgn);
    noBlend->replace(&other, rgn);
    emit noBlendPainted(noBlend, rgn);
    return old;
}

void MapDocument::swapNoBlend(MapNoBlend *noBlend, MapNoBlend *other)
{
    MapNoBlend old(noBlend->layerName(), noBlend->width(), noBlend->height());
    old.replace(noBlend);
    noBlend->replace(other);
    other->replace(&old);
    // swapNoBlend() gets called when resizing a map
//    emit noBlendPainted(noBlend, QRect(0, 0, noBlend->width(), noBlend->height()));
}
#endif // ZOMBOID

void MapDocument::setSelectedObjects(const QList<MapObject *> &selectedObjects)
{
    mSelectedObjects = selectedObjects;
    emit selectedObjectsChanged();
}

/**
 * Makes sure the all tilesets which are used at the given \a map will be
 * present in the map document.
 *
 * To reach the aim, all similar tilesets will be replaced by the version
 * in the current map document and all missing tilesets will be added to
 * the current map document.
 */
void MapDocument::unifyTilesets(Map *map)
{
    QList<QUndoCommand*> undoCommands;
    QList<Tileset*> existingTilesets = mMap->tilesets();
    TilesetManager *tilesetManager = TilesetManager::instance();

    // Add tilesets that are not yet part of this map
    foreach (Tileset *tileset, map->tilesets()) {
        if (existingTilesets.contains(tileset))
            continue;

        Tileset *replacement = tileset->findSimilarTileset(existingTilesets);
        if (!replacement) {
            undoCommands.append(new AddTileset(this, tileset));
            continue;
        }

        // Merge the tile properties
        const int sharedTileCount = qMin(tileset->tileCount(),
                                         replacement->tileCount());
        for (int i = 0; i < sharedTileCount; ++i) {
            Tile *replacementTile = replacement->tileAt(i);
            Properties properties = replacementTile->properties();
            properties.merge(tileset->tileAt(i)->properties());
            undoCommands.append(new ChangeProperties(tr("Tile"),
                                                     replacementTile,
                                                     properties));
        }
        map->replaceTileset(tileset, replacement);

        tilesetManager->addReference(replacement);
        tilesetManager->removeReference(tileset);
    }
    if (!undoCommands.isEmpty()) {
        mUndoStack->beginMacro(tr("Tileset Changes"));
        foreach (QUndoCommand *command, undoCommands)
            mUndoStack->push(command);
        mUndoStack->endMacro();
    }
}

/**
 * Emits the map changed signal. This signal should be emitted after changing
 * the map size or its tile size.
 */
void MapDocument::emitMapChanged()
{
#ifdef ZOMBOID
    MapManager::instance()->mapParametersChanged(mMapComposite->mapInfo());
#endif
    emit mapChanged();
}

#ifdef ZOMBOID
void MapDocument::emitRegionChanged(const QRegion &region, Layer *layer)
{
    emit regionChanged(region, layer);
}
#else
void MapDocument::emitRegionChanged(const QRegion &region)
{
    emit regionChanged(region);
}
#endif

void MapDocument::emitRegionEdited(const QRegion &region, Layer *layer)
{
    emit regionEdited(region, layer);
}

#ifdef ZOMBOID
void MapDocument::emitRegionAltered(const QRegion &region, Layer *layer)
{
#if 1
    if (mMapComposite->bmpBlender()->tileLayerNames().contains(layer->name())) {
        mMapComposite->bmpBlender()->markDirty(region);
    }
#endif
    emit regionAltered(region, layer);
}

void MapDocument::setTileLayerName(Tile *tile, const QString &name)
{
    TilesetManager::instance()->setLayerName(tile, name);
}

#include "mainwindow.h"
#include "zprogress.h"

class SetBlendEdgesEverywhere : public QUndoCommand
{
public:
    SetBlendEdgesEverywhere(MapDocument *mapDocument, bool enabled)
        : mDocument(mapDocument)
        , mEnabled(enabled)
        , mTileSelection(mapDocument->tileSelection())
    {
        setText(QCoreApplication::translate("Undo Commands", "Toggle Blend Edges Everywhere"));
    }

    void swap(bool redo)
    {
        bool oldValue = mDocument->map()->bmpSettings()->isBlendEdgesEverywhere();
        mDocument->map()->rbmpSettings()->setBlendEdgesEverywhere(mEnabled);

        // Highlight changed parts of the map.
        PROGRESS progress(QLatin1String("BMP blending..."), Tiled::Internal::MainWindow::instance());
        QRegion tileSelection;
        mDocument->mapComposite()->bmpBlender()->testBlendEdgesEverywhere(mEnabled, tileSelection);

        mDocument->mapComposite()->bmpBlender()->setBlendEdgesEverywhere(mEnabled);
        mEnabled = oldValue;
        mDocument->setTileSelection(redo ? tileSelection : mTileSelection);
        emit mDocument->bmpBlendEdgesEverywhereChanged();
    }

    void undo()
    {
        swap(false);
    }

    void redo()
    {
        swap(true);
    }

    MapDocument *mDocument;
    bool mEnabled;
    QRegion mTileSelection;
};

void MapDocument::setBlendEdgesEverywhere(bool enabled)
{
    if (enabled == mMap->bmpSettings()->isBlendEdgesEverywhere())
        return;
    mUndoStack->push(new SetBlendEdgesEverywhere(this, enabled));
}

#endif // ZOMBOID

/**
 * Before forwarding the signal, the objects are removed from the list of
 * selected objects, triggering a selectedObjectsChanged signal when
 * appropriate.
 */
void MapDocument::onObjectsRemoved(ObjectGroup *objectGroup, const QList<MapObject*> &objects)
{
    deselectObjects(objects);
    emit objectsRemoved(objectGroup, objects);
}

void MapDocument::onLayerAdded(int z, int index)
{
    emit layerAdded(z, index);
#ifdef ZOMBOID
    mMapComposite->layerAdded(z, index);
#endif

    // Select the first layer that gets added to the map
    if (mMap->layerCount() == 1)
        setCurrentLayerIndex(z, 0);
}

void MapDocument::onLayerAboutToBeRemoved(int z, int index)
{
    MapLevel *mapLevel = mMap->levelAt(z);
    // Deselect any objects on this layer when necessary
    if (ObjectGroup *og = dynamic_cast<ObjectGroup*>(mapLevel->layerAt(index)))
        deselectObjects(og->objects());
#ifdef ZOMBOID
    mMapComposite->layerAboutToBeRemoved(z, index);
#endif
    emit layerAboutToBeRemoved(z, index);
}

void MapDocument::onLayerRemoved(int z, int index)
{
    MapLevel *mapLevel = mMap->levelAt(z);
    // Bring the current layer index to safety
    bool currentLayerRemoved = mCurrentLayerIndex == mapLevel->layerCount();
    if (currentLayerRemoved)
        mCurrentLayerIndex = mCurrentLayerIndex - 1;

    emit layerRemoved(z, index);

    // Emitted after the layerRemoved signal so that the MapScene has a chance
    // of synchronizing before adapting to the newly selected index
    if (currentLayerRemoved)
        emit currentLayerIndexChanged(mCurrentLevelIndex, mCurrentLayerIndex);
}

#ifdef ZOMBOID
void MapDocument::setLayerGroupVisibility(CompositeLayerGroup *layerGroup, bool visible)
{
    layerGroup->setVisible(visible);
    emit layerGroupVisibilityChanged(layerGroup);
}

void MapDocument::onLayerRenamed(int z, int index)
{
    mMapComposite->layerRenamed(z, index);

    emit layerRenamed(z, index);
}

void MapDocument::onMapAboutToChange(MapInfo *mapInfo)
{
    mMapComposite->mapAboutToChange(mapInfo);
}

void MapDocument::onMapChanged(MapInfo *mapInfo)
{
    bool changed = false;

    if (mMapComposite->mapChanged(mapInfo))
        changed = true;

    // If an adjacent map was just reloaded, all the WorldEd lots in it will
    // have been deleted.
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x == 0 && y == 0) continue;
            if (MapComposite *adjacentMap = mMapComposite->adjacentMap(x, y)) {
                if (adjacentMap->mapInfo() == mapInfo) {
                    int cx = mWorldCell->x(), cy = mWorldCell->y();
                    if (WorldCell *cell = mWorldCell->world()->cellAt(cx + x, cy + y)) {
                        foreach (WorldCellLot *lot, cell->lots()) {
                            MapInfo *subMapInfo = MapManager::instance()->loadMap(
                                        lot->mapName(), QString(), true, MapManager::PriorityLow);
                            if (subMapInfo) {
                                if (subMapInfo->isLoading())
                                    mAdjacentSubMapsLoading.insert(subMapInfo, LoadingSubMap(lot, subMapInfo));
                                else
                                    adjacentMap->addMap(subMapInfo, lot->pos(), lot->level());
                            }
                        }
                    }
                }
            }
        }
    }

    if (changed)
        emit mapCompositeChanged();
}

void MapDocument::bmpBlenderRegionAltered(const QRegion &region)
{
    MapLevel *mapLevel = mMap->levelAt(0);
    const QStringList layerNames = mapComposite()->bmpBlender()->tileLayerNames();
    for (const QString &layerName : layerNames) {
        int index = mapLevel->indexOfLayer(layerName, Layer::TileLayerType);
        if (index == -1)
            continue;
        TileLayer *tl = mapLevel->layerAt(index)->asTileLayer();
        mapComposite()->tileLayersForLevel(0)->regionAltered(tl);
        emit regionAltered(region, tl); // infinite loop with emitRegionAltered()
        break; // this should redraw the whole layergroup anyway
    }
}

void MapDocument::mapLoaded(MapInfo *info)
{
    if (!mAdjacentMapsLoading.contains(info) &&
            !mAdjacentSubMapsLoading.contains(info)) return;

    if (mMapsLoaded.isEmpty())
        QMetaObject::invokeMethod(this, "handleMapsLoadedNow", Qt::QueuedConnection);
    mMapsLoaded += info;
}

void MapDocument::mapFailedToLoad(MapInfo *info)
{
    mAdjacentMapsLoading.remove(info);
    mAdjacentSubMapsLoading.remove(info);
}

void MapDocument::handleMapsLoadedNow()
{
    bool changed = false;
    // It could happen that the WorldEd project file was changed while
    // adjacent maps were being loaded, causes mWorldCell to be set to null.
    WorldCell *cell = mWorldCell;
    if (!cell) {
        mMapsLoaded.clear();
        mAdjacentMapsLoading.clear();
        mAdjacentSubMapsLoading.clear();
        return;
    }

    while (!mMapsLoaded.isEmpty()) {
        MapInfo *info = mMapsLoaded.takeFirst();

        foreach (const AdjacentMap &am, mAdjacentMapsLoading.values(info)) {
            mMapComposite->setAdjacentMap(am.pos.x(), am.pos.y(), am.info);

            MapComposite *adjacentMap = mMapComposite->adjacentMap(am.pos.x(),
                                                                   am.pos.y());
            WorldCell *cell2 = cell->world()->cellAt(am.pos + cell->pos());
            foreach (WorldCellLot *lot, cell2->lots()) {
                MapInfo *subMapInfo = MapManager::instance()->loadMap(
                            lot->mapName(), QString(), true, MapManager::PriorityLow);
                if (subMapInfo && !subMapInfo->isLoading() && !mAdjacentSubMapsLoading.contains(subMapInfo))
                    adjacentMap->addMap(subMapInfo, lot->pos(), lot->level());
            }

            changed = true;
        }
        mAdjacentMapsLoading.remove(info);

        foreach (const LoadingSubMap &sm, mAdjacentSubMapsLoading.values(info)) {
            int x = sm.lot->cell()->x(), y = sm.lot->cell()->y();
            if (MapComposite *adjacentMap = mMapComposite->adjacentMap(x - cell->x(),
                                                                       y - cell->y()))
                adjacentMap->addMap(info, sm.lot->pos(), sm.lot->level());
            changed = true;
        }
        mAdjacentSubMapsLoading.remove(info);
    }

    // This lets ZomboidScene update itself (syncing and repainting).
    if (changed)
        emit mapCompositeChanged(); ///////

    if (!mMapsLoaded.isEmpty())
        QMetaObject::invokeMethod(this, "handleMapsLoadedNow", Qt::QueuedConnection);
}

void MapDocument::beforeWorldChanged(const QString &fileName)
{
    Q_UNUSED(fileName);
    mWorldCell = 0;
}

void MapDocument::afterWorldChanged(const QString &fileName)
{
    Q_UNUSED(fileName);
    mWorldCell = WorldEd::WorldEdMgr::instance()->cellForMap(mFileName);
}

#endif // ZOMBOID

void MapDocument::deselectObjects(const QList<MapObject *> &objects)
{
    int removedCount = 0;
    foreach (MapObject *object, objects)
        removedCount += mSelectedObjects.removeAll(object);

    if (removedCount > 0)
        emit selectedObjectsChanged();
}

#ifdef ZOMBOID
void MapDocument::initAdjacentMaps()
{
    QVector<MapInfo*> adjacentMaps(9);

    if (WorldCell *cell = WorldEd::WorldEdMgr::instance()->cellForMap(mFileName)) {
        mWorldCell = cell;
        int cx = cell->x(), cy = cell->y();
        for (int y = -1; y <= 1; y++) {
            if (cy + y < 0 || cy + y >= cell->world()->height()) continue;
            for (int x = -1; x <= 1; x++) {
                if (cx + x < 0 || cx + x >= cell->world()->width()) continue;
                if (x == 0 && y == 0) continue;
                if (WorldCell *cell2 = cell->world()->cellAt(cx + x, cy + y)) {
                    if (cell2->mapFilePath().isEmpty()) continue;
                    QFileInfo info(cell2->mapFilePath());
                    if (info.exists()) {
                        MapInfo *mapInfo = MapManager::instance()->loadMap(
                                    info.absoluteFilePath(), QString(), true,
                                    MapManager::PriorityMedium);
                        if (mapInfo) {
                            if (mapInfo->isLoading())
                                mAdjacentMapsLoading.insert(mapInfo, AdjacentMap(x, y, mapInfo));
                            else
                                mMapComposite->setAdjacentMap(x, y, mapInfo);

                            MapComposite *adjacentMap = mMapComposite->adjacentMap(x, y);
                            foreach (WorldCellLot *lot, cell2->lots()) {
                                MapInfo *subMapInfo = MapManager::instance()->loadMap(
                                            lot->mapName(), QString(), true, MapManager::PriorityLow);
                                if (subMapInfo) {
                                    if (subMapInfo->isLoading())
                                        mAdjacentSubMapsLoading.insert(subMapInfo, LoadingSubMap(lot, subMapInfo));
                                    else if (adjacentMap)
                                        adjacentMap->addMap(subMapInfo, lot->pos(), lot->level());
                                }
                            }
                            adjacentMaps[(x + 1) + (y + 1) * 3] = mapInfo;
                        }
                    }
                }
            }
        }
    }

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            if (x == 0 && y == 0) continue;
            if (MapComposite *mc = mMapComposite->adjacentMap(x, y)) {
                int index = (x + 1) + (y + 1) * 3;
                if (mc->mapInfo() != adjacentMaps[index])
                    mMapComposite->setAdjacentMap(x, y, 0);
            }
        }
    }
}
#endif // ZOMBOID

void MapDocument::setTilesetFileName(Tileset *tileset,
                                     const QString &fileName)
{
    tileset->setFileName(fileName);
    emit tilesetFileNameChanged(tileset);
}

void MapDocument::setTilesetName(Tileset *tileset, const QString &name)
{
    tileset->setName(name);
    emit tilesetNameChanged(tileset);
}
