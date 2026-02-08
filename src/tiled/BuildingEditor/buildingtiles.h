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

#ifndef BUILDINGTILES_H
#define BUILDINGTILES_H

#include <QImage>
#include <QMap>
#include <QObject>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Tiled {
class Tile;
class Tileset;
}

namespace BuildingEditor {

class BuildingTileCategory;

class BuildingTile
{
public:
    BuildingTile(const QString &tilesetName, int index) :
        mTilesetName(tilesetName),
        mIndex(index)
    {}
    virtual ~BuildingTile() {}

    virtual bool isNone() const
    { return false; }

    virtual QString name() const;

    QString mTilesetName;
    int mIndex;
};

class NoneBuildingTile : public BuildingTile
{
public:
    NoneBuildingTile() :
        BuildingTile(QString(), 0)
    {}

    virtual bool isNone() const
    { return true; }

    virtual QString name() const
    { return QString(); }
};

class BuildingTileEntry
{
public:
    BuildingTileEntry(BuildingTileCategory *category);
    virtual ~BuildingTileEntry() {}

    void set(const BuildingTileEntry *other);
    BuildingTileEntry *createCopy(BuildingTileCategory *category) const;

    BuildingTileCategory *category() const
    { return mCategory; }

    BuildingTile *displayTile() const;

    void setTile(int e, BuildingTile *btile);
    BuildingTile *tile(int n) const;

    int tileCount() const
    { return mTiles.size(); }

    /* NOTE about these offsets.  Some roof tiles must be placed at an x,y
      offset from their actual position in order be displayed in the expected
      *visual* position.  Ideally, every roof tile would be created so that
      it doesn't need to be offset from its actual x,y position in the map.
      */
    void setOffset(int e, const QPoint &offset);
    QPoint offset(int e) const;

    bool usesTile(BuildingTile *btile) const;

    // Check for the universal null entry
    virtual bool isNone() const { return false; }
    virtual BuildingTileEntry *asNone() { return 0; }

    bool equals(BuildingTileEntry *other) const;
    bool equals(BuildingTileEntry *other, const QVector<int> &enums) const;
    bool equalsIgnoreCategory(BuildingTileEntry *other, const QVector<int> &enums) const;

    bool isNorth(int e) const;
    bool isWest(int e) const;

    int wallEnum(int e) const;

    BuildingTileEntry *asCategory(int n);
    BuildingTileEntry *asExteriorWall();
    BuildingTileEntry *asInteriorWall();
    BuildingTileEntry *asExteriorWallTrim();
    BuildingTileEntry *asInteriorWallTrim();
    BuildingTileEntry *asFloor();
    BuildingTileEntry *asDoor();
    BuildingTileEntry *asDoorFrame();
    BuildingTileEntry *asWindow();
    BuildingTileEntry *asCurtains();
    BuildingTileEntry *asShutters();
    BuildingTileEntry *asStairs();
    BuildingTileEntry *asCeiling();
    BuildingTileEntry *asRoofCap();
    BuildingTileEntry *asRoofSlope();
    BuildingTileEntry *asRoofTop();

    BuildingTileCategory *mCategory;
    QVector<BuildingTile*> mTiles;
    QVector<QPoint> mOffsets;
};

struct BuildingTileEntrySpec
{
    BuildingTileEntry *entry;
    int e; // BuildingTileCategory::TileEnum
};

class NoneBuildingTileEntry : public BuildingTileEntry
{
public:
    NoneBuildingTileEntry(BuildingTileCategory *category) :
        BuildingTileEntry(category)
    {}
    BuildingTileEntry *asNone() { return this; }
    bool isNone() const { return true; }
};

class BuildingTileCategory
{
public:
    enum TileEnum
    {
        Invalid = -1
    };

    BuildingTileCategory(const QString &name, const QString &label,
                         int displayIndex);
    virtual ~BuildingTileCategory() {}

    QString name() const
    { return mName; }

    void setLabel(const QString &label)
    { mLabel = label; }

    QString label() const
    { return mLabel; }

    int displayIndex() const
    { return mDisplayIndex; }

    void setDefaultEntry(BuildingTileEntry *entry)
    { mDefaultEntry = entry; }

    BuildingTileEntry *defaultEntry() const
    { return mDefaultEntry; }

    const QList<BuildingTileEntry*> &entries() const
    { return mEntries; }

    BuildingTileEntry *entry(int index) const;

    int entryCount() const
    { return mEntries.size(); }

