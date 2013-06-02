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

#include "worldedmgr.h"

#include "world.h"
#include "worldcell.h"
#include "worldreader.h"

#include <QFileInfo>

using namespace WorldEd;

WorldEdMgr *WorldEdMgr::mInstance = 0;

WorldEdMgr *WorldEdMgr::instance()
{
    if (!mInstance)
        mInstance = new WorldEdMgr;
    return mInstance;
}

WorldEdMgr::WorldEdMgr(QObject *parent) :
    QObject(parent)
{
}

WorldEdMgr::~WorldEdMgr()
{
    qDeleteAll(mWorlds);
}

void WorldEdMgr::addProject(const QString &fileName)
{
    WorldReader reader;
    World *world = reader.readWorld(fileName);
    if (!world)
        return;

    mWorlds += world;
}

WorldCell *WorldEdMgr::cellForMap(const QString &fileName)
{
    foreach (World *world, mWorlds) {
        for (int y = 0; y < world->height(); y++) {
            for (int x = 0; x < world->width(); x++) {
                WorldCell *cell = world->cellAt(x, y);
                if (cell->mapFilePath().isEmpty())
                    continue;
                QFileInfo info1(cell->mapFilePath());
                QFileInfo info2(fileName);
                if (info1 == info2)
                    return cell;
            }
        }
    }
    return 0;
}

