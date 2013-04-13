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
#include "tileselectionitem.h"
#include "undocommands.h"

#include "map.h"
#include "maprenderer.h"

#include <QApplication>
#include <QDebug>
#include <QPainter>
#include <QUndoCommand>
#include <QVector2D>
#include <qmath.h>

using namespace Tiled;
using namespace Tiled::Internal;

/////

AbstractBmpTool::AbstractBmpTool(const QString &name,
                                 const QIcon &icon,
                                 const QKeySequence &shortcut,
                                 QObject *parent)
    : AbstractTool(name, icon, shortcut, parent)
    , mScene(0)
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

    tilePos = QPoint(qFloor(tilePosF.x()),
                     qFloor(tilePosF.y()));

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

PaintBMP::PaintBMP(MapDocument *mapDocument, int bmpIndex,
                   int x, int y, const QImage &source, const QRegion &region) :
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
    mMapDocument->paintBmp(mBmpIndex, mX, mY, source, mRegion);
}

int PaintBMP::id() const
{
    return Cmd_PaintBMP;
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

    // Resize the erased tiles and source image when necessary
    if (bounds != combinedBounds) {
        const QPoint shift = bounds.topLeft() - combinedBounds.topLeft();
        mErased.resize(combinedBounds.size(), shift);
        mSource.resize(combinedBounds.size(), shift);
    }

    mX = combinedBounds.left();
    mY = combinedBounds.top();
    mRegion = combinedRegion;

    // Copy the painted pixels from the other command over
    const QPoint pos = QPoint(o->mX, o->mY) - combinedBounds.topLeft();
    mSource.merge(pos, &o->mSource, o->mRegion);

    // Copy the newly-erased pixels from the other command over
    mErased.merge(pos, &o->mErased, newRegion);

    return true;
}

/////

BmpBrushTool *BmpBrushTool::mInstance = 0;

BmpBrushTool *BmpBrushTool::instance()
{
    if (!mInstance)
        mInstance = new BmpBrushTool;
    return mInstance;
}

BmpBrushTool::BmpBrushTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Brush"),
                     QIcon(QLatin1String(":images/22x22/bmp-tool.png")),
                     QKeySequence(tr("")),
                     parent),
    mPainting(false),
    mBmpIndex(0),
    mBrushSize(1),
    mBrushShape(Square),
    mRestrictToSelection(false)
{
}

BmpBrushTool::~BmpBrushTool()
{
}

void BmpBrushTool::activate(MapScene *scene)
{
    AbstractBmpTool::activate(scene);
}

void BmpBrushTool::deactivate(MapScene *scene)
{
    AbstractBmpTool::deactivate(scene);
}

void BmpBrushTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        mPainting = true;
        mDidFirstPaint = false;
        mStampPos = tilePosition();
        mErasing = (event->modifiers() & Qt::ControlModifier) != 0;
        paint();
    }
}

void BmpBrushTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mPainting = false;
}

void BmpBrushTool::setBrushSize(int size)
{
    if (size == mBrushSize)
        return;

    mBrushSize = size;
    tilePositionChanged(tilePosition());
    emit brushChanged();
}

void BmpBrushTool::setBrushShape(BmpBrushTool::BrushShape shape)
{
    mBrushShape = shape;
    tilePositionChanged(tilePosition());
}

void BmpBrushTool::mapDocumentChanged(MapDocument *oldDocument,
                                 MapDocument *newDocument)
{
    AbstractBmpTool::mapDocumentChanged(oldDocument, newDocument);
}

void BmpBrushTool::languageChanged()
{
    setName(tr("BMP Brush"));
    setShortcut(QKeySequence(tr("")));
}

/**
 * Returns the lists of points on a line from (x0,y0) to (x1,y1).
 *
 * This is an implementation of bresenhams line algorithm, initially copied
 * from http://en.wikipedia.org/wiki/Bresenham's_line_algorithm#Optimization
 * changed to C++ syntax.
 */
