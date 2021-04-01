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

#ifndef BMPTOOL_H
#define BMPTOOL_H

#include "abstracttool.h"

#include "map.h" // for MapRands

#include <QUndoCommand>

namespace Tiled {
class Layer;

namespace Internal {
class BmpToolDialog;
class BrushItem;
class EraseTiles;
class PaintTileLayer;

// Base class for all BMP-editing tools, shamelessly ripped from AbstractTileTool.
class AbstractBmpTool : public AbstractTool
{
    Q_OBJECT

public:
    /**
     * Constructs an abstract tile tool with the given \a name and \a icon.
     */
    AbstractBmpTool(const QString &name,
                     const QIcon &icon,
                     const QKeySequence &shortcut,
                     QObject *parent = 0);

    ~AbstractBmpTool();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mouseEntered();
    void mouseLeft();
    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument);

    void updateEnabledState();

    virtual void tilePositionChanged(const QPoint &tilePos) = 0;

    virtual void updateStatusInfo();

    bool isBrushVisible() const { return mBrushVisible; }

    QPoint tilePosition() const { return QPoint(mTileX, mTileY); }

    BrushItem *brushItem() const { return mBrushItem; }

    Layer *currentLayer() const;

    MapScene *scene() const
    { return mScene; }

private:
    void setBrushVisible(bool visible);
    void updateBrushVisibility();

    MapScene *mScene;
    BrushItem *mBrushItem;
    int mTileX, mTileY;
    bool mBrushVisible;
};

// This tool is for painting (and erasing) pixels in a map's BMP images.
class BmpBrushTool : public AbstractBmpTool
{
    Q_OBJECT
public:
    static BmpBrushTool *instance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void setColor(int index, QRgb color)
    { mBmpIndex = index; mColor = color; emit ruleChanged(); }

    int bmpIndex() const
    { return mBmpIndex; }

    QRgb color() const
    { return mColor; }

    void setBrushSize(int size);
    int brushSize() const
    { return mBrushSize; }

    enum BrushShape {
        Square,
        Circle
    };
    void setBrushShape(BrushShape shape);
    BrushShape brushShape() const
    { return mBrushShape; }

    void setRestrictToSelection(bool isRestricted)
    {
        if (mRestrictToSelection == isRestricted) return;
        mRestrictToSelection = isRestricted;
        emit restrictToSelectionChanged();
    }
    bool restrictToSelection() const
    { return mRestrictToSelection;}

protected:
    void mapDocumentChanged(MapDocument *oldDocument,
                            MapDocument *newDocument);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);
    void setBrushRegion(const QPoint &tilePos);
    void paint();

signals:
    void ruleChanged();
    void restrictToSelectionChanged();
    void brushChanged();

private:
    Q_DISABLE_COPY(BmpBrushTool)
    static BmpBrushTool *mInstance;
    BmpBrushTool(QObject *parent = 0);
    ~BmpBrushTool();

    bool mPainting;
    bool mDidFirstPaint;
    QPoint mStampPos;
    bool mErasing;
    int mBmpIndex;
    QRgb mColor;
    int mBrushSize;
    BrushShape mBrushShape;
    bool mRestrictToSelection;
};

// This tool is for erasing pixels in a map's BMP images.
class BmpEraserTool : public AbstractBmpTool
{
    Q_OBJECT
public:
    static BmpEraserTool *instance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

protected:
    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);
    void setBrushRegion(const QPoint &tilePos);
    void paint();
    void eraseBmp(int bmpIndex, const QRegion &tileRgn);

private slots:
    void brushChanged();

private:
    Q_DISABLE_COPY(BmpEraserTool)
    static BmpEraserTool *mInstance;
    BmpEraserTool(QObject *parent = 0);
    ~BmpEraserTool();

    bool mPainting;
    bool mDidFirstPaint;
    QPoint mStampPos;
};

// This tool is for selecting and moving pixels in a map's BMP images.
class BmpSelectionTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static BmpSelectionTool *instance();

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

private:
    Q_DISABLE_COPY(BmpSelectionTool)
    static BmpSelectionTool *mInstance;
    BmpSelectionTool(QObject *parent = 0);

    enum Mode {
        NoMode,
        Selecting,
        Dragging
    };

    enum SelectionMode {
        Replace,
        Add,
        Subtract,
        Intersect
    };

    QRect selectedArea() const;

    Mode mMode;
    QPoint mDragStart;

    QPointF mStartScenePos;
    bool mMouseDown;
    bool mMouseMoved;

    QPoint mSelectionStart;
    SelectionMode mSelectionMode;
    bool mSelecting;
};

