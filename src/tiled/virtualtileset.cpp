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

#include <QGLPixelBuffer>
#include <QVector2D>
#include <QVector3D>

using namespace Tiled;
using namespace Internal;

VirtualTile::VirtualTile(int x, int y) :
    mX(x),
    mY(y),
    mSrcX(-1),
    mSrcY(-1),
    mType(Invalid)
{

}

VirtualTile::VirtualTile(int x, int y, const QString &imageSource, int srcX, int srcY, VirtualTile::IsoType type) :
    mImageSource(imageSource),
    mX(x),
    mY(y),
    mSrcX(srcX),
    mSrcY(srcY),
    mType(type)
{

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
            mTiles[y * columnCount + x] = new VirtualTile(x, y);
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

/////

SINGLETON_IMPL(VirtualTilesetMgr)

VirtualTilesetMgr::VirtualTilesetMgr()
{
    initPixelBuffer();

    VirtualTileset *vts = new VirtualTileset(QLatin1String("walls_exterior_house_01"), 8, 8);
    VirtualTile *vtile = vts->tileAt(0, 0);
    vtile->setImageSource(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\Tiles\\Textures\\tex_walls_exterior_house_01.png"), 0, 0);
    vtile->setType(VirtualTile::WallW);
    vtile->setImage(renderIsoTile(vtile));

    mTilesetByName[vts->name()] = vts;
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
    return true;
}

bool VirtualTilesetMgr::writeTxt()
{
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

    QVector2D uv(int x, int y)
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

    QVector3D flat(int x, int y, int z)
    {
        QPointF p = toScene(x, y);
        return QVector3D(p.x(), p.y() - z, 0);
    }

    QVector3D west(int x, int y, int dx)
    {
        QPointF p = toScene(dx / qreal(mTileWidth), (1 - x / qreal(mTileWidth)));
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
        indices.resize(0);
        vertices.resize(0);
        texcoords.resize(0);
        textureids.resize(0);
    }

    void add(GLuint textureid,
             const QVector2D &uv1, const QVector2D &uv2, const QVector2D &uv3, const QVector2D &uv4,
             const QVector3D &v1, const QVector3D &v2, const QVector3D &v3, const QVector3D &v4)
    {
        indices << indices.size() << indices.size() + 1 << indices.size() + 2 << indices.size() + 3;
        textureids += textureid;
        colors += QVector3D(1, 1, 1);
        texcoords << uv1 << uv2 << uv3 << uv4;
        vertices << v1 << v2 << v3 << v4;

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
        for (int i = 0; i < textureids.size(); i++) {
            glColor3f(colors[i].x(), colors[i].y(), colors[i].z());
            glBindTexture(GL_TEXTURE_2D, textureids[i]);
            glDrawElements(GL_QUADS, 4, GL_UNSIGNED_SHORT, indices.constData() + i * 4);
        }
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        clear();
#endif
    }

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

uint VirtualTilesetMgr::loadGLTexture(const QString &imageSource)
{
    QImage b(imageSource);
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

    GLuint textureID = loadGLTexture(vtile->imageSource());
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

    /////

    int tx = vtile->srcX(), ty = vtile->srcY(); // tile col/row
    int tw = 32, th = 96; // tile width/height
    int texWid = 32, texHgt = 96;
    int wallWidth = 3;
    TextureTile tt(texWid, texHgt, tw, th, tx, ty);
    PBufferFace f(mPixelBuffer, 0, 0, tw, th);
    DrawElements de;
    if (vtile->type() == VirtualTile::WallW) {
        // west wall
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, 0, wallWidth), f.west(tw, 0, wallWidth),
               f.west(tw, th, wallWidth), f.west(0, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallN) {
        // north wall
        de.add(textureID,
                tt.uv(0, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(0, th),
                f.north(0, 0, wallWidth), f.north(tw, 0, wallWidth),
                f.north(tw, th, wallWidth), f.north(0, th, wallWidth));
    }
    if (vtile->type() == VirtualTile::WallNW) {
        // nw corner
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw - wallWidth, 0),
               tt.uv(tw - wallWidth, th), tt.uv(0, th),
               f.west(0, 0, wallWidth), f.west(tw - wallWidth, 0, wallWidth),
               f.west(tw - wallWidth, th, wallWidth), f.west(0, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(wallWidth, 0), tt.uv(tw, 0),
                tt.uv(tw, th), tt.uv(wallWidth, th),
                f.north(wallWidth, 0, wallWidth), f.north(tw, 0, wallWidth),
                f.north(tw, th, wallWidth), f.north(wallWidth, th, wallWidth));
    }
    if (vtile->type() == VirtualTile::WallSE) {
        // se pillar
        de.add(textureID,
               tt.uv(tw - wallWidth, 0), tt.uv(wallWidth, 0),
               tt.uv(tw, th), tt.uv(tw - wallWidth, th),
               f.west(tw - wallWidth, 0, wallWidth), f.west(tw, 0, wallWidth),
               f.west(tw, th, wallWidth), f.west(tw - wallWidth, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        de.add(textureID,
                tt.uv(0, 0), tt.uv(wallWidth, 0),
                tt.uv(wallWidth, th), tt.uv(0, th),
                f.north(0, 0, wallWidth), f.north(wallWidth, 0, wallWidth),
                f.north(wallWidth, th, wallWidth), f.north(0, th, wallWidth));
    }
    int topHgt = th*0.2, botHgt = th*0.4;
    if (vtile->type() == VirtualTile::WallWindowW) {
        // west window
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallWidth), f.west(tw, 0, wallWidth),
               f.west(tw, topHgt, wallWidth), f.west(0, topHgt, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.west(0, th - botHgt, wallWidth), f.west(tw, th - botHgt, wallWidth),
               f.west(tw, th, wallWidth), f.west(0, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.west(0, topHgt, wallWidth), f.west(side, topHgt, wallWidth),
               f.west(side, th - botHgt, wallWidth), f.west(0, th - botHgt, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.west(tw - side, topHgt, wallWidth), f.west(tw, topHgt, wallWidth),
               f.west(tw, th - botHgt, wallWidth), f.west(tw - side, th - botHgt, wallWidth));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallWindowN) {
        // north window
        int side = 6;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallWidth), f.north(tw, 0, wallWidth),
               f.north(tw, topHgt, wallWidth), f.north(0, topHgt, wallWidth));
        // bottom
        de.add(textureID,
               tt.uv(0, th - botHgt), tt.uv(tw, th - botHgt),
               tt.uv(tw, th), tt.uv(0, th),
               f.north(0, th - botHgt, wallWidth), f.north(tw, th - botHgt, wallWidth),
               f.north(tw, th, wallWidth), f.north(0, th, wallWidth));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th - botHgt), tt.uv(0, th - botHgt),
               f.north(0, topHgt, wallWidth), f.north(side, topHgt, wallWidth),
               f.north(side, th - botHgt, wallWidth), f.north(0, th - botHgt, wallWidth));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th - botHgt), tt.uv(tw - side, th - botHgt),
               f.north(tw - side, topHgt, wallWidth), f.north(tw, topHgt, wallWidth),
               f.north(tw, th - botHgt, wallWidth), f.north(tw - side, th - botHgt, wallWidth));
    }
    if (vtile->type() == VirtualTile::WallDoorW) {
        // west door
        int side = 3;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.west(0, 0, wallWidth), f.west(tw, 0, wallWidth),
               f.west(tw, topHgt, wallWidth), f.west(0, topHgt, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.west(0, topHgt, wallWidth), f.west(side, topHgt, wallWidth),
               f.west(side, th, wallWidth), f.west(0, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.west(tw - side, topHgt, wallWidth), f.west(tw, topHgt, wallWidth),
               f.west(tw, th, wallWidth), f.west(tw - side, th, wallWidth));
        de.color(0.8f,0.8f,0.8f);
    }
    if (vtile->type() == VirtualTile::WallDoorN) {
        // north door
        int side = 3;
        // top
        de.add(textureID,
               tt.uv(0, 0), tt.uv(tw, 0),
               tt.uv(tw, topHgt), tt.uv(0, topHgt),
               f.north(0, 0, wallWidth), f.north(tw, 0, wallWidth),
               f.north(tw, topHgt, wallWidth), f.north(0, topHgt, wallWidth));
        // left
        de.add(textureID,
               tt.uv(0, topHgt), tt.uv(side, topHgt),
               tt.uv(side, th), tt.uv(0, th),
               f.north(0, topHgt, wallWidth), f.north(side, topHgt, wallWidth),
               f.north(side, th, wallWidth), f.north(0, th, wallWidth));
        // right
        de.add(textureID,
               tt.uv(tw - side, topHgt), tt.uv(tw, topHgt),
               tt.uv(tw, th), tt.uv(tw - side, th),
               f.north(tw - side, topHgt, wallWidth), f.north(tw, topHgt, wallWidth),
               f.north(tw, th, wallWidth), f.north(tw - side, th, wallWidth));
    }
#if 0
    de.add(textureID, QVector2D(0,0), QVector2D(1,0), QVector2D(1,1), QVector2D(0,1),
           QVector3D(0,0,0), QVector3D(32,0,0), QVector3D(32,96,0), QVector3D(0,96,0));
#endif
    de.flush();

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
