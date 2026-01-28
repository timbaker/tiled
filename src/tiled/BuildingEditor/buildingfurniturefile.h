/*
 * Copyright 2026, Tim Baker <treectrl@users.sf.net>
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

#ifndef BUILDINGFURNITUREFILE_H
#define BUILDINGFURNITUREFILE_H

#include "furnituregroups.h"

#include <QList>

class SimpleFileBlock;

namespace BuildingEditor {

class FurnitureGroup;
class FurnitureTiles;

class BuildingFurnitureFile
{
public:
    BuildingFurnitureFile();
    ~BuildingFurnitureFile();

    static int getVersionLatest();

    bool read(const QString &fileName);
    bool write(const QString &fileName, const int revision, const int sourceRevision, const QList<FurnitureGroup*> &groups);
    bool write(const QString &fileName);

    int getVersion() const;
    int getRevision() const;
    int getSourceRevision() const;

    QList<FurnitureGroup*> takeGroups();

    const QString &errorString() const;

    static SimpleFileBlock furnitureTilesToSFB(const FurnitureTiles *ftiles);
    static FurnitureTiles *furnitureTilesFromSFB(const SimpleFileBlock &furnitureBlock, QString &error);
    static FurnitureTile::FurnitureOrientation orientFromString(const QString &s);

private:
    static bool booleanFromString(const QString &s, bool &result, QString &error);

private:
    int mVersion;
    int mRevision;
    int mSourceRevision;
    QList<FurnitureGroup*> mGroups;
    QString mError;
};

} // namespace BuildingEditor

#endif // BUILDINGFURNITUREFILE_H
