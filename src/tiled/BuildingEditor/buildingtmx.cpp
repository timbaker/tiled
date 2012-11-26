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

#include "buildingtmx.h"

#include "building.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "listofstringsdialog.h"
#include "simplefile.h"

#include "mapcomposite.h"

#include "tilesetmanager.h"
#include "tmxmapwriter.h"

#include "map.h"
#include "mapobject.h"
#include "objectgroup.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QDebug>
#include <QDir>
#include <QImage>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

static const char *TXT_FILE = "TMXConfig.txt";

BuildingTMX *BuildingTMX::mInstance = 0;

BuildingTMX *BuildingTMX::instance()
{
    if (!mInstance)
        mInstance = new BuildingTMX;
    return mInstance;
}

void BuildingTMX::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

BuildingTMX::BuildingTMX()
{
}

bool BuildingTMX::exportTMX(Building *building, const MapComposite *mapComposite,
                            const QString &fileName)
{
    Map *clone = mapComposite->map()->clone();

    foreach (BuildingFloor *floor, building->floors()) {

        // The given map has layers required by the editor, i.e., Floors, Walls,
        // Doors, etc.  The TMXConfig.txt file may specify extra layer names.
        // So we need to insert any extra layers in the order specified in
        // TMXConfig.txt.  If the layer name has a N_ prefix, it is only added
        // to level N, otherwise it is added to every level.  Object layers are
        // added above *all* the tile layers in the map.
        int previousExistingLayer = -1;
        foreach (LayerInfo layerInfo, mLayers) {
            QString layerName = layerInfo.mName;
            int level;
            if (MapComposite::levelForLayer(layerName, &level)) {
                if (level != floor->level())
                    continue;
            } else {
                layerName = tr("%1_%2").arg(floor->level()).arg(layerName);
            }
            int n;
            if ((n = clone->indexOfLayer(layerName)) >= 0) {
                previousExistingLayer = n;
                continue;
            }
            if (layerInfo.mType == LayerInfo::Tile) {
                TileLayer *tl = new TileLayer(layerName, 0, 0,
                                              clone->width(), clone->height());
                if (previousExistingLayer < 0)
                    previousExistingLayer = 0;
                clone->insertLayer(previousExistingLayer + 1, tl);
                previousExistingLayer++;
            } else {
                ObjectGroup *og = new ObjectGroup(layerName,
                                                  0, 0, clone->width(), clone->height());
                clone->addLayer(og);
            }
        }

        ObjectGroup *objectGroup = new ObjectGroup(tr("%1_RoomDefs").arg(floor->level()),
                                                   0, 0, clone->width(), clone->height());

        // Add the RoomDefs layer above all the tile layers
        clone->addLayer(objectGroup);

        int delta = (mapComposite->maxLevel() - floor->level()) * 3;
        QPoint offset(delta, delta); // FIXME: not for LevelIsometric
        foreach (Room *room, building->rooms()) {
            QRegion roomRegion = floor->roomRegion(room);
            foreach (QRect rect, roomRegion.rects()) {
                MapObject *mapObject = new MapObject(room->internalName, tr("room"),
                                                     rect.topLeft() + offset,
                                                     rect.size());
                objectGroup->addObject(mapObject);
            }
        }
    }

    TmxMapWriter writer;
    if (!writer.write(clone, fileName)) {
        mError = writer.errorString();
        delete clone;
        return false;
    }
    delete clone;
    return true;
}

QString BuildingTMX::txtName()
{
    return QLatin1String(TXT_FILE);
}

QString BuildingTMX::txtPath()
{
    return BuildingPreferences::instance()->configPath(txtName());
}

#define VERSION0 0
#define VERSION_LATEST VERSION0

bool BuildingTMX::readTxt()
{
    // Make sure the user has chosen the Tiles directory.
    QString tilesDirectory = BuildingPreferences::instance()->tilesDirectory();
    QDir dir(tilesDirectory);
    if (!dir.exists()) {
        mError = tr("The Tiles directory specified in the preferences doesn't exist!\n%1")
                .arg(tilesDirectory);
        return false;
    }

    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    if (!upgradeTxt())
        return false;

    if (!mergeTxt())
        return false;

    QString path = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected %1 version %2, got %3")
                .arg(txtName()).arg(VERSION_LATEST).arg(simple.version());
        return false;
    }

    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    QStringList missingTilesets;

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tileset")) {
                    QString source = tilesDirectory + QLatin1Char('/') + kv.value
                            + QLatin1String(".png");
                    QFileInfo info(source);
                    if (!info.exists()) {
                        Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
                        BuildingTilesMgr::instance()->addTileset(ts);
                        mTilesets += ts->name();
                        missingTilesets += QDir::toNativeSeparators(info.absoluteFilePath());
                        continue;
                    }
                    source = info.canonicalFilePath();
                    Tileset *ts = loadTileset(source);
                    if (!ts)
                        return false;
                    BuildingTilesMgr::instance()->addTileset(ts);
                    mTilesets += ts->name();
                } else {
                    mError = tr("Unknown value name '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else if (block.name == QLatin1String("layers")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name == QLatin1String("tile")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Tile);
                } else if (kv.name == QLatin1String("object")) {
                    mLayers += LayerInfo(kv.value, LayerInfo::Object);
                } else {
                    mError = tr("Unknown layer type '%1'.\n%2")
                            .arg(kv.name)
                            .arg(path);
                    return false;
                }
            }
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    if (missingTilesets.size()) {
        ListOfStringsDialog dialog(tr("The following tileset files were not found."),
                                   missingTilesets,
                                   BuildingEditorWindow::instance());
        dialog.setWindowTitle(tr("Missing Tilesets"));
        dialog.exec();
    }

    return true;
}

