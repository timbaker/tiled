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

#ifndef BUILDINGTILESFILE_H
#define BUILDINGTILESFILE_H

#include <QMap>

class SimpleFileBlock;

namespace BuildingEditor {

class BuildingTileCategory;
class BuildingTileEntry;

class BuildingTilesFile
{
public:
    BuildingTilesFile();
    ~BuildingTilesFile();

    static int getVersionLatest();

    bool read(const QString &fileName);
    bool write(const QString &fileName, const int revision, const int sourceRevision, const QVector<BuildingTileCategory *> &categories);
    bool write(const QString &fileName);

    int getVersion() const;
    int getRevision() const;
    int getSourceRevision() const;

    const QVector<BuildingTileCategory*> &categories() const;

    const QString &errorString() const;

private:
    void writeTileEntry(SimpleFileBlock &parentBlock, BuildingTileEntry *entry);
    BuildingTileEntry *readTileEntry(BuildingTileCategory *category, const SimpleFileBlock &block, QString &error);

private:
    int mVersion;
    int mRevision;
    int mSourceRevision;
    QVector<BuildingTileCategory*> mCategories;
    QMap<QString,BuildingTileCategory*> mCategoryByName;
    QString mError;
};

} // namespace BuildingEditor

#endif // BUILDINGTILESFILE_H