// Copied from stampbrush.cpp
static QVector<QPoint> calculateLine(int x0, int y0, int x1, int y1)
{
    QVector<QPoint> ret;

    bool steep = qAbs(y1 - y0) > qAbs(x1 - x0);
    if (steep) {
        qSwap(x0, y0);
        qSwap(x1, y1);
    }
    if (x0 > x1) {
        qSwap(x0, x1);
        qSwap(y0, y1);
    }
    const int deltax = x1 - x0;
    const int deltay = qAbs(y1 - y0);
    int error = deltax / 2;
    int ystep;
    int y = y0;

    if (y0 < y1)
        ystep = 1;
    else
        ystep = -1;

    for (int x = x0; x < x1 + 1 ; x++) {
        if (steep)
            ret += QPoint(y, x);
        else
            ret += QPoint(x, y);
        error = error - deltay;
        if (error < 0) {
             y = y + ystep;
             error = error + deltax;
        }
    }

    return ret;
}

void BmpBrushTool::tilePositionChanged(const QPoint &tilePos)
{
    setBrushRegion(tilePos);

    if (mPainting) {
        foreach (const QPoint &p, calculateLine(mStampPos.x(), mStampPos.y(),
                                                tilePos.x(), tilePos.y())) {
            setBrushRegion(p);
            paint();
        }
        mStampPos = tilePos;
    }
}

class BresenhamCircle
{
public:
    BresenhamCircle(int cx, int cy, int diameter) :
        xCenter(cx),
        yCenter(cy),
        diameter(diameter),
        cheat(!(diameter % 2))
    {
        if (diameter == 2) {
            region = QRect(cx - 1, cy - 1, 2, 2);
            return;
        }
        if (diameter == 4) {
            region += QRect(cx - 1, cy - 2, 2, 4);
            region += QRect(cx - 2, cy - 1, 4, 2);
            return;
        }
        circleMidpoint();
    }

    void point(int x, int y)
    {
        // For non-integer radius (i.e., even-diameter circles), just
        // chop out the pixels along the axes.
        if (cheat) {
            if (x == xCenter || y == yCenter) return;
            if (x > xCenter) --x;
            if (y > yCenter) --y;
        }
        region += QRect(x, y, 1, 1);
    }

    void line(int x1, int y1, int x2)
    {
        while (x1 <= x2)
            point(x1++, y1);
    }

    void points(int x, int y)
    {
        int cx = xCenter, cy = yCenter;

        if (x == 0) {
            point(cx, cy + y);
            point(cx, cy - y);
            line(cx - y, cy, cx + y );
        } else if (x == y) {
            line(cx - x, cy + y, cx + x);
            line(cx - x, cy - y, cx + x);
        } else if (x < y) {
            line(cx - x, cy + y, cx + x);
            line(cx - x, cy - y, cx + x);
            line(cx - y, cy + x, cx + y);
            line(cx - y, cy - x, cx + y);
        }
    }

    void circleMidpoint()
    {
        int radius = diameter / 2;
        if (cheat) radius = (diameter + 1) / 2;

        int x = 0;
        int y = radius;
        int p = (5 - radius*4)/4;

        points(x, y);
        while (x < y) {
            x++;
            if (p < 0) {
                p += 2*x+1;
            } else {
                y--;
                p += 2*(x-y)+1;
            }
            points(x, y);
        }
    }

    int xCenter;
    int yCenter;
    int diameter;
    bool cheat;
    QRegion region;
};

void BmpBrushTool::setBrushRegion(const QPoint &tilePos)
{
    if (mBrushShape == Circle) {
        BresenhamCircle bc(tilePos.x(), tilePos.y(), mBrushSize);
        brushItem()->setTileRegion(bc.region);
        return;
    }
    brushItem()->setTileRegion(QRect(tilePos - QPoint(mBrushSize/2, mBrushSize/2),
                                     QSize(mBrushSize, mBrushSize)));
}

// Calculate the region of pixels that do *not* have a given pixel value.
static QRegion bmpPixelRegion(Map *map, int bmpIndex, const QRegion &tileRgn, QRgb pixel)
{
    const QImage &bmpImage = map->rbmp(bmpIndex).rimage();
    QRect mapBounds(QPoint(), map->size());

    QRegion paintRgn;
    foreach (QRect r, tileRgn.rects()) {
        r &= mapBounds;
        for (int y = r.top(); y <= r.bottom(); y++) {
            for (int x = r.left(); x <= r.right(); x++) {
                if (bmpImage.pixel(x, y) != pixel) {
                    paintRgn += QRect(x, y, 1, 1);
                }
            }
        }
    }
    return paintRgn;
}