    int indexOf(BuildingTileEntry *entry) const
    { return mEntries.indexOf(entry); }

    void insertEntry(int index, BuildingTileEntry *entry);
    BuildingTileEntry *removeEntry(int index);

    BuildingTileEntry *noneTileEntry();

    int enumCount() const
    { return mEnumNames.size(); }

    QString enumToString(int index) const;
    int enumFromString(const QString &s) const;

    void setShadowImage(const QImage &image)
    { mShadowImage = image; }

    QImage shadowImage() const
    { return mShadowImage; }

    virtual int shadowColumns() const;
    virtual int shadowRows() const;
    virtual int shadowCount() const { return shadowColumns() * shadowRows(); }
    virtual int shadowToEnum(int shadowIndex) { return shadowIndex; }
    virtual int enumToShadow(int e);

    /*
     * This is the method used to fill in all the tiles in an entry from a single
     * tile. For example, given a door tile, all the different door tiles for each
     * of the enumeration values (west, north, southeast, etc) used by the doors
     * category are assigned to a new entry.
     */
    virtual BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileEntry *findMatch(BuildingTileEntry *entry) const;
    BuildingTileEntry *findMatchForVersion(BuildingTileEntry *entry, int buildingTilesFileVersion) const;
    virtual QVector<int> enumsForVersion(int buildingTilesFileVersion) const;
    BuildingTileEntry *findMatchIgnoreCategory(BuildingTileEntry *entry, int buildingTilesFileVersion) const;
    bool usesTile(Tiled::Tile *tile) const;

    virtual bool canAssignNone() const
    { return false; }

    virtual bool isNone() const { return false; }

    virtual bool isNorth(int e) const { Q_UNUSED(e) return false; }
    virtual bool isWest(int e) const { Q_UNUSED(e) return false; }

    // Return BTC_Walls::TileEnum for this subclass's TileEnum.
    // This is used to determine which wall tile to use for a given window shape.
    virtual int wallEnum(const BuildingTileEntry *entry, int e) const { Q_UNUSED(entry) Q_UNUSED(e) return TileEnum::Invalid; }

    virtual BuildingTileCategory *asNone() { return 0; }
    virtual BuildingTileCategory *asExteriorWalls() { return 0; }
    virtual BuildingTileCategory *asInteriorWalls() { return 0; }
    virtual BuildingTileCategory *asExteriorWallTrim() { return 0; }
    virtual BuildingTileCategory *asInteriorWallTrim() { return 0; }
    virtual BuildingTileCategory *asFloors() { return 0; }
    virtual BuildingTileCategory *asDoors() { return 0; }
    virtual BuildingTileCategory *asDoorFrames() { return 0; }
    virtual BuildingTileCategory *asWindows() { return 0; }
    virtual BuildingTileCategory *asCurtains() { return 0; }
    virtual BuildingTileCategory *asShutters() { return 0; }
    virtual BuildingTileCategory *asStairs() { return 0; }
    virtual BuildingTileCategory *asGrimeFloor() { return 0; }
    virtual BuildingTileCategory *asGrimeWall() { return 0; }
    virtual BuildingTileCategory *asCeiling() { return 0; }
    virtual BuildingTileCategory *asRoofCaps() { return 0; }
    virtual BuildingTileCategory *asRoofSlopes() { return 0; }
    virtual BuildingTileCategory *asRoofTops() { return 0; }

protected:
    virtual BuildingTile *getTile(const QString &tilesetName, int offset = 0);

protected:
    QString mName;
    QString mLabel;
    int mDisplayIndex;
    QList<BuildingTileEntry*> mEntries;
    QStringList mEnumNames;
    BuildingTileEntry *mDefaultEntry;
    NoneBuildingTileEntry *mNoneTileEntry;
    QImage mShadowImage;
};

class NoneBuildingTileCategory : public BuildingTileCategory
{
public:
    NoneBuildingTileCategory()
        : BuildingTileCategory(QString(), QString(), 0)
    {}

    bool isNone() const { return true; }
    BuildingTileCategory *asNone() { return this; }
};

class BTC_Curtains : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        East,
        North,
        South,
        // Could add separate open/closed, but I want the user to be able to
        // choose opened/closed from the browser
        EnumCount
    };

    BTC_Curtains(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    virtual BuildingTileCategory *asCurtains() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Shutters : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        WestBelow,
        WestAbove,
        NorthLeft,
        NorthRight,
        EnumCount
    };

    BTC_Shutters(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    virtual BuildingTileCategory *asShutters() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Doors : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        WestOpen,
        NorthOpen,
        EnumCount
    };

    BTC_Doors(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName) override;

    bool canAssignNone() const override
    { return true; }

    BuildingTileCategory *asDoors() override { return this; }

    int shadowToEnum(int shadowIndex) override;

    bool isNorth(int e) const override;
    bool isWest(int e) const override;
    int wallEnum(const BuildingTileEntry *entry, int e) const override;
};

