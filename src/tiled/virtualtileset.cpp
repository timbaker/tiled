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

#include "virtualtileset.h"

#include "preferences.h"
#include "tilesetmanager.h"
#include "tileshapeeditor.h"

#include "BuildingEditor/buildingtiles.h"

#include "tile.h"
#include "tileset.h"

#include <QDebug>
#include <QDir>
#include <QGLPixelBuffer>
#include <QVector2D>
#include <QVector3D>

using namespace Tiled;
using namespace Internal;

VirtualTile::VirtualTile(VirtualTileset *vts, int x, int y) :
    mTileset(vts),
    mX(x),
    mY(y),
    mSrcX(-1),
    mSrcY(-1),
    mType(Invalid)
{

}

VirtualTile::VirtualTile(VirtualTileset *vts, int x, int y, const QString &imageSource,
                         int srcX, int srcY, VirtualTile::IsoType type) :
    mTileset(vts),
    mImageSource(imageSource),
    mX(x),
    mY(y),
    mSrcX(srcX),
    mSrcY(srcY),
    mType(type)
{

}

QImage VirtualTile::image()
{
    if (mImage.isNull())
        mImage = VirtualTilesetMgr::instance().renderIsoTile(this);
    return mImage;
}

void VirtualTile::clear()
{
    mImageSource.clear();
    mSrcX = mSrcY = -1;
    mType = Invalid;
    mImage = QImage();
}

/////

VirtualTileset::VirtualTileset(const QString &name, int columnCount, int rowCount) :
    mName(name),
    mColumnCount(columnCount),
    mRowCount(rowCount),
    mTiles(mColumnCount * mRowCount)
{
    for (int y = 0; y < rowCount; y++)
        for (int x = 0; x < columnCount; x++)
            mTiles[y * columnCount + x] = new VirtualTile(this, x, y);
}

VirtualTileset::~VirtualTileset()
{
    qDeleteAll(mTiles);
}

VirtualTile *VirtualTileset::tileAt(int n)
{
    if (n >= 0 && n < mTiles.size())
        return mTiles.at(n);
    return 0;
}

VirtualTile *VirtualTileset::tileAt(int x, int y)
{
    if (!QRect(0, 0, mColumnCount, mRowCount).contains(x, y))
        return 0;
    return tileAt(y * mColumnCount + x);
}

void VirtualTileset::resize(int columnCount, int rowCount, QVector<VirtualTile *> &tiles)
{
    Q_ASSERT(tiles.size() == columnCount * rowCount);
    QVector<VirtualTile*> old = mTiles;
    mTiles = tiles;
    mColumnCount = columnCount;
    mRowCount = rowCount;
    tiles = old;
}

QImage VirtualTileset::image()
{
    if (mImage.isNull()) {
        mImage = QImage(columnCount() * 64, rowCount() * 128, QImage::Format_ARGB32);
        mImage.fill(Qt::transparent);
        QPainter painter(&mImage);
        for (int i = 0; i < tileCount(); i++) {
            VirtualTile *vtile = tileAt(i);
            painter.drawImage(vtile->x() * 64, vtile->y() * 128, vtile->image());
        }
        painter.end();
    }
    return mImage;
}

/////

SINGLETON_IMPL(VirtualTilesetMgr)

#include "textureunpacker.h"
VirtualTilesetMgr::VirtualTilesetMgr()
{
    initPixelBuffer();

    TextureUnpacker unpacker;
    unpacker.unpack(QLatin1String("ntiles_0"));
    unpacker.unpack(QLatin1String("ntiles_1"));
    unpacker.unpack(QLatin1String("ntiles_2"));
    unpacker.unpack(QLatin1String("ntiles_3"));
    foreach (Tileset *ts, unpacker.createTilesets())
        mUnpackedTilesets[ts->name()] = ts;

#if 0
    VirtualTileset *vts = new VirtualTileset(QLatin1String("walls_exterior_house_01"), 512/64, 1024/128);
    VirtualTile *vtile = vts->tileAt(0, 0);
    vtile->setImageSource(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\Tiles\\Textures\\tex_walls_exterior_house_01.png"), 0, 0);
    vtile->setType(VirtualTile::WallW);
    vtile->setImage(renderIsoTile(vtile));

    mTilesetByName[vts->name()] = vts;
#endif

#if 1
    for (int i = VirtualTile::Invalid + 1; i < VirtualTile::IsoTypeCount; i++) {
        TileShape *shape = createTileShape(i);
        mShapeByType[i] = shape;
    }
#else
    VirtualTilesetsFile file;
    for (int i = VirtualTile::Invalid + 1; i < VirtualTile::IsoTypeCount; i++) {
        TileShape *shape = createTileShape(i);
        mShapeByType[i] = shape;

        QString s;
        QTextStream ts(&s);
        ts << file.typeToName(static_cast<VirtualTile::IsoType>(i)) << QLatin1String(",");
        foreach (TileShape::Element e, shape->mElements) {
            foreach (QPointF p, e.mGeom) {
                p = TileShapeScene::toScene(p);
                ts << QString::fromLatin1("%1 %2 ").arg(p.x()).arg(p.y());
            }
            foreach (QPointF p, e.mUV)
                ts << QString::fromLatin1("%1 %2 ").arg(p.x()).arg(p.y());
            ts << QLatin1String(",");
        }
        qDebug() << s;
    }
#endif
}

VirtualTilesetMgr::~VirtualTilesetMgr()
{
    delete mPixelBuffer;
}

QString VirtualTilesetMgr::txtPath() const
{
    return Preferences::instance()->configPath(txtName());
}

bool VirtualTilesetMgr::readTxt()
{
    QFileInfo info(txtPath());
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(txtName());
        return false;
    }

    VirtualTilesetsFile file;
    if (!file.read(txtPath())) {
        mError = file.errorString();
        return false;
    }

    mRevision = file.revision();
    mSourceRevision = file.sourceRevision();

    foreach (VirtualTileset *vts, file.takeTilesets())
        addTileset(vts);

    return true;
}

bool VirtualTilesetMgr::writeTxt()
{
    VirtualTilesetsFile file;
    file.setRevision(++mRevision, mSourceRevision);
    if (!file.write(txtPath(), tilesets())) {
        mError = file.errorString();
        return false;
    }

    return true;
}

void VirtualTilesetMgr::addTileset(VirtualTileset *vts)
{
    Q_ASSERT(mTilesetByName.contains(vts->name()) == false);
    mTilesetByName[vts->name()] = vts;
    mRemovedTilesets.removeAll(vts);
    emit tilesetAdded(vts);
}

void VirtualTilesetMgr::removeTileset(VirtualTileset *vts)
{
    Q_ASSERT(mTilesetByName.contains(vts->name()));
    Q_ASSERT(mRemovedTilesets.contains(vts) == false);
    emit tilesetAboutToBeRemoved(vts);
    mTilesetByName.remove(vts->name());
    emit tilesetRemoved(vts);

    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += vts;
}

void VirtualTilesetMgr::renameTileset(VirtualTileset *vts, const QString &name)
{
    removeTileset(vts);
    vts->setName(name);
    addTileset(vts);
}

