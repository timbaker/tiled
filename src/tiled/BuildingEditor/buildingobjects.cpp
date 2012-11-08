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
#include "buildingtiles.h"
#include "furnituregroups.h"
#include "rooftiles.h"

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
    } else {
        int index = 4 + FurnitureTiles::orientIndex(oldTile->mOrient) - 1;
        newTile = oldTile->owner()->mTiles[index % 4];
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
                       int thickness, int width1, int width2, bool capped1,
                       bool capped2, int depth) :
    BuildingObject(floor, x, y, dir),
    mLength(length),
    mThickness(thickness),
    mWidth1(width1),
    mWidth2(width2),
    mDepth(depth),
    mCapped1(capped1),
    mCapped2(capped2)
{
}

QRect RoofObject::bounds() const
{
    if (mDir == N)
        return QRect(mX, mY, mThickness, mLength);
    if (mDir == W)
        return QRect(mX, mY, mLength, mThickness);
    return QRect();
}

void RoofObject::rotate(bool right)
{
    int oldFloorWidth = mFloor->height();
    int oldFloorHeight = mFloor->width();

    mDir = isW() ? N : W;

    if (right) {
        int x = mX;
        mX = oldFloorHeight - mY - bounds().width();
        mY = x;

        if (isW()) // was N
            qSwap(mCapped1, mCapped2);

    } else {
        int x = mX;
        mX = mY;
        mY = oldFloorWidth - x - bounds().height();

        if (isN()) // was W
            qSwap(mCapped1, mCapped2);
    }
}

void RoofObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - bounds().width();
        if (isW())
            qSwap(mCapped1, mCapped2);
    } else {
        mY = mFloor->height() - mY - bounds().height();
        if (isN())
            qSwap(mCapped1, mCapped2);
    }
}

BuildingTile *RoofObject::tile(int alternate) const
{
    if (alternate == 1)
        return mCapTile;
    else
        return mTile;
}

void RoofObject::setTile(BuildingTile *tile, int alternate)
{
    if (alternate == 1)
        mCapTile = tile;
    else
        mTile = tile;
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
    if (thickness == 1) {
        if (mWidth1) mWidth1 = 1;
        if (mWidth2) mWidth2 = 1;
    } else if (thickness == 2) {
        if (mWidth1) mWidth1 = qMin(mDepth, mWidth2 ? 1 : 2);
        if (mWidth2) mWidth2 = qMin(mDepth, mWidth1 ? 1 : 2);
    } else if (thickness == 3) {
        if (mWidth1) mWidth1 = qMin(mDepth, mWidth2 ? 1 : 3);
        if (mWidth2) mWidth2 = qMin(mDepth, mWidth1 ? 1 : 3);
    } else if (thickness < 6) {
        if (mWidth1) mWidth1 = qMin(mDepth, mWidth2 ? 2 : 3);
        if (mWidth2) mWidth2 = qMin(mDepth, mWidth1 ? 2 : 3);
    } else {
        if (mWidth1) mWidth1 = qMin(mDepth, 3);
        if (mWidth2) mWidth2 = qMin(mDepth, 3);
    }

    mLength = length;
    mThickness = thickness;
}

void RoofObject::toggleWidth1()
{
    int thickness = this->thickness();
    if (mWidth1 > 0) {
        mWidth1 = 0;
    } else {
        mWidth1 = 1; // some non-zero value
    }
    resize(length(), thickness);
}

void RoofObject::toggleWidth2()
{
    int thickness = this->thickness();
    if (mWidth2 > 0) {
        mWidth2 = 0;
    } else {
        mWidth2 = 1; // some non-zero value
    }
    resize(length(), thickness);
}

void RoofObject::setDepth(int height)
{
    mDepth = height;
    resize(length(), thickness());
}

void RoofObject::toggleCapped1()
{
    mCapped1 = !mCapped1;
}

void RoofObject::toggleCapped2()
{
    mCapped2 = !mCapped2;
}

int RoofObject::actualDepth() const
{
    int depthW = (isW() && mWidth1 > 0) ? mDepth : 0;
    int depthE = (isW() && mWidth2 > 0) ? mDepth : 0;
    int depthWE = qMax(depthW, depthE);
    if (depthW + depthE > bounds().width()) {
        depthWE = bounds().width() / 2;
    }

    int depthN = (isN() && mWidth1 > 0) ? mDepth : 0;
    int depthS = (isN() && mWidth2 > 0) ? mDepth : 0;
    int depthNS = qMax(depthN, depthS);
    if (depthN + depthS > bounds().height()) {
        depthNS = bounds().height() / 2;
    }

    if (depthWE && depthNS)
        return qMin(depthWE, depthNS);
    return depthWE + depthNS;
}

