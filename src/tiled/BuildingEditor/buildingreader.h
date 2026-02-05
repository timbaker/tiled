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

#ifndef BUILDINGREADER_H
#define BUILDINGREADER_H

#include <QString>

class QIODevice;

namespace BuildingEditor {

class Building;
class BuildingReaderPrivate;

class BuildingReader
{
public:

    // version="1.0"
    static const int VERSION1 = 1;

    // version="2"
    // BuildingTileEntry rewrite
    // added FurnitureTiles::mCorners
    static const int VERSION2 = 2;

    // version="3"
    // Added <properties> for the in-game map
    static const int VERSION3 = 3;

    // version="4"
    // Added Ceiling tile category to rooms
    static const int VERSION4 = 4;

    // version="5"
    // Added window-frame shapes
    static const int VERSION5 = 5;

    // version="6"
    // Added 30-degree roof shapes
    static const int VERSION6 = 6;

    static const int VERSION_LATEST = VERSION6;

    explicit BuildingReader();
    ~BuildingReader();

    Building *read(const QString &fileName);

    QString errorString() const;

    void fix(Building *building);

private:
    Building *read(QIODevice *device, const QString &path);

    friend class BuildingReaderPrivate;
    BuildingReaderPrivate *d;
};

} // namespace BuildingEditor

#endif // BUILDINGREADER_H