void BmpBrushTool::paint()
{
    if (!mErasing && mColor == qRgb(0, 0, 0))
        return;

    QRect mapBounds(QPoint(), mapDocument()->map()->size());

    QRegion tileRgn = brushItem()->tileRegion();
    if (restrictToSelection()) {
        QRegion selection = mapDocument()->bmpSelection();
        if (!selection.isEmpty())
            tileRgn &= selection;
    }

    QRgb color = mErasing ? qRgb(0, 0, 0) : mColor;
    QRegion paintRgn = bmpPixelRegion(mapDocument()->map(), mBmpIndex, tileRgn, color);

    if (paintRgn.isEmpty())
        return;

    QRect r = paintRgn.boundingRect();
    QPoint topLeft = r.topLeft();
    QImage image(r.size(), QImage::Format_ARGB32);
    image.fill(Qt::black);
    QPainter p(&image);
    foreach (QRect r, paintRgn.rects())
        p.fillRect(r.translated(-topLeft), color);
    p.end();
    PaintBMP *cmd = new PaintBMP(mapDocument(), mBmpIndex,
                                 topLeft.x(), topLeft.y(), image,
                                 paintRgn);
    cmd->setMergeable(mDidFirstPaint);
    mapDocument()->undoStack()->push(cmd);
    mDidFirstPaint = true;
}

/////

BmpEraserTool *BmpEraserTool::mInstance = 0;

BmpEraserTool *BmpEraserTool::instance()
{
    if (!mInstance)
        mInstance = new BmpEraserTool;
    return mInstance;
}

BmpEraserTool::BmpEraserTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Eraser"),
                     QIcon(QLatin1String(":images/22x22/bmp-eraser.png")),
                     QKeySequence(tr("")),
                     parent),
    mPainting(false)
{
    connect(BmpBrushTool::instance(), SIGNAL(brushChanged()),
            SLOT(brushChanged()));
}

BmpEraserTool::~BmpEraserTool()
{
}

void BmpEraserTool::activate(MapScene *scene)
{
    AbstractBmpTool::activate(scene);
}

void BmpEraserTool::deactivate(MapScene *scene)
{
    AbstractBmpTool::deactivate(scene);
}

void BmpEraserTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    if (!brushItem()->isVisible())
        return;

    if (event->button() == Qt::LeftButton) {
        mPainting = true;
        mDidFirstPaint = false;
        mStampPos = tilePosition();
        paint();
    }
}

void BmpEraserTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mPainting = false;
}

void BmpEraserTool::languageChanged()
{
    setName(tr("BMP Eraser"));
    setShortcut(QKeySequence(tr("")));
}

void BmpEraserTool::tilePositionChanged(const QPoint &tilePos)
{
    setBrushRegion(tilePos);

    if (mPainting) {
        foreach (const QPoint &p, calculateLine(mStampPos.x(), mStampPos.y(),
                                                tilePos.x(), tilePos.y())) {
            setBrushRegion(p);
            paint();
        }
        mStampPos = tilePos;
    }
}

void BmpEraserTool::setBrushRegion(const QPoint &tilePos)
{
    int brushSize = BmpBrushTool::instance()->brushSize();
    if (BmpBrushTool::instance()->brushShape() == BmpBrushTool::Circle) {
#if 1
        BresenhamCircle bc(tilePos.x(), tilePos.y(), brushSize);
        QRegion rgn = bc.region;
#else
        QRegion rgn;
        qreal radius = brushSize / 2.0;
        QVector2D center = QVector2D(tilePos) + QVector2D(0.5, 0.5);
        for (int y = -qFloor(radius); y <= qCeil(radius); y++) {
            for (int x = -qFloor(radius); x <= qCeil(radius); x++) {
                QVector2D p(tilePos.x() + x + 0.5, tilePos.y() + y + 0.5);
                if ((p - center).length() <= radius + 0.05)
                    rgn += QRect(tilePos + QPoint(x, y), QSize(1, 1));
            }
        }
#endif
        brushItem()->setTileRegion(rgn);
        return;
    }
    brushItem()->setTileRegion(QRect(tilePos - QPoint(brushSize/2, brushSize/2),
                                     QSize(brushSize, brushSize)));
}