class BTC_DoorFrames : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        EnumCount
    };

    BTC_DoorFrames(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asDoorFrames() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Floors : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        Floor,
        EnumCount
    };

    BTC_Floors(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asFloors() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Stairs : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West1,
        West2,
        West3,
        North1,
        North2,
        North3,
        EnumCount
    };

    BTC_Stairs(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    BuildingTileCategory *asStairs() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_Walls : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,

        // NOTE: Code assumes this order.
        WestWindow1,
        NorthWindow1,
        WestWindow2,
        NorthWindow2,
        WestWindow3,
        NorthWindow3,
        WestWindow4,
        NorthWindow4,

        WestWindow5,
        NorthWindow5,
        WestWindow6,
        NorthWindow6,
        WestWindow7,
        NorthWindow7,
        WestWindow8,
        NorthWindow8,

        WestWindow9,
        NorthWindow9,
        WestWindow10,
        NorthWindow10,
        WestWindow11,
        NorthWindow11,
        WestWindow12,
        NorthWindow12,

        WestWindow13,
        NorthWindow13,
        WestWindow14,
        NorthWindow14,
        WestWindow15,
        NorthWindow15,
        WestWindow16,
        NorthWindow16,

        WestWindow17, // 2-part trailer type - left
        NorthWindow17,
        WestWindow18, // 2-part trailer type - right
        NorthWindow18,

        WestWindow19, // Tall skinny round top
        NorthWindow19,

        EnumCount
    };

    static const int NUM_WINDOW_FRAMES = 19;

    BTC_Walls(const QString &name, const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName) override;

    int shadowToEnum(int shadowIndex) override;

    bool isNorth(int e) const override;
    bool isWest(int e) const override;

    static TileEnum windowShapeToEnumW(const QString &windowShape);
    static TileEnum windowShapeToEnumN(const QString &windowShape);
};

class BTC_EWalls : public BTC_Walls
{
public:
    BTC_EWalls(const QString &label) :
        BTC_Walls(QLatin1String("exterior_walls"), label)
    {}

    bool canAssignNone() const override
    { return true; }

    BuildingTileCategory *asExteriorWalls() override { return this; }

    QVector<int> enumsForVersion(int buildingTilesFileVersion) const override;
};

class BTC_IWalls : public BTC_Walls
{
public:
    BTC_IWalls(const QString &label) :
        BTC_Walls(QLatin1String("interior_walls"), label)
    {}

    bool canAssignNone() const override
    { return true; }

    BuildingTileCategory *asInteriorWalls() override { return this; }

    QVector<int> enumsForVersion(int buildingTilesFileVersion) const override;
};

class BTC_EWallTrim : public BTC_Walls
{
public:
    BTC_EWallTrim(const QString &label) :
        BTC_Walls(QLatin1String("exterior_wall_trim"), label)
    {}

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asExteriorWallTrim() { return this; }
};

class BTC_IWallTrim : public BTC_Walls
{
public:
    BTC_IWallTrim(const QString &label) :
        BTC_Walls(QLatin1String("interior_wall_trim"), label)
    {}

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asInteriorWallTrim() { return this; }
};

class BTC_Windows : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        // TODO: open/broken
        EnumCount
    };

    BTC_Windows(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName) override;

    bool canAssignNone() const override
    { return true; }

    BuildingTileCategory *asWindows() override { return this; }

    int shadowColumns() const override;
    int shadowRows() const override;
    int shadowToEnum(int shadowIndex) override;

    bool isNorth(int e) const override;
    bool isWest(int e) const override;
    int wallEnum(const BuildingTileEntry *entry, int e) const override;

    bool shadowHack(const BuildingTileEntry *entry, int e, QPoint &p) const;

private:
    int defaultWallEnum(const BuildingTileEntry *entry, int e) const;
};

class BTC_Ceiling : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        Ceiling,
        EnumCount
    };

    BTC_Ceiling(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asCeiling() { return this; }

    int shadowToEnum(int shadowIndex);
};

