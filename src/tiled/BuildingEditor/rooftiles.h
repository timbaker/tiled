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
#include <QList>

namespace BuildingEditor {

class BuildingTile;

// The set of cap tiles to use is based on the building's exterior wall tile.
class RoofCapTile {
public:
    BuildingTile *mExteriorWall;
    BuildingTile *mCapTile;
};

class RoofTiles : public QObject
{
    Q_OBJECT
public:
    static RoofTiles *instance();
    static void deleteInstance();

    RoofTiles();

    QString txtName();
    QString txtPath();

    bool readTxt();
    bool writeTxt();

    const QList<RoofCapTile*> &tiles() const
    { return mTiles; }

    BuildingTile *gapTileForCap(BuildingTile *cap);
    BuildingTile *capTileForExteriorWall(BuildingTile *exteriorWall);

    QString errorString() const
    { return mError; }

private:
    bool upgradeTxt();

private:
    static RoofTiles *mInstance;
    QList<RoofCapTile*> mTiles;
    QString mError;
};

} // namespace BuildingEditor

#endif // ROOFTILES_H
