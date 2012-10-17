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

#ifndef BUILDING_H
#define BUILDING_H

#include <QList>

namespace BuildingEditor {

class BuildingFloor;
class Layout;
class WallType;

class Building
{
public:
    Building(int width, int height);

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    void recreate(const QList<Layout*> &layouts, WallType *wallType);

    const QList<BuildingFloor*> &floors() const
    { return mFloors; }

    void insertFloor(int index, BuildingFloor *floor);
    BuildingFloor *removeFloor(int index);

private:
    int mWidth, mHeight;
    QList<BuildingFloor*> mFloors;
};

} // namespace BuildingEditor

#endif // BUILDING_H