void VirtualTilesetMgr::resizeTileset(VirtualTileset *vts, QSize &size,
                                      QVector<VirtualTile*> &tiles)
{
    removeTileset(vts);
    QSize oldSize(vts->columnCount(), vts->rowCount());
    vts->resize(size.width(), size.height(), tiles);
    size = oldSize;
    vts->tileChanged();
    addTileset(vts);
}

QString VirtualTilesetMgr::imageSource(VirtualTileset *vts)
{
    QString tilesDir = Preferences::instance()->tilesDirectory();
    if (QFileInfo(tilesDir).exists())
        tilesDir = QFileInfo(tilesDir).canonicalFilePath();
    return QDir(tilesDir).filePath(vts->name() + QLatin1String(".png"));
}

bool VirtualTilesetMgr::resolveImageSource(QString &imageSource)
{
    if (imageSource.isEmpty())
        return false;
    QFileInfo info(imageSource);
    if (info.isRelative())
        return false;
    if (!mTilesetByName.contains(info.completeBaseName()))
        return false;
    QString tilesDir = Preferences::instance()->tilesDirectory();
    if (tilesDir.isEmpty() || QFileInfo(tilesDir).isRelative())
        return false;
    // FIXME: use QFileInfo::operator==() if both exist
    if (QDir::cleanPath(tilesDir) == QDir::cleanPath(info.absolutePath())) {
        if (QFileInfo(tilesDir).exists())
            tilesDir = QFileInfo(tilesDir).canonicalFilePath();
        imageSource = QDir(tilesDir).filePath(info.fileName());
        return true;
    }
    return false;
}

VirtualTileset *VirtualTilesetMgr::tilesetFromPath(const QString &path)
{
    QString canonical = path;
    if (resolveImageSource(canonical))
        return tileset(QFileInfo(canonical).completeBaseName());
    return 0;
}

struct TextureTile
{
    TextureTile(int texWidth, int texHeight, int tileWidth, int tileHeight, int texCol, int texRow) :
        mTexWid(texWidth),
        mTexHgt(texHeight),
        mTileWidth(tileWidth),
        mTileHeight(tileHeight),
        mTexCol(texCol),
        mTexRow(texRow)
    {}

    QVector2D uv(qreal x, qreal y)
    {
        return QVector2D((mTexCol * mTileWidth + x) / qreal(mTexWid),
                         1 - (mTexRow * mTileHeight + y) / qreal(mTexHgt));
    }

    int mTexWid;
    int mTexHgt;
    int mTileWidth;
    int mTileHeight;
    int mTexCol;
    int mTexRow;
};

struct PBufferFace
{
    PBufferFace(QGLPixelBuffer *pbuffer, int col, int row, int tileWidth, int tileHeight) :
        mPBuffer(pbuffer),
        mColumn(col),
        mRow(row),
        mTileWidth(tileWidth),
        mTileHeight(tileHeight)
    {
    }

    QVector3D flatNS(qreal x, qreal y, qreal z = 0)
    {
        QPointF p = toScene(x / qreal(mTileWidth), y / qreal(mTileHeight));
        return QVector3D(p.x(), p.y() - z, 0);
    }

    QVector3D flatWE(qreal x, qreal y, qreal z = 0)
    {
        QPointF p = toScene(y / qreal(mTileHeight), 1 - x / qreal(mTileWidth));
        return QVector3D(p.x(), p.y() - z, 0);
    }

    QVector3D west(int x, int y, int dx, int dy = 1)
    {
        // HACK - dy is the offset from top (the original tilesets were offset by 1)
        QPointF p = toScene(dx / qreal(mTileWidth), (1 - (x - dy) / qreal(mTileWidth)));
        return QVector3D(p.x(), p.y() - mTileHeight + y, 0);
    }

    QVector3D north(int x, int y, int dy)
    {
        QPointF p = toScene(x / qreal(mTileWidth), dy / qreal(mTileWidth));
        return QVector3D(p.x(), p.y() - mTileHeight + y, 0);
    }

    QPointF toScene(qreal x, qreal y)
    {
        const int tileWidth = 64, tileHeight = 32;
        const int originX = mColumn * 64 + tileWidth / 2;
        const int originY = mRow * 128 + 96;
        return QPointF((x - y) * tileWidth / 2 + originX,
                       (x + y) * tileHeight / 2 + originY);
    }

    QGLPixelBuffer *mPBuffer;
    int mColumn;
    int mRow;
    int mTileWidth;
    int mTileHeight;
};

class DrawElements
{
public:
    void clear()
    {
        counts.resize(0);
        indices.resize(0);
        vertices.resize(0);
        texcoords.resize(0);
        textureids.resize(0);
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3, const QVector2D &uv4,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4)
    {
        counts += 4;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        colors += QVector3D(1, 1, 1);
        texcoords << uv1 << uv2 << uv3 << uv4;
        vertices << v1 << v2 << v3 << v4;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3)
    {
        counts += 3;
        indices << indices.size() << indices.size() + 1 << indices.size() + 2;
        textureids += textureid;
        colors += QVector3D(1, 1, 1);
        texcoords << uv1 << uv2 << uv3;
        vertices << v1 << v2 << v3;

        if (indices.size() > 1024 * 2)
            flush();
    }

    void color(float r, float g, float b)
    {
        colors.last() = QVector3D(r, g, b);
    }

    void flush()
    {
#if QT_VERSION >= 0x050000
#else
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords.constData());
        const GLushort *ind = indices.constData();
        for (int i = 0; i < textureids.size(); i++) {
            glColor3f(colors[i].x(), colors[i].y(), colors[i].z());
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            if (counts[i] == 4)
                glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, ind);
            else if (counts[i] == 3)
                glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, ind);
            ind += counts[i];
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        clear();
#endif
    }

    QVector<char> counts;
    QVector<GLushort> indices;
    QVector<QVector3D> vertices;
    QVector<QVector2D> texcoords;
    QVector<GLuint> textureids;
    QVector<QVector3D> colors;

};


void VirtualTilesetMgr::initPixelBuffer()
{
    int width = 64, height = 128; // Size of one iso tile

    QGLFormat pbufferFormat;
    pbufferFormat.setSampleBuffers(false);
    mPixelBuffer = new QGLPixelBuffer(QSize(width, height), pbufferFormat);

    mPixelBuffer->makeCurrent();

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -999999, 999999);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if QT_VERSION >= 0x050000
#else
    glShadeModel(GL_FLAT);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

