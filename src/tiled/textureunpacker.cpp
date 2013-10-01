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

    foreach (TxtEntry e, mEntries) {
        QString tilesetName;
        int index;
        if (BuildingEditor::BuildingTilesMgr::parseTileName(e.mTileName, tilesetName, index)) {
            int tileCol = index % 8, tileRow = index / 8;

            if (!mTilesetImages.contains(tilesetName)) {
                mTilesetImages[tilesetName] = QImage(32 * 8, 96 * 16, QImage::Format_ARGB32);
//                mTilesetImages[tilesetName].fill(QColor(254,254,254)); // not pure white???
                mTilesetImages[tilesetName].fill(Qt::transparent);
            }

            QImage tileImg = image.copy(e.x1, e.y1, e.x2, e.y2);
            QPainter p(&mTilesetImages[tilesetName]);
            p.drawImage(tileCol * 32 + e.x3, tileRow * 96 + e.y3, tileImg);
        }
    }

    return true;
}

QList<Tileset *> TextureUnpacker::createTilesets()
{
    QList<Tileset*> ret;

    foreach (QString tilesetName, mTilesetImages.keys()) {
        Tileset *ts = new Tileset(tilesetName, 32, 96);
//        ts->setTransparentColor(QColor(254,254,254)); // not pure white???
        ts->loadFromImage(mTilesetImages[tilesetName], QString());
        ret += ts;
    }

    return ret;
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
            QStringList xyList = xy.split(QLatin1Char(' '), QString::SkipEmptyParts);
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
}
