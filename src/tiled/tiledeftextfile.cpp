/*
 * Copyright 2022, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#include "tiledeftextfile.h"

#include "tiledeffile.h"

#include "BuildingEditor/simplefile.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QMap>

#if defined(Q_OS_WIN) && (_MSC_VER >= 1600)
// Hmmmm.  libtiled.dll defines the Properties class as so:
// class TILEDSHARED_EXPORT Properties : public QMap<QString,QString>
// Suddenly I'm getting a 'multiply-defined symbol' error.
// I found the solution here:
// http://www.archivum.info/qt-interest@trolltech.com/2005-12/00242/RE-Linker-Problem-while-using-QMap.html
template class __declspec(dllimport) QMap<QString, QString>;
#endif

using namespace Tiled;
using namespace Tiled::Internal;

TileDefTextFile::TileDefTextFile()
{
}

TileDefTextFile::~TileDefTextFile()
{
    qDeleteAll(mTilesets);
}

#define VERSION1 1
#define VERSION_LATEST VERSION1

bool TileDefTextFile::read(const QString &fileName)
{
    qDeleteAll(mTilesets);
    mTilesets.clear();
    mTilesetByName.clear();

    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path2 = info.canonicalFilePath();
    SimpleFile simple;
    if (!simple.read(path2)) {
        mError = simple.errorString();
        return false;
    }

    if (simple.version() != VERSION_LATEST) {
        mError = tr("Expected version %1, got %2.\n%3")
                .arg(VERSION_LATEST).arg(simple.version()).arg(fileName);
        return false;
    }

    mFileName = path2;

    for (const SimpleFileBlock& block : qAsConst(simple.blocks)) {
        if (block.name == QLatin1String("tileset")) {
            TileDefTileset *tileset = readTileset(block);
            if (tileset == nullptr) {
                return false;
            }
            insertTileset(mTilesets.size(), tileset);
        } else {
            mError = tr("Unknown block name '%1'.\n%2")
                    .arg(block.name)
                    .arg(fileName);
            return false;
        }
    }

    return true;
}

bool TileDefTextFile::write(const QString &fileName, const QList<TileDefTileset *> &tilesets)
{
    SimpleFile simpleFile;

    for (TileDefTileset *tileset : qAsConst(tilesets/*mTilesets*/)) {
        SimpleFileBlock tilesetBlock;
        tilesetBlock.name = QLatin1String("tileset");
        tilesetBlock.addValue("file", tileset->mName);
        tilesetBlock.addValue("size", QString(QLatin1String("%1,%2")).arg(tileset->mColumns).arg(tileset->mRows));
        tilesetBlock.addValue("id", QString::number(tileset->mID));
        for (TileDefTile* tile : qAsConst(tileset->mTiles)) {
            SimpleFileBlock tileBlock;
            tileBlock.comments += QString(QStringLiteral("%1_%2").arg(tileset->mName).arg(tile->mID));
            tileBlock.name = QLatin1String("tile");
            int tx = tile->mID % tileset->mColumns;
            int ty = tile->mID / tileset->mColumns;
            tileBlock.addValue("xy", QString(QLatin1String("%1,%2")).arg(tx).arg(ty));
            QMap<QString,QString> &properties = tile->mProperties;
            tile->mPropertyUI.ToProperties(properties);
            if (properties.isEmpty()) {
                continue;
            }
            const QStringList keys = properties.keys();
            for (const QString& key : keys) {
                tileBlock.addValue(key, properties[key]);
            }
            tilesetBlock.blocks += tileBlock;
        }
        simpleFile.blocks += tilesetBlock;
    }

    simpleFile.setVersion(VERSION_LATEST);
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }
    return true;

#if 0
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
        mError = tr("Error opening file for writing.\n%1").arg(fileName);
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    out << quint8('t') << quint8('d') << quint8('e') << quint8('f');
    out << qint32(VERSION_LATEST);

    out << qint32(mTilesets.size());
    foreach (TileDefTileset *ts, mTilesets) {
        SaveString(out, ts->mName);
        SaveString(out, ts->mImageSource); // no path, just file + extension
        out << qint32(ts->mColumns);
        out << qint32(ts->mRows);
        out << qint32(ts->mID);
        out << qint32(ts->mTiles.size());
        foreach (TileDefTile *tile, ts->mTiles) {
            QMap<QString,QString> &properties = tile->mProperties;
            tile->mPropertyUI.ToProperties(properties);
            out << qint32(properties.size());
            foreach (QString key, properties.keys()) {
                SaveString(out, key);
                SaveString(out, properties[key]);
            }
        }
    }
