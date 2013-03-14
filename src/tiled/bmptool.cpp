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

#include "bmptool.h"

#include "bmpblender.h"
#include "bmptooldialog.h"
#include "brushitem.h"
#include "mapcomposite.h"
#include "mainwindow.h"
#include "mapscene.h"
#include "undocommands.h"

#include "map.h"

#include <QCoreApplication>
#include <QDebug>
#include <QPainter>
#include <QUndoCommand>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace Tiled {
namespace Internal {

// This is a QImage with resize() and merge() methods mirroring those of
// TileLayer.
class ResizableImage : public QImage
{
public:
    ResizableImage() :
        QImage()
    {

    }

    ResizableImage(const QSize &size) :
        QImage(size, QImage::Format_ARGB32)
    {

    }

    ResizableImage(const QImage &image) :
        QImage(image)
    {
    }

    // This is like TileLayer::resize().
    void resize(const QSize &size, const QPoint &offset)
    {
        QImage newImage(size, QImage::Format_ARGB32);
        newImage.fill(Qt::black);

        // Copy over the preserved part
        const int startX = qMax(0, -offset.x());
        const int startY = qMax(0, -offset.y());
        const int endX = qMin(width(), size.width() - offset.x());
        const int endY = qMin(height(), size.height() - offset.y());

        for (int y = startY; y < endY; ++y) {
            for (int x = startX; x < endX; ++x) {
                newImage.setPixel(x + offset.x(), y + offset.y(), pixel(x, y));
            }
        }

        *this = newImage;
    }

    // This is like TileLayer::merge().
    void merge(const QPoint &pos, const ResizableImage *other)
    {
        // Determine the overlapping area
        QRect area = QRect(pos, QSize(other->width(), other->height()));
        area &= QRect(0, 0, width(), height());

        for (int y = area.top(); y <= area.bottom(); ++y) {
            for (int x = area.left(); x <= area.right(); ++x) {
                setPixel(x, y, other->pixel(x - area.left(), y - area.top()));
            }
        }
    }
};

// This is based on PaintTileLayer.
class PaintBMP : public QUndoCommand
{
public:
    PaintBMP(MapDocument *mapDocument, int bmpIndex, int x, int y,
             QImage &source, QRegion &region);
//    ~PaintBMP();

    void setMergeable(bool mergeable)
    { mMergeable = mergeable; }

    void undo() { paint(mErased); }
    void redo() { paint(mSource); }

    void paint(const ResizableImage &source);

    int id() const { return Cmd_PaintBMP; }
    bool mergeWith(const QUndoCommand *other);

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    ResizableImage mSource;
    ResizableImage mErased;
    int mX;
    int mY;
    QRegion mRegion;
    bool mMergeable;
};

PaintBMP::PaintBMP(MapDocument *mapDocument, int bmpIndex,
                   int x, int y, QImage &source, QRegion &region) :
    QUndoCommand(QCoreApplication::translate("UndoCommands", "Paint BMP")),
    mMapDocument(mapDocument),
    mBmpIndex(bmpIndex),
    mSource(source),
    mX(x),
    mY(y),
    mRegion(region),
    mMergeable(false)
{
    QImage *image = 0;
    if (mBmpIndex == 0)
        image = &mMapDocument->map()->rbmpMain();
    if (mBmpIndex == 1)
        image = &mMapDocument->map()->rbmpVeg();
    if (!image)
        return;
    mErased = image->copy(mX, mY, mSource.width(), mSource.height());
}

void PaintBMP::paint(const ResizableImage &source)
{
    QImage *image = 0;
    if (mBmpIndex == 0)
        image = &mMapDocument->map()->rbmpMain();
    if (mBmpIndex == 1)
        image = &mMapDocument->map()->rbmpVeg();
    if (!image)
        return;

    QRegion region = mRegion & QRect(0, 0, image->width(), image->height());

    foreach (QRect r, region.rects()) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                image->setPixel(x, y, source.pixel(x - mX, y - mY));
            }
        }
    }

    const QRect r = region.boundingRect();
    mMapDocument->bmpBlender()->imagesToTileNames(r.left(), r.top(), r.right(), r.bottom());
    mMapDocument->bmpBlender()->blend(r.left() - 1, r.top() - 1, r.right() + 1, r.bottom() + 1);
    mMapDocument->bmpBlender()->tileNamesToLayers(r.left() - 1, r.top() - 1, r.right() + 1, r.bottom() + 1);
    // FIXME: tileLayersForLevel(0) may be 0
    mMapDocument->mapComposite()->tileLayersForLevel(0)->setBmpBlendLayers(
                mMapDocument->bmpBlender()->mTileLayers.values());

    foreach (QString layerName, mMapDocument->bmpBlender()->mTileLayers.keys()) {
        int index = mMapDocument->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index == -1)
            continue;
        mMapDocument->emitRegionAltered(region, mMapDocument->map()->layerAt(index)->asTileLayer());
    }
}

