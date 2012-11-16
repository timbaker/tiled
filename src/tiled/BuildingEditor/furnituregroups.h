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

    FurnitureTile(FurnitureTiles *ftiles, FurnitureOrientation orient);

    FurnitureTiles *owner() const
    { return mOwner; }

    void clear()
    {
        mTiles.fill(0);
    }

    bool isEmpty() const;

    FurnitureOrientation orient() const
    { return mOrient; }

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

    bool equals(FurnitureTile *other) const;

    void setTile(int n, BuildingTile *btile)
    { mTiles[n] = btile; }

    BuildingTile *tile(int n) const
    { return mTiles[n]; }

    const QVector<BuildingTile*> &tiles() const
    { return mTiles; }

    const QVector<BuildingTile*> &resolvedTiles() const;

    static bool isCornerOrient(FurnitureOrientation orient);

private:
    FurnitureTiles *mOwner;
    FurnitureOrientation mOrient;
    QVector<BuildingTile*> mTiles;
};

class FurnitureTiles
{
public:
    FurnitureTiles(bool corners);
    ~FurnitureTiles();

    bool isEmpty() const;

    bool hasCorners() const
    { return mCorners; }

    void toggleCorners()
    { mCorners = !mCorners; }

    void setTile(FurnitureTile *ftile);
    FurnitureTile *tile(FurnitureTile::FurnitureOrientation orient) const;

    const QVector<FurnitureTile*> &tiles() const
    { return mTiles; }

    bool equals(const FurnitureTiles *other);

private:
    QVector<FurnitureTile*> mTiles;
    bool mCorners;
};

class FurnitureGroup
{
public:
    FurnitureTiles *findMatch(FurnitureTiles *ftiles) const;
    QString mLabel;
    QList<FurnitureTiles*> mTiles;
};

class FurnitureGroups : public QObject
{
    Q_OBJECT
public:
    static FurnitureGroups *instance();
    static void deleteInstance();

    FurnitureGroups();

    void addGroup(FurnitureGroup *group);
    void insertGroup(int index, FurnitureGroup *group);
    FurnitureGroup *removeGroup(int index);
    void removeGroup(FurnitureGroup *group);

    FurnitureTiles *findMatch(FurnitureTiles *other);

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();

    const QList<FurnitureGroup*> groups() const
    { return mGroups; }

    int groupCount() const
    { return mGroups.count(); }

    FurnitureGroup *group(int index) const
    {
        if (index < 0 || index >= mGroups.count())
            return 0;
        return mGroups[index];
    }

    int indexOf(FurnitureGroup *group) const
    { return mGroups.indexOf(group); }

    int indexOf(const QString &name) const;

    QString errorString()
    { return mError; }

    void tileChanged(FurnitureTile *ftile);

    static FurnitureTile::FurnitureOrientation orientFromString(const QString &s);

signals:
    void furnitureTileChanged(FurnitureTile *ftile);

private:
    bool upgradeTxt();

private:
    static FurnitureGroups *mInstance;
    QList<FurnitureGroup*> mGroups;
    QString mError;
};

} // namespace BuildingEditor

#endif // FURNITUREGROUPS_H
