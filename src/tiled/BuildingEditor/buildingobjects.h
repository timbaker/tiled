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

#ifndef BUILDINGOBJECTS_H
#define BUILDINGOBJECTS_H

#include <QRect>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;

class BaseMapObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W
    };

    BaseMapObject(BuildingFloor *floor, int x, int y, Direction mDir);

    BuildingFloor *floor() const
    { return mFloor; }

    int index();

    virtual QRect bounds() const
    { return QRect(mX, mY, 1, 1); }

    void setPos(int x, int y)
    { mX = x, mY = y; }

    void setPos(const QPoint &pos)
    { setPos(pos.x(), pos.y()); }

    QPoint pos() const
    { return QPoint(mX, mY); }

    int x() const { return mX; }
    int y() const { return mY; }

    void setDir(Direction dir)
    { mDir = dir; }

    Direction dir() const
    { return mDir; }

    BuildingTile *mTile;

protected:
    BuildingFloor *mFloor;
    Direction mDir;
    int mX;
    int mY;
};

class Door : public BaseMapObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }

    BuildingTile *mFrameTile;
};

class Stairs : public BaseMapObject
{
public:
    Stairs(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir)
    {
    }

    QRect bounds() const;

    int getOffset(int x, int y);
};

class Window : public BaseMapObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BaseMapObject(floor, x, y, dir)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
