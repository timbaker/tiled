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
#include "bmpselectionitem.h"
#include "bmptooldialog.h"
#include "brushitem.h"
#include "mapcomposite.h"
#include "mainwindow.h"
#include "mapscene.h"
#include "undocommands.h"

#include "brushitem.h"
#include "map.h"
#include "maprenderer.h"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QUndoCommand>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace Tiled {
namespace Internal {

/////

AbstractBmpTool::AbstractBmpTool(const QString &name,
                                 const QIcon &icon,
                                 const QKeySequence &shortcut,
                                 QObject *parent)
    : AbstractTool(name, icon, shortcut, parent)
    , mBrushItem(new BrushItem)
    , mTileX(0), mTileY(0)
    , mBrushVisible(false)
{
    mBrushItem->setVisible(false);
    mBrushItem->setZValue(10000);
}

AbstractBmpTool::~AbstractBmpTool()
{
    delete mBrushItem;
}

void AbstractBmpTool::activate(MapScene *scene)
{
    mScene = scene;
    scene->addItem(mBrushItem);
    BmpToolDialog::instance()->setVisibleLater(true);
}

void AbstractBmpTool::deactivate(MapScene *scene)
{
    BmpToolDialog::instance()->setVisibleLater(false);
    mScene = 0;
    scene->removeItem(mBrushItem);
}

void AbstractBmpTool::mouseEntered()
{
    setBrushVisible(true);
}

void AbstractBmpTool::mouseLeft()
{
    setBrushVisible(false);
}

void AbstractBmpTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers)
{
    const MapRenderer *renderer = mapDocument()->renderer();
    Layer *layer = currentLayer();
    const QPointF tilePosF = renderer->pixelToTileCoords(pos, layer ? layer->level() : 0);
    QPoint tilePos;

    tilePos = QPoint((int) std::floor(tilePosF.x()),
                     (int) std::floor(tilePosF.y()));

    if (mTileX != tilePos.x() || mTileY != tilePos.y()) {
        mTileX = tilePos.x();
        mTileY = tilePos.y();

        tilePositionChanged(tilePos);
        updateStatusInfo();
    }
}

void AbstractBmpTool::mapDocumentChanged(MapDocument *oldDocument,
                                          MapDocument *newDocument)
{
    Q_UNUSED(oldDocument)
    mBrushItem->setMapDocument(newDocument);
    BmpToolDialog::instance()->setDocument(newDocument);
}

void AbstractBmpTool::updateEnabledState()
{
    setEnabled(mapDocument()
               && mapDocument()->mapComposite()->tileLayersForLevel(0)
               && currentLayer() != 0);
}

void AbstractBmpTool::updateStatusInfo()
{
    if (mBrushVisible) {
        setStatusInfo(QString(QLatin1String("%1, %2"))
                      .arg(mTileX).arg(mTileY));
    } else {
        setStatusInfo(QString());
    }
}

void AbstractBmpTool::setBrushVisible(bool visible)
{
    if (mBrushVisible == visible)
        return;

    mBrushVisible = visible;
    updateStatusInfo();
    updateBrushVisibility();
}

void AbstractBmpTool::updateBrushVisibility()
{
    bool showBrush = false;
    if (mBrushVisible) {
        if (Layer *layer = currentLayer()) {
            showBrush = true;
        }
    }
    mBrushItem->setVisible(showBrush);
}

Layer *AbstractBmpTool::currentLayer() const
{
    if (!mapDocument())
        return 0;

    return mapDocument()->currentLayer();
}

/////

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
    QImage &image = mMapDocument->map()->rbmp(mBmpIndex).rimage();
    mErased = image.copy(mX, mY, mSource.width(), mSource.height());
}