RoofObject::RoofHeight RoofObject::roofHeight() const
{
    if (mThickness == 1) {
        if (mWidth1 && mWidth2)
            return Point5;
        if (mWidth1 || mWidth2)
            return One;
        return (mDepth == 3) ? Three : ((mDepth == 2) ? Two : One);
    }
    if (mThickness == 2) {
        if (mWidth1 && mWidth2)
            return One;
        if (mWidth1 || mWidth2)
            return (mDepth >= 2) ? Two : One;
        return (mDepth == 3) ? Three : ((mDepth == 2) ? Two : One);
    }
    if (mThickness == 3) {
        if (mWidth1 && mWidth2)
            return (mDepth > 1) ? OnePoint5 : One;
        return (mDepth == 3) ? Three : ((mDepth == 2) ? Two : One);
    }
    if (mThickness < 6) {
        if (mWidth1 && mWidth2)
            return (mDepth > 1) ? Two : One;
        return (mDepth == 3) ? Three : ((mDepth == 2) ? Two : One);
    }

    return (mDepth == 3) ? Three : ((mDepth == 2) ? Two : One);
}

BuildingTile *RoofObject::roofTile(RoofObject::RoofTile tile) const
{
    BuildingTile *btile =  mTile;
    if (tile >= CapRiseE1)
        btile = mCapTile;

    if (!btile)
        return 0;

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

    case FlatTopW: index = 22; break; // not even sure about these
    case FlatTopN: index = 23; break;

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

    case CapMidPt5S: index = 7; break;
    case CapMidPt5E: index = 15; break;

    case CapGapS1: case CapGapS2:
    case CapGapE1: case CapGapE2:
        // These are the 1/3- and 2/3-height walls which don't have tiles
        return 0;

    case CapGapS3:
        btile = RoofTiles::instance()->gapTileForCap(btile);
        if (!btile)
            return 0;
        index = 1; // South wall
        break;
    case CapGapE3:
        btile = RoofTiles::instance()->gapTileForCap(btile);
        if (!btile)
            return 0;
        index = 0; // West wall
        break;
    }
    return BuildingTiles::instance()->getFurnitureTile(
                BuildingTiles::instance()->nameForTile(btile->mTilesetName,
                                                       btile->mIndex + index));
}

QRect RoofObject::eastEdge()
{
    QRect r = bounds();
    if (isN())
        return QRect(r.right() - width2() + 1, r.top(),
                      width2(), r.height());
    return QRect();
}

QRect RoofObject::southEdge()
{
    QRect r = bounds();
    if (isW())
        return QRect(r.left(), r.bottom() - width2() + 1,
                     r.width(), width2());
    return QRect();
}

QRect RoofObject::eastGap(RoofHeight height)
{
    if (height != roofHeight())
        return QRect();
    QRect r = bounds();
    if (isN() && !mWidth2) {
        return QRect(r.right()+1, r.top(), 1, r.height());
    }
    if (isW() && mCapped2) {
        return QRect(r.right()+1, r.top() + mWidth1, 1, r.height() - mWidth1 - mWidth2);
    }
    return QRect();
}

QRect RoofObject::southGap(RoofHeight height)
{
    if (height != roofHeight())
        return QRect();
    QRect r = bounds();
    if (isN() && mCapped2) {
        return QRect(r.left() + mWidth1, r.bottom()+1, r.width() - mWidth1 - mWidth2, 1);
    }
    if (isW() && !mWidth2) {
        return QRect(r.left(), r.bottom()+1, r.width(), 1);
    }
    return QRect();
}

QRect RoofObject::flatTop()
{
    QRect r = bounds();
    if (isW())
        return QRect(r.left(), r.top() + width1(),
                     r.width(), gap());
    if (isN())
        return QRect(r.left() + width1(), r.top(),
                     gap(), r.height());
    return QRect();
}

/////

RoofCornerObject::RoofCornerObject(BuildingFloor *floor, int x, int y,
                                   int width, int height, int depth,
                                   Orient orient) :
    BuildingObject(floor, x, y, BuildingObject::Invalid),
    mWidth(width),
    mHeight(height),
    mDepth(depth),
    mOrient(orient)
{
}

QRect RoofCornerObject::bounds() const
{
    return QRect(mX, mY, mWidth, mHeight);
}

void RoofCornerObject::rotate(bool right)
{
    int oldFloorWidth = mFloor->height();
    int oldFloorHeight = mFloor->width();

    qSwap(mWidth, mHeight);

    if (right) {
        int x = mX;
        mX = oldFloorHeight - mY - mWidth;
        mY = x;

        if (isSW()) mOrient = NW;
        else if (isNW()) mOrient = NE;
        else if (isNE()) mOrient = SE;
        else if (isSE()) mOrient = SW;
    } else {
        int x = mX;
        mX = mY;
        mY = oldFloorWidth - x - mHeight;

        if (isSW()) mOrient = SE;
        else if (isSE()) mOrient = NE;
        else if (isNE()) mOrient = NW;
        else if (isNW()) mOrient = SW;
    }
}

void RoofCornerObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - mWidth;
        if (isSW()) mOrient = SE;
        else if (isNW()) mOrient = NE;
        else if (isNE()) mOrient = NW;
        else if (isSE()) mOrient = SW;
    } else {
        mY = mFloor->height() - mY - mHeight;
        if (isSW()) mOrient = NW;
        else if (isNW()) mOrient = SW;
        else if (isNE()) mOrient = SE;
        else if (isSE()) mOrient = NE;
    }
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

void RoofCornerObject::toggleOrient(bool right)
{
    if (right) {
        // Rotate clockwise
        if (isSW()) mOrient = NW;
        else if (isNW()) mOrient = NE;
        else if (isNE()) mOrient = SE;
        else if (isSE()) mOrient = SW;
    } else {
        // Rotate counter-clockwise
        if (isSW()) mOrient = SE;
        else if (isSE()) mOrient = NE;
        else if (isNE()) mOrient = NW;
        else if (isNW()) mOrient = SW;
    }
}

QString RoofCornerObject::orientToString(RoofCornerObject::Orient orient)
{
    if (orient == SW) return QLatin1String("SW");
    if (orient == NW) return QLatin1String("NW");
    if (orient == NE) return QLatin1String("NE");
    if (orient == SE) return QLatin1String("SE");
    return QString();
}

RoofCornerObject::Orient RoofCornerObject::orientFromString(const QString &s)
{
    if (s == QLatin1String("SW")) return SW;
    if (s == QLatin1String("NW")) return NW;
    if (s == QLatin1String("NE")) return NE;
    if (s == QLatin1String("SE")) return SE;
    return Invalid;
}

BuildingTile *RoofCornerObject::roofTile(RoofCornerObject::RoofTile tile) const
{
    BuildingTile *btile = mTile;
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

    case HalfFlatS: index = 15; break;
    case HalfFlatE: index = 14; break;

    case FlatTopW: index = 22; break; // not even sure about these
    case FlatTopN: index = 23; break;
    }

    return BuildingTiles::instance()->getFurnitureTile(
                BuildingTiles::instance()->nameForTile(btile->mTilesetName,
                                                       btile->mIndex + index));
}

QRect RoofCornerObject::corners()
{
    QRect r = bounds();
    if (isNW())
        return QRect(r.right() - depth(), r.bottom() - depth(),
                     depth(), depth());
    if (isSE())
        return QRect(r.left(), r.top(), depth(), depth());
    return QRect();
}

QRect RoofCornerObject::southEdge(int &dx1, int &dx2)
{
    QRect r = bounds();
    dx1 = dx2 = 0;
    if (isSW())
        return QRect(r.left() + depth(), r.bottom() - depth() + 1,
                     r.width() - depth(), depth());
    if (isNW()) {
        dx1 = 1;
        return QRect(r.right() - depth() + 1, r.bottom() - depth() + 1,
                     depth(), depth());
    }
    if (isNE())
        return QRect(r.left(), r.bottom() - depth() + 1,
                     r.width() - depth(), depth());
    if (isSE()) {
        dx2 = 1;
        return QRect(r.left(), r.bottom() - depth() + 1,
                     r.width(), depth());
    }
    return QRect();
}

QRect RoofCornerObject::eastEdge(int &dy1, int &dy2)
{
    QRect r = bounds();
    dy1 = dy2 = 0;
    if (isSW())
        return QRect(r.right() - depth() + 1, r.top(),
                     depth(), r.height() - depth() - (midTile() ? 1 : 0));
    if (isNW()) {
        dy1 = 1;
        return QRect(r.right() - depth() + 1, r.bottom() - depth() + 1,
                     depth(), depth());
    }
    if (isNE())
        return QRect(r.right() - depth() + 1, r.top() + depth(),
                     depth(), r.height() - depth());
    if (isSE()) {
        dy2 = 1;
        return QRect(r.right() - depth() + 1, r.top(),
                     depth(), r.height());
    }
    return QRect();
}

QRegion RoofCornerObject::flatTop()
{
    QRect r = bounds();
    if (isSW())
        return QRegion(r.adjusted(depth(), 0, -depth(), -depth()))
                | QRegion(r.adjusted(depth(), depth(), 0, -depth()));
    if (isNW())
        return QRegion(r.adjusted(depth(), depth(), 0, -depth()))
                | QRegion(r.adjusted(depth(), depth(), -depth(), 0));
    if (isNE())
        return QRegion(r.adjusted(0, depth(), -depth(), -depth()))
                | QRegion(r.adjusted(depth(), depth(), -depth(), 0));
    if (isSE())
        return QRegion(r.adjusted(depth(), 0, -depth(), -depth()))
                | QRegion(r.adjusted(0, depth(), -depth(), -depth()));
    return QRect();
}

/////
