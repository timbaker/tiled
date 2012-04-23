/*
 * tilesetmanager.cpp
 * Copyright 2008-2010, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Edward Hutchins <eah1@yahoo.com>
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

#include "tilesetmanager.h"

#include "filesystemwatcher.h"
#include "tileset.h"

#include <QImage>

using namespace Tiled;
using namespace Tiled::Internal;

TilesetManager *TilesetManager::mInstance = 0;

TilesetManager::TilesetManager():
    mWatcher(new FileSystemWatcher(this)),
    mReloadTilesetsOnChange(false)
{
    connect(mWatcher, SIGNAL(fileChanged(QString)),
            this, SLOT(fileChanged(QString)));

    mChangedFilesTimer.setInterval(500);
    mChangedFilesTimer.setSingleShot(true);

    connect(&mChangedFilesTimer, SIGNAL(timeout()),
            this, SLOT(fileChangedTimeout()));
}

TilesetManager::~TilesetManager()
{
    // Since all MapDocuments should be deleted first, we assert that there are
    // no remaining tileset references.
    Q_ASSERT(mTilesets.size() == 0);
}

TilesetManager *TilesetManager::instance()
{
    if (!mInstance)
        mInstance = new TilesetManager;

    return mInstance;
}

void TilesetManager::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

Tileset *TilesetManager::findTileset(const QString &fileName) const
{
    foreach (Tileset *tileset, tilesets())
        if (tileset->fileName() == fileName)
            return tileset;

    return 0;
}

Tileset *TilesetManager::findTileset(const TilesetSpec &spec) const
{
    foreach (Tileset *tileset, tilesets()) {
        if (tileset->imageSource() == spec.imageSource
            && tileset->tileWidth() == spec.tileWidth
            && tileset->tileHeight() == spec.tileHeight
            && tileset->tileSpacing() == spec.tileSpacing
            && tileset->margin() == spec.margin)
        {
            return tileset;
        }
    }

    return 0;
}

void TilesetManager::addReference(Tileset *tileset)
{
    if (mTilesets.contains(tileset)) {
        mTilesets[tileset]++;
    } else {
        mTilesets.insert(tileset, 1);
        if (!tileset->imageSource().isEmpty())
            mWatcher->addPath(tileset->imageSource());
    }
#ifdef ZOMBOID
	if (!tileset->imageSource().isEmpty())
		readTileLayerNames(tileset);
#endif
}

void TilesetManager::removeReference(Tileset *tileset)
{
    Q_ASSERT(mTilesets.value(tileset) > 0);
    mTilesets[tileset]--;

    if (mTilesets.value(tileset) == 0) {
        mTilesets.remove(tileset);
        if (!tileset->imageSource().isEmpty())
            mWatcher->removePath(tileset->imageSource());

        delete tileset;
    }
}

void TilesetManager::addReferences(const QList<Tileset*> &tilesets)
{
    foreach (Tileset *tileset, tilesets)
        addReference(tileset);
}

void TilesetManager::removeReferences(const QList<Tileset*> &tilesets)
{
    foreach (Tileset *tileset, tilesets)
        removeReference(tileset);
}

QList<Tileset*> TilesetManager::tilesets() const
{
    return mTilesets.keys();
}

void TilesetManager::setReloadTilesetsOnChange(bool enabled)
{
    mReloadTilesetsOnChange = enabled;
    // TODO: Clear the file system watcher when disabled
}

void TilesetManager::fileChanged(const QString &path)
{
    if (!mReloadTilesetsOnChange)
        return;

    /*
     * Use a one-shot timer since GIMP (for example) seems to generate many
     * file changes during a save, and some of the intermediate attempts to
     * reload the tileset images actually fail (at least for .png files).
     */
    mChangedFiles.insert(path);
    mChangedFilesTimer.start();
}

