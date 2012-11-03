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

RoofObject::RoofObject(BuildingFloor *floor, int x, int y,
                       BuildingObject::Direction dir, int length,
                       int width1, int width2, int gap) :
    BuildingObject(floor, x, y, dir),
    mLength(length),
    mWidth1(width1),
    mWidth2(width2),
    mGap(gap),
    mMidTile(false),
    mHeight(3),
    mCapped(true)
{
}

QRect RoofObject::bounds() const
{
    if (mDir == N)
        return mMidTile ? QRect(mX, mY, 3, mLength) // restricted to 3 tiles wide
                        : QRect(mX, mY, mWidth1 + mWidth2 + mGap, mLength);
    if (mDir == W)
        return mMidTile ? QRect(mX, mY, mLength, 3) // restricted to 3 tiles wide
                        : QRect(mX, mY, mLength, mWidth1 + mWidth2 + mGap);
    return QRect();
}

void RoofObject::rotate(bool right)
{
}

void RoofObject::flip(bool horizontal)
{
}

bool RoofObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because roofs can't be on the outside edge of the building.
    // However, the E or S cap wall tiles can be on the outside edge.
    QRect floorBounds(0, 0, floor->width(), floor->height());
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void RoofObject::resize(int length, int thickness)
{
    if (thickness <= 2) {
        if (mWidth1) mWidth1 = qMin(mHeight,mWidth2 ? 1 : 2);
        if (mWidth2) mWidth2 = qMin(mHeight,mWidth1 ? 1 : 2);
        mGap = thickness - mWidth1 - mWidth2;
        mMidTile = false;
    } else if (thickness == 3) {
        if (mWidth1) mWidth1 = qMin(mHeight,mWidth2 ? 1 : 3);
        if (mWidth2) mWidth2 = qMin(mHeight,mWidth1 ? 1 : 3);
        mGap = thickness - mWidth1 - mWidth2;
        if (mGap == 1)
            mGap = 0;
        mMidTile = mWidth1 && mWidth2;
    } else if (thickness < 6) {
        if (mWidth1) mWidth1 = qMin(mHeight, mWidth2 ? 2 : 3);
        if (mWidth2) mWidth2 = qMin(mHeight,mWidth1 ? 2 : 3);
        mGap = thickness - mWidth1 - mWidth2;
        mMidTile = false;
    } else {
        if (mWidth1) mWidth1 = qMin(mHeight, 3);
        if (mWidth2) mWidth2 = qMin(mHeight, 3);
        mGap = qMax(0, thickness - mWidth1 - mWidth2);
        mMidTile = false;
    }

    mLength = length;
}

void RoofObject::toggleWidth1()
{
    int thickness = this->thickness();
    if (mWidth1 > 0) {
        mWidth1 = 0;
        mGap += mWidth1;
    } else {
        mWidth1 = 1; // some non-zero value
    }
    resize(length(), thickness);
}

void RoofObject::toggleWidth2()
{
    int thickness = this->thickness();
    if (mWidth2 > 0) {
        mGap += mWidth2;
        mWidth2 = 0;
    } else {
        mWidth2 = 1; // some non-zero value
    }
    resize(length(), thickness);
}

void RoofObject::setHeight(int height)
{
    mHeight = height;
    resize(length(), thickness());
}

void RoofObject::toggleCapped()
{
    mCapped = !mCapped;
}

#include "buildingtiles.h"
BuildingTile *RoofObject::tile(RoofObject::RoofTile tile) const
{
    QString tilesetName = QLatin1String("roofs_01");
    if (tile >= CapRiseE1)
        tilesetName = QLatin1String("walls_exterior_roofs_03");
    int index = 0;
    switch (tile) {
    case FlatS1: index = 0; break;
    case FlatS2: index = 1; break;
    case FlatS3: index = 2; break;

    case FlatE1: index = 5; break;
    case FlatE2: index = 4; break;
    case FlatE3: index = 3; break;

    case HalfFlatS: index = 15; break;
    case HalfFlatE: index = 14; break;

    case CapRiseE1: index = 0; break;
    case CapRiseE2: index = 1; break;
    case CapRiseE3: index = 2; break;
    case CapFallE1: index = 8; break;
    case CapFallE2: index = 9; break;
    case CapFallE3: index = 10; break;

    case CapRiseS1: index = 13; break;
    case CapRiseS2: index = 12; break;
    case CapRiseS3: index = 11; break;
    case CapFallS1: index = 5; break;
    case CapFallS2: index = 4; break;
    case CapFallS3: index = 3; break;

    case CapMidS: index = 6; break;
    case CapMidE: index = 14; break;

    case CapGapS1: case CapGapS2:
    case CapGapE1: case CapGapE2:
        // These are the 1/3- and 2/3-height walls which don't have tiles
        return 0;

    case CapGapS3: tilesetName = QLatin1String("walls_exterior_house_01"); index = 49; break;
    case CapGapE3: tilesetName = QLatin1String("walls_exterior_house_01"); index = 48; break;
    }
    return BuildingTiles::instance()->getFurnitureTile(BuildingTiles::instance()->nameForTile(tilesetName, index));
}

/////

RoofCornerObject::RoofCornerObject(BuildingFloor *floor, int x, int y,
                                   int width, int height, int depth) :
    BuildingObject(floor, x, y, Invalid),
    mWidth(width),
    mHeight(height),
    mDepth(depth),
    mInner(true)
{
}

QRect RoofCornerObject::bounds() const
{
    return QRect(mX, mY, mWidth, mHeight);
}

void RoofCornerObject::rotate(bool right)
{
}

void RoofCornerObject::flip(bool horizontal)
{
}

bool RoofCornerObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because roofs can't be on the outside edge of the building.
    QRect floorBounds(0, 0, floor->width(), floor->height());
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void RoofCornerObject::setDepth(int depth)
{
    mDepth = depth;
}

void RoofCornerObject::resize(int width, int height)
{
    mWidth = width, mHeight = height;
}

void RoofCornerObject::toggleInner()
{
    mInner = !mInner;
}

BuildingTile *RoofCornerObject::tile(RoofCornerObject::RoofTile tile) const
{
    QString tilesetName = QLatin1String("roofs_01");
    int index = 0;

    switch (tile) {
    case FlatS1: index = 0; break;
    case FlatS2: index = 1; break;
    case FlatS3: index = 2; break;

    case FlatE1: index = 5; break;
    case FlatE2: index = 4; break;
    case FlatE3: index = 3; break;

    case Outer1: index = 8; break;
    case Outer2: index = 9; break;
    case Outer3: index = 10; break;

    case Inner1: index = 11; break;
    case Inner2: index = 12; break;
    case Inner3: index = 13; break;
    }

    return BuildingTiles::instance()->getFurnitureTile(BuildingTiles::instance()->nameForTile(tilesetName, index));
}

/////