class BmpFloodFill
{
public:
    void floodFillScanlineStack(int x, int y, QRgb newColor, QRgb oldColor);
    QRgb pixel(int x, int y);
    void setPixel(int x, int y, QRgb pixel);
    bool push(int x, int y);
    bool pop(int &x, int &y);
    void emptyStack();

    QImage mImage;
    QRegion mRegion;
    enum { STACK_SIZE = 300 * 300 };
    int stack[STACK_SIZE];
    int stackPointer;
};

// This tool is for selecting and moving pixels in a map's BMP images.
class BmpWandTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static BmpWandTool *instance();

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

    void languageChanged();

protected:
    void mapDocumentChanged(MapDocument *oldDocument, MapDocument *newDocument);

    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

private slots:
    void bmpImageChanged();

private:
    Q_DISABLE_COPY(BmpWandTool)
    static BmpWandTool *mInstance;
    BmpWandTool(QObject *parent = 0);

    enum Mode {
        NoMode,
        Selecting,
        Dragging
    };

    enum SelectionMode {
        Replace,
        Add,
        Subtract,
        Intersect
    };

    Mode mMode;
    QPoint mDragStart;

    QPointF mStartScenePos;
    bool mMouseDown;
    bool mMouseMoved;

    BmpFloodFill mFloodFill;
};

// This tool is for drawing rectangles in a map's BMP images.
class BmpRectTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static BmpRectTool *instance();

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

    void modifiersChanged(Qt::KeyboardModifiers);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

private:
    Q_DISABLE_COPY(BmpRectTool)
    static BmpRectTool *mInstance;
    BmpRectTool(QObject *parent = 0);

    enum Mode {
        NoMode,
        Painting
    };

    QRect selectedArea() const;

    Mode mMode;

    QPointF mStartScenePos;
    QPoint mStartTilePos;
    bool mMouseDown;
    bool mMouseMoved;
    bool mErasing;
};

// This tool is for flood-filling in a map's BMP images.
class BmpBucketTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static BmpBucketTool *instance();

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void languageChanged();

protected:
    void mapDocumentChanged(MapDocument *oldDocument, MapDocument *newDocument);

    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

private:

private slots:
    void bmpImageChanged();

private:
    Q_DISABLE_COPY(BmpBucketTool)
    static BmpBucketTool *mInstance;
    BmpBucketTool(QObject *parent = 0);

    QPointF mStartScenePos;
    QPoint mStartTilePos;
    bool mErasing;
    BmpFloodFill mFloodFill;
};

// This tool is for baking auto-generated tiles to tile layers.
class BmpToLayersTool : public AbstractBmpTool
{
    Q_OBJECT
public:
    static BmpToLayersTool *instance();

    void activate(MapScene *scene);
    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

protected:
    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);
    void setBrushRegion(const QPoint &tilePos);
    void paint();

private slots:
    void brushChanged();

private:
    Q_DISABLE_COPY(BmpToLayersTool)
    static BmpToLayersTool *mInstance;
    BmpToLayersTool(QObject *parent = 0);
    ~BmpToLayersTool();

    bool mPainting;
    bool mDidFirstPaint;
    QPoint mStampPos;
};

// This tool is for drawing no-blend areas.
class NoBlendTool : public AbstractBmpTool
{
    Q_OBJECT

public:
    static NoBlendTool *instance();

    void deactivate(MapScene *scene);

    void mousePressed(QGraphicsSceneMouseEvent *event);
    void mouseReleased(QGraphicsSceneMouseEvent *event);

    void mouseMoved(const QPointF &pos, Qt::KeyboardModifiers modifiers);

    void modifiersChanged(Qt::KeyboardModifiers);

    void languageChanged();

protected:
    void tilePositionChanged(const QPoint &tilePos);

    void updateStatusInfo();

    bool isBlendLayer();

private:
    Q_DISABLE_COPY(NoBlendTool)
    static NoBlendTool *mInstance;
    NoBlendTool(QObject *parent = 0);

    enum Mode {
        NoMode,
        Painting
    };

    QRect selectedArea() const;
    void clearNoBlend();

    Mode mMode;