bool PaintBMP::mergeWith(const QUndoCommand *other)
{
    const PaintBMP *o = static_cast<const PaintBMP*>(other);
    if (!(mMapDocument == o->mMapDocument &&
          mBmpIndex == o->mBmpIndex &&
          o->mMergeable))
        return false;

    const QRegion newRegion = o->mRegion.subtracted(mRegion);
    const QRegion combinedRegion = mRegion.united(o->mRegion);
    const QRect bounds = QRect(mX, mY, mSource.width(), mSource.height());
    const QRect combinedBounds = combinedRegion.boundingRect();

    // Resize the erased tiles and source layers when necessary
    if (bounds != combinedBounds) {
        const QPoint shift = bounds.topLeft() - combinedBounds.topLeft();
        mErased.resize(combinedBounds.size(), shift);
        mSource.resize(combinedBounds.size(), shift);
    }

    mX = combinedBounds.left();
    mY = combinedBounds.top();
    mRegion = combinedRegion;

    // Copy the painted tiles from the other command over
    const QPoint pos = QPoint(o->mX, o->mY) - combinedBounds.topLeft();
    mSource.merge(pos, &o->mSource);

    // Copy the newly erased tiles from the other command over
    foreach (const QRect &rect, newRegion.rects())
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
                mErased.setPixel(x - mX, y - mY,
                                 o->mErased.pixel(x - o->mX, y - o->mY));
    return true;
}

}
}

/////

BmpTool *BmpTool::mInstance = 0;

BmpTool *BmpTool::instance()
{
    if (!mInstance)
        mInstance = new BmpTool;
    return mInstance;
}

BmpTool::BmpTool(QObject *parent) :
    AbstractTileTool(tr("BMP Painter"),
                     QIcon(QLatin1String(
                             ":images/22x22/bmp-tool.png")),
                     QKeySequence(tr("P")),
                     parent),
    mPainting(false),
    mBmpIndex(0),
    mBrushSize(1),
    mDialog(0)
{
}

BmpTool::~BmpTool()
{
    delete mDialog;
}

void BmpTool::activate(MapScene *scene)
{
    AbstractTileTool::activate(scene);
    mDialog->show();
}

void BmpTool::deactivate(MapScene *scene)
{
    mDialog->hide();
    AbstractTileTool::deactivate(scene);
}

void BmpTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        mPainting = true;
        mErasing = (event->modifiers() & Qt::ControlModifier) != 0;
        paint(false);
    }
}

void BmpTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mPainting = false;
}

void BmpTool::setBrushSize(int size)
{
    mBrushSize = size;
    tilePositionChanged(tilePosition());
}

void BmpTool::mapDocumentChanged(MapDocument *oldDocument,
                                 MapDocument *newDocument)
{
    AbstractTileTool::mapDocumentChanged(oldDocument, newDocument);

    if (!mDialog)
        mDialog = new BmpToolDialog(MainWindow::instance());
    mDialog->setDocument(newDocument);
}

void BmpTool::languageChanged()
{
    setName(tr("BMP Painter"));
    setShortcut(QKeySequence(tr("P")));
}

void BmpTool::tilePositionChanged(const QPoint &tilePos)
{
    brushItem()->setTileRegion(QRect(tilePos - QPoint(mBrushSize/2, mBrushSize/2),
                                     QSize(mBrushSize, mBrushSize)));

    if (mPainting)
        paint(true);
}

void BmpTool::paint(bool mergeable)
{
    TileLayer *tileLayer = currentTileLayer();

    QRect r = brushItem()->tileRegion().boundingRect();

    if (!tileLayer->bounds().intersects(r))
        return;

    QPoint topLeft = r.topLeft();
    QImage image(r.size(), QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter p(&image);
    foreach (QRect r, brushItem()->tileRegion().rects())
        p.fillRect(r.translated(-topLeft), mErasing ? qRgb(0, 0, 0) : mColor);
    p.end();
    PaintBMP *cmd = new PaintBMP(mapDocument(), mBmpIndex,
                                 topLeft.x(), topLeft.y(), image,
                                 brushItem()->tileRegion());
    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
}
