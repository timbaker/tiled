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

#include "textureunpacker.h"

#include "preferences.h"

#include "BuildingEditor/buildingtiles.h"

#include "tileset.h"

#include <QFile>
#include <QPainter>
#include <QTextStream>

using namespace Tiled;
using namespace Internal;

TextureUnpacker::TextureUnpacker()
{
}

bool TextureUnpacker::unpack(const QString &prefix)
{
    if (!readTxt(Preferences::instance()->tilesDirectory() + QLatin1String("/../PackedTilesheets/") + prefix + QLatin1String(".txt")))
        return false;

    QImage image(Preferences::instance()->tilesDirectory() + QLatin1String("/../PackedTilesheets/") + prefix + QLatin1String(".png"));
    if (image.isNull())
        return false;

    Pack pack;
    pack.mImage = image;
    pack.mEntries = mEntries;
    mPacks += pack;

    for (int i = 0; i < pack.mEntries.size(); i++) {
        TxtEntry &e = pack.mEntries[i];
        e.mPack = &mPacks.last();
        QString tilesetName;
        int index;
        if (BuildingEditor::BuildingTilesMgr::parseTileName(e.mTileName, tilesetName, index)) {
            int tileWidth = e.x4, tileHeight = e.y4;
            mTilesetSize[tilesetName] = mTilesetSize[tilesetName].expandedTo(QSize(tileWidth, tileHeight));
        }
    }

    return true;
}

QList<Tileset *> TextureUnpacker::createTilesets()
{
    QList<Tileset*> ret;

    const QStringList tilesetNames = mTilesetImages.keys();
    for (const QString &tilesetName : mTilesetImages.keys()) {
        QSize tileSize = mTilesetSize[tilesetName];
        Tileset *ts = new Tileset(tilesetName, tileSize.width(), tileSize.height());
//        ts->setTransparentColor(QColor(254,254,254)); // not pure white???
        ts->loadFromImage(mTilesetImages[tilesetName], QString());
        ret += ts;
    }

    return ret;
}

void TextureUnpacker::createImages()
{
    foreach (Pack pack, mPacks) {
        for (int i = 0; i < pack.mEntries.size(); i++) {
            TxtEntry &e = pack.mEntries[i];
            QString tilesetName;
            int index;
            if (BuildingEditor::BuildingTilesMgr::parseTileName(e.mTileName, tilesetName, index)) {
                int tileCol = index % 8, tileRow = index / 8;
                QSize tileSize = mTilesetSize[tilesetName];
                if (!mTilesetImages.contains(tilesetName)) {
                    mTilesetImages[tilesetName] = QImage(tileSize.width() * 8, tileSize.height() * 16, QImage::Format_ARGB32);
                    //                mTilesetImages[tilesetName].fill(QColor(254,254,254)); // not pure white???
                    mTilesetImages[tilesetName].fill(Qt::transparent);
                }

                QImage tileImg = pack.mImage.copy(e.x1, e.y1, e.x2, e.y2);
                QPainter p(&mTilesetImages[tilesetName]);
                p.drawImage(tileCol * tileSize.width() + e.x3, tileRow * tileSize.height() + e.y3, tileImg);
            }
        }
    }
}

void TextureUnpacker::writeImages(const QString &dirName)
{
    const QStringList tilesetNames = mTilesetImages.keys();
    for (const QString &tilesetName : tilesetNames) {
        mTilesetImages[tilesetName].save(dirName + QLatin1String("/") + tilesetName + QLatin1String(".png"));
    }
}

bool TextureUnpacker::readTxt(const QString &fileName)
{
    mEntries.clear();

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return false;

    QTextStream ts(&file);
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (line.contains(QLatin1Char('='))) {
            int n = line.indexOf(QLatin1Char('='));
            QString tileName = line.left(n).trimmed();
            QString xy = line.mid(n + 1).trimmed();
            QStringList xyList = xy.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (xyList.size() != 8)
                return false;
            TxtEntry e;
            e.mTileName = tileName;
            e.x1 = xyList[0].toInt();
            e.y1 = xyList[1].toInt();
            e.x2 = xyList[2].toInt();
            e.y2 = xyList[3].toInt();
            e.x3 = xyList[4].toInt();
            e.y3 = xyList[5].toInt();
            e.x4 = xyList[6].toInt();
            e.y4 = xyList[7].toInt();
            mEntries += e;
        }
    }

    return true;
}
