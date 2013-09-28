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

#ifndef VIRTUALTILESET_H
#define VIRTUALTILESET_H

#include "BuildingEditor/singleton.h"

#include <QImage>
#include <QMap>
#include <QObject>

class QGLPixelBuffer;

namespace Tiled {
namespace Internal {

class VirtualTileset;

class VirtualTile
{
public:
    enum IsoType {
        Invalid,
        WallW,
        WallN,
        WallNW,
        WallSE,
        WallWindowW,
        WallWindowN,
        WallDoorW,
        WallDoorN
    };

    VirtualTile(VirtualTileset *vts, int x, int y);
    VirtualTile(VirtualTileset *vts, int x, int y, const QString &imageSource,
                int srcX, int srcY, IsoType type);

    VirtualTileset *tileset() const { return mTileset; }

    int x() const { return mX; }
    int y() const { return mY; }

    void setImageSource(const QString &imageSource, int srcX, int srcY)
    {
        mImageSource = imageSource;
        mSrcX = srcX, mSrcY = srcY;
    }
    QString imageSource() const { return mImageSource; }
    int srcX() const { return mSrcX; }
    int srcY() const { return mSrcY; }

    void setType(IsoType type) { mType = type; }
    IsoType type() const { return mType; }

    void setImage(const QImage &image) { mImage = image; }
    QImage image() const { return mImage; }

    void clear();

private:
    VirtualTileset *mTileset;
    int mX;
    int mY;
    QString mImageSource;
    int mSrcX;
    int mSrcY;
    IsoType mType;
    QImage mImage;
};

class VirtualTileset
{
public:
    VirtualTileset(const QString &name, int columnCount, int rowCount);
    ~VirtualTileset();

    void setName(const QString &name) { mName = name; }
    QString name() const { return mName; }

    int columnCount() const { return mColumnCount; }
    int rowCount() const { return mRowCount; }
    int tileCount() const { return mColumnCount * mRowCount; }

    const QVector<VirtualTile*> &tiles() const { return mTiles; }
    VirtualTile *tileAt(int n);
    VirtualTile *tileAt(int x, int y);

    void resize(int columnCount, int rowCount);

    void tileChanged() { mImage = QImage(); }

    QImage image();

private:
    QString mName;
    int mColumnCount;
    int mRowCount;
    QVector<VirtualTile*> mTiles;
    QImage mImage;
};

class VirtualTilesetMgr : public QObject, public Singleton<VirtualTilesetMgr>
{
    Q_OBJECT
public:
    VirtualTilesetMgr();
    ~VirtualTilesetMgr();

    QString txtName() const { return QLatin1String("VirtualTilesets.txt"); }
    QString txtPath() const;

    bool readTxt();
    bool writeTxt();

    QList<VirtualTileset*> tilesets() const { return mTilesetByName.values(); }
    VirtualTileset *tileset(const QString &name)
    {
        return mTilesetByName.contains(name) ? mTilesetByName[name] : 0;
    }

    void addTileset(VirtualTileset *vts);
    void removeTileset(VirtualTileset *vts);

    QString imageSource(VirtualTileset *vts);
    bool resolveImageSource(QString &imageSource);
    VirtualTileset *tilesetFromPath(const QString &path);

    QImage renderIsoTile(VirtualTile *vtile);

    void emitTilesetChanged(VirtualTileset *vts)
    { emit tilesetChanged(vts); }

private:
    void initPixelBuffer();
    uint loadGLTexture(const QString &imageSource);

signals:
    void tilesetAdded(VirtualTileset *vts);
    void tilesetAboutToBeRemoved(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);

    void tilesetChanged(VirtualTileset *vts);

private:
    QMap<QString,VirtualTileset*> mTilesetByName;
    QList<VirtualTileset*> mRemovedTilesets;
    QGLPixelBuffer *mPixelBuffer;
};

} // namespace Internal
} // namespace Tiled

#endif // VIRTUALTILESET_H
