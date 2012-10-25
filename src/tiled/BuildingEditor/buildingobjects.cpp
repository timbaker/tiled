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

BaseMapObject::BaseMapObject(BuildingFloor *floor, int x, int y, Direction dir) :
    mFloor(floor),
    mX(x),
    mY(y),
    mDir(dir),
    mTile(0)
{
}

QString BaseMapObject::dirString() const
{
    static const char *s[] = { "N", "S", "E", "W" };
    return QLatin1String(s[mDir]);
}

BaseMapObject::Direction BaseMapObject::dirFromString(const QString &s)
{
    if (s == QLatin1String("N")) return N;
    if (s == QLatin1String("S")) return S;
    if (s == QLatin1String("W")) return W;
    if (s == QLatin1String("E")) return E;
    return Invalid;
}

int BaseMapObject::index()
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