void BmpEraserTool::paint()
{
    QRegion tileRgn = brushItem()->tileRegion();
    if (BmpBrushTool::instance()->restrictToSelection()) {
        QRegion selection = mapDocument()->bmpSelection();
        if (!selection.isEmpty())
            tileRgn &= selection;
    }

    int bmpIndex = BmpBrushTool::instance()->bmpIndex();

    QRgb black = qRgb(0, 0, 0);
    QRegion paintRgn = bmpPixelRegion(mapDocument()->map(), bmpIndex, tileRgn, black);
    if (paintRgn.isEmpty())
        return;

    QRect r = paintRgn.boundingRect();
    QPoint topLeft = r.topLeft();
    QImage image(r.size(), QImage::Format_ARGB32);
    image.fill(black);
    PaintBMP *cmd = new PaintBMP(mapDocument(), bmpIndex,
                                 topLeft.x(), topLeft.y(), image,
                                 paintRgn);
    cmd->setMergeable(mDidFirstPaint);
    mapDocument()->undoStack()->push(cmd);
    mDidFirstPaint = true;
}

void BmpEraserTool::eraseBmp(int bmpIndex, const QRegion &tileRgn)
{
}

void BmpEraserTool::brushChanged()
{
    setBrushRegion(tilePosition());
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
    AbstractBmpTool(tr("BMP Rectangle Select"),
                    QIcon(QLatin1String(
                              ":images/22x22/bmp-select.png")),
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
        if (mMode == Selecting) {
            mSelecting = false;
            brushItem()->setTileRegion(QRegion());
            updateStatusInfo();
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
                int bmpIndex = BmpBrushTool::instance()->bmpIndex();
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
                    // Hold down Ctrl to copy the moved pixels, otherwise erase.
                    if (!(event->modifiers() & Qt::ControlModifier)) {
                        QPainter p(&image);
                        foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                            p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                        }
                        p.end();
                    }
                    doc->undoStack()->push(new PaintBMP(doc, bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }

                // Hold down Shift to affect every BMP.
                if (event->modifiers() & Qt::ShiftModifier) {
                    QImage &bmp = doc->map()->rbmp(!bmpIndex).rimage();
                    QRect r = paintedRgn.boundingRect();
                    QImage image = bmp.copy(r.x(), r.y(), r.width(), r.height());
                    foreach (QRect rect, oldSelection.rects()) {
                        copyImageToImage(rect.x(), rect.y(),
                                         rect.width(), rect.height(), bmp,
                                         rect.x() + offset.x() - r.x(),
                                         rect.y() + offset.y() - r.y(), image);
                    }
                    // Hold down Ctrl to copy the moved pixels, otherwise erase.
                    if (!(event->modifiers() & Qt::ControlModifier)) {
                        QPainter p(&image);
                        foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                            p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                        }
                        p.end();
                    }
                    doc->undoStack()->push(new PaintBMP(doc, !bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }
                doc->undoStack()->push(new ChangeBmpSelection(doc, newSelection));
                doc->undoStack()->endMacro();
            }
            return;
        }
        if (mMode == Selecting) {
            mMode = NoMode;
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
        if (dragDistance >= QApplication::startDragDistance()) {
            mMouseMoved = true;
            tilePositionChanged(tilePosition());
        }
    }

    AbstractBmpTool::mouseMoved(pos, modifiers);
}

void BmpSelectionTool::languageChanged()
{
    setName(tr("BMP Rectangle Select"));
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

/////

BmpWandTool *BmpWandTool::mInstance = 0;

BmpWandTool *BmpWandTool::instance()
{
    if (!mInstance)
        mInstance = new BmpWandTool;
    return mInstance;
}

BmpWandTool::BmpWandTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Fuzzy Select"),
                    QIcon(QLatin1String(
                              ":images/22x22/bmp-wand.png")),
                    QKeySequence(/*tr("R")*/),
                    parent),
    mMode(NoMode),
    mMouseDown(false)
{
    connect(BmpBrushTool::instance(), SIGNAL(ruleChanged()),
            SLOT(bmpImageChanged()));
    connect(BmpBrushTool::instance(), SIGNAL(restrictToSelectionChanged()),
            SLOT(bmpImageChanged()));
}

void BmpWandTool::tilePositionChanged(const QPoint &tilePos)
{
    if (mMode == Dragging) {
        QPoint offset = tilePos - mDragStart;
        if (scene() && scene()->bmpSelectionItem())
            scene()->bmpSelectionItem()->setDragOffset(offset);
        return;
    }

    if (!QRect(QPoint(), mapDocument()->map()->size()).contains(tilePos)) {
        brushItem()->setTileRegion(QRegion());
        mFloodFill.mRegion = QRegion();
        return;
    }
    if (mFloodFill.mRegion.contains(tilePos))
        return;
    int bmpIndex = BmpBrushTool::instance()->bmpIndex();
    mFloodFill.mImage = mapDocument()->map()->bmp(bmpIndex).image().copy();
    mFloodFill.mRegion = QRegion();
    mFloodFill.floodFillScanlineStack(tilePos.x(), tilePos.y(),
                                      qRgba(0, 0, 0, 0),
                                      mFloodFill.mImage.pixel(tilePos.x(), tilePos.y()));
    brushItem()->setTileRegion(mFloodFill.mRegion);
}

void BmpWandTool::updateStatusInfo()
{
    AbstractBmpTool::updateStatusInfo();
}

void BmpWandTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (button == Qt::LeftButton) {
        mMouseDown = true;
        mMouseMoved = false;
        mStartScenePos = event->scenePos();

        QRegion selection = mapDocument()->bmpSelection();
        if (modifiers == Qt::ControlModifier) {
            selection -= brushItem()->tileRegion();
        } else if (modifiers == Qt::ShiftModifier) {
            selection += brushItem()->tileRegion();
        } else if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier)) {
            selection &= brushItem()->tileRegion();
        } else {
            if (mapDocument()->bmpSelection().contains(tilePosition())) {
                mMode = Dragging;
                mDragStart = tilePosition();
                return;
            }
            selection = brushItem()->tileRegion();
        }
        if (selection != mapDocument()->bmpSelection()) {
            ChangeBmpSelection *cmd = new ChangeBmpSelection(mapDocument(), selection);
            mapDocument()->undoStack()->push(cmd);
            tilePositionChanged(tilePosition());
        }
    }

    if (button == Qt::RightButton) {
        if (mMode == Dragging) {
            if (scene() && scene()->bmpSelectionItem())
                scene()->bmpSelectionItem()->setDragOffset(QPoint());
            mMode = NoMode;
        }
    }
}