class BTC_RoofCaps : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
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

        EnumCount
    };

    BTC_RoofCaps(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName) override;

    BuildingTileCategory *asRoofCaps() override { return this; }

    int shadowCount() const override { return EnumCount + 4; }
    int shadowToEnum(int shadowIndex) override;

    QVector<int> enumsForVersion(int buildingTilesFileVersion) const override;
};

class BTC_RoofSlopes : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        // Sloped sides
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

        // Sloped corners
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

        EnumCount
    };

    BTC_RoofSlopes(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName) override;

    BuildingTileCategory *asRoofSlopes() override { return this; }

    int shadowCount() const override { return EnumCount + 4; }
    int shadowToEnum(int shadowIndex) override;

    QVector<int> enumsForVersion(int buildingTilesFileVersion) const override;
};

class BTC_RoofTops : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West1,
        West2,
        West3,
        North1,
        North2,
        North3,
        EnumCount
    };

    BTC_RoofTops(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asRoofTops() { return this; }

    int shadowCount() const { return 3; }
    int shadowToEnum(int shadowIndex);
};

class BTC_GrimeFloor : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        East,
        South,
        SouthWest,
        NorthWest,
        NorthEast,
        SouthEast,
        EnumCount
    };

    BTC_GrimeFloor(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asGrimeFloor() { return this; }

    int shadowCount() const { return 8; }
    int shadowToEnum(int shadowIndex);
};


class BTC_GrimeWall : public BuildingTileCategory
{
public:
    enum TileEnum
    {
        West,
        North,
        NorthWest,
        SouthEast,
        WestWindow,
        NorthWindow,
        WestDoor,
        NorthDoor,
        WestTrim,
        NorthTrim,
        NorthWestTrim,
        SouthEastTrim,
        WestDoubleLeft,
        WestDoubleRight,
        NorthDoubleLeft,
        NorthDoubleRight,
        EnumCount
    };

    BTC_GrimeWall(const QString &label);

    BuildingTileEntry *createEntryFromSingleTile(const QString &tileName);

    bool canAssignNone() const
    { return true; }

    BuildingTileCategory *asGrimeWall() { return this; }

    int shadowCount() const { return 16; }
    int shadowToEnum(int shadowIndex);
};

class BuildingTilesMgr : public QObject
{
    Q_OBJECT
public:
    enum CategoryEnum {
        ExteriorWalls,
        InteriorWalls,
        ExteriorWallTrim,
        InteriorWallTrim,
        Floors,
        Doors,
        DoorFrames,
        Windows,
        Curtains,
        Shutters,
        Stairs,
        GrimeFloor,
        GrimeWall,
        RoofCaps,
        RoofSlopes,
        RoofTops,
        Ceiling,
        Count
    };

    static BuildingTilesMgr *instance();
    static void deleteInstance();

    BuildingTilesMgr();
    ~BuildingTilesMgr();

    BuildingTile *add(const QString &tileName);

    BuildingTile *get(const QString &tileName, int offset = 0);

    static void createCategories(QVector<BuildingTileCategory *> &categories);

    const QList<BuildingTileCategory*> &categories() const
    { return mCategories; }

    int categoryCount() const
    { return mCategories.count(); }

    BuildingTileCategory *category(int index) const
    {
        if (index < 0 || index >= mCategories.count())
            return 0;
        return mCategories[index];
    }

    BuildingTileCategory *category(const QString &name) const
    {
        if (mCategoryByName.contains(name))
            return mCategoryByName[name];
        return 0;
    }

    int indexOf(BuildingTileCategory *category) const
    { return mCategories.indexOf(category); }

    int indexOf(const QString &name) const
    {
        if (BuildingTileCategory *category = this->category(name))
            return mCategories.indexOf(category);
        return -1;
    }

    static QString nameForTile(const QString &tilesetName, int index);
    static QString nameForTile2(const QString &tilesetName, int index);
    static QString nameForTile(Tiled::Tile *tile);
    static bool parseTileName(const QString &tileName, QString &tilesetName, int &index);
    static bool legalTileName(const QString &tileName);
    static QString adjustTileNameIndex(const QString &tileName, int offset);
    static QString normalizeTileName(const QString &tileName);

    Tiled::Tile *tileFor(const QString &tileName);
    Tiled::Tile *tileFor(BuildingTile *tile, int offset = 0);

    BuildingTile *fromTiledTile(Tiled::Tile *tile);

    BuildingTile *noneTile() const
    { return mNoneBuildingTile; }

    BuildingTileEntry *noneTileEntry() const
    { return mNoneTileEntry; }