    QPointF mStartScenePos;
    QPoint mStartTilePos;
    bool mMouseDown;
    bool mMouseMoved;
    bool mErasing;
    QSet<int> mToolNoBlendLevel;
};

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
    void merge(const QPoint &pos, const ResizableImage *other, const QRegion &otherRegion)
    {
        QRegion region = otherRegion.translated(pos - otherRegion.boundingRect().topLeft()) & QRect(0, 0, width(), height());
        for (const QRect &area : region) {
            for (int y = area.top(); y <= area.bottom(); ++y) {
                for (int x = area.left(); x <= area.right(); ++x) {
                    setPixel(x, y, other->pixel(x - pos.x(), y - pos.y()));
                }
            }
        }
    }

    // This is like TileLayer::offset()
    void offset(const QPoint &offset, const QRect &bounds, int wrapX, int wrapY)
    {
        QImage newImage(size(), format());
        const QRgb black = qRgb(0, 0, 0);
        QRect imgBounds(QPoint(), size());

        for (int y = 0; y < height(); ++y) {
            for (int x = 0; x < width(); ++x) {
                // Skip out of bounds tiles
                if (!bounds.contains(x, y)) {
                    newImage.setPixel(x, y, pixel(x, y));
                    continue;
                }

                // Get position to pull tile value from
                int oldX = x - offset.x();
                int oldY = y - offset.y();

                // Wrap x value that will be pulled from
                if (wrapX && bounds.width() > 0) {
                    while (oldX < bounds.left())
                        oldX += bounds.width();
                    while (oldX > bounds.right())
                        oldX -= bounds.width();
                }

                // Wrap y value that will be pulled from
                if (wrapY && bounds.height() > 0) {
                    while (oldY < bounds.top())
                        oldY += bounds.height();
                    while (oldY > bounds.bottom())
                        oldY -= bounds.height();
                }

                // Set the new tile
                if (imgBounds.contains(oldX, oldY) && bounds.contains(oldX, oldY))
                    newImage.setPixel(x, y, pixel(oldX, oldY));
                else
                    newImage.setPixel(x, y, black);
            }
        }

        *this = newImage;
    }
};

// This is based on PaintTileLayer.
class PaintBMP : public QUndoCommand
{
public:
    PaintBMP(MapDocument *mapDocument, int bmpIndex, int x, int y,
             const QImage &source, const QRegion &region, bool erase = true);
//    ~PaintBMP();

    void setMergeable(bool mergeable);

    void undo();
    void redo();

    void paint(const ResizableImage &source);

    int id() const;
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
    QList<EraseTiles*> mEraseTilesCmds;
    QList<QRegion> mEraseRgns;
};

class ChangeBmpSelection : public QUndoCommand
{
public:
    ChangeBmpSelection(MapDocument *mapDocument, const QRegion &selection);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    MapDocument *mMapDocument;
    QRegion mSelection;
};

class OffsetBmpImage : public QUndoCommand
{
public:
    OffsetBmpImage(MapDocument *mapDocument, int bmpIndex, const QPoint &offset,
                   const QRect &bounds, int wrapX, int wrapY);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    QRect mBounds;
    QImage mOriginal;
    QImage mNew;
};

class ResizeBmpImage : public QUndoCommand
{
public:
    ResizeBmpImage(MapDocument *mapDocument, int bmpIndex, const QSize &size,
                   const QPoint &offset);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    QImage mOriginal;
    QImage mResized;
};

class ResizeBmpRands : public QUndoCommand
{
public:
    ResizeBmpRands(MapDocument *mapDocument, int bmpIndex, const QSize &size);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    int mBmpIndex;
    MapRands mOriginal;
    MapRands mResized;
};

class PaintNoBlend : public QUndoCommand
{
public:
    PaintNoBlend(MapDocument *mapDocument, MapNoBlend *noBlend,
                 const MapNoBlend &other, const QRegion &rgn);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    MapDocument *mMapDocument;
    MapNoBlend *mNoBlend;
    MapNoBlend mNew;
    QRegion mRegion;
};

class OffsetNoBlend : public QUndoCommand
{
public:
    OffsetNoBlend(MapDocument *mapDocument, MapNoBlend *noBlend,
                  const QPoint &offset, const QRect &bounds,
                  int wrapX, int wrapY);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    MapDocument *mMapDocument;
    MapNoBlend *mOriginal;
    MapNoBlend mOffset;
};

class ResizeNoBlend : public QUndoCommand
{
public:
    ResizeNoBlend(MapDocument *mapDocument, MapNoBlend *noBlend,
                  const QSize &size, const QPoint &offset);

    void undo() { swap(); }
    void redo() { swap(); }

private:
    void swap();

    MapDocument *mMapDocument;
    MapNoBlend *mOriginal;
    MapNoBlend mResized;
};

class BmpToLayers : public QUndoCommand
{
public:
    BmpToLayers(MapDocument *mapDocument, const QRegion &region, bool mergeable);
    ~BmpToLayers();

    int id() const;
    bool mergeWith(const QUndoCommand *other);

    void undo();
    void redo();

private:
    MapDocument *mMapDocument;
    bool mMergeable;
    QList<PaintTileLayer*> mLayerCmds;
    PaintBMP *mPaintCmd0;
    PaintBMP *mPaintCmd1;
};

/////

} // namespace Internal
} // namespace Tiled

#endif // BMPTOOL_H
