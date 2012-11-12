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

#ifndef ROOFTILES_H
#define ROOFTILES_H

#include <QObject>
#include <QPoint>
#include <QList>
#include <QStringList>
#include <QVector>

namespace BuildingEditor {

class BuildingTile;

class RoofTile
{
public:
    RoofTile();
    RoofTile(BuildingTile *btile) :
        mTile(btile)
    {}
    RoofTile(BuildingTile *btile, const QPoint &offset) :
        mTile(btile),
        mOffset(offset)
    {}

    BuildingTile *tile() const
    { return mTile; }

    /* NOTE about these offsets.  Some roof tiles must be placed at an x,y
      offset from their actual position in order be displayed in the expected
      *visual* position.  Ideally, every roof tile would be created so that
      it doesn't need to be offset from its actual x,y position in the map.
      */
    void setOffset(const QPoint &offset)
    { mOffset = offset; }

    QPoint offset() const
    { return mOffset; }

    bool operator ==(const RoofTile &other);

private:
    BuildingTile *mTile;
    QPoint mOffset;
};

class RoofSlopeTiles
{
public:
    enum TileEnum {
        First,
        // Sloped sides
        SlopeS1 = First, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopePt5E,
        SlopeOnePt5S, SlopeOnePt5E,
        SlopeTwoPt5S, SlopeTwoPt5E,

        // Flat rooftops
        FlatTopW1, FlatTopW2, FlatTopW3,
        FlatTopN1, FlatTopN2, FlatTopN3,

        // Sloped corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,

        NumTiles,

        Invalid
    };

    RoofSlopeTiles();

    const QVector<RoofTile> &roofTiles() const
    { return mTiles; }

    RoofTile roofTile(int tile) const
    { return mTiles[tile]; }

    RoofTile roofTile(TileEnum tile) const
    { return mTiles[tile]; }

    bool equals(RoofSlopeTiles *other) const;

    static QString enumToString(int tile);
    static QString enumToString(TileEnum tile);
    static TileEnum enumFromString(const QString &s);
    static void initNames();

    static QStringList mEnumNames;
    QVector<RoofTile> mTiles;
};

class RoofCapTiles
{
public:
    enum TileEnum {
        First,
        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseE1 = First, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,

        // Cap tiles with peaked tops
        PeakPt5S, PeakPt5E,
        PeakOnePt5S, PeakOnePt5E,
        PeakTwoPt5S, PeakTwoPt5E,

        // Cap tiles with flat tops
        CapGapS1, CapGapS2, CapGapS3,
        CapGapE1, CapGapE2, CapGapE3,

        NumTiles,

        Invalid
    };

    RoofCapTiles();

    const QVector<RoofTile> &roofTiles() const
    { return mTiles; }

    RoofTile roofTile(int tile) const
    { return mTiles[tile]; }

    RoofTile roofTile(TileEnum tile) const
    { return mTiles[tile]; }

    bool equals(RoofCapTiles *other) const;

    static QString enumToString(int tile);
    static QString enumToString(TileEnum tile);
    static TileEnum enumFromString(const QString &s);
    static void initNames();

    static QStringList mEnumNames;
    QVector<RoofTile> mTiles;
};

class RoofTiles : public QObject
{
    Q_OBJECT
public:
    static RoofTiles *instance();
    static void deleteInstance();

    RoofTiles();
    ~RoofTiles();

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();

    QString errorString() const
    { return mError; }

    const QList<RoofCapTiles*> &capTiles() const
    { return mCapTiles; }

    const QList<RoofSlopeTiles*> &slopeTiles() const
    { return mSlopeTiles; }

    RoofCapTiles *findMatch(RoofCapTiles *rtiles) const;
    RoofSlopeTiles *findMatch(RoofSlopeTiles *rtiles) const;

private:
    bool upgradeTxt();

private:
    static RoofTiles *mInstance;
    QList<RoofCapTiles*> mCapTiles;
    QList<RoofSlopeTiles*> mSlopeTiles;
    QString mError;
};

} // namespace BuildingEditor

#endif // ROOFTILES_H