    Tiled::Tile *noneTiledTile() const
    { return mNoneTiledTile; }

    void entryTileChanged(BuildingTileEntry *entry, int e);

    QString txtName();
    QString txtPath();

    bool readTxt();
    void writeTxt(QWidget *parent = 0);

    int setRevision(int revision);
    int setSourceRevision(int sourceRevision);

    int revision() const
    { return mRevision; }

    int sourceRevision() const
    { return mSourceRevision; }

    BuildingTileCategory *catEWalls() const { return mCatEWalls; }
    BuildingTileCategory *catIWalls() const { return mCatIWalls; }
    BuildingTileCategory *catEWallTrim() const { return mCatEWallTrim; }
    BuildingTileCategory *catIWallTrim() const { return mCatIWallTrim; }
    BuildingTileCategory *catFloors() const { return mCatFloors; }
    BuildingTileCategory *catDoors() const { return mCatDoors; }
    BuildingTileCategory *catDoorFrames() const { return mCatDoorFrames; }
    BuildingTileCategory *catWindows() const { return mCatWindows; }
    BuildingTileCategory *catCurtains() const { return mCatCurtains; }
    BuildingTileCategory *catStairs() const { return mCatStairs; }
    BuildingTileCategory *catCeiling() const { return mCatCeiling; }
    BuildingTileCategory *catRoofCaps() const { return mCatRoofCaps; }
    BuildingTileCategory *catRoofSlopes() const { return mCatRoofSlopes; }
    BuildingTileCategory *catRoofTops() const { return mCatRoofTops; }
    BuildingTileCategory *catGrimeFloor() const { return mCatGrimeFloor; }
    BuildingTileCategory *catGrimeWall() const { return mCatGrimeWall; }

    BuildingTileEntry *defaultCategoryTile(int e) const;

    BuildingTileEntry *defaultExteriorWall() const;
    BuildingTileEntry *defaultInteriorWall() const;
    BuildingTileEntry *defaultExteriorWallTrim() const;
    BuildingTileEntry *defaultInteriorWallTrim() const;
    BuildingTileEntry *defaultFloorTile() const;
    BuildingTileEntry *defaultDoorTile() const;
    BuildingTileEntry *defaultDoorFrameTile() const;
    BuildingTileEntry *defaultWindowTile() const;
    BuildingTileEntry *defaultCurtainsTile() const;
    BuildingTileEntry *defaultStairsTile() const;
    BuildingTileEntry *defaultCeilingTile() const;
    BuildingTileEntry *defaultRoofCapTiles() const;
    BuildingTileEntry *defaultRoofSlopeTiles() const;
    BuildingTileEntry *defaultRoofTopTiles() const;

    QString errorString() const
    { return mError; }

private:
    bool mergeTxt();

signals:
    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetRemoved(Tiled::Tileset *tileset);

    void entryTileChanged(BuildingEditor::BuildingTileEntry *entry);

private:
    static BuildingTilesMgr *mInstance;

    QList<BuildingTileCategory*> mCategories;
    QMap<QString,BuildingTileCategory*> mCategoryByName;

    QList<BuildingTile*> mTiles;
    QMap<QString,BuildingTile*> mTileByName;

    Tiled::Tile *mMissingTile;
    Tiled::Tile *mNoneTiledTile;
    BuildingTile *mNoneBuildingTile;

    BuildingTileCategory *mNoneCategory;
    BuildingTileEntry *mNoneTileEntry;

    int mRevision;
    int mSourceRevision;

    QString mError;

    // The categories
    BTC_Curtains *mCatCurtains;
    BTC_Doors *mCatDoors;
    BTC_DoorFrames *mCatDoorFrames;
    BTC_Floors *mCatFloors;
    BTC_Stairs *mCatStairs;
    BTC_EWalls *mCatEWalls;
    BTC_IWalls *mCatIWalls;
    BTC_EWallTrim *mCatEWallTrim;
    BTC_IWallTrim *mCatIWallTrim;
    BTC_Shutters *mCatShutters;
    BTC_Windows *mCatWindows;
    BTC_GrimeFloor *mCatGrimeFloor;
    BTC_GrimeWall *mCatGrimeWall;
    BTC_Ceiling *mCatCeiling;
    BTC_RoofCaps *mCatRoofCaps;
    BTC_RoofSlopes *mCatRoofSlopes;
    BTC_RoofTops *mCatRoofTops;
};

} // namespace BuildingEditor

#endif // BUILDINGTILES_H
