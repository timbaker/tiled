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

#include "buildingfloor.h"
#include "buildingobjects.h"

using namespace BuildingEditor;

/////

BuildingObject::BuildingObject(BuildingFloor *floor, int x, int y, Direction dir) :
    mFloor(floor),
    mX(x),
    mY(y),
    mDir(dir),
    mTile(0)
{
}

QString BuildingObject::dirString() const
{
    static const char *s[] = { "N", "S", "E", "W" };
    return QLatin1String(s[mDir]);
}

BuildingObject::Direction BuildingObject::dirFromString(const QString &s)
{
    if (s == QLatin1String("N")) return N;
    if (s == QLatin1String("S")) return S;
    if (s == QLatin1String("W")) return W;
    if (s == QLatin1String("E")) return E;
    return Invalid;
}

bool BuildingObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor; // hackery for BaseObjectTool

    // +1 because doors/windows can be on the outside edge of the building
    QRect floorBounds(0, 0, floor->width() + 1, floor->height() + 1);
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void BuildingObject::rotate(bool right)
{
    Q_UNUSED(right)
    mDir = (mDir == N) ? W : N;

    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();
    if (right) {
        int x = mX;
        mX = oldHeight - mY - 1;
        mY = x;
        if (mDir == N)
            ;
        else
            mX++;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - 1;
        if (mDir == N)
            mY++;
        else
            ;
    }
}

void BuildingObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - 1;
        if (mDir == W)
            mX++;
    } else {
        mY = mFloor->height() - mY - 1;
        if (mDir == N)
            mY++;
    }
}

int BuildingObject::index()
{
    return mFloor->indexOf(this);
}

/////

QRect Stairs::bounds() const
{
    if (mDir == N)
        return QRect(mX, mY, 1, 5);
    if (mDir == W)
        return QRect(mX, mY, 5, 1);
    return QRect();
}

void Stairs::rotate(bool right)
{
    BuildingObject::rotate(right);
    if (right) {
        if (mDir == W) // used to be N
            mX -= 5;
    } else {
        if (mDir == N) // used to be W
            mY -= 5;
    }
}

void Stairs::flip(bool horizontal)
{
    BuildingObject::flip(horizontal);
    if (mDir == W && horizontal)
        mX -= 5;
    else if (mDir == N && !horizontal)
        mY -= 5;
}

bool Stairs::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because stairs can't be on the outside edge of the building.
    QRect floorBounds(0, 0, floor->width(), floor->height());
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

int Stairs::getOffset(int x, int y)
{
    if (mDir == N) {
        if (x == mX) {
            int index = y - mY;
            if (index == 1)
                return 2 + 8; // FIXME: +8 is Tileset::columnCount()
            if (index == 2)
                return 1 + 8; // FIXME: +8 is Tileset::columnCount()
            if (index == 3)
                return 0 + 8; // FIXME: +8 is Tileset::columnCount()
        }
    }
    if (mDir == W) {
        if (y == mY) {
            int index = x - mX;
            if (index == 1)
                return 2;
            if (index == 2)
                return 1;
            if (index == 3)
                return 0;
        }
    }
    return -1;
}

/////
