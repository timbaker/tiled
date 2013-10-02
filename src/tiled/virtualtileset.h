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
#include <QStringList>

class QGLPixelBuffer;

namespace Tiled {
class Tileset;

namespace Internal {

class TileShape;
class VirtualTileset;

class VirtualTile
{
public:
#if 0
    enum IsoType {
        Invalid = -1,
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

        // Sloped cap tiles go left-to-right or bottom-to-top
        CapRiseS1, CapRiseS2, CapRiseS3, CapFallS1, CapFallS2, CapFallS3,
        CapRiseE1, CapRiseE2, CapRiseE3, CapFallE1, CapFallE2, CapFallE3,

        // Cap tiles with peaked tops
        CapPeakPt5S, CapPeakOnePt5S, CapPeakTwoPt5S,
        CapPeakPt5E, CapPeakOnePt5E, CapPeakTwoPt5E,

        // Cap tiles with flat tops
        CapFlatS1, CapFlatS2, CapFlatS3,
        CapFlatE1, CapFlatE2, CapFlatE3,

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
#endif

    VirtualTile(VirtualTileset *vts, int x, int y);
    VirtualTile(VirtualTileset *vts, int x, int y, const QString &imageSource,
                int srcX, int srcY, TileShape *shape);

    VirtualTileset *tileset() const { return mTileset; }

    int x() const { return mX; }
    int y() const { return mY; }
    int index() const;

    void setImageSource(const QString &imageSource, int srcX, int srcY)
    {
        mImageSource = imageSource;
        mSrcX = srcX, mSrcY = srcY;
    }
    QString imageSource() const { return mImageSource; }
    int srcX() const { return mSrcX; }
    int srcY() const { return mSrcY; }

    void setShape(TileShape *shape) { mShape = shape; }
    TileShape *shape() const { return mShape; }

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
    TileShape *mShape;
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

    QList<TileShape*> tileShapes() const { return mShapeByName.values(); }
    TileShape *tileShape(const QString &name);
    TileShape *createTileShape(const QString &name);

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

    QMap<QString,TileShape*> mShapeByName;

    int mSourceRevision;
    int mRevision;
    QString mError;
};

/**
  * This class represents a single binary *.vts file.
  */
class VirtualTilesetsFile
{
    Q_DECLARE_TR_FUNCTIONS(VirtualTilesetsFile)
public:
    VirtualTilesetsFile();
    ~VirtualTilesetsFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<VirtualTileset*> &tilesets);

    QString directory() const;

    void addTileset(VirtualTileset *vts)
    { mTilesets += vts; mTilesetByName[vts->name()] = vts; }

    void removeTileset(VirtualTileset *vts);

    const QList<VirtualTileset*> &tilesets() const
    { return mTilesets; }

    QList<VirtualTileset*> takeTilesets()
    {
        QList<VirtualTileset*> ret = mTilesets;
        mTilesets.clear();
        mTilesetByName.clear();
        return ret;
    }

    VirtualTileset *tileset(const QString &name);

    QStringList tilesetNames() const
    { return mTilesetByName.keys(); }

    QString errorString() const
    { return mError; }

private:
    QList<VirtualTileset*> mTilesets;
    QMap<QString,VirtualTileset*> mTilesetByName;
    QString mFileName;
    QString mError;
};

#if 0
class OldVirtualTilesetsTxtFile
{
    Q_DECLARE_TR_FUNCTIONS(OldVirtualTilesetsTxtFile)

public:
    OldVirtualTilesetsTxtFile();
    ~OldVirtualTilesetsTxtFile();

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
#endif

/**
  * This class represents the TileShapes.txt file.
  */
class TileShapesFile
{
    Q_DECLARE_TR_FUNCTIONS(TileShapesFile)
public:
    TileShapesFile();
    ~TileShapesFile();

    QString fileName() const
    { return mFileName; }

    void setFileName(const QString &fileName)
    { mFileName = fileName; }

    bool read(const QString &fileName);
    bool write(const QString &fileName);
    bool write(const QString &fileName, const QList<TileShape*> &shapes);

    const QList<TileShape*> &shapes() const
    { return mShapes; }

    QList<TileShape*> takeShapes()
    {
        QList<TileShape*> ret = mShapes;
        mShapes.clear();
        mShapeByName.clear();
        return ret;
    }

    TileShape *shape(const QString &name)
    { return mShapeByName.contains(name) ? mShapeByName[name] : 0; }

    QStringList shapeNames() const
    { return mShapeByName.keys(); }

    bool parseDoubles(const QString &s, int stride, QList<qreal> &out);

    QString errorString() const
    { return mError; }

private:
    QList<TileShape*> mShapes;
    QMap<QString,TileShape*> mShapeByName;
    QString mFileName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // VIRTUALTILESET_H