uint VirtualTilesetMgr::loadGLTexture(const QString &imageSource, int srcX, int srcY)
{
    if (!mUnpackedTilesets.contains(imageSource))
        return 0;
    Tile *unpackedTile = mUnpackedTilesets[imageSource]->tileAt(srcY * mUnpackedTilesets[imageSource]->columnCount() + srcX);
    QImage b = unpackedTile->image();
    if (b.isNull()) return 0;

    QImage fixedImage(b.width(), b.height(), QImage::Format_ARGB32);
    QPainter painter(&fixedImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(fixedImage.rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(0, 0, b);
    painter.end();

    b = QGLWidget::convertToGLFormat( fixedImage );
    GLuint textureID;
    glGenTextures( 1, &textureID );
    glBindTexture( GL_TEXTURE_2D, textureID );
    glTexImage2D( GL_TEXTURE_2D, 0, 4, b.width(), b.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, b.bits() );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    return textureID;
}

QImage VirtualTilesetMgr::renderIsoTile(VirtualTile *vtile)
{
#if 1
    if (vtile->type() == VirtualTile::Invalid)
        return QImage();

    const QGLContext *context = QGLContext::currentContext();
    mPixelBuffer->makeCurrent();

    GLuint textureID = loadGLTexture(vtile->imageSource(), vtile->srcX(), vtile->srcY());
    if (!textureID) {
        if (context)
            const_cast<QGLContext*>(context)->makeCurrent();
        return QImage();
    }
#else
    int width = 64, height = 128;
    // Create a pbuffer, has its own OpenGL context
    const QGLContext *context = QGLContext::currentContext();
    QGLFormat pbufferFormat;
    pbufferFormat.setSampleBuffers(false);
    QGLPixelBuffer pbuffer(QSize(width, height), pbufferFormat);
    pbuffer.makeCurrent();

    GLuint textureID = loadGLTexture(vtile->imageSource());
    if (!textureID)
        return QImage();

    // guesswork from qpaintengine_opengl.cpp
    {
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -999999, 999999);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#if QT_VERSION >= 0x050000
#else
    glShadeModel(GL_FLAT);
#endif

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
#endif
#if QT_VERSION >= 0x050000
#else
    glClearDepth(1.0f);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


#if 1
    if (TileShape *shape = tileShape(vtile->type())) {
        DrawElements de;
        foreach (TileShape::Element e, shape->mElements) {
            if (e.mGeom.size() == 4)
                de.add(textureID,
                       QVector2D(e.mUV[0]), QVector2D(e.mUV[1]), QVector2D(e.mUV[2]), QVector2D(e.mUV[3]),
                        QVector3D(TileShapeScene::toScene(e.mGeom[0])), QVector3D(TileShapeScene::toScene(e.mGeom[1])),
                        QVector3D(TileShapeScene::toScene(e.mGeom[2])), QVector3D(TileShapeScene::toScene(e.mGeom[3])));
            if (e.mGeom.size() == 3)
                de.add(textureID,
                       QVector2D(e.mUV[0]), QVector2D(e.mUV[1]), QVector2D(e.mUV[2]),
                        QVector3D(TileShapeScene::toScene(e.mGeom[0])), QVector3D(TileShapeScene::toScene(e.mGeom[1])),
                        QVector3D(TileShapeScene::toScene(e.mGeom[2])));
            de.flush();
        }
    }
#else

    /////

    int tx = vtile->srcX(), ty = vtile->srcY(); // tile col/row
    int tw = 32, th = 96; // tile width/height
    int texWid = 32, texHgt = 96;
    // West walls are 3 from left edge of a tile, AND offset 1 pixel from top
    // North walls are 4 from top edge of a tile
    int wallDX = 3, wallDY = 4;
    TextureTile tt(texWid, texHgt, tw, th, tx, ty);
    PBufferFace f(mPixelBuffer, 0, 0, tw, th);
    DrawElements de;
    if (vtile->type() == VirtualTile::Floor) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, 0), f.flatNS(tw, 0, 0),
               f.flatNS(tw, th, 0), f.flatNS(0, th, 0));
    }

    //
    // WALLS
    //
    if (vtile->type() == VirtualTile::WallW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallN) {
        de.add(textureID,
                tt.uv(0, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(0, th),
                f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
                f.north(tw, th, wallDY), f.north(0, th, wallDY));
    }
    if (vtile->type() == VirtualTile::WallNW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw - wallDY, 0),
               tt.uv(tw - wallDY, th), tt.uv(0, th),
               f.west(0, 0, wallDX), f.west(tw - wallDY, 0, wallDX, 0),
               f.west(tw - wallDY, th, wallDX, 0), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(wallDX, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(wallDX, th),
                f.north(wallDX, 0, wallDY), f.north(tw, 0, wallDY),
                f.north(tw, th, wallDY), f.north(wallDX, th, wallDY));
    }
    if (vtile->type() == VirtualTile::WallSE) {
        de.add(textureID,
               tt.uv(tw - wallDY, 0), tt.uv(tw, 0),
               tt.uv(tw, th), tt.uv(tw - wallDY, th),
               f.west(tw - wallDY, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, th, wallDX), f.west(tw - wallDY, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(0, 0), tt.uv(wallDX, 0),
                tt.uv(wallDX, th), tt.uv(0, th),
                f.north(0, 0, wallDY), f.north(wallDX, 0, wallDY),
                f.north(wallDX, th, wallDY), f.north(0, th, wallDY));
    }
    int topHgt = 21, botHgt = 37;
    if (vtile->type() == VirtualTile::WallWindowW) {
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, topHgt, wallDX), f.west(0, topHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, th - botHgt, wallDX), f.west(tw, th - botHgt, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.west(0, topHgt, wallDX), f.west(side, topHgt, wallDX),
               f.west(side, th - botHgt, wallDX), f.west(0, th - botHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.west(tw - side, topHgt, wallDX), f.west(tw, topHgt, wallDX),
               f.west(tw, th - botHgt, wallDX), f.west(tw - side, th - botHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallWindowN) {
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
               f.north(tw, topHgt, wallDY), f.north(0, topHgt, wallDY));
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.north(0, th - botHgt, wallDY), f.north(tw, th - botHgt, wallDY),
               f.north(tw, th, wallDY), f.north(0, th, wallDY));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.north(0, topHgt, wallDY), f.north(side, topHgt, wallDY),
               f.north(side, th - botHgt, wallDY), f.north(0, th - botHgt, wallDY));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.north(tw - side, topHgt, wallDY), f.north(tw, topHgt, wallDY),
               f.north(tw, th - botHgt, wallDY), f.north(tw - side, th - botHgt, wallDY));
    }
    if (vtile->type() == VirtualTile::WallDoorW) {
        int side = 4;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, topHgt, wallDX), f.west(0, topHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.west(0, topHgt, wallDX), f.west(side, topHgt, wallDX),
               f.west(side, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.west(tw - side, topHgt, wallDX), f.west(tw, topHgt, wallDX),
               f.west(tw, th, wallDX), f.west(tw - side, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallDoorN) {
        int side = 4;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
               f.north(tw, topHgt, wallDY), f.north(0, topHgt, wallDY));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.north(0, topHgt, wallDY), f.north(side, topHgt, wallDY),
               f.north(side, th, wallDY), f.north(0, th, wallDY));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.north(tw - side, topHgt, wallDY), f.north(tw, topHgt, wallDY),
               f.north(tw, th, wallDY), f.north(tw - side, th, wallDY));
    }
    const int shortWallHgt = 57;
    if (vtile->type() == VirtualTile::WallShortW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.west(0, th - shortWallHgt, wallDX), f.west(tw, th - shortWallHgt, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallShortN) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.north(0, th - shortWallHgt, wallDY), f.north(tw, th - shortWallHgt, wallDY),
               f.north(tw, th, wallDY), f.north(0, th, wallDY));
    }
    if (vtile->type() == VirtualTile::WallShortNW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.west(0, th - shortWallHgt, wallDX), f.west(tw - wallDY, th - shortWallHgt, wallDX, 0),
               f.west(tw - wallDY, th, wallDX, 0), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.north(wallDX, th - shortWallHgt, wallDY), f.north(tw, th - shortWallHgt, wallDY),
               f.north(tw, th, wallDY), f.north(wallDX, th, wallDY));
    }
    if (vtile->type() == VirtualTile::WallShortSE) {
        de.add(textureID,
               tt.uv(tw - wallDY, th - shortWallHgt), tt.uv(tw, th - shortWallHgt),
               tt.uv(tw, th), tt.uv(tw - wallDY, th),
               f.west(tw - wallDY, th - shortWallHgt, wallDX), f.west(tw, th - shortWallHgt, wallDX),
               f.west(tw, th, wallDX), f.west(tw - wallDY, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(0, th - shortWallHgt), tt.uv(wallDX, th - shortWallHgt),
                tt.uv(wallDX, th), tt.uv(0, th),
                f.north(0, th - shortWallHgt, wallDY), f.north(wallDX, th - shortWallHgt, wallDY),
                f.north(wallDX, th, wallDY), f.north(0, th, wallDY));
    }

    //
    // ROOFS
    //
    if (vtile->type() >= VirtualTile::SlopeS1 && vtile->type() <= VirtualTile::SlopeS3) {
        int dy = (vtile->type() - vtile->SlopeS1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (vtile->type() >= VirtualTile::SlopeE1 && vtile->type() <= VirtualTile::SlopeE3) {
        int dy = (vtile->type() - vtile->SlopeE1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatWE(0, 0, 32 + dy), f.flatWE(tw, 0, 32 + dy), f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::SlopePt5S && vtile->type() <= VirtualTile::SlopeTwoPt5S) {
        int dy = (vtile->type() - vtile->SlopePt5S) * 32;
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, th / 2, 16 + dy), f.flatNS(tw, th / 2, 16 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (vtile->type() >= VirtualTile::SlopePt5E && vtile->type() <= VirtualTile::SlopeTwoPt5E) {
        int dy = (vtile->type() - vtile->SlopePt5E) * 32;
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatWE(0, th / 2, 16 + dy), f.flatWE(tw, th / 2, 16 + dy), f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::Outer1 && vtile->type() <= VirtualTile::Outer3) {
        int dy = (vtile->type() - vtile->Outer1) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
        // east
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::Inner1 && vtile->type() <= VirtualTile::Inner3) {
        int dy = (vtile->type() - vtile->Inner1) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy));
        // east
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(0, 64),
               f.flatNS(0, th, 32 + dy), f.flatNS(0, 0, 32 + dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::OuterPt5 && vtile->type() <= VirtualTile::OuterTwoPt5) {
        int dy = (vtile->type() - vtile->OuterPt5) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw / 2, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(tw / 2, th, 32 + dy), f.flatNS(0, 0, dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
        // east
        de.add(textureID,
               tt.uv(tw / 2, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, dy), f.flatNS(tw, th / 2, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::InnerPt5 && vtile->type() <= VirtualTile::InnerTwoPt5) {
        int dy = (vtile->type() - vtile->InnerPt5) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw / 2, 32), tt.uv(tw, 64),
               f.flatNS(0, 0, dy), f.flatNS(tw / 2, 0, dy), f.flatNS(tw, th, dy));
        // east
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64),
               f.flatNS(0, th / 2, dy), f.flatNS(0, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::CornerSW1 && vtile->type() <= VirtualTile::CornerSW3) {
        int dy = (vtile->type() - vtile->CornerSW1) * 32;
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (vtile->type() >= VirtualTile::CornerNE1 && vtile->type() <= VirtualTile::CornerNE3) {
        int dy = (vtile->type() - vtile->CornerNE1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, th, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() >= VirtualTile::RoofTopN1 && vtile->type() <= VirtualTile::RoofTopN3) {
        int dy = (vtile->type() - vtile->RoofTopN1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, dy), f.flatNS(tw, 0, dy),
               f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (vtile->type() >= VirtualTile::RoofTopW1 && vtile->type() <= VirtualTile::RoofTopW3) {
        int dy = (vtile->type() - vtile->RoofTopW1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, dy), f.flatWE(tw, 0, dy),
               f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
    }

    int shallowRise = 8;
    if (vtile->type() >= VirtualTile::ShallowSlopeN1 && vtile->type() <= VirtualTile::ShallowSlopeN2) {
        int dy = 10 + (vtile->type() - vtile->ShallowSlopeN1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, dy), f.flatNS(tw, 0, dy),
               f.flatNS(tw, th, shallowRise + dy), f.flatNS(0, th, shallowRise + dy));
    }
    if (vtile->type() >= VirtualTile::ShallowSlopeS1 && vtile->type() <= VirtualTile::ShallowSlopeS2) {
        int dy = 10 + (vtile->type() - vtile->ShallowSlopeS1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, shallowRise + dy), f.flatNS(tw, 0, shallowRise + dy),
               f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (vtile->type() >= VirtualTile::ShallowSlopeW1 && vtile->type() <= VirtualTile::ShallowSlopeW2) {
        int dy = 10 + (vtile->type() - vtile->ShallowSlopeW1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, dy), f.flatWE(tw, 0, dy),
               f.flatWE(tw, th, shallowRise + dy), f.flatWE(0, th, shallowRise + dy));
    }
    if (vtile->type() >= VirtualTile::ShallowSlopeE1 && vtile->type() <= VirtualTile::ShallowSlopeE2) {
        int dy = 10 + (vtile->type() - vtile->ShallowSlopeE1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, shallowRise + dy), f.flatWE(tw, 0, shallowRise + dy),
               f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
    }

    if (vtile->type() == VirtualTile::TrimS) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3), tt.uv(0, 64/3),
               f.flatNS(0, 0, 96), f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3, 96-(32/3+1)), f.flatNS(0, th / 3, 96-(32/3+1)));
    }
    if (vtile->type() == VirtualTile::TrimE) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3), tt.uv(0, 64/3),
               f.flatWE(0, 0, 96), f.flatWE(tw, 0, 96), f.flatWE(tw, th / 3, 96-(32/3+1)), f.flatWE(0, th / 3, 96-(32/3+1)));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::TrimOuter) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64/3), tt.uv(0, 64/3),
               f.flatNS(0, 0, 96), f.flatNS(tw * 0.66, th * 0.66, 96), f.flatNS(0, th / 3, 96-(32/3+1)));
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64/3), tt.uv(0, 64/3),
               f.flatNS(0, 0, 96), f.flatNS(tw / 3, 0, 96-(32/3+1)), f.flatNS(tw * 0.66, th * 0.66, 96));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::TrimInner) {
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3), tt.uv(64 / 3, 64/3),
               f.flatNS(0, 0, 96), f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3, 96-(32/3+1)), f.flatNS((tw * 2) / 3, (th * 2) / 3, 96));
        // east
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw/3, 64/3), tt.uv(0, 64/3),
               f.flatWE(0, 0, 96), f.flatWE(tw, 0, 96), f.flatWE(tw / 3, (th * 2) / 3, 96), f.flatWE(0, th / 3, 96-(32/3+1)));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::TrimCornerSW) {
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw - 64/3, 64/3), tt.uv(0, 64/3),
               f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3, 96-(32/3+1)), f.flatNS(tw, th * 2 / 3, 96));
    }
    if (vtile->type() == VirtualTile::TrimCornerNE) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw / 3, 64/3), tt.uv(0, 64/3),
               f.flatNS(0, th, 96), f.flatNS(tw * 2 / 3, th, 96), f.flatNS(tw / 3, th, 96-(32/3+1)));
        de.color(0.8f,0.8f,0.8f);
    }
