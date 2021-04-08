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

#ifndef WORLDEDMGR_H
#define WORLDEDMGR_H

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include "filesystemwatcher.h"

class World;
class WorldCell;
class WorldCellLevel;
class WorldCellLot;

namespace WorldEd {

class WorldEdMgr : public QObject
{
    Q_OBJECT
public:
    static WorldEdMgr *instance();
    static void deleteInstance();

    void addProject(const QString &fileName);

    WorldCell *cellForMap(const QString &fileName);

    void setLevelVisible(WorldCellLevel *level, bool visible);
    void setLotVisible(WorldCellLot *lot, bool visible);

    void setSelectedLots(const QSet<WorldCellLot*> &selected);
    const QSet<WorldCellLot*> &selectedLots() const
    { return mSelectedLots; }

    int worldCount() const
    { return mWorlds.size(); }
    World *worldAt(int n);
    QString worldFileName(int n);

signals:
    void beforeWorldChanged(const QString &fileName);
    void afterWorldChanged(const QString &fileName);

    void levelVisibilityChanged(WorldCellLevel *level);
    void lotVisibilityChanged(WorldCellLot *lot);

    void selectedLotsChanged();

public slots:
    void fileChanged(const QString &fileName);
    void fileChangedTimeout();
    
private:
    Q_DISABLE_COPY(WorldEdMgr)
    static WorldEdMgr *mInstance;
    WorldEdMgr(QObject *parent = 0);
    ~WorldEdMgr();

    QList<World*> mWorlds;
    QStringList mWorldFileNames;
    Tiled::Internal::FileSystemWatcher mWatcher;
    QSet<QString> mChangedFiles;
    QTimer mChangedFilesTimer;
    QSet<WorldCellLot*> mSelectedLots;
    QHash<World*,QMap<QString,WorldCell*>> mCheckedDocuments;
    QSet<QString> mMapWithoutWorld;
};

} // namespace WorldEd

#endif // WORLDEDMGR_H
