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

#include "buildingobjects.h"

#include "buildingfloor.h"
#include "furnituregroups.h"

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

FurnitureObject::FurnitureObject(BuildingFloor *floor, int x, int y) :
    BuildingObject(floor, x, y, Invalid),
    mFurnitureTile(0)
{

}

QRect FurnitureObject::bounds() const
{
    return mFurnitureTile ? QRect(pos(), mFurnitureTile->size())
                           : QRect(mX, mY, 1, 1);
}

void FurnitureObject::rotate(bool right)
{
    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();

    FurnitureTile *oldTile = mFurnitureTile;
    FurnitureTile *newTile = mFurnitureTile;
    if (right) {
        int index = FurnitureTiles::orientIndex(oldTile->mOrient) + 1;
        newTile = oldTile->owner()->mTiles[index % 4];
#if 0
        if (oldTile->isW())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureN];
        else if (oldTile->isN())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureE];
        else if (oldTile->isE())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureS];
        else if (oldTile->isS())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureW];
#endif
    } else {
        int index = 4 + FurnitureTiles::orientIndex(oldTile->mOrient) - 1;
        newTile = oldTile->owner()->mTiles[index % 4];
#if 0
        if (oldTile->isW())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureS];
        else if (oldTile->isS())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureE];
        else if (oldTile->isE())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureN];
        else if (oldTile->isN())
            newTile = oldTile->owner()->mTiles[FurnitureTile::FurnitureW];
#endif
    }

    if (right) {
        int x = mX;
        mX = oldHeight - mY - newTile->size().width();
        mY = x;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - newTile->size().height();
    }

    // Stop things going out of bounds, which can happen if the furniture
    // is asymmetric.
    if (mX < 0)
        mX = 0;
    if (mX + newTile->size().width() > mFloor->width())
        mX = mFloor->width() - newTile->size().width();
    if (mY < 0)
        mY = 0;
    if (mY + newTile->size().height() > mFloor->height())
        mY = mFloor->height() - newTile->size().height();

    mFurnitureTile = newTile;
}

void FurnitureObject::flip(bool horizontal)
{
    if (horizontal) {
        int oldWidth = mFurnitureTile->size().width();
        if (mFurnitureTile->isW())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureE];
        else if (mFurnitureTile->isE())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureW];
        else if (mFurnitureTile->isNW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNE);
        else if (mFurnitureTile->isNE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNW);
        else if (mFurnitureTile->isSW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSE);
        else if (mFurnitureTile->isSE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSW);
        mX = mFloor->width() - mX - qMax(oldWidth, mFurnitureTile->size().width());
    } else {
        int oldHeight = mFurnitureTile->size().height();
        if (mFurnitureTile->isN())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureS];
        else if (mFurnitureTile->isS())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureN];
        else if (mFurnitureTile->isNW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSW);
        else if (mFurnitureTile->isSW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNW);
        else if (mFurnitureTile->isNE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSE);
        else if (mFurnitureTile->isSE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNE);
        mY = mFloor->height() - mY - qMax(oldHeight, mFurnitureTile->size().height());
    }
}

bool FurnitureObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because furniture can't be on the outside edge of the building.
    QRect floorBounds(0, 0, floor->width(), floor->height());
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void FurnitureObject::setFurnitureTile(FurnitureTile *tile)
{
    mFurnitureTile = tile;
}

/////
