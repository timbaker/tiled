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
#include <QString>

namespace BuildingEditor {

class BuildingTile;
class FurnitureTiles;

class FurnitureTile
{
public:
    enum FurnitureOrientation {
        FurnitureW = 0,
        FurnitureN = 1,
        FurnitureE = 2,
        FurnitureS = 3,
        FurnitureMaxOrient,
        FurnitureUnknown
    };

    FurnitureTile(FurnitureTiles *tiles, FurnitureOrientation orient) :
        mOwner(tiles),
        mOrient(orient)
    {
        mTiles[0] = mTiles[1] = mTiles[2] = mTiles[3] = 0;
    }

    FurnitureTiles *owner() const
    { return mOwner; }

    void clear()
    {
        mTiles[0] = mTiles[1] = mTiles[2] = mTiles[3] = 0;
    }

    bool isEmpty()
    { return !(mTiles[0] || mTiles[1] && mTiles[2] && mTiles[3]); }

    QString orientToString() const
    {
        static const char *s[] = { "W", "N", "E", "S" };
        return QLatin1String(s[mOrient]);
    }

    FurnitureTiles *mOwner;
    FurnitureOrientation mOrient;
    BuildingTile *mTiles[4]; // NW NE SW SE
};

class FurnitureTiles
{
public:
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

    FurnitureTile *mTiles[FurnitureTile::FurnitureMaxOrient];
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

    bool readTxt();
    bool writeTxt();

    const QList<FurnitureGroup*> groups() const
    { return mGroups; }

    QString errorString()
    { return mError; }

    FurnitureTile::FurnitureOrientation orientFromString(const QString &s);

private:
    static FurnitureGroups *mInstance;
    QList<FurnitureGroup*> mGroups;
    QString mError;
};

} // namespace BuildingEditor

#endif // FURNITUREGROUPS_H
