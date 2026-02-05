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

#include <QPolygon>
#include <QRegion>
#include <QSet>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;
class BuildingTileEntry;
class Door;
class FurnitureObject;
class FurnitureTile;
class RoofObject;
class RoofTile;
class Stairs;
class WallObject;
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
    virtual ~BuildingObject() {}

    void setFloor(BuildingFloor *floor)
    { mFloor = floor; }

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

    virtual void setTile(BuildingTileEntry *tile, int alternate = 0)
    {
        Q_UNUSED(alternate)
        mTile = tile;
    }

    virtual BuildingTileEntry *tile(int alternate = 0) const
    {
        Q_UNUSED(alternate)
        return mTile;
    }

    virtual QList<BuildingTileEntry*> tiles() const
    {
        QList<BuildingTileEntry*> ret;
        ret += mTile;
        return ret;
    }

    virtual QSet<BuildingTile*> buildingTiles() const;

    virtual bool isValidPos(const QPoint &offset = QPoint(),
                            BuildingEditor::BuildingFloor *floor = 0) const;

    virtual void rotate(bool right);
    virtual void flip(bool horizontal);

    virtual bool affectsFloorAbove() const { return false; }

    virtual BuildingObject *clone() const = 0;
    virtual bool sameAs(BuildingObject *other) = 0;
    virtual bool sameTilesAs(BuildingObject *other);

    virtual QPolygonF calcShape() const = 0;

    virtual Door *asDoor() { return 0; }
    virtual Window *asWindow() { return 0; }
    virtual Stairs *asStairs() { return 0; }
    virtual FurnitureObject *asFurniture() { return 0; }
    virtual RoofObject *asRoof() { return 0; }
    virtual WallObject *asWall() { return 0; }

protected:
    BuildingFloor *mFloor;
    int mX;
    int mY;
    Direction mDir;
    BuildingTileEntry *mTile;
};

// Not an object
class BasementAccess
{
public:
    int mX = 0;
    int mY = 0;
    BuildingObject::Direction mDirection = BuildingObject::Direction::Invalid;

    bool fromString(const QString& str);
    QString toString() const;
    QString dirString() const;

    bool isValid() const
    {
        return mDirection != BuildingObject::Direction::Invalid;
    }
};

class Door : public BuildingObject
{
public:
    Door(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mFrameTile(0)
    {

    }

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    Door *asDoor() { return this; }

    int getOffset()
    { return (mDir == N) ? 1 : 0; }

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    { alternate ? mFrameTile = tile : mTile = tile; }

    BuildingTileEntry *tile(int alternate = 0) const
    { return alternate ? mFrameTile : mTile; }

    virtual QList<BuildingTileEntry*> tiles() const
    {
        QList<BuildingTileEntry*> ret;
        ret << mTile << mFrameTile;
        return ret;
    }

    BuildingTileEntry *frameTile() const
    { return mFrameTile; }
private:
    BuildingTileEntry *mFrameTile;
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

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    Stairs *asStairs() { return this; }

    int getOffset(int x, int y);
};

class WallObject : public BuildingObject
{
public:
    WallObject(BuildingFloor *floor, int x, int y, Direction dir, int length);

    QRect bounds() const;

    enum {
        TileExterior,
        TileInterior,
        TileExteriorTrim,
        TileInteriorTrim,
        TileCount
    };

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    {
        if (alternate == TileInterior) mInteriorTile = tile;
        else if (alternate == TileExteriorTrim) mExteriorTrimTile = tile;
        else if (alternate == TileInteriorTrim) mInteriorTrimTile = tile;
        else mTile = tile;
    }

    BuildingTileEntry *tile(int alternate = 0) const
    {
        if (alternate == TileInterior) return mInteriorTile;
        if (alternate == TileExteriorTrim) return mExteriorTrimTile;
        if (alternate == TileInteriorTrim) return mInteriorTrimTile;
        return mTile;
    }

    virtual QList<BuildingTileEntry*> tiles() const
    {
        QList<BuildingTileEntry*> ret;
        ret << mTile << mInteriorTile << mExteriorTrimTile << mInteriorTrimTile;
        return ret;
    }

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    WallObject *asWall() { return this; }

    void setLength(int length)
    { mLength = length; }

    int length() const
    { return mLength; }

private:
    int mLength;
    BuildingTileEntry *mExteriorTrimTile;
    BuildingTileEntry *mInteriorTile;
    BuildingTileEntry *mInteriorTrimTile;
};

class Window : public BuildingObject
{
public:
    Window(BuildingFloor *floor, int x, int y, Direction dir) :
        BuildingObject(floor, x, y, dir),
        mCurtainsTile(0),
        mShuttersTile(0)
    {

    }

    enum {
        TileWindow,
        TileCurtains,
        TileShutters,
        TileCount
    };

    void setTile(BuildingTileEntry *tile, int alternate = 0)
    {
        if (alternate == TileCurtains) mCurtainsTile = tile;
        else if (alternate == TileShutters) mShuttersTile = tile;
        else mTile = tile;
    }