#if 0
    de.add(textureID, QVector2D(0,0), QVector2D(1,0), QVector2D(1,1), QVector2D(0,1),
           QVector3D(0,0,0), QVector3D(32,0,0), QVector3D(32,96,0), QVector3D(0,96,0));
#endif
    de.flush();
#endif
    /////

#if 1
    glDeleteTextures(1, &textureID);
    mPixelBuffer->doneCurrent();

    QImage img = mPixelBuffer->toImage();
#else
    QImage img = pbuffer.toImage();

    pbuffer.doneCurrent();
#endif

    if (context)
        const_cast<QGLContext*>(context)->makeCurrent();

    return img;
}

TileShape *VirtualTilesetMgr::tileShape(int isoType)
{
    if (mShapeByType.contains(isoType))
        return mShapeByType[isoType];
    return 0;
}

TileShape *VirtualTilesetMgr::createTileShape(int isoType)
{
    QList<QPolygonF> ret;

    int tx = 0/*vtile->srcX()*/, ty = 0/*vtile->srcY()*/; // tile col/row
    int tw = 32, th = 96; // tile width/height
    int texWid = 32, texHgt = 96;
    // West walls are 3 from left edge of a tile, AND offset 1 pixel from top
    // North walls are 4 from top edge of a tile
    int wallDX = 3, wallDY = 4;
    TextureTile tt(texWid, texHgt, tw, th, tx, ty);
    PBufferFace f(mPixelBuffer, 0, 0, tw, th);
    DrawElements de;

    GLuint textureID = 0;

    if (isoType == VirtualTile::Floor) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, 0), f.flatNS(tw, 0, 0),
               f.flatNS(tw, th, 0), f.flatNS(0, th, 0));
    }

    //
    // WALLS
    //

    //
    // WALLS
    //
    if (isoType == VirtualTile::WallW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::WallN) {
        de.add(textureID,
                tt.uv(0, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(0, th),
                f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
                f.north(tw, th, wallDY), f.north(0, th, wallDY));
    }
    if (isoType == VirtualTile::WallNW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw - wallDY, 0),
               tt.uv(tw - wallDY, th), tt.uv(0, th),
               f.west(0, 0, wallDX), f.west(tw - wallDY, 0, wallDX, 0),
               f.west(tw - wallDY, th, wallDX, 0), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(wallDX, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(wallDX, th),
                f.north(wallDX, 0, wallDY), f.north(tw, 0, wallDY),
                f.north(tw, th, wallDY), f.north(wallDX, th, wallDY));
    }
    if (isoType == VirtualTile::WallSE) {
        int dy = 1; // hack for west walls (see PBufferFace::west())
        de.add(textureID,
               tt.uv(tw - wallDY, 0), tt.uv(tw, 0),
               tt.uv(tw, th), tt.uv(tw - wallDY, th),
               f.west(tw - wallDY + dy, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, th, wallDX), f.west(tw - wallDY + dy, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(0, 0), tt.uv(wallDX, 0),
                tt.uv(wallDX, th), tt.uv(0, th),
                f.north(0, 0, wallDY), f.north(wallDX, 0, wallDY),
                f.north(wallDX, th, wallDY), f.north(0, th, wallDY));
    }
    int topHgt = 21, botHgt = 37;
    if (isoType == VirtualTile::WallWindowW) {
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, topHgt, wallDX), f.west(0, topHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, th - botHgt, wallDX), f.west(tw, th - botHgt, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.west(0, topHgt, wallDX), f.west(side, topHgt, wallDX),
               f.west(side, th - botHgt, wallDX), f.west(0, th - botHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.west(tw - side, topHgt, wallDX), f.west(tw, topHgt, wallDX),
               f.west(tw, th - botHgt, wallDX), f.west(tw - side, th - botHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::WallWindowN) {
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
               f.north(tw, topHgt, wallDY), f.north(0, topHgt, wallDY));
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.north(0, th - botHgt, wallDY), f.north(tw, th - botHgt, wallDY),
               f.north(tw, th, wallDY), f.north(0, th, wallDY));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.north(0, topHgt, wallDY), f.north(side, topHgt, wallDY),
               f.north(side, th - botHgt, wallDY), f.north(0, th - botHgt, wallDY));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.north(tw - side, topHgt, wallDY), f.north(tw, topHgt, wallDY),
               f.north(tw, th - botHgt, wallDY), f.north(tw - side, th - botHgt, wallDY));
    }
    if (isoType == VirtualTile::WallDoorW) {
        int side = 4;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallDX), f.west(tw, 0, wallDX),
               f.west(tw, topHgt, wallDX), f.west(0, topHgt, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.west(0, topHgt, wallDX), f.west(side, topHgt, wallDX),
               f.west(side, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.west(tw - side, topHgt, wallDX), f.west(tw, topHgt, wallDX),
               f.west(tw, th, wallDX), f.west(tw - side, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::WallDoorN) {
        int side = 4;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallDY), f.north(tw, 0, wallDY),
               f.north(tw, topHgt, wallDY), f.north(0, topHgt, wallDY));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.north(0, topHgt, wallDY), f.north(side, topHgt, wallDY),
               f.north(side, th, wallDY), f.north(0, th, wallDY));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.north(tw - side, topHgt, wallDY), f.north(tw, topHgt, wallDY),
               f.north(tw, th, wallDY), f.north(tw - side, th, wallDY));
    }
    const int shortWallHgt = 57;
    if (isoType == VirtualTile::WallShortW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.west(0, th - shortWallHgt, wallDX), f.west(tw, th - shortWallHgt, wallDX),
               f.west(tw, th, wallDX), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::WallShortN) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.north(0, th - shortWallHgt, wallDY), f.north(tw, th - shortWallHgt, wallDY),
               f.north(tw, th, wallDY), f.north(0, th, wallDY));
    }
    if (isoType == VirtualTile::WallShortNW) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.west(0, th - shortWallHgt, wallDX), f.west(tw - wallDY, th - shortWallHgt, wallDX, 0),
               f.west(tw - wallDY, th, wallDX, 0), f.west(0, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, shortWallHgt), tt.uv(0, shortWallHgt),
               f.north(wallDX, th - shortWallHgt, wallDY), f.north(tw, th - shortWallHgt, wallDY),
               f.north(tw, th, wallDY), f.north(wallDX, th, wallDY));
    }
    if (isoType == VirtualTile::WallShortSE) {
        int dy = 1; // hack for west walls (see PBufferFace::west())
        de.add(textureID,
               tt.uv(tw - wallDY, th - shortWallHgt), tt.uv(tw, th - shortWallHgt),
               tt.uv(tw, th), tt.uv(tw - wallDY, th),
               f.west(tw - wallDY + dy, th - shortWallHgt, wallDX), f.west(tw, th - shortWallHgt, wallDX),
               f.west(tw, th, wallDX), f.west(tw - wallDY + dy, th, wallDX));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(0, th - shortWallHgt), tt.uv(wallDX, th - shortWallHgt),
                tt.uv(wallDX, th), tt.uv(0, th),
                f.north(0, th - shortWallHgt, wallDY), f.north(wallDX, th - shortWallHgt, wallDY),
                f.north(wallDX, th, wallDY), f.north(0, th, wallDY));
    }

    //
    // ROOFS
    //
    if (isoType >= VirtualTile::SlopeS1 && isoType <= VirtualTile::SlopeS3) {
        int dy = (isoType - VirtualTile::SlopeS1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (isoType >= VirtualTile::SlopeE1 && isoType <= VirtualTile::SlopeE3) {
        int dy = (isoType - VirtualTile::SlopeE1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatWE(0, 0, 32 + dy), f.flatWE(tw, 0, 32 + dy), f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::SlopePt5S && isoType <= VirtualTile::SlopeTwoPt5S) {
        int dy = (isoType - VirtualTile::SlopePt5S) * 32;
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, th / 2, 16 + dy), f.flatNS(tw, th / 2, 16 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (isoType >= VirtualTile::SlopePt5E && isoType <= VirtualTile::SlopeTwoPt5E) {
        int dy = (isoType - VirtualTile::SlopePt5E) * 32;
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatWE(0, th / 2, 16 + dy), f.flatWE(tw, th / 2, 16 + dy), f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::Outer1 && isoType <= VirtualTile::Outer3) {
        int dy = (isoType - VirtualTile::Outer1) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
        // east
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::Inner1 && isoType <= VirtualTile::Inner3) {
        int dy = (isoType - VirtualTile::Inner1) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64),
               f.flatNS(0, 0, 32 + dy), f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy));
        // east
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(0, 64),
               f.flatNS(0, th, 32 + dy), f.flatNS(0, 0, 32 + dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::OuterPt5 && isoType <= VirtualTile::OuterTwoPt5) {
        int dy = (isoType - VirtualTile::OuterPt5) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw / 2, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(tw / 2, th, 32 + dy), f.flatNS(0, 0, dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
        // east
        de.add(textureID,
               tt.uv(tw / 2, 32), tt.uv(tw, 32), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, 0, dy), f.flatNS(tw, th / 2, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::InnerPt5 && isoType <= VirtualTile::InnerTwoPt5) {
        int dy = (isoType - VirtualTile::InnerPt5) * 32;
        // south
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw / 2, 32), tt.uv(tw, 64),
               f.flatNS(0, 0, dy), f.flatNS(tw / 2, 0, dy), f.flatNS(tw, th, dy));
        // east
        de.add(textureID,
               tt.uv(0, 32), tt.uv(tw, 32), tt.uv(tw, 64),
               f.flatNS(0, th / 2, dy), f.flatNS(0, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::CornerSW1 && isoType <= VirtualTile::CornerSW3) {
        int dy = (isoType - VirtualTile::CornerSW1) * 32;
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(tw, 0, 32 + dy), f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (isoType >= VirtualTile::CornerNE1 && isoType <= VirtualTile::CornerNE3) {
        int dy = (isoType - VirtualTile::CornerNE1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64), tt.uv(0, 64),
               f.flatNS(0, th, 32 + dy), f.flatNS(tw, 0, dy), f.flatNS(tw, th, dy));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType >= VirtualTile::RoofTopN1 && isoType <= VirtualTile::RoofTopN3) {
        int dy = (isoType - VirtualTile::RoofTopN1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, dy), f.flatNS(tw, 0, dy),
               f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (isoType >= VirtualTile::RoofTopW1 && isoType <= VirtualTile::RoofTopW3) {
        int dy = (isoType - VirtualTile::RoofTopW1) * 32;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, dy), f.flatWE(tw, 0, dy),
               f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
    }

    int shallowRise = 8;
    if (isoType >= VirtualTile::ShallowSlopeN1 && isoType <= VirtualTile::ShallowSlopeN2) {
        int dy = 10 + (isoType - VirtualTile::ShallowSlopeN1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, dy), f.flatNS(tw, 0, dy),
               f.flatNS(tw, th, shallowRise + dy), f.flatNS(0, th, shallowRise + dy));
    }
    if (isoType >= VirtualTile::ShallowSlopeS1 && isoType <= VirtualTile::ShallowSlopeS2) {
        int dy = 10 + (isoType - VirtualTile::ShallowSlopeS1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatNS(0, 0, shallowRise + dy), f.flatNS(tw, 0, shallowRise + dy),
               f.flatNS(tw, th, dy), f.flatNS(0, th, dy));
    }
    if (isoType >= VirtualTile::ShallowSlopeW1 && isoType <= VirtualTile::ShallowSlopeW2) {
        int dy = 10 + (isoType - VirtualTile::ShallowSlopeW1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, dy), f.flatWE(tw, 0, dy),
               f.flatWE(tw, th, shallowRise + dy), f.flatWE(0, th, shallowRise + dy));
    }
    if (isoType >= VirtualTile::ShallowSlopeE1 && isoType <= VirtualTile::ShallowSlopeE2) {
        int dy = 10 + (isoType - VirtualTile::ShallowSlopeE1) * shallowRise;
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, 32), tt.uv(0, 32),
               f.flatWE(0, 0, shallowRise + dy), f.flatWE(tw, 0, shallowRise + dy),
               f.flatWE(tw, th, dy), f.flatWE(0, th, dy));
    }

    if (isoType == VirtualTile::TrimS) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3.0), tt.uv(0, 64/3.0),
               f.flatNS(0, 0, 96), f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3.0, 96-(32/3.0)), f.flatNS(0, th / 3.0, 96-(32/3.0)));
    }
    if (isoType == VirtualTile::TrimE) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3.0), tt.uv(0, 64/3.0),
               f.flatWE(0, 0, 96), f.flatWE(tw, 0, 96), f.flatWE(tw, th / 3.0, 96-(32/3.0)), f.flatWE(0, th / 3, 96-(32/3.0)));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::TrimOuter) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64/3.0), tt.uv(0, 64/3.0),
               f.flatNS(0, 0, 96), f.flatNS((tw * 2) / 3.0, (th * 2) / 3.0, 96), f.flatNS(0, th / 3.0, 96-(32/3.0)));
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 64/3.0), tt.uv(0, 64/3.0),
               f.flatNS(0, 0, 96), f.flatNS(tw / 3.0, 0, 96-(32/3.0)), f.flatNS((tw * 2) / 3.0, (th * 2) / 3.0, 96));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::TrimInner) {
        // south
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw, 64/3.0), tt.uv(64 / 3, 64/3.0),
               f.flatNS(0, 0, 96), f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3.0, 96-(32/3.0)), f.flatNS((tw * 2) / 3.0, (th * 2) / 3.0, 96));
        // east
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0), tt.uv(tw/3, 64/3.0), tt.uv(0, 64/3.0),
               f.flatWE(0, 0, 96), f.flatWE(tw, 0, 96), f.flatWE(tw / 3.0, (th * 2) / 3.0, 96), f.flatWE(0, th / 3, 96-(32/3.0)));
        de.color(0.8f,0.8f,0.8f);
    }
    if (isoType == VirtualTile::TrimCornerSW) {
        de.add(textureID,
               tt.uv(tw, 0), tt.uv(tw - 64/3.0, 64/3.0), tt.uv(0, 64/3.0),
               f.flatNS(tw, 0, 96), f.flatNS(tw, th / 3.0, 96-(32/3.0)), f.flatNS(tw, th * 2 / 3.0, 96));
    }
    if (isoType == VirtualTile::TrimCornerNE) {
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw / 3, 64/3.0), tt.uv(0, 64/3.0),
               f.flatNS(0, th, 96), f.flatNS(tw * 2 / 3.0, th, 96), f.flatNS(tw / 3.0, th, 96-(32/3.0)));
        de.color(0.8f,0.8f,0.8f);
    }

    TileShape *shape = new TileShape;

    int ind = 0;
    for (int i = 0; i < de.textureids.size(); i++) {
        QPolygonF scenePoly;
        QPolygonF uvPoly;
        if (de.counts[i] == 4) {
            scenePoly << de.vertices[ind].toPointF() << de.vertices[ind+1].toPointF() << de.vertices[ind+2].toPointF() << de.vertices[ind+3].toPointF();
            uvPoly << de.texcoords[ind].toPointF() << de.texcoords[ind+1].toPointF() << de.texcoords[ind+2].toPointF() << de.texcoords[ind+3].toPointF();
        } else if (de.counts[i] == 3) {
            scenePoly << de.vertices[ind].toPointF() << de.vertices[ind+1].toPointF() << de.vertices[ind+2].toPointF();
            uvPoly << de.texcoords[ind].toPointF() << de.texcoords[ind+1].toPointF() << de.texcoords[ind+2].toPointF();
        }
        ret += scenePoly;
        ind += de.counts[i];

        TileShape::Element e;
        e.mGeom = TileShapeScene::toTile(scenePoly);
        e.mUV = uvPoly;
        shape->mElements += e;

    }
    return shape;
}

