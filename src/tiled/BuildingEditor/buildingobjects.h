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
class FurnitureTile;
class RoofCornerObject;
class RoofObject;

class BuildingObject
{
public:
    enum Direction
    {
        N,
        S,
        E,
        W,
        Invalid
    };

    BuildingObject(BuildingFloor *floor, int x, int y, Direction mDir);

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

    bool isW() const
    { return mDir == W; }

    bool isN() const
    { return mDir == N; }

    QString dirString() const;
    static Direction dirFromString(const QString &s);

    void setTile(BuildingTile *tile)
    { mTile = tile; }

    BuildingTile *tile() const
    { return mTile; }

    virtual bool isValidPos(const QPoint &offset = QPoint(),
                            BuildingEditor::BuildingFloor *floor = 0) const;

    virtual void rotate(bool right);
    virtual void flip(bool horizontal);

    virtual RoofObject *asRoof() { return 0; }
    virtual RoofCornerObject *asRoofCorner() { return 0; }

protected:
    BuildingFloor *mFloor;
    Direction mDir;
    int mX;
    int mY;
    BuildingTile *mTile;
};

class Door : public BuildingObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }

    void setFrameTile(BuildingTile *tile)
    { mFrameTile = tile; }

    BuildingTile *frameTile() const
    { return mFrameTile; }

private:
    BuildingTile *mFrameTile;
};

class Stairs : public BuildingObject
{
public:
    Stairs(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {
    }

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    int getOffset(int x, int y);
};

class Window : public BuildingObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {

    }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }
};

class FurnitureObject : public BuildingObject
{
public:
    FurnitureObject(BuildingFloor *floor, int x, int y);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    void setFurnitureTile(FurnitureTile *tile);

    FurnitureTile *furnitureTile() const
    { return mFurnitureTile; }

private:
    FurnitureTile *mFurnitureTile;
};

class RoofObject : public BuildingObject
{
public:
    RoofObject(BuildingFloor *floor, int x, int y, Direction dir, int length,
               int width1, int width2, int gap);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    RoofObject *asRoof() { return this; }

    void setLength(int length)
    { mLength = length; }

    int length() const
    { return mLength; }

    int thickness() const
    {
        if (mMidTile)
            return 3;
        return mWidth1 + mGap + mWidth2;
    }

    void setWidths(int width1, int width2)
    { mWidth1 = width1, mWidth2 = width2; }

    int width1() const { return mWidth1; }
    int width2() const { return mWidth2; }

    void setGap(int gap)
    { mGap = gap; }

    int gap() const
    { return mGap; }

    void setMidTile(bool midTile)
    { mMidTile = midTile; }

    bool midTile() const
    { return mMidTile; }

    void resize(int length, int thickness);

    void toggleWidth1();
    void toggleWidth2();

    void setHeight(int height);

    int height() const
    { return mHeight; }

    enum RoofTile {
        FlatS1, FlatS2, FlatS3,
        FlatE1, FlatE2, FlatE3,
        HalfFlatS, HalfFlatE,
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        CapMidS, CapMidE,
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3
    };

    BuildingTile *tile(RoofTile tile) const;

private:
    int mLength;
    int mWidth1; // Thickness above (mDir=W) or left of (mDir=N) the gap
    int mWidth2; // Thickness below (mDir=W) or right of (mDir=N) the gap
    int mGap; // Gap between mWidth1 and mWidth2 slopes
    bool mMidTile;
    int mHeight;
};

class RoofCornerObject : public BuildingObject
{
public:
    RoofCornerObject(BuildingFloor *floor, int x, int y, int width, int height,
                     int depth);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    RoofCornerObject *asRoofCorner()
    { return this; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }

    void setWidth(int width) { resize(width, mHeight); }
    void setHeight(int height) { resize(mWidth, height); }
    void setDepth(int depth);

    void resize(int width, int height);

    bool isInner() const { return mInner; }
    void toggleInner();

    enum RoofTile {
        FlatS1, FlatS2, FlatS3,
        FlatE1, FlatE2, FlatE3,
        Outer1, Outer2, Outer3,
        Inner1, Inner2, Inner3
    };
    BuildingTile *tile(RoofTile tile) const;

private:
    int mWidth;
    int mHeight;
    int mDepth;
    bool mInner;
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
