/*
 * mapdocumentactionhandler.cpp
 * Copyright 2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2011, Stefan Beller <stefanbeller@googlemail.com
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

#include "mapdocumentactionhandler.h"

#include "changetileselection.h"
#include "documentmanager.h"
#include "layer.h"
#include "map.h"
#include "mapdocument.h"
#include "maplevel.h"
#include "maprenderer.h"
#include "utils.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QtCore/qmath.h>

using namespace Tiled;
using namespace Tiled::Internal;

MapDocumentActionHandler *MapDocumentActionHandler::mInstance;

MapDocumentActionHandler::MapDocumentActionHandler(QObject *parent)
    : QObject(parent)
    , mMapDocument(0)
{
    Q_ASSERT(!mInstance);
    mInstance = this;

    mActionSelectAll = new QAction(this);
    mActionSelectAll->setShortcuts(QKeySequence::SelectAll);
    mActionSelectNone = new QAction(this);
    mActionSelectNone->setShortcut(tr("Ctrl+Shift+A"));

    mActionCropToSelection = new QAction(this);

    mActionAddTileLayer = new QAction(this);
    mActionAddObjectGroup = new QAction(this);
    mActionAddImageLayer = new QAction(this);

    mActionDuplicateLayer = new QAction(this);
    mActionDuplicateLayer->setShortcut(tr("Ctrl+Shift+D"));
    mActionDuplicateLayer->setIcon(
            QIcon(QLatin1String(":/images/16x16/stock-duplicate-16.png")));

    mActionMergeLayerDown = new QAction(this);

    mActionRemoveLayer = new QAction(this);
    mActionRemoveLayer->setIcon(
            QIcon(QLatin1String(":/images/16x16/edit-delete.png")));

    mActionRenameLayer = new QAction(this);
    mActionRenameLayer->setShortcut(tr("F2"));

    mActionSelectPreviousLevel = new QAction(this);
    mActionSelectPreviousLevel->setShortcut(tr("PgUp"));

    mActionSelectNextLevel = new QAction(this);
    mActionSelectNextLevel->setShortcut(tr("PgDown"));

    mActionMoveLayerUp = new QAction(this);
    mActionMoveLayerUp->setShortcut(tr("Ctrl+Shift+Up"));
    mActionMoveLayerUp->setIcon(
            QIcon(QLatin1String(":/images/16x16/go-up.png")));

    mActionMoveLayerDown = new QAction(this);
    mActionMoveLayerDown->setShortcut(tr("Ctrl+Shift+Down"));
    mActionMoveLayerDown->setIcon(
            QIcon(QLatin1String(":/images/16x16/go-down.png")));

    mActionToggleOtherLayers = new QAction(this);
    mActionToggleOtherLayers->setShortcut(tr("Ctrl+Shift+H"));
    mActionToggleOtherLayers->setIcon(
            QIcon(QLatin1String(":/images/16x16/show_hide_others.png")));

    mActionLayerProperties = new QAction(this);
    mActionLayerProperties->setIcon(
            QIcon(QLatin1String(":images/16x16/document-properties.png")));

    Utils::setThemeIcon(mActionRemoveLayer, "edit-delete");
    Utils::setThemeIcon(mActionMoveLayerUp, "go-up");
    Utils::setThemeIcon(mActionMoveLayerDown, "go-down");
    Utils::setThemeIcon(mActionLayerProperties, "document-properties");

    connect(mActionSelectAll, &QAction::triggered, this, &MapDocumentActionHandler::selectAll);
    connect(mActionSelectNone, &QAction::triggered, this, &MapDocumentActionHandler::selectNone);
    connect(mActionCropToSelection, &QAction::triggered,
            this, &MapDocumentActionHandler::cropToSelection);
    connect(mActionAddTileLayer, &QAction::triggered, this, &MapDocumentActionHandler::addTileLayer);
    connect(mActionAddObjectGroup, &QAction::triggered,
            this, &MapDocumentActionHandler::addObjectGroup);
    connect(mActionAddImageLayer, &QAction::triggered, this, &MapDocumentActionHandler::addImageLayer);
    connect(mActionDuplicateLayer, &QAction::triggered,
            this, &MapDocumentActionHandler::duplicateLayer);
    connect(mActionMergeLayerDown, &QAction::triggered,
            this, &MapDocumentActionHandler::mergeLayerDown);
    connect(mActionSelectPreviousLevel, &QAction::triggered,
            this, &MapDocumentActionHandler::selectPreviousLevel);
    connect(mActionSelectNextLevel, &QAction::triggered,
            this, &MapDocumentActionHandler::selectNextLevel);
    connect(mActionRenameLayer, &QAction::triggered, this, &MapDocumentActionHandler::renameLayer);
    connect(mActionRemoveLayer, &QAction::triggered, this, &MapDocumentActionHandler::removeLayer);
    connect(mActionMoveLayerUp, &QAction::triggered, this, &MapDocumentActionHandler::moveLayerUp);
    connect(mActionMoveLayerDown, &QAction::triggered, this, &MapDocumentActionHandler::moveLayerDown);
    connect(mActionToggleOtherLayers, &QAction::triggered,
            this, &MapDocumentActionHandler::toggleOtherLayers);

    updateActions();
    retranslateUi();
}

MapDocumentActionHandler::~MapDocumentActionHandler()
{
    mInstance = 0;
}

void MapDocumentActionHandler::retranslateUi()
{
    mActionSelectAll->setText(tr("Select &All"));
    mActionSelectNone->setText(tr("Select &None"));
    mActionCropToSelection->setText(tr("&Crop to Selection"));
    mActionAddTileLayer->setText(tr("Add &Tile Layer"));
    mActionAddObjectGroup->setText(tr("Add &Object Layer"));
    mActionAddImageLayer->setText(tr("Add &Image Layer"));
    mActionDuplicateLayer->setText(tr("&Duplicate Layer"));
    mActionMergeLayerDown->setText(tr("&Merge Layer Down"));
    mActionRemoveLayer->setText(tr("&Remove Layer"));
    mActionRenameLayer->setText(tr("Re&name Layer"));
#ifdef ZOMBOID
    mActionSelectPreviousLevel->setText(tr("Select Level Above"));
    mActionSelectNextLevel->setText(tr("Select Level Below"));
#else
    mActionSelectPreviousLayer->setText(tr("Select Pre&vious Layer"));
    mActionSelectNextLayer->setText(tr("Select &Next Layer"));
#endif
    mActionMoveLayerUp->setText(tr("R&aise Layer"));
    mActionMoveLayerDown->setText(tr("&Lower Layer"));
    mActionToggleOtherLayers->setText(tr("Show/&Hide all Other Layers"));
    mActionLayerProperties->setText(tr("Layer &Properties..."));
}

void MapDocumentActionHandler::setMapDocument(MapDocument *mapDocument)
{
    if (mMapDocument == mapDocument)
        return;

    if (mMapDocument)
        mMapDocument->disconnect(this);

    mMapDocument = mapDocument;
    updateActions();

    if (mMapDocument) {
        connect(mapDocument, &MapDocument::currentLayerIndexChanged,
                this, &MapDocumentActionHandler::updateActions);
        connect(mapDocument, &MapDocument::tileSelectionChanged,
                this, &MapDocumentActionHandler::updateActions);
    }

    emit mapDocumentChanged(mMapDocument);
}

void MapDocumentActionHandler::selectAll()
{
    if (!mMapDocument)
        return;

    Map *map = mMapDocument->map();
    QRect all(0, 0, map->width(), map->height());
    if (mMapDocument->tileSelection() == all)
        return;

    QUndoCommand *command = new ChangeTileSelection(mMapDocument, all);
    mMapDocument->undoStack()->push(command);
}

void MapDocumentActionHandler::selectNone()
{
    if (!mMapDocument)
        return;

    if (mMapDocument->tileSelection().isEmpty())
        return;

    QUndoCommand *command = new ChangeTileSelection(mMapDocument, QRegion());
    mMapDocument->undoStack()->push(command);
}

void MapDocumentActionHandler::copyPosition()
{
    const MapView *view = DocumentManager::instance()->currentMapView();
    if (!view)
        return;

    const QPoint globalPos = QCursor::pos();
    const QPoint viewportPos = view->viewport()->mapFromGlobal(globalPos);
    const QPointF scenePos = view->mapToScene(viewportPos);

    const MapRenderer *renderer = mapDocument()->renderer();
    const QPointF tilePos = renderer->pixelToTileCoords(scenePos);
    const int x = qFloor(tilePos.x());
    const int y = qFloor(tilePos.y());

    QApplication::clipboard()->setText(QString::number(x) +
                                       QLatin1String(", ") +
                                       QString::number(y));
}

void MapDocumentActionHandler::cropToSelection()
{
    if (!mMapDocument)
        return;

    const QRect bounds = mMapDocument->tileSelection().boundingRect();
    if (bounds.isNull())
        return;

    mMapDocument->resizeMap(bounds.size(), -bounds.topLeft());
}

void MapDocumentActionHandler::addTileLayer()
{
    if (mMapDocument)
        mMapDocument->addLayer(Layer::TileLayerType);
}

void MapDocumentActionHandler::addObjectGroup()
{
    if (mMapDocument)
        mMapDocument->addLayer(Layer::ObjectGroupType);
}

void MapDocumentActionHandler::addImageLayer()
{
     if (mMapDocument)
         mMapDocument->addLayer(Layer::ImageLayerType);
}

void MapDocumentActionHandler::duplicateLayer()
{
    if (mMapDocument)
        mMapDocument->duplicateLayer();
}

void MapDocumentActionHandler::mergeLayerDown()
{
    if (mMapDocument)
        mMapDocument->mergeLayerDown();
}

#ifndef ZOMBOID
void MapDocumentActionHandler::selectPreviousLayer()
{
    if (mMapDocument) {
        const int currentLayer = mMapDocument->currentLayerIndex();
        if (currentLayer < mMapDocument->map()->layerCount() - 1)
            mMapDocument->setCurrentLayerIndex(currentLayer + 1);
    }
}

void MapDocumentActionHandler::selectNextLayer()
{
    if (mMapDocument) {
        const int currentLayer = mMapDocument->currentLayerIndex();
        if (currentLayer > 0)
            mMapDocument->setCurrentLayerIndex(currentLayer - 1);
    }
}
#else
#include "mapcomposite.h"
#include "objectgroup.h"
static void switchToLevel(MapDocument *mMapDocument, int level)
{
    MapLevel *mapLevel = mMapDocument->map()->levelAt(level);
    if (mapLevel == nullptr)
        return;

    if (mapLevel->layerCount() == 0) {
        mMapDocument->setCurrentLayerIndex(level, -1);
        return;
    }

    // Try to switch to a layer with the same name in the new level
    if (Layer *layer = mMapDocument->currentLayer()) {
        for (Layer *layer2 : mapLevel->layers()) {
            if (layer2->name() != layer->name())
                continue;
            int index = mapLevel->layers().indexOf(layer2);
            mMapDocument->setCurrentLevelAndLayer(level, index);
            return;
        }
#if 0
        if (CompositeLayerGroup *layerGroup = mMapDocument->mapComposite()->tileLayersForLevel(level)) {
            // Try to switch to a layer with the same name in the new level
            QString name = MapComposite::layerNameWithoutPrefix(layer);
            if (layer->isTileLayer()) {
                for (TileLayer *tl : layerGroup->layers()) {
                    QString name2 = MapComposite::layerNameWithoutPrefix(tl);
                    if (name == name2) {
                        int index = mMapDocument->map()->levelAt(level)->layers().indexOf(tl);
                        mMapDocument->setCurrentLevelAndLayer(level, index);
                        return;
                    }
                }
            } else if (layer->isObjectGroup()) {
                for (ObjectGroup *og : mMapDocument->map()->objectGroups()) {
                    if (og->level() == level) {
                        QString name2 = MapComposite::layerNameWithoutPrefix(og);
                        if (name == name2) {
                            int index = mMapDocument->map()->levelAt(level)->layers().indexOf(og);
                            mMapDocument->setCurrentLevelAndLayer(level, index);
                            return;
                        }
                    }
                }
            }
        }
#endif
    }

    mMapDocument->setCurrentLayerIndex(level, 0);
}

void MapDocumentActionHandler::selectPreviousLevel()
{
    if (mMapDocument)
        switchToLevel(mMapDocument, mMapDocument->currentLevelIndex() + 1);
}

void MapDocumentActionHandler::selectNextLevel()
{
    if (mMapDocument)
        switchToLevel(mMapDocument, mMapDocument->currentLevelIndex() - 1);
}
#endif // ZOMBOID

void MapDocumentActionHandler::moveLayerUp()
{
    if (mMapDocument)
        mMapDocument->moveLayerUp(mMapDocument->currentLevelIndex(), mMapDocument->currentLayerIndex());
}

void MapDocumentActionHandler::moveLayerDown()
{
    if (mMapDocument)
        mMapDocument->moveLayerDown(mMapDocument->currentLevelIndex(), mMapDocument->currentLayerIndex());
}

void MapDocumentActionHandler::removeLayer()
{
    if (mMapDocument)
        mMapDocument->removeLayer(mMapDocument->currentLevelIndex(), mMapDocument->currentLayerIndex());
}

void MapDocumentActionHandler::renameLayer()
{
    if (mMapDocument)
        mMapDocument->emitEditLayerNameRequested();
}

void MapDocumentActionHandler::toggleOtherLayers()
{
    if (mMapDocument)
        mMapDocument->toggleOtherLayers(mMapDocument->currentLevelIndex(), mMapDocument->currentLayerIndex());
}

void MapDocumentActionHandler::updateActions()
{
    Map *map = nullptr;
    MapLevel *mapLevel = nullptr;
    int currentLevelIndex = -1;
    int currentLayerIndex = -1;
    QRegion selection;
    bool canMergeDown = false;

    if (mMapDocument) {
        map = mMapDocument->map();
        mapLevel = mMapDocument->currentMapLevel();
        currentLevelIndex = mMapDocument->currentLevelIndex();
        currentLayerIndex = mMapDocument->currentLayerIndex();
        selection = mMapDocument->tileSelection();

        if (currentLayerIndex > 0) {
            Layer *upper = mapLevel->layerAt(currentLayerIndex);
            Layer *lower = mapLevel->layerAt(currentLayerIndex - 1);
            canMergeDown = lower->canMergeWith(upper);
        }
    }

    mActionSelectAll->setEnabled(map);
    mActionSelectNone->setEnabled(!selection.isEmpty());

    mActionCropToSelection->setEnabled(!selection.isEmpty());

    mActionAddTileLayer->setEnabled(map);
    mActionAddObjectGroup->setEnabled(map);
    mActionAddImageLayer->setEnabled(map);

    const int levelCount = map ? map->levelCount() : 0;
    const bool hasPreviousLevel = (currentLevelIndex >= 0) && (currentLevelIndex < levelCount - 1);
    const bool hasNextLevel = currentLevelIndex > 0;

    const int layerCount = mapLevel ? mapLevel->layerCount() : 0;
    const bool hasPreviousLayer = (currentLayerIndex >= 0) && (currentLayerIndex < layerCount - 1);
    const bool hasNextLayer = currentLayerIndex > 0;

    mActionDuplicateLayer->setEnabled(currentLayerIndex >= 0);
    mActionMergeLayerDown->setEnabled(canMergeDown);
    mActionSelectPreviousLevel->setEnabled(hasPreviousLevel);
    mActionSelectNextLevel->setEnabled(hasNextLevel);
    mActionMoveLayerUp->setEnabled(hasPreviousLayer || hasPreviousLevel);
    mActionMoveLayerDown->setEnabled(hasNextLayer || hasNextLevel);
    mActionToggleOtherLayers->setEnabled(layerCount > 1);
    mActionRemoveLayer->setEnabled(currentLayerIndex >= 0);
    mActionRenameLayer->setEnabled(currentLayerIndex >= 0);
    mActionLayerProperties->setEnabled(currentLayerIndex >= 0);
}
