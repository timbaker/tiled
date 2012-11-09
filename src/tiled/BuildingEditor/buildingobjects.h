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
    enum RoofType {
        SlopeW,
        SlopeN,
        SlopeE,
        SlopeS,
        PeakWE,
        PeakNS,
        FlatTop,
        CornerInnerSW,
        CornerInnerNW,
        CornerInnerNE,
        CornerInnerSE,
        CornerOuterSW,
        CornerOuterNW,
        CornerOuterNE,
        CornerOuterSE,
        InvalidType
    };

    enum RoofDepth {
        Point5,
        One,
        OnePoint5,
        Two,
        Three,
        InvalidDepth
    };

    RoofObject(BuildingFloor *floor, int x, int y, int width, int height,
               RoofType type,
               bool cappedW, bool cappedN, bool cappedE, bool cappedS);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    BuildingTile *tile(int alternate) const;

    void setTile(BuildingTile *roofTile, int alternate = 0);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    bool affectsFloorAbove() const { return true; }

    RoofObject *asRoof() { return this; }

    void setType(RoofType type);

    RoofType roofType() const
    { return mType; }

    bool isCorner() const
    { return mType >= CornerInnerSW && mType <= CornerOuterSE; }

    void setWidth(int width);

    int width() const
    { return mWidth; }

    void setHeight(int height);

    int height() const
    { return mHeight; }

    void resize(int width, int height);

    void depthUp();
    void depthDown();

    bool isDepthMax();
    bool isDepthMin();

    RoofDepth depth() const
    { return mDepth; }

    int actualWidth() const;
    int actualHeight() const;

    bool isCappedW() const
    { return mCappedW; }

    bool isCappedN() const
    { return mCappedN; }

    bool isCappedE() const
    { return mCappedE; }

    bool isCappedS() const
    { return mCappedS; }

    void toggleCappedW();
    void toggleCappedN();
    void toggleCappedE();
    void toggleCappedS();

    enum RoofTile {
        FlatS1, FlatS2, FlatS3,
        FlatE1, FlatE2, FlatE3,
        HalfFlatS, HalfFlatE,
        FlatTopW, FlatTopN,

        // Corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,

        // Caps must come last (see roofTile())
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        CapMidS, CapMidE,
        CapMidPt5S, CapMidPt5E,
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3
    };

    BuildingTile *roofTile(RoofTile roofTile) const;

    BuildingTile *capTile() const
    { return mCapTile; }

    QRect westEdge();
    QRect northEdge();
    QRect eastEdge();
    QRect southEdge();
    QRect eastGap(RoofDepth depth);
    QRect southGap(RoofDepth depth);
    QRect flatTop();

    QString typeToString() const
    { return typeToString(mType); }

    static QString typeToString(RoofType type);
    static RoofType typeFromString(const QString &s);

    QString depthToString() const
    { return depthToString(mDepth); }

    static QString depthToString(RoofDepth depth);
    static RoofDepth depthFromString(const QString &s);


private:
    int mWidth;
    int mHeight;
    RoofType mType;
    RoofDepth mDepth;
    bool mCappedW;
    bool mCappedN;
    bool mCappedE;
    bool mCappedS;
    BuildingTile *mCapTile;
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
