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

class World;
class WorldCell;

namespace WorldEd {

class WorldEdMgr : public QObject
{
    Q_OBJECT
public:
    static WorldEdMgr *instance();
    static void deleteInstance();

    void addProject(const QString &fileName);

    WorldCell *cellForMap(const QString &fileName);
    
signals:
    
public slots:
    
private:
    Q_DISABLE_COPY(WorldEdMgr)
    static WorldEdMgr *mInstance;
    WorldEdMgr(QObject *parent = 0);
    ~WorldEdMgr();

    QList<World*> mWorlds;
};

} // namespace WorldEd

#endif // WORLDEDMGR_H