/////

#include "BuildingEditor/simplefile.h"

#define VERSION0 0

#define VERSION_LATEST VERSION0

VirtualTilesetsFile::VirtualTilesetsFile() :
    mVersion(0),
    mRevision(0),
    mSourceRevision(0)
{
    mNameToType[QLatin1String("floor")] = VirtualTile::Floor;

    mNameToType[QLatin1String("wall_w")] = VirtualTile::WallW;
    mNameToType[QLatin1String("wall_n")] = VirtualTile::WallN;
    mNameToType[QLatin1String("wall_nw")] = VirtualTile::WallNW;
    mNameToType[QLatin1String("wall_se")] = VirtualTile::WallSE;
    mNameToType[QLatin1String("wall_window_w")] = VirtualTile::WallWindowW;
    mNameToType[QLatin1String("wall_window_n")] = VirtualTile::WallWindowN;
    mNameToType[QLatin1String("wall_door_w")] = VirtualTile::WallDoorW;
    mNameToType[QLatin1String("wall_door_n")] = VirtualTile::WallDoorN;

    mNameToType[QLatin1String("wall_short_w")] = VirtualTile::WallShortW;
    mNameToType[QLatin1String("wall_short_n")] = VirtualTile::WallShortN;
    mNameToType[QLatin1String("wall_short_nw")] = VirtualTile::WallShortNW;
    mNameToType[QLatin1String("wall_short_se")] = VirtualTile::WallShortSE;

    mNameToType[QLatin1String("roof_slope_s1")] = VirtualTile::SlopeS1;
    mNameToType[QLatin1String("roof_slope_s2")] = VirtualTile::SlopeS2;
    mNameToType[QLatin1String("roof_slope_s3")] = VirtualTile::SlopeS3;
    mNameToType[QLatin1String("roof_slope_e1")] = VirtualTile::SlopeE1;
    mNameToType[QLatin1String("roof_slope_e2")] = VirtualTile::SlopeE2;
    mNameToType[QLatin1String("roof_slope_e3")] = VirtualTile::SlopeE3;

    mNameToType[QLatin1String("roof_shallow_n1")] = VirtualTile::ShallowSlopeN1;
    mNameToType[QLatin1String("roof_shallow_n2")] = VirtualTile::ShallowSlopeN2;
    mNameToType[QLatin1String("roof_shallow_s1")] = VirtualTile::ShallowSlopeS1;
    mNameToType[QLatin1String("roof_shallow_s2")] = VirtualTile::ShallowSlopeS2;
    mNameToType[QLatin1String("roof_shallow_w1")] = VirtualTile::ShallowSlopeW1;
    mNameToType[QLatin1String("roof_shallow_w2")] = VirtualTile::ShallowSlopeW2;
    mNameToType[QLatin1String("roof_shallow_e1")] = VirtualTile::ShallowSlopeE1;
    mNameToType[QLatin1String("roof_shallow_e2")] = VirtualTile::ShallowSlopeE2;

    mNameToType[QLatin1String("roof_slope_pt5s")] = VirtualTile::SlopePt5S;
    mNameToType[QLatin1String("roof_slope_1pt5s")] = VirtualTile::SlopeOnePt5S;
    mNameToType[QLatin1String("roof_slope_2pt5s")] = VirtualTile::SlopeTwoPt5S;
    mNameToType[QLatin1String("roof_slope_pt5e")] = VirtualTile::SlopePt5E;
    mNameToType[QLatin1String("roof_slope_1pt5e")] = VirtualTile::SlopeOnePt5E;
    mNameToType[QLatin1String("roof_slope_2pt5e")] = VirtualTile::SlopeTwoPt5E;

    mNameToType[QLatin1String("roof_trim_inner")] = VirtualTile::TrimInner;
    mNameToType[QLatin1String("roof_trim_outer")] = VirtualTile::TrimOuter;
    mNameToType[QLatin1String("roof_trim_s")] = VirtualTile::TrimS;
    mNameToType[QLatin1String("roof_trim_e")] = VirtualTile::TrimE;
    mNameToType[QLatin1String("roof_trim_corner_sw")] = VirtualTile::TrimCornerSW;
    mNameToType[QLatin1String("roof_trim_corner_ne")] = VirtualTile::TrimCornerNE;

    mNameToType[QLatin1String("roof_inner1")] = VirtualTile::Inner1;
    mNameToType[QLatin1String("roof_inner2")] = VirtualTile::Inner2;
    mNameToType[QLatin1String("roof_inner3")] = VirtualTile::Inner3;
    mNameToType[QLatin1String("roof_outer1")] = VirtualTile::Outer1;
    mNameToType[QLatin1String("roof_outer2")] = VirtualTile::Outer2;
    mNameToType[QLatin1String("roof_outer3")] = VirtualTile::Outer3;

    mNameToType[QLatin1String("roof_inner_pt5")] = VirtualTile::InnerPt5;
    mNameToType[QLatin1String("roof_inner_1pt5")] = VirtualTile::InnerOnePt5;
    mNameToType[QLatin1String("roof_inner_2pt5")] = VirtualTile::InnerTwoPt5;
    mNameToType[QLatin1String("roof_outer_p5")] = VirtualTile::OuterPt5;
    mNameToType[QLatin1String("roof_outer_1pt5")] = VirtualTile::OuterOnePt5;
    mNameToType[QLatin1String("roof_outer_2pt5")] = VirtualTile::OuterTwoPt5;

    mNameToType[QLatin1String("roof_corner_sw1")] = VirtualTile::CornerSW1;
    mNameToType[QLatin1String("roof_corner_sw2")] = VirtualTile::CornerSW2;
    mNameToType[QLatin1String("roof_corner_sw3")] = VirtualTile::CornerSW3;
    mNameToType[QLatin1String("roof_corner_ne1")] = VirtualTile::CornerNE1;
    mNameToType[QLatin1String("roof_corner_ne2")] = VirtualTile::CornerNE2;
    mNameToType[QLatin1String("roof_corner_ne3")] = VirtualTile::CornerNE3;

    mNameToType[QLatin1String("roof_top_n1")] = VirtualTile::RoofTopN1;
    mNameToType[QLatin1String("roof_top_n2")] = VirtualTile::RoofTopN2;
    mNameToType[QLatin1String("roof_top_n3")] = VirtualTile::RoofTopN3;
    mNameToType[QLatin1String("roof_top_w1")] = VirtualTile::RoofTopW1;
    mNameToType[QLatin1String("roof_top_w2")] = VirtualTile::RoofTopW2;
    mNameToType[QLatin1String("roof_top_w3")] = VirtualTile::RoofTopW3;

    foreach (QString name, mNameToType.keys())
        mTypeToName[mNameToType[name]] = name;
}

