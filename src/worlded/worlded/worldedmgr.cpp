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

#include <QDebug>
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
    connect(&mWatcher, SIGNAL(fileChanged(QString)), SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, SIGNAL(timeout()),
            SLOT(fileChangedTimeout()));
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
    mWorldFileNames += fileName;

    mWatcher.addPath(fileName);
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

void WorldEdMgr::setLevelVisible(WorldCellLevel *level, bool visible)
{
    if (level->isVisible() != visible) {
        level->setVisible(visible);
        emit levelVisibilityChanged(level);
    }
}

void WorldEdMgr::setLotVisible(WorldCellLot *lot, bool visible)
{
    if (lot->isVisible() != visible) {
        lot->setVisible(visible);
        emit lotVisibilityChanged(lot);
    }
}

void WorldEdMgr::setSelectedLots(const QSet<WorldCellLot *> &selected)
{
    mSelectedLots = selected;
    emit selectedLotsChanged();
}

World *WorldEdMgr::worldAt(int n)
{
    if (n >= 0 && n < mWorlds.size())
        return mWorlds[n];
    return 0;
}

const QString &WorldEdMgr::worldFileName(int n)
{
    if (n >= 0 && n < mWorlds.size())
        return mWorldFileNames[n];
    return QString();
}

void WorldEdMgr::fileChanged(const QString &fileName)
{
    qDebug() << "WorldEdMgr::fileChanged" << fileName;
    mChangedFiles.insert(fileName);
    mChangedFilesTimer.start();
}

void WorldEdMgr::fileChangedTimeout()
{
    qDebug() << "WorldEdMgr::fileChangedTimeout";
    QStringList files = mChangedFiles.toList();
    mChangedFiles.clear();

    foreach (QString fileName, files) {
        QFileInfo info(fileName);
        for (int i = 0; i < mWorlds.size(); i++) {
            if (info == QFileInfo(mWorldFileNames[i])) {
                setSelectedLots(QSet<WorldCellLot*>());
                emit beforeWorldChanged(mWorldFileNames[i]);
                delete mWorlds[i];
                mWorlds.removeAt(i);
                mWorldFileNames.removeAt(i);
                mWatcher.removePath(fileName);
                if (info.exists())
                    addProject(fileName);
                emit afterWorldChanged(fileName);
            }
        }
    }
}