    BuildingTileEntry *tile(int alternate = 0) const
    {
        if (alternate == TileCurtains) return mCurtainsTile;
        if (alternate == TileShutters) return mShuttersTile;
        return mTile;
    }

    virtual QList<BuildingTileEntry*> tiles() const
    {
        QList<BuildingTileEntry*> ret;
        ret << mTile << mCurtainsTile << mShuttersTile;
        return ret;
    }

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    Window *asWindow() { return this; }

    int getOffset() const
    { return (mDir == N) ? 1 : 0; }

    BuildingTileEntry *curtainsTile()
    { return mCurtainsTile; }

    BuildingTileEntry *shuttersTile()
    { return mShuttersTile; }

private:
    BuildingTileEntry *mCurtainsTile;
    BuildingTileEntry *mShuttersTile;
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

    virtual QSet<BuildingTile*> buildingTiles() const;

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    FurnitureObject *asFurniture() { return this; }

    void setFurnitureTile(FurnitureTile *tile);

    FurnitureTile *furnitureTile() const
    { return mFurnitureTile; }

    bool inWallLayer() const;

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
        DormerW,
        DormerN,
        DormerE,
        DormerS,
        FlatTop,

        ShallowSlopeW,
        ShallowSlopeN,
        ShallowSlopeE,
        ShallowSlopeS,
        ShallowPeakWE,
        ShallowPeakNS,

        Slope30W,
        Slope30N,
        Slope30E,
        Slope30S,
        Peak30WE,
        Peak30NS,
        Peak30Quad,

        CornerInnerSW,
        CornerInnerNW,
        CornerInnerNE,
        CornerInnerSE,
        CornerOuterSW,
        CornerOuterNW,
        CornerOuterNE,
        CornerOuterSE,

        CornerSlope30InnerSW,
        CornerSlope30InnerNW,
        CornerSlope30InnerNE,
        CornerSlope30InnerSE,
        CornerSlope30OuterSW,
        CornerSlope30OuterNW,
        CornerSlope30OuterNE,
        CornerSlope30OuterSE,

        InvalidType
    };

    enum RoofDepth {
        Zero,
        Point5,
        One,
        OnePoint5,
        Two,
        TwoPoint5,
        Three,
        InvalidDepth
    };

    /* Enum for setTile() and tile() 'alternate' argument. */
    enum RoofObjectTiles {
        TileCap,
        TileSlope,
        TileTop
    };

    RoofObject(BuildingFloor *floor, int x, int y, int width, int height,
               RoofType type, RoofDepth depth,
               bool cappedW, bool cappedN, bool cappedE, bool cappedS);

    QRect bounds() const;

    void rotate(bool right);
    void flip(bool horizontal);

    bool isValidPos(const QPoint &offset = QPoint(),
                    BuildingEditor::BuildingFloor *floor = 0) const;

    void setTile(BuildingTileEntry *tile, int alternate = 0);

    BuildingTileEntry *tile(int alternate = 0) const;

    virtual QList<BuildingTileEntry*> tiles() const
    {
        QList<BuildingTileEntry*> ret;
        ret << mCapTiles << mSlopeTiles << mTopTiles;
        return ret;
    }

    bool affectsFloorAbove() const { return true; }

    BuildingObject *clone() const;
    bool sameAs(BuildingObject *other);

    QPolygonF calcShape() const;

    RoofObject *asRoof() { return this; }

    void setCapTiles(BuildingTileEntry *entry);

    BuildingTileEntry *capTiles() const
    { return mCapTiles; }

    void setSlopeTiles(BuildingTileEntry *entry);

    BuildingTileEntry *slopeTiles() const
    { return mSlopeTiles; }

    void setTopTiles(BuildingTileEntry *entry);

    BuildingTileEntry *topTiles() const
    { return mTopTiles; }

    void setType(RoofType type);

    RoofType roofType() const
    { return mType; }

    bool isCorner() const
    { return (mType >= CornerInnerSW && mType <= CornerOuterSE) || (mType >= CornerSlope30InnerSW && mType <= CornerSlope30OuterSE); }

    void setWidth(int width);

    int width() const
    { return mWidth; }

    void setHeight(int height);

    int height() const
    { return mHeight; }

    void resize(int width, int height, bool halfDepth);

    void depthUp();
    void depthDown();

    bool isDepthMax();
    bool isDepthMin();

    RoofDepth depth() const
    { return mDepth; }

    int actualWidth() const;
    int actualHeight() const;

    int slopeThickness() const;
    void setHalfDepth(bool half) { mHalfDepth = half; }
    bool isHalfDepth() const;

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

    void setDefaultCaps();

    enum RoofTile {
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Shallow sides
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,