VirtualTilesetsFile::~VirtualTilesetsFile()
{
    qDeleteAll(mTilesets);
}

bool VirtualTilesetsFile::read(const QString &fileName)
{
    QFileInfo info(fileName);
    if (!info.exists()) {
        mError = tr("The %1 file doesn't exist.").arg(fileName);
        return false;
    }

    QString path = info.absoluteFilePath();
    SimpleFile simple;
    if (!simple.read(path)) {
        mError = tr("Error reading %1.\n%2").arg(path).arg(simple.errorString());
        return false;
    }

    mVersion = simple.version();
    mRevision = simple.value("revision").toInt();
    mSourceRevision = simple.value("source_revision").toInt();

    if (mVersion > VERSION_LATEST) {
        mError = tr("%1 is from a newer version of TileZed").arg(info.fileName());
        return false;
    }

    qDeleteAll(mTilesets);
    mTilesets.clear();

    foreach (SimpleFileBlock block, simple.blocks) {
        if (block.name == QLatin1String("tileset")) {
            QString name = block.value("name");
            if (name.isEmpty()) {
                mError = tr("Line %1: Missing tileset name")
                        .arg(block.lineNumber);
                return false;
            }
            if (mTilesetByName.contains(name)) {
                mError = tr("Line %1: Duplicate tileset name '%2'")
                        .arg(block.lineNumber)
                        .arg(name);
                return false;
            }
            int columnCount, rowCount;
            if (!parse2Ints(block.value("size"), &columnCount, &rowCount)) {
                mError = tr("Line %1: Invalid 'size' attribute").arg(block.lineNumber);
                return false;
            }
            VirtualTileset *vts = new VirtualTileset(name, columnCount, rowCount);

            foreach (SimpleFileBlock block2, block.blocks) {
                if (block2.name == QLatin1String("tile")) {
                    // FIXME: The Tiles Directory could be unset/invalid here
                    QString file = block2.value("file");
#if 0
                    if (QDir::isRelativePath(file))
                        file = Preferences::instance()->tilesDirectory() + QLatin1String("/") + file;
#endif
                    int x, y;
                    if (!parse2Ints(block2.value("xy"), &x, &y) ||
                            !QRect(0, 0, columnCount, rowCount).contains(x, y)) {
                        mError = tr("Line %1: Invalid 'xy' attribute").arg(block2.lineNumber);
                        return false;
                    }
                    int srcX, srcY;
                    if (!parse2Ints(block2.value("file-xy"), &srcX, &srcY)) {
                        mError = tr("Line %1: Invalid 'file-xy' attribute").arg(block2.lineNumber);
                        return false;
                    }
                    QString shape = block2.value("shape");
                    VirtualTile::IsoType isoType = VirtualTile::Invalid;
                    if (!parseIsoType(shape, isoType)) {
                        mError = tr("Line %1: Invalid 'shape' attribute '%2'.")
                                .arg(block2.lineNumber)
                                .arg(shape);
                        return false;
                    }
                    vts->tileAt(x, y)->setImageSource(file, srcX, srcY);
                    vts->tileAt(x, y)->setType(isoType);
                } else {
                    mError = tr("Line %1: Unknown block name '%1'\n%2")
                            .arg(block2.lineNumber)
                            .arg(block2.name)
                            .arg(path);
                    return false;
                }
            }
            addTileset(vts);
        } else {
            mError = tr("Line %1: Unknown block name '%1'.\n%2")
                    .arg(block.lineNumber)
                    .arg(block.name)
                    .arg(path);
            return false;
        }
    }

    return true;
}

