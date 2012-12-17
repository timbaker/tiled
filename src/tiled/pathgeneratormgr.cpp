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

#include "pathgeneratormgr.h"

#include "tilesetmanager.h"

#include "pathgenerator.h"
#include "tileset.h"

#include <QFileInfo>
#include <QImage>

using namespace Tiled;
using namespace Internal;

PathGeneratorMgr *PathGeneratorMgr::mInstance = 0;

PathGeneratorMgr *PathGeneratorMgr::instance()
{
    if (!mInstance)
        mInstance = new PathGeneratorMgr;
    return mInstance;
}

void PathGeneratorMgr::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

void PathGeneratorMgr::insertGenerator(int index, PathGenerator *pgen)
{
    Q_ASSERT(pgen && !mGenerators.contains(pgen));
    pgen->refCountUp();
    mGenerators.insert(index, pgen);
}

PathGenerator *PathGeneratorMgr::removeGenerator(int index)
{
    mGenerators.at(index)->refCountDown();
    return mGenerators.takeAt(index);
}

PathGeneratorMgr::PathGeneratorMgr(QObject *parent) :
    QObject(parent)
{
    mGenerators += new PG_Fence(QLatin1String("Fence #1"));
    mGenerators += new PG_StreetLight(QLatin1String("Street Light #1"));

    if (Tileset *ts = loadTileset(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\FromLemmy\\Maps\\Tiles\\fencing_01.png")))
        mTilesets += ts;
    if (Tileset *ts = loadTileset(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\FromLemmy\\Maps\\Tiles\\lighting_outdoor_01.png")))
        mTilesets += ts;
}

Tileset *PathGeneratorMgr::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128); // FIXME: hard-coded size!!!

    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            delete ts;
//            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }

    return ts;
}
