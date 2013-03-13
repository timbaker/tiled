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
#include <QUndoCommand>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace Tiled {
namespace Internal {

class PaintBMP : public QUndoCommand
{
public:
    PaintBMP(MapDocument *mapDocument, const QPoint &pos, int bmpIndex, QRgb color);
//    ~PaintBMP();

    void setMergeable(bool mergeable)
    { mMergeable = mergeable; }

    void undo() { swap(); }
    void redo() { swap(); }

    void swap();

    int id() const { return Cmd_PaintBMP; }
    bool mergeWith(const QUndoCommand *other);

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    QImage mImage;
    QRegion mRegion;
    bool mMergeable;
};

PaintBMP::PaintBMP(MapDocument *mapDocument, const QPoint &pos, int bmpIndex, QRgb color) :
    QUndoCommand(QCoreApplication::translate("UndoCommands", "Paint BMP")),
    mMapDocument(mapDocument),
    mBmpIndex(bmpIndex),
    mMergeable(false)
{
    mImage = QImage(mapDocument->map()->size(), QImage::Format_ARGB32);
    mImage.fill(Qt::black);
    mImage.setPixel(pos, color);
    mRegion = QRect(pos, QSize(1, 1));
}

void PaintBMP::swap()
{
    QImage *image = 0;
    if (mBmpIndex == 0)
        image = &mMapDocument->map()->bmpMain();
    if (mBmpIndex == 1)
        image = &mMapDocument->map()->bmpVeg();
    if (!image)
        return;

    QImage old = QImage(mImage.size(), QImage::Format_ARGB32);
    old.fill(Qt::black);

    const QRect r = mRegion.boundingRect();
    for (int y = r.top(); y <= r.bottom(); y++) {
        for (int x = r.left(); x <= r.right(); x++) {
            if (mRegion.contains(QPoint(x, y))) {
                old.setPixel(x, y, image->pixel(x, y));
                image->setPixel(x, y, mImage.pixel(x, y));
            }
        }
    }

    mImage = old;

    mMapDocument->bmpBlender()->imagesToTileNames(r.left(), r.top(), r.right(), r.bottom());
    mMapDocument->bmpBlender()->blend(r.left() - 1, r.top() - 1, r.right() + 1, r.bottom() + 1);
    mMapDocument->bmpBlender()->tileNamesToLayers(r.left() - 1, r.top() - 1, r.right() + 1, r.bottom() + 1);
    mMapDocument->mapComposite()->tileLayersForLevel(0)->setBmpBlendLayers(
                mMapDocument->bmpBlender()->mTileLayers.values());

    foreach (QString layerName, mMapDocument->bmpBlender()->mTileLayers.keys()) {
        int index = mMapDocument->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index == -1)
            continue;
        mMapDocument->emitRegionAltered(mRegion, mMapDocument->map()->layerAt(index)->asTileLayer());
    }
}

bool PaintBMP::mergeWith(const QUndoCommand *other)
{
    const PaintBMP *o = static_cast<const PaintBMP*>(other);
    if (!(mMapDocument == o->mMapDocument &&
          mBmpIndex == o->mBmpIndex &&
          o->mMergeable))
        return false;

    const QRegion combinedRegion = mRegion.united(o->mRegion);
    if (mRegion != combinedRegion) {
        const QRect r = o->mRegion.boundingRect();
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (o->mRegion.contains(QPoint(x, y)) /*&& !mRegion.contains(QPoint(x, y))*/) {
                    mImage.setPixel(x, y, o->mImage.pixel(x, y));
                }
            }
        }
        mRegion = combinedRegion;
    }

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
        paint(false);
    }
}

void BmpTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mPainting = false;
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
    brushItem()->setTileRegion(QRect(tilePos, QSize(1, 1)));

    if (mPainting)
        paint(true);
}

void BmpTool::paint(bool mergeable)
{
    TileLayer *tileLayer = currentTileLayer();
    const QPoint tilePos = tilePosition();

    if (!tileLayer->bounds().contains(tilePos))
        return;

#if 1
    PaintBMP *cmd = new PaintBMP(mapDocument(), tilePos, mBmpIndex, mColor);
    cmd->setMergeable(mergeable);
    mapDocument()->undoStack()->push(cmd);
#else
    if (mBmpIndex == 0)
        mapDocument()->map()->bmpMain().setPixel(tilePos, mColor);
    if (mBmpIndex == 1)
        mapDocument()->map()->bmpVeg().setPixel(tilePos, mColor);

    int x1 = tilePos.x()-1, x2 = tilePos.x()+1;
    int y1 = tilePos.y()-1, y2 = tilePos.y()+1;
    mapDocument()->bmpBlender()->imagesToTileNames(x1+1, y1+1, x2-1, y2-1);
    mapDocument()->bmpBlender()->blend(x1, y1, x2, y2);
    mapDocument()->bmpBlender()->tileNamesToLayers(x1, y1, x2, y2);
    mapDocument()->mapComposite()->tileLayersForLevel(0)->setBmpBlendLayers(
                mapDocument()->bmpBlender()->mTileLayers.values());
    foreach (QString layerName, mapDocument()->bmpBlender()->mTileLayers.keys()) {
        int index = mapDocument()->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index == -1)
            continue;
        mapDocument()->emitRegionAltered(QRegion(QRect(tilePos.x()-1,tilePos.y()-1,3,3)),
                                         mapDocument()->map()->layerAt(index)->asTileLayer());
    }
#endif
}
