/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef TEXTUREUNPACKER_H
#define TEXTUREUNPACKER_H

#include <QImage>
#include <QList>
#include <QMap>
#include <QString>

namespace Tiled {
class Tileset;

namespace Internal {

class TextureUnpacker
{
public:
    TextureUnpacker();

    bool unpack(const QString &prefix);
    QList<Tileset*> createTilesets();
    void createImages();
    void writeImages(const QString &dirName);
    bool readTxt(const QString &fileName);

    struct TxtEntry;
    struct Pack
    {
        QImage mImage;
        QList<TxtEntry> mEntries;
    };

    struct TxtEntry
    {
        Pack *mPack;
        QString mTileName;
        int x1, y1, x2, y2, x3, y3, x4, y4;
    };

    QMap<QString,QImage> mTilesetImages;
    QList<Pack> mPacks;
    QList<TxtEntry> mEntries;
    QMap<QString,QSize> mTilesetSize;
};

} // namespace Internal
} // namespace Tiled

#endif // TEXTUREUNPACKER_H