void BmpWandTool::mouseReleased(QGraphicsSceneMouseEvent *event)
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
                int bmpIndex = BmpBrushTool::instance()->bmpIndex();
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
                    // Hold down Ctrl to copy the moved pixels, otherwise erase.
                    if (!(event->modifiers() & Qt::ControlModifier)) {
                        QPainter p(&image);
                        foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                            p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                        }
                        p.end();
                    }
                    doc->undoStack()->push(new PaintBMP(doc, bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }

                // Hold down Shift to affect every BMP.
                if (event->modifiers() & Qt::ShiftModifier) {
                    QImage &bmp = doc->map()->rbmp(!bmpIndex).rimage();
                    QRect r = paintedRgn.boundingRect();
                    QImage image = bmp.copy(r.x(), r.y(), r.width(), r.height());
                    foreach (QRect rect, oldSelection.rects()) {
                        copyImageToImage(rect.x(), rect.y(),
                                         rect.width(), rect.height(), bmp,
                                         rect.x() + offset.x() - r.x(),
                                         rect.y() + offset.y() - r.y(), image);
                    }
                    // Hold down Ctrl to copy the moved pixels, otherwise erase.
                    if (!(event->modifiers() & Qt::ControlModifier)) {
                        QPainter p(&image);
                        foreach (QRect rect, (paintedRgn - newSelection).rects()) {
                            p.fillRect(rect.translated(-r.topLeft()), Qt::black);
                        }
                        p.end();
                    }
                    doc->undoStack()->push(new PaintBMP(doc, !bmpIndex, r.x(), r.y(),
                                                        image, paintedRgn));
                }
                doc->undoStack()->push(new ChangeBmpSelection(doc, newSelection));
                doc->undoStack()->endMacro();
            }
            return;
        }
    }
}

void BmpWandTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (mMouseDown && !mMouseMoved) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            mMouseMoved = true;
            tilePositionChanged(tilePosition());
        }
    }

    AbstractBmpTool::mouseMoved(pos, modifiers);
}

void BmpWandTool::languageChanged()
{
    setName(tr("BMP Fuzzy Select"));
    setShortcut(QKeySequence(/*tr("R")*/));
}

void BmpWandTool::mapDocumentChanged(MapDocument *oldDocument,
                                       MapDocument *newDocument)
{
    AbstractBmpTool::mapDocumentChanged(oldDocument, newDocument);

    if (oldDocument)
        oldDocument->disconnect(this);

    if (newDocument) {
        connect(newDocument, SIGNAL(bmpPainted(int,QRegion)),
                SLOT(bmpImageChanged()));
        connect(newDocument, SIGNAL(mapChanged()),
                SLOT(bmpImageChanged()));
    }

    mFloodFill.mRegion = QRegion();
}

void BmpWandTool::bmpImageChanged()
{
    // Recompute the fill region if the BMP or tool settings change.
    mFloodFill.mRegion = QRegion();
    if (scene()) {
        tilePositionChanged(tilePosition());
    }
}

/////

BmpRectTool *BmpRectTool::mInstance = 0;

BmpRectTool *BmpRectTool::instance()
{
    if (!mInstance)
        mInstance = new BmpRectTool;
    return mInstance;
}

BmpRectTool::BmpRectTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Rectangle"),
                    QIcon(QLatin1String(
                              ":images/22x22/bmp-rect.png")),
                    QKeySequence(/*tr("R")*/),
                    parent),
    mMode(NoMode),
    mMouseDown(false)
{
}

void BmpRectTool::tilePositionChanged(const QPoint &tilePos)
{
    Q_UNUSED(tilePos)
    if (mMode == Painting)
        brushItem()->setTileRegion(selectedArea());
}

void BmpRectTool::updateStatusInfo()
{
    if (!isBrushVisible() || (mMode == NoMode)) {
        AbstractBmpTool::updateStatusInfo();
        return;
    }

    const QPoint pos = tilePosition();
    const QRect area = selectedArea();
    setStatusInfo(tr("%1, %2 - Rectangle: (%3 x %4)")
                  .arg(pos.x()).arg(pos.y())
                  .arg(area.width()).arg(area.height()));
}

void BmpRectTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (button == Qt::LeftButton) {
        mMode = Painting;
        mMouseDown = true;
        mMouseMoved = false;
        mStartScenePos = event->scenePos();
        mStartTilePos = tilePosition();
        mErasing = (modifiers & Qt::ControlModifier) != 0;
        brushItem()->setTileRegion(QRegion());
    }

    if (button == Qt::RightButton) {
        if (mMode == Painting) {
            brushItem()->setTileRegion(QRegion());
            updateStatusInfo();
            mMode = NoMode;
        }
    }
}

void BmpRectTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
    MapDocument *doc = mapDocument();
    if (event->button() == Qt::LeftButton) {
        mMouseDown = false;
        if (mMode == Painting) {
            mMode = NoMode;

            const QRect r = selectedArea();
            int bmpIndex = BmpBrushTool::instance()->bmpIndex();
            QRgb color = mErasing ? qRgb(0, 0, 0)
                                  : BmpBrushTool::instance()->color();
            if (!r.isEmpty()) {
                QRegion tileRgn(r);
                if (BmpBrushTool::instance()->restrictToSelection()) {
                    QRegion selection = mapDocument()->bmpSelection();
                    if (!selection.isEmpty())
                        tileRgn &= selection;
                }

                QRegion paintRgn = bmpPixelRegion(mapDocument()->map(), bmpIndex,
                                                  tileRgn, color);
                if (!paintRgn.isEmpty()) {
                    QRect paintRect = paintRgn.boundingRect();
                    QPoint topLeft = paintRect.topLeft();
                    QImage image(paintRect.size(), QImage::Format_ARGB32);
                    image.fill(color);
                    PaintBMP *cmd = new PaintBMP(doc, bmpIndex,
                                                 topLeft.x(), topLeft.y(),
                                                 image, paintRgn);
                    cmd->setMergeable(false);
                    doc->undoStack()->push(cmd);
                }
            }
            brushItem()->setTileRegion(QRegion());
            updateStatusInfo();
        }
    }
}