bool BuildingTMX::writeTxt()
{
    SimpleFile simpleFile;

    QDir tilesDir(BuildingPreferences::instance()->tilesDirectory());
    SimpleFileBlock tilesetBlock;
    tilesetBlock.name = QLatin1String("tilesets");
    foreach (Tiled::Tileset *tileset, BuildingTilesMgr::instance()->tilesets()) {
        QString relativePath = tilesDir.relativeFilePath(tileset->imageSource());
        relativePath.truncate(relativePath.length() - 4); // remove .png
        tilesetBlock.values += SimpleFileKeyValue(QLatin1String("tileset"), relativePath);
    }
    simpleFile.blocks += tilesetBlock;

    SimpleFileBlock layersBlock;
    layersBlock.name = QLatin1String("layers");
    foreach (LayerInfo layerInfo, mLayers) {
        QString key;
        switch (layerInfo.mType) {
        case LayerInfo::Tile: key = QLatin1String("tile"); break;
        case LayerInfo::Object: key = QLatin1String("object"); break;
        }
        layersBlock.values += SimpleFileKeyValue(key, layerInfo.mName);
    }
    simpleFile.blocks += layersBlock;

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(++mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(txtPath())) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;
}

Tiled::Tileset *BuildingTMX::loadTileset(const QString &source)
{
    QFileInfo info(source);

    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);

    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            delete ts;
            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }
    return ts;
}

bool BuildingTMX::upgradeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }

    int userVersion = userFile.version(); // may be zero for unversioned file
    if (userVersion == VERSION_LATEST)
        return true;

    if (userVersion > VERSION_LATEST) {
        mError = tr("%1 is from a newer version of TileZed").arg(txtName());
        return false;
    }

    // Not the latest version -> upgrade it.

    QString sourcePath = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + txtName();

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    // UPGRADE HERE

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}

bool BuildingTMX::mergeTxt()
{
    QString userPath = txtPath();

    SimpleFile userFile;
    if (!userFile.read(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    Q_ASSERT(userFile.version() == VERSION_LATEST);

    QString sourcePath = QCoreApplication::applicationDirPath() + QLatin1Char('/')
            + txtName();

    SimpleFile sourceFile;
    if (!sourceFile.read(sourcePath)) {
        mError = sourceFile.errorString();
        return false;
    }
    Q_ASSERT(sourceFile.version() == VERSION_LATEST);

    int userSourceRevision = userFile.value("source_revision").toInt();
    int sourceRevision = sourceFile.value("revision").toInt();
    if (sourceRevision == userSourceRevision)
        return true;

    // MERGE HERE

    int tilesetBlock = -1;
    int layersBlock = -1;
    QMap<QString,SimpleFileKeyValue> userTilesetMap;
    QMap<QString,int> userLayersMap;
    int blockIndex = 0;
    foreach (SimpleFileBlock b, userFile.blocks) {
        if (b.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, b.values) {
                userTilesetMap[kv.value] = kv;
            }
            tilesetBlock = blockIndex;
        } else if (b.name == QLatin1String("layers"))  {
            int layerIndex = 0;
            foreach (SimpleFileKeyValue kv, b.values)
                userLayersMap[kv.value] = layerIndex++;
            layersBlock = blockIndex;
        }
        ++blockIndex;
    }

    foreach (SimpleFileBlock b, sourceFile.blocks) {
        if (b.name == QLatin1String("tilesets")) {
            foreach (SimpleFileKeyValue kv, b.values) {
                if (!userTilesetMap.contains(kv.value)) {
                    userFile.blocks[tilesetBlock].values += kv;
                    qDebug() << "TMXConfig.txt merge: added tileset" << kv.value;
                }
            }
        } else if (b.name == QLatin1String("layers"))  {
            int insertIndex = 0;
            foreach (SimpleFileKeyValue kv, b.values) {
                if (userLayersMap.contains(kv.value)) {
                    insertIndex = userLayersMap[kv.value] + 1;
                } else {
                    userFile.blocks[layersBlock].values.insert(insertIndex, kv);
                    userLayersMap[kv.value] = insertIndex;
                    qDebug() << "TMXConfig.txt merge: added layer" << kv.value << "at" << insertIndex;
                    insertIndex++;
                }
            }
        }
    }

    userFile.replaceValue("revision", QString::number(sourceRevision + 1));
    userFile.replaceValue("source_revision", QString::number(sourceRevision));

    userFile.setVersion(VERSION_LATEST);
    if (!userFile.write(userPath)) {
        mError = userFile.errorString();
        return false;
    }
    return true;
}