void TilesetManager::fileChangedTimeout()
{
    foreach (Tileset *tileset, tilesets()) {
        QString fileName = tileset->imageSource();
        if (mChangedFiles.contains(fileName))
			if (tileset->loadFromImage(QImage(fileName), fileName))
#ifdef ZOMBOID
			{
				syncTileLayerNames(tileset);
                emit tilesetChanged(tileset);
			}
#else
                emit tilesetChanged(tileset);
#endif
	}

    mChangedFiles.clear();
}

#ifdef ZOMBOID
#include "tile.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QXmlStreamReader>

namespace Tiled {
namespace Internal {

struct ZTileLayerName
{
	ZTileLayerName()
		: mId(-1)
	{}
	int mId;
	QString mLayerName;
};

struct ZTileLayerNames
{
	ZTileLayerNames()
		: mThumbIndex(-1)
		, mColumns(0)
		, mRows(0)
	{}
	ZTileLayerNames(const QString& filePath, int columns, int rows)
		: mThumbIndex(-1)
		, mColumns(columns)
		, mRows(rows)
		, mFilePath(filePath)
	{
		mTiles.resize(columns * rows);
	}
	void enforceSize(int columns, int rows)
	{
		if (columns == mColumns && rows == mRows)
			return;
		if (columns == mColumns) {
			// Number of rows changed, tile ids are still valid
			mTiles.resize(columns * rows);
			return;
		}
		// Number of columns (and maybe rows) changed.
		// Copy over the preserved part.
		QRect oldBounds(0, 0, mColumns, mRows);
		QRect newBounds(0, 0, columns, rows);
		QRect preserved = oldBounds & newBounds;
		QVector<ZTileLayerName> tiles(columns * rows);
		for (int y = 0; y < preserved.height(); y++) {
			for (int x = 0; x < preserved.width(); x++) {
				tiles[y * columns + x] = mTiles[y * mColumns + x];
				tiles[y * columns + x].mId = y * columns + x;
			}
		}
		mColumns = columns;
		mRows = rows;
		mTiles = tiles;
	}

	int mColumns;
	int mRows;
	QString mFilePath;
	QString mDisplayName;
	int mThumbIndex;
	QVector<ZTileLayerName> mTiles;
};

} // namespace Internal
} // namespace Tiled

void TilesetManager::setThumbIndex(Tileset *ts, int index)
{
	if (mTileLayerNames.contains(ts->imageSource()))
		mTileLayerNames[ts->imageSource()]->mThumbIndex = index;
}

int TilesetManager::thumbIndex(Tileset *ts)
{
	if (mTileLayerNames.contains(ts->imageSource()))
		return mTileLayerNames[ts->imageSource()]->mThumbIndex;
	return -1;
}

void TilesetManager::setThumbName(Tileset *ts, const QString &name)
{
	if (mTileLayerNames.contains(ts->imageSource()))
		mTileLayerNames[ts->imageSource()]->mDisplayName = name;
}

QString TilesetManager::thumbName(Tileset *ts)
{
	if (mTileLayerNames.contains(ts->imageSource()))
		return mTileLayerNames[ts->imageSource()]->mDisplayName;
	return QString();
}

void TilesetManager::setLayerName(Tile *tile, const QString &name)
{
	Tileset *ts = tile->tileset();
	if (mTileLayerNames.contains(ts->imageSource()))
		mTileLayerNames[ts->imageSource()]->mTiles[tile->id()].mLayerName = name;
}

QString TilesetManager::layerName(Tile *tile)
{
	Tileset *ts = tile->tileset();
	if (mTileLayerNames.contains(ts->imageSource()))
		return mTileLayerNames[ts->imageSource()]->mTiles[tile->id()].mLayerName;
	return QString();
}

class ZTileLayerNamesReader
{
public:
    bool read(const QString &filePath)
	{
		mError.clear();

		QFile file(filePath);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
			mError = QCoreApplication::translate("TileLayerNames", "Could not open file.");
			return false;
		}

		xml.setDevice(&file);