void BmpRectTool::mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (mMouseDown && !mMouseMoved) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            mMouseMoved = true;
            tilePositionChanged(tilePosition());
        }
    }

    AbstractBmpTool::mouseMoved(pos, modifiers);
}

void BmpRectTool::modifiersChanged(Qt::KeyboardModifiers)
{
    tilePositionChanged(tilePosition());
}

void BmpRectTool::languageChanged()
{
    setName(tr("BMP Rectangle"));
    setShortcut(QKeySequence(/*tr("R")*/));
}

QRect BmpRectTool::selectedArea() const
{
    if (!mMouseMoved)
        return QRect();
    const QPoint tilePos = tilePosition();
    QRect r(QPoint(qMin(mStartTilePos.x(), tilePos.x()),
                   qMin(mStartTilePos.y(), tilePos.y())),
            QPoint(qMax(mStartTilePos.x(), tilePos.x()),
                   qMax(mStartTilePos.y(), tilePos.y())));
    if (qApp->keyboardModifiers() & Qt::ShiftModifier) {
        int side = qMax(r.width(), r.height());
        if (tilePos.x() < mStartTilePos.x()) {
            r.setLeft(mStartTilePos.x() - side + 1);
            r.setRight(mStartTilePos.x());
        } else {
            r.setWidth(side);
        }
        if (tilePos.y() < mStartTilePos.y()) {
            r.setTop(mStartTilePos.y() - side + 1);
            r.setBottom(mStartTilePos.y());
        } else {
            r.setHeight(side);
        }
    }
    return r & QRect(QPoint(0, 0), mapDocument()->map()->size());
}


/////

BmpBucketTool *BmpBucketTool::mInstance = 0;

BmpBucketTool *BmpBucketTool::instance()
{
    if (!mInstance)
        mInstance = new BmpBucketTool;
    return mInstance;
}

BmpBucketTool::BmpBucketTool(QObject *parent) :
    AbstractBmpTool(tr("BMP Bucket Fill"),
                    QIcon(QLatin1String(
                              ":images/22x22/bmp-bucket.png")),
                    QKeySequence(/*tr("R")*/),
                    parent)
{
    connect(BmpBrushTool::instance(), SIGNAL(ruleChanged()),
            SLOT(bmpImageChanged()));
    connect(BmpBrushTool::instance(), SIGNAL(restrictToSelectionChanged()),
            SLOT(bmpImageChanged()));
}

void BmpBucketTool::tilePositionChanged(const QPoint &tilePos)
{
    if (!QRect(QPoint(), mapDocument()->map()->size()).contains(tilePos)) {
        brushItem()->setTileRegion(QRegion());
        mFloodFill.mRegion = QRegion();
        return;
    }
    if (mFloodFill.mRegion.contains(tilePos))
        return;
    int bmpIndex = BmpBrushTool::instance()->bmpIndex();
    mFloodFill.mImage = mapDocument()->map()->bmp(bmpIndex).image().copy();
    mFloodFill.mRegion = QRegion();
    mFloodFill.floodFillScanlineStack(tilePos.x(), tilePos.y(),
                                      BmpBrushTool::instance()->color(),
                                      mFloodFill.mImage.pixel(tilePos.x(), tilePos.y()));

    QRegion tileRgn = mFloodFill.mRegion;
    if (BmpBrushTool::instance()->restrictToSelection()) {
        QRegion selection = mapDocument()->bmpSelection();
        if (!selection.isEmpty())
            tileRgn &= selection;
    }
    brushItem()->setTileRegion(tileRgn);
}

void BmpBucketTool::updateStatusInfo()
{
    if (!isBrushVisible()) {
        AbstractBmpTool::updateStatusInfo();
        return;
    }

    AbstractBmpTool::updateStatusInfo();
}