#endif
    return true;
}

QString TileDefTextFile::directory() const
{
    return QFileInfo(mFileName).absolutePath();
}

void TileDefTextFile::insertTileset(int index, TileDefTileset *ts)
{
    Q_ASSERT(!mTilesets.contains(ts));
    Q_ASSERT(!mTilesetByName.contains(ts->mName));
    mTilesets.insert(index, ts);
    mTilesetByName[ts->mName] = ts;
}

TileDefTileset *TileDefTextFile::removeTileset(int index)
{
    mTilesetByName.remove(mTilesets[index]->mName);
    return mTilesets.takeAt(index);
}

TileDefTileset *TileDefTextFile::tileset(const QString &name) const
{
    if (mTilesetByName.contains(name))
        return mTilesetByName[name];
    return 0;
}

QList<TileDefTileset *> TileDefTextFile::takeTilesets()
{
    QList<TileDefTileset*> tilesets = mTilesets;
    mTilesets.clear();
    return tilesets;
}

TileDefTileset *TileDefTextFile::readTileset(const SimpleFileBlock &block)
{
    QString tilesetFileName = block.value("file");
    if (tilesetFileName.isEmpty()) {
        mError = tr("No-name tilesets aren't allowed.");
        return nullptr;
    }
    std::unique_ptr<TileDefTileset> ts(new TileDefTileset());
    ts->mName = tilesetFileName;
    ts->mImageSource = tilesetFileName + QLatin1String(".png");
    if ((parse2Ints(block.value("size"), &ts->mColumns, &ts->mRows) == false) || (ts->mColumns < 1) || (ts->mRows < 1)) {
        mError = tr("invalid or missing tileset.size");
        return nullptr;
    }
    if (parseUInt(block, QLatin1String("id"), ts->mID) == false) {
        mError = tr("invalid or missing tileset.rows");
        return nullptr;
    }
    ts->mTiles.resize(ts->mColumns * ts->mRows);
    for (const SimpleFileBlock& child : block.blocks) {
        if (child.name == QLatin1String("tile")) {
            TileDefTile *tile = readTile(child, ts.get());
            if (tile == nullptr) {
                return nullptr;
            }
            ts->mTiles[tile->id()] = tile;
        } else {
            mError = tr("Unknown block name '%1'.").arg(child.name);
            return nullptr;
        }
    }
    for (int i = 0; i < ts->mTiles.size(); i++) {
        if (ts->mTiles[i] == nullptr) {
            ts->mTiles[i] = new TileDefTile(ts.get(), i);
        }
    }
    return ts.release();
}

TileDefTile *TileDefTextFile::readTile(const SimpleFileBlock &block, TileDefTileset *tileset)
{
    std::unique_ptr<TileDefTile> tile(new TileDefTile(tileset, -1));
    QMap<QString,QString> properties;
    for (const SimpleFileKeyValue& kv : block.values) {
        if (kv.name == QLatin1String("xy")) {
            int column, row;
            if (!parse2Ints(kv.value, &column, &row) || (column < 0) || (row < 0)) {
                mError = tr("Invalid %1 = %2").arg(kv.name).arg(kv.value);
                return nullptr;
            }
            tile->mID = column + row * tileset->mColumns;
        } else {
            properties[kv.name] = kv.value;
        }
    }
    if (tile->id() < 0 || tile->id() >= tileset->mColumns * tileset->mRows) {
        mError = tr("invalid or missing tile.xy");
        return nullptr;
    }
    TilePropertyMgr::instance()->modify(properties);
    tile->mPropertyUI.FromProperties(properties);
    tile->mProperties = properties;
    return tile.release();
}

bool TileDefTextFile::parseUInt(const SimpleFileBlock &block, const QString &key, int &result)
{
    return parseUInt(block.value(key), result);
}

bool TileDefTextFile::parseUInt(const QString &str, int &result)
{
    bool ok = false;
    result = str.toUInt(&ok);
    return ok;
}

bool TileDefTextFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a;
    *pb = b;
    return true;
}