void PaintBMP::paint(const ResizableImage &source)
{
#if 1
    mMapDocument->paintBmp(mBmpIndex, mX, mY, source, mRegion);
#else
    QImage &image = mMapDocument->map()->rbmp(mBmpIndex);
    QRegion region = mRegion & QRect(0, 0, image.width(), image.height());

    foreach (QRect r, region.rects()) {
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                image.setPixel(x, y, source.pixel(x - mX, y - mY));
            }
        }
    }

    const QRect r = region.boundingRect();
    mMapDocument->bmpBlender()->update(r.left(), r.top(), r.right(), r.bottom());
    Q_ASSERT(mMapDocument->mapComposite()->tileLayersForLevel(0));
    mMapDocument->mapComposite()->tileLayersForLevel(0)->setBmpBlendLayers(
                mMapDocument->bmpBlender()->mTileLayers.values());

    foreach (QString layerName, mMapDocument->bmpBlender()->mTileLayers.keys()) {
        int index = mMapDocument->map()->indexOfLayer(layerName, Layer::TileLayerType);
        if (index == -1)
            continue;
        mMapDocument->emitRegionAltered(region, mMapDocument->map()->layerAt(index)->asTileLayer());
    }
#endif
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

BmpPainterTool *BmpPainterTool::mInstance = 0;

BmpPainterTool *BmpPainterTool::instance()
{
    if (!mInstance)
        mInstance = new BmpPainterTool;
    return mInstance;
}

BmpPainterTool::BmpPainterTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Painter"),
                     QIcon(QLatin1String(
                             ":images/22x22/bmp-tool.png")),
                     QKeySequence(tr("P")),
                     parent),
    mPainting(false),
    mBmpIndex(0),
    mBrushSize(1)
{
}

BmpPainterTool::~BmpPainterTool()
{
}

void BmpPainterTool::activate(MapScene *scene)
{
    AbstractBmpTool::activate(scene);
}

void BmpPainterTool::deactivate(MapScene *scene)
{
    AbstractBmpTool::deactivate(scene);
}

void BmpPainterTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        mPainting = true;
        mErasing = (event->modifiers() & Qt::ControlModifier) != 0;
        paint(false);
    }
}

void BmpPainterTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mPainting = false;
}

void BmpPainterTool::setBrushSize(int size)
{
    mBrushSize = size;
    tilePositionChanged(tilePosition());
}

void BmpPainterTool::mapDocumentChanged(MapDocument *oldDocument,
                                 MapDocument *newDocument)
{
    AbstractBmpTool::mapDocumentChanged(oldDocument, newDocument);
}

void BmpPainterTool::languageChanged()
{
    setName(tr("BMP Painter"));
    setShortcut(QKeySequence(tr("P")));
}

void BmpPainterTool::tilePositionChanged(const QPoint &tilePos)
{
    brushItem()->setTileRegion(QRect(tilePos - QPoint(mBrushSize/2, mBrushSize/2),
                                     QSize(mBrushSize, mBrushSize)));

    if (mPainting)
        paint(true);
}