bool VirtualTilesetsFile::write(const QString &fileName)
{
    return write(fileName, mTilesets);
}

bool VirtualTilesetsFile::write(const QString &fileName,
                                const QList<VirtualTileset *> &tilesets)
{
    SimpleFile simpleFile;

    QDir tilesDir(Preferences::instance()->tilesDirectory());

    foreach (VirtualTileset *vts, tilesets) {
        SimpleFileBlock block;
        block.name = QLatin1String("tileset");
        block.addValue("name", vts->name());
        block.addValue("size", QString::fromLatin1("%1,%2")
                       .arg(vts->columnCount())
                       .arg(vts->rowCount()));
        for (int i = 0; i < vts->tileCount(); i++) {
            VirtualTile *vtile = vts->tileAt(i);
            if (vtile->type() == VirtualTile::Invalid)
                continue;
            SimpleFileBlock block2;
            block2.name = QLatin1String("tile");
            block2.addValue("xy", QString::fromLatin1("%1,%2")
                           .arg(vtile->x())
                           .arg(vtile->y()));
            QString relativePath = tilesDir.relativeFilePath(vtile->imageSource());
            block2.addValue("file", relativePath);
            block2.addValue("file-xy", QString::fromLatin1("%1,%2")
                            .arg(vtile->srcX())
                            .arg(vtile->srcY()));
            block2.addValue("shape", mTypeToName[vtile->type()]);
            block.blocks += block2;
        }

        simpleFile.blocks += block;
    }

    simpleFile.setVersion(VERSION_LATEST);
    simpleFile.replaceValue("revision", QString::number(mRevision));
    simpleFile.replaceValue("source_revision", QString::number(mSourceRevision));
    if (!simpleFile.write(fileName)) {
        mError = simpleFile.errorString();
        return false;
    }

    return true;
}

bool VirtualTilesetsFile::parse2Ints(const QString &s, int *pa, int *pb)
{
    QStringList coords = s.split(QLatin1Char(','), QString::SkipEmptyParts);
    if (coords.size() != 2)
        return false;
    bool ok;
    int a = coords[0].toInt(&ok);
    if (!ok) return false;
    int b = coords[1].toInt(&ok);
    if (!ok) return false;
    *pa = a, *pb = b;
    return true;
}

bool VirtualTilesetsFile::parseIsoType(const QString &s, VirtualTile::IsoType &isoType)
{
    if (mNameToType.contains(s)) {
        isoType = mNameToType[s];
        return true;
    }
    return false;
}