void BmpBucketTool::mousePressed(QGraphicsSceneMouseEvent *event)
{
    const Qt::MouseButton button = event->button();
    const Qt::KeyboardModifiers modifiers = event->modifiers();

    if (button == Qt::LeftButton) {
        if (mFloodFill.mRegion.isEmpty())
            return;
        int bmpIndex = BmpBrushTool::instance()->bmpIndex();
        QRegion region = brushItem()->tileRegion();
        mapDocument()->undoStack()->push(
                    new PaintBMP(mapDocument(),
                                 bmpIndex,
                                 region.boundingRect().x(),
                                 region.boundingRect().y(),
                                 mFloodFill.mImage.copy(region.boundingRect()),
                                 region));
    }
}

void BmpBucketTool::mouseReleased(QGraphicsSceneMouseEvent *event)
{
}

void BmpBucketTool::languageChanged()
{
    setName(tr("BMP Bucket Fill"));
    setShortcut(QKeySequence(/*tr("R")*/));
}

void BmpBucketTool::mapDocumentChanged(MapDocument *oldDocument,
                                       MapDocument *newDocument)
{
    AbstractBmpTool::mapDocumentChanged(oldDocument, newDocument);

    if (oldDocument)
        oldDocument->disconnect(this);

    if (newDocument) {
        connect(newDocument, SIGNAL(bmpPainted(int,QRegion)),
                SLOT(bmpImageChanged()));
        connect(newDocument, SIGNAL(mapChanged()),
                SLOT(bmpImageChanged()));
    }

    mFloodFill.mRegion = QRegion();
}

void BmpBucketTool::bmpImageChanged()
{
    // Recompute the fill region if the BMP changes.
    mFloodFill.mRegion = QRegion();
    if (scene()) {
        tilePositionChanged(tilePosition());
    }
}

/////

// Taken from http://lodev.org/cgtutor/floodfill.html#Recursive_Scanline_Floodfill_Algorithm
// Copyright (c) 2004-2007 by Lode Vandevenne. All rights reserved.
void BmpFloodFill::floodFillScanlineStack(int x, int y, QRgb newColor, QRgb oldColor)
{
    if (oldColor == newColor) return;
    emptyStack();

    int y1;
    bool spanLeft, spanRight;

    if (!push(x, y)) return;

    while (pop(x, y)) {
        y1 = y;
        while (y1 >= 0 && pixel(x, y1) == oldColor) y1--;
        y1++;
        spanLeft = spanRight = false;
        QRect r;
        while (y1 < mImage.height() && pixel(x, y1) == oldColor ) {
            setPixel(x, y1, newColor);
#if 1 // This seems a lot faster (in Debug mode at least) than mRegion += QRect(x, y, 1, 1) each setPixel()
            if (r.isEmpty()) {
                r = QRect(x, y1, 1, 1);
            } else if (r.bottom() + 1 != y1) {
                mRegion += r;
                r = QRect();
            } else {
                r.setBottom(y1);
            }
#else
            mRegion += QRect(x, y1, 1, 1);
#endif
            if (!spanLeft && x > 0 && pixel(x - 1, y1) == oldColor) {
                if (!push(x - 1, y1)) return;
                spanLeft = true;
            }
            else if (spanLeft && x > 0 && pixel(x - 1, y1) != oldColor) {
                spanLeft = false;
            }
            if (!spanRight && x < mImage.width() - 1 && pixel(x + 1, y1) == oldColor) {
                if (!push(x + 1, y1)) return;
                spanRight = true;
            }
            else if (spanRight && x < mImage.width() - 1 && pixel(x + 1, y1) != oldColor) {
                spanRight = false;
            }
            y1++;
        }
        if (!r.isEmpty())
            mRegion += r;
    }
}

QRgb BmpFloodFill::pixel(int x, int y)
{
    return mImage.pixel(x, y);
}

void BmpFloodFill::setPixel(int x, int y, QRgb pixel)
{
    mImage.setPixel(x, y, pixel);
}

bool BmpFloodFill::push(int x, int y)
{
    if (stackPointer < STACK_SIZE - 1) {
        stackPointer++;
        stack[stackPointer] = mImage.height() * x + y;
        return true;
    } else {
        return false;
    }
}

bool BmpFloodFill::pop(int &x, int &y)
{
    if (stackPointer > 0) {
        int p = stack[stackPointer];
        x = p / mImage.height();
        y = p % mImage.height();
        stackPointer--;
        return true;
    } else {
        return false;
    }
}

void BmpFloodFill::emptyStack()
{
    stackPointer = 0;
}
