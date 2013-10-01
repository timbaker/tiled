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

#include <QCoreApplication>
#include <QImage>
#include <QMap>
#include <QObject>

class QGLPixelBuffer;

namespace Tiled {
class Tileset;

namespace Internal {

class TileShape;
class VirtualTileset;

class VirtualTile
{
public:
    enum IsoType {
        Invalid,
        Floor,

        // Walls
        WallW,
        WallN,
        WallNW,
        WallSE,
        WallWindowW,
        WallWindowN,
        WallDoorW,
        WallDoorN,
        WallShortW,
        WallShortN,
        WallShortNW,
        WallShortSE,

        // Roof steep slopes
        SlopeS1, SlopeS2, SlopeS3,
        SlopeE1, SlopeE2, SlopeE3,
        SlopePt5S, SlopeOnePt5S, SlopeTwoPt5S,
        SlopePt5E, SlopeOnePt5E, SlopeTwoPt5E,

        // Roof trim
        TrimS, TrimE, TrimInner, TrimOuter,
        TrimCornerSW, TrimCornerNE,

        // Roof shallow slopes
        ShallowSlopeN1, ShallowSlopeN2,
        ShallowSlopeS1, ShallowSlopeS2,
        ShallowSlopeW1, ShallowSlopeW2,
        ShallowSlopeE1, ShallowSlopeE2,

        // Roof corners
        Inner1, Inner2, Inner3,
        Outer1, Outer2, Outer3,
        InnerPt5, InnerOnePt5, InnerTwoPt5,
        OuterPt5, OuterOnePt5, OuterTwoPt5,
        CornerSW1, CornerSW2, CornerSW3,
        CornerNE1, CornerNE2, CornerNE3,

        // Roof tops
        RoofTopN1, RoofTopN2, RoofTopN3,
        RoofTopW1, RoofTopW2, RoofTopW3,

        IsoTypeCount
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
    QImage image();

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

    void resize(int columnCount, int rowCount, QVector<VirtualTile *> &tiles);

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
    void renameTileset(VirtualTileset *vts, const QString &name);
    void resizeTileset(VirtualTileset *vts, QSize &size,
                       QVector<VirtualTile*> &tiles);

    QString imageSource(VirtualTileset *vts);
    bool resolveImageSource(QString &imageSource);
    VirtualTileset *tilesetFromPath(const QString &path);

    QImage renderIsoTile(VirtualTile *vtile);

    TileShape *tileShape(int isoType);
    TileShape *VirtualTilesetMgr::createTileShape(int isoType);

    void emitTilesetChanged(VirtualTileset *vts)
    { emit tilesetChanged(vts); }

    QString errorString() const
    { return mError; }

private:
    void initPixelBuffer();
    uint loadGLTexture(const QString &imageSource, int srcX, int srcY);

signals:
    void tilesetAdded(VirtualTileset *vts);
    void tilesetAboutToBeRemoved(VirtualTileset *vts);
    void tilesetRemoved(VirtualTileset *vts);

    void tilesetChanged(VirtualTileset *vts);

private:
    QMap<QString,VirtualTileset*> mTilesetByName;
    QList<VirtualTileset*> mRemovedTilesets;
    QGLPixelBuffer *mPixelBuffer;

    QMap<int,TileShape*> mShapeByType;

    QMap<QString,Tileset*> mUnpackedTilesets;

    int mSourceRevision;
    int mRevision;
    QString mError;
};

class VirtualTilesetsFile
{
    Q_DECLARE_TR_FUNCTIONS(VirtualTilesetsFile)

public:
    VirtualTilesetsFile();
    ~VirtualTilesetsFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<VirtualTileset*> &tilesets);

    void setRevision(int revision, int sourceRevision)
    { mRevision = revision, mSourceRevision = sourceRevision; }

    int revision() const { return mRevision; }
    int sourceRevision() const { return mSourceRevision; }

    const QList<VirtualTileset*> &tilesets() const
    { return mTilesets; }

    QList<VirtualTileset*> takeTilesets()
    {
        QList<VirtualTileset*> ret = mTilesets;
        mTilesets.clear();
        mTilesetByName.clear();
        return ret;
    }

    void addTileset(VirtualTileset *vts)
    { mTilesets += vts; mTilesetByName[vts->name()] = vts; }

    QString typeToName(VirtualTile::IsoType isoType)
    { return mTypeToName[isoType]; }

    QString errorString() const
    { return mError; }

private:
    bool parse2Ints(const QString &s, int *pa, int *pb);
    bool parseIsoType(const QString &s, VirtualTile::IsoType &isoType);

private:
    QList<VirtualTileset*> mTilesets;
    QMap<QString,VirtualTileset*> mTilesetByName;
    QMap<QString,VirtualTile::IsoType> mNameToType;
    QMap<VirtualTile::IsoType,QString> mTypeToName;

    int mVersion;
    int mRevision;
    int mSourceRevision;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // VIRTUALTILESET_H