        // 30-degree sides
        Slope30S1, Slope30S2, Slope30S3, Slope30S4, Slope30S5, Slope30S6,
        Slope30E1, Slope30E2, Slope30E3, Slope30E4, Slope30E5, Slope30E6,
        Slope30W1, Slope30W2, Slope30W3, Slope30W4, Slope30W5, Slope30W6,
        Slope30N1, Slope30N2, Slope30N3, Slope30N4, Slope30N5, Slope30N6,

        // 30-degree peaks
        Peak30NS1, Peak30NS2, Peak30NS3, Peak30NS4, Peak30NS5, Peak30NS6, // intersection runs west-east
        Peak30WE1, Peak30WE2, Peak30WE3, Peak30WE4, Peak30WE5, Peak30WE6, // intersection runs north-south
        Peak30Quad1, Peak30Quad2, Peak30Quad3, Peak30Quad4, Peak30Quad5, Peak30Quad6,

        // Corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,

        // 30-degree corners
        InnerSlope30SE1, InnerSlope30SE2, InnerSlope30SE3, InnerSlope30SE4, InnerSlope30SE5, InnerSlope30SE6,
        InnerSlope30NE1, InnerSlope30NE2, InnerSlope30NE3, InnerSlope30NE4, InnerSlope30NE5, InnerSlope30NE6,
        InnerSlope30NW1, InnerSlope30NW2, InnerSlope30NW3, InnerSlope30NW4, InnerSlope30NW5, InnerSlope30NW6,
        InnerSlope30SW1, InnerSlope30SW2, InnerSlope30SW3, InnerSlope30SW4, InnerSlope30SW5, InnerSlope30SW6,

        OuterSlope30SE1, OuterSlope30SE2, OuterSlope30SE3, OuterSlope30SE4, OuterSlope30SE5, OuterSlope30SE6,
        OuterSlope30NE1, OuterSlope30NE2, OuterSlope30NE3, OuterSlope30NE4, OuterSlope30NE5, OuterSlope30NE6,
        OuterSlope30NW1, OuterSlope30NW2, OuterSlope30NW3, OuterSlope30NW4, OuterSlope30NW5, OuterSlope30NW6,
        OuterSlope30SW1, OuterSlope30SW2, OuterSlope30SW3, OuterSlope30SW4, OuterSlope30SW5, OuterSlope30SW6,

        // Caps
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        // Cap tiles for shallow (garage, trailer, etc) roofs
        CapShallowRiseS1, CapShallowRiseS2, CapShallowFallS1, CapShallowFallS2,
        CapShallowRiseE1, CapShallowRiseE2, CapShallowFallE1, CapShallowFallE2,

        // Cap tiles for 30-degree roofs
        CapSlope30RiseE1, CapSlope30RiseE2, CapSlope30RiseE3, CapSlope30RiseE4, CapSlope30RiseE5, CapSlope30RiseE6,
        CapSlope30FallE1, CapSlope30FallE2, CapSlope30FallE3, CapSlope30FallE4, CapSlope30FallE5, CapSlope30FallE6,
        CapSlope30RiseS1, CapSlope30RiseS2, CapSlope30RiseS3, CapSlope30RiseS4, CapSlope30RiseS5, CapSlope30RiseS6,
        CapSlope30FallS1, CapSlope30FallS2, CapSlope30FallS3, CapSlope30FallS4, CapSlope30FallS5, CapSlope30FallS6,
        CapPeak30E1, CapPeak30E2, CapPeak30E3, CapPeak30E4, CapPeak30E5, CapPeak30E6,
        CapPeak30S1, CapPeak30S2, CapPeak30S3, CapPeak30S4, CapPeak30S5, CapPeak30S6,

        TileCount
    };

    int getOffset(RoofTile getOffset) const;

    QRect westEdge();
    QRect northEdge();
    QRect eastEdge();
    QRect southEdge();
#if 0
    QRect northCapRise();
    QRect northCapFall();
    QRect southCapRise();
    QRect southCapFall();

    QRect westGap(RoofDepth depth);
    QRect northGap(RoofDepth depth);
    QRect eastGap(RoofDepth depth);
    QRect southGap(RoofDepth depth);
#endif
    QRect flatTop();
#if 0
    QRect shallowWestEdge();
    QRect shallowEastEdge();
    QRect shallowNorthEdge();
    QRect shallowSouthEdge();

    QRect tileRect(RoofTile tile, bool alt);
#endif
    QVector<RoofTile> slopeTiles(QRect &b);

    QVector<RoofTile> westCapTiles(QRect &b);
    QVector<RoofTile> eastCapTiles(QRect &b);
    QVector<RoofTile> northCapTiles(QRect &b);
    QVector<RoofTile> southCapTiles(QRect &b);

    QVector<RoofTile> cornerTiles(QRect &b);
#if 0
    QRect cornerInner(bool &slopeE, bool &slopeS);
    QRect cornerOuter();
#endif
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
    bool mHalfDepth;
    BuildingTileEntry *mCapTiles;
    BuildingTileEntry *mSlopeTiles;
    BuildingTileEntry *mTopTiles;
};

} // namespace BulidingEditor

#endif // BUILDINGOBJECTS_H