		if (xml.readNextStartElement() && xml.name() == "tileset") {
			mTLN.mFilePath = filePath;
			return readTileset();
		} else {
			mError = QCoreApplication::translate("TileLayerNames", "File doesn't contain <tilesets>.");
			return false;
		}
	}

	bool readTileset()
	{
		Q_ASSERT(xml.isStartElement() && xml.name() == "tileset");

		const QXmlStreamAttributes atts = xml.attributes();
		const QString tilesetName = atts.value(QLatin1String("name")).toString();
		uint columns = atts.value(QLatin1String("columns")).toString().toUInt();
		uint rows = atts.value(QLatin1String("rows")).toString().toUInt();
		uint thumb = atts.value(QLatin1String("thumb")).toString().toUInt();

		mTLN.mTiles.resize(columns * rows);
//		for (uint i = 0; i < columns * rows; i++)
//			mTLN.mTiles[i] = ZTileLayerName();

		mTLN.mColumns = columns;
		mTLN.mRows = rows;
		mTLN.mDisplayName = tilesetName;
		mTLN.mThumbIndex = thumb;

		while (xml.readNextStartElement()) {
			if (xml.name() == "tile") {
				const QXmlStreamAttributes atts = xml.attributes();
				uint id = atts.value(QLatin1String("id")).toString().toUInt();
				if (id >= columns * rows) {
					qDebug() << "<tile> " << id << " out-of-bounds: ignored";
				} else {
					const QString layerName(atts.value(QLatin1String("layername")).toString());
					mTLN.mTiles[id].mId = id;
					mTLN.mTiles[id].mLayerName = layerName;
				}
			}
			xml.skipCurrentElement();
		}

		return true;
	}

    QString errorString() const { return mError; }
	ZTileLayerNames &result() { return mTLN; }

private:
    QString mError;
	QXmlStreamReader xml;
	ZTileLayerNames mTLN;
};

class ZTileLayerNamesWriter
{
public:
    ZTileLayerNames write(const QString &fileName, const ZTileLayerNames &tln)
	{
	}

    QString errorString() const { return mError; }

private:
    QString mError;
};

void TilesetManager::readTileLayerNames(Tileset *ts)
{
	QString imageSource = ts->imageSource();
	if (mTileLayerNames.contains(imageSource))
		return;

	int columns = ts->columnCount();
	int rows = columns ? ts->tileCount() / ts->columnCount() : 0;

	QFileInfo fileInfoImgSrc(imageSource);
	QDir dir = fileInfoImgSrc.absoluteDir();
	QFileInfo fileInfo(dir, fileInfoImgSrc.completeBaseName() + QLatin1String(".tilelayers.xml"));
	QString filePath = fileInfo.absoluteFilePath();
	qDebug() << filePath;
	if (fileInfo.exists()) {
		ZTileLayerNamesReader reader;
		if (reader.read(filePath)) {
			mTileLayerNames[imageSource] = new ZTileLayerNames(reader.result());
			// Handle the source image being resized
			mTileLayerNames[imageSource]->enforceSize(columns, rows);
		} else {
			mTileLayerNames[imageSource] = new ZTileLayerNames(filePath, columns, rows);
			QMessageBox::critical(0, tr("Error Reading Tile Layer Names"),
								  filePath + QLatin1String("\n") + reader.errorString());
		}
	} else {
		mTileLayerNames[imageSource] = new ZTileLayerNames(filePath, columns, rows);
	}
}

void TilesetManager::writeTileLayerNames(ZTileLayerNames *tln)
{
}

void TilesetManager::syncTileLayerNames(Tileset *ts)
{
	if (mTileLayerNames.contains(ts->imageSource())) {
		ZTileLayerNames *tln = mTileLayerNames[ts->imageSource()];
		int columns = ts->columnCount();
		int rows = columns ? ts->tileCount() / ts->columnCount() : 0;
		tln->enforceSize(columns, rows);
	}
}
#endif // ZOMBOID