void BmpPainterTool::paint(bool mergeable)
{
    QRect mapBounds(QPoint(), mapDocument()->map()->size());
    QRect r = brushItem()->tileRegion().boundingRect();

    if (!mapBounds.intersects(r))
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

/////

ChangeBmpSelection::ChangeBmpSelection(MapDocument *mapDocument,
                                       const QRegion &newSelection)
    : QUndoCommand(QCoreApplication::translate("Undo Commands",
                                               "Change BMP Selection"))
    , mMapDocument(mapDocument)
    , mSelection(newSelection)
{
}

void ChangeBmpSelection::swap()
{
    const QRegion oldSelection = mMapDocument->bmpSelection();
    mMapDocument->setBmpSelection(mSelection);
    mSelection = oldSelection;
}

/////

ResizeBmpImage::ResizeBmpImage(MapDocument *mapDocument, int bmpIndex, const QSize &size,
                     const QPoint &offset) :
    QUndoCommand(QCoreApplication::translate("Undo Commands",
                                             "Resize BMP Image")),
    mMapDocument(mapDocument),
    mBmpIndex(bmpIndex)
{
    ResizableImage resized = ResizableImage(mMapDocument->map()->bmp(mBmpIndex).image());
    resized.resize(size, offset);
    mResized = resized;
}

void ResizeBmpImage::undo()
{
    mResized = mMapDocument->swapBmpImage(mBmpIndex, mOriginal);
}

void ResizeBmpImage::redo()
{
    mOriginal = mMapDocument->swapBmpImage(mBmpIndex, mResized);
}

/////

ResizeBmpRands::ResizeBmpRands(MapDocument *mapDocument, int bmpIndex,
                               const QSize &size) :
    QUndoCommand(QCoreApplication::translate("Undo Commands",
                                             "Resize BMP Rands")),
    mMapDocument(mapDocument),
    mBmpIndex(bmpIndex),
    mOriginal(mapDocument->map()->bmp(bmpIndex).rands()),
    mResized(mOriginal)
{
    mResized.setSize(size.width(), size.height());
}

void ResizeBmpRands::undo()
{
    /*mResized = */mMapDocument->swapBmpRands(mBmpIndex, mOriginal);
}

void ResizeBmpRands::redo()
{
    /*mOriginal = */mMapDocument->swapBmpRands(mBmpIndex, mResized);
}

/////

BmpSelectionTool *BmpSelectionTool::mInstance = 0;

BmpSelectionTool *BmpSelectionTool::instance()
{
    if (!mInstance)
        mInstance = new BmpSelectionTool;
    return mInstance;
}

BmpSelectionTool::BmpSelectionTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Select"),
                    QIcon(QLatin1String(
                              ":images/22x22/stock-tool-rect-select.png")),
                    QKeySequence(/*tr("R")*/),
                    parent),
    mMode(NoMode),
    mMouseDown(false),
    mSelectionMode(Replace),
    mSelecting(false)
{
}

void BmpSelectionTool::tilePositionChanged(const QPoint &pos)
{
    if (mMode == Dragging) {
        QPoint offset = pos - mDragStart;
        if (scene() && scene()->bmpSelectionItem())
            scene()->bmpSelectionItem()->setDragOffset(offset);
        return;
    }
    if (mSelecting)
        brushItem()->setTileRegion(selectedArea());
}

void BmpSelectionTool::updateStatusInfo()
{
    if (!isBrushVisible() || !mSelecting) {
        AbstractBmpTool::updateStatusInfo();
        return;
    }

    const QPoint pos = tilePosition();
    const QRect area = selectedArea();
    setStatusInfo(tr("%1, %2 - Rectangle: (%3 x %4)")
                  .arg(pos.x()).arg(pos.y())
                  .arg(area.width()).arg(area.height()));
}

void BmpSelectionTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (button == Qt::LeftButton) {
        mMouseDown = true;
        mMouseMoved = false;
        mStartScenePos = event->scenePos();

        if (modifiers == Qt::ControlModifier) {
            mSelectionMode = Subtract;
        } else if (modifiers == Qt::ShiftModifier) {
            mSelectionMode = Add;
        } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
            mSelectionMode = Intersect;
        } else {
            if (mapDocument()->bmpSelection().contains(tilePosition())) {
                mMode = Dragging;
                mDragStart = tilePosition();
                return;
            }
            mSelectionMode = Replace;
        }

        mMode = Selecting;
        mSelecting = true;
        mSelectionStart = tilePosition();
        brushItem()->setTileRegion(QRegion());
    }

    if (button == Qt::RightButton) {
        if (mMode == Dragging) {
            if (scene() && scene()->bmpSelectionItem())
                scene()->bmpSelectionItem()->setDragOffset(QPoint());
            mMode = NoMode;
        }
    }
}

static void copyImageToImage(int sx, int sy, int sw, int sh, const QImage &src,
                             int dx, int dy, QImage &dst)
{
    QRect srcRct(0, 0, src.width(), src.height());
    QRect dstRct(0, 0, dst.width(), dst.height());
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x < sw; x++) {
            if (srcRct.contains(sx + x, sy + y) && dstRct.contains(dx + x, dy + y))
                dst.setPixel(dx + x, dy + y, src.pixel(sx + x, sy + y));
        }
    }
}

void BmpSelectionTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    MapDocument *doc = mapDocument();
    if (event->button() == Qt::LeftButton) {
        mMouseDown = false;
        if (mMode == Dragging) {
            if (scene() && scene()->bmpSelectionItem())
                scene()->bmpSelectionItem()->setDragOffset(QPoint());
            mMode = NoMode;
            QPoint offset = tilePosition() - mDragStart;
            if (!offset.isNull()) {
                QRegion oldSelection = doc->bmpSelection();
                QRegion newSelection = oldSelection.translated(offset);
                QRegion paintedRgn = oldSelection | newSelection;
                paintedRgn &= QRect(0, 0, doc->map()->width(), doc->map()->height());
                int bmpIndex = BmpPainterTool::instance()->bmpIndex();
                doc->undoStack()->beginMacro(tr("Drag BMP Selection"));
                {
                    QImage &bmp = doc->map()->rbmp(bmpIndex).rimage();
                    QRect r = paintedRgn.boundingRect();
                    QImage image = bmp.copy(r.x(), r.y(), r.width(), r.height());
                    foreach (QRect rect, oldSelection.rects()) {
                        copyImageToImage(rect.x(), rect.y(),
                                         rect.width(), rect.height(), bmp,
                                         rect.x() + offset.x() - r.x(),
                                         rect.y() + offset.y() - r.y(), image);
                    }
                    QPainter p(&image);
                    foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                        p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                    }
                    p.end();
                    doc->undoStack()->push(new PaintBMP(doc, bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }
                if (event->modifiers() & Qt::ControlModifier) {
                    QImage &bmp = doc->map()->rbmp(!bmpIndex).rimage(); // opposite of above
                    QRect r = paintedRgn.boundingRect();
                    QImage image = bmp.copy(r.x(), r.y(), r.width(), r.height());
                    foreach (QRect rect, oldSelection.rects()) {
                        copyImageToImage(rect.x(), rect.y(),
                                         rect.width(), rect.height(), bmp,
                                         rect.x() + offset.x() - r.x(),
                                         rect.y() + offset.y() - r.y(), image);
                    }
                    QPainter p(&image);
                    foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                        p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                    }
                    p.end();
                    doc->undoStack()->push(new PaintBMP(doc, !bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }
                doc->undoStack()->push(new ChangeBmpSelection(doc, newSelection));
                doc->undoStack()->endMacro();
            }
            return;
        }
        if (mMode == Selecting) {
            mSelecting = false;

            QRegion selection = doc->bmpSelection();
            const QRect area = selectedArea();

            switch (mSelectionMode) {
            case Replace:   selection = area; break;
            case Add:       selection += area; break;
            case Subtract:  selection -= area; break;
            case Intersect: selection &= area; break;
            }

            if (!mMouseMoved)
                selection = QRegion();

            if (selection != doc->bmpSelection()) {
                QUndoCommand *cmd = new ChangeBmpSelection(doc, selection);
                doc->undoStack()->push(cmd);
            }

            brushItem()->setTileRegion(QRegion());
            updateStatusInfo();
        }
    }
}

void BmpSelectionTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (mMouseDown && !mMouseMoved) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance())
            mMouseMoved = true;
    }

    AbstractBmpTool::mouseMoved(pos, modifiers);
}

void BmpSelectionTool::languageChanged()
{
    setName(tr("BMP Select"));
    setShortcut(QKeySequence(/*tr("R")*/));
}

QRect BmpSelectionTool::selectedArea() const
{
    const QPoint tilePos = tilePosition();
    return QRect(QPoint(qMin(mSelectionStart.x(), tilePos.x()),
                        qMin(mSelectionStart.y(), tilePos.y())),
                 QPoint(qMax(mSelectionStart.x(), tilePos.x()),
                        qMax(mSelectionStart.y(), tilePos.y())));
}
