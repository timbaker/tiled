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

#ifndef FURNITUREGROUPS_H
#define FURNITUREGROUPS_H

#include <QCoreApplication>
#include <QList>
#include <QSize>
#include <QString>
#include <QVector>

namespace BuildingEditor {

class BuildingTile;
class FurnitureTiles;

class FurnitureTile
{
public:
    enum FurnitureOrientation {
        FurnitureW,
        FurnitureN,
        FurnitureE,
        FurnitureS,
        FurnitureSW,
        FurnitureNW,
        FurnitureNE,
        FurnitureSE,
        FurnitureUnknown
    };

    FurnitureTile(FurnitureTiles *tiles, FurnitureOrientation orient) :
        mOwner(tiles),
        mOrient(orient),
        mTiles(4, 0)
    {
    }

    FurnitureTiles *owner() const
    { return mOwner; }

    void clear()
    {
        mTiles[0] = mTiles[1] = mTiles[2] = mTiles[3] = 0;
    }

    bool isEmpty() const
    { return !(mTiles[0] || mTiles[1] || mTiles[2] || mTiles[3]); }

    bool isW() const { return mOrient == FurnitureW; }
    bool isN() const { return mOrient == FurnitureN; }
    bool isE() const { return mOrient == FurnitureE; }
    bool isS() const { return mOrient == FurnitureS; }

    bool isSW() const { return mOrient == FurnitureSW; }
    bool isNW() const { return mOrient == FurnitureNW; }
    bool isNE() const { return mOrient == FurnitureNE; }
    bool isSE() const { return mOrient == FurnitureSE; }

    QString orientToString() const
    {
        static const char *s[] = { "W", "N", "E", "S", "SW", "NW", "NE", "SE" };
        return QLatin1String(s[mOrient]);
    }

    QSize size() const;

    const QVector<BuildingTile*> &resolvedTiles() const;

    FurnitureTiles *mOwner;
    FurnitureOrientation mOrient;
    QVector<BuildingTile*> mTiles; // W N E S or SW NW NE SE
};

class FurnitureTiles
{
public:
    FurnitureTiles() :
        mTiles(4, 0)
    {
    }

    ~FurnitureTiles()
    {
        delete mTiles[0];
        delete mTiles[1];
        delete mTiles[2];
        delete mTiles[3];
    }

    bool isEmpty() const
    { return mTiles[0]->isEmpty() && mTiles[1]->isEmpty() &&
                mTiles[2]->isEmpty() && mTiles[3]->isEmpty(); }

    bool isCorners() const;
    void toggleCorners();

    FurnitureTile *tile(FurnitureTile::FurnitureOrientation orient) const;

    static int orientIndex(FurnitureTile::FurnitureOrientation orient);

    QVector<FurnitureTile*> mTiles;
};

class FurnitureGroup
{
public:
    QString mLabel;
    QList<FurnitureTiles*> mTiles;
};

class FurnitureGroups
{
    Q_DECLARE_TR_FUNCTIONS(FurnitureGroups)
public:
    static FurnitureGroups *instance();
    static void deleteInstance();

    FurnitureGroups();

    void addGroup(FurnitureGroup *group);
    void insertGroup(int index, FurnitureGroup *group);
    FurnitureGroup *removeGroup(int index);
    void removeGroup(FurnitureGroup *group);

    bool readTxt();
    bool writeTxt();

    const QList<FurnitureGroup*> groups() const
    { return mGroups; }

    int groupCount() const
    { return mGroups.count(); }

    int indexOf(FurnitureGroup *group) const
    { return mGroups.indexOf(group); }

    QString errorString()
    { return mError; }

    static FurnitureTile::FurnitureOrientation orientFromString(const QString &s);

private:
    static FurnitureGroups *mInstance;
    QList<FurnitureGroup*> mGroups;
    QString mError;
};

} // namespace BuildingEditor

#endif // FURNITUREGROUPS_H
