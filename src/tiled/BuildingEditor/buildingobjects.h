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
#include <QRegion>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;
class Door;
class FurnitureObject;
class FurnitureTile;
class RoofCornerObject;
class RoofObject;
class Stairs;
class Window;

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

    virtual void setTile(BuildingTile *tile, int alternate = 0)
    {
        Q_UNUSED(alternate)
        mTile = tile;
    }

    virtual BuildingTile *tile(int alternate = 0) const
    {
        Q_UNUSED(alternate)
        return mTile;
    }

    virtual bool isValidPos(const QPoint &offset = QPoint(),
                            BuildingEditor::BuildingFloor *floor = 0) const;

    virtual void rotate(bool right);
    virtual void flip(bool horizontal);

    virtual bool affectsFloorAbove() const { return false; }

    virtual Door *asDoor() { return 0; }
    virtual Window *asWindow() { return 0; }
    virtual Stairs *asStairs() { return 0; }
    virtual FurnitureObject *asFurniture() { return 0; }
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

    Door *asDoor() { return this; }

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

    bool affectsFloorAbove() const { return true; }

    Stairs *asStairs() { return this; }

    int getOffset(int x, int y);
};

class Window : public BuildingObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir)
    {

    }

    Window *asWindow() { return this; }

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

    FurnitureObject *asFurniture() { return this; }

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
               int thickness, int width1, int width2, bool capped1,
               bool capped2, int depth);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    BuildingTile *tile(int alternate) const;

    void setTile(BuildingTile *roofTile, int alternate = 0);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    bool affectsFloorAbove() const { return true; }

    RoofObject *asRoof() { return this; }

    void setLength(int length)
    { mLength = length; }

    int length() const
    { return mLength; }

    int thickness() const
    { return mThickness; }

    int width1() const { return mWidth1; }
    int width2() const { return mWidth2; }

    int gap() const
    {
        if (midTile())
            return 0;
        return mThickness - mWidth1 - mWidth2;
    }

    bool midTile() const
    { return (mThickness == 3) && (mWidth1 && mWidth2) && (mDepth > 1); }

    void resize(int length, int thickness);

    void toggleWidth1();
    void toggleWidth2();

    void setDepth(int height);

    int depth() const
    { return mDepth; }

    bool isCapped1() const
    { return mCapped1; }

    bool isCapped2() const
    { return mCapped2; }

    void toggleCapped1();
    void toggleCapped2();

    BuildingTile *capTile() const
    { return mCapTile; }

    enum RoofTile {
        FlatS1, FlatS2, FlatS3,
        FlatE1, FlatE2, FlatE3,
        HalfFlatS, HalfFlatE,
        FlatTopW, FlatTopN,
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        CapMidS, CapMidE,
        CapMidPt5S, CapMidPt5E,
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3
    };

    BuildingTile *roofTile(RoofTile roofTile) const;

    QRect southEdge();
    QRect eastEdge();
    QRect flatTop();

private:
    int mLength;
    int mThickness;
    int mWidth1; // Thickness above (mDir=W) or left of (mDir=N) the gap
    int mWidth2; // Thickness below (mDir=W) or right of (mDir=N) the gap
    int mDepth;
    bool mCapped1; // Capped on left (mDir=W) or top (mDir=N)
    bool mCapped2; // Capped on right (mDir=W) or bottom (mDir=N)
    BuildingTile *mCapTile;
};

class RoofCornerObject : public BuildingObject
{
public:
    enum Orient {
        SW,
        NW,
        NE,
        SE,
        Invalid
    };

    RoofCornerObject(BuildingFloor *floor, int x, int y, int width, int height,
                     int depth, Orient orient);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    bool affectsFloorAbove() const { return true; }

    RoofCornerObject *asRoofCorner() { return this; }

    int width() const { return mWidth; }
    int height() const { return mHeight; }
    int depth() const { return mDepth; }

    bool midTile() const
    { return (mWidth==3 || mHeight==3) && (mDepth == 1); }

    void setWidth(int width) { resize(width, mHeight); }
    void setHeight(int height) { resize(mWidth, height); }
    void setDepth(int depth);

    void resize(int width, int height);

    Orient orient() const
    { return mOrient; }

    void toggleOrient(bool right);

    QString orientToString() { return orientToString(mOrient); }
    static QString orientToString(Orient orient);
    static Orient orientFromString(const QString &s);

    bool isSW() { return mOrient == SW; }
    bool isNW() { return mOrient == NW; }
    bool isNE() { return mOrient == NE; }
    bool isSE() { return mOrient == SE; }

    enum RoofTile {
        FlatS1, FlatS2, FlatS3,
        FlatE1, FlatE2, FlatE3,
        Outer1, Outer2, Outer3,
        Inner1, Inner2, Inner3,
        HalfFlatS, HalfFlatE,
        FlatTopW, FlatTopN
    };
    BuildingTile *roofTile(RoofTile roofTile) const;

    QRect corners();
    QRect southEdge(int &dx1, int &dx2);
    QRect eastEdge(int &dy1, int &dy2);
    QRegion flatTop();

private:
    int mWidth;
    int mHeight;
    int mDepth;
    Orient mOrient;
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
