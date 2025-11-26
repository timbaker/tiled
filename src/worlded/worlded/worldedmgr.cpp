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

#include "mainwindow.h"

#include <QDebug>
#include <QFileInfo>
#include <QMessageBox>

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
    connect(&mWatcher, &Tiled::Internal::FileSystemWatcher::fileChanged, this, &WorldEdMgr::fileChanged);

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);
    connect(&mChangedFilesTimer, &QTimer::timeout,
            this, &WorldEdMgr::fileChangedTimeout);
}

WorldEdMgr::~WorldEdMgr()
{
    qDeleteAll(mWorlds);
}

void WorldEdMgr::addProject(const QString &fileName)
{
    WorldReader reader;
    World *world = reader.readWorld(fileName);
    if (world == nullptr) {
        QMessageBox::warning(Tiled::Internal::MainWindow::instance(), QLatin1String("Error reading PZW"), reader.errorString());
        return;
    }

    mWorlds += world;
    mWorldFileNames += fileName;

    mWatcher.addPath(fileName);
}

WorldCell *WorldEdMgr::cellForMap(const QString &fileName)
{
    QFileInfo info2(fileName);
    QString canonicalPath = info2.canonicalFilePath();
    if (mMapWithoutWorld.contains(canonicalPath)) {
        return nullptr;
    }
    for (World *world : std::as_const(mWorlds)) {
        if (mCheckedDocuments.contains(world) == false) {
            auto& nameToCell = mCheckedDocuments[world];
            for (int y = 0; y < world->height(); y++) {
                for (int x = 0; x < world->width(); x++) {
                    WorldCell *cell = world->cellAt(x, y);
                    if (cell->mapFilePath().isEmpty())
                        continue;
                    QFileInfo info1(cell->mapFilePath());
                    nameToCell.insert(info1.canonicalFilePath(), cell);
                }
            }
        }
        const auto& nameToCell = mCheckedDocuments[world];
        if (nameToCell.contains(canonicalPath)) {
            return nameToCell[canonicalPath];
        }
    }

    mMapWithoutWorld.insert(fileName);
    return nullptr;
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
    if (mSelectedLots == selected) return;
    mSelectedLots = selected;
    emit selectedLotsChanged();
}

World *WorldEdMgr::worldAt(int n)
{
    if (n >= 0 && n < mWorlds.size())
        return mWorlds[n];
    return nullptr;
}

QString WorldEdMgr::worldFileName(int n)
{
    if (n >= 0 && n < mWorlds.size())
        return mWorldFileNames[n];
    return QString();
}

WorldCellLotList WorldEdMgr::getOverlappingLots(WorldCell *cell, bool includeAdjacentCells)
{
    WorldCellLotList result;
    const QRect cellRect(cell->pos() * 300, QSize(300, 300));
    const QRect adjacentRect(cell->x() - 1, cell->y() - 1, 3+1, 3+1);
    for (int i = 0; i < mWorlds.size(); i++) {
        World *world = mWorlds[i];
        for (int cy = 0; cy < world->height(); cy++) {
            for (int cx = 0; cx < world->width(); cx++) {
                if (!includeAdjacentCells && adjacentRect.contains(cx, cy))
                    continue;
                WorldCell *cell2 = world->cellAt(cx, cy);
                if (cell2 == nullptr)
                    continue;
                if (cell2 == cell)
                    continue;
                for (WorldCellLot *lot : cell2->lots()) {
                    if (lot->bounds().translated(cell2->pos() * 300).intersects(cellRect)) {
                        result += lot;
                    }
                }
            }
        }
    }
    return result;
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
    QStringList files(mChangedFiles.begin(), mChangedFiles.end());
    mChangedFiles.clear();

    foreach (QString fileName, files) {
        QFileInfo info(fileName);
        for (int i = 0; i < mWorlds.size(); i++) {
            if (info == QFileInfo(mWorldFileNames[i])) {
                setSelectedLots(QSet<WorldCellLot*>());
                emit beforeWorldChanged(mWorldFileNames[i]);
                if (mCheckedDocuments.contains(mWorlds[i])) {
                    mCheckedDocuments.remove(mWorlds[i]);
                }
                mMapWithoutWorld.clear();
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